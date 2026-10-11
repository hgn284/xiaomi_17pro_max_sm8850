// SPDX-License-Identifier: GPL-2.0-only
/* Whole stock system-suspend wrapper including Xiaomi CLD failure rollback.
 * Real providers are source-paired; not a declaration of hardware acceptance. */
#include <linux/ktime.h>
#include <linux/pm_runtime.h>
#include <ufs/ufshcd.h>
#include "os44_wlun_contract.h"
#include "os44_cld_view.h"
#include "os44_system_pm.h"
#define CREATE_TRACE_POINTS
#include "os44_system_pm_trace.h"
int os44_ufshcd_system_suspend(struct device *dev)
{
 struct ufs_hba *hba=dev_get_drvdata(dev);
 int ret=0;
 ktime_t start=ktime_get();
 if(pm_runtime_suspended(hba->dev)) goto out;
 if(os44_ufscld_get_state(hba)==1) os44_ufscld_suspend(hba);
 ret=os44_ufshcd_suspend_core(hba);
 if(ret && os44_ufscld_get_state(hba)==2) os44_ufscld_resume(hba);
out:
 trace_ufshcd_system_suspend(hba,ret,ktime_to_us(ktime_sub(ktime_get(),start)),hba->curr_dev_pwr_mode,hba->uic_link_state);
 return ret;
}
static_assert(offsetof(struct device,driver_data)==0x98);
static_assert(offsetof(struct ufs_hba,dev)==0x40);
static_assert(offsetof(struct device,power.runtime_status)==0x1f4);
