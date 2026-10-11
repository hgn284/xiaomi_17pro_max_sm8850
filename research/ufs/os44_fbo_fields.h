/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_FIELDS_H
#define OS44_FBO_FIELDS_H
#include "os44_fbo_lifetime.h"
ssize_t os44_ufsfbo_sysfs_show_fbo_wholefile_enable(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_map_state(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_mapping_start(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_show_fbo2_linear_mapping_end(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_store_fbo_wholefile_enable(struct os44_fbo_lifetime *,const char *,size_t);
#endif
