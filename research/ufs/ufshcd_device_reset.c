/* SPDX-License-Identifier: GPL-2.0-only */
/* Source-owned full device-reset callback. Public baseline plus original
 * CLD idle call; real reconstructed TM/clear/completion providers only. */
#include <linux/blk-mq.h>
#include <linux/sched/clock.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_tm_contract.h"
#include "os44_command_trace_contract.h"
extern int os44_core_clear_cmd(struct ufs_hba *, u32);
extern void os44_ufshcd_compl_one_cqe(struct ufs_hba *, int, struct cq_entry *);
extern void os44_ufscld_on_idle(struct ufs_hba *);
unsigned long os44_ufshcd_mcq_poll_cqe_lock(struct ufs_hba *, struct ufs_hw_queue *);

static bool local_cmd_inflight(struct scsi_cmnd *cmd)
{
	return cmd && blk_mq_rq_state(scsi_cmd_to_rq(cmd)) == MQ_RQ_IN_FLIGHT;
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

static int ufshcd_issue_tm_cmd(struct ufs_hba *hba, int lun_id, int task_id,
		u8 tm_function, u8 *tm_response)
{
	struct utp_task_req_desc treq = { };
	enum utp_ocs ocs_value;
	int err;

	/* Configure task request descriptor */
	treq.header.interrupt = 1;
	treq.header.ocs = OCS_INVALID_COMMAND_STATUS;

	/* Configure task request UPIU */
	treq.upiu_req.req_header.transaction_code = UPIU_TRANSACTION_TASK_REQ;
	treq.upiu_req.req_header.lun = lun_id;
	treq.upiu_req.req_header.tm_function = tm_function;

	/*
	 * The host shall provide the same value for LUN field in the basic
	 * header and for Input Parameter.
	 */
	treq.upiu_req.input_param1 = cpu_to_be32(lun_id);
	treq.upiu_req.input_param2 = cpu_to_be32(task_id);

	err = os44___ufshcd_issue_tm_cmd(hba, &treq, tm_function);
	if (err == -ETIMEDOUT)
		return err;

	ocs_value = treq.header.ocs & MASK_OCS;
	if (ocs_value != OCS_SUCCESS)
		dev_err(hba->dev, "%s: failed, ocs = 0x%x\n",
				"ufshcd_issue_tm_cmd", ocs_value);
	else if (tm_response)
		*tm_response = be32_to_cpu(treq.upiu_rsp.output_param1) &
				MASK_TM_SERVICE_RESP;
	return err;
}

static void __ufshcd_transfer_req_compl(struct ufs_hba *hba,
					unsigned long completed_reqs)
{
	int tag;

	for_each_set_bit(tag, &completed_reqs, hba->nutrs)
		os44_ufshcd_compl_one_cqe(hba, tag, NULL);
	os44_ufscld_on_idle(hba);
}

int os44_ufshcd_eh_device_reset_handler(struct scsi_cmnd *cmd)
{
	unsigned long flags, pending_reqs = 0, not_cleared = 0;
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	struct ufs_hw_queue *hwq;
	struct ufshcd_lrb *lrbp;
	u32 pos, not_cleared_mask = 0;
	int err;
	u8 resp = 0xF, lun;

	host = cmd->device->host;
	hba = shost_priv(host);

	lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);
	err = ufshcd_issue_tm_cmd(hba, lun, 0, UFS_LOGICAL_RESET, &resp);
	if (err || resp != UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
		if (!err)
			err = resp;
		goto out;
	}

	if (hba->mcq_enabled) {
		for (pos = 0; pos < hba->nutrs; pos++) {
			lrbp = &hba->lrb[pos];
			if (local_cmd_inflight(lrbp->cmd) &&
			    lrbp->lun == lun) {
				os44_core_clear_cmd(hba, pos);
				hwq = os44_ufshcd_mcq_req_to_hwq(hba, scsi_cmd_to_rq(lrbp->cmd));
				os44_ufshcd_mcq_poll_cqe_lock(hba, hwq);
			}
		}
		err = 0;
		goto out;
	}

	/* clear the commands that were pending for corresponding LUN */
	spin_lock_irqsave(&hba->outstanding_lock, flags);
	for_each_set_bit(pos, &hba->outstanding_reqs, hba->nutrs)
		if (hba->lrb[pos].lun == lun)
			__set_bit(pos, &pending_reqs);
	hba->outstanding_reqs &= ~pending_reqs;
	spin_unlock_irqrestore(&hba->outstanding_lock, flags);

	for_each_set_bit(pos, &pending_reqs, hba->nutrs) {
		if (os44_core_clear_cmd(hba, pos) < 0) {
			spin_lock_irqsave(&hba->outstanding_lock, flags);
			not_cleared = 1U << pos &
				ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
			hba->outstanding_reqs |= not_cleared;
			not_cleared_mask |= not_cleared;
			spin_unlock_irqrestore(&hba->outstanding_lock, flags);

			dev_err(hba->dev, "%s: failed to clear request %d\n",
				"ufshcd_eh_device_reset_handler", pos);
		}
	}
	__ufshcd_transfer_req_compl(hba, pending_reqs & ~not_cleared_mask);

out:
	hba->req_abort_count = 0;
	local_update_evt_hist(hba, UFS_EVT_DEV_RESET, (u32)err);
	if (!err) {
		err = SUCCESS;
	} else {
		dev_err(hba->dev, "%s: failed with err %d\n", "ufshcd_eh_device_reset_handler", err);
		err = FAILED;
	}
	return err;
}
