/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_MICRON_H
#define OS44_CLD_MICRON_H
#include "os44_cld_ops.h"
int os44_micron_init_idn(struct os44_cld_view *);
int os44_micron_get_frag_level(struct os44_cld_view *, u32 *);
int os44_micron_cld_set_trigger(struct os44_cld_view *, unsigned int);
int os44_micron_cld_get_trigger(struct os44_cld_view *, bool *);
int os44_micron_cld_operation_status(struct os44_cld_view *, u32 *);
extern const struct os44_cld_ops os44_micron_cld_ops;
#endif
