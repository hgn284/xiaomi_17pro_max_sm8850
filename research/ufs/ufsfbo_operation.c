// SPDX-License-Identifier: GPL-2.0-only
#include <linux/pm_runtime.h>
#include "os44_fbo_operation.h"
#include "ufsswap_query_reconstruction.h"
int os44_ufsfbo_operation_control(struct os44_fbo_lifetime *fbo,int *ctrl)
{
 struct ufs_hba *hba;
 int ret;
 if(*ctrl>15)return -EINVAL;
 hba=fbo->hba;
 if((unsigned int)(*ctrl-3)<=8 && fbo->vendor_selector==5) {
  if(fbo->debug==1)printk("\0016[info]%s:%d:Vendor %d Not Support ctrl val[0x%x]\n","ufsfbo_operation_control",120,5,*ctrl);
  return 0;
 }
 if((unsigned int)(*ctrl-5)<=6 && fbo->vendor_selector==4) {
  if(fbo->debug==1)printk("\0016[info]%s:%d:Vendor %d Not Support ctrl val[0x%x]\n","ufsfbo_operation_control",127,4,*ctrl);
  return 0;
 }
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ret=os44_query_attr(hba,4,0x31,0,0,(u32 *)ctrl);
 if(ret) {
  printk("\0013[err]%s:%d:Query attr [0x%.2X] failed. opcode(%d) (%d)\n","ufsfbo_query_attr",55,0x31,4,ret);
  os44_query_error(1,0,-1,0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 if(ret)printk("\0013[err]%s:%d:Query fbo control attr failed. ret(%d), val[0x%x]\n","ufsfbo_operation_control",133,ret,*ctrl);
 return ret;
}
