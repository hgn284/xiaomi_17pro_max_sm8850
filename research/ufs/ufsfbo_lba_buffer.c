// SPDX-License-Identifier: GPL-2.0-only
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kstrtox.h>
#include <linux/unaligned.h>
#include "os44_fbo_lba.h"
/* Original-equivalent candidate, not hardened: no output-capacity argument;
 * counts and address/length truncation follow the original ARM64 function. */
int os44_ufsfbo_check_lr_list_buf(struct os44_fbo_lifetime *fbo,const char *buf)
{
 const char *p;
 int count=1;
 if(!buf){printk("\0013[err]%s:%d:Invalid fbo write buf input, please try again\n","ufsfbo_check_lr_list_buf",406);return -EINVAL;}
 p=strchr(buf,',');
 if(!p || buf[strlen(buf)-1]==','){printk("\0013[err]%s:%d:Invalid lba range, please input lba range separated by ','\n","ufsfbo_check_lr_list_buf",412);return -EINVAL;}
 do {p=strchr(p+1,',');count++;}while(p);
 if((count-2)&1){printk("\0013[err]%s:%d:Invalid lba range count, please input again\n","ufsfbo_check_lr_list_buf",425);return -EINVAL;}
 if(fbo->debug==1)printk("\0016[info]%s:%d:Valid lba range count\n","ufsfbo_check_lr_list_buf",423);
 fbo->lr_count=count>>1;
 return 0;
}
int os44_ufsfbo_init_lba_buffer(struct os44_fbo_lifetime *fbo,const char *buf,u8 *data,bool pin,bool ss)
{
 char *copy=kzalloc(strlen(buf)+1,GFP_KERNEL),*cursor,*token;
 unsigned long long value=0,start=0,total=0;
 int offset,index=1,ret=0;
 if(!copy){printk("\0013[err]%s:%d:Alloc buffer fail\n","ufsfbo_init_lba_buffer",469);return -ENOMEM;}
 strcpy(copy,buf);cursor=copy;
 if(pin){put_unaligned_be16((u16)fbo->lr_count,data);put_unaligned_be16(0,data+2);offset=4;}
 else {
  if(fbo->is_ufs31){data[1]=fbo->lr_count;offset=8;}
  else {data[5]=fbo->lr_count;data[6]=fbo->wholefile;offset=12;}
  if(ss)data[4]=1;
 }
 while((token=strsep(&cursor,","))!=NULL) {
  if(kstrtoull(token,16,&value)) {printk("\0013[err]%s:%d:invalid lba range value\n","ufsfbo_init_lba_buffer",499);ret=-ENODEV;goto out;}
  if(index&1) {put_unaligned_be32((u32)value,data+offset);start=value;}
  else {
   if(value<start){printk("\0013[err]%s:%d:invalid lba range length\n","ufsfbo_init_lba_buffer",509);ret=-ENODEV;goto out;}
   value=value-start+1;
   if(pin)put_unaligned_be32((u32)value,data+offset+4);
   else {data[offset+4]=value>>16;data[offset+5]=value>>8;data[offset+6]=value;}
   total+=value;offset+=8;
  }
  index++;
 }
 if(fbo->enforce_range_limit && total>0x10000){printk("\0013[err]%s:%d:Pre-load LBA range can not greater than 256MB\n","ufsfbo_init_lba_buffer",527);ret=-EINVAL;}
 if(pin) {
  if(fbo->debug==1)printk("\0016[info]%s:%d:check pin LBA range length:%lld\n","ufsfbo_init_lba_buffer",534,total);
  if(total>0x7e00){printk("\0013[err]%s:%d:check pin LBA range can not greater than 126MB\n","ufsfbo_init_lba_buffer",536);ret=-EINVAL;}
 }
 fbo->total_blocks=total;
out:
 kfree(copy);
 return ret;
}
