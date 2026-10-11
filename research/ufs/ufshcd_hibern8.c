/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction: stock category4 error; stock-owned tracepoint. */
#include <linux/module.h>
#include <linux/sched/clock.h>
#include "ufsswap_provider_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
#include "os44_uic_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#define CREATE_TRACE_POINTS
#include "os44_hibern8_trace.h"
int os44_ufshcd_uic_hibern8_exit(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {
		.command = UIC_CMD_DME_HIBER_EXIT,
	};
	int ret;
	ktime_t start = ktime_get();

	ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_EXIT, PRE_CHANGE);

	ret = os44_ufshcd_uic_pwr_ctrl(hba, &uic_cmd);
	trace_ufshcd_profile_hibern8(hba, "exit",
			     ktime_to_us(ktime_sub(ktime_get(), start)), ret);

	if (ret) {
		os44_query_error(0, 4, -1, 0);
		dev_err(hba->dev, "%s: hibern8 exit failed. ret = %d\n",
			"ufshcd_uic_hibern8_exit", ret);
	} else {
		ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_EXIT,
								POST_CHANGE);
		hba->ufs_stats.last_hibern8_exit_tstamp = local_clock();
		hba->ufs_stats.hibern8_exit_cnt++;
	}

	return ret;
}

int os44_ufshcd_uic_hibern8_enter(struct ufs_hba *hba)
{
 struct uic_command cmd = { .command = UIC_CMD_DME_HIBER_ENTER };
 int ret;
 ktime_t start = ktime_get();
 ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_ENTER, PRE_CHANGE);
 ret = os44_ufshcd_uic_pwr_ctrl(hba, &cmd);
 trace_ufshcd_profile_hibern8(hba, "enter",
  ktime_to_us(ktime_sub(ktime_get(), start)), ret);
 if (ret) {
  os44_query_error(0, 3, -1, 0);
  dev_err(hba->dev, "%s: hibern8 enter failed. ret = %d\n",
   "ufshcd_uic_hibern8_enter", ret);
 } else {
  ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_ENTER, POST_CHANGE);
  if (os44_ufs_data.last_hibern8_enter) {
   u64 gap = start - os44_ufs_data.last_hibern8_enter;
   os44_ufs_data.max_hibern8_gap = max(gap, os44_ufs_data.max_hibern8_gap);
   os44_ufs_data.max_hibern8_gap_alltime = max(gap, os44_ufs_data.max_hibern8_gap_alltime);
  }
  os44_ufs_data.last_hibern8_enter = start;
 }
 return ret;
}
