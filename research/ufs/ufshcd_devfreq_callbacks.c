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
#include "os44_devfreq_trace.h"
#define ufshcd_eh_in_progress(h) ((h)->eh_flags & 1)
static void local_release(struct ufs_hba *hba)
{ guard(spinlock_irqsave)(&hba->clk_gating.lock); os44_core_release_locked(hba); }
static bool ufshcd_is_devfreq_scaling_required(struct ufs_hba *hba,
					       unsigned long freq, bool scale_up)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		return false;

	if (hba->use_pm_opp)
		return freq != hba->clk_scaling.target_freq;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (scale_up && clki->max_freq) {
				if (clki->curr_freq == clki->max_freq)
					continue;
				return true;
			} else if (!scale_up && clki->min_freq) {
				if (clki->curr_freq == clki->min_freq)
					continue;
				return true;
			}
		}
	}

	return false;
}
static void ufshcd_resume_clkscaling(struct ufs_hba *hba)
{
	unsigned long flags;
	bool resume = false;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_scaling.is_suspended) {
		resume = true;
		hba->clk_scaling.is_suspended = false;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (resume)
		devfreq_resume_device(hba->devfreq);
}
int os44_ufshcd_devfreq_target(struct device *dev,
				unsigned long *freq, u32 flags)
{
	int ret = 0;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	ktime_t start;
	bool scale_up = false, sched_clk_scaling_suspend_work = false;
	struct list_head *clk_list = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	unsigned long irq_flags;

	if (!ufshcd_is_clkscaling_supported(hba))
		return -EINVAL;

	if (hba->use_pm_opp) {
		struct dev_pm_opp *opp;

		/* Get the recommended frequency from OPP framework */
		opp = devfreq_recommended_opp(dev, freq, flags);
		if (IS_ERR(opp))
			return PTR_ERR(opp);

		dev_pm_opp_put(opp);
	} else {
		/* Override with the closest supported frequency */
		clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info,
					list);
		*freq =	(unsigned long) clk_round_rate(clki->clk, *freq);
	}

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (ufshcd_eh_in_progress(hba)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return 0;
	}

	/* Skip scaling clock when clock scaling is suspended */
	if (hba->clk_scaling.is_suspended) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		dev_warn(hba->dev, "clock scaling is suspended, skip");
		return 0;
	}

	if (!hba->clk_scaling.active_reqs)
		sched_clk_scaling_suspend_work = true;

	if (list_empty(clk_list)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		goto out;
	}

	/* Decide based on the target or rounded-off frequency and update */
	if (hba->use_pm_opp)
		scale_up = *freq > hba->clk_scaling.target_freq;
	else
		scale_up = *freq == clki->max_freq;

	if (!hba->use_pm_opp && !scale_up)
		*freq = clki->min_freq;

	/* Update the frequency */
	if (!ufshcd_is_devfreq_scaling_required(hba, *freq, scale_up)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		ret = 0;
		goto out; /* no state change required */
	}
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	start = ktime_get();
	ret = os44_ufshcd_devfreq_scale(hba, *freq, scale_up);
	if (!ret)
		hba->clk_scaling.target_freq = *freq;

	local_profile_clk_scaling(hba,
		(scale_up ? "up" : "down"),
		ktime_to_us(ktime_sub(ktime_get(), start)), ret);

out:
	if (sched_clk_scaling_suspend_work &&
			(!scale_up || hba->clk_scaling.suspend_on_no_request))
		queue_work(hba->clk_scaling.workq,
			   &hba->clk_scaling.suspend_work);

	return ret;
}

int os44_ufshcd_devfreq_get_dev_status(struct device *dev,
		struct devfreq_dev_status *stat)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_clk_scaling *scaling = &hba->clk_scaling;
	unsigned long flags;
	ktime_t curr_t;

	if (!ufshcd_is_clkscaling_supported(hba))
		return -EINVAL;

	memset(stat, 0, sizeof(*stat));

	spin_lock_irqsave(hba->host->host_lock, flags);
	curr_t = ktime_get();
	if (!scaling->window_start_t)
		goto start_window;

	/*
	 * If current frequency is 0, then the ondemand governor considers
	 * there's no initial frequency set. And it always requests to set
	 * to max. frequency.
	 */
	if (hba->use_pm_opp) {
		stat->current_frequency = hba->clk_scaling.target_freq;
	} else {
		struct list_head *clk_list = &hba->clk_list_head;
		struct ufs_clk_info *clki;

		clki = list_first_entry(clk_list, struct ufs_clk_info, list);
		stat->current_frequency = clki->curr_freq;
	}

	if (scaling->is_busy_started)
		scaling->tot_busy_t += ktime_us_delta(curr_t,
				scaling->busy_start_t);
	stat->total_time = ktime_us_delta(curr_t, scaling->window_start_t);
	stat->busy_time = scaling->tot_busy_t;
start_window:
	scaling->window_start_t = curr_t;
	scaling->tot_busy_t = 0;

	if (scaling->active_reqs) {
		scaling->busy_start_t = curr_t;
		scaling->is_busy_started = true;
	} else {
		scaling->busy_start_t = 0;
		scaling->is_busy_started = false;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return 0;
}

ssize_t os44_ufshcd_clkscale_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", hba->clk_scaling.is_enabled);
}

ssize_t os44_ufshcd_clkscale_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_clk_info *clki;
	unsigned long freq;
	u32 value;
	int err = 0;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	down(&hba->host_sem);
	if (!ufshcd_is_user_access_allowed(hba)) {
		err = -EBUSY;
		goto out;
	}

	value = !!value;
	if (value == hba->clk_scaling.is_enabled)
		goto out;

	ufshcd_rpm_get_sync(hba);
	os44_core_hold(hba);

	hba->clk_scaling.is_enabled = value;

	if (value) {
		ufshcd_resume_clkscaling(hba);
		goto out_rel;
	}

	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);
	freq = clki->max_freq;

	os44_ufshcd_suspend_clkscaling(hba);

	if (!ufshcd_is_devfreq_scaling_required(hba, freq, true))
		goto out_rel;

	err = os44_ufshcd_devfreq_scale(hba, freq, true);
	if (err)
		dev_err(hba->dev, "%s: failed to scale clocks up %d\n",
				"ufshcd_clkscale_enable_store", err);
	else
		hba->clk_scaling.target_freq = freq;

out_rel:
	local_release(hba);
	ufshcd_rpm_put_sync(hba);
out:
	up(&hba->host_sem);
	return err ? err : count;
}

struct ufs_hba_variant_params os44_ufs_hba_vps = {
	.hba_enable_delay_us		= 1000,
	.wb_flush_threshold		= UFS_WB_BUF_REMAIN_PERCENT(40),
	.devfreq_profile.polling_ms	= 100,
	.devfreq_profile.target		= os44_ufshcd_devfreq_target,
	.devfreq_profile.get_dev_status	= os44_ufshcd_devfreq_get_dev_status,
	.ondemand_data.upthreshold	= 70,
	.ondemand_data.downdifferential	= 5,
};
