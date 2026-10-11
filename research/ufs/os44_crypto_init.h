/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CRYPTO_INIT_H
#define OS44_CRYPTO_INIT_H
#include <ufs/ufshcd.h>
int os44_ufshcd_hba_init_crypto_capabilities(struct ufs_hba *);
void os44_ufshcd_init_crypto(struct ufs_hba *);
void os44_ufshcd_pm_qos_init(struct ufs_hba *);
#endif
