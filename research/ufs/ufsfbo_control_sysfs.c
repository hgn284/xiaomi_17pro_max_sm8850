// SPDX-License-Identifier: GPL-2.0-only
/* Original validation and call ordering retained; shared real power providers. */
#include <linux/kstrtox.h>
#include "os44_fbo_control_sysfs.h"
ssize_t os44_ufsfbo_sysfs_store_debug(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 unsigned long long value=0;
 if(kstrtoull(buf,0,&value) || value>1)return -EINVAL;
 fbo->debug=value;
 if(fbo->debug==1)printk(KERN_INFO "[info]%s:%d:Debug:%d\n","ufsfbo_sysfs_store_debug",299,fbo->debug);
 return count;
}
ssize_t os44_ufsfbo_sysfs_store_block_suspend(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 unsigned long long value=0;
 if(kstrtoull(buf,0,&value) || value>1)return -EINVAL;
 if(fbo->debug==1)printk(KERN_INFO "[info]%s:%d:fbo_block_suspend %lu\n","ufsfbo_sysfs_store_block_suspend",323,(unsigned long)value);
 if(value!=fbo->block_suspend) {
  if(value)os44_ufsfbo_block_enter_suspend(fbo);
  else os44_ufsfbo_allow_enter_suspend(fbo);
  fbo->block_suspend=!!value;
 }
 return count;
}
ssize_t os44_ufsfbo_sysfs_show_auto_hibern8_enable(struct os44_fbo_lifetime *fbo,char *buf)
{
 if(fbo->debug==1)printk(KERN_INFO "[info]%s:%d:HCI auto hibern8 %d\n","ufsfbo_sysfs_show_auto_hibern8_enable",341,fbo->is_auto_enabled);
 return sysfs_emit(buf,"%d\n",fbo->is_auto_enabled);
}
ssize_t os44_ufsfbo_sysfs_store_auto_hibern8_enable(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 unsigned long long value=0;
 if(kstrtoull(buf,0,&value) || value>1)return -EINVAL;
 os44_ufsfbo_auto_hibern8_enable(fbo,value);
 return count;
}
