/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO3_PIN_WRITE_H
#define OS44_FBO3_PIN_WRITE_H
#include "os44_fbo_write.h"
int os44_ufsfbo3_ss_lba_list_write(struct os44_fbo_lifetime *,const char *);
ssize_t os44_ufsfbo_sysfs_store_fbo3_ss_send_pin_lr_list(struct os44_fbo_lifetime *,const char *,size_t);
#endif
