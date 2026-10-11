// SPDX-License-Identifier: GPL-2.0-only
/* Preserves original unbounded sysfs output and bounded input-array trap. */
#include <linux/slab.h>
#include "os44_fbo_pin_info.h"
ssize_t os44_ufsfbo_sysfs_show_fbo3_get_lr_pinned_info(struct os44_fbo_lifetime *fbo,char *buf)
{
 u8 *buffer=kzalloc(4096,GFP_KERNEL);
 unsigned long i,pos=0,count;
 int ret;
 if(!buffer)return -ENOMEM;
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,false);
 ret=os44_ufsfbo_get_lr_pinned_info(fbo,buffer);
 os44_ufsfbo_auto_hibern8_enable(fbo,true);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret){printk("\0013[err]%s:%d:Get lba pinned info failed\n","ufsfbo_sysfs_show_fbo3_get_lr_pinned_info",761);ret=0;goto out;}
 if(fbo->debug==1)printk("\0016[info]%s:%d:lba pinned info, len %lld\n\n","ufsfbo_sysfs_show_fbo3_get_lr_pinned_info",765,fbo->total_blocks);
 count=fbo->total_blocks>>3;
 for(i=0;i<count;i++){
  u8 b;
  if(i>=4096)__builtin_trap();
  b=buffer[i];
  unsigned long n=0;
  n+=sprintf(buf+pos+n,"%d",b>>7);
  n+=sprintf(buf+pos+n,"%d",(b>>6)&1);
  n+=sprintf(buf+pos+n,"%d",(b>>5)&1);
  n+=sprintf(buf+pos+n,"%d",(b>>4)&1);
  n+=sprintf(buf+pos+n,"%d",(b>>3)&1);
  n+=sprintf(buf+pos+n,"%d",(b>>2)&1);
  n+=sprintf(buf+pos+n,"%d",(b>>1)&1);
  n+=sprintf(buf+pos+n,"%d",b&1);
  buf[pos+n]=0;pos+=n;
  buf[pos++]=((i+1)%4)?' ':'\n';
 }
 buf[pos]=0;ret=pos;
out:
 kfree(buffer);return ret;
}
