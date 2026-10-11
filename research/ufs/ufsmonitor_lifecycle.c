/* SPDX-License-Identifier: GPL-2.0-only */
/* Full original constructor semantics, not a leak/lifetime repair.
 * Existing shared owner remains external until its whole lifecycle is restored. */
#include <ufs/ufshcd.h>
#include "os44_xiaomi_owner.h"
static_assert(offsetof(struct ufs_hba,dev)==0x40);
static_assert(offsetof(struct device,kobj)==0);
int os44_ufsmonitor_create_sysfs(struct ufs_hba *hba)
{
 struct os44_ufsmonitor_sysfs *monitor=&os44_ufs_xiaomi.monitor;
 struct kobject *parent=&hba->dev->kobj;
 struct os44_ufsmonitor_entry *entry;
 int ret;
 monitor->monitor.hba=hba;
 monitor->entries=os44_ufsmonitor_sysfs_entries;
 kobject_init(&monitor->kobj,&os44_ufsmonitor_ktype);
 mutex_init(&monitor->sysfs_lock);
 printk(KERN_INFO "%s:%d info: ufs_monitor creates sysfs ufs_monitor %p dev->kobj %p\n", "ufsmonitor_create_sysfs",6541,&monitor->kobj,parent);
 ret=kobject_add(&monitor->kobj,kobject_get(parent),"ufs_monitor");
 if (ret) {
  printk(KERN_ERR "%s:%d err: kobject_add failed\n", "ufsmonitor_create_sysfs",6558);
  return ret;
 }
 for (entry=monitor->entries;entry->attr.name;entry++) {
  printk(KERN_INFO "%s:%d info: ufs_monitor sysfs attr creates: %s\n", "ufsmonitor_create_sysfs",6548,entry->attr.name);
  ret=sysfs_create_file(&monitor->kobj,&entry->attr);
  if (ret) {
   printk(KERN_ERR "%s:%d err: create entry(%s) failed\n", "ufsmonitor_create_sysfs",6552,entry->attr.name);
   goto remove;
  }
 }
 kobject_uevent(&monitor->kobj,KOBJ_ADD);
 return 0;
remove:
 ret=kobject_uevent(&monitor->kobj,KOBJ_REMOVE);
 printk(KERN_INFO "%s:%d info: kobject removed (%d)\n", "ufsmonitor_create_sysfs",6564,ret);
 kobject_del(&monitor->kobj);
 return -EINVAL;
}
