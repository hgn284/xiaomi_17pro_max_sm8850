// SPDX-License-Identifier: GPL-2.0-only
#include "os44_fbo_constructor.h"
ssize_t os44_ufsfbo_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
 struct os44_fbo_entry *entry=container_of(attr,struct os44_fbo_entry,attr);
 struct os44_fbo_lifetime *fbo=container_of(kobj,struct os44_fbo_lifetime,kobj);
 ssize_t ret;
 if(!entry->show)return -EIO;
 if(os44_ufsfbo_is_not_present(fbo))return -ENODEV;
 mutex_lock(&fbo->lock);ret=entry->show(fbo,buf);mutex_unlock(&fbo->lock);return ret;
}
ssize_t os44_ufsfbo_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
 struct os44_fbo_entry *entry=container_of(attr,struct os44_fbo_entry,attr);
 struct os44_fbo_lifetime *fbo=container_of(kobj,struct os44_fbo_lifetime,kobj);
 ssize_t ret;
 if(!entry->store)return -EIO;
 if(os44_ufsfbo_is_not_present(fbo))return -ENODEV;
 mutex_lock(&fbo->lock);ret=entry->store(fbo,buf,count);mutex_unlock(&fbo->lock);return ret;
}
