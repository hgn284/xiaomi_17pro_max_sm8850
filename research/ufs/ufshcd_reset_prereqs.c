/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction audited against OS44 whole original functions. */
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_reset_prereqs.h"
#include "os44_completion_contract.h"
struct ufs_hw_queue *os44_ufshcd_mcq_req_to_hwq(struct ufs_hba *, struct request *);
bool os44_ufshcd_mcq_force_compl_one(struct request *rq, void *priv)
{
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(rq);
	struct scsi_device *sdev = rq->q->queuedata;
	struct Scsi_Host *shost = sdev->host;
	struct ufs_hba *hba = shost_priv(shost);
	struct ufshcd_lrb *lrbp = &hba->lrb[rq->tag];
	struct ufs_hw_queue *hwq = os44_ufshcd_mcq_req_to_hwq(hba, rq);

	if (!hwq)
		return true;

	os44_ufshcd_mcq_compl_all_cqes_lock(hba, hwq);

	/*
	 * For those cmds of which the cqes are not present in the cq, complete
	 * them explicitly.
	 */
	scoped_guard(spinlock_irqsave, &hwq->cq_lock) {
		if (!test_bit(SCMD_STATE_COMPLETE, &cmd->state)) {
			set_host_byte(cmd, DID_REQUEUE);
			os44_ufshcd_release_scsi_cmd(hba, lrbp);
			scsi_done(cmd);
		}
	}

	return true;
}

irqreturn_t os44_ufshcd_tmc_handler(struct ufs_hba *hba)
{
	unsigned long flags, pending, issued;
	irqreturn_t ret = IRQ_NONE;
	int tag;

	spin_lock_irqsave(hba->host->host_lock, flags);
	pending = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
	issued = hba->outstanding_tasks & ~pending;
	for_each_set_bit(tag, &issued, hba->nutmrs) {
		struct request *req = hba->tmf_rqs[tag];
		struct completion *c = req->end_io_data;

		complete(c);
		ret = IRQ_HANDLED;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ret;
}
