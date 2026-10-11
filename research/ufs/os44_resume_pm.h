/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_RESUME_PM_H
#define OS44_RESUME_PM_H
#include <ufs/ufshcd.h>
int os44_ufshcd_resume_core(struct ufs_hba *hba);
int os44_ufshcd_runtime_resume(struct device *dev);
int os44_ufshcd_system_resume(struct device *dev);
int os44_ufshcd_setup_vreg(struct ufs_hba *hba, bool on);
#endif
