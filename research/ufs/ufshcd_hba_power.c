/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/regulator/consumer.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_pm_contract.h"
int os44_ufshcd_config_vreg(struct device *, struct ufs_vreg *, bool);
static int ufshcd_disable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg || !vreg->enabled || vreg->always_on)
		goto out;

	ret = regulator_disable(vreg->reg);

	if (!ret) {
		/* ignore errors on applying disable config */
		os44_ufshcd_config_vreg(dev, vreg, false);
		vreg->enabled = false;
	} else {
		dev_err(dev, "%s: %s disable failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}
void os44_ufshcd_hba_vreg_set_lpm(struct ufs_hba *hba)
{
	if (ufshcd_is_link_off(hba) || ufshcd_can_aggressive_pc(hba))
		ufshcd_disable_vreg(hba->dev, hba->vreg_info.vdd_hba);
}
