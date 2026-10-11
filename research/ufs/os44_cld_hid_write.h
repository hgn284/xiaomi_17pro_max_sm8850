/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_HID_WRITE_H
#define OS44_CLD_HID_WRITE_H
#include "os44_cld_hid.h"
#include "os44_cld_ops.h"
int os44_ufshid_defrag(struct os44_cld_view *,int);
int os44_ufshid_disable(struct os44_cld_view *);
int os44_ufshid_check_progress(struct os44_cld_view *,int);
int os44_hid_set_trigger(struct os44_cld_view *,u32);
extern const struct os44_cld_ops os44_hid_ops;
#endif
