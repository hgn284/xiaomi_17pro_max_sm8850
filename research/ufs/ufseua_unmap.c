// SPDX-License-Identifier: GPL-2.0-only
/* Faithful source candidate, NOT a safety repair: original proceeds after get
 * failure/offline put and ignores numeric parse errors. No runtime deployment. */
#include <linux/slab.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include "os44_eua.h"
#include "ufseua_executor.h"
ssize_t os44_ufseua_sysfs_store_eua_file_lba_unmap(struct os44_eua_view *eua,const char *buf,size_t count)
{
 struct ufs_hba *hba=eua->hba;struct scsi_device *sdev=eua->sdev;
 struct scsi_sense_hdr sshdr={0};struct scsi_exec_args args={.sshdr=&sshdr,.req_flags=4};
 u8 data[24]={0},cmd[10]={0};char *allocation,*cursor,*token;
 unsigned long flags;unsigned long long value=0,start=0;u32 length=0;int index=1,ret;
 spin_lock_irqsave(hba->host->host_lock,flags);
 ret=scsi_device_get(sdev);
 if(!ret && !scsi_device_online(sdev))scsi_device_put(sdev);
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 allocation=kstrdup(buf,GFP_KERNEL);if(!allocation)goto error;
 cursor=allocation;
 while((token=strsep(&cursor,","))) {
  ret=kstrtoull(token,16,&value); /* return ignored by original */
  if(index&1)start=value;else length=value-start+1;
  index++;
 }
 printk("\0013[UFSEUA]unmap lba:0x%llx,length:%d",start,length);
 data[1]=22;data[3]=16;
 data[12]=start>>24;data[13]=start>>16;data[14]=start>>8;data[15]=start;
 data[16]=length>>24;data[17]=length>>16;data[18]=length>>8;data[19]=length;
 cmd[0]=0x42;cmd[8]=24;
 ret=os44_eua_scsi_execute(sdev,cmd,35,data,sizeof(data),7500,&args);
 if(ret) {
  printk("\0013[UFSEUA]UNMAP failed,sense key:0x%x;asc:0x%x;ascq:0x%x",sshdr.sense_key,sshdr.asc,sshdr.ascq);
  scsi_device_put(sdev);kfree(allocation);goto error;
 }
 scsi_device_put(sdev);kfree(allocation);
 printk("\0016[UFSEUA]unmap lba range success");return count;
error:printk("\0013[UFSEUA]unmap lba range failed");return -EINVAL;
}
