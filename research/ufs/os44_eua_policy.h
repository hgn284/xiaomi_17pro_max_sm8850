/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstruction-owned names. Stock table identity: section9+0x6eb0, not other local policy symbols. */
#ifndef OS44_EUA_POLICY_H
#define OS44_EUA_POLICY_H
#include "os44_eua.h"
struct os44_eua_policy_record { char vendor[9],model[17],revision[5];int support_size;const struct os44_eua_ops *ops; };
struct os44_eua_check_record { u32 platform;char model[20];u8 hide_size; };
static_assert(sizeof(struct os44_eua_policy_record)==48);
static_assert(offsetof(struct os44_eua_policy_record,support_size)==32);
static_assert(offsetof(struct os44_eua_policy_record,ops)==40);
static_assert(sizeof(struct os44_eua_check_record)==28);
extern struct os44_eua_policy_record os44_eua_policy[12];
extern struct os44_eua_check_record os44_eua_checklist[3];
extern const struct os44_eua_ops os44_hynix_eua_ops;
extern const struct os44_eua_ops os44_samsung_eua_ops;
extern const struct os44_eua_ops os44_micron_eua_ops;
extern const struct os44_eua_ops os44_ymtc_eua_ops;
#endif
