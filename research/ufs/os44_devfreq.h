/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_DEVFREQ_H
#define OS44_DEVFREQ_H
#include <linux/devfreq.h>
#include <ufs/ufshcd.h>
int os44_ufshcd_scale_gear(struct ufs_hba *, u32, bool);
int os44_ufshcd_devfreq_scale(struct ufs_hba *, unsigned long, bool);
int os44_ufshcd_devfreq_target(struct device *, unsigned long *, u32);
int os44_ufshcd_devfreq_get_dev_status(struct device *, struct devfreq_dev_status *);
ssize_t os44_ufshcd_clkscale_enable_show(struct device *, struct device_attribute *, char *);
ssize_t os44_ufshcd_clkscale_enable_store(struct device *, struct device_attribute *, const char *, size_t);
extern struct ufs_hba_variant_params os44_ufs_hba_vps;
#endif
