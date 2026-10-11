/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_WLUN_CONTRACT_H
#define OS44_WLUN_CONTRACT_H
struct ufs_hba;
int os44_wl_suspend_core(struct ufs_hba *, enum ufs_pm_op);
int os44_wl_resume_core(struct ufs_hba *, enum ufs_pm_op);
int os44_ufshcd_suspend_core(struct ufs_hba *);
int os44_ufscld_get_state(struct ufs_hba *);
void os44_ufscld_suspend(struct ufs_hba *);
#endif
