/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent pre/post power callback wrapper; real DME power owner. */
#include "os44_uic_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
int os44_ufshcd_config_pwr_mode(struct ufs_hba *hba,
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
