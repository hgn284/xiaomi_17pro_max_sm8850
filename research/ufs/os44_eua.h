/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstructed access view, not original private type identity. No owner storage. */
#ifndef OS44_EUA_H
#define OS44_EUA_H
#include "ufsswap_provider_reconstruction.h"
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
struct os44_eua_view;
struct os44_eua_ops {
 int (*get_mapping_status)(struct os44_eua_view *,u8 *);
 int (*get_total_used_lba)(struct os44_eua_view *,u32 *);
 int (*get_total_free_space)(struct os44_eua_view *,u32 *);
};
struct os44_eua_entry;
struct os44_eua_view {
 struct ufs_hba *hba;
 const struct os44_eua_ops *ops;
 struct scsi_device *sdev;
 u8 support,enable,hide_size;
 u8 unexamined_1b;
 u16 mapping_length;
 u8 unexamined_1e[2];
 u64 mapping_start,mapping_end;
 int support_size;
 u8 unexamined_34[4];
 struct kobject kobj;
 struct mutex lock;
 struct os44_eua_entry *entries;
};
struct os44_eua_entry { struct attribute attr;ssize_t (*show)(struct os44_eua_view *,char *);ssize_t (*store)(struct os44_eua_view *,const char *,size_t); };
static_assert(offsetof(struct os44_eua_view,ops)==8);
static_assert(offsetof(struct os44_eua_view,support)==0x18);
static_assert(offsetof(struct os44_eua_view,support_size)==0x30);
static_assert(offsetof(struct os44_eua_view,kobj)==0x38);
static_assert(offsetof(struct os44_eua_view,lock)==0x98);
static_assert(offsetof(struct os44_eua_view,entries)==0xc8);
static_assert(sizeof(struct os44_eua_view)==0xd0);
static_assert(sizeof(struct os44_eua_entry)==32);
int os44_ufseua_read_desc(struct ufs_hba *,unsigned int,u8 *,int);
ssize_t os44_ufseua_sysfs_show_eua_support(struct os44_eua_view *,char *);
ssize_t os44_ufseua_sysfs_show_eua_support_size(struct os44_eua_view *,char *);
ssize_t os44_ufseua_sysfs_show_eua_enable(struct os44_eua_view *,char *);
ssize_t os44_ufseua_sysfs_show_eua_hide_size(struct os44_eua_view *,char *);
ssize_t os44_ufseua_sysfs_show_eua_get_total_used_lba(struct os44_eua_view *,char *);
ssize_t os44_ufseua_sysfs_show_eua_get_total_free_space(struct os44_eua_view *,char *);
ssize_t os44_ufseua_attr_show(struct kobject *,struct attribute *,char *);
ssize_t os44_ufseua_attr_store(struct kobject *,struct attribute *,const char *,size_t);
ssize_t os44_ufseua_sysfs_show_eua_get_mapping_status(struct os44_eua_view *,char *);
ssize_t os44_ufseua_sysfs_store_eua_get_mapping_status(struct os44_eua_view *,const char *,size_t);
ssize_t os44_ufseua_sysfs_store_eua_file_lba_unmap(struct os44_eua_view *,const char *,size_t);
int os44_ufseua_create_sysfs(struct os44_eua_view *);
static_assert(offsetof(struct os44_eua_view,sdev)==0x10);
static_assert(offsetof(struct os44_eua_view,mapping_length)==0x1c);
static_assert(offsetof(struct os44_eua_view,mapping_start)==0x20);
static_assert(offsetof(struct os44_eua_view,mapping_end)==0x28);
#endif
