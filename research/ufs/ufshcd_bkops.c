/* SPDX-License-Identifier: GPL-2.0-only */
#include "ufsswap_provider_reconstruction.h"
#include "os44_bkops_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#define CREATE_TRACE_POINTS
#include "os44_bkops_trace.h"
static inline int os44_bkops_query_flag_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum flag_idn idn, u8 index, bool *flag_res)
{
	int ret;
	int retries;

	for (retries = 0; retries < 3; retries++) {
		ret = os44_ufshcd_query_flag(hba, opcode, idn, index, flag_res);
		if (ret)
			dev_dbg(hba->dev,
				"%s: failed with error %d, retries %d\n",
				"ufshcd_query_flag_retry", ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query flag, opcode %d, idn %d, failed with error %d after %d retries\n",
			"ufshcd_query_flag_retry", opcode, idn, ret, retries);
	return ret;
}
static inline int os44_bkops_update_ee_control(struct ufs_hba *hba, u16 *mask,
			     const u16 *other_mask, u16 set, u16 clr)
{
	u16 new_mask, ee_ctrl_mask;
	int err = 0;

	mutex_lock(&hba->ee_ctrl_mutex);
	new_mask = (*mask & ~clr) | set;
	ee_ctrl_mask = new_mask | *other_mask;
	if (ee_ctrl_mask != hba->ee_ctrl_mask)
		err = __ufshcd_write_ee_control(hba, ee_ctrl_mask);
	/* Still need to update 'mask' even if 'ee_ctrl_mask' was unchanged */
	if (!err) {
		hba->ee_ctrl_mask = ee_ctrl_mask;
		*mask = new_mask;
	}
	mutex_unlock(&hba->ee_ctrl_mutex);
	return err;
}
int os44_ufshcd_enable_auto_bkops(struct ufs_hba *hba)
{
	int err = 0;

	if (hba->auto_bkops_enabled)
		goto out;

	err = os44_bkops_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
			QUERY_FLAG_IDN_BKOPS_EN, 0, NULL);
	if (err) {
		dev_err(hba->dev, "%s: failed to enable bkops %d\n",
				"ufshcd_enable_auto_bkops", err);
		goto out;
	}

	hba->auto_bkops_enabled = true;
	trace_ufshcd_auto_bkops_state(hba, "Enabled");

	/* No need of URGENT_BKOPS exception from the device */
	err = os44_bkops_update_ee_control(hba, &hba->ee_drv_mask, &hba->ee_usr_mask, 0, MASK_EE_URGENT_BKOPS);
	if (err)
		dev_err(hba->dev, "%s: failed to disable exception event %d\n",
				"ufshcd_enable_auto_bkops", err);
out:
	return err;
}

int os44_ufshcd_disable_auto_bkops(struct ufs_hba *hba)
{
	int err = 0;

	if (!hba->auto_bkops_enabled)
		goto out;

	/*
	 * If host assisted BKOPs is to be enabled, make sure
	 * urgent bkops exception is allowed.
	 */
	err = os44_bkops_update_ee_control(hba, &hba->ee_drv_mask, &hba->ee_usr_mask, MASK_EE_URGENT_BKOPS, 0);
	if (err) {
		dev_err(hba->dev, "%s: failed to enable exception event %d\n",
				"ufshcd_disable_auto_bkops", err);
		goto out;
	}

	err = os44_bkops_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
			QUERY_FLAG_IDN_BKOPS_EN, 0, NULL);
	if (err) {
		dev_err(hba->dev, "%s: failed to disable bkops %d\n",
				"ufshcd_disable_auto_bkops", err);
		os44_bkops_update_ee_control(hba, &hba->ee_drv_mask, &hba->ee_usr_mask, 0, MASK_EE_URGENT_BKOPS);
		goto out;
	}

	hba->auto_bkops_enabled = false;
	trace_ufshcd_auto_bkops_state(hba, "Disabled");
	hba->is_urgent_bkops_lvl_checked = false;
out:
	return err;
}
