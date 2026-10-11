/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_HID_H
#define OS44_CLD_HID_H
#include "os44_cld_view.h"
int os44_ufshid_get_state(struct os44_cld_view *, u32 *);
int os44_ufshid_analyze(struct os44_cld_view *);
int os44_ufshid_set_defragsize(struct os44_cld_view *);
int os44_hid_get_frag_level(struct os44_cld_view *, u32 *);
int os44_hid_get_trigger(struct os44_cld_view *, u32 *);
int os44_hid_operation_status(struct os44_cld_view *, u32 *);
#endif
