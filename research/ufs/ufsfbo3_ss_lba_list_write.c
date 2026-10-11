// SPDX-License-Identifier: GPL-2.0-only
/* Reconstruction candidate preserves original failure propagation; not safety-qualified. */
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <linux/blk-mq.h>
#include <linux/slab.h>
#include "os44_fbo3_pin_write.h"
#include "ufsswap_partition_reconstruction.h"
int os44_ufsfbo3_ss_lba_list_write(struct os44_fbo_lifetime *fbo,const char *input)
{
 struct ufs_hba *hba=fbo->hba;
 struct scsi_device *sdev=fbo->sdev;
 struct scsi_sense_hdr sense={0};
 const struct scsi_exec_args args={.sshdr=&sense,.req_flags=BLK_MQ_REQ_PM};
 u8 cdb[10]={0};
 unsigned int captured_count=fbo->lr_count, len;
 unsigned long flags;
 u8 *buffer;
 int ret;
 buffer=kzalloc(4096,GFP_KERNEL);
 if(!buffer){printk("\0013[err]%s:%d:Alloc lba_buf fail\n","ufsfbo3_ss_lba_list_write",790);return -ENOMEM;}
 mutex_lock(&fbo->range_lock);
 ret=os44_ufsfbo_init_lba_buffer(fbo,input,buffer,false,true);
 if(ret){printk("\0013[err]%s:%d:init lba_buf fail\n","ufsfbo3_ss_lba_list_write",798);mutex_unlock(&fbo->range_lock);goto out;}

 mutex_unlock(&fbo->range_lock);
 spin_lock_irqsave(hba->host->host_lock,flags);
 if(!sdev || !scsi_device_online(sdev))ret=-ENODEV;
 else ret=scsi_device_get(sdev);
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 if(ret){printk("\0013[err]%s:%d:Get device fail\n","ufsfbo3_ss_lba_list_write",813);goto out;}
 hba->host->eh_noresume=1;
 len=(captured_count<<3)+12;
 cdb[0]=0x3b;cdb[1]=2;cdb[2]=1;
 cdb[6]=len>>16;cdb[7]=len>>8;cdb[8]=len;
 ret=os44_mi_scsi_execute_cmd(sdev,cdb,REQ_OP_DRV_OUT,buffer,len,3750,0,&args);
 if(ret)printk("\0013[err]%s:%d:Write Buffer failed,sense key:0x%x;asc:0x%x;ascq:0x%x\n","ufsfbo3_ss_lba_list_write",829,sense.sense_key,sense.asc,sense.ascq);
 scsi_device_put(sdev);
 hba->host->eh_noresume=0;
out:
 kfree(buffer);
 return ret;
}
