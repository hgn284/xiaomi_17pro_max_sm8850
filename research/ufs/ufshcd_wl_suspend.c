/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole OS44 WL suspend with original error logging and real providers. */
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
#include "os44_reset_prereqs.h"
#include "os44_wb_contract.h"
#include "ufsswap_provider_reconstruction.h"
const struct ufs_pm_lvl_states os44_pm_lvl_states[] = {
	[UFS_PM_LVL_0] = {UFS_ACTIVE_PWR_MODE, UIC_LINK_ACTIVE_STATE},
	[UFS_PM_LVL_1] = {UFS_ACTIVE_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	[UFS_PM_LVL_2] = {UFS_SLEEP_PWR_MODE, UIC_LINK_ACTIVE_STATE},
	[UFS_PM_LVL_3] = {UFS_SLEEP_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	[UFS_PM_LVL_4] = {UFS_POWERDOWN_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	[UFS_PM_LVL_5] = {UFS_POWERDOWN_PWR_MODE, UIC_LINK_OFF_STATE},
	/*
	 * For DeepSleep, the link is first put in hibern8 and then off.
	 * Leaving the link in hibern8 is not supported.
	 */
	[UFS_PM_LVL_6] = {UFS_DEEPSLEEP_PWR_MODE, UIC_LINK_OFF_STATE},
};

static inline enum ufs_dev_pwr_mode
ufs_get_pm_lvl_to_dev_pwr_mode(enum ufs_pm_level lvl)
{
	return os44_pm_lvl_states[lvl].dev_state;
}

static inline enum uic_link_state
ufs_get_pm_lvl_to_link_pwr_state(enum ufs_pm_level lvl)
{
	return os44_pm_lvl_states[lvl].link_state;
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
static inline void local_force_error_recovery(struct ufs_hba *hba)
{
	spin_lock_irq(hba->host->host_lock);
	hba->force_reset = true;
	/* force_reset is already true under the same lock: original inline path. */
 if (hba->ufshcd_state != UFSHCD_STATE_ERROR) {
  hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;
  queue_work(hba->eh_wq, &hba->eh_work);
 }
	spin_unlock_irq(hba->host->host_lock);
}
int os44_wl_suspend_core(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int ret = 0;
	bool check_for_bkops;
	enum ufs_pm_level pm_lvl;
	enum ufs_dev_pwr_mode req_dev_pwr_mode;
	enum uic_link_state req_link_state;

	hba->pm_op_in_progress = true;
	if (pm_op != UFS_SHUTDOWN_PM) {
		pm_lvl = pm_op == UFS_RUNTIME_PM ?
			 hba->rpm_lvl : hba->spm_lvl;
		req_dev_pwr_mode = ufs_get_pm_lvl_to_dev_pwr_mode(pm_lvl);
		req_link_state = ufs_get_pm_lvl_to_link_pwr_state(pm_lvl);
	} else {
		req_dev_pwr_mode = UFS_POWERDOWN_PWR_MODE;
		req_link_state = UIC_LINK_OFF_STATE;
	}

	/*
	 * If we can't transition into any of the low power modes
	 * just gate the clocks.
	 */
	os44_core_hold(hba);
	hba->clk_gating.is_suspended = true;

	if (ufshcd_is_clkscaling_supported(hba))
		ufshcd_clk_scaling_suspend(hba, true);

	if (req_dev_pwr_mode == UFS_ACTIVE_PWR_MODE &&
			req_link_state == UIC_LINK_ACTIVE_STATE) {
		os44_ufshcd_disable_auto_bkops(hba);
		flush_work(&hba->eeh_work);
		goto vops_suspend;
	}

	if ((req_dev_pwr_mode == hba->curr_dev_pwr_mode) &&
	    (req_link_state == hba->uic_link_state))
		goto enable_scaling;

	/* UFS device & link must be active before we enter in this function */
	if (!ufshcd_is_ufs_dev_active(hba) || !ufshcd_is_link_active(hba)) {
		/*  Wait err handler finish or trigger err recovery */
		if (!(hba->eh_flags & 1))
			local_force_error_recovery(hba);
		ret = -EBUSY;
		goto enable_scaling;
	}

	if (pm_op == UFS_RUNTIME_PM) {
		if (ufshcd_can_autobkops_during_suspend(hba)) {
			/*
			 * The device is idle with no requests in the queue,
			 * allow background operations if bkops status shows
			 * that performance might be impacted.
			 */
			ret = os44_ufshcd_bkops_ctrl(hba);
			if (ret) {
				/*
				 * If return err in suspend flow, IO will hang.
				 * Trigger error handler and break suspend for
				 * error recovery.
				 */
				os44_ufshcd_force_error_recovery(hba);
				ret = -EBUSY;
				goto enable_scaling;
			}
		} else {
			/* make sure that auto bkops is disabled */
			os44_ufshcd_disable_auto_bkops(hba);
		}
		/*
		 * If device needs to do BKOP or WB buffer flush during
		 * Hibern8, keep device power mode as "active power mode"
		 * and VCC supply.
		 */
		hba->dev_info.b_rpm_dev_flush_capable =
			hba->auto_bkops_enabled ||
			(((req_link_state == UIC_LINK_HIBERN8_STATE) ||
			((req_link_state == UIC_LINK_ACTIVE_STATE) &&
			ufshcd_is_auto_hibern8_enabled(hba))) &&
			os44_ufshcd_wb_need_flush(hba));
	}

	flush_work(&hba->eeh_work);

	ret = ufshcd_vops_suspend(hba, pm_op, PRE_CHANGE);
	if (ret)
		goto enable_scaling;

	if (req_dev_pwr_mode != hba->curr_dev_pwr_mode) {
		if (pm_op != UFS_RUNTIME_PM)
			/* ensure that bkops is disabled */
			os44_ufshcd_disable_auto_bkops(hba);

		if (!hba->dev_info.b_rpm_dev_flush_capable) {
			ret = os44_ufshcd_set_dev_pwr_mode(hba, req_dev_pwr_mode);
			if (ret && pm_op != UFS_SHUTDOWN_PM) {
				/*
				 * If return err in suspend flow, IO will hang.
				 * Trigger error handler and break suspend for
				 * error recovery.
				 */
				os44_ufshcd_force_error_recovery(hba);
				ret = -EBUSY;
			}
			if (ret)
				goto enable_scaling;
		}
	}

	/*
	 * In the case of DeepSleep, the device is expected to remain powered
	 * with the link off, so do not check for bkops.
	 */
	check_for_bkops = !ufshcd_is_ufs_dev_deepsleep(hba);
	ret = os44_ufshcd_link_state_transition(hba, req_link_state, check_for_bkops);
	if (ret && pm_op != UFS_SHUTDOWN_PM) {
		/*
		 * If return err in suspend flow, IO will hang.
		 * Trigger error handler and break suspend for
		 * error recovery.
		 */
		os44_ufshcd_force_error_recovery(hba);
		ret = -EBUSY;
	}
	if (ret)
		goto set_dev_active;

vops_suspend:
	/*
	 * Call vendor specific suspend callback. As these callbacks may access
	 * vendor specific host controller register space call them before the
	 * host clocks are ON.
	 */
	ret = ufshcd_vops_suspend(hba, pm_op, POST_CHANGE);
	if (ret)
		goto set_link_active;

	cancel_delayed_work_sync(&hba->ufs_rtc_update_work);
	goto out;

set_link_active:
	/*
	 * Device hardware reset is required to exit DeepSleep. Also, for
	 * DeepSleep, the link is off so host reset and restore will be done
	 * further below.
	 */
	if (ufshcd_is_ufs_dev_deepsleep(hba)) {
		os44_ufshcd_device_reset(hba);
		WARN_ON(!ufshcd_is_link_off(hba));
	}
	if (ufshcd_is_link_hibern8(hba) && !os44_ufshcd_uic_hibern8_exit(hba))
		ufshcd_set_link_active(hba);
	else if (ufshcd_is_link_off(hba)) {
		os44_query_error(0, 11, -1, 0);
		os44_ufshcd_host_reset_and_restore(hba);
	}
set_dev_active:
	/* Can also get here needing to exit DeepSleep */
	if (ufshcd_is_ufs_dev_deepsleep(hba)) {
		os44_ufshcd_device_reset(hba);
		os44_ufshcd_host_reset_and_restore(hba);
	}
	if (!os44_ufshcd_set_dev_pwr_mode(hba, UFS_ACTIVE_PWR_MODE))
		os44_ufshcd_disable_auto_bkops(hba);
enable_scaling:
	if (ufshcd_is_clkscaling_supported(hba))
		ufshcd_clk_scaling_suspend(hba, false);

	hba->dev_info.b_rpm_dev_flush_capable = false;
out:
	if (hba->dev_info.b_rpm_dev_flush_capable) {
		schedule_delayed_work(&hba->rpm_dev_flush_recheck_work,
			msecs_to_jiffies(5000));
	}

	if (ret) {
		os44_query_error(0, 12, -1, 0);
		local_update_evt_hist(hba, UFS_EVT_WL_SUSP_ERR, (u32)ret);
		hba->clk_gating.is_suspended = false;
		local_release(hba);
	}
	hba->pm_op_in_progress = false;
	return ret;
}
