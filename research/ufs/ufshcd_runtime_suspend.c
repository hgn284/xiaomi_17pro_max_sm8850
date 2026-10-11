// SPDX-License-Identifier: GPL-2.0-only
/* Whole stock runtime suspend; wrap existing real reconstructed suspend core. */
#include <linux/ktime.h>
#include <ufs/ufshcd.h>
#include "os44_wlun_contract.h"
#include "ufsswap_debug_reconstruction.h"
#define CREATE_TRACE_POINTS
#include "os44_runtime_trace.h"
int os44_ufshcd_runtime_suspend(struct device *dev)
{
 struct ufs_hba *hba=dev_get_drvdata(dev);
 ktime_t start=ktime_get();
 int ret=os44_ufshcd_suspend_core(hba);
 if (!ret) {
  if (os44_ufs_data.last_runtime_suspend) {
   u64 gap=start-os44_ufs_data.last_runtime_suspend;
   os44_ufs_data.max_runtime_suspend_gap=max(gap,os44_ufs_data.max_runtime_suspend_gap);
   os44_ufs_data.max_runtime_suspend_gap_alltime=max(gap,os44_ufs_data.max_runtime_suspend_gap_alltime);
  }
  os44_ufs_data.last_runtime_suspend=start;
 }
 trace_ufshcd_runtime_suspend(hba,ret,ktime_to_us(ktime_sub(ktime_get(),start)),hba->curr_dev_pwr_mode,hba->uic_link_state);
 return ret;
}
static_assert(offsetof(struct device,driver_data)==0x98);
