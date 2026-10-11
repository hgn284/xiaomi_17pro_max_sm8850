/* SPDX-License-Identifier: GPL-2.0-only */
/* Preserves stock reference/error behavior, including NULL ktype.release. */
#include <ufs/ufshcd.h>
#include "os44_cld_sysfs.h"
ssize_t os44_ufscld_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
 struct os44_cld_sysfs_entry *entry = container_of(attr, struct os44_cld_sysfs_entry, attr);
 struct os44_cld_view *cld = container_of(kobj, struct os44_cld_view, kobj);
 ssize_t ret;
 if (!entry->show) return -EIO;
 ret = os44_ufscld_hold_runtime_pm(cld);
 if (ret) {
  printk(KERN_ERR "%s:%d err: ufscld_hold_runtime_pm fail\n", "ufscld_attr_show", 277);
  return ret;
 }
 mutex_lock(&cld->sysfs_lock);
 ret = entry->show(cld, buf);
 mutex_unlock(&cld->sysfs_lock);
 os44_ufscld_release_runtime_pm(cld);
 return ret;
}
ssize_t os44_ufscld_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
 struct os44_cld_sysfs_entry *entry = container_of(attr, struct os44_cld_sysfs_entry, attr);
 struct os44_cld_view *cld = container_of(kobj, struct os44_cld_view, kobj);
 ssize_t ret;
 if (!entry->store) return -EIO;
 ret = os44_ufscld_hold_runtime_pm(cld);
 if (ret) {
  printk(KERN_ERR "%s:%d err: ufscld_hold_runtime_pm fail\n", "ufscld_attr_store", 304);
  return ret;
 }
 mutex_lock(&cld->sysfs_lock);
 ret = entry->store(cld, buf, count);
 mutex_unlock(&cld->sysfs_lock);
 os44_ufscld_release_runtime_pm(cld);
 return ret;
}
const struct sysfs_ops os44_ufscld_sysfs_ops = {
 .show = os44_ufscld_attr_show, .store = os44_ufscld_attr_store,
};
struct kobj_type os44_ufscld_ktype = { .sysfs_ops = &os44_ufscld_sysfs_ops };
struct os44_cld_sysfs_entry os44_ufscld_sysfs_entries[] = {
 { .attr = { .name = "frag_level", .mode = 0444 }, .show = os44_ufscld_sysfs_show_frag_level },
 { .attr = { .name = "cld_operation_status", .mode = 0444 }, .show = os44_ufscld_sysfs_show_cld_operation_status },
 { .attr = { .name = "hid_available_size", .mode = 0444 }, .show = os44_ufscld_sysfs_show_hid_available_size },
 { .attr = { .name = "hid_state", .mode = 0444 }, .show = os44_ufscld_sysfs_show_hid_state },
 { .attr = { .name = "trigger", .mode = 0644 }, .show = os44_ufscld_sysfs_show_trigger, .store = os44_ufscld_sysfs_store_trigger },
 { .attr = { .name = "trigger_interval", .mode = 0644 }, .show = os44_ufscld_sysfs_show_trigger_interval, .store = os44_ufscld_sysfs_store_trigger_interval },
 { .attr = { .name = "work_state", .mode = 0444 }, .show = os44_ufscld_sysfs_show_work_state },
 { .attr = { .name = "debug", .mode = 0644 }, .show = os44_ufscld_sysfs_show_debug, .store = os44_ufscld_sysfs_store_debug },
 { .attr = { .name = "block_suspend", .mode = 0644 }, .show = os44_ufscld_sysfs_show_block_suspend, .store = os44_ufscld_sysfs_store_block_suspend },
 { .attr = { .name = "auto_hibern8_enable", .mode = 0644 }, .show = os44_ufscld_sysfs_show_auto_hibern8_enable, .store = os44_ufscld_sysfs_store_auto_hibern8_enable },
 {},
};
int os44_ufscld_create_sysfs(struct os44_cld_view *cld)
{
 struct kobject *parent = &cld->hba->dev->kobj;
 struct os44_cld_sysfs_entry *entry;
 int ret;
 cld->entries = os44_ufscld_sysfs_entries;
 kobject_init(&cld->kobj, &os44_ufscld_ktype);
 mutex_init(&cld->sysfs_lock);
 printk(KERN_INFO "%s:%d info: ufscld creates sysfs ufscld %p dev->kobj %p\n", "ufscld_create_sysfs", 347, &cld->kobj, parent);
 ret = kobject_add(&cld->kobj, kobject_get(parent), "ufscld");
 if (ret) {
  printk(KERN_ERR "%s:%d err: kobject_add failed\n", "ufscld_create_sysfs", 366);
  return ret;
 }
 for (entry = cld->entries; entry->attr.name; entry++) {
  if ((!strcmp(entry->attr.name, "hid_available_size") || !strcmp(entry->attr.name, "hid_state")) && cld->hba->dev_info.wspecversion < 0x410) continue;
  printk(KERN_INFO "%s:%d info: ufscld sysfs attr creates: %s\n", "ufscld_create_sysfs", 356, entry->attr.name);
  ret = sysfs_create_file(&cld->kobj, &entry->attr);
  if (ret) {
   printk(KERN_ERR "%s:%d err: create entry(%s) failed\n", "ufscld_create_sysfs", 360, entry->attr.name);
   ret = kobject_uevent(&cld->kobj, KOBJ_REMOVE);
   printk(KERN_INFO "%s:%d info: kobject removed (%d)\n", "ufscld_create_sysfs", 372, ret);
   kobject_del(&cld->kobj);
   return -EINVAL;
  }
 }
 kobject_uevent(&cld->kobj, KOBJ_ADD);
 return 0;
}
