/* SPDX-License-Identifier: GPL-2.0-only */
/* Preserves original NULL release and per-file uevent, no owner storage. */
#include "os44_eua.h"
ssize_t os44_ufseua_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
 struct os44_eua_entry *entry=container_of(attr,struct os44_eua_entry,attr);
 struct os44_eua_view *snapshot=container_of(kobj,struct os44_eua_view,kobj);
 ssize_t ret;
 if(!entry->show) return -EIO;
 mutex_lock(&snapshot->lock);ret=entry->show(snapshot,buf);mutex_unlock(&snapshot->lock);return ret;
}
ssize_t os44_ufseua_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
 struct os44_eua_entry *entry=container_of(attr,struct os44_eua_entry,attr);
 struct os44_eua_view *snapshot=container_of(kobj,struct os44_eua_view,kobj);
 ssize_t ret;
 if(!entry->store) return -EIO;
 mutex_lock(&snapshot->lock);ret=entry->store(snapshot,buf,count);mutex_unlock(&snapshot->lock);return ret;
}
