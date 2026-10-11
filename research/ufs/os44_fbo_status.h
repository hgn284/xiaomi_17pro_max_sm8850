/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_STATUS_H
#define OS44_FBO_STATUS_H
#include "os44_fbo_power.h"
/* Reconstruction-owned shared contract, not original private type identity. */
int os44_ufsfbs_get_ops(struct os44_fbo_lifetime *,int *);
int os44_ufsfbs_get_level_check_ops(struct os44_fbo_lifetime *,int *);
ssize_t os44_ufsfbo_sysfs_show_fbs_flc_ops(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_show_fbs_defrag_ops(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_store_fbs_flc_enable(struct os44_fbo_lifetime *,const char *,size_t);
ssize_t os44_ufsfbo_sysfs_store_fbs_defrag_enable(struct os44_fbo_lifetime *,const char *,size_t);
#endif
