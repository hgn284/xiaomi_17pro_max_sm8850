/* SPDX-License-Identifier: GPL-2.0-only */
#include "os44_ufsmonitor_sysfs.h"
ssize_t os44_ufsmonitor_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
 struct os44_ufsmonitor_sysfs *s=container_of(kobj,struct os44_ufsmonitor_sysfs,kobj);
 struct os44_ufsmonitor_entry *e=container_of(attr,struct os44_ufsmonitor_entry,attr);
 ssize_t ret;
 if (!e->show) return -EIO;
 mutex_lock(&s->sysfs_lock);
 ret=e->show(&s->monitor,buf);
 mutex_unlock(&s->sysfs_lock);
 return ret;
}
ssize_t os44_ufsmonitor_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
 struct os44_ufsmonitor_sysfs *s=container_of(kobj,struct os44_ufsmonitor_sysfs,kobj);
 struct os44_ufsmonitor_entry *e=container_of(attr,struct os44_ufsmonitor_entry,attr);
 ssize_t ret;
 if (!e->store) return -EIO;
 mutex_lock(&s->sysfs_lock);
 ret=e->store(&s->monitor,buf,count);
 mutex_unlock(&s->sysfs_lock);
 return ret;
}
#define MONITOR_ENTRY(n) { .attr={.name=#n,.mode=0644}, .show=os44_ufsmonitor_sysfs_show_##n, .store=os44_ufsmonitor_sysfs_store_##n }
struct os44_ufsmonitor_entry os44_ufsmonitor_sysfs_entries[7]={
 MONITOR_ENTRY(monitor_enable),
 MONITOR_ENTRY(monitor_latency_read_threshold_us),
 MONITOR_ENTRY(monitor_latency_write_threshold_us),
 MONITOR_ENTRY(monitor_window_period_s),
 MONITOR_ENTRY(monitor_daily_cmd_lat),
 MONITOR_ENTRY(monitor_window_cmd_lat),
 {}
};
const struct sysfs_ops os44_ufsmonitor_sysfs_ops={.show=os44_ufsmonitor_attr_show,.store=os44_ufsmonitor_attr_store};
/* Stock has no release callback: do not silently invent ownership semantics. */
struct kobj_type os44_ufsmonitor_ktype={.sysfs_ops=&os44_ufsmonitor_sysfs_ops};
