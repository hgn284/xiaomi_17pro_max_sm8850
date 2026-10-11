/* SPDX-License-Identifier: GPL-2.0-only */
/* Preserves original NULL release and per-file uevent, no owner storage. */
#include "os44_snapshot_sysfs.h"
ssize_t os44_ufssnapshot_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
 struct os44_snapshot_entry *entry=container_of(attr,struct os44_snapshot_entry,attr);
 struct os44_snapshot_view *snapshot=container_of(kobj,struct os44_snapshot_view,kobj);
 ssize_t ret;
 if(!entry->show) return -EIO;
 mutex_lock(&snapshot->lock);ret=entry->show(snapshot,buf);mutex_unlock(&snapshot->lock);return ret;
}
ssize_t os44_ufssnapshot_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
 struct os44_snapshot_entry *entry=container_of(attr,struct os44_snapshot_entry,attr);
 struct os44_snapshot_view *snapshot=container_of(kobj,struct os44_snapshot_view,kobj);
 ssize_t ret;
 if(!entry->store) return -EIO;
 mutex_lock(&snapshot->lock);ret=entry->store(snapshot,buf,count);mutex_unlock(&snapshot->lock);return ret;
}
const struct sysfs_ops os44_ufssnapshot_sysfs_ops={ .show=os44_ufssnapshot_attr_show,.store=os44_ufssnapshot_attr_store };
struct kobj_type os44_ufssnapshot_ktype={ .sysfs_ops=&os44_ufssnapshot_sysfs_ops };
struct os44_snapshot_entry os44_snapshot_attrs[]={
 { .attr={.name="ctrl",.mode=0644},.show=os44_ufssnapshot_sysfs_show_ctrl,.store=os44_ufssnapshot_sysfs_store_ctrl },
 { .attr={.name="status",.mode=0444},.show=os44_ufssnapshot_sysfs_show_status },
 { .attr={.name="dropfreesize",.mode=0644},.show=os44_ufssnapshot_sysfs_show_dropfreesize,.store=os44_ufssnapshot_sysfs_store_dropfreesize },
 { .attr={.name="debug",.mode=0644},.show=os44_ufssnapshot_sysfs_show_debug,.store=os44_ufssnapshot_sysfs_store_debug },
 { .attr={.name="freespace",.mode=0444},.show=os44_ufssnapshot_sysfs_show_freespace },
 { .attr={.name="support",.mode=0444},.show=os44_ufssnapshot_sysfs_show_support },
 { .attr={.name="version",.mode=0444},.show=os44_ufssnapshot_sysfs_show_version },
 { .attr={.name="eecount",.mode=0444},.show=os44_ufssnapshot_sysfs_show_eecount },
 { .attr={.name="backupcancelled",.mode=0444},.show=os44_ufssnapshot_sysfs_show_backupcancelled },
 {},
};
int os44_ufssnapshot_create_sysfs(struct os44_snapshot_view *snapshot)
{
 struct kobject *parent=&snapshot->hba->dev->kobj;
 struct os44_snapshot_entry *entry;
 static struct lock_class_key __key;
 int ret;
 snapshot->entries=os44_snapshot_attrs;
 kobject_init(&snapshot->kobj,&os44_ufssnapshot_ktype);
 __mutex_init(&snapshot->lock,"&snapshot->sysfs_lock",&__key);
 ret=kobject_add(&snapshot->kobj,parent,"ufssnapshot");
 if(ret) {
  printk("\0013[err]%s: %d:[ufssnapshot] kobject add filed, err %d\n","ufssnapshot_create_sysfs",482,ret);return ret;
 }
 for(entry=snapshot->entries;entry->attr.name;entry++) {
  ret=sysfs_create_file(&snapshot->kobj,&entry->attr);
  if(ret) {
   printk("\0013[err]%s: %d:[ufssnapshot] create attr file failed, attr name %s\n","ufssnapshot_create_sysfs",489,entry->attr.name);
   kobject_uevent(&snapshot->kobj,KOBJ_REMOVE);kobject_del(&snapshot->kobj);return -EINVAL;
  }
  kobject_uevent(&snapshot->kobj,KOBJ_ADD);
 }
 return 0;
}
