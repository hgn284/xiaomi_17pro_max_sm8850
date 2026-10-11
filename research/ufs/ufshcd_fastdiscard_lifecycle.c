/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction. The getter still imports the real shared owner;
 * no new owner storage. Original pre-null-check dereferences are preserved. */
#include "os44_remove_owner_view.h"
#include <linux/slab.h>
#include <linux/unaligned.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi.h>
void os44_fastdiscard_init(struct ufs_hba *hba)
{
 struct os44_fastdiscard_oem_view *oem=(void *)hba->android_oem_data1;
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 struct os44_fastdiscard_ctrl *ctrl=&owner->fastdiscard;
 if (!hba) {
  pr_err("[ufsfastdiscard] init hba ptr is NULL!!");
  return;
 }
 ctrl->hba=hba;
 ctrl->debug_enabled=false;
 ctrl->unmap_count=0;
 ctrl->unmap_time=0;
 ctrl->multi_unmap_count=0;
 ctrl->multi_unmap_time=0;
 ctrl->multi_unmap_lba=0;
 if (hba->android_oem_data1) {
  struct os44_fastdiscard_state_view *info=kzalloc(sizeof(*info),GFP_KERNEL);
  if (!info) {
   pr_err("fastdiscard_info Memory allocation failed = %d\n",-ENOMEM);
   return;
  }
  memset(info,0,sizeof(*info));
  oem->fastdiscard=info;
 }
 os44_fastdiscard_create_sysfs(ctrl);
}
void os44_update_fastdiscard_time(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
 struct scsi_cmnd *cmd=lrbp->cmd;
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 struct os44_fastdiscard_ctrl *ctrl=&owner->fastdiscard;
 u16 len;
 if (!hba) {
  pr_err("[ufs fastdiscard] hba ptr is NULL!!");
  return;
 }
 if (cmd->cmnd[0]!=UNMAP || ctrl->debug_enabled!=1) return;
 len=get_unaligned_be16(&cmd->cmnd[7]);
 if (len>24) {
  ctrl->multi_unmap_count++;
  ctrl->multi_unmap_lba+=(len-8)>>4;
  ctrl->multi_unmap_time+=ktime_to_ns(ktime_sub(lrbp->compl_time_stamp,lrbp->issue_time_stamp));
 } else {
  ctrl->unmap_count++;
  ctrl->unmap_time+=ktime_to_ns(ktime_sub(lrbp->compl_time_stamp,lrbp->issue_time_stamp));
 }
}
static_assert(offsetof(struct ufshcd_lrb,cmd)==0x40);
static_assert(offsetof(struct ufshcd_lrb,issue_time_stamp)==0x58);
static_assert(offsetof(struct ufshcd_lrb,compl_time_stamp)==0x68);
static_assert(offsetof(struct scsi_cmnd,cmnd)==0xb4);
