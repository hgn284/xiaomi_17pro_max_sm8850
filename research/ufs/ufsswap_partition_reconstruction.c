/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole stock READ32cb8..32ebc / WRITE33020..33250 reconstruction.
 * Faithful exceptional behavior is deliberately NOT silently repaired.
 * No physical-I/O claim or registration into a live driver. */
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/printk.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "ufsswap_debug_reconstruction.h"
#include "ufsswap_partition_reconstruction.h"
static_assert(offsetof(struct scsi_device,host)==0);
static_assert(offsetof(struct scsi_device,sdev_state)==0x978);
static_assert(offsetof(struct Scsi_Host,hostdata)==0x9a8);
static_assert(offsetof(struct Scsi_Host,host_lock)==0x38);
static_assert(offsetof(struct ufs_hba,host)==0x38);
static_assert(offsetof(struct ufs_hba,dev)==0x40);
static_assert(sizeof(struct scsi_exec_args)==48);
static_assert(offsetof(struct scsi_exec_args,sshdr)==16);
static_assert(sizeof(struct scsi_sense_hdr)==8);
/* Stock accesses a u16 in the Scsi_Host flag storage. This is a bounded
 * layout view; its private field identity is not inferred from bit0x40. */
static u16 *flag_word(struct ufs_hba *hba)
{ return (u16 *)((u8 *)hba->host+0x224); }
static bool offline(struct scsi_device *sdev)
{ return sdev->sdev_state==4 || sdev->sdev_state==6 || sdev->sdev_state==7; }
static void offline_log(struct ufs_hba *hba,struct scsi_device *sdev,int ret)
{
 dev_err(hba->dev,"os44 partition offline %u\n",*(u32 *)((u8 *)hba+0x1f0));
 pr_err("os44 partition get=%d online=%d\n",ret,!offline(sdev));
}
static int transfer(struct scsi_device *sdev,struct ufs_hba *hba,
 void *buffer,u32 lba,u32 count,bool write)
{
 unsigned char cdb[16]={0};
 struct scsi_sense_hdr sense={0};
 struct scsi_exec_args args={.sshdr=&sense};
 int ret;
 cdb[0]=write?0x2a:0x28;
 cdb[2]=lba>>24;cdb[3]=lba>>16;cdb[4]=lba>>8;cdb[5]=lba;
 cdb[7]=write?0:count>>8;cdb[8]=count;
 ret=os44_mi_scsi_execute_cmd(sdev,cdb,write?35:34,buffer,count<<12,3750,3,&args);
 if (ret) pr_err("os44 partition I/O error %d\n",ret);
 sdev_printk(KERN_ERR,sdev,"sense %u %u %u\n",sense.sense_key,sense.asc,sense.ascq);
 scsi_device_put(sdev);
 *flag_word(hba)&=~0x40;
 return ret;
}
int os44_debug_read_partition(struct scsi_device *sdev,void *buffer,u32 lba,u32 count)
{
 struct ufs_hba *hba=shost_priv(sdev->host);
 unsigned long flags;int ret;
 spin_lock_irqsave(hba->host->host_lock,flags);
 ret=scsi_device_get(sdev);
 /* Stock READ only tests offline after failed get. */
 if (ret && offline(sdev)) {
  scsi_device_put(sdev);offline_log(hba,sdev,ret);
  spin_unlock_irqrestore(hba->host->host_lock,flags);return ret;
 }
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 *flag_word(hba)|=0x40;
 return transfer(sdev,hba,buffer,lba,count,false);
}
int os44_debug_write_partition(struct scsi_device *sdev,void *buffer,u32 lba,u32 count)
{
 struct ufs_hba *hba=shost_priv(sdev->host);
 unsigned long flags;int ret;
 spin_lock_irqsave(hba->host->host_lock,flags);
 ret=scsi_device_get(sdev);
 if (ret) {
  spin_unlock_irqrestore(hba->host->host_lock,flags);
  pr_err("os44 partition get failed\n");return ret;
 }
 if (offline(sdev)) {
  scsi_device_put(sdev);offline_log(hba,sdev,0);
  spin_unlock_irqrestore(hba->host->host_lock,flags);return 0;
 }
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 *flag_word(hba)|=0x40;
 /* Exact stock early return: no put or flag clear within this body. */
 if (count>128) return -EINVAL;
 return transfer(sdev,hba,buffer,lba,count,true);
}
