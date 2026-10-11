/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_HBA_ENABLE_CONTRACT_H
#define OS44_HBA_ENABLE_CONTRACT_H
#include <ufs/ufshcd.h>
int os44_ufshcd_hba_enable(struct ufs_hba *);
bool os44_ufshcd_is_hba_active(struct ufs_hba *);
void os44_ufshcd_delay_us(unsigned long, unsigned long);
bool os44_ufshcd_crypto_enable(struct ufs_hba *);
#endif
