/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_UFSMONITOR_SYSFS_H
#define OS44_UFSMONITOR_SYSFS_H
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include "os44_ufsmonitor.h"
struct os44_ufsmonitor_entry {
 struct attribute attr;
 ssize_t (*show)(struct os44_ufsmonitor *, char *);
 ssize_t (*store)(struct os44_ufsmonitor *, const char *, size_t);
};
/* Source-pair view, no owner instantiated. Prefix shared with completion stats. */
struct os44_ufsmonitor_sysfs {
 struct os44_ufsmonitor monitor;
 struct kobject kobj;
 struct mutex sysfs_lock;
 struct os44_ufsmonitor_entry *entries;
};
static_assert(sizeof(struct os44_ufsmonitor_entry)==32);
static_assert(offsetof(struct os44_ufsmonitor_entry,show)==16);
static_assert(offsetof(struct os44_ufsmonitor_entry,store)==24);
static_assert(offsetof(struct os44_ufsmonitor_sysfs,kobj)==0x710);
static_assert(offsetof(struct os44_ufsmonitor_sysfs,sysfs_lock)==0x770);
static_assert(offsetof(struct os44_ufsmonitor_sysfs,entries)==0x7a0);
extern struct os44_ufsmonitor_entry os44_ufsmonitor_sysfs_entries[7];
extern const struct sysfs_ops os44_ufsmonitor_sysfs_ops;
extern struct kobj_type os44_ufsmonitor_ktype;
#endif
