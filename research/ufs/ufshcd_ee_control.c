/* SPDX-License-Identifier: GPL-2.0-only */
/* Real EE control providers; locked update only commits masks after query success.
 * Query retry binds to the existing source-owned implementation, never stock aliases. */
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "ufsswap_provider_reconstruction.h"
#define OS44_QUERY_RETRIES 3
static_assert(offsetof(struct ufs_hba, ee_ctrl_mask)==0x1fc);
static_assert(offsetof(struct ufs_hba, ee_ctrl_mutex)==0x208);
static inline int os44_ee_query_attr_retry(struct ufs_hba *hba,
	unsigned int opcode, u8 idn, u8 index, u8 selector,
	u32 *attr_val)
{
	int ret = 0;
	u32 retries;

	for (retries = OS44_QUERY_RETRIES; retries > 0; retries--) {
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
			"ufshcd_query_attr_retry", idn, ret, OS44_QUERY_RETRIES);
	return ret;
}

int __ufshcd_write_ee_control(struct ufs_hba *hba, u32 ee_ctrl_mask)
{
	return os44_ee_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
				       QUERY_ATTR_IDN_EE_CONTROL, 0, 0,
				       &ee_ctrl_mask);
}

int ufshcd_write_ee_control(struct ufs_hba *hba)
{
	int err;

	mutex_lock(&hba->ee_ctrl_mutex);
	err = __ufshcd_write_ee_control(hba, hba->ee_ctrl_mask);
	mutex_unlock(&hba->ee_ctrl_mutex);
	if (err)
		dev_err(hba->dev, "%s: failed to write ee control %d\n",
			__func__, err);
	return err;
}

int ufshcd_update_ee_control(struct ufs_hba *hba, u16 *mask,
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
