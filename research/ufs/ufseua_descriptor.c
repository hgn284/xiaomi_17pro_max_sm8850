// SPDX-License-Identifier: GPL-2.0-only
#include <linux/pm_runtime.h>
#include <scsi/scsi_device.h>
#include "os44_eua.h"
int os44_ufseua_read_desc(struct ufs_hba *hba,unsigned int idn,u8 *buf,int size)
{
 int ret,i;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 for(i=0;i<3;i++) {
  ret=os44_query_descriptor_once(hba,1,idn,0,0,buf,&size);
  if(!ret || ret==-EINVAL)break;
 }
 if(ret) {
  os44_query_error(1,3,-1,0);
  printk("\0013[UFSEUA]Read desc [0x%.2X] failed. (%d)",(u8)idn,ret);
 }
 pm_runtime_put_sync(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
