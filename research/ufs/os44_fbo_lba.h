/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_LBA_H
#define OS44_FBO_LBA_H
#include "os44_fbo_lifetime.h"
int os44_ufsfbo_check_lr_list_buf(struct os44_fbo_lifetime *, const char *);
int os44_ufsfbo_init_lba_buffer(struct os44_fbo_lifetime *, const char *, u8 *, bool, bool);
#endif
