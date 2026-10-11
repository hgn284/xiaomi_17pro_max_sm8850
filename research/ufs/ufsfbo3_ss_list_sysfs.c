// SPDX-License-Identifier: GPL-2.0-only
#include "os44_fbo3_pin_write.h"
ssize_t os44_ufsfbo_sysfs_store_fbo3_ss_send_pin_lr_list(struct os44_fbo_lifetime *fbo,const char *input,size_t count)
{
 u32 prog=0;
 int ret;
 if(os44_ufsfbo_check_lr_list_buf(fbo,input)){printk("\0013[err]%s:%d:L-range list check fail\n","ufsfbo_sysfs_store_fbo3_ss_send_pin_lr_list",712);return -EINVAL;}
 if(os44_ufsfbo_get_fbo_prog_state(fbo,&prog)){printk("\0013[err]%s:%d:Invalid fbo_prog_state\n","ufsfbo_sysfs_store_fbo3_ss_send_pin_lr_list",719);return -EINVAL;}
 if(!prog){
  os44_ufsfbo_block_enter_suspend(fbo);
  os44_ufsfbo_auto_hibern8_enable(fbo,false);
  ret=os44_ufsfbo3_ss_lba_list_write(fbo,input);
  os44_ufsfbo_auto_hibern8_enable(fbo,true);
  os44_ufsfbo_allow_enter_suspend(fbo);
  if(ret){printk("\0013[err]%s:%d:Send lba range failed\n","ufsfbo_sysfs_store_fbo3_ss_send_pin_lr_list",732);return -EINVAL;}
 }else printk("\0013[err]%s:%d:Invalid fbo prog state\n","ufsfbo_sysfs_store_fbo3_ss_send_pin_lr_list",736);
 return count;
}
