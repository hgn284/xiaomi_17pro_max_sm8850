/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstruction-owned typed contract; compiler KCFI compared separately. */
#ifndef OS44_DESCRIPTOR_OWNER_H
#define OS44_DESCRIPTOR_OWNER_H
#include <ufs/ufshcd.h>
int os44_mi_ufshcd_read_desc_param(struct ufs_hba *, enum desc_idn, int, u8, u8 *, u8);
#endif
