// SPDX-License-Identifier: GPL-2.0-only
/* Whole operational sysfs callbacks. Cleanup and original odd error policies retained. */
#include <linux/kstrtox.h>
#include "os44_fbo_status.h"
ssize_t os44_ufsfbo_sysfs_show_fbs_flc_ops(struct os44_fbo_lifetime *fbo,char *buf)
{
 int ops=0,ret;
 if(!fbo->is_ufs31)return -ENOTSUPP;
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,0);
 ret=os44_ufsfbs_get_level_check_ops(fbo,&ops);
 os44_ufsfbo_auto_hibern8_enable(fbo,1);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret) {
  printk(KERN_ERR "[err]%s:%d:Get level check opstatus failed\n","ufsfbo_sysfs_show_fbs_flc_ops",551);
  return -EINVAL;
 }
 return sysfs_emit(buf,"%d\n",ops);
}
ssize_t os44_ufsfbo_sysfs_show_fbs_defrag_ops(struct os44_fbo_lifetime *fbo,char *buf)
{
 int ops=0,ret;
 if(!fbo->is_ufs31)return -ENOTSUPP;
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,0);
 ret=os44_ufsfbs_get_ops(fbo,&ops);
 os44_ufsfbo_auto_hibern8_enable(fbo,1);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret) {
  printk(KERN_ERR "[err]%s:%d:Get defrag opstatus failed\n","ufsfbo_sysfs_show_fbs_defrag_ops",573);
  return -EINVAL;
 }
 return sysfs_emit(buf,"%d\n",ops);
}
ssize_t os44_ufsfbo_sysfs_store_fbs_flc_enable(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 bool enable=false;
 int ret;
 if(kstrtobool(buf,&enable))return -EINVAL;
 if(!fbo->is_ufs31)return -ENOTSUPP;
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,0);
 ret=os44_ufsfbs_frag_level_check_enable(fbo,enable);
 os44_ufsfbo_auto_hibern8_enable(fbo,1);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret) {
  printk(KERN_ERR "[err]%s:%d:Enable frag level check failed\n","ufsfbo_sysfs_store_fbs_flc_enable",601);
  return -EINVAL;
 }
 return count;
}
ssize_t os44_ufsfbo_sysfs_store_fbs_defrag_enable(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 bool enable=false;
 int ops=0,ret;
 if(kstrtobool(buf,&enable))return -EINVAL;
 if(!fbo->is_ufs31)return -ENOTSUPP;
 if(os44_ufsfbs_get_ops(fbo,&ops)) {
  printk(KERN_ERR "[err]%s:%d:Get defrag ops failed\n","ufsfbo_sysfs_store_fbs_defrag_enable",623);
  return -EINVAL;
 }
 if(fbo->vendor_selector && ops!=2 && !enable) {
  printk(KERN_ERR "[err]%s:%d:Invalid degrag_ops(%d), or enable value(%d) \n\n","ufsfbo_sysfs_store_fbs_defrag_enable",640,ops,0);
  return count;
 }
 os44_ufsfbo_block_enter_suspend(fbo);
 os44_ufsfbo_auto_hibern8_enable(fbo,0);
 ret=os44_ufsfbs_defrag_enable(fbo,enable);
 os44_ufsfbo_auto_hibern8_enable(fbo,1);
 os44_ufsfbo_allow_enter_suspend(fbo);
 if(ret) {
  printk(KERN_ERR "[err]%s:%d:Enable level check failed\n","ufsfbo_sysfs_store_fbs_defrag_enable",636);
  return -EINVAL;
 }
 return count;
}
