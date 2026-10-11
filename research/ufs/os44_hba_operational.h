/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_HBA_OPERATIONAL_H
#define OS44_HBA_OPERATIONAL_H
#include <ufs/ufshcd.h>
int os44_ufshcd_make_hba_operational(struct ufs_hba *);
void os44_ufshcd_mcq_enable(struct ufs_hba *);
#endif
