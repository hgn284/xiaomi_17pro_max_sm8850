/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_PIN_CHECK_H
#define OS44_FBO_PIN_CHECK_H
#include "os44_fbo_write.h"
int os44_ufsfbo_check_lba_pin(struct os44_fbo_lifetime *,const char *);
ssize_t os44_ufsfbo_sysfs_store_fbo3_send_lr_list(struct os44_fbo_lifetime *,const char *,size_t);
#endif
