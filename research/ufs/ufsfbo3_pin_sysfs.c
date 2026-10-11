// SPDX-License-Identifier: GPL-2.0-only
#include "os44_fbo_pin_check.h"
ssize_t os44_ufsfbo_sysfs_store_fbo3_send_lr_list(struct os44_fbo_lifetime *fbo,const char *input,size_t count)
{
 int ret;
 if(os44_ufsfbo_check_lr_list_buf(fbo,input)){printk("\0013[err]%s:%d:L-range list check fail\n","ufsfbo_sysfs_store_fbo3_send_lr_list",781);return -EINVAL;}
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,false);
 ret=os44_ufsfbo_check_lba_pin(fbo,input);
 os44_ufsfbo_auto_hibern8_enable(fbo,true);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret){printk("\0013[err]%s:%d:Send lba range failed\n","ufsfbo_sysfs_store_fbo3_send_lr_list",792);return -EINVAL;}
 return count;
}
