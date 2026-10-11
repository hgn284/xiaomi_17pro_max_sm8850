/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_CONSTRUCTOR_H
#define OS44_FBO_CONSTRUCTOR_H
#include "os44_fbo_lifetime.h"
#include <linux/sysfs.h>
struct os44_fbo_entry {
 struct attribute attr;
 ssize_t (*show)(struct os44_fbo_lifetime *, char *);
 ssize_t (*store)(struct os44_fbo_lifetime *, const char *, size_t);
};
static_assert(sizeof(struct os44_fbo_entry)==32);
static_assert(offsetof(struct os44_fbo_entry,show)==16);
static_assert(offsetof(struct os44_fbo_entry,store)==24);
int os44_ufsfbo_is_not_present(struct os44_fbo_lifetime *);
ssize_t os44_ufsfbo_attr_show(struct kobject *, struct attribute *, char *);
ssize_t os44_ufsfbo_attr_store(struct kobject *, struct attribute *, const char *, size_t);
int os44_ufsfbo_create_sysfs(struct os44_fbo_lifetime *);
ssize_t os44_ufsfbo_sysfs_show_auto_hibern8_enable(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_block_suspend(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_debug(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_map_state(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_mapping_end(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_mapping_start(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo3_get_lr_pinned_info(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_err_cnt(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_exe_threshold(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_get_lr_frag_level(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_lra(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_max_lrc(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_max_lrs(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_min_lrs(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_prog_state(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_rec_lrs(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_retry_cnt(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_support(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_version(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_wholefile_enable(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbs_defrag_ops(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbs_flc_ops(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_is_ufs31(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_store_auto_hibern8_enable(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_block_suspend(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_debug(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo3_send_lr_list(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo3_ss_send_pin_lr_list(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo_app_preload(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo_err_cnt(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo_exe_threshold(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo_operation_control(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo_retry_cnt(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo_send_lr_list(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbo_wholefile_enable(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbs_defrag_enable(struct os44_fbo_lifetime *, const char *, size_t);
ssize_t os44_ufsfbo_sysfs_store_fbs_flc_enable(struct os44_fbo_lifetime *, const char *, size_t);
#endif
