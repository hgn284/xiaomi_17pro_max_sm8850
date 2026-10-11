/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_CONTROL_SYSFS_H
#define OS44_FBO_CONTROL_SYSFS_H
#include "os44_fbo_power.h"
/* Source-owned signatures, not original private type identity. */
ssize_t os44_ufsfbo_sysfs_show_fbo_err_cnt(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_store_fbo_err_cnt(struct os44_fbo_lifetime *,const char *,size_t);
ssize_t os44_ufsfbo_sysfs_show_fbo_retry_cnt(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_store_fbo_retry_cnt(struct os44_fbo_lifetime *,const char *,size_t);
ssize_t os44_ufsfbo_sysfs_store_debug(struct os44_fbo_lifetime *,const char *,size_t);
ssize_t os44_ufsfbo_sysfs_store_block_suspend(struct os44_fbo_lifetime *,const char *,size_t);
ssize_t os44_ufsfbo_sysfs_show_auto_hibern8_enable(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_store_auto_hibern8_enable(struct os44_fbo_lifetime *,const char *,size_t);
#endif
