/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_SNAPSHOT_H
#define OS44_SNAPSHOT_H
#include <ufs/ufshcd.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
/* Reconstructed shared view, not original private type identity. No storage. */
struct os44_snapshot_entry;
struct os44_snapshot_view {
 struct ufs_hba *hba;
 struct kobject kobj;
 struct os44_snapshot_entry *entries;
 struct mutex lock;
 struct scsi_device *sdev;
 u8 vendor, debug;
 u8 reserved0aa[2];
 u32 ctrl;
 u32 status;
 u32 drop_freesize;
 u16 version;
 u16 reserved0ba;
 u32 supported;
 u64 ee_count;
};
static_assert(offsetof(struct os44_snapshot_view,ctrl)==0xac);
static_assert(offsetof(struct os44_snapshot_view,kobj)==8);
static_assert(offsetof(struct os44_snapshot_view,entries)==104);
static_assert(offsetof(struct os44_snapshot_view,lock)==112);
static_assert(offsetof(struct os44_snapshot_view,sdev)==160);
static_assert(offsetof(struct os44_snapshot_view,vendor)==168);
static_assert(offsetof(struct os44_snapshot_view,debug)==169);
static_assert(offsetof(struct os44_snapshot_view,status)==176);
static_assert(offsetof(struct os44_snapshot_view,drop_freesize)==180);
static_assert(offsetof(struct os44_snapshot_view,version)==184);
static_assert(offsetof(struct os44_snapshot_view,supported)==188);
static_assert(offsetof(struct os44_snapshot_view,ee_count)==192);
int os44_ufssnapshot_query_attr(struct ufs_hba *, unsigned int, unsigned int, u32 *);
int os44_ufssnapshot_read_desc(struct ufs_hba *, unsigned int, u8 *, int);
int os44_ufssnapshot_get_info(struct os44_snapshot_view *, u32 *, u16 *);
int os44_ufssnapshot_set_drop_freesize(struct os44_snapshot_view *, u32);
int os44_ufssnapshot_get_bBackupCancelled(struct os44_snapshot_view *, u32 *);
#endif
