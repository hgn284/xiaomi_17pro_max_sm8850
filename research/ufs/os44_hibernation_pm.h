/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_HIBERNATION_PM_H
#define OS44_HIBERNATION_PM_H
#include <linux/device.h>
int os44_ufshcd_system_restore(struct device *dev);
int os44_ufshcd_system_thaw(struct device *dev);
#endif
