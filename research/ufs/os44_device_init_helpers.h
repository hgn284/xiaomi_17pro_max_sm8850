/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_DEVICE_INIT_HELPERS_H
#define OS44_DEVICE_INIT_HELPERS_H
#include <ufs/ufshcd.h>
void os44_ufshcd_set_dev_ref_clk(struct ufs_hba *);
void os44_ufshcd_force_reset_auto_bkops(struct ufs_hba *);
int os44_ufshcd_verify_dev_init(struct ufs_hba *);
int local_attr_retry(struct ufs_hba *, enum query_opcode, enum attr_idn, u8, u8, u32 *);
#endif
