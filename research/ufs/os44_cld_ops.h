/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstruction-owned signatures, NOT original private type identities. */
#ifndef OS44_CLD_OPS_H
#define OS44_CLD_OPS_H
#include "os44_cld_view.h"
struct os44_cld_ops {
 int (*get_frag_level)(struct os44_cld_view *, u32 *);
 int (*set_trigger)(struct os44_cld_view *, unsigned int);
 union {
  int (*boolean)(struct os44_cld_view *, bool *);
  int (*word)(struct os44_cld_view *, u32 *);
 } get_trigger;
 int (*operation_status)(struct os44_cld_view *, u32 *);
};
struct os44_cld_vendor {
 char vendor[8];
 u8 auto_hibern8;
 const struct os44_cld_ops *ops;
};
static_assert(sizeof(struct os44_cld_ops)==32);
static_assert(offsetof(struct os44_cld_ops,set_trigger)==8);
static_assert(offsetof(struct os44_cld_ops,get_trigger)==16);
static_assert(offsetof(struct os44_cld_ops,operation_status)==24);
static_assert(sizeof(struct os44_cld_vendor)==24);
static_assert(offsetof(struct os44_cld_vendor,auto_hibern8)==8);
static_assert(offsetof(struct os44_cld_vendor,ops)==16);
extern const struct os44_cld_ops os44_wdc_cld_ops;
#endif
