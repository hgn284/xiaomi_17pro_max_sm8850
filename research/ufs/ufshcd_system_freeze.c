// SPDX-License-Identifier: GPL-2.0-only
#include "os44_system_pm.h"
int os44_ufshcd_system_freeze(struct device *dev)
{
 return os44_ufshcd_system_suspend(dev);
}
