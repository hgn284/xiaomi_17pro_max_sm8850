/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_FRAG_H
#define OS44_FBO_FRAG_H
#include "os44_fbo_power.h"
int os44_ufsfbo_read_frag_level(struct os44_fbo_lifetime *,u8 *);
ssize_t os44_ufsfbo_sysfs_show_fbo_get_lr_frag_level(struct os44_fbo_lifetime *,char *);
#endif
