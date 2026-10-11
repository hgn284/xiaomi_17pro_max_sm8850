/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_PROGRESS_H
#define OS44_FBO_PROGRESS_H
#include "os44_fbo_status.h"
/* Source-owned shared declarations, original private type identity unproven. */
int os44_ufsfbo_get_fbo_prog_state(struct os44_fbo_lifetime *,u32 *);
int os44_ufsfbo_get_exe_level(struct os44_fbo_lifetime *,u32 *);
int os44_ufsfbo_set_exe_level(struct os44_fbo_lifetime *,u32 *);
ssize_t os44_ufsfbo_sysfs_show_fbo_prog_state(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_show_fbo_exe_threshold(struct os44_fbo_lifetime *,char *);
ssize_t os44_ufsfbo_sysfs_store_fbo_exe_threshold(struct os44_fbo_lifetime *,const char *,size_t);
#endif
