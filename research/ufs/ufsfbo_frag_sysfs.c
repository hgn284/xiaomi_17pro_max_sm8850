// SPDX-License-Identifier: GPL-2.0-only
#include <linux/slab.h>
#include "os44_fbo_frag.h"
/* Preserve stock snprintf accumulation and bounds behavior. This is not a
 * safety repair: large counts can exceed the sysfs output extent. */
ssize_t os44_ufsfbo_sysfs_show_fbo_get_lr_frag_level(struct os44_fbo_lifetime *fbo,char *buf)
{
 u8 (*response)[4096]=kzalloc(sizeof(*response),GFP_KERNEL);
 int ret, count=0, i, len;
 if(!response)return -ENOMEM;
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,0);
 ret=os44_ufsfbo_read_frag_level(fbo,*response);
 os44_ufsfbo_auto_hibern8_enable(fbo,1);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret){printk("\0013[err]%s:%d:Get lba range level failed\n","ufsfbo_sysfs_show_fbo_get_lr_frag_level",112);goto out;}
 len=(fbo->lr_count<<3)+8;
 if(fbo->is_ufs31) {
  for(i=0;i<len;i++) {
   count+=snprintf(buf+count,PAGE_SIZE-count,"%02x ",(*response)[i]);
   if((i+1)%8==0)count+=snprintf(buf+count,PAGE_SIZE-count,"\n");
  }
 } else {
  for(i=0;i<len;i++) {
   count+=snprintf(buf+count,PAGE_SIZE-count,"%02x ",(*response)[i+4]);
   if((i+1)%8==0)count+=snprintf(buf+count,PAGE_SIZE-count,"\n");
  }
 }
out:
 kfree(response);
 return count;
}
