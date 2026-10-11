/* SPDX-License-Identifier: GPL-2.0-only */
/* OS44 MCQ cleanup0x28c38, stop0x28ee4, start0x28fd0. */
#include "ufsswap_provider_reconstruction.h"
#include <linux/blk-mq.h>
#include "os44_command_trace_contract.h"
#include <linux/bitfield.h>
#include <linux/iopoll.h>
#include <scsi/scsi_cmnd.h>
#include <trace/hooks/ufshcd.h>
#define MCQ_POLL_US 500000
#define CQE_UCD_BA GENMASK_ULL(63, 7)
static_assert(offsetof(struct ufs_hba,mcq_opr[OPR_SQD].stride)==0x13e8);
static_assert(offsetof(struct ufs_hba,mcq_opr[OPR_SQD].base)==0x13f0);
static_assert(offsetof(struct ufs_hw_queue,sq_mutex)==0x60);
static_assert(offsetof(struct ufs_hw_queue,id)==0x44);
static_assert(UFSHCD_QUIRK_MCQ_BROKEN_RTC==BIT(21));
static void __iomem *mcq_opr_base(struct ufs_hba *hba,enum ufshcd_mcq_opr n,int i)
{
 struct ufshcd_mcq_opr_info_t *opr=&hba->mcq_opr[n];
 return opr->base+opr->stride*i;
}
static struct ufs_hw_queue *os44_req_to_hwq(struct ufs_hba *hba,struct request *req)
{
 struct blk_mq_hw_ctx *hctx=READ_ONCE(req->mq_hctx);
 return hctx ? &hba->uhq[hctx->queue_num] : NULL;
}
static int ufshcd_mcq_sq_stop(struct ufs_hba *hba, struct ufs_hw_queue *hwq)
{
	void __iomem *reg;
	u32 id = hwq->id, val;
	int err;

	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_RTC)
		return -ETIMEDOUT;

	writel(SQ_STOP, mcq_opr_base(hba, OPR_SQD, id) + REG_SQRTC);
	reg = mcq_opr_base(hba, OPR_SQD, id) + REG_SQRTS;
	err = read_poll_timeout(readl, val, val & SQ_STS, 20,
				MCQ_POLL_US, false, reg);
	if (err)
		dev_err(hba->dev, "%s: failed. hwq-id=%d, err=%d\n",
			"ufshcd_mcq_sq_stop", id, err);
	return err;
}
static int ufshcd_mcq_sq_start(struct ufs_hba *hba, struct ufs_hw_queue *hwq)
{
	void __iomem *reg;
	u32 id = hwq->id, val;
	int err;

	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_RTC)
		return -ETIMEDOUT;

	writel(SQ_START, mcq_opr_base(hba, OPR_SQD, id) + REG_SQRTC);
	reg = mcq_opr_base(hba, OPR_SQD, id) + REG_SQRTS;
	err = read_poll_timeout(readl, val, !(val & SQ_STS), 20,
				MCQ_POLL_US, false, reg);
	if (err)
		dev_err(hba->dev, "%s: failed. hwq-id=%d, err=%d\n",
			"ufshcd_mcq_sq_start", id, err);
	return err;
}
int os44_core_mcq_sq_cleanup(struct ufs_hba *hba, int task_tag)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[task_tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct ufs_hw_queue *hwq;
	void __iomem *reg, *opr_sqd_base;
	u32 nexus, id, val, rtc;
	int err;

	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_RTC)
		return -ETIMEDOUT;

	if (task_tag != hba->nutrs - UFSHCD_NUM_RESERVED) {
		if (!cmd)
			return -EINVAL;
		hwq = os44_req_to_hwq(hba, scsi_cmd_to_rq(cmd));
		if (!hwq)
			return 0;
	} else {
		hwq = hba->dev_cmd_queue;
	}

	/* No Android BROKEN_ICU SQE search branch exists in OS44 stock. */

	id = hwq->id;

	mutex_lock(&hwq->sq_mutex);

	trace_android_vh_ufs_mcq_cleanup(hba, task_tag, true);

	/* stop the SQ fetching before working on it */
	err = ufshcd_mcq_sq_stop(hba, hwq);
	if (err)
		goto unlock;

	/* SQCTI = EXT_IID, IID, LUN, Task Tag */
	nexus = lrbp->ucd_req_ptr->header.iid << 16 | lrbp->lun << 8 | task_tag;
	opr_sqd_base = mcq_opr_base(hba, OPR_SQD, id);
	writel(nexus, opr_sqd_base + REG_SQCTI);

	/* Initiate Cleanup */
	writel(readl(opr_sqd_base + REG_SQRTC) | SQ_ICU,
		opr_sqd_base + REG_SQRTC);

	/* Poll SQRTSy.CUS = 1. Return result from SQRTSy.RTC */
	reg = opr_sqd_base + REG_SQRTS;
	err = read_poll_timeout(readl, val, val & SQ_CUS, 20,
				MCQ_POLL_US, false, reg);
	rtc = FIELD_GET(SQ_ICU_ERR_CODE_MASK, readl(reg));
	if (err || rtc)
		dev_err(hba->dev, "%s: failed. hwq=%d, tag=%d err=%d RTC=%d\n",
			"ufshcd_mcq_sq_cleanup", id, task_tag, err, rtc);

	if (ufshcd_mcq_sq_start(hba, hwq))
		err = -ETIMEDOUT;

unlock:
	trace_android_vh_ufs_mcq_cleanup(hba, task_tag, false);
	mutex_unlock(&hwq->sq_mutex);
	return err;
}

struct ufs_hw_queue *os44_ufshcd_mcq_req_to_hwq(struct ufs_hba *hba,struct request *req)
{
 struct blk_mq_hw_ctx *hctx=READ_ONCE(req->mq_hctx);
 return hctx ? &hba->uhq[hctx->queue_num] : NULL;
}

#include <scsi/scsi_host.h>
#include "os44_abort_contract.h"
#include "os44_mcq_abort_contract.h"
#include "os44_completion_contract.h"
static void ufshcd_mcq_nullify_sqe(struct utp_transfer_req_desc *utrd)
{
	utrd->header.command_type = 0xf;
}

/**
 * ufshcd_mcq_sqe_search - Search for the command in the submission queue
 * If the command is in the submission queue and not issued to the device yet,
 * nullify the sqe so the host controller will skip fetching the sqe.
 *
 * @hba: per adapter instance.
 * @hwq: Hardware Queue to be searched.
 * @task_tag: The command's task tag.
 *
 * Return: true if the SQE containing the command is present in the SQ
 * (not fetched by the controller); returns false if the SQE is not in the SQ.
 */
static bool ufshcd_mcq_sqe_search(struct ufs_hba *hba,
				  struct ufs_hw_queue *hwq, int task_tag)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[task_tag];
	struct utp_transfer_req_desc *utrd;
	__le64  cmd_desc_base_addr;
	bool ret = false;
	u64 addr, match;
	u32 sq_head_slot;

	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_RTC)
		return true;

	mutex_lock(&hwq->sq_mutex);

	ufshcd_mcq_sq_stop(hba, hwq);
	sq_head_slot = (readl(hwq->mcq_sq_head) / sizeof(struct utp_transfer_req_desc));
	if (sq_head_slot == hwq->sq_tail_slot)
		goto out;

	cmd_desc_base_addr = lrbp->utr_descriptor_ptr->command_desc_base_addr;
	addr = le64_to_cpu(cmd_desc_base_addr) & CQE_UCD_BA;

	while (sq_head_slot != hwq->sq_tail_slot) {
		utrd = hwq->sqe_base_addr + sq_head_slot;
		match = le64_to_cpu(utrd->command_desc_base_addr) & CQE_UCD_BA;
		if (addr == match) {
			ufshcd_mcq_nullify_sqe(utrd);
			ret = true;
			goto out;
		}

		sq_head_slot++;
		if (sq_head_slot == hwq->max_entries)
			sq_head_slot = 0;
	}

out:
	ufshcd_mcq_sq_start(hba, hwq);
	mutex_unlock(&hwq->sq_mutex);
	return ret;
}


/* Complete original MCQ abort behavior, including source-owned release under
 * CQ lock. Original ignores stop/start errors in SQE search; preserved. */
int os44_ufshcd_mcq_abort(struct scsi_cmnd *cmd)
{
 struct Scsi_Host *host = cmd->device->host;
 struct ufs_hba *hba = shost_priv(host);
 int tag = scsi_cmd_to_rq(cmd)->tag;
 struct ufshcd_lrb *lrbp = &hba->lrb[tag];
 struct ufs_hw_queue *hwq;
 unsigned long flags;
 int err;
 if (!os44_ufshcd_cmd_inflight(lrbp->cmd)) {
  dev_err(hba->dev, "%s: skip abort. cmd at tag %d already completed.\n", "ufshcd_mcq_abort", tag);
  return FAILED;
 }
 if (lrbp->req_abort_skip) {
  dev_err(hba->dev, "%s: skip abort. tag %d failed earlier\n", "ufshcd_mcq_abort", tag);
  return FAILED;
 }
 hwq = os44_req_to_hwq(hba, scsi_cmd_to_rq(cmd));
 if (ufshcd_mcq_sqe_search(hba, hwq, tag)) {
  dev_err(hba->dev, "%s: cmd found in sq. hwq=%d, tag=%d\n", "ufshcd_mcq_abort", hwq->id, tag);
  return FAILED;
 }
 err = os44_ufshcd_try_to_abort_task(hba, tag);
 if (err) {
  dev_err(hba->dev, "%s: device abort failed %d\n", "ufshcd_mcq_abort", err);
  lrbp->req_abort_skip = true;
  return FAILED;
 }
 spin_lock_irqsave(&hwq->cq_lock, flags);
 if (os44_ufshcd_cmd_inflight(lrbp->cmd))
  os44_ufshcd_release_scsi_cmd(hba, lrbp);
 spin_unlock_irqrestore(&hwq->cq_lock, flags);
 return SUCCESS;
}
