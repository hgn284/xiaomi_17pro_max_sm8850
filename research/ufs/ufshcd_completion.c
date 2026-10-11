/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction; private original C names not recovered.
 * Owners are declarations only: no fabricated globals or fake initialization. */
#include <linux/blk-mq.h>
#include <linux/sched/clock.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include <trace/hooks/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_completion_contract.h"
#include "os44_command_trace_contract.h"
#include "os44_xiaomi_lifetime_view.h"
#include "ufsswap_debug_reconstruction.h"
#include "os44_cld_view.h"
extern void os44_update_fastdiscard_time(struct ufs_hba *, struct ufshcd_lrb *);
extern void os44_ufscld_on_idle(struct ufs_hba *);
extern void os44_ufshcd_print_evt_hist(struct ufs_hba *);
extern void os44_ufshcd_print_host_state(struct ufs_hba *);
static_assert(offsetof(struct ufs_hba,priv)==0xb8);
static inline enum utp_ocs ufshcd_get_tr_ocs(struct ufshcd_lrb *l, struct cq_entry *q)
{ return q ? le32_to_cpu(q->status)&MASK_OCS : l->utr_descriptor_ptr->header.ocs & MASK_OCS; }
#define ufshcd_eh_in_progress(h) ((h)->eh_flags & 1)
static inline enum upiu_response_transaction ufshcd_get_req_rsp(struct utp_upiu_rsp *r)
{ return r->header.transaction_code; }
static inline bool ufshcd_is_exception_event(struct utp_upiu_rsp *r)
{ return r->header.device_information & 1; }
static inline bool ufshcd_should_inform_monitor(struct ufs_hba *hba,
						struct ufshcd_lrb *lrbp)
{
	const struct ufs_hba_monitor *m = &hba->monitor;

	return (m->enabled && lrbp && lrbp->cmd &&
		(!m->chunk_size || m->chunk_size == lrbp->cmd->sdb.length) &&
		ktime_before(hba->monitor.enabled_ts, lrbp->issue_time_stamp));
}

static inline void ufshcd_copy_sense_data(struct ufshcd_lrb *lrbp)
{
	u8 *const sense_buffer = lrbp->cmd->sense_buffer;
	u16 resp_len;
	int len;

	resp_len = be16_to_cpu(lrbp->ucd_rsp_ptr->header.data_segment_length);
	if (sense_buffer && resp_len) {
		int len_to_copy;

		len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);
		len_to_copy = min_t(int, UFS_SENSE_SIZE, len);

		memcpy(sense_buffer, lrbp->ucd_rsp_ptr->sr.sense_data,
		       len_to_copy);
	}
}
static inline int
ufshcd_scsi_cmd_status(struct ufshcd_lrb *lrbp, int scsi_status)
{
	int result = 0;

	switch (scsi_status) {
	case SAM_STAT_CHECK_CONDITION:
		ufshcd_copy_sense_data(lrbp);
		fallthrough;
	case SAM_STAT_GOOD:
		result |= DID_OK << 16 | scsi_status;
		break;
	case SAM_STAT_TASK_SET_FULL:
	case SAM_STAT_BUSY:
	case SAM_STAT_TASK_ABORTED:
		ufshcd_copy_sense_data(lrbp);
		result |= scsi_status;
		break;
	default:
		result |= DID_ERROR << 16;
		break;
	} /* end of switch */

	return result;
}

/**
 * ufshcd_transfer_rsp_status - Get overall status of the response
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block of completed command
 * @cqe: pointer to the completion queue entry
 *
 * Return: result of the command to notify SCSI midlayer.
 */
static inline int
ufshcd_transfer_rsp_status(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
			   struct cq_entry *cqe)
{
	int result = 0;
	int scsi_status;
	enum utp_ocs ocs;
	u8 upiu_flags;
	u32 resid;

	upiu_flags = lrbp->ucd_rsp_ptr->header.flags;
	resid = be32_to_cpu(lrbp->ucd_rsp_ptr->sr.residual_transfer_count);
	/*
	 * Test !overflow instead of underflow to support UFS devices that do
	 * not set either flag.
	 */
	if (resid && !(upiu_flags & UPIU_RSP_FLAG_OVERFLOW))
		scsi_set_resid(lrbp->cmd, resid);

	/* overall command status of utrd */
	ocs = ufshcd_get_tr_ocs(lrbp, cqe);

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_OCS_FATAL_ERROR) {
		if (lrbp->ucd_rsp_ptr->header.response ||
		    lrbp->ucd_rsp_ptr->header.status)
			ocs = OCS_SUCCESS;
	}

	switch (ocs) {
	case OCS_SUCCESS:
		hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
		switch (ufshcd_get_req_rsp(lrbp->ucd_rsp_ptr)) {
		case UPIU_TRANSACTION_RESPONSE:
			/*
			 * get the result based on SCSI status response
			 * to notify the SCSI midlayer of the command status
			 */
			scsi_status = lrbp->ucd_rsp_ptr->header.status;
			result = ufshcd_scsi_cmd_status(lrbp, scsi_status);

			/*
			 * Currently we are only supporting BKOPs exception
			 * events hence we can ignore BKOPs exception event
			 * during power management callbacks. BKOPs exception
			 * event is not expected to be raised in runtime suspend
			 * callback as it allows the urgent bkops.
			 * During system suspend, we are anyway forcefully
			 * disabling the bkops and if urgent bkops is needed
			 * it will be enabled on system resume. Long term
			 * solution could be to abort the system suspend if
			 * UFS device needs urgent BKOPs.
			 */
			if (!hba->pm_op_in_progress &&
			    !ufshcd_eh_in_progress(hba) &&
			    ufshcd_is_exception_event(lrbp->ucd_rsp_ptr))
				/* Flushed in suspend */
				schedule_work(&hba->eeh_work);
			break;
		case UPIU_TRANSACTION_REJECT_UPIU:
			/* TODO: handle Reject UPIU Response */
			result = DID_ERROR << 16;
			dev_err(hba->dev,
				"Reject UPIU not fully implemented\n");
			break;
		default:
			dev_err(hba->dev,
				"Unexpected request response code = %x\n",
				result);
			result = DID_ERROR << 16;
			break;
		}
		break;
	case OCS_ABORTED:
	case OCS_INVALID_COMMAND_STATUS:
		result |= DID_REQUEUE << 16;
		dev_warn(hba->dev,
				"OCS %s from controller for tag %d\n",
				(ocs == OCS_ABORTED ? "aborted" : "invalid"),
				lrbp->task_tag);
		break;
	case OCS_INVALID_CMD_TABLE_ATTR:
	case OCS_INVALID_PRDT_ATTR:
	case OCS_MISMATCH_DATA_BUF_SIZE:
	case OCS_MISMATCH_RESP_UPIU_SIZE:
	case OCS_PEER_COMM_FAILURE:
	case OCS_FATAL_ERROR:
	case OCS_DEVICE_FATAL_ERROR:
	case OCS_INVALID_CRYPTO_CONFIG:
	case OCS_GENERAL_CRYPTO_ERROR:
	default:
		result |= DID_ERROR << 16;
		dev_err(hba->dev,
				"OCS error from controller = %x for tag %d\n",
				ocs, lrbp->task_tag);
		os44_ufshcd_print_evt_hist(hba);
		os44_ufshcd_print_host_state(hba);
		break;
	} /* end of switch */

	if ((host_byte(result) != DID_OK) &&
	    (host_byte(result) != DID_REQUEUE) && !hba->silence_err_logs)
		os44_ufshcd_print_tr(hba, lrbp->task_tag, true);

	if (host_byte(result) == DID_ABORT || host_byte(result) == DID_ERROR)
		os44_query_error(0, 8, -1, 0);
	trace_android_vh_ufs_transfer_rsp_status(hba, lrbp, cqe, result);
	return result;
}

static inline void os44_account_cell(struct os44_latency_cell *c, u64 latency)
{
 c->sum += latency;
 c->count++;
 c->maximum = max(c->maximum, latency);
}
static inline void os44_account_completion(struct ufshcd_lrb *lrbp)
{
 struct os44_completion_stats *s = &os44_ufs_xiaomi.stats;
 unsigned int dir, size_class;
 u64 latency, hist;
 u32 len;
 if (s->enabled != 1) return;
 switch (lrbp->cmd->cmnd[0]) {
 case READ_6: case READ_10: case READ_16: dir=0; break;
 case WRITE_6: case WRITE_10: case WRITE_16: dir=1; break;
 default: return;
 }
 spin_lock(&s->lock);
 latency = lrbp->compl_time_stamp - lrbp->issue_time_stamp;
 if ((u64)(lrbp->compl_time_stamp - s->timestamp) > s->interval) {
  s->timestamp = lrbp->compl_time_stamp;
  s->bank = (~s->bank) & 1;
  memset(s->normal[s->bank], 0, sizeof(s->normal[0]));
  memset(s->slow[s->bank], 0, sizeof(s->slow[0]));
 }
 len=lrbp->cmd->sdb.length >> 10;
 size_class=(len>16)+(len>128)+(len>256)+(len>512);
 if (latency < 1000000) hist=0;
 else if (latency < 5000000) hist=1;
 else if (latency < 10000000) hist=2;
 else if (latency < 50000000) hist=3;
 else hist=4;
 os44_account_cell(&s->normal[s->bank][dir][size_class],latency);
 os44_account_cell(&s->cumulative[dir][size_class],latency);
 if (latency > s->slow_us[dir][size_class]*1000) {
  os44_account_cell(&s->slow[s->bank][dir][size_class],latency);
  os44_account_cell(&s->cumulative_slow[dir][size_class],latency);
 }
 os44_account_cell(&s->histogram[dir][hist],latency);
 spin_unlock(&s->lock);
}
void os44_ufshcd_compl_one_cqe(struct ufs_hba *hba, int task_tag,
                            struct cq_entry *cqe)
{
 struct ufshcd_lrb *lrbp;
 struct scsi_cmnd *cmd;
 struct os44_mcq_idle_view *mcq;
 ktime_t now=ktime_get();
 BUG_ON(!hba);
 mcq=hba->priv;
 lrbp=&hba->lrb[task_tag];
 lrbp->compl_time_stamp=ktime_get();
 lrbp->compl_time_stamp_local_clock=local_clock();
 cmd=lrbp->cmd;
 if (cmd) {
  trace_android_vh_ufs_compl_command(hba,lrbp);
  if (unlikely(ufshcd_should_inform_monitor(hba,lrbp)))
   os44_ufshcd_update_monitor(hba,lrbp);
  os44_update_fastdiscard_time(hba,lrbp);
  os44_account_completion(lrbp);
  os44_ufshcd_add_command_trace(hba,task_tag,UFS_CMD_COMP);
  cmd->result=ufshcd_transfer_rsp_status(hba,lrbp,cqe);
  os44_ufshcd_release_scsi_cmd(hba,lrbp);
  scsi_done(cmd);
 } else if (hba->dev_cmd.complete) {
  trace_android_vh_ufs_compl_command(hba,lrbp);
  if (cqe)
   lrbp->utr_descriptor_ptr->header.ocs=le32_to_cpu(cqe->status)&MASK_OCS;
  complete(hba->dev_cmd.complete);
 }
 if (!mcq->active_cmds) {
  if (os44_ufs_data.last_completion) {
   u64 gap=max_t(s64,0,now-os44_ufs_data.last_completion);
   os44_ufs_data.max_completion_gap=max(os44_ufs_data.max_completion_gap,gap);
   os44_ufs_data.max_completion_gap_alltime=max(os44_ufs_data.max_completion_gap_alltime,gap);
  }
  os44_ufs_data.last_completion=now;
 }
 os44_ufscld_on_idle(hba);
}
