/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_SNAPSHOT_SYSFS_H
#define OS44_SNAPSHOT_SYSFS_H
#include "os44_snapshot.h"
struct os44_snapshot_entry { struct attribute attr; ssize_t (*show)(struct os44_snapshot_view *, char *); ssize_t (*store)(struct os44_snapshot_view *, const char *, size_t); };
static_assert(sizeof(struct os44_snapshot_entry)==32);
static_assert(offsetof(struct os44_snapshot_entry,show)==16);
static_assert(offsetof(struct os44_snapshot_entry,store)==24);
ssize_t os44_ufssnapshot_sysfs_show_ctrl(struct os44_snapshot_view *, char *);
ssize_t os44_ufssnapshot_sysfs_store_ctrl(struct os44_snapshot_view *, const char *, size_t);
ssize_t os44_ufssnapshot_sysfs_show_status(struct os44_snapshot_view *, char *);
ssize_t os44_ufssnapshot_sysfs_show_dropfreesize(struct os44_snapshot_view *, char *);
ssize_t os44_ufssnapshot_sysfs_store_dropfreesize(struct os44_snapshot_view *, const char *, size_t);
ssize_t os44_ufssnapshot_sysfs_show_debug(struct os44_snapshot_view *, char *);
ssize_t os44_ufssnapshot_sysfs_store_debug(struct os44_snapshot_view *, const char *, size_t);
ssize_t os44_ufssnapshot_sysfs_show_freespace(struct os44_snapshot_view *, char *);
ssize_t os44_ufssnapshot_sysfs_show_support(struct os44_snapshot_view *, char *);
ssize_t os44_ufssnapshot_sysfs_show_version(struct os44_snapshot_view *, char *);
ssize_t os44_ufssnapshot_sysfs_show_eecount(struct os44_snapshot_view *, char *);
ssize_t os44_ufssnapshot_sysfs_show_backupcancelled(struct os44_snapshot_view *, char *);
int os44_ufssnapshot_create_sysfs(struct os44_snapshot_view *);
#endif
