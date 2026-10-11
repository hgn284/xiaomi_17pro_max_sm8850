// SPDX-License-Identifier: GPL-2.0-only
/* Whole OS44 BSG UIC and advanced RPMB source-pair reconstruction. */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include "drivers/ufs/core/ufs_bsg.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#include "drivers/ufs/core/ufshcd-crypto.h"
#include "ufsswap_provider_reconstruction.h"
#include "os44_uic_contract.h"
#include "os44_bsg_contract.h"
#define ADVANCED_RPMB_REQ_TIMEOUT 3000
static void local_ufshcd_release(struct ufs_hba *hba)
{
	guard(spinlock_irqsave)(&hba->clk_gating.lock);
	os44_core_release_locked(hba);
}

static void ufshcd_dev_man_lock(struct ufs_hba *hba)
{
	os44_core_hold(hba);
	mutex_lock(&hba->dev_cmd.lock);
	down_read(&hba->clk_scaling_lock);
}

static void ufshcd_dev_man_unlock(struct ufs_hba *hba)
{
	up_read(&hba->clk_scaling_lock);
	mutex_unlock(&hba->dev_cmd.lock);
	local_ufshcd_release(hba);
}

static inline void ufshcd_add_delay_before_dme_cmd(struct ufs_hba *hba)
{
	#define MIN_DELAY_BEFORE_DME_CMDS_US	1000
	unsigned long min_sleep_time_us;

	if (!(hba->quirks & UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS))
		return;

	/*
	 * last_dme_cmd_tstamp will be 0 only for 1st call to
	 * this function
	 */
	if (unlikely(!ktime_to_us(hba->last_dme_cmd_tstamp))) {
		min_sleep_time_us = MIN_DELAY_BEFORE_DME_CMDS_US;
	} else {
		unsigned long delta =
			(unsigned long) ktime_to_us(
				ktime_sub(ktime_get(),
				hba->last_dme_cmd_tstamp));

		if (delta < MIN_DELAY_BEFORE_DME_CMDS_US)
			min_sleep_time_us =
				MIN_DELAY_BEFORE_DME_CMDS_US - delta;
		else
			min_sleep_time_us = 0; /* no more delay required */
	}

	if (min_sleep_time_us > 0) {
		/* allow sleep for extra 50us if needed */
		usleep_range(min_sleep_time_us, min_sleep_time_us + 50);
	}

	/* update the last_dme_cmd_tstamp */
	hba->last_dme_cmd_tstamp = ktime_get();
}

static void ufshcd_setup_dev_cmd(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
			     enum dev_cmd_type cmd_type, u8 lun, int tag)
{
	lrbp->cmd = NULL;
	lrbp->task_tag = tag;
	lrbp->lun = lun;
	lrbp->intr_cmd = true; /* No interrupt aggregation */
	ufshcd_prepare_lrbp_crypto(NULL, lrbp);
	hba->dev_cmd.type = cmd_type;
}

static void
ufshcd_prepare_req_desc_hdr(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
			    u8 *upiu_flags, enum dma_data_direction cmd_dir,
			    int ehs_length)
{
	struct utp_transfer_req_desc *req_desc = lrbp->utr_descriptor_ptr;
	struct request_desc_header *h = &req_desc->header;
	enum utp_data_direction data_direction;

	lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;

	*h = (typeof(*h)){ };

	if (cmd_dir == DMA_FROM_DEVICE) {
		data_direction = UTP_DEVICE_TO_HOST;
		*upiu_flags = UPIU_CMD_FLAGS_READ;
	} else if (cmd_dir == DMA_TO_DEVICE) {
		data_direction = UTP_HOST_TO_DEVICE;
		*upiu_flags = UPIU_CMD_FLAGS_WRITE;
	} else {
		data_direction = UTP_NO_DATA_TRANSFER;
		*upiu_flags = UPIU_CMD_FLAGS_NONE;
	}

	h->command_type = lrbp->command_type;
	h->data_direction = data_direction;
	h->ehs_length = ehs_length;

	if (lrbp->intr_cmd)
		h->interrupt = 1;

	/* Prepare crypto related dwords */
	ufshcd_prepare_req_desc_hdr_crypto(lrbp, h);

	/*
	 * assigning invalid value for command status. Controller
	 * updates OCS on command completion, with the command
	 * status
	 */
	h->ocs = OCS_INVALID_COMMAND_STATUS;

	req_desc->prd_table_length = 0;
}

static void ufshcd_sgl_to_prdt(struct ufs_hba *hba, struct ufshcd_lrb *lrbp, int sg_entries,
			       struct scatterlist *sg_list)
{
	struct ufshcd_sg_entry *prd;
	struct scatterlist *sg;
	int i;

	if (sg_entries) {

		if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN)
			lrbp->utr_descriptor_ptr->prd_table_length =
				cpu_to_le16(sg_entries * ufshcd_sg_entry_size(hba));
		else
			lrbp->utr_descriptor_ptr->prd_table_length = cpu_to_le16(sg_entries);

		prd = lrbp->ucd_prdt_ptr;

		for_each_sg(sg_list, sg, sg_entries, i) {
			const unsigned int len = sg_dma_len(sg);

			/*
			 * From the UFSHCI spec: "Data Byte Count (DBC): A '0'
			 * based value that indicates the length, in bytes, of
			 * the data block. A maximum of length of 256KB may
			 * exist for any entry. Bits 1:0 of this field shall be
			 * 11b to indicate Dword granularity. A value of '3'
			 * indicates 4 bytes, '7' indicates 8 bytes, etc."
			 */
			WARN_ONCE(len > SZ_256K, "len = %#x\n", len);
			prd->size = cpu_to_le32(len - 1);
			prd->addr = cpu_to_le64(sg->dma_address);
			prd->reserved = 0;
			prd = (void *)prd + ufshcd_sg_entry_size(hba);
		}
	} else {
		lrbp->utr_descriptor_ptr->prd_table_length = 0;
	}
}

int os44_ufshcd_send_bsg_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_UIC_CMD)
		return 0;

	os44_core_hold(hba);

	if (uic_cmd->argument1 == UIC_ARG_MIB(PA_PWRMODE) &&
	    uic_cmd->command == UIC_CMD_DME_SET) {
		ret = os44_ufshcd_uic_pwr_ctrl(hba, uic_cmd);
		goto out;
	}

	mutex_lock(&hba->uic_cmd_mutex);
	ufshcd_add_delay_before_dme_cmd(hba);

	ret = os44___ufshcd_send_uic_cmd(hba, uic_cmd);
	if (!ret)
		ret = os44_ufshcd_wait_for_uic_cmd(hba, uic_cmd);

	mutex_unlock(&hba->uic_cmd_mutex);

out:
	local_ufshcd_release(hba);
	return ret;
}

int os44_ufshcd_advanced_rpmb_req_handler(struct ufs_hba *hba, struct utp_upiu_req *req_upiu,
			 struct utp_upiu_req *rsp_upiu, struct ufs_ehs *req_ehs,
			 struct ufs_ehs *rsp_ehs, int sg_cnt, struct scatterlist *sg_list,
			 enum dma_data_direction dir)
{
	const u32 tag = hba->reserved_slot;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	int err = 0;
	int result;
	u8 upiu_flags;
	u8 *ehs_data;
	u16 ehs_len;
	int ehs = (hba->capabilities & MASK_EHSLUTRD_SUPPORTED) ? 2 : 0;

	/* Protects use of hba->reserved_slot. */
	ufshcd_dev_man_lock(hba);

	ufshcd_setup_dev_cmd(hba, lrbp, DEV_CMD_TYPE_RPMB, UFS_UPIU_RPMB_WLUN, tag);

	ufshcd_prepare_req_desc_hdr(hba, lrbp, &upiu_flags, DMA_NONE, ehs);

	/* update the task tag */
	req_upiu->header.task_tag = tag;

	/* copy the UPIU(contains CDB) request as it is */
	memcpy(lrbp->ucd_req_ptr, req_upiu, sizeof(*lrbp->ucd_req_ptr));
	/* Copy EHS, starting with byte32, immediately after the CDB package */
	memcpy(lrbp->ucd_req_ptr + 1, req_ehs, sizeof(*req_ehs));

	if (dir != DMA_NONE && sg_list)
		ufshcd_sgl_to_prdt(hba, lrbp, sg_cnt, sg_list);

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));

	err = os44_core_issue_dev_cmd(hba, lrbp, tag, ADVANCED_RPMB_REQ_TIMEOUT);

	if (!err) {
		/* Just copy the upiu response as it is */
		memcpy(rsp_upiu, lrbp->ucd_rsp_ptr, sizeof(*rsp_upiu));
		/* Get the response UPIU result */
		result = (lrbp->ucd_rsp_ptr->header.response << 8) |
			lrbp->ucd_rsp_ptr->header.status;

		ehs_len = lrbp->ucd_rsp_ptr->header.ehs_length;
		/*
		 * Since the bLength in EHS indicates the total size of the EHS Header and EHS Data
		 * in 32 Byte units, the value of the bLength Request/Response for Advanced RPMB
		 * Message is 02h
		 */
		if (ehs_len == 2 && rsp_ehs) {
			/*
			 * ucd_rsp_ptr points to a buffer with a length of 512 bytes
			 * (ALIGNED_UPIU_SIZE = 512), and the EHS data just starts from byte32
			 */
			ehs_data = (u8 *)lrbp->ucd_rsp_ptr + EHS_OFFSET_IN_RESPONSE;
			memcpy(rsp_ehs, ehs_data, ehs_len * 32);
		}
	}

	ufshcd_dev_man_unlock(hba);

	return err ? : result;
}

