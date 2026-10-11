/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FASTDISCARD_SYSFS_H
#define OS44_FASTDISCARD_SYSFS_H
#include "os44_fastdiscard_contract.h"
#include <linux/kobject.h>
#include <linux/mutex.h>
/* Reconstruction-owned shared source-pair types. Original private C tag
 * identity is unknown. No binary callback ABI compatibility claimed. */
struct os44_fastdiscard_attr;
struct os44_fastdiscard_ctrl {
 struct ufs_hba *hba;
 u8 debug_enabled;
 unsigned long unmap_count;
 s64 unmap_time;
 unsigned long multi_unmap_count;
 s64 multi_unmap_time;
 unsigned long multi_unmap_lba;
 struct kobject kobj;
 struct mutex sysfs_lock;
 struct os44_fastdiscard_attr *attrs;
};
struct os44_fastdiscard_attr {
 struct attribute attr;
 ssize_t (*show)(struct os44_fastdiscard_ctrl *, char *);
 ssize_t (*store)(struct os44_fastdiscard_ctrl *, const char *, size_t);
};
static_assert(offsetof(struct os44_fastdiscard_ctrl,debug_enabled)==8);
static_assert(offsetof(struct os44_fastdiscard_ctrl,unmap_count)==0x10);
static_assert(offsetof(struct os44_fastdiscard_ctrl,unmap_time)==0x18);
static_assert(offsetof(struct os44_fastdiscard_ctrl,multi_unmap_count)==0x20);
static_assert(offsetof(struct os44_fastdiscard_ctrl,multi_unmap_time)==0x28);
static_assert(offsetof(struct os44_fastdiscard_ctrl,multi_unmap_lba)==0x30);
static_assert(offsetof(struct os44_fastdiscard_ctrl,kobj)==0x38);
static_assert(offsetof(struct os44_fastdiscard_ctrl,sysfs_lock)==0x98);
static_assert(offsetof(struct os44_fastdiscard_ctrl,attrs)==0xc8);
static_assert(sizeof(struct os44_fastdiscard_attr)==32);
int os44_fastdiscard_create_sysfs(struct os44_fastdiscard_ctrl *);
#endif
