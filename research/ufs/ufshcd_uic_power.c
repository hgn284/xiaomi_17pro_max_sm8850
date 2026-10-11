/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent stock diagnostic/power-control source, no skipped diagnostics. */
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "ufsswap_provider_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
#include "os44_uic_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
static inline void ufshcd_add_delay_before_dme_cmd(struct ufs_hba *hba)
{
	#define MIN_DELAY_BEFORE_DME_CMDS_US	1000
	unsigned long min_sleep_time_us;

	if (!(hba->quirks & UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS))
		return;

	/*
	 * last_dme_cmd_tstamp will be 0 only for 1st call to
	 * this function
	 */
	if (unlikely(!ktime_to_us(hba->last_dme_cmd_tstamp))) {
		min_sleep_time_us = MIN_DELAY_BEFORE_DME_CMDS_US;
	} else {
		unsigned long delta =
			(unsigned long) ktime_to_us(
				ktime_sub(ktime_get(),
				hba->last_dme_cmd_tstamp));

		if (delta < MIN_DELAY_BEFORE_DME_CMDS_US)
			min_sleep_time_us =
				MIN_DELAY_BEFORE_DME_CMDS_US - delta;
		else
			min_sleep_time_us = 0; /* no more delay required */
	}

	if (min_sleep_time_us > 0) {
		/* allow sleep for extra 50us if needed */
		usleep_range(min_sleep_time_us, min_sleep_time_us + 50);
	}

	/* update the last_dme_cmd_tstamp */
	hba->last_dme_cmd_tstamp = ktime_get();
}

static void ufshcd_disable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	set &= ~intrs;
	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

static void ufshcd_enable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	set |= intrs;
	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

static inline u8 ufshcd_get_upmcrs(struct ufs_hba *hba)
{
	return (ufshcd_readl(hba, REG_CONTROLLER_STATUS) >> 8) & 0x7;
}

static void ufshcd_print_pwr_info(struct ufs_hba *hba)
{
	static const char * const names[] = {
		"INVALID MODE",
		"FAST MODE",
		"SLOW_MODE",
		"INVALID MODE",
		"FASTAUTO_MODE",
		"SLOWAUTO_MODE",
		"INVALID MODE",
	};

	/*
	 * Using dev_dbg to avoid messages during runtime PM to avoid
	 * never-ending cycles of messages written back to storage by user space
	 * causing runtime resume, causing more messages and so on.
	 */
	dev_dbg(hba->dev, "%s:[RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%s, %s], rate = %d\n",
		 "ufshcd_print_pwr_info",
		 hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		 hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		 names[hba->pwr_info.pwr_rx],
		 names[hba->pwr_info.pwr_tx],
		 hba->pwr_info.hs_rate);
}

int os44_ufshcd_uic_pwr_ctrl(struct ufs_hba *hba, struct uic_command *cmd)
{
	DECLARE_COMPLETION_ONSTACK(uic_async_done);
	unsigned long flags;
	u8 status;
	int ret;
	bool reenable_intr = false;

	mutex_lock(&hba->uic_cmd_mutex);
	ufshcd_add_delay_before_dme_cmd(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (ufshcd_is_link_broken(hba)) {
		ret = -ENOLINK;
		goto out_unlock;
	}
	hba->uic_async_done = &uic_async_done;
	if (ufshcd_readl(hba, REG_INTERRUPT_ENABLE) & UIC_COMMAND_COMPL) {
		ufshcd_disable_intr(hba, UIC_COMMAND_COMPL);
		/*
		 * Make sure UIC command completion interrupt is disabled before
		 * issuing UIC command.
		 */
		ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
		reenable_intr = true;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ret = os44___ufshcd_send_uic_cmd(hba, cmd);
	if (ret) {
		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x with mode 0x%x uic error %d\n",
			cmd->command, cmd->argument3, ret);
		goto out;
	}

	if (!wait_for_completion_timeout(hba->uic_async_done,
					 msecs_to_jiffies(os44_uic_cmd_timeout))) {
		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x with mode 0x%x completion timeout\n",
			cmd->command, cmd->argument3);

		if (!cmd->cmd_active) {
			dev_err(hba->dev, "%s: Power Mode Change operation has been completed, go check UPMCRS\n",
				"ufshcd_uic_pwr_ctrl");
			goto check_upmcrs;
		}

		os44_uicerr_flag = true;
		ret = -ETIMEDOUT;
		goto out;
	}

check_upmcrs:
	status = ufshcd_get_upmcrs(hba);
	if (status != PWR_LOCAL) {
		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x failed, host upmcrs:0x%x\n",
			cmd->command, status);
		ret = (status != PWR_OK) ? status : -1;
	}
out:
	if (ret) {
		os44_query_error(0, 2, -1, 0);
		os44_ufshcd_print_host_state(hba);
		ufshcd_print_pwr_info(hba);
		os44_ufshcd_print_evt_hist(hba);
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->active_uic_cmd = NULL;
	hba->uic_async_done = NULL;
	if (reenable_intr)
		ufshcd_enable_intr(hba, UIC_COMMAND_COMPL);
	if (ret && !hba->pm_op_in_progress) {
		ufshcd_set_link_broken(hba);
		/* Link was just set broken: exact scheduling branch of stock. */
		if (hba->ufshcd_state != UFSHCD_STATE_ERROR) {
			hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;
			queue_work(hba->eh_wq, &hba->eh_work);
		}
	}
out_unlock:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	mutex_unlock(&hba->uic_cmd_mutex);

	return ret;
}
