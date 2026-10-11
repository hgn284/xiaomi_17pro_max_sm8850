/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_PM_HELPERS_CONTRACT_H
#define OS44_PM_HELPERS_CONTRACT_H
#include <ufs/ufshcd.h>
void os44_ufshcd_suspend_clkscaling(struct ufs_hba *);
void os44_ufshcd_device_reset(struct ufs_hba *);
int os44_ufshcd_set_dev_pwr_mode(struct ufs_hba *, enum ufs_dev_pwr_mode);
#endif
