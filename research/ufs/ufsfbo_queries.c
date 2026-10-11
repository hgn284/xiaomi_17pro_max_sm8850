// SPDX-License-Identifier: GPL-2.0-only
#include <linux/pm_runtime.h>
#include <scsi/scsi_device.h>
#include "os44_fbo_query.h"
int os44_ufsfbo_read_desc(struct ufs_hba *hba,unsigned int idn,u8 *buf)
{
 int ret,i,size=255;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 for(i=0;i<3;i++) {
  ret=os44_query_descriptor_once(hba,1,idn,0,0,buf,&size);
  if(!ret || ret==-EINVAL)break;
 }
 if(ret) {
  printk("\0013[err]%s:%d:Read desc [0x%.2X] failed. (%d)\n","ufsfbo_read_desc",35,(u8)idn,ret);
  os44_query_error(1,0,-1,0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
int os44_ufsfbo_query_flag(struct ufs_hba *hba,unsigned int opcode,unsigned int idn,bool *value)
{
 int ret,i;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 for(i=0;i<3;i++) {
  ret=os44_ufshcd_query_flag(hba,opcode,(u8)idn,0,value);
  if(!ret)break;
  printk("\0013[err]%s:%d:Query flag [0x%.2X] failed. opcode(%d) (%d)\n","ufsfbo_query_flag",74,(u8)idn,opcode,ret);
  os44_query_error(1,0,-1,0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
