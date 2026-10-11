/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_SNAPSHOT_FLOW_H
#define OS44_SNAPSHOT_FLOW_H
#include "os44_snapshot.h"
typedef int (*os44_snapshot_flow_fn)(struct os44_snapshot_view *);
int os44_ufssnapshot_start(struct os44_snapshot_view *);
int os44_ufssnapshot_stop(struct os44_snapshot_view *);
int os44_ufssnapshot_drop(struct os44_snapshot_view *);
int os44_ufssnapshot_active(struct os44_snapshot_view *);
int os44_ufsnapshot_backup_flow_ymtc(struct os44_snapshot_view *);
int os44_ufssnapshot_query_status(struct os44_snapshot_view *);
os44_snapshot_flow_fn os44_ufssnapshot_find_flows(struct os44_snapshot_view *, u32);
long os44_ufssnapshot_do_ctrl(struct os44_snapshot_view *, u32);
#endif
