/* SPDX-License-Identifier: GPL-2.0-only */
/* Preserves original NULL release and per-file uevent, no owner storage. */
#include "os44_snapshot_v2_sysfs.h"
ssize_t os44_ufssnapshot_attr_showV2(struct kobject *kobj, struct attribute *attr, char *buf)
{
 struct os44_snapshot_v2_entry *entry=container_of(attr,struct os44_snapshot_v2_entry,attr);
 struct os44_snapshot_v2_view *snapshot=container_of(kobj,struct os44_snapshot_v2_view,kobj);
 ssize_t ret;
 if(!entry->show) return -EIO;
 mutex_lock(&snapshot->lock);ret=entry->show(snapshot,buf);mutex_unlock(&snapshot->lock);return ret;
}
ssize_t os44_ufssnapshot_attr_storeV2(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
 struct os44_snapshot_v2_entry *entry=container_of(attr,struct os44_snapshot_v2_entry,attr);
 struct os44_snapshot_v2_view *snapshot=container_of(kobj,struct os44_snapshot_v2_view,kobj);
 ssize_t ret;
 if(!entry->store) return -EIO;
 mutex_lock(&snapshot->lock);ret=entry->store(snapshot,buf,count);mutex_unlock(&snapshot->lock);return ret;
}
const struct sysfs_ops os44_ufssnapshot_sysfs_opsV2={ .show=os44_ufssnapshot_attr_showV2,.store=os44_ufssnapshot_attr_storeV2 };
struct kobj_type os44_ufssnapshot_ktypeV2={ .sysfs_ops=&os44_ufssnapshot_sysfs_opsV2 };
struct os44_snapshot_v2_entry os44_snapshot_attrsV2[]={
 { .attr={.name="range",.mode=0644},.show=os44_ufssnapshotV2_sysfs_show_range,.store=os44_ufssnapshotV2_sysfs_store_range },
 { .attr={.name="status",.mode=0444},.show=os44_ufssnapshotV2_sysfs_show_status },
 { .attr={.name="dropslot",.mode=0644},.show=os44_ufssnapshotV2_sysfs_show_dropslot,.store=os44_ufssnapshotV2_sysfs_store_dropslot },
 { .attr={.name="progress",.mode=0444},.show=os44_ufssnapshotV2_sysfs_show_progress },
 {},
};
int os44_ufssnapshot_create_sysfsV2(struct os44_snapshot_view *v1,struct os44_snapshot_v2_view *snapshot)
{
 struct kobject *parent=&v1->kobj;
 struct os44_snapshot_v2_entry *entry;
 static struct lock_class_key __key;
 int ret;
 snapshot->entries=os44_snapshot_attrsV2;
 kobject_init(&snapshot->kobj,&os44_ufssnapshot_ktypeV2);
 __mutex_init(&snapshot->lock,"&snapshotV2->sysfs_lock",&__key);
 ret=kobject_add(&snapshot->kobj,parent,"v2");
 if(ret) {
  printk("\0013[err]%s: %d:[ufssnapshotV2] kobject add filed, err %d\n","ufssnapshot_create_sysfsV2",334,ret);return ret;
 }
 for(entry=snapshot->entries;entry->attr.name;entry++) {
  ret=sysfs_create_file(&snapshot->kobj,&entry->attr);
  if(ret) {
   printk("\0013[err]%s: %d:[ufssnapshotV2] create attr file failed, attr name %s\n","ufssnapshot_create_sysfsV2",341,entry->attr.name);
   kobject_uevent(&snapshot->kobj,KOBJ_REMOVE);kobject_del(&snapshot->kobj);return -EINVAL;
  }
  kobject_uevent(&snapshot->kobj,KOBJ_ADD);
 }
 return 0;
}
