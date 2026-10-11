// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kstrtox.h>
#include "os44_fbo_progress.h"
ssize_t os44_ufsfbo_sysfs_show_fbo_prog_state(struct os44_fbo_lifetime *fbo,char *buf)
{
 u32 state=0;
 int ret;
 if(fbo->is_ufs31)return -ENOTSUPP;
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,0);
 ret=os44_ufsfbo_get_fbo_prog_state(fbo,&state);
 os44_ufsfbo_auto_hibern8_enable(fbo,1);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret) {
  printk("\0013[err]%s:%d:Get fbo_prog_state failed\n","ufsfbo_sysfs_show_fbo_prog_state",86);
  return -EINVAL;
 }
 return sysfs_emit(buf,"%x\n",state);
}
ssize_t os44_ufsfbo_sysfs_show_fbo_exe_threshold(struct os44_fbo_lifetime *fbo,char *buf)
{
 u32 level=0;
 if(os44_ufsfbo_get_exe_level(fbo,&level)) {
  printk("\0013[err]%s:%d:Get execute threshold failed\n","ufsfbo_sysfs_show_fbo_exe_threshold",215);
  return -EINVAL;
 }
 return sysfs_emit(buf,"%d\n",level);
}
ssize_t os44_ufsfbo_sysfs_store_fbo_exe_threshold(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 unsigned long long value=0;
 u32 state=0;
 int ops=0;
 if(kstrtoull(buf,0,&value))return -EINVAL;
 if(value>10) {printk("\0013[err]%s:%d:fbo_exe_threshold set error, illegal value\n","ufsfbo_sysfs_store_fbo_exe_threshold",234);return -EINVAL;}
 if(fbo->is_ufs31) {
  if(os44_ufsfbs_get_ops(fbo,&ops)) {printk("\0013[err]%s:%d:Get defrag opstatus failed\n","ufsfbo_sysfs_store_fbo_exe_threshold",241);return -EINVAL;}
  if((u32)ops>1) {printk("\0013[err]%s:%d:fbs_exe_level set error, illegal defrag ops value\n","ufsfbo_sysfs_store_fbo_exe_threshold",252);return -EINVAL;}
  /* Stock passes address of low u32 in the parsed u64, not a scalar value. */
  if(os44_ufsfbo_set_exe_level(fbo,(u32 *)&value)) {printk("\0013[err]%s:%d:Get execute level failed\n","ufsfbo_sysfs_store_fbo_exe_threshold",247);return -EINVAL;}
  if(fbo->debug==1)printk("\0016[info]%s:%d:fbs_set_exe_level:%lu\n","ufsfbo_sysfs_store_fbo_exe_threshold",250,(unsigned long)value);
 } else {
  if(os44_ufsfbo_get_fbo_prog_state(fbo,&state)) {printk("\0013[err]%s:%d:Get fbo prog state failed\n","ufsfbo_sysfs_store_fbo_exe_threshold",258);return -EINVAL;}
  if(state>3 || state==1) {printk("\0013[err]%s:%d:fbo_exe_threshold set error, illegal fbo prog state\n","ufsfbo_sysfs_store_fbo_exe_threshold",271);return -EINVAL;}
  if(os44_ufsfbo_set_exe_level(fbo,(u32 *)&value)) {printk("\0013[err]%s:%d:Get execute level failed\n","ufsfbo_sysfs_store_fbo_exe_threshold",266);return -EINVAL;}
  if(fbo->debug==1)printk("\0016[info]%s:%d:fbo_set_exe_threshold %lu\n","ufsfbo_sysfs_store_fbo_exe_threshold",269,(unsigned long)value);
 }
 return count;
}
