/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole OS44 queuecommand reconstruction. Original input maintained separately.
 * Xiaomi write10 range policy + real reconstructed clock/MCQ/send providers.
 * Owner is declared only: no storage/getter invented to pass modpost. */
#include <linux/blk-mq.h>
#include <linux/ioprio.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include <trace/hooks/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "drivers/ufs/core/ufshcd-crypto.h"
#include "os44_xiaomi_owner.h"
#include "ufsswap_provider_reconstruction.h"
#include "os44_command_trace_contract.h"
extern void os44_core_send_command(struct ufs_hba *, unsigned int, struct ufs_hw_queue *);
enum { UFSHCD_EH_IN_PROGRESS = (1 << 0) };
#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)
static void local_release(struct ufs_hba *hba)
{
 guard(spinlock_irqsave)(&hba->clk_gating.lock);
 os44_core_release_locked(hba);
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

static int ufshcd_map_sg(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	int sg_segments = scsi_dma_map(cmd);
	int err;

	if (sg_segments < 0)
		return sg_segments;

	ufshcd_sgl_to_prdt(hba, lrbp, sg_segments, scsi_sglist(cmd));

	/*
	 * TODO(b/160883801): remove this vendor hook in favor of the upstream
	 * variant op.  This isn't possible yet because the upstream variant op
	 * doesn't yet make it possible for the host driver to get the keyslot.
	 */
	err = 0;
	trace_android_vh_ufs_fill_prdt(hba, lrbp, sg_segments, &err);
	if (err)
		return err;
	return ufshcd_crypto_fill_prdt(hba, lrbp);
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

static
void ufshcd_prepare_utp_scsi_cmd_upiu(struct ufshcd_lrb *lrbp, u8 upiu_flags)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	unsigned short cdb_len;

	ucd_req_ptr->header = (struct utp_upiu_header){
		.transaction_code = UPIU_TRANSACTION_COMMAND,
		.flags = upiu_flags,
		.lun = lrbp->lun,
		.task_tag = lrbp->task_tag,
		.command_set_type = UPIU_COMMAND_SET_TYPE_SCSI,
	};

	WARN_ON_ONCE(ucd_req_ptr->header.task_tag != lrbp->task_tag);

	ucd_req_ptr->sc.exp_data_transfer_len = cpu_to_be32(cmd->sdb.length);

	cdb_len = min_t(unsigned short, cmd->cmd_len, UFS_CDB_SIZE);
	memset(ucd_req_ptr->sc.cdb, 0, UFS_CDB_SIZE);
	memcpy(ucd_req_ptr->sc.cdb, cmd->cmnd, cdb_len);

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

static void ufshcd_comp_scsi_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct request *rq = scsi_cmd_to_rq(lrbp->cmd);
	unsigned int ioprio_class = IOPRIO_PRIO_CLASS(req_get_ioprio(rq));
	u8 upiu_flags;

	ufshcd_prepare_req_desc_hdr(hba, lrbp, &upiu_flags, lrbp->cmd->sc_data_direction, 0);
	if (ioprio_class == IOPRIO_CLASS_RT)
		upiu_flags |= UPIU_CMD_FLAGS_CP;
	ufshcd_prepare_utp_scsi_cmd_upiu(lrbp, upiu_flags);

	if (hba->android_quirks & UFSHCD_ANDROID_QUIRK_SET_IID_TO_ONE)
		lrbp->ucd_req_ptr->header.iid = 1;
}

int os44_ufshcd_queuecommand(struct Scsi_Host *host, struct scsi_cmnd *cmd)
{
	struct ufs_hba *hba = shost_priv(host);
	int tag = scsi_cmd_to_rq(cmd)->tag;
	struct ufshcd_lrb *lrbp;
	int err = 0;
	struct ufs_hw_queue *hwq = NULL;

	os44_ufs_xiaomi.queue_hba = hba;

	switch (hba->ufshcd_state) {
	case UFSHCD_STATE_OPERATIONAL:
		break;
	case UFSHCD_STATE_EH_SCHEDULED_NON_FATAL:
		/*
		 * SCSI error handler can call ->queuecommand() while UFS error
		 * handler is in progress. Error interrupts could change the
		 * state from UFSHCD_STATE_RESET to
		 * UFSHCD_STATE_EH_SCHEDULED_NON_FATAL. Prevent requests
		 * being issued in that case.
		 */
		if (ufshcd_eh_in_progress(hba)) {
			err = SCSI_MLQUEUE_HOST_BUSY;
			goto out;
		}
		break;
	case UFSHCD_STATE_EH_SCHEDULED_FATAL:
		/*
		 * pm_runtime_get_sync() is used at error handling preparation
		 * stage. If a scsi cmd, e.g. the SSU cmd, is sent from hba's
		 * PM ops, it can never be finished if we let SCSI layer keep
		 * retrying it, which gets err handler stuck forever. Neither
		 * can we let the scsi cmd pass through, because UFS is in bad
		 * state, the scsi cmd may eventually time out, which will get
		 * err handler blocked for too long. So, just fail the scsi cmd
		 * sent from PM ops, err handler can recover PM error anyways.
		 */
		if (hba->pm_op_in_progress) {
			hba->force_reset = true;
			set_host_byte(cmd, DID_BAD_TARGET);
			scsi_done(cmd);
			goto out;
		}
		fallthrough;
	case UFSHCD_STATE_RESET:
		err = SCSI_MLQUEUE_HOST_BUSY;
		goto out;
	case UFSHCD_STATE_ERROR:
		set_host_byte(cmd, DID_ERROR);
		scsi_done(cmd);
		goto out;
	}

	hba->req_abort_count = 0;

	os44_core_hold(hba);

	lrbp = &hba->lrb[tag];
	if (os44_ufs_xiaomi.swap.swap.enabled) {
	 struct os44_list_view *p;
	 unsigned long sector = blk_rq_pos(scsi_cmd_to_rq(cmd));
	 unsigned int shift = ilog2(cmd->device->sector_size) - 9;
	 sector >>= shift;
	 for (p = os44_ufs_xiaomi.swap.swap.head.next;
	      p != &os44_ufs_xiaomi.swap.swap.head; p = p->next) {
	  struct os44_range_view *range = (struct os44_range_view *)p;
	  if (sector >= range->start && sector <= range->end) {
	   if (cmd->cmnd[0] == WRITE_10)
	    cmd->cmnd[6] = os44_ufs_xiaomi.swap.swap.config->write10_cdb6;
	   break;
	  }
	 }
	}
	lrbp->cmd = cmd;
	lrbp->task_tag = tag;
	lrbp->lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);
	lrbp->intr_cmd = !ufshcd_is_intr_aggr_allowed(hba);

	ufshcd_prepare_lrbp_crypto(scsi_cmd_to_rq(cmd), lrbp);

	trace_android_vh_ufs_prepare_command(hba, scsi_cmd_to_rq(cmd), lrbp,
					     &err);
	if (err) {
		lrbp->cmd = NULL;
		local_release(hba);
		goto out;
	}

	lrbp->req_abort_skip = false;

	ufshcd_comp_scsi_upiu(hba, lrbp);

	err = ufshcd_map_sg(hba, lrbp);
	if (err) {
		local_release(hba);
		goto out;
	}

	if (hba->mcq_enabled)
		hwq = os44_ufshcd_mcq_req_to_hwq(hba, scsi_cmd_to_rq(cmd));

	os44_core_send_command(hba, tag, hwq);

out:
	return err;
}
