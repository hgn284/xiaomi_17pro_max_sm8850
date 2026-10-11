/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_PRELOAD_H
#define OS44_FBO_PRELOAD_H
#include "os44_fbo_lba.h"
#include "os44_fbo_operation.h"
int os44_ufsfbo_app_preload(struct os44_fbo_lifetime *,const char *);
ssize_t os44_ufsfbo_sysfs_store_fbo_app_preload(struct os44_fbo_lifetime *,const char *,size_t);
#endif
