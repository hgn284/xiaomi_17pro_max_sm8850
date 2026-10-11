// SPDX-License-Identifier: GPL-2.0-only
/* Public Qualcomm scaffold; whole OS44 provider/IO/KCFI audit required.
 * Preserve actual queue programming order and relaxed-vs-ordered MMIO. */
#include <linux/bitfield.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_init_controls.h"
#define MCQ_CFG_MAC_MASK GENMASK(16,8)
#define MCQ_ENTRY_SIZE_IN_DWORD 8
#define QUEUE_EN_OFFSET 31
#define QUEUE_ID_OFFSET 16
static_assert(sizeof(struct ufs_hw_queue)==0x90);
static_assert(offsetof(struct ufs_hba,mcq_base)==0x13c8);
static_assert(offsetof(struct ufs_hba,uhq)==0x13d0);
static_assert(offsetof(struct ufs_hba,mcq_opr)==0x13e0);

static void __iomem *mcq_opr_base(struct ufs_hba *hba,
					 enum ufshcd_mcq_opr n, int i)
{
	struct ufshcd_mcq_opr_info_t *opr = &hba->mcq_opr[n];

	return opr->base + opr->stride * i;
}

void os44_ufshcd_mcq_config_mac(struct ufs_hba *hba, u32 max_active_cmds)
{
	u32 val;

	val = ufshcd_readl(hba, REG_UFS_MCQ_CFG);
	val &= ~MCQ_CFG_MAC_MASK;
	val |= FIELD_PREP(MCQ_CFG_MAC_MASK, max_active_cmds - 1);
	ufshcd_writel(hba, val, REG_UFS_MCQ_CFG);
}

void os44_ufshcd_mcq_make_queues_operational(struct ufs_hba *hba)
{
	struct ufs_hw_queue *hwq;
	u16 qsize;
	int i;

	for (i = 0; i < hba->nr_hw_queues; i++) {
		hwq = &hba->uhq[i];
		hwq->id = i;
		qsize = hwq->max_entries * MCQ_ENTRY_SIZE_IN_DWORD - 1;

		/* Submission Queue Lower Base Address */
		ufsmcq_writelx(hba, lower_32_bits(hwq->sqe_dma_addr),
			      ufshcd_mcq_cfg_offset(REG_SQLBA, i));
		/* Submission Queue Upper Base Address */
		ufsmcq_writelx(hba, upper_32_bits(hwq->sqe_dma_addr),
			      ufshcd_mcq_cfg_offset(REG_SQUBA, i));
		/* Submission Queue Doorbell Address Offset */
		ufsmcq_writelx(hba, ufshcd_mcq_opr_offset(hba, OPR_SQD, i),
			      ufshcd_mcq_cfg_offset(REG_SQDAO, i));
		/* Submission Queue Interrupt Status Address Offset */
		ufsmcq_writelx(hba, ufshcd_mcq_opr_offset(hba, OPR_SQIS, i),
			      ufshcd_mcq_cfg_offset(REG_SQISAO, i));

		/* Completion Queue Lower Base Address */
		ufsmcq_writelx(hba, lower_32_bits(hwq->cqe_dma_addr),
			      ufshcd_mcq_cfg_offset(REG_CQLBA, i));
		/* Completion Queue Upper Base Address */
		ufsmcq_writelx(hba, upper_32_bits(hwq->cqe_dma_addr),
			      ufshcd_mcq_cfg_offset(REG_CQUBA, i));
		/* Completion Queue Doorbell Address Offset */
		ufsmcq_writelx(hba, ufshcd_mcq_opr_offset(hba, OPR_CQD, i),
			      ufshcd_mcq_cfg_offset(REG_CQDAO, i));
		/* Completion Queue Interrupt Status Address Offset */
		ufsmcq_writelx(hba, ufshcd_mcq_opr_offset(hba, OPR_CQIS, i),
			      ufshcd_mcq_cfg_offset(REG_CQISAO, i));

		/* Save the base addresses for quicker access */
		hwq->mcq_sq_head = mcq_opr_base(hba, OPR_SQD, i) + REG_SQHP;
		hwq->mcq_sq_tail = mcq_opr_base(hba, OPR_SQD, i) + REG_SQTP;
		hwq->mcq_cq_head = mcq_opr_base(hba, OPR_CQD, i) + REG_CQHP;
		hwq->mcq_cq_tail = mcq_opr_base(hba, OPR_CQD, i) + REG_CQTP;

		/* Reinitializing is needed upon HC reset */
		hwq->sq_tail_slot = hwq->cq_tail_slot = hwq->cq_head_slot = 0;

		/* Enable Tail Entry Push Status interrupt only for non-poll queues */
		if (i < hba->nr_hw_queues - hba->nr_queues[HCTX_TYPE_POLL])
			writel(1, mcq_opr_base(hba, OPR_CQIS, i) + REG_CQIE);

		/* Completion Queue Enable|Size to Completion Queue Attribute */
		ufsmcq_writel(hba, (1 << QUEUE_EN_OFFSET) | qsize,
			      ufshcd_mcq_cfg_offset(REG_CQATTR, i));

		/*
		 * Submission Qeueue Enable|Size|Completion Queue ID to
		 * Submission Queue Attribute
		 */
		ufsmcq_writel(hba, (1 << QUEUE_EN_OFFSET) | qsize |
			      (i << QUEUE_ID_OFFSET),
			      ufshcd_mcq_cfg_offset(REG_SQATTR, i));
	}
}
