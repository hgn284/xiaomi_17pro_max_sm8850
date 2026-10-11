/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_SAMSUNG_H
#define OS44_CLD_SAMSUNG_H
#include "os44_cld_ops.h"
/* Source-pair contract: original get_trigger queries and reads FOUR bytes.
 * No bool-pointer cast or generic ops table assignment is permitted. */
int os44_Samsung_init_idn(struct os44_cld_view *);
int os44_samsung_cld_set_trigger(struct os44_cld_view *, unsigned int);
int os44_samsung_cld_get_trigger(struct os44_cld_view *, u32 *);
int os44_samsung_get_frag_level(struct os44_cld_view *, u32 *);
int os44_samsung_cld_operation_status(struct os44_cld_view *, u32 *);
extern const struct os44_cld_ops os44_samsung_cld_ops;
#endif
