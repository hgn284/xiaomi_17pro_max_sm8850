// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kstrtox.h>
#include "os44_fbo_fields.h"
ssize_t os44_ufsfbo_sysfs_show_fbo_wholefile_enable(struct os44_fbo_lifetime *fbo,char *buf)
{ return sysfs_emit(buf,"whole file flag: %d\n",fbo->wholefile); }
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_map_state(struct os44_fbo_lifetime *fbo,char *buf)
{ return sysfs_emit(buf,"%d\n",fbo->linear_state); }
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_mapping_start(struct os44_fbo_lifetime *fbo,char *buf)
{ return sysfs_emit(buf,"0x%x\n",fbo->linear_start); }
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_mapping_end(struct os44_fbo_lifetime *fbo,char *buf)
{ return sysfs_emit(buf,"0x%x\n",fbo->linear_end); }
ssize_t os44_ufsfbo_sysfs_store_fbo_wholefile_enable(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 bool value=false;
 if(kstrtobool(buf,&value)) {
  printk("\0013[err]%s:%d:Convert bool type fail from char * type\n","ufsfbo_sysfs_store_fbo_wholefile_enable",149);
  return -EINVAL;
 }
 fbo->wholefile=value;
 return count;
}
