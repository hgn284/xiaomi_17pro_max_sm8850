/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/module.h>
#include "ufsswap_provider_reconstruction.h"
#include "os44_link_transition_contract.h"
#include "os44_hba_bkops_contract.h"
int os44_ufshcd_link_state_transition(struct ufs_hba *hba,
					enum uic_link_state req_link_state,
					bool check_for_bkops)
{
	int ret = 0;

	if (req_link_state == hba->uic_link_state)
		return 0;

	if (req_link_state == UIC_LINK_HIBERN8_STATE) {
		ret = os44_ufshcd_uic_hibern8_enter(hba);
		if (!ret) {
			ufshcd_set_link_hibern8(hba);
		} else {
			dev_err(hba->dev, "%s: hibern8 enter failed %d\n",
					"ufshcd_link_state_transition", ret);
			goto out;
		}
	}
	/*
	 * If autobkops is enabled, link can't be turned off because
	 * turning off the link would also turn off the device, except in the
	 * case of DeepSleep where the device is expected to remain powered.
	 */
	else if ((req_link_state == UIC_LINK_OFF_STATE) &&
		 (!check_for_bkops || !hba->auto_bkops_enabled)) {
		/*
		 * Let's make sure that link is in low power mode, we are doing
		 * this currently by putting the link in Hibern8. Otherway to
		 * put the link in low power mode is to send the DME end point
		 * to device and then send the DME reset command to local
		 * unipro. But putting the link in hibern8 is much faster.
		 *
		 * Note also that putting the link in Hibern8 is a requirement
		 * for entering DeepSleep.
		 */
		ret = os44_ufshcd_uic_hibern8_enter(hba);
		if (ret) {
			dev_err(hba->dev, "%s: hibern8 enter failed %d\n",
					"ufshcd_link_state_transition", ret);
			goto out;
		}
		/*
		 * Change controller state to "reset state" which
		 * should also put the link in off/reset state
		 */
		os44_ufshcd_hba_stop(hba);
		/*
		 * TODO: Check if we need any delay to make sure that
		 * controller is reset
		 */
		ufshcd_set_link_off(hba);
	}

out:
	return ret;
}
