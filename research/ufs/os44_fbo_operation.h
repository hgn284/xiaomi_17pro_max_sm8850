/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_OPERATION_H
#define OS44_FBO_OPERATION_H
#include "os44_fbo_power.h"
int os44_ufsfbo_operation_control(struct os44_fbo_lifetime *,int *);
ssize_t os44_ufsfbo_sysfs_store_fbo_operation_control(struct os44_fbo_lifetime *,const char *,size_t);
#endif
