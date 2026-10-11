/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_LIFETIME_H
#define OS44_FBO_LIFETIME_H
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <ufs/ufshcd.h>
/* Reconstruction-owned layout, not recovered original private type identity.
 * No storage; producer and static-owner deallocation conflict remain open. */
struct os44_fbo_entry;
struct os44_fbo_lifetime {
 struct ufs_hba *hba;
 struct scsi_device *sdev;
 u32 control_010; /* zeroed at init; original field name unknown */
 u8 reserved_014[0x80-0x14];
 u32 scan_interval_ms;
 u32 saved_ahit;
 u8 is_auto_enabled;
 u8 block_suspend;
 u8 reserved_08a[2];
 int state;
 struct kobject kobj;
 struct mutex lock;
 struct os44_fbo_entry *entries;
 struct mutex range_lock;
 u32 lr_count;
 u8 vendor_selector;
 char model[16];
 u8 is_ufs31;
 u8 supported;
 u8 reserved_16f;
 u16 version;
 u8 reserved_172[2];
 u32 recommended_lrs;
 u32 maximum_lrs;
 u32 minimum_lrs;
 u32 maximum_lrc;
 u32 lra;
 u8 reserved_188;
 u8 linear_state;
 u8 enforce_range_limit;
 u8 reserved_18b;
 u32 linear_start;
 u32 linear_end;
 u8 reserved_194[4];
 u8 pin_check_pending;
 u8 reserved_199[7];
 u64 total_blocks;
 u8 debug;
 u8 wholefile;
 u64 fbo_err_cnt;
 u64 fbo_retry_cnt;
};
static_assert(offsetof(struct os44_fbo_lifetime,control_010)==0x10);
static_assert(offsetof(struct os44_fbo_lifetime,scan_interval_ms)==0x80);
static_assert(offsetof(struct os44_fbo_lifetime,entries)==0x120);
static_assert(offsetof(struct os44_fbo_lifetime,range_lock)==0x128);
static_assert(offsetof(struct os44_fbo_lifetime,saved_ahit)==0x84);
static_assert(offsetof(struct os44_fbo_lifetime,is_auto_enabled)==0x88);
static_assert(offsetof(struct os44_fbo_lifetime,block_suspend)==0x89);
static_assert(offsetof(struct os44_fbo_lifetime,state)==0x8c);
static_assert(offsetof(struct os44_fbo_lifetime,kobj)==0x90);
static_assert(offsetof(struct os44_fbo_lifetime,lock)==0xf0);
static_assert(offsetof(struct os44_fbo_lifetime,debug)==0x1a8);
static_assert(offsetof(struct os44_fbo_lifetime,vendor_selector)==0x15c);
static_assert(offsetof(struct os44_fbo_lifetime,model)==0x15d);
static_assert(offsetof(struct os44_fbo_lifetime,is_ufs31)==0x16d);
static_assert(offsetof(struct os44_fbo_lifetime,supported)==0x16e);
static_assert(offsetof(struct os44_fbo_lifetime,version)==0x170);
static_assert(offsetof(struct os44_fbo_lifetime,recommended_lrs)==0x174);
static_assert(offsetof(struct os44_fbo_lifetime,maximum_lrs)==0x178);
static_assert(offsetof(struct os44_fbo_lifetime,minimum_lrs)==0x17c);
static_assert(offsetof(struct os44_fbo_lifetime,maximum_lrc)==0x180);
static_assert(offsetof(struct os44_fbo_lifetime,lra)==0x184);
static_assert(offsetof(struct os44_fbo_lifetime,pin_check_pending)==0x198);
static_assert(sizeof(struct os44_fbo_lifetime)==0x1c0);
static_assert(offsetof(struct os44_fbo_lifetime,fbo_err_cnt)==0x1b0);
static_assert(offsetof(struct os44_fbo_lifetime,fbo_retry_cnt)==0x1b8);
static_assert(offsetof(struct os44_fbo_lifetime,linear_state)==0x189);
static_assert(offsetof(struct os44_fbo_lifetime,linear_start)==0x18c);
static_assert(offsetof(struct os44_fbo_lifetime,linear_end)==0x190);
static_assert(offsetof(struct os44_fbo_lifetime,wholefile)==0x1a9);
static_assert(offsetof(struct os44_fbo_lifetime,sdev)==0x8);
static_assert(offsetof(struct os44_fbo_lifetime,lr_count)==0x158);
static_assert(offsetof(struct os44_fbo_lifetime,enforce_range_limit)==0x18a);
static_assert(offsetof(struct os44_fbo_lifetime,total_blocks)==0x1a0);
/* 0x1c0 is the proven accessed extent, not original private sizeof proof. */
int os44_ufsfbo_get_fbo_desc_info(struct os44_fbo_lifetime *);
int os44_ufsfbo_get_fbo_support_state(struct os44_fbo_lifetime *);
void os44_ufsfbo_allow_enter_suspend(struct os44_fbo_lifetime *);
void os44_ufsfbo_remove_sysfs(struct os44_fbo_lifetime *);
void os44_ufsfbo_remove(struct ufs_hba *);
void os44_ufsfbo_init(struct ufs_hba *);
#endif
