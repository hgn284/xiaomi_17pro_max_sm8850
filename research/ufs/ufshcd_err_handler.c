// SPDX-License-Identifier: GPL-2.0-only
/* Reconstructed OS44 complete recovery functions; no owner storage or ABI bypass. */
#include <linux/blk-mq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_err_handler.h"
#include "os44_abort_contract.h"
#include "os44_completion_contract.h"
#include "os44_command_trace_contract.h"
#include "os44_reset_recovery.h"
#include "os44_pm_helpers_contract.h"
#include "os44_pm_contract.h"
#include "os44_uic_contract.h"
#include "os44_tm_contract.h"
#include "os44_reset_prereqs.h"
#include "os44_device_init_helpers.h"
#include "ufsswap_provider_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
extern int os44_ufshcd_poll(struct Scsi_Host *, unsigned int);
extern unsigned long os44_ufshcd_mcq_poll_cqe_lock(struct ufs_hba *, struct ufs_hw_queue *);
extern int os44_ufshcd_setup_vreg(struct ufs_hba *, bool);

#undef INT_FATAL_ERRORS
#define INT_FATAL_ERRORS 0x70880
#define MAX_ERR_HANDLER_RETRIES 5
#define UFSHCD_POLL_FROM_INTERRUPT_CONTEXT (-1)
#define ufshcd_set_eh_in_progress(h) ((h)->eh_flags |= 1)
#define ufshcd_clear_eh_in_progress(h) ((h)->eh_flags &= ~1)
#define ufshcd_hex_dump(p,b,l) print_hex_dump(KERN_ERR,p,(l)>4?DUMP_PREFIX_OFFSET:DUMP_PREFIX_NONE,16,4,b,l,false)
static const char *const ufshcd_state_name[] = {
	[UFSHCD_STATE_RESET]			= "reset",
	[UFSHCD_STATE_OPERATIONAL]		= "operational",
	[UFSHCD_STATE_ERROR]			= "error",
	[UFSHCD_STATE_EH_SCHEDULED_FATAL]	= "eh_fatal",
	[UFSHCD_STATE_EH_SCHEDULED_NON_FATAL]	= "eh_non_fatal",
};

/* UFSHCD UIC layer error flags */
enum {
	UFSHCD_UIC_DL_PA_INIT_ERROR = (1 << 0), /* Data link layer error */
	UFSHCD_UIC_DL_NAC_RECEIVED_ERROR = (1 << 1), /* Data link layer error */
	UFSHCD_UIC_DL_TCx_REPLAY_ERROR = (1 << 2), /* Data link layer error */
	UFSHCD_UIC_NL_ERROR = (1 << 3), /* Network layer error */
	UFSHCD_UIC_TL_ERROR = (1 << 4), /* Transport Layer error */
	UFSHCD_UIC_DME_ERROR = (1 << 5), /* DME error */
	UFSHCD_UIC_PA_GENERIC_ERROR = (1 << 6), /* Generic PA error */
};

static int ufshcd_config_vreg_load(struct device *dev, struct ufs_vreg *vreg,
				   int ua)
{
	int ret;

	if (!vreg)
		return 0;

	/*
	 * "set_load" operation shall be required on those regulators
	 * which specifically configured current limitation. Otherwise
	 * zero max_uA may cause unexpected behavior when regulator is
	 * enabled or set as high power mode.
	 */
	if (!vreg->max_uA)
		return 0;

	ret = regulator_set_load(vreg->reg, ua);
	if (ret < 0) {
		dev_err(dev, "%s: %s set load (ua=%d) failed, err=%d\n",
				"ufshcd_config_vreg_load", vreg->name, ua, ret);
	}

	return ret;
}
static inline int ufshcd_config_vreg_hpm(struct ufs_hba *hba,
					 struct ufs_vreg *vreg)
{
	if (!vreg)
		return 0;

	return ufshcd_config_vreg_load(hba->dev, vreg, vreg->max_uA);
}
static int ufshcd_config_vreg(struct device *dev,
		struct ufs_vreg *vreg, bool on)
{
	if (regulator_count_voltages(vreg->reg) <= 0)
		return 0;

	return ufshcd_config_vreg_load(dev, vreg, on ? vreg->max_uA : 0);
}
static int ufshcd_enable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg || vreg->enabled)
		goto out;

	ret = ufshcd_config_vreg(dev, vreg, true);
	if (!ret)
		ret = regulator_enable(vreg->reg);

	if (!ret)
		vreg->enabled = true;
	else
		dev_err(dev, "%s: %s enable failed, err=%d\n",
				"ufshcd_enable_vreg", vreg->name, ret);
out:
	return ret;
}
static void local_enable_irq(struct ufs_hba *hba)
{
	if (!hba->is_irq_enabled) {
		enable_irq(hba->irq);
		hba->is_irq_enabled = true;
	}
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
static void ufshcd_clk_scaling_allow(struct ufs_hba *hba, bool allow)
{
	mutex_lock(&hba->wb_mutex);
	down_write(&hba->clk_scaling_lock);
	hba->clk_scaling.is_allowed = allow;
	up_write(&hba->clk_scaling_lock);
	mutex_unlock(&hba->wb_mutex);
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
static void local_release(struct ufs_hba *hba)
{ guard(spinlock_irqsave)(&hba->clk_gating.lock); os44_core_release_locked(hba); }
static void local_dme_get_mode(struct ufs_hba *hba, u32 *mode)
{
 struct uic_command cmd = { .command = UIC_CMD_DME_GET, .argument1 = UIC_ARG_MIB(PA_PWRMODE) };
 int ret = os44_ufshcd_send_uic_cmd(hba, &cmd);
 if (ret) dev_err(hba->dev, "%s: attr-id 0x%x failed %d retries\n", "dme-get", PA_PWRMODE, 0);
 *mode = ret == 0 ? cmd.argument3 : 0;
}
static inline void local_reset_intr_aggr(struct ufs_hba *hba)
{ ufshcd_writel(hba, 0x80010000, REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL); }
static irqreturn_t ufshcd_transfer_req_compl(struct ufs_hba *hba)
{
	/* Resetting interrupt aggregation counters first and reading the
	 * DOOR_BELL afterward allows us to handle all the completed requests.
	 * In order to prevent other interrupts starvation the DB is read once
	 * after reset. The down side of this solution is the possibility of
	 * false interrupt if device completes another request after resetting
	 * aggregation and before reading the DB.
	 */
	if (ufshcd_is_intr_aggr_allowed(hba) &&
	    !(hba->quirks & UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR))
		local_reset_intr_aggr(hba);

	

	/*
	 * Ignore the os44_ufshcd_poll() return value and return IRQ_HANDLED since we
	 * do not want polling to trigger spurious interrupt complaints.
	 */
	os44_ufshcd_poll(hba->host, UFSHCD_POLL_FROM_INTERRUPT_CONTEXT);

	return IRQ_HANDLED;
}
static void ufshcd_complete_requests(struct ufs_hba *hba, bool force_compl)
{
	if (hba->mcq_enabled)
		blk_mq_tagset_busy_iter(&hba->host->tag_set, os44_ufshcd_mcq_compl_one, NULL);
	else
		ufshcd_transfer_req_compl(hba);

	os44_ufshcd_tmc_handler(hba);
}
static bool ufshcd_quirk_dl_nac_errors(struct ufs_hba *hba)
{
	unsigned long flags;
	bool err_handling = true;

	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS only workaround the
	 * device fatal error and/or DL NAC & REPLAY timeout errors.
	 */
	if (hba->saved_err & (CONTROLLER_FATAL_ERROR | SYSTEM_BUS_FATAL_ERROR))
		goto out;

	if ((hba->saved_err & DEVICE_FATAL_ERROR) ||
	    ((hba->saved_err & UIC_ERROR) &&
	     (hba->saved_uic_err & UFSHCD_UIC_DL_TCx_REPLAY_ERROR)))
		goto out;

	if ((hba->saved_err & UIC_ERROR) &&
	    (hba->saved_uic_err & UFSHCD_UIC_DL_NAC_RECEIVED_ERROR)) {
		int err;
		/*
		 * wait for 50ms to see if we can get any other errors or not.
		 */
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		msleep(50);
		spin_lock_irqsave(hba->host->host_lock, flags);

		/*
		 * now check if we have got any other severe errors other than
		 * DL NAC error?
		 */
		if ((hba->saved_err & INT_FATAL_ERRORS) ||
		    ((hba->saved_err & UIC_ERROR) &&
		    (hba->saved_uic_err & ~UFSHCD_UIC_DL_NAC_RECEIVED_ERROR)))
			goto out;

		/*
		 * As DL NAC is the only error received so far, send out NOP
		 * command to confirm if link is still active or not.
		 *   - If we don't get any response then do error recovery.
		 *   - If we get response then clear the DL NAC error bit.
		 */

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		err = os44_ufshcd_verify_dev_init(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);

		if (err)
			goto out;

		/* Link seems to be alive hence ignore the DL NAC errors */
		if (hba->saved_uic_err == UFSHCD_UIC_DL_NAC_RECEIVED_ERROR)
			hba->saved_err &= ~UIC_ERROR;
		/* clear NAC error */
		hba->saved_uic_err &= ~UFSHCD_UIC_DL_NAC_RECEIVED_ERROR;
		if (!hba->saved_uic_err)
			err_handling = false;
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return err_handling;
}
static inline bool ufshcd_is_saved_err_fatal(struct ufs_hba *hba)
{
	return (hba->saved_uic_err & UFSHCD_UIC_DL_PA_INIT_ERROR) ||
	       (hba->saved_err & (INT_FATAL_ERRORS | UFSHCD_UIC_HIBERN8_MASK));
}
static void ufshcd_scsi_block_requests(struct ufs_hba *hba)
{ if (atomic_inc_return(&hba->scsi_block_reqs_cnt)==1) scsi_block_requests(hba->host); }
static void ufshcd_scsi_unblock_requests(struct ufs_hba *hba)
{ if (atomic_dec_return(&hba->scsi_block_reqs_cnt)==0) scsi_unblock_requests(hba->host); }
static void ufshcd_err_handling_prepare(struct ufs_hba *hba)
{
	/*
	 * A WLUN resume failure could potentially lead to the HBA being
	 * runtime suspended, so take an extra reference on hba->dev.
	 */
	pm_runtime_get_sync(hba->dev);
	ufshcd_rpm_get_sync(hba);
	if (pm_runtime_status_suspended(&hba->ufs_device_wlun->sdev_gendev) ||
	    hba->is_sys_suspended) {
		enum ufs_pm_op pm_op;

		/*
		 * Don't assume anything of resume, if
		 * resume fails, irq and clocks can be OFF, and powers
		 * can be OFF or in LPM.
		 */
		ufshcd_enable_vreg(hba->dev, hba->vreg_info.vdd_hba);
		local_enable_irq(hba);
		os44_ufshcd_setup_vreg(hba, true);
		ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq);
		ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq2);
		os44_core_hold(hba);
		if (!ufshcd_is_clkgating_allowed(hba))
			os44_ufshcd_setup_clocks(hba, true);
		pm_op = hba->is_sys_suspended ? UFS_SYSTEM_PM : UFS_RUNTIME_PM;
		ufshcd_vops_resume(hba, pm_op);
	} else {
		os44_core_hold(hba);
		if (ufshcd_is_clkscaling_supported(hba) &&
		    hba->clk_scaling.is_enabled)
			os44_ufshcd_suspend_clkscaling(hba);
		ufshcd_clk_scaling_allow(hba, false);
	}
	ufshcd_scsi_block_requests(hba);
	/* Wait for ongoing ufshcd_queuecommand() calls to finish. */
	blk_mq_wait_quiesce_done(&hba->host->tag_set);
	cancel_work_sync(&hba->eeh_work);
}
static void ufshcd_err_handling_unprepare(struct ufs_hba *hba)
{
	ufshcd_scsi_unblock_requests(hba);
	local_release(hba);
	if (ufshcd_is_clkscaling_supported(hba))
		ufshcd_clk_scaling_suspend(hba, false);
	ufshcd_rpm_put(hba);
	pm_runtime_put(hba->dev);
}
static inline bool ufshcd_err_handling_should_stop(struct ufs_hba *hba)
{
	return (!hba->is_powered || hba->shutting_down ||
		!hba->ufs_device_wlun ||
		hba->ufshcd_state == UFSHCD_STATE_ERROR ||
		(!(hba->saved_err || hba->saved_uic_err || hba->force_reset ||
		   ufshcd_is_link_broken(hba))));
}
static void ufshcd_recover_pm_error(struct ufs_hba *hba)
{
	struct scsi_target *starget = hba->ufs_device_wlun->sdev_target;
	struct Scsi_Host *shost = hba->host;
	struct scsi_device *sdev;
	struct request_queue *q;
	bool resume_sdev_queues = false;

	hba->is_sys_suspended = false;

	/*
	 * Ensure the parent's error status is cleared before proceeding
	 * to the child, as the parent must be active to activate the child.
	 */
	if (hba->dev->power.runtime_error) {
		/* hba->dev has no functional parent thus simplily set RPM_ACTIVE */
		pm_runtime_set_active(hba->dev);
		resume_sdev_queues = true;
	}

	if (hba->ufs_device_wlun->sdev_gendev.power.runtime_error) {
		/*
		 * starget, parent of wlun, might be suspended if wlun resume failed.
		 * Make sure parent is resumed before set child (wlun) active.
		 */
		pm_runtime_get_sync(&starget->dev);
		pm_runtime_set_active(&hba->ufs_device_wlun->sdev_gendev);
		pm_runtime_put_sync(&starget->dev);
		resume_sdev_queues = true;
	}

	/*
	 * If wlun device had runtime error, we also need to resume those
	 * consumer scsi devices in case any of them has failed to be
	 * resumed due to supplier runtime resume failure. This is to unblock
	 * blk_queue_enter in case there are bios waiting inside it.
	 */
	if (resume_sdev_queues) {
		shost_for_each_device(sdev, shost) {
			q = sdev->request_queue;
			if (q->dev && (q->rpm_status == RPM_SUSPENDED ||
				       q->rpm_status == RPM_SUSPENDING))
				pm_request_resume(q->dev);
		}
	}
}
static bool ufshcd_is_pwr_mode_restore_needed(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr *pwr_info = &hba->pwr_info;
	u32 mode;

	local_dme_get_mode(hba, &mode);

	if (pwr_info->pwr_rx != ((mode >> PWRMODE_RX_OFFSET) & PWRMODE_MASK))
		return true;

	if (pwr_info->pwr_tx != (mode & PWRMODE_MASK))
		return true;

	return false;
}
static bool ufshcd_abort_all(struct ufs_hba *hba)
{
	int tag, ret = 0;

	blk_mq_tagset_busy_iter(&hba->host->tag_set, os44_ufshcd_abort_one, &ret);
	if (ret)
		goto out;

	/* Clear pending task management requests */
	for_each_set_bit(tag, &hba->outstanding_tasks, hba->nutmrs) {
		ret = os44_ufshcd_clear_tm_cmd(hba, tag);
		if (ret)
			goto out;
	}

out:
	/* Complete the requests that are cleared by s/w */
	ufshcd_complete_requests(hba, false);

	return ret != 0;
}
static void ufshcd_print_tmrs(struct ufs_hba *hba, unsigned long bitmap)
{
	int tag;

	for_each_set_bit(tag, &bitmap, hba->nutmrs) {
		struct utp_task_req_desc *tmrdp = &hba->utmrdl_base_addr[tag];

		dev_err(hba->dev, "TM[%d] - Task Management Header\n", tag);
		ufshcd_hex_dump("", tmrdp, sizeof(*tmrdp));
	}
}
static void ufshcd_print_trs_all(struct ufs_hba *hba, bool pr_prdt)
{
	blk_mq_tagset_busy_iter(&hba->host->tag_set, os44_ufshcd_print_tr_iter, &pr_prdt);
}
static int local_config_pwr_mode(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *desired_pwr_mode)
{
	struct ufs_pa_layer_attr final_params = { 0 };
	int ret;

	ret = ufshcd_vops_pwr_change_notify(hba, PRE_CHANGE,
					desired_pwr_mode, &final_params);

	if (ret)
		memcpy(&final_params, desired_pwr_mode, sizeof(final_params));

	ret = os44_ufshcd_change_power_mode(hba, &final_params);

	if (!ret)
		ufshcd_vops_pwr_change_notify(hba, POST_CHANGE, NULL,
					&final_params);

	return ret;
}
void os44_ufshcd_err_handler(struct work_struct *work)
{
	int retries = MAX_ERR_HANDLER_RETRIES;
	struct ufs_hba *hba;
	unsigned long flags;
	bool needs_restore;
	bool needs_reset;
	int pmc_err;

	hba = container_of(work, struct ufs_hba, eh_work);
	os44_query_error(0, 9, -1, 0);

	dev_info(hba->dev,
		 "%s started; HBA state %s; powered %d; shutting down %d; saved_err = %d; saved_uic_err = %d; force_reset = %d%s\n",
		 "ufshcd_err_handler", ufshcd_state_name[hba->ufshcd_state],
		 hba->is_powered, hba->shutting_down, hba->saved_err,
		 hba->saved_uic_err, hba->force_reset,
		 ufshcd_is_link_broken(hba) ? "; link is broken" : "");

	down(&hba->host_sem);
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (ufshcd_err_handling_should_stop(hba)) {
		if (hba->ufshcd_state != UFSHCD_STATE_ERROR)
			hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		up(&hba->host_sem);
		return;
	}
	ufshcd_set_eh_in_progress(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_err_handling_prepare(hba);

	/* Complete requests that have door-bell cleared by h/w */
	ufshcd_complete_requests(hba, false);
	spin_lock_irqsave(hba->host->host_lock, flags);
again:
	needs_restore = false;
	needs_reset = false;

	if (hba->ufshcd_state != UFSHCD_STATE_ERROR)
		hba->ufshcd_state = UFSHCD_STATE_RESET;
	/*
	 * A full reset and restore might have happened after preparation
	 * is finished, double check whether we should stop.
	 */
	if (ufshcd_err_handling_should_stop(hba))
		goto skip_err_handling;

	if ((hba->dev_quirks & UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS) &&
	    !hba->force_reset) {
		bool ret;

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		/* release the lock as ufshcd_quirk_dl_nac_errors() may sleep */
		ret = ufshcd_quirk_dl_nac_errors(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (!ret && ufshcd_err_handling_should_stop(hba))
			goto skip_err_handling;
	}

	if ((hba->saved_err & (INT_FATAL_ERRORS | UFSHCD_UIC_HIBERN8_MASK)) ||
	    (hba->saved_uic_err &&
	     (hba->saved_uic_err != UFSHCD_UIC_PA_GENERIC_ERROR))) {
		bool pr_prdt = !!(hba->saved_err & SYSTEM_BUS_FATAL_ERROR);

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		os44_ufshcd_print_host_state(hba);
		os44_ufshcd_print_evt_hist(hba);
		ufshcd_print_tmrs(hba, hba->outstanding_tasks);
		ufshcd_print_trs_all(hba, pr_prdt);
		spin_lock_irqsave(hba->host->host_lock, flags);
	}

	/*
	 * if host reset is required then skip clearing the pending
	 * transfers forcefully because they will get cleared during
	 * host reset and restore
	 */
	if (hba->force_reset || ufshcd_is_link_broken(hba) ||
	    ufshcd_is_saved_err_fatal(hba) ||
	    ((hba->saved_err & UIC_ERROR) &&
	     (hba->saved_uic_err & (UFSHCD_UIC_DL_NAC_RECEIVED_ERROR |
				    UFSHCD_UIC_DL_TCx_REPLAY_ERROR)))) {
		needs_reset = true;
		goto do_reset;
	}

	/*
	 * If LINERESET was caught, UFS might have been put to PWM mode,
	 * check if power mode restore is needed.
	 */
	if (hba->saved_uic_err & UFSHCD_UIC_PA_GENERIC_ERROR) {
		hba->saved_uic_err &= ~(UFSHCD_UIC_PA_GENERIC_ERROR | UFSHCD_UIC_DL_PA_INIT_ERROR);
		if (!hba->saved_uic_err)
			hba->saved_err &= ~(UIC_ERROR | INT_FATAL_ERRORS | UFSHCD_UIC_HIBERN8_MASK);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		if (ufshcd_is_pwr_mode_restore_needed(hba))
			needs_restore = true;
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (!hba->saved_err && !needs_restore)
			goto skip_err_handling;
	}

	hba->silence_err_logs = true;
	/* release lock as clear command might sleep */
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	needs_reset = ufshcd_abort_all(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->silence_err_logs = false;
	if (needs_reset)
		goto do_reset;

	/*
	 * After all reqs and tasks are cleared from doorbell,
	 * now it is safe to retore power mode.
	 */
	if (needs_restore) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		/*
		 * Hold the scaling lock just in case dev cmds
		 * are sent via bsg and/or sysfs.
		 */
		down_write(&hba->clk_scaling_lock);
		hba->force_pmc = true;
		pmc_err = local_config_pwr_mode(hba, &(hba->pwr_info));
		if (pmc_err) {
			needs_reset = true;
			dev_err(hba->dev, "%s: Failed to restore power mode, err = %d\n",
					"ufshcd_err_handler", pmc_err);
		}
		hba->force_pmc = false;
		up_write(&hba->clk_scaling_lock);
		spin_lock_irqsave(hba->host->host_lock, flags);
	}

do_reset:
	/* Fatal errors need reset */
	if (needs_reset) {
		int err;

		hba->force_reset = false;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		err = os44_ufshcd_reset_and_restore(hba);
		if (err)
			dev_err(hba->dev, "%s: reset and restore failed with err %d\n",
					"ufshcd_err_handler", err);
		else
			ufshcd_recover_pm_error(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);
	}

skip_err_handling:
	if (!needs_reset) {
		if (hba->ufshcd_state == UFSHCD_STATE_RESET)
			hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;
		if (hba->saved_err || hba->saved_uic_err) {
   static DEFINE_RATELIMIT_STATE(_rs, DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST);
   if (___ratelimit(&_rs, "ufshcd_err_handler"))
    dev_err(hba->dev, "%s: exit: saved_err 0x%x saved_uic_err 0x%x", "ufshcd_err_handler", hba->saved_err, hba->saved_uic_err);
  }
	}
	/* Exit in an operational state or dead */
	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL &&
	    hba->ufshcd_state != UFSHCD_STATE_ERROR) {
		if (--retries)
			goto again;
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
	}
	ufshcd_clear_eh_in_progress(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_err_handling_unprepare(hba);
	up(&hba->host_sem);

	dev_info(hba->dev, "%s finished; HBA state %s\n", "ufshcd_err_handler",
		 ufshcd_state_name[hba->ufshcd_state]);
}
