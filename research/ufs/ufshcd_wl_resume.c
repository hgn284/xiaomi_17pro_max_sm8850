/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole OS44 WL resume with original error logging and real providers. */
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_wlun_contract.h"
#include "os44_reset_recovery.h"
#include "os44_pm_helpers_contract.h"
#include "os44_pm_contract.h"
#include "os44_link_transition_contract.h"
#include "os44_bkops_contract.h"
#include "os44_hba_bkops_contract.h"
#include "os44_uic_contract.h"
#include "ufsswap_debug_reconstruction.h"
static inline int local_write_ee_control(struct ufs_hba *hba)
{
	int err;

	mutex_lock(&hba->ee_ctrl_mutex);
	err = __ufshcd_write_ee_control(hba, hba->ee_ctrl_mask);
	mutex_unlock(&hba->ee_ctrl_mutex);
	if (err)
		dev_err(hba->dev, "%s: failed to write ee control %d\n",
			"ufshcd_write_ee_control", err);
	return err;
}

extern void os44_core_release_locked(struct ufs_hba *);
static inline void local_release(struct ufs_hba *hba)
{
 guard(spinlock_irqsave)(&hba->clk_gating.lock);
 os44_core_release_locked(hba);
}

static void local_update_evt_hist(struct ufs_hba *hba, u32 id, u32 val)
{
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];
	e->val[e->pos] = val;
	e->tstamp[e->pos] = local_clock();
	e->cnt += 1;
	e->pos = (e->pos + 1) % UFS_EVENT_HIST_LENGTH;

	ufshcd_vops_event_notify(hba, id, &val);
}
static void ufshcd_clk_scaling_allow(struct ufs_hba *hba, bool allow)
{
	mutex_lock(&hba->wb_mutex);
	down_write(&hba->clk_scaling_lock);
	hba->clk_scaling.is_allowed = allow;
	up_write(&hba->clk_scaling_lock);
	mutex_unlock(&hba->wb_mutex);
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
static void ufshcd_clk_scaling_suspend(struct ufs_hba *hba, bool suspend)
{
	if (suspend) {
		if (hba->clk_scaling.is_enabled)
			os44_ufshcd_suspend_clkscaling(hba);
		ufshcd_clk_scaling_allow(hba, false);
	} else {
		ufshcd_clk_scaling_allow(hba, true);
		if (hba->clk_scaling.is_enabled)
			ufshcd_resume_clkscaling(hba);
	}
}
static void ufshcd_configure_auto_hibern8(struct ufs_hba *hba)
{
	if (!ufshcd_is_auto_hibern8_supported(hba))
		return;

	ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);
}
int os44_wl_resume_core(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int ret;
	enum uic_link_state old_link_state = hba->uic_link_state;

	hba->pm_op_in_progress = true;

	/*
	 * Call vendor specific resume callback. As these callbacks may access
	 * vendor specific host controller register space call them when the
	 * host clocks are ON.
	 */
	ret = ufshcd_vops_resume(hba, pm_op);
	if (ret)
		goto out;

	/* For DeepSleep, the only supported option is to have the link off */
	WARN_ON(ufshcd_is_ufs_dev_deepsleep(hba) && !ufshcd_is_link_off(hba));

	if (ufshcd_is_link_hibern8(hba)) {
		ret = os44_ufshcd_uic_hibern8_exit(hba);
		if (!ret) {
			ufshcd_set_link_active(hba);
		} else {
			dev_err(hba->dev, "%s: hibern8 exit failed %d\n",
					"__ufshcd_wl_resume", ret);
			/*
			 * If the h8 exit fails during the runtime resume
			 * process, it becomes stuck and cannot be recovered
			 * through the error handler. To fix this, use link
			 * recovery instead of the error handler.
			 */
			ret = os44_ufshcd_link_recovery(hba);
			if (ret)
				goto vendor_suspend;
		}
	} else if (ufshcd_is_link_off(hba)) {
		/*
		 * A full initialization of the host and the device is
		 * required since the link was put to off during suspend.
		 * Note, in the case of DeepSleep, the device will exit
		 * DeepSleep due to device reset.
		 */
		os44_query_error(0, 11, -1, 0);
		ret = os44_ufshcd_reset_and_restore(hba);
		/*
		 * os44_ufshcd_reset_and_restore() should have already
		 * set the link state as active
		 */
		if (ret || !ufshcd_is_link_active(hba))
			goto vendor_suspend;
	}

	if (!ufshcd_is_ufs_dev_active(hba)) {
		ret = os44_ufshcd_set_dev_pwr_mode(hba, UFS_ACTIVE_PWR_MODE);
		if (ret)
			goto set_old_link_state;
		os44_ufshcd_set_timestamp_attr(hba);
		schedule_delayed_work(&hba->ufs_rtc_update_work,
				      msecs_to_jiffies(10000));
	}

	if (ufshcd_keep_autobkops_enabled_except_suspend(hba))
		os44_ufshcd_enable_auto_bkops(hba);
	else
		/*
		 * If BKOPs operations are urgently needed at this moment then
		 * keep auto-bkops enabled or else disable it.
		 */
		os44_ufshcd_bkops_ctrl(hba);

	if (hba->ee_usr_mask)
		local_write_ee_control(hba);

	if (ufshcd_is_clkscaling_supported(hba))
		ufshcd_clk_scaling_suspend(hba, false);

	if (hba->dev_info.b_rpm_dev_flush_capable) {
		hba->dev_info.b_rpm_dev_flush_capable = false;
		cancel_delayed_work(&hba->rpm_dev_flush_recheck_work);
	}

	ufshcd_configure_auto_hibern8(hba);

	goto out;

set_old_link_state:
	os44_ufshcd_link_state_transition(hba, old_link_state, 0);
vendor_suspend:
	ufshcd_vops_suspend(hba, pm_op, PRE_CHANGE);
	ufshcd_vops_suspend(hba, pm_op, POST_CHANGE);
out:
	if (ret) {
		os44_query_error(0, 13, -1, 0);
		local_update_evt_hist(hba, UFS_EVT_WL_RES_ERR, (u32)ret);
	}
	hba->clk_gating.is_suspended = false;
	local_release(hba);
	hba->pm_op_in_progress = false;
	return ret;
}
