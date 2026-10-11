/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_RESET_RECOVERY_H
#define OS44_RESET_RECOVERY_H
#include <ufs/ufshcd.h>
int os44_ufshcd_reset_and_restore(struct ufs_hba *);
int os44_ufshcd_link_recovery(struct ufs_hba *);
void os44_ufshcd_force_error_recovery(struct ufs_hba *);
#endif
