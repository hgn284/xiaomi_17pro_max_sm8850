/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_SNAPSHOT_CTRL_H
#define OS44_SNAPSHOT_CTRL_H
#include "os44_snapshot.h"
struct os44_snapshot_ctrl_name { u32 ctrl; char name[20]; };
static_assert(sizeof(struct os44_snapshot_ctrl_name)==24);
/* Writable like the original .data objects; not invented owner storage. */
extern struct os44_snapshot_ctrl_name os44_ctrl_maps[5], os44_ctrl_mapsV2[3];
long os44_ufssnapshotV2_find_ctrl_name_default(u32 ctrl, char *name);
long os44_ufssnapshot_find_ctrl_by_nameV2(u32 *ctrl, const char *name);
#endif
