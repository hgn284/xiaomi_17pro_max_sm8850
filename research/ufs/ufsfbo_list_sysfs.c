// SPDX-License-Identifier: GPL-2.0-only
#include "os44_fbo_write.h"
ssize_t os44_ufsfbo_sysfs_store_fbo_send_lr_list(struct os44_fbo_lifetime *fbo,const char *input,size_t count)
{
 u32 prog=0;
 int level=0,ops=0;
 int ret;
 if(os44_ufsfbo_check_lr_list_buf(fbo,input)){printk("\0013[err]%s:%d:L-range list check fail\n","ufsfbo_sysfs_store_fbo_send_lr_list",471);return -EINVAL;}
 if(fbo->is_ufs31){
  if(os44_ufsfbs_get_level_check_ops(fbo,&level)){printk("\0013[err]%s:%d:Get level_ops fail\n","ufsfbo_sysfs_store_fbo_send_lr_list",478);return -EINVAL;}
  if(os44_ufsfbs_get_ops(fbo,&ops)){printk("\0013[err]%s:%d:Get defrag_ops fail\n","ufsfbo_sysfs_store_fbo_send_lr_list",484);return -EINVAL;}
  if(level==3){
   if(fbo->debug==1)printk("\0016[info]%s:%d:level_ops:%d, abnormal, clear flag again\n","ufsfbo_sysfs_store_fbo_send_lr_list",489,level);
   os44_ufsfbs_frag_level_check_enable(fbo,false);
  }
  if(!(ops|level)){
   os44_ufsfbo_block_enter_suspend(fbo);
   os44_ufsfbo_auto_hibern8_enable(fbo,false);
   ret=os44_ufsfbo_lba_list_write(fbo,input);
   os44_ufsfbo_auto_hibern8_enable(fbo,true);
   os44_ufsfbo_allow_enter_suspend(fbo);
   if(ret){printk("\0013[err]%s:%d:Send lba range failed\n","ufsfbo_sysfs_store_fbo_send_lr_list",502);return -EINVAL;}
  }else printk("\0013[err]%s:%d:Invalid defrag or level check ops\n","ufsfbo_sysfs_store_fbo_send_lr_list",506);
 }else{
  if(os44_ufsfbo_get_fbo_prog_state(fbo,&prog)){printk("\0013[err]%s:%d:Invalid fbo_prog_state\n","ufsfbo_sysfs_store_fbo_send_lr_list",511);return -EINVAL;}
  if(!prog){
   os44_ufsfbo_block_enter_suspend(fbo);
   os44_ufsfbo_auto_hibern8_enable(fbo,false);
   ret=os44_ufsfbo_lba_list_write(fbo,input);
   os44_ufsfbo_auto_hibern8_enable(fbo,true);
   os44_ufsfbo_allow_enter_suspend(fbo);
   if(ret){printk("\0013[err]%s:%d:Send lba range failed\n","ufsfbo_sysfs_store_fbo_send_lr_list",524);return -EINVAL;}
  }else printk("\0013[err]%s:%d:Invalid defrag or level check ops\n","ufsfbo_sysfs_store_fbo_send_lr_list",528);
 }
 return count;
}
