/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_HYNIX_H
#define OS44_CLD_HYNIX_H
#include "os44_cld_ops.h"
/* Source-pair types, not recovered stock private KCFI. All output is u32. */
int os44_sk_get_frag_level(struct os44_cld_view *, u32 *);
int os44_sk_cld_set_trigger(struct os44_cld_view *, unsigned int);
int os44_sk_cld_get_trigger(struct os44_cld_view *, u32 *);
int os44_sk_cld_operation_status(struct os44_cld_view *, u32 *);
extern const struct os44_cld_ops os44_hynix_cld_ops;
#endif
