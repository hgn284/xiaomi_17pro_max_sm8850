// SPDX-License-Identifier: GPL-2.0-only
/* Source-owned reconstruction; original private type identity unresolved. */
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <linux/blk-mq.h>
#include "os44_fbo_frag.h"
#include "ufsswap_partition_reconstruction.h"
int os44_ufsfbo_read_frag_level(struct os44_fbo_lifetime *fbo,u8 *buffer)
{
 struct ufs_hba *hba=fbo->hba;
 struct scsi_device *sdev=fbo->sdev;
 struct scsi_sense_hdr sense={0};
 const struct scsi_exec_args args={.sshdr=&sense,.req_flags=BLK_MQ_REQ_PM};
 u8 cdb[16]={0};
 unsigned long flags;
 unsigned int len;
 int ret;
 if(!fbo->lr_count)printk("\0013[err]%s:%d:Invaild lba range count:%d\n","ufsfbo_read_frag_level",413,0);
 len=(fbo->is_ufs31?8:12)+(fbo->lr_count<<3);
 if(fbo->vendor_selector==5)len=4096;
 spin_lock_irqsave(hba->host->host_lock,flags);
 if(!sdev || !scsi_device_online(sdev))ret=-ENODEV;
 else ret=scsi_device_get(sdev);
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 if(ret){printk("\0013[err]%s:%d:Get device fail\n","ufsfbo_read_frag_level",430);return ret;}
 hba->host->eh_noresume=1;
 cdb[0]=0x3c;
 if(fbo->is_ufs31)cdb[1]=0x1d;
 else {cdb[1]=2;cdb[2]=2;}
 cdb[6]=len>>16;cdb[7]=len>>8;cdb[8]=len;
 ret=os44_mi_scsi_execute_cmd(sdev,cdb,REQ_OP_DRV_IN,buffer,len,3750,0,&args);
 if(ret)printk("\0013[err]%s:%d:Read Buffer failed,sense key:0x%x;asc:0x%x;ascq:0x%x\n","ufsfbo_read_frag_level",448,sense.sense_key,sense.asc,sense.ascq);
 scsi_device_put(sdev);
 hba->host->eh_noresume=0;
 return ret;
}
