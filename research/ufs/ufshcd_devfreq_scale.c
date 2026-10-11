// SPDX-License-Identifier: GPL-2.0-only
/* OS44 reconstruction. Real providers; no fabricated owner or ABI metadata. */
#include <linux/clk.h>
#include <linux/blk-mq.h>
#include <linux/devfreq.h>
#include <linux/pm_opp.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "ufsswap_provider_reconstruction.h"
#include "os44_cld_drain.h"
#include "os44_reset_prereqs.h"
#include "os44_pm_helpers_contract.h"
#include "os44_pm_contract.h"
#include "os44_devfreq.h"
static void local_release(struct ufs_hba *hba)
{ guard(spinlock_irqsave)(&hba->clk_gating.lock); os44_core_release_locked(hba); }
static int ufshcd_clock_scaling_prepare(struct ufs_hba *hba, u64 timeout_us)
{
	int ret = 0;
	/*
	 * make sure that there are no outstanding requests when
	 * clock scaling is in progress
	 */
	mutex_lock(&hba->host->scan_mutex);

	blk_mq_quiesce_tagset(&hba->host->tag_set);
	mutex_lock(&hba->wb_mutex);
	down_write(&hba->clk_scaling_lock);

	if (!hba->clk_scaling.is_allowed ||
	    os44_ufshcd_wait_for_doorbell_clr(hba, timeout_us)) {
		ret = -EBUSY;
		up_write(&hba->clk_scaling_lock);
		mutex_unlock(&hba->wb_mutex);
		blk_mq_unquiesce_tagset(&hba->host->tag_set);
		mutex_unlock(&hba->host->scan_mutex);
		goto out;
	}

	/* let's not get into low power until clock scaling is completed */
	os44_core_hold(hba);

out:
	return ret;
}
static void ufshcd_clock_scaling_unprepare(struct ufs_hba *hba, int err)
{
	up_write(&hba->clk_scaling_lock);

	dev_info(hba->dev, "%s keep write booster always on\n", "ufshcd_clock_scaling_unprepare");

	mutex_unlock(&hba->wb_mutex);

	blk_mq_unquiesce_tagset(&hba->host->tag_set);
	mutex_unlock(&hba->host->scan_mutex);
	local_release(hba);
}
int os44_ufshcd_devfreq_scale(struct ufs_hba *hba, unsigned long freq,
				bool scale_up)
{
	u32 old_gear = hba->pwr_info.gear_rx;
	u32 new_gear = 0;
	int ret = 0;

	new_gear = ufshcd_vops_freq_to_gear_speed(hba, freq);

	ret = ufshcd_clock_scaling_prepare(hba, 1 * USEC_PER_SEC);
	if (ret)
		return ret;

	/* scale down the gear before scaling down clocks */
	if (!scale_up) {
		ret = os44_ufshcd_scale_gear(hba, new_gear, false);
		if (ret)
			goto out_unprepare;
	}

	ret = os44_ufshcd_scale_clks(hba, freq, scale_up);
	if (ret) {
		if (!scale_up)
			os44_ufshcd_scale_gear(hba, old_gear, true);
		goto out_unprepare;
	}

	/* scale up the gear after scaling up clocks */
	if (scale_up) {
		ret = os44_ufshcd_scale_gear(hba, new_gear, true);
		if (ret) {
			os44_ufshcd_scale_clks(hba, hba->devfreq->previous_freq,
					  false);
			goto out_unprepare;
		}
	}

out_unprepare:
	ufshcd_clock_scaling_unprepare(hba, ret);
	return ret;
}
