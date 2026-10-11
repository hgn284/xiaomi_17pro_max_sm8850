// SPDX-License-Identifier: GPL-2.0-only
#include "os44_fbo_preload.h"
ssize_t os44_ufsfbo_sysfs_store_fbo_app_preload(struct os44_fbo_lifetime *fbo,const char *input,size_t count)
{
 int ctrl=0;
 if(fbo->vendor_selector==4){if(fbo->debug==1)printk("\0016[info]%s:%d:SAMSUNG Not support FBO2 pre-load\n","ufsfbo_sysfs_store_fbo_app_preload",654);return count;}
 if(os44_ufsfbo_check_lr_list_buf(fbo,input)){printk("\0013[err]%s:%d:L-range list check fail\n","ufsfbo_sysfs_store_fbo_app_preload",659);return -EINVAL;}
 if(os44_ufsfbo_operation_control(fbo,&ctrl)){printk("\0013[err]%s:%d:ctrl fbo to 0x0 fail\n","ufsfbo_sysfs_store_fbo_app_preload",664);return -EINVAL;}
 if(os44_ufsfbo_app_preload(fbo,input)){printk("\0013[err]%s:%d:pre-load lba range failed\n","ufsfbo_sysfs_store_fbo_app_preload",670);return -EINVAL;}
 if(fbo->debug==1)printk("\0016[info]%s:%d:pre-load lba range success\n","ufsfbo_sysfs_store_fbo_app_preload",674);
 return count;
}
