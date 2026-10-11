/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction of whole stock UIC wait and outer send.
 * Uses the single real timeout, error reporter, hold and locked-release owners. */
#include <linux/delay.h>
#include <linux/module.h>
#include "ufsswap_provider_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
#include "os44_uic_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#include <trace/hooks/ufshcd.h>
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

static inline void os44_uic_release(struct ufs_hba *hba)
{
	guard(spinlock_irqsave)(&hba->clk_gating.lock);
	os44_core_release_locked(hba);
}

int os44_ufshcd_wait_for_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	lockdep_assert_held(&hba->uic_cmd_mutex);

	if (wait_for_completion_timeout(&uic_cmd->done,
					msecs_to_jiffies(os44_uic_cmd_timeout))) {
		ret = uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;
	} else {
		ret = -ETIMEDOUT;
		dev_err(hba->dev,
			"uic cmd 0x%x with arg3 0x%x completion timeout\n",
			uic_cmd->command, uic_cmd->argument3);

		if (!uic_cmd->cmd_active) {
			dev_err(hba->dev, "%s: UIC cmd has been completed, return the result\n",
				"ufshcd_wait_for_uic_cmd");
			ret = uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;
		}
	}

	/* Stock reports nonzero UIC result, not an active-command timeout. */
	if (ret > 0)
		os44_query_error(0, 0, -1, 0);

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->active_uic_cmd = NULL;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	trace_android_vh_ufs_wait_for_uic_cmd(hba, uic_cmd, ret);
	return ret;
}

int os44_ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_UIC_CMD)
		return 0;

	os44_core_hold(hba);
	mutex_lock(&hba->uic_cmd_mutex);
	ufshcd_add_delay_before_dme_cmd(hba);

	ret = os44___ufshcd_send_uic_cmd(hba, uic_cmd);
	if (!ret)
		ret = os44_ufshcd_wait_for_uic_cmd(hba, uic_cmd);

	mutex_unlock(&hba->uic_cmd_mutex);

	os44_uic_release(hba);
	return ret;
}
