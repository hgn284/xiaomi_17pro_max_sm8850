/* SPDX-License-Identifier: GPL-2.0-only */
/* Original whole abort behavior. Source-owned providers and real error-ring
 * dependency retained; no fallback export or stand-in global. */
#include <linux/blk-mq.h>
#include <linux/sched/clock.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <ufs/ufshcd.h>
#include <trace/hooks/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "ufsswap_provider_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
#include "os44_uic_contract.h"
#include "os44_completion_contract.h"
#include "os44_mcq_abort_contract.h"
#include "os44_abort_contract.h"
extern void os44_ufshcd_compl_one_cqe(struct ufs_hba *, int, struct cq_entry *);
extern void os44_ufscld_on_idle(struct ufs_hba *);
static bool local_cmd_inflight(struct scsi_cmnd *cmd)
{ return cmd && blk_mq_rq_state(scsi_cmd_to_rq(cmd)) == MQ_RQ_IN_FLIGHT; }
static void local_release(struct ufs_hba *hba)
{
 guard(spinlock_irqsave)(&hba->clk_gating.lock);
 os44_core_release_locked(hba);
}
static void local_update_evt_hist(struct ufs_hba *hba, u32 id, u32 val)
{
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];
	e->val[e->pos] = val;
	e->tstamp[e->pos] = local_clock();
	e->cnt += 1;
	e->pos = (e->pos + 1) % UFS_EVENT_HIST_LENGTH;

	ufshcd_vops_event_notify(hba, id, &val);
}
static noinline void ufshcd_set_req_abort_skip(struct ufs_hba *hba, unsigned long bitmap)
{
	struct ufshcd_lrb *lrbp;
	int tag;

	for_each_set_bit(tag, &bitmap, hba->nutrs) {
		lrbp = &hba->lrb[tag];
		lrbp->req_abort_skip = true;
	}
}
static void __ufshcd_transfer_req_compl(struct ufs_hba *hba,
					unsigned long completed_reqs)
{
	int tag;

	for_each_set_bit(tag, &completed_reqs, hba->nutrs)
		os44_ufshcd_compl_one_cqe(hba, tag, NULL);
 os44_ufscld_on_idle(hba);
}
int os44_ufshcd_abort(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;
	struct ufs_hba *hba = shost_priv(host);
	int tag = scsi_cmd_to_rq(cmd)->tag;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	unsigned long flags;
	int err = FAILED;
	bool outstanding;
	u32 reg;

	if (local_cmd_inflight(lrbp->cmd))
  os44_query_error(0, 10, cmd->cmnd[0], cmd->cmnd[1]);
 os44_core_hold(hba);

	if (!hba->mcq_enabled) {
		reg = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
		if (!test_bit(tag, &hba->outstanding_reqs)) {
			/* If command is already aborted/completed, return FAILED. */
			dev_err(hba->dev,
				"%s: cmd at tag %d already completed, outstanding=0x%lx, doorbell=0x%x\n",
				"ufshcd_abort", tag, hba->outstanding_reqs, reg);
			goto release;
		}
	}

	/* Print Transfer Request of aborted task */
	dev_info(hba->dev, "%s: Device abort task at tag %d\n", "ufshcd_abort", tag);

	/*
	 * Print detailed info about aborted request.
	 * As more than one request might get aborted at the same time,
	 * print full information only for the first aborted request in order
	 * to reduce repeated printouts. For other aborted requests only print
	 * basic details.
	 */
	scsi_print_command(cmd);
	if (!hba->req_abort_count) {
		local_update_evt_hist(hba, UFS_EVT_ABORT, tag);
		os44_ufshcd_print_evt_hist(hba);
		os44_ufshcd_print_host_state(hba);
		os44_ufshcd_print_tr(hba, tag, true);
	} else {
		os44_ufshcd_print_tr(hba, tag, false);
	}
	hba->req_abort_count++;

	if (!hba->mcq_enabled && !(reg & (1 << tag))) {
		/* only execute this code in single doorbell mode */
		dev_err(hba->dev,
		"%s: cmd was completed, but without a notifying intr, tag = %d",
		"ufshcd_abort", tag);
		__ufshcd_transfer_req_compl(hba, 1UL << tag);
		goto release;
	}

	/*
	 * Task abort to the device W-LUN is illegal. When this command
	 * will fail, due to spec violation, scsi err handling next step
	 * will be to send LU reset which, again, is a spec violation.
	 * To avoid these unnecessary/illegal steps, first we clean up
	 * the lrb taken by this cmd and re-set it in outstanding_reqs,
	 * then queue the eh_work and bail.
	 */
	if (lrbp->lun == UFS_UPIU_UFS_DEVICE_WLUN) {
		local_update_evt_hist(hba, UFS_EVT_ABORT, lrbp->lun);

		spin_lock_irqsave(host->host_lock, flags);
		hba->force_reset = true;
		/* force_reset is already true, hence the real scheduling contract
   * reduces to this state transition plus queue publication. */
  if (hba->ufshcd_state != UFSHCD_STATE_ERROR) {
   hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;
   queue_work(hba->eh_wq, &hba->eh_work);
  }
		spin_unlock_irqrestore(host->host_lock, flags);
		goto release;
	}

	if (hba->mcq_enabled) {
		/* MCQ mode. Branch off to handle abort for mcq mode */
		err = os44_ufshcd_mcq_abort(cmd);
  if (!err)
   trace_android_vh_ufs_compl_command(hba, lrbp);
		goto release;
	}

	/* Skip task abort in case previous aborts failed and report failure */
	if (lrbp->req_abort_skip) {
		dev_err(hba->dev, "%s: skipping abort\n", "ufshcd_abort");
		ufshcd_set_req_abort_skip(hba, hba->outstanding_reqs);
		goto release;
	}

	err = os44_ufshcd_try_to_abort_task(hba, tag);
	if (err) {
		dev_err(hba->dev, "%s: failed with err %d\n", "ufshcd_abort", err);
		ufshcd_set_req_abort_skip(hba, hba->outstanding_reqs);
		err = FAILED;
		goto release;
	}

 trace_android_vh_ufs_compl_command(hba, lrbp);
	/*
	 * Clear the corresponding bit from outstanding_reqs since the command
	 * has been aborted successfully.
	 */
	spin_lock_irqsave(&hba->outstanding_lock, flags);
	outstanding = __test_and_clear_bit(tag, &hba->outstanding_reqs);
	spin_unlock_irqrestore(&hba->outstanding_lock, flags);

	if (outstanding)
		os44_ufshcd_release_scsi_cmd(hba, lrbp);

	err = SUCCESS;

release:
	/* Matches the ufshcd_hold() call at the start of this function. */
	local_release(hba);
	return err;
}
