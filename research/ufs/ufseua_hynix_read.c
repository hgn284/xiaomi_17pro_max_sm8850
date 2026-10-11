// SPDX-License-Identifier: GPL-2.0-only
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <scsi/scsi_host.h>
#include "os44_eua.h"
#include "ufseua_executor.h"
int os44_hynix_eua_get_mapping_status(struct os44_eua_view *eua,u8 *buf)
{
 struct ufs_hba *hba=eua->hba;struct scsi_device *sdev=eua->sdev;
 struct scsi_sense_hdr sense={0};const struct scsi_exec_args args={.sshdr=&sense,.req_flags=4};
 u8 cmd[16]={0};unsigned long flags;int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ufshcd_hold(hba);
 spin_lock_irqsave(hba->host->host_lock,flags);
 ret=scsi_device_get(sdev);
 if(!ret && !scsi_device_online(sdev)){scsi_device_put(sdev);ret=-ENODEV;}
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 if(ret){printk("\0013[UFSEUA]Get device fail");goto out;}
 cmd[0]=0x3c;cmd[1]=1;
 cmd[2]=7;cmd[6]=0;cmd[7]=eua->mapping_length>>8;cmd[8]=eua->mapping_length;
 cmd[10]=eua->mapping_start>>24;cmd[11]=eua->mapping_start>>16;cmd[12]=eua->mapping_start>>8;cmd[13]=eua->mapping_start;
 cmd[14]=eua->mapping_length>>8;cmd[15]=eua->mapping_length;
 ret=os44_eua_scsi_execute(sdev,cmd,34,buf,eua->mapping_length,3750,&args);
 if(ret)printk("\0013[UFSEUA]Read Buffer failed,sense key:0x%x;asc:0x%x;ascq:0x%x",sense.sense_key,sense.asc,sense.ascq);
 scsi_device_put(sdev);
out:ufshcd_release(hba);pm_runtime_put_sync(&hba->ufs_device_wlun->sdev_gendev);return ret;
}
int os44_hynix_eua_get_total_used_lba(struct os44_eua_view *eua,u32 *buf)
{
 struct ufs_hba *hba=eua->hba;struct scsi_device *sdev=eua->sdev;
 struct scsi_sense_hdr sense={0};const struct scsi_exec_args args={.sshdr=&sense,.req_flags=4};
 u8 cmd[16]={0};unsigned long flags;int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ufshcd_hold(hba);
 spin_lock_irqsave(hba->host->host_lock,flags);
 ret=scsi_device_get(sdev);
 if(!ret && !scsi_device_online(sdev)){scsi_device_put(sdev);ret=-ENODEV;}
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 if(ret){printk("\0013[UFSEUA]Get device fail");goto out;}
 cmd[0]=0x3c;cmd[1]=1;
 cmd[2]=8;cmd[8]=4;
 ret=os44_eua_scsi_execute(sdev,cmd,34,buf,4,3750,&args);
 if(ret)printk("\0013[UFSEUA]Read Buffer failed,sense key:0x%x;asc:0x%x;ascq:0x%x",sense.sense_key,sense.asc,sense.ascq);
 scsi_device_put(sdev);
out:ufshcd_release(hba);pm_runtime_put_sync(&hba->ufs_device_wlun->sdev_gendev);return ret;
}
