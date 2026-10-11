/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_SNAPSHOT_V2_SYSFS_H
#define OS44_SNAPSHOT_V2_SYSFS_H
#include "os44_snapshot.h"
#include "os44_snapshot_v2.h"
struct os44_snapshot_v2_entry { struct attribute attr; ssize_t (*show)(struct os44_snapshot_v2_view *, char *); ssize_t (*store)(struct os44_snapshot_v2_view *, const char *, size_t); };
static_assert(sizeof(struct os44_snapshot_v2_entry)==32);
static_assert(offsetof(struct os44_snapshot_v2_entry,show)==16);
static_assert(offsetof(struct os44_snapshot_v2_entry,store)==24);
int os44_ufssnapshot_create_sysfsV2(struct os44_snapshot_view *,struct os44_snapshot_v2_view *);
void os44_ufssnapshotV2_init(struct ufs_hba *);
#endif
