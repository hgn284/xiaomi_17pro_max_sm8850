/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_POWER_H
#define OS44_FBO_POWER_H
#include "os44_fbo_lifetime.h"
/* Source-owned contract; no recovered original private type identity. */
int os44_ufsfbo_is_not_present(struct os44_fbo_lifetime *);
void os44_ufsfbo_block_enter_suspend(struct os44_fbo_lifetime *);
void os44_ufsfbo_auto_hibern8_enable(struct os44_fbo_lifetime *,unsigned int);
int os44_ufsfbs_frag_level_check_enable(struct os44_fbo_lifetime *,bool);
int os44_ufsfbs_defrag_enable(struct os44_fbo_lifetime *,bool);
#endif
