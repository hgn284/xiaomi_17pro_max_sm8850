// SPDX-License-Identifier: GPL-2.0-only
#include <linux/slab.h>
#include <linux/sysfs.h>
#include "os44_snapshot_v2.h"
ssize_t os44_ufssnapshotV2_sysfs_show_status(struct os44_snapshot_v2_view *s,char *buf)
{
 u8 data[56]={0}; char *tmp,*p; int ret,len=0,i; ssize_t result;
 ret=os44_ufssnapshotV2_rw_buffer(s,s->vendor==3?0x1d:0x1e,0,255,56,data,true);
 if(ret) {
 printk("\0013[err]%s: %d:ufssnapshotV2_sysfs_show_status failed, ret %d\n","ufssnapshotV2_sysfs_show_status",183,ret);return ret;
 }
 tmp=kzalloc(1024,GFP_KERNEL);if(!tmp)return -ENOMEM;p=tmp;
 for(i=0;i<6;i++) {
  u8 *q=&data[8+i*8];
  u64 address=((u64)q[1]<<40)|((u64)q[2]<<32)|((u64)q[3]<<24)|((u64)q[4]<<16)|((u64)q[5]<<8)|q[6];
  unsigned int state=q[7] & (s->vendor==3?3:1);
  if(s->debug==1)printk("\0016[info]%s:%d:ufssnapshotV2_sysfs_show_status %d,%llu,%d\n","ufssnapshotV2_sysfs_show_status",209,q[0],address,state);
  len=snprintf(p,1024-len,"%d,%llu,%d;",q[0],address,state);
  if(len<1)return -EINVAL; /* Exact stock error ownership, not a repair. */
  p+=len;
 }
 result=sysfs_emit(buf,"%s\n",tmp);kfree(tmp);return result;
}
