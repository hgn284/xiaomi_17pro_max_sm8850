// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kstrtox.h>
#include "os44_fbo_operation.h"
ssize_t os44_ufsfbo_sysfs_store_fbo_operation_control(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 unsigned long long value=0;
 int ret;
 u8 debug;
 if(kstrtoull(buf,16,&value))return -EINVAL;
 if(fbo->is_ufs31)return -ENOTSUPP;
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,0);
 debug=fbo->debug;
 if(debug==1) {
  printk("\0016[info]%s:%d:Defrag_enable start power mode: %d\n","ufsfbo_sysfs_store_fbo_operation_control",380,fbo->hba->curr_dev_pwr_mode);
  debug=fbo->debug&1;
 }
 if(debug)printk("\0016[info]%s:%d:fbo control val = 0x%lx\n","ufsfbo_sysfs_store_fbo_operation_control",386,(unsigned long)value);
 ret=os44_ufsfbo_operation_control(fbo,(int *)&value);
 os44_ufsfbo_auto_hibern8_enable(fbo,1);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret){printk("\0013[err]%s:%d:Enable frag level check failed\n","ufsfbo_sysfs_store_fbo_operation_control",393);return -EINVAL;}
 return count;
}
