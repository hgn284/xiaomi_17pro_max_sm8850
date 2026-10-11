/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstruction-owned signatures and real attribute layout; not original private ABI. */
#ifndef OS44_CLD_SYSFS_H
#define OS44_CLD_SYSFS_H
#include "os44_cld_work.h"
#include <linux/sysfs.h>
struct os44_cld_sysfs_entry {
 struct attribute attr;
 ssize_t (*show)(struct os44_cld_view *, char *);
 ssize_t (*store)(struct os44_cld_view *, const char *, size_t);
};
static_assert(sizeof(struct os44_cld_sysfs_entry)==32);
static_assert(offsetof(struct os44_cld_sysfs_entry,show)==16);
static_assert(offsetof(struct os44_cld_sysfs_entry,store)==24);
int os44_ufscld_get_frag_level(struct os44_cld_view *, u32 *);
int os44_ufscld_trigger_on(struct os44_cld_view *);
void os44_ufscld_release_runtime_pm(struct os44_cld_view *);
void os44_ufscld_auto_hibern8_enable(struct os44_cld_view *, unsigned int);
ssize_t os44_ufscld_sysfs_show_frag_level(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_show_cld_operation_status(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_show_hid_available_size(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_show_hid_state(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_show_trigger(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_store_trigger(struct os44_cld_view *, const char *, size_t);
ssize_t os44_ufscld_sysfs_show_trigger_interval(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_store_trigger_interval(struct os44_cld_view *, const char *, size_t);
ssize_t os44_ufscld_sysfs_show_work_state(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_show_debug(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_store_debug(struct os44_cld_view *, const char *, size_t);
ssize_t os44_ufscld_sysfs_show_block_suspend(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_store_block_suspend(struct os44_cld_view *, const char *, size_t);
ssize_t os44_ufscld_sysfs_show_auto_hibern8_enable(struct os44_cld_view *, char *);
ssize_t os44_ufscld_sysfs_store_auto_hibern8_enable(struct os44_cld_view *, const char *, size_t);
#endif
