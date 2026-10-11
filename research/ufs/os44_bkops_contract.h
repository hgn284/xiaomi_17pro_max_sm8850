/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_BKOPS_CONTRACT_H
#define OS44_BKOPS_CONTRACT_H
#include <ufs/ufshcd.h>
int os44_ufshcd_query_flag(struct ufs_hba *, enum query_opcode, enum flag_idn, u8, bool *);
int os44_ufshcd_query_flag_retry(struct ufs_hba *, enum query_opcode, enum flag_idn, u8, bool *);
int os44_ufshcd_enable_auto_bkops(struct ufs_hba *);
int os44_ufshcd_disable_auto_bkops(struct ufs_hba *);
#endif
