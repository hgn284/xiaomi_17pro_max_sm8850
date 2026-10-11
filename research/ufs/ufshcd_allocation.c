// SPDX-License-Identifier: GPL-2.0-only
/* Qualcomm public scaffold, OS44 whole-function reconstruction.
 * Real queue parameter owners and coherent/devm allocation, not fake globals.
 * This preserves stock semantics, including DMA-address failure checks. */
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/bitfield.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_mcq_allocation.h"
#define MAX_QUEUE_SUP GENMASK(7, 0)
#define UFS_MCQ_MIN_RW_QUEUES 2
#define UFS_MCQ_MIN_READ_QUEUES 0
#define UFS_MCQ_MIN_POLL_QUEUES 0
static_assert(sizeof(struct ufs_hw_queue)==0x90);
static_assert(sizeof(struct ufshcd_lrb)==0x98);
static_assert(offsetof(struct ufs_hba,uhq)==0x13d0);
static_assert(offsetof(struct ufs_hba,nutrs)==0x8c);
static_assert(offsetof(struct ufs_hba_variant_ops,get_hba_mac)==0xd0);
static_assert(offsetof(struct ufs_hba_variant_ops,mcq_config_resource)==0xc8);
static_assert(offsetof(struct ufs_hba_variant_ops,op_runtime_config)==0xd8);


int os44_ufshcd_memory_alloc(struct ufs_hba *hba)
{
	size_t utmrdl_size, utrdl_size, ucdl_size;

	/* Allocate memory for UTP command descriptors */
	ucdl_size = ufshcd_get_ucd_size(hba) * hba->nutrs;
	hba->ucdl_base_addr = dmam_alloc_coherent(hba->dev,
						  ucdl_size,
						  &hba->ucdl_dma_addr,
						  GFP_KERNEL);

	/*
	 * UFSHCI requires UTP command descriptor to be 128 byte aligned.
	 */
	if (!hba->ucdl_base_addr ||
	    WARN_ON(hba->ucdl_dma_addr & (128 - 1))) {
		dev_err(hba->dev,
			"Command Descriptor Memory allocation failed\n");
		goto out;
	}

	/*
	 * Allocate memory for UTP Transfer descriptors
	 * UFSHCI requires 1KB alignment of UTRD
	 */
	utrdl_size = (sizeof(struct utp_transfer_req_desc) * hba->nutrs);
	hba->utrdl_base_addr = dmam_alloc_coherent(hba->dev,
						   utrdl_size,
						   &hba->utrdl_dma_addr,
						   GFP_KERNEL);
	if (!hba->utrdl_base_addr ||
	    WARN_ON(hba->utrdl_dma_addr & (SZ_1K - 1))) {
		dev_err(hba->dev,
			"Transfer Descriptor Memory allocation failed\n");
		goto out;
	}

	/*
	 * Skip utmrdl allocation; it may have been
	 * allocated during first pass and not released during
	 * MCQ memory allocation.
	 * See ufshcd_release_sdb_queue() and ufshcd_config_mcq()
	 */
	if (hba->utmrdl_base_addr)
		goto skip_utmrdl;
	/*
	 * Allocate memory for UTP Task Management descriptors
	 * UFSHCI requires 1KB alignment of UTMRD
	 */
	utmrdl_size = sizeof(struct utp_task_req_desc) * hba->nutmrs;
	hba->utmrdl_base_addr = dmam_alloc_coherent(hba->dev,
						    utmrdl_size,
						    &hba->utmrdl_dma_addr,
						    GFP_KERNEL);
	if (!hba->utmrdl_base_addr ||
	    WARN_ON(hba->utmrdl_dma_addr & (SZ_1K - 1))) {
		dev_err(hba->dev,
		"Task Management Descriptor Memory allocation failed\n");
		goto out;
	}

skip_utmrdl:
	/* Allocate memory for local reference block */
	hba->lrb = devm_kcalloc(hba->dev,
				hba->nutrs, sizeof(struct ufshcd_lrb),
				GFP_KERNEL);
	if (!hba->lrb) {
		dev_err(hba->dev, "LRB Memory allocation failed\n");
		goto out;
	}
	return 0;
out:
	return -ENOMEM;
}

static void ufshcd_init_lrb(struct ufs_hba *hba, struct ufshcd_lrb *lrb, int i)
{
	struct utp_transfer_cmd_desc *cmd_descp = (void *)hba->ucdl_base_addr +
		i * ufshcd_get_ucd_size(hba);
	struct utp_transfer_req_desc *utrdlp = hba->utrdl_base_addr;
	dma_addr_t cmd_desc_element_addr = hba->ucdl_dma_addr +
		i * ufshcd_get_ucd_size(hba);
	u16 response_offset = le16_to_cpu(utrdlp[i].response_upiu_offset);
	u16 prdt_offset = le16_to_cpu(utrdlp[i].prd_table_offset);

	lrb->utr_descriptor_ptr = utrdlp + i;
	lrb->utrd_dma_addr = hba->utrdl_dma_addr +
		i * sizeof(struct utp_transfer_req_desc);
	lrb->ucd_req_ptr = (struct utp_upiu_req *)cmd_descp->command_upiu;
	lrb->ucd_req_dma_addr = cmd_desc_element_addr;
	lrb->ucd_rsp_ptr = (struct utp_upiu_rsp *)cmd_descp->response_upiu;
	lrb->ucd_rsp_dma_addr = cmd_desc_element_addr + response_offset;
	lrb->ucd_prdt_ptr = (struct ufshcd_sg_entry *)cmd_descp->prd_table;
	lrb->ucd_prdt_dma_addr = cmd_desc_element_addr + prdt_offset;
}

static void ufshcd_host_memory_configure(struct ufs_hba *hba)
{
	struct utp_transfer_req_desc *utrdlp;
	dma_addr_t cmd_desc_dma_addr;
	dma_addr_t cmd_desc_element_addr;
	u16 response_offset;
	u16 prdt_offset;
	int cmd_desc_size;
	int i;

	utrdlp = hba->utrdl_base_addr;

	response_offset =
		offsetof(struct utp_transfer_cmd_desc, response_upiu);
	prdt_offset =
		offsetof(struct utp_transfer_cmd_desc, prd_table);

	cmd_desc_size = ufshcd_get_ucd_size(hba);
	cmd_desc_dma_addr = hba->ucdl_dma_addr;

	for (i = 0; i < hba->nutrs; i++) {
		/* Configure UTRD with command descriptor base address */
		cmd_desc_element_addr =
				(cmd_desc_dma_addr + (cmd_desc_size * i));
		utrdlp[i].command_desc_base_addr =
				cpu_to_le64(cmd_desc_element_addr);

		/* Response upiu and prdt offset should be in double words */
		if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN) {
			utrdlp[i].response_upiu_offset =
				cpu_to_le16(response_offset);
			utrdlp[i].prd_table_offset =
				cpu_to_le16(prdt_offset);
			utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE);
		} else {
			utrdlp[i].response_upiu_offset =
				cpu_to_le16(response_offset >> 2);
			utrdlp[i].prd_table_offset =
				cpu_to_le16(prdt_offset >> 2);
			utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE >> 2);
		}

		ufshcd_init_lrb(hba, &hba->lrb[i], i);
	}
}

static void ufshcd_release_sdb_queue(struct ufs_hba *hba, int nutrs)
{
	size_t ucdl_size, utrdl_size;

	ucdl_size = ufshcd_get_ucd_size(hba) * nutrs;
	dmam_free_coherent(hba->dev, ucdl_size, hba->ucdl_base_addr,
			   hba->ucdl_dma_addr);

	utrdl_size = sizeof(struct utp_transfer_req_desc) * nutrs;
	dmam_free_coherent(hba->dev, utrdl_size, hba->utrdl_base_addr,
			   hba->utrdl_dma_addr);

	devm_kfree(hba->dev, hba->lrb);
}

int os44_ufshcd_alloc_mcq(struct ufs_hba *hba)
{
	int ret;
	int old_nutrs = hba->nutrs;

	ret = os44_ufshcd_mcq_decide_queue_depth(hba);
	if (ret < 0)
		return ret;

	hba->nutrs = ret;
	ret = os44_ufshcd_mcq_init(hba);
	if (ret)
		goto err;

	/*
	 * Previously allocated memory for nutrs may not be enough in MCQ mode.
	 * Number of supported tags in MCQ mode may be larger than SDB mode.
	 */
	if (hba->nutrs != old_nutrs) {
		ufshcd_release_sdb_queue(hba, old_nutrs);
		ret = os44_ufshcd_memory_alloc(hba);
		if (ret)
			goto err;
		ufshcd_host_memory_configure(hba);
	}

	ret = os44_ufshcd_mcq_memory_alloc(hba);
	if (ret)
		goto err;

	hba->host->can_queue = hba->nutrs - UFSHCD_NUM_RESERVED;
	hba->reserved_slot = hba->nutrs - UFSHCD_NUM_RESERVED;

	return 0;
err:
	hba->nutrs = old_nutrs;
	return ret;
}
