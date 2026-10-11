// SPDX-License-Identifier: GPL-2.0-or-later
/* Whole OS44 initialization prerequisites from public scaffold + stock audit.
 * Actual HBA state, locks, query/executor providers. No owner stand-ins. */
#include <linux/delay.h>
#include "ufsswap_provider_reconstruction.h"
#include "os44_bkops_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_device_init_helpers.h"
#define NOP_OUT_RETRIES 10
#define QUERY_REQ_RETRIES 3
static_assert(offsetof(struct ufs_hba,nop_out_timeout)==0x9e8);
static_assert(offsetof(struct ufs_hba,dev_ref_clk_freq)==0xd0);
struct ufs_ref_clk { unsigned long freq_hz; enum ufs_ref_clk_freq val; };
static const struct ufs_ref_clk ufs_ref_clk_freqs[] = {
 {19200000, REF_CLK_FREQ_19_2_MHZ}, {26000000, REF_CLK_FREQ_26_MHZ},
 {38400000, REF_CLK_FREQ_38_4_MHZ}, {52000000, REF_CLK_FREQ_52_MHZ},
 {0, REF_CLK_FREQ_INVAL},
};
static noinline void os44_dev_man_lock_local(struct ufs_hba *hba)
{
 os44_core_hold(hba);
 mutex_lock(&hba->dev_cmd.lock);
 down_read(&hba->clk_scaling_lock);
}
static noinline void os44_dev_man_unlock_local(struct ufs_hba *hba)
{
 unsigned long flags;
 up_read(&hba->clk_scaling_lock);
 mutex_unlock(&hba->dev_cmd.lock);
 spin_lock_irqsave(&hba->clk_gating.lock,flags);
 os44_core_release_locked(hba);
 spin_unlock_irqrestore(&hba->clk_gating.lock,flags);
}
int local_attr_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum attr_idn idn, u8 index, u8 selector,
	u32 *attr_val)
{
	int ret = 0;
	u32 retries;

	for (retries = QUERY_REQ_RETRIES; retries > 0; retries--) {
		ret = os44_query_attr_once(hba, opcode, idn, index,
						selector, attr_val);
		if (ret)
			dev_dbg(hba->dev, "%s: failed with error %d, retries %d\n",
				"ufshcd_query_attr_retry", ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query attribute, idn %d, failed with error %d after %d retries\n",
			"ufshcd_query_attr_retry", idn, ret, QUERY_REQ_RETRIES);
	return ret;
}

void os44_ufshcd_set_dev_ref_clk(struct ufs_hba *hba)
{
	int err;
	u32 ref_clk;
	u32 freq = hba->dev_ref_clk_freq;

	err = local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_REF_CLK_FREQ, 0, 0, &ref_clk);

	if (err) {
		dev_err(hba->dev, "failed reading bRefClkFreq. err = %d\n",
			err);
		goto out;
	}

	if (ref_clk == freq)
		goto out; /* nothing to update */

	err = local_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			QUERY_ATTR_IDN_REF_CLK_FREQ, 0, 0, &freq);

	if (err) {
		dev_err(hba->dev, "bRefClkFreq setting to %lu Hz failed\n",
			ufs_ref_clk_freqs[freq].freq_hz);
		goto out;
	}

	dev_dbg(hba->dev, "bRefClkFreq setting to %lu Hz succeeded\n",
			ufs_ref_clk_freqs[freq].freq_hz);

out:
	return;
}

void os44_ufshcd_force_reset_auto_bkops(struct ufs_hba *hba)
{
	if (ufshcd_keep_autobkops_enabled_except_suspend(hba)) {
		hba->auto_bkops_enabled = false;
		hba->ee_ctrl_mask |= MASK_EE_URGENT_BKOPS;
		os44_ufshcd_enable_auto_bkops(hba);
	} else {
		hba->auto_bkops_enabled = true;
		hba->ee_ctrl_mask &= ~MASK_EE_URGENT_BKOPS;
		os44_ufshcd_disable_auto_bkops(hba);
	}
	hba->urgent_bkops_lvl = BKOPS_STATUS_PERF_IMPACT;
	hba->is_urgent_bkops_lvl_checked = false;
}

int os44_ufshcd_verify_dev_init(struct ufs_hba *hba)
{
	int err = 0;
	int retries;

	os44_dev_man_lock_local(hba);

	for (retries = NOP_OUT_RETRIES; retries > 0; retries--) {
		err = os44_core_exec_dev_cmd(hba, DEV_CMD_TYPE_NOP,
					  hba->nop_out_timeout);

		if (!err || err == -ETIMEDOUT)
			break;

		dev_dbg(hba->dev, "%s: error %d retrying\n", "ufshcd_verify_dev_init", err);
	}

	os44_dev_man_unlock_local(hba);

	if (err)
		dev_err(hba->dev, "%s: NOP OUT failed %d\n", "ufshcd_verify_dev_init", err);
	return err;
}
