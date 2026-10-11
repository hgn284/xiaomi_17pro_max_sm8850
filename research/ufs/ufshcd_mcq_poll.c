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
#define CQE_UCD_BA GENMASK_ULL(63, 7)
static int ufshcd_mcq_get_tag(struct ufs_hba *hba, struct cq_entry *cqe)
{
	u64 addr;

	/* sizeof(struct utp_transfer_cmd_desc) must be a multiple of 128 */
	BUILD_BUG_ON(sizeof(struct utp_transfer_cmd_desc) & GENMASK(6, 0));

	/* Bits 63:7 UCD base address, 6:5 are reserved, 4:0 is SQ ID */
	addr = (le64_to_cpu(cqe->command_desc_base_addr) & CQE_UCD_BA) -
		hba->ucdl_dma_addr;

	return div_u64(addr, ufshcd_get_ucd_size(hba));
}

static void ufshcd_mcq_process_cqe(struct ufs_hba *hba,
				   struct ufs_hw_queue *hwq)
{
	struct cq_entry *cqe = ufshcd_mcq_cur_cqe(hwq);
	int tag = ufshcd_mcq_get_tag(hba, cqe);

	if (cqe->command_desc_base_addr) {
		os44_ufshcd_compl_one_cqe(hba, tag, cqe);
		/* After processed the cqe, mark it empty (invalid) entry */
		cqe->command_desc_base_addr = 0;
	}
}

unsigned long os44_ufshcd_mcq_poll_cqe_lock(struct ufs_hba *hba,
				       struct ufs_hw_queue *hwq)
{
	unsigned long completed_reqs = 0;
	unsigned long flags;

	spin_lock_irqsave(&hwq->cq_lock, flags);
	ufshcd_mcq_update_cq_tail_slot(hwq);
	while (!ufshcd_mcq_is_cq_empty(hwq)) {
		ufshcd_mcq_process_cqe(hba, hwq);
		ufshcd_mcq_inc_cq_head_slot(hwq);
		completed_reqs++;
	}

	if (completed_reqs)
		ufshcd_mcq_update_cq_head(hwq);
	spin_unlock_irqrestore(&hwq->cq_lock, flags);

	return completed_reqs;
}

#include "os44_reset_prereqs.h"
void os44_ufshcd_mcq_compl_all_cqes_lock(struct ufs_hba *hba,
				    struct ufs_hw_queue *hwq)
{
	unsigned long flags;
	u32 entries = hwq->max_entries;

	spin_lock_irqsave(&hwq->cq_lock, flags);
	while (entries > 0) {
		ufshcd_mcq_process_cqe(hba, hwq);
		ufshcd_mcq_inc_cq_head_slot(hwq);
		entries--;
	}

	ufshcd_mcq_update_cq_tail_slot(hwq);
	hwq->cq_head_slot = hwq->cq_tail_slot;
	spin_unlock_irqrestore(&hwq->cq_lock, flags);
}
