/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_SYSTEM_PM_H
#define OS44_SYSTEM_PM_H
#include <linux/device.h>
int os44_ufshcd_system_suspend(struct device *dev);
int os44_ufshcd_system_freeze(struct device *dev);
#endif
