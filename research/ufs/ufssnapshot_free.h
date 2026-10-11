/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_SNAPSHOT_FREE_H
#define OS44_SNAPSHOT_FREE_H
#include "os44_snapshot.h"
int os44_hynix_get_free_space(struct os44_snapshot_view *, u32 *);
int os44_micron_get_free_space(struct os44_snapshot_view *, u32 *);
int os44_samsung_get_free_space(struct os44_snapshot_view *, u32 *);
ssize_t os44_ufssnapshot_sysfs_show_freespace(struct os44_snapshot_view *, char *);
#endif
