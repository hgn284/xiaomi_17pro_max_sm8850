/* SPDX-License-Identifier: GPL-2.0-only */
/* Stock 0x11b40..0x11cfc. Actual command issue/completion remains an explicit dependency. */
#include "ufsswap_provider_reconstruction.h"
#define CHECK(m,v) static_assert(offsetof(struct ufs_hba,m)==(v))
CHECK(lrb,0x68); CHECK(reserved_slot,0x9c); CHECK(dev_cmd.type,0x968);
CHECK(android_quirks,0xd8);
static_assert(sizeof(struct ufshcd_lrb)==0x98);
static_assert(offsetof(struct ufshcd_lrb,crypto_key_slot)==0x78);
static_assert(sizeof(struct utp_upiu_req)==0x20);
static_assert(sizeof(struct utp_upiu_rsp)==0x34);
int os44_core_exec_dev_cmd(struct ufs_hba *hba,unsigned int kind,int timeout)
{
 u32 tag=hba->reserved_slot;
 struct ufshcd_lrb *lrb=&hba->lrb[tag];
 struct utp_transfer_req_desc *tr=lrb->utr_descriptor_ptr;
 struct utp_upiu_req *req=lrb->ucd_req_ptr;
 struct ufs_query *query=&hba->dev_cmd.query;
 lrb->cmd=NULL; lrb->task_tag=tag; lrb->lun=0; lrb->intr_cmd=true;
 lrb->crypto_key_slot=-1;
 hba->dev_cmd.type=kind;
 lrb->command_type=UTP_CMD_TYPE_UFS_STORAGE;
 tr->header=(struct request_desc_header){
  .command_type=UTP_CMD_TYPE_UFS_STORAGE,.interrupt=1,
  .ocs=OCS_INVALID_COMMAND_STATUS
 };
 tr->prd_table_length=0;
 if (kind==DEV_CMD_TYPE_QUERY) {
  req->header=(struct utp_upiu_header){
   .transaction_code=UPIU_TRANSACTION_QUERY_REQ,
   .lun=lrb->lun,.task_tag=lrb->task_tag,
   .query_function=query->request.query_func,
   .data_segment_length=query->request.upiu_req.opcode==UPIU_QUERY_OPCODE_WRITE_DESC ? query->request.upiu_req.length : 0
  };
  memcpy(&req->qr,&query->request.upiu_req,QUERY_OSF_SIZE);
  if(query->request.upiu_req.opcode==UPIU_QUERY_OPCODE_WRITE_DESC)
   memcpy(req+1,query->descriptor,be16_to_cpu(query->request.upiu_req.length));
 } else if(kind==DEV_CMD_TYPE_NOP) {
  memset(req,0,sizeof(*req));
  req->header.task_tag=lrb->task_tag;
 } else return -EINVAL;
 memset(lrb->ucd_rsp_ptr,0,sizeof(*lrb->ucd_rsp_ptr));
 if(hba->android_quirks & UFSHCD_ANDROID_QUIRK_SET_IID_TO_ONE)
  req->header.iid=1;
 return os44_core_issue_dev_cmd(hba,lrb,tag,timeout);
}
