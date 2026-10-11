/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_WORK_H
#define OS44_CLD_WORK_H
#include "os44_cld_init.h"
#include "os44_cld_ops.h"
int os44_ufscld_hold_runtime_pm(struct os44_cld_view *);
int os44_ufscld_get_operation_status(struct os44_cld_view *, u32 *);
int os44_ufscld_trigger_off(struct os44_cld_view *);
#endif
