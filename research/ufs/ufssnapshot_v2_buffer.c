// SPDX-License-Identifier: GPL-2.0-only
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include "ufsswap_partition_reconstruction.h"
#include "ufsswap_query_reconstruction.h"
#include "os44_snapshot_v2.h"
int os44_ufssnapshotV2_rw_buffer(struct os44_snapshot_v2_view *s,
 u8 mode, int op, u8 buffer_id, u32 len, u8 *buffer, bool is_read)
{
 struct scsi_sense_hdr sshdr={0};
 const struct scsi_exec_args args={.sshdr=&sshdr};
 u8 cdb[16]={0};
 struct scsi_device *sdev=s->sdev;
 int ret, i;
 cdb[0]=is_read ? 0x3c : 0x3b;
 cdb[1]=(mode & 0x1f) | ((unsigned int)op << 5);
 cdb[2]=buffer_id;
 cdb[6]=len >> 16; cdb[7]=len >> 8; cdb[8]=len;
 if(s->debug==1)
  printk("\0016[info]%s:%d:[ufssnapshotV2]rw buffer mode %u op %d buffer_id %u len %u is_read %d\n","ufssnapshotV2_rw_buffer",137,mode,op,buffer_id,len,is_read);
 ret=os44_mi_scsi_execute_cmd(sdev,cdb,is_read ? 34 : 35,buffer,len,7500,3,&args);
 if(buffer && len) for(i=0;i<len;i++)
  if(s->debug==1) printk("\0016[info]%s:%d:[ufssnapshotV2] buffer[%d] 0x%02x\n\n","ufssnapshotV2_rw_buffer",147,i,buffer[i]);
 if(ret) {
  printk("\0013[err]%s: %d:[ufssnapshotV2] rw buffer failed, ret %d, isread %d\n\n","ufssnapshotV2_rw_buffer",152,ret,is_read);
  printk("\0013[err]%s: %d:[ufssnapshotV2] code %x sense_key %x  asc %x ascq %x\n\n","ufssnapshotV2_rw_buffer",153,sshdr.response_code,sshdr.sense_key,sshdr.asc,sshdr.ascq);
  printk("\0013[err]%s: %d:[ufssnapshotV2] byte4 %x byte5 %x  byte6 %x additional_len %x\n\n","ufssnapshotV2_rw_buffer",155,sshdr.byte4,sshdr.byte5,sshdr.byte6,sshdr.additional_length);
  os44_query_error(1,4,-1,0);
 }
 return ret;
}
