// SPDX-License-Identifier: GPL-2.0-only
#include <linux/ktime.h>
#include <linux/pm_runtime.h>
#include "os44_resume_pm.h"
#include "os44_cld_view.h"
#include "os44_wlun_contract.h"
#define CREATE_TRACE_POINTS
#include "os44_resume_trace.h"
int os44_ufshcd_system_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	ktime_t start = ktime_get();
	int ret = 0;

	if (pm_runtime_suspended(hba->dev))
		goto out;

	ret = os44_ufshcd_resume_core(hba);
	if (!ret && os44_ufscld_get_state(hba)==2)
		os44_ufscld_resume(hba);

out:
	trace_ufshcd_system_resume(hba, ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);

	return ret;
}

int os44_ufshcd_runtime_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	int ret;
	ktime_t start = ktime_get();

	ret = os44_ufshcd_resume_core(hba);

	trace_ufshcd_runtime_resume(hba, ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	return ret;
}
