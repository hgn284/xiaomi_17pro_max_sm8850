/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_KIOXIA_H
#define OS44_CLD_KIOXIA_H
#include "os44_cld_ops.h"
/* A u32 output object is required: query writes its low byte; failure log
 * reads all four bytes. This is not a bool-output callback contract. */
int os44_kioxia_init_idn(struct os44_cld_view *);
int os44_kioxia_get_frag_level(struct os44_cld_view *, u32 *);
int os44_kioxia_cld_set_trigger(struct os44_cld_view *, unsigned int);
int os44_kioxia_cld_get_trigger(struct os44_cld_view *, u32 *);
int os44_kioxia_cld_operation_status(struct os44_cld_view *, u32 *);
extern const struct os44_cld_ops os44_kioxia_cld_ops;
#endif
