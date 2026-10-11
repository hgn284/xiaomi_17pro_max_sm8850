/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_PIN_INFO_H
#define OS44_FBO_PIN_INFO_H
#include "os44_fbo_power.h"
int os44_ufsfbo_get_lr_pinned_info(struct os44_fbo_lifetime *,u8 *);
ssize_t os44_ufsfbo_sysfs_show_fbo3_get_lr_pinned_info(struct os44_fbo_lifetime *,char *);
#endif
