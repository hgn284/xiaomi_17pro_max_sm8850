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
#include "os44_uic_contract.h"
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
int os44_ufshcd_scale_gear(struct ufs_hba *hba, u32 target_gear, bool scale_up)
{
	int ret = 0;
	struct ufs_pa_layer_attr new_pwr_info;

	if (target_gear) {
		new_pwr_info = hba->pwr_info;
		new_pwr_info.gear_tx = target_gear;
		new_pwr_info.gear_rx = target_gear;

		goto config_pwr_mode;
	}

	/* Legacy gear scaling, in case vops_freq_to_gear_speed() is not implemented */
	if (scale_up) {
		memcpy(&new_pwr_info, &hba->clk_scaling.saved_pwr_info,
		       sizeof(struct ufs_pa_layer_attr));
	} else {
		memcpy(&new_pwr_info, &hba->pwr_info,
		       sizeof(struct ufs_pa_layer_attr));

		if (hba->pwr_info.gear_tx > hba->clk_scaling.min_gear ||
		    hba->pwr_info.gear_rx > hba->clk_scaling.min_gear) {
			/* save the current power mode */
			memcpy(&hba->clk_scaling.saved_pwr_info,
				&hba->pwr_info,
				sizeof(struct ufs_pa_layer_attr));

			/* scale down gear */
			new_pwr_info.gear_tx = hba->clk_scaling.min_gear;
			new_pwr_info.gear_rx = hba->clk_scaling.min_gear;
		}
	}

config_pwr_mode:
	/* check if the power mode needs to be changed or not? */
	ret = local_config_pwr_mode(hba, &new_pwr_info);
	if (ret)
		dev_err(hba->dev, "%s: failed err %d, old gear: (tx %d rx %d), new gear: (tx %d rx %d)",
			"ufshcd_scale_gear", ret,
			hba->pwr_info.gear_tx, hba->pwr_info.gear_rx,
			new_pwr_info.gear_tx, new_pwr_info.gear_rx);

	return ret;
}
