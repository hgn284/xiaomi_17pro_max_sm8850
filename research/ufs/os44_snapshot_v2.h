/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_SNAPSHOT_V2_H
#define OS44_SNAPSHOT_V2_H
#include <ufs/ufshcd.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
struct os44_snapshot_v2_entry;
/* Access-only V2 view, NOT V1: V2 debug is 0xb0, V1 debug is 0xa9.
 * kobject/entries/mutex prefix recovered in run119. No owner storage. */
struct os44_snapshot_v2_range { u8 lun; u8 reserved[3]; u32 first, last; };
struct os44_snapshot_v2_record {
 u32 ctrl, status; u8 slot; u8 reserved009[7]; u64 address; u8 count; u8 reserved019[3];
 struct os44_snapshot_v2_range ranges[16]; u8 reserved0dc[4];
};
static_assert(sizeof(struct os44_snapshot_v2_record)==224);
struct os44_snapshot_v2_view {
 struct ufs_hba *hba;
 struct kobject kobj;
 struct os44_snapshot_v2_entry *entries;
 struct mutex lock;
 struct scsi_device *sdev;
 u8 vendor, selected;
 u8 unexamined0aa[2];
 u32 buffer_id;
 u8 debug;
 u8 unexamined0b1[3];
 u32 progress;
 struct os44_snapshot_v2_record records[7];
};
static_assert(offsetof(struct os44_snapshot_v2_view,sdev)==0xa0);
static_assert(offsetof(struct os44_snapshot_v2_view,vendor)==0xa8);
static_assert(offsetof(struct os44_snapshot_v2_view,buffer_id)==0xac);
static_assert(offsetof(struct os44_snapshot_v2_view,debug)==0xb0);
int os44_ufssnapshotV2_rw_buffer(struct os44_snapshot_v2_view *, u8, int, u8, u32, u8 *, bool);
static_assert(offsetof(struct os44_snapshot_v2_view,records)==0xb8);
static_assert(offsetof(struct os44_snapshot_v2_record,address)==16);
static_assert(offsetof(struct os44_snapshot_v2_record,ranges)==28);
typedef int (*os44_snapshot_v2_flow_fn)(struct os44_snapshot_v2_view *);
int os44_ufssnapshotV2_query_attr(struct ufs_hba *, unsigned int, unsigned int, u32 *);
int os44_ufssnapshot_start_V2(struct os44_snapshot_v2_view *);
int os44_ufssnapshot_stop_V2(struct os44_snapshot_v2_view *);
os44_snapshot_v2_flow_fn os44_ufssnapshotV2_find_flows(struct os44_snapshot_v2_view *,u32);
long os44_ufssnapshot_do_ctrl_V2(struct os44_snapshot_v2_view *,u32);
static_assert(offsetof(struct os44_snapshot_v2_view,progress)==0xb4);
static_assert(offsetof(struct os44_snapshot_v2_record,slot)==8);
int os44_ufssnapshot_query_progress(struct os44_snapshot_v2_view *);
ssize_t os44_ufssnapshotV2_sysfs_show_dropslot(struct os44_snapshot_v2_view *,char *);
ssize_t os44_ufssnapshotV2_sysfs_store_dropslot(struct os44_snapshot_v2_view *,const char *,size_t);
ssize_t os44_ufssnapshotV2_sysfs_show_progress(struct os44_snapshot_v2_view *,char *);
ssize_t os44_ufssnapshotV2_sysfs_show_range(struct os44_snapshot_v2_view *,char *);
ssize_t os44_ufssnapshotV2_sysfs_store_range(struct os44_snapshot_v2_view *,const char *,size_t);
ssize_t os44_ufssnapshotV2_sysfs_show_status(struct os44_snapshot_v2_view *,char *);
static_assert(offsetof(struct os44_snapshot_v2_view,kobj)==8);
static_assert(offsetof(struct os44_snapshot_v2_view,entries)==0x68);
static_assert(offsetof(struct os44_snapshot_v2_view,lock)==0x70);
#endif
