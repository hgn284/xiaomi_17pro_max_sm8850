/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_WRITE_H
#define OS44_FBO_WRITE_H
#include "os44_fbo_preload.h"
#include "os44_fbo_status.h"
#include "os44_fbo_progress.h"
int os44_ufsfbo_lba_list_write(struct os44_fbo_lifetime *,const char *);
ssize_t os44_ufsfbo_sysfs_store_fbo_send_lr_list(struct os44_fbo_lifetime *,const char *,size_t);
#endif
