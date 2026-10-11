/* SPDX-License-Identifier: GPL-2.0-only */
/* Source-owned partial view; offsets are instruction-derived, not original type names.
 * No storage/getter definition. Original owner initialization remains mandatory. */
#ifndef OS44_CLD_VIEW_H
#define OS44_CLD_VIEW_H
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
struct ufs_hba;
struct os44_cld_ops;
struct os44_cld_vendor;
struct os44_cld_sysfs_entry;
struct os44_cld_identity { char vendor[8]; char product[16]; char revision[4]; };
static_assert(sizeof(struct os44_cld_identity)==28);
struct os44_cld_view {
 struct ufs_hba *hba;
 int trigger;
 unsigned char pad_c[4];
 struct delayed_work idle_work;
 u32 interval_ms; /* original init stores2000 at CLD+0x78 */
 bool is_auto_enabled;
 unsigned char before_last_status[3];
 s64 last_status_ms; /* original Samsung status helper reads/writes +0x80 */
 atomic_t state;
 unsigned char before_kobj[4];
 struct kobject kobj;
 struct mutex sysfs_lock;
 unsigned char before_entries[0x120 - 0xf0 - sizeof(struct mutex)];
 struct os44_cld_sysfs_entry *entries;
 union { unsigned long callback_present; const struct os44_cld_ops *ops; };
 const struct os44_cld_vendor *vendor;
 bool debug;
 u8 block_suspend; /* stock block tests bit0; allow requires byte==1 */
 unsigned char before_saved_ahit[2];
 u32 saved_ahit;
 u64 trigger_start_ms; /* zeroed by constructor; raw layout, signedness unclaimed */
 struct os44_cld_identity identity; /* real init_ops copies SCSI inquiry bytes */
};
static_assert(offsetof(struct os44_cld_view,trigger)==8);
static_assert(offsetof(struct os44_cld_view,idle_work)==0x10);
static_assert(offsetof(struct os44_cld_view,state)==0x88);
static_assert(offsetof(struct os44_cld_view,callback_present)==0x128);
static_assert(offsetof(struct os44_cld_view,debug)==0x138);
static_assert(offsetof(struct os44_cld_view,kobj)==0x90);
static_assert(offsetof(struct os44_cld_view,sysfs_lock)==0xf0);
static_assert(offsetof(struct os44_cld_view,block_suspend)==0x139);
static_assert(offsetof(struct os44_cld_view,saved_ahit)==0x13c);
static_assert(offsetof(struct os44_cld_view,is_auto_enabled)==0x7c);
static_assert(offsetof(struct os44_cld_view,ops)==0x128);
static_assert(offsetof(struct os44_cld_view,vendor)==0x130);
static_assert(offsetof(struct os44_cld_view,last_status_ms)==0x80);
static_assert(offsetof(struct os44_cld_view,interval_ms)==0x78);
static_assert(offsetof(struct os44_cld_view,entries)==0x120);
static_assert(offsetof(struct os44_cld_view,trigger_start_ms)==0x140);
static_assert(offsetof(struct os44_cld_view,identity)==0x148);
static_assert(offsetof(struct os44_cld_view,identity.product)==0x150);
static_assert(sizeof(struct os44_cld_view)==0x168);
void os44_ufscld_allow_enter_suspend(struct os44_cld_view *);
void os44_ufscld_block_enter_suspend(struct os44_cld_view *);
void os44_ufscld_remove_sysfs(struct os44_cld_view *);
void os44_ufscld_resume(struct ufs_hba *);
struct os44_mcq_idle_view { unsigned char prefix[0xc80]; unsigned long active_cmds; };
#endif
