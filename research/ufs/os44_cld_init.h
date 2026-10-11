/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_INIT_H
#define OS44_CLD_INIT_H
#include "os44_cld_view.h"
/* Real missing providers, not stubs; source-pair declarations. */
int os44_ufscld_init_ops(struct os44_cld_view *);
int os44_ufscld_create_sysfs(struct os44_cld_view *);
void os44_ufscld_trigger_work_fn(struct work_struct *);
void os44_ufscld_init(struct ufs_hba *);
#endif
