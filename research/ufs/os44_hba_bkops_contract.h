/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_HBA_BKOPS_CONTRACT_H
#define OS44_HBA_BKOPS_CONTRACT_H
#include <ufs/ufshcd.h>
void os44_ufshcd_hba_stop(struct ufs_hba *);
int os44_ufshcd_bkops_ctrl(struct ufs_hba *);
#endif
