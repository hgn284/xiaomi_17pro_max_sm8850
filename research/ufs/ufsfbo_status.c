// SPDX-License-Identifier: GPL-2.0-only
/* Equivalent reconstruction from whole stock functions. No owner substitute. */
#include <linux/pm_runtime.h>
#include "os44_fbo_status.h"
#include "ufsswap_query_reconstruction.h"
int os44_ufsfbs_get_ops(struct os44_fbo_lifetime *fbo,int *ops)
{
 struct ufs_hba *hba=fbo->hba;
 u32 value=~0U;
 int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ret=os44_query_attr(hba,3,58,0,0,&value);
 if(ret) {
  printk(KERN_ERR "[err]%s:%d:Query attr [0x%.2X] failed. opcode(%d) (%d)\n","ufsfbo_query_attr",55,58,3,ret);
  os44_query_error(1,0,-1,0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 if(ret)return ret;
 switch((int)value) {
 case 0:case 1:case 2:case 3:case 4:case 5:case 255:
  *ops=value;return 0;
 default:
  if(fbo->debug==1)
   printk(KERN_INFO "[info]%s:%d:unknown defrag ops %d\n\n","ufsfbs_get_ops",320,value);
  return -1;
 }
}
int os44_ufsfbs_get_level_check_ops(struct os44_fbo_lifetime *fbo,int *ops)
{
 struct ufs_hba *hba=fbo->hba;
 u32 value=~0U;
 int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ret=os44_query_attr(hba,3,57,0,0,&value);
 if(ret) {
  printk(KERN_ERR "[err]%s:%d:Query attr [0x%.2X] failed. opcode(%d) (%d)\n","ufsfbo_query_attr",55,57,3,ret);
  os44_query_error(1,0,-1,0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 if(ret)return ret;
 switch((int)value) {
 case 0:case 1:case 2:case 3:case 4:case 5:case 255:
  *ops=value;return 0;
 default:
  if(fbo->debug==1)
   printk(KERN_INFO "[info]%s:%d:unknown level ops %d\n\n","ufsfbs_get_level_check_ops",290,value);
  return -1;
 }
}
