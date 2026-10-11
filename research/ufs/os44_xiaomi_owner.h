/* SPDX-License-Identifier: GPL-2.0-only */
/* One reconstruction-owned layout declaration for the shared 5976-byte owner.
 * Anonymous overlapping views retain known fields, NOT stock private C types.
 * No storage or getter definition: real probe/publication/remove still pending. */
#ifndef OS44_XIAOMI_OWNER_H
#define OS44_XIAOMI_OWNER_H
#include "ufsswap_sysfs_reconstruction.h"
#include "os44_cld_view.h"
#include "os44_fastdiscard_sysfs.h"
#include "os44_ufsmonitor_sysfs.h"
#include "os44_fbo_lifetime.h"
#include "os44_snapshot.h"
#include "os44_snapshot_v2.h"
#include "os44_eua.h"
struct scsi_device;
/* Only accessed control fields recovered; not a complete V2 private declaration. */
struct os44_snapshot_v2_ctrl_record { u32 ctrl; u8 unexamined[0xe0-4]; };
static_assert(sizeof(struct os44_snapshot_v2_ctrl_record)==0xe0);
struct os44_remove_owner_view {
 union {
 struct { struct ufs_hba *queue_hba; };
 struct { u8 eua_prefix[0x588];struct os44_eua_view eua; };
 struct { u8 snapshot_v2_prefix[0x8e0]; struct os44_snapshot_v2_view snapshot_v2; };
 struct { u8 snapshot_prefix[0x818]; struct os44_snapshot_view snapshot; };
 struct { u8 snapshot_v2_selected_prefix[0x989]; u8 snapshot_v2_selected; };
 struct { u8 snapshot_v2_ctrl_prefix[0x998]; struct os44_snapshot_v2_ctrl_record snapshot_v2_ctrl[7]; };
 struct { u8 snapshot_v2_debug_prefix[0x990]; u8 snapshot_v2_debug; };
 struct { u8 snapshot_count_prefix[0x8d8]; u64 snapshot_ee_count; };
 struct { u8 snapshot_time_prefix[0x1750]; unsigned long snapshot_last_event; };
 struct { u8 fbo_prefix[0x658]; struct os44_fbo_lifetime fbo; };
 struct {
 union {
  unsigned char before_swap[0x170];
  struct { unsigned char cld_prefix[8]; struct os44_cld_view cld; };
 };
 struct os44_swap_sysfs swap;
 unsigned char before_eua_kobj[0x5c0 - 0x170 - sizeof(struct os44_swap_sysfs)];
 struct kobject eua_kobj;
 struct mutex eua_lock;
 unsigned char before_fastdiscard[0xed8 - 0x620 - sizeof(struct mutex)];
 struct os44_fastdiscard_ctrl fastdiscard;
 unsigned char before_monitor[0xfa8 - 0xed8 - sizeof(struct os44_fastdiscard_ctrl)];
 struct os44_ufsmonitor_sysfs monitor;
 unsigned char tail[0x1758 - 0xfa8 - sizeof(struct os44_ufsmonitor_sysfs)];
 };
 struct {
  unsigned char reserved_000[0x598];
  struct scsi_device *sdev_598;
  unsigned char reserved_5a0[0xc0];
  struct scsi_device *sdev_660;
  unsigned char reserved_668[0x250];
  struct scsi_device *sdev_8b8;
  unsigned char reserved_8c0[0xc0];
  struct scsi_device *sdev_980;
 };
 struct { unsigned char stats_prefix[0xfb0]; struct os44_completion_stats stats; };
 };
};
static_assert(offsetof(struct os44_remove_owner_view,swap)==0x170);
static_assert(offsetof(struct os44_remove_owner_view,swap.kobj)==0x1a8);
static_assert(offsetof(struct os44_remove_owner_view,swap.lock)==0x208);
static_assert(offsetof(struct os44_remove_owner_view,eua_kobj)==0x5c0);
static_assert(offsetof(struct os44_remove_owner_view,eua_lock)==0x620);
static_assert(offsetof(struct os44_remove_owner_view,fastdiscard)==0xed8);
static_assert(sizeof(struct os44_remove_owner_view)==0x1758);
static_assert(offsetof(struct os44_remove_owner_view,sdev_598)==1432);
static_assert(offsetof(struct os44_remove_owner_view,sdev_660)==1632);
static_assert(offsetof(struct os44_remove_owner_view,sdev_8b8)==2232);
static_assert(offsetof(struct os44_remove_owner_view,sdev_980)==2432);
static_assert(offsetof(struct os44_remove_owner_view,stats)==4016);
static_assert(offsetof(struct os44_remove_owner_view,monitor)==4008);
static_assert(offsetof(struct os44_remove_owner_view,monitor.monitor.stats)==4016);
static_assert(offsetof(struct os44_remove_owner_view,monitor.kobj)==5816);
static_assert(offsetof(struct os44_remove_owner_view,monitor.sysfs_lock)==5912);
static_assert(offsetof(struct os44_remove_owner_view,monitor.entries)==5960);
static_assert(offsetof(struct os44_remove_owner_view,queue_hba)==0);
static_assert(offsetof(struct os44_remove_owner_view,swap.swap.config)==392);
static_assert(offsetof(struct os44_remove_owner_view,swap.swap.enabled)==404);
static_assert(offsetof(struct os44_remove_owner_view,swap.swap.head)==408);
static_assert(offsetof(struct os44_remove_owner_view,fbo)==0x658);
static_assert(offsetof(struct os44_remove_owner_view,fbo.debug)==0x800);
static_assert(offsetof(struct os44_remove_owner_view,snapshot_ee_count)==0x8d8);
static_assert(offsetof(struct os44_remove_owner_view,snapshot_last_event)==0x1750);
static_assert(offsetof(struct os44_remove_owner_view,snapshot)==0x818);
static_assert(offsetof(struct os44_remove_owner_view,snapshot.ee_count)==0x8d8);
static_assert(offsetof(struct os44_remove_owner_view,snapshot_v2_debug)==0x990);
static_assert(offsetof(struct os44_remove_owner_view,snapshot_v2_selected)==0x989);
static_assert(offsetof(struct os44_remove_owner_view,snapshot_v2_ctrl)==0x998);
static_assert(offsetof(struct os44_remove_owner_view,snapshot_v2)==0x8e0);
static_assert(offsetof(struct os44_remove_owner_view,eua)==0x588);
static_assert(offsetof(struct os44_remove_owner_view,eua.kobj)==0x5c0);
static_assert(offsetof(struct os44_remove_owner_view,eua.lock)==0x620);
static_assert(offsetof(struct os44_remove_owner_view,fbo)+sizeof(struct os44_fbo_lifetime)==offsetof(struct os44_remove_owner_view,snapshot));
extern struct os44_remove_owner_view os44_ufs_xiaomi;
struct os44_remove_owner_view *os44_get_ufs_xiaomi_for_remove(void);
int os44_ufsmonitor_create_sysfs(struct ufs_hba *hba);
#endif
