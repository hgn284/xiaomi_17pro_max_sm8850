/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstructed source-owned provider boundaries; no alias or fake completion. */
#include <linux/blk-mq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
extern void os44_ufshcd_compl_one_cqe(struct ufs_hba *, int, struct cq_entry *);
extern void os44_ufscld_on_idle(struct ufs_hba *);
unsigned long os44_ufshcd_mcq_poll_cqe_lock(struct ufs_hba *, struct ufs_hw_queue *);
enum { UFSHCD_POLL_FROM_INTERRUPT_CONTEXT = -1 };
static void __ufshcd_transfer_req_compl(struct ufs_hba *hba,
					unsigned long completed_reqs)
{
	int tag;

	for_each_set_bit(tag, &completed_reqs, hba->nutrs)
		os44_ufshcd_compl_one_cqe(hba, tag, NULL);
	/* Stock vendor callback after completing the bitmap. */
	os44_ufscld_on_idle(hba);
}

static void ufshcd_clear_polled(struct ufs_hba *hba,
				unsigned long *completed_reqs)
{
	int tag;

	for_each_set_bit(tag, completed_reqs, hba->nutrs) {
		struct scsi_cmnd *cmd = hba->lrb[tag].cmd;

		if (!cmd)
			continue;
		if (scsi_cmd_to_rq(cmd)->cmd_flags & REQ_POLLED)
			__clear_bit(tag, completed_reqs);
	}
}

int os44_ufshcd_poll(struct Scsi_Host *shost, unsigned int queue_num)
{
	struct ufs_hba *hba = shost_priv(shost);
	unsigned long completed_reqs, flags;
	u32 tr_doorbell;
	struct ufs_hw_queue *hwq;

	if (hba->mcq_enabled) {
		hwq = &hba->uhq[queue_num];

		return os44_ufshcd_mcq_poll_cqe_lock(hba, hwq);
	}

	spin_lock_irqsave(&hba->outstanding_lock, flags);
	tr_doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	completed_reqs = ~tr_doorbell & hba->outstanding_reqs;
	WARN_ONCE(completed_reqs & ~hba->outstanding_reqs,
		  "completed: %#lx; outstanding: %#lx\n", completed_reqs,
		  hba->outstanding_reqs);
	if (queue_num == UFSHCD_POLL_FROM_INTERRUPT_CONTEXT) {
		/* Do not complete polled requests from interrupt context. */
		ufshcd_clear_polled(hba, &completed_reqs);
	}
	hba->outstanding_reqs &= ~completed_reqs;
	spin_unlock_irqrestore(&hba->outstanding_lock, flags);

	if (completed_reqs)
		__ufshcd_transfer_req_compl(hba, completed_reqs);

	return completed_reqs != 0;
}
