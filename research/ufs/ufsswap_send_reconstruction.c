/* SPDX-License-Identifier: GPL-2.0-only */
/* OS44 stock send0x1fe50..200f0; source paired with true SQ/MMIO writes. */
#include "ufsswap_provider_reconstruction.h"
#include <linux/blk-mq.h>
#include <linux/sched/clock.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <trace/hooks/ufshcd.h>
#include "os44_clock_trace.h"
#define CHECK(m,v) static_assert(offsetof(struct ufs_hba,m)==(v))
CHECK(clk_scaling.active_reqs,0xbc8); CHECK(clk_scaling.window_start_t,0xbd8);
CHECK(clk_scaling.is_enabled,0xc80); CHECK(clk_scaling.is_busy_started,0xc83);
CHECK(monitor.enabled,0x11a0); CHECK(monitor.chunk_size,0x1118);
CHECK(monitor.enabled_ts,0x1198); CHECK(monitor.nr_queued,0x1180);
CHECK(monitor.busy_start_ts,0x1188);
static_assert(offsetof(struct ufs_hw_queue,sq_tail_slot)==0x48);
static_assert(offsetof(struct ufs_hw_queue,sqe_base_addr)==0x20);
static_assert(offsetof(struct ufshcd_lrb,issue_time_stamp)==0x58);
static struct ufs_hw_queue *os44_req_to_hwq(struct ufs_hba *hba,struct request *req)
{
 struct blk_mq_hw_ctx *hctx=READ_ONCE(req->mq_hctx);
 return hctx ? &hba->uhq[hctx->queue_num] : NULL;
}
static void ufshcd_add_cmd_upiu_trace(struct ufs_hba *hba, unsigned int tag,
				      enum ufs_trace_str_t str_t)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;
	struct utp_upiu_header *header;

	if (!trace_ufshcd_upiu_enabled())
		return;

	if (str_t == UFS_CMD_SEND)
		header = &rq->header;
	else
		header = &hba->lrb[tag].ucd_rsp_ptr->header;

	trace_ufshcd_upiu(hba, str_t, header, &rq->sc.cdb,
			  UFS_TSF_CDB);
}
static void ufshcd_add_command_trace(struct ufs_hba *hba, unsigned int tag,
				     enum ufs_trace_str_t str_t)
{
	u64 lba = 0;
	u8 opcode = 0, group_id = 0;
	u32 doorbell = 0;
	u32 intr;
	int hwq_id = -1;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct request *rq = scsi_cmd_to_rq(cmd);
	int transfer_len = -1;

	if (!cmd)
		return;

	/* trace UPIU also */
	ufshcd_add_cmd_upiu_trace(hba, tag, str_t);
	if (!trace_ufshcd_command_enabled())
		return;

	opcode = cmd->cmnd[0];

	if (opcode == READ_10 || opcode == WRITE_10 ||
				opcode == READ_16 || opcode == WRITE_16) {
		transfer_len =
		       be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
		lba = scsi_get_lba(cmd);
		if (opcode == WRITE_10)
			group_id = lrbp->cmd->cmnd[6];
		if (opcode == WRITE_16)
			group_id = lrbp->cmd->cmnd[14];
	} else if (opcode == UNMAP) {
		/*
		 * The number of Bytes to be unmapped beginning with the lba.
		 */
		transfer_len = blk_rq_bytes(rq);
		lba = scsi_get_lba(cmd);
	}

	intr = ufshcd_readl(hba, REG_INTERRUPT_STATUS);

	if (hba->mcq_enabled) {
		struct ufs_hw_queue *hwq = os44_req_to_hwq(hba, rq);

		hwq_id = hwq->id;
	} else {
		doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	}
	trace_ufshcd_command(cmd->device, hba, str_t, tag, doorbell, hwq_id,
			     transfer_len, intr, lba, opcode, group_id);
}
static void ufshcd_clk_scaling_start_busy(struct ufs_hba *hba)
{
	bool queue_resume_work = false;
	ktime_t curr_t = ktime_get();
	unsigned long flags;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!hba->clk_scaling.active_reqs++)
		queue_resume_work = true;

	if (!hba->clk_scaling.is_enabled || hba->pm_op_in_progress) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		return;
	}

	if (queue_resume_work)
		queue_work(hba->clk_scaling.workq,
			   &hba->clk_scaling.resume_work);

	if (!hba->clk_scaling.window_start_t) {
		hba->clk_scaling.window_start_t = curr_t;
		hba->clk_scaling.tot_busy_t = 0;
		hba->clk_scaling.is_busy_started = false;
	}

	if (!hba->clk_scaling.is_busy_started) {
		hba->clk_scaling.busy_start_t = curr_t;
		hba->clk_scaling.is_busy_started = true;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}
static inline int ufshcd_monitor_opcode2dir(u8 opcode)
{
	if (opcode == READ_6 || opcode == READ_10 || opcode == READ_16)
		return READ;
	else if (opcode == WRITE_6 || opcode == WRITE_10 || opcode == WRITE_16)
		return WRITE;
	else
		return -EINVAL;
}
static inline bool ufshcd_should_inform_monitor(struct ufs_hba *hba,
						struct ufshcd_lrb *lrbp)
{
	const struct ufs_hba_monitor *m = &hba->monitor;

	return (m->enabled && lrbp && lrbp->cmd &&
		(!m->chunk_size || m->chunk_size == lrbp->cmd->sdb.length) &&
		ktime_before(hba->monitor.enabled_ts, lrbp->issue_time_stamp));
}
static void ufshcd_start_monitor(struct ufs_hba *hba,
				 const struct ufshcd_lrb *lrbp)
{
	int dir = ufshcd_monitor_opcode2dir(*lrbp->cmd->cmnd);
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (dir >= 0 && hba->monitor.nr_queued[dir]++ == 0)
		hba->monitor.busy_start_ts[dir] = ktime_get();
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}
static inline void ufshcd_inc_sq_tail(struct ufs_hw_queue *q)
	__must_hold(&q->sq_lock)
{
	u32 val;

	q->sq_tail_slot++;
	if (q->sq_tail_slot == q->max_entries)
		q->sq_tail_slot = 0;
	val = q->sq_tail_slot * sizeof(struct utp_transfer_req_desc);
	writel(val, q->mcq_sq_tail);
}
void os44_core_send_command(struct ufs_hba *hba, unsigned int task_tag,
			 struct ufs_hw_queue *hwq)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[task_tag];
	unsigned long flags;

	lrbp->issue_time_stamp = ktime_get();
	lrbp->issue_time_stamp_local_clock = local_clock();
	lrbp->compl_time_stamp = ktime_set(0, 0);
	lrbp->compl_time_stamp_local_clock = 0;
	trace_android_vh_ufs_send_command(hba, lrbp);
	ufshcd_add_command_trace(hba, task_tag, UFS_CMD_SEND);
	if (lrbp->cmd)
		ufshcd_clk_scaling_start_busy(hba);
	if (unlikely(ufshcd_should_inform_monitor(hba, lrbp)))
		ufshcd_start_monitor(hba, lrbp);

	if (hba->mcq_enabled) {
		int utrd_size = sizeof(struct utp_transfer_req_desc);
		struct utp_transfer_req_desc *src = lrbp->utr_descriptor_ptr;
		struct utp_transfer_req_desc *dest;

		spin_lock(&hwq->sq_lock);
		dest = hwq->sqe_base_addr + hwq->sq_tail_slot;
		memcpy(dest, src, utrd_size);
		ufshcd_inc_sq_tail(hwq);
		spin_unlock(&hwq->sq_lock);
	} else {
		spin_lock_irqsave(&hba->outstanding_lock, flags);
		if (hba->vops && hba->vops->setup_xfer_req)
			hba->vops->setup_xfer_req(hba, lrbp->task_tag,
						  !!lrbp->cmd);
		__set_bit(lrbp->task_tag, &hba->outstanding_reqs);
		ufshcd_writel(hba, 1 << lrbp->task_tag,
			      REG_UTP_TRANSFER_REQ_DOOR_BELL);
		spin_unlock_irqrestore(&hba->outstanding_lock, flags);
	}
}
