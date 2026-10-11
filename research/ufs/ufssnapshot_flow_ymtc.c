// SPDX-License-Identifier: GPL-2.0-only
/* Original flags0xdc0 = GFP_KERNEL | __GFP_ZERO; zero all68 bytes. */
#include <linux/slab.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include "ufssnapshot_flow.h"
#include "ufssnapshot_executor.h"
#include "os44_xiaomi_owner.h"
int os44_ufsnapshot_backup_flow_ymtc(struct os44_snapshot_view *s)
{
 struct scsi_device *sdev=os44_get_ufs_xiaomi_for_remove()->snapshot.sdev;
 struct scsi_sense_hdr sshdr={0};
 struct scsi_exec_args args={.sshdr=&sshdr,.req_flags=4};
 u8 cmd[16]={0x3b,0xe1,0,0,0,0,0,0,0x2c};
 u8 *buffer;int ret;
 ret=os44_ufssnapshot_start(s);
 if(ret) {printk("\0013[err]%s: %d:[ufssnapshot] YMTC start back up failed, err 0x%x\n","ufsnapshot_backup_flow_ymtc",409,ret);return ret;}
 buffer=kzalloc(68,GFP_KERNEL);
 if(!buffer) {ret=-ENOMEM;goto fail;}
 buffer[0]=0xb1;buffer[1]=0xd0;buffer[2]=0x2c;
 ret=os44_ufssnapshot_scsi_execute(sdev,cmd,35,buffer,44,&args);
 if(ret) {
  sdev_prefix_printk("\0013",sdev,NULL,"sense key:0x%x; asc:0x%x; ascq:0x%x\n",sshdr.sense_key,sshdr.asc,sshdr.ascq);
  kfree(buffer);ret=-EIO;goto fail;
 }
 kfree(buffer);
 ret=os44_ufssnapshot_active(s);
 if(ret)printk("\0013[err]%s: %d:[ufssnapshot] YMTC start back up failed, err 0x%x\n","ufsnapshot_backup_flow_ymtc",421,ret);
 return ret;
fail:
 printk("\0013[err]%s: %d:[ufssnapshot] YMTC vendor cmd fail 0x%x\n\n","ufsnapshot_backup_flow_ymtc",415,ret);return ret;
}
