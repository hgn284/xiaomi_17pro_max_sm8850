/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLOCK_GATING_H
#define OS44_CLOCK_GATING_H
#include <ufs/ufshcd.h>
void os44_core_release_locked(struct ufs_hba *);
void os44_ufshcd_hba_vreg_set_hpm(struct ufs_hba *);
void os44_ufshcd_gate_work(struct work_struct *);
void os44_ufshcd_ungate_work(struct work_struct *);
ssize_t os44_ufshcd_clkgate_delay_show(struct device *,struct device_attribute *,char *);
ssize_t os44_ufshcd_clkgate_delay_store(struct device *,struct device_attribute *,const char *,size_t);
ssize_t os44_ufshcd_clkgate_enable_show(struct device *,struct device_attribute *,char *);
ssize_t os44_ufshcd_clkgate_enable_store(struct device *,struct device_attribute *,const char *,size_t);
#endif
