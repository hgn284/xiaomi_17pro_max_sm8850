/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_PM_CONTRACT_H
#define OS44_PM_CONTRACT_H
struct ufs_hba;
int os44_ufshcd_setup_clocks(struct ufs_hba *, bool);
void os44_ufshcd_hba_vreg_set_lpm(struct ufs_hba *);
void os44_ufshcd_release(struct ufs_hba *);
void os44_ufshcd_set_timestamp_attr(struct ufs_hba *);
#endif
