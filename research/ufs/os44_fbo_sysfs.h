/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_SYSFS_H
#define OS44_FBO_SYSFS_H
#include "os44_fbo_lifetime.h"
ssize_t os44_ufsfbo_sysfs_show_fbo_rec_lrs(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_max_lrs(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_min_lrs(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_max_lrc(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_lra(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_support(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_version(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_is_ufs31(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_debug(struct os44_fbo_lifetime *, char *);
ssize_t os44_ufsfbo_sysfs_show_block_suspend(struct os44_fbo_lifetime *, char *);
#endif
