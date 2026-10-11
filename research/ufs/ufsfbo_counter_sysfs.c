// SPDX-License-Identifier: GPL-2.0-only
/* Whole stock callback reconstruction; no fake owner or lifecycle repair. */
#include <linux/kstrtox.h>
#include "os44_fbo_control_sysfs.h"
ssize_t os44_ufsfbo_sysfs_show_fbo_err_cnt(struct os44_fbo_lifetime *fbo,char *buf)
{
 if(fbo->debug==1)printk(KERN_INFO "[info]%s:%d:fbo_err_cnt:%lu\n","ufsfbo_sysfs_show_fbo_err_cnt",160,(unsigned long)fbo->fbo_err_cnt);
 return snprintf(buf,4096,"%lu\n",(unsigned long)fbo->fbo_err_cnt);
}
ssize_t os44_ufsfbo_sysfs_store_fbo_err_cnt(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 unsigned long long value=0;
 if(kstrtoull(buf,0,&value))return -EINVAL;
 fbo->fbo_err_cnt+=value;
 if(fbo->debug==1)printk(KERN_INFO "[info]%s:%d:fbo_err_cnt:%lu\n","ufsfbo_sysfs_store_fbo_err_cnt",178,(unsigned long)fbo->fbo_err_cnt);
 return count;
}
ssize_t os44_ufsfbo_sysfs_show_fbo_retry_cnt(struct os44_fbo_lifetime *fbo,char *buf)
{
 if(fbo->debug==1)printk(KERN_INFO "[info]%s:%d:fbo_retry_cnt:%lu\n","ufsfbo_sysfs_show_fbo_retry_cnt",185,(unsigned long)fbo->fbo_retry_cnt);
 return snprintf(buf,4096,"%lu\n",(unsigned long)fbo->fbo_retry_cnt);
}
ssize_t os44_ufsfbo_sysfs_store_fbo_retry_cnt(struct os44_fbo_lifetime *fbo,const char *buf,size_t count)
{
 unsigned long long value=0;
 if(kstrtoull(buf,0,&value))return -EINVAL;
 fbo->fbo_retry_cnt+=value;
 if(fbo->debug==1)printk(KERN_INFO "[info]%s:%d:fbo_retry_cnt:%lu\n","ufsfbo_sysfs_store_fbo_retry_cnt",203,(unsigned long)fbo->fbo_retry_cnt);
 return count;
}
