/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstructed OS44 issue0x17000..174d4, real typed source, not a mock issue. */
#include "ufsswap_provider_reconstruction.h"
#include <linux/completion.h>
#include <trace/hooks/ufshcd.h>
#include "os44_clock_trace.h"
extern void os44_core_send_command(struct ufs_hba *,unsigned int,struct ufs_hw_queue *);
extern int os44_core_clear_cmd(struct ufs_hba *,u32);
#define CHECK(m,v) static_assert(offsetof(struct ufs_hba,m)==(v))
CHECK(dev_cmd.complete,0x9a0); CHECK(dev_cmd.type,0x968);
CHECK(dev_cmd_queue,0x13d8); CHECK(mcq_enabled,0x131d);
CHECK(ufs_stats.last_hibern8_exit_tstamp,0x2c8);
CHECK(outstanding_lock,0x78); CHECK(outstanding_reqs,0x80);
static_assert(offsetof(struct ufshcd_lrb,task_tag)==0x50);
static_assert(sizeof(struct ufs_query_res)==20);
static
int ufshcd_copy_query_response(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_query_res *query_res = &hba->dev_cmd.query.response;

	memcpy(&query_res->upiu_res, &lrbp->ucd_rsp_ptr->qr, QUERY_OSF_SIZE);

	/* Get the descriptor */
	if (hba->dev_cmd.query.descriptor &&
	    lrbp->ucd_rsp_ptr->qr.opcode == UPIU_QUERY_OPCODE_READ_DESC) {
		u8 *descp = (u8 *)lrbp->ucd_rsp_ptr +
				GENERAL_UPIU_REQUEST_SIZE;
		u16 resp_len;
		u16 buf_len;

		/* data segment length */
		resp_len = be16_to_cpu(lrbp->ucd_rsp_ptr->header
				       .data_segment_length);
		buf_len = be16_to_cpu(
				hba->dev_cmd.query.request.upiu_req.length);
		if (likely(buf_len >= resp_len)) {
			memcpy(hba->dev_cmd.query.descriptor, descp, resp_len);
		} else {
			dev_warn(hba->dev,
				 "%s: rsp size %d is bigger than buffer size %d",
				 "ufshcd_copy_query_response", resp_len, buf_len);
			return -EINVAL;
		}
	}

	return 0;
}
static int
ufshcd_dev_cmd_completion(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	enum upiu_response_transaction resp;
	int err = 0;

	hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
	resp = lrbp->ucd_rsp_ptr->header.transaction_code;

	switch (resp) {
	case UPIU_TRANSACTION_NOP_IN:
		if (hba->dev_cmd.type != DEV_CMD_TYPE_NOP) {
			err = -EINVAL;
			dev_err(hba->dev, "%s: unexpected response %x\n",
					"ufshcd_dev_cmd_completion", resp);
		}
		break;
	case UPIU_TRANSACTION_QUERY_RSP: {
		u8 response = lrbp->ucd_rsp_ptr->header.response;

		if (response == 0) {
			err = ufshcd_copy_query_response(hba, lrbp);
		} else {
			err = -EINVAL;
			dev_err(hba->dev, "%s: unexpected response in Query RSP: %x\n",
					"ufshcd_dev_cmd_completion", response);
		}
		break;
	}
	case UPIU_TRANSACTION_REJECT_UPIU:
		/* TODO: handle Reject UPIU Response */
		err = -EPERM;
		dev_err(hba->dev, "%s: Reject UPIU not fully implemented\n",
				"ufshcd_dev_cmd_completion");
		break;
	case UPIU_TRANSACTION_RESPONSE:
		if (hba->dev_cmd.type != DEV_CMD_TYPE_RPMB) {
			err = -EINVAL;
			dev_err(hba->dev, "%s: unexpected response %x\n", "ufshcd_dev_cmd_completion", resp);
		}
		break;
	default:
		err = -EINVAL;
		dev_err(hba->dev, "%s: Invalid device management cmd response: %x\n",
				"ufshcd_dev_cmd_completion", resp);
		break;
	}
	trace_android_vh_ufs_dev_cmd_completion(hba, lrbp, err);
	return err;
}

static int os44_wait_for_dev_cmd(struct ufs_hba *hba,struct ufshcd_lrb *lrbp,int timeout)
{
 unsigned long left=msecs_to_jiffies(timeout),flags;
 bool pending,mcq;
 int err,cleared;
retry:
 left=wait_for_completion_timeout(hba->dev_cmd.complete,left);
 if (left) {
  hba->dev_cmd.complete=NULL;
  err=lrbp->utr_descriptor_ptr->header.ocs & 0xf;
  if (!err) err=ufshcd_dev_cmd_completion(hba,lrbp);
 } else {
  dev_err(hba->dev,"%s: dev_cmd request timedout, tag %d\n","ufshcd_wait_for_dev_cmd",lrbp->task_tag);
  /* Stock snapshots mode BEFORE calling clear, including timeout races. */
  mcq=hba->mcq_enabled;
  cleared=os44_core_clear_cmd(hba,lrbp->task_tag);
  err=cleared ? -ETIMEDOUT : -EAGAIN;
  if (mcq) {
   hba->dev_cmd.complete=NULL;
  } else {
   if (cleared) dev_err(hba->dev,"%s: failed to clear tag %d\n","ufshcd_wait_for_dev_cmd",lrbp->task_tag);
   spin_lock_irqsave(&hba->outstanding_lock,flags);
   pending=test_bit(lrbp->task_tag,&hba->outstanding_reqs);
   if (pending) {
    hba->dev_cmd.complete=NULL;
    if (!cleared) __clear_bit(lrbp->task_tag,&hba->outstanding_reqs);
   }
   spin_unlock_irqrestore(&hba->outstanding_lock,flags);
   if (!pending) { left=1; goto retry; }
  }
  trace_android_vh_ufs_compl_command(hba,lrbp);
 }
 return err;
}
int os44_core_issue_dev_cmd(struct ufs_hba *hba,struct ufshcd_lrb *lrbp,const u32 tag,int timeout)
{
 DECLARE_COMPLETION_ONSTACK(wait);
 int err;
 hba->dev_cmd.complete=&wait;
 trace_ufshcd_upiu(hba,UFS_QUERY_SEND,&lrbp->ucd_req_ptr->header,&lrbp->ucd_req_ptr->qr,UFS_TSF_OSF);
 os44_core_send_command(hba,tag,hba->dev_cmd_queue);
 err=os44_wait_for_dev_cmd(hba,lrbp,timeout);
 if (err) {
  if (hba->dev_cmd.type==DEV_CMD_TYPE_QUERY)
   os44_query_error(0,1,hba->dev_cmd.query.request.upiu_req.opcode,hba->dev_cmd.query.request.upiu_req.idn);
  os44_query_error(0,1,-1,0);
 }
 trace_ufshcd_upiu(hba,err ? UFS_QUERY_ERR : UFS_QUERY_COMP,&lrbp->ucd_rsp_ptr->header,&lrbp->ucd_rsp_ptr->qr,UFS_TSF_OSF);
 return err;
}
