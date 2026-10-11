// SPDX-License-Identifier: GPL-2.0-only
/* Whole OS44 probe reconstruction; real source-pair dependencies, stock DSS policy. */
#include <linux/module.h>
#include <linux/unaligned.h>
#include <trace/hooks/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_device_init.h"
#include "os44_pm_helpers_contract.h"
#include "os44_hba_bkops_contract.h"
#include "os44_hba_enable_contract.h"
#include "os44_descriptor_owner.h"
#include "os44_wb_contract.h"
#include "os44_bkops_contract.h"
#include "os44_pm_contract.h"
#include "os44_uic_contract.h"
#include "ufsswap_provider_reconstruction.h"
#include "os44_probe_hba.h"
#define CREATE_TRACE_POINTS
#include "os44_init_trace.h"
#define QUERY_REQ_RETRIES 3
static void ufs_put_device_desc(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;

	kfree(dev_info->model);
	dev_info->model = NULL;
}

static int local_ufshcd_query_attr_retry(struct ufs_hba *hba,
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

static int local_ufshcd_query_flag_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum flag_idn idn, u8 index, bool *flag_res)
{
	int ret;
	int retries;

	for (retries = 0; retries < QUERY_REQ_RETRIES; retries++) {
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

static u32 ufshcd_get_max_icc_level(int sup_curr_uA, u32 start_scan,
				    const char *buff)
{
	int i;
	int curr_uA;
	u16 data;
	u16 unit;

	for (i = start_scan; i >= 0; i--) {
		data = get_unaligned_be16(&buff[2 * i]);
		unit = (data & ATTR_ICC_LVL_UNIT_MASK) >>
						ATTR_ICC_LVL_UNIT_OFFSET;
		curr_uA = data & ATTR_ICC_LVL_VALUE_MASK;
		switch (unit) {
		case UFSHCD_NANO_AMP:
			curr_uA = curr_uA / 1000;
			break;
		case UFSHCD_MILI_AMP:
			curr_uA = curr_uA * 1000;
			break;
		case UFSHCD_AMP:
			curr_uA = curr_uA * 1000 * 1000;
			break;
		case UFSHCD_MICRO_AMP:
		default:
			break;
		}
		if (sup_curr_uA >= curr_uA)
			break;
	}
	if (i < 0) {
		i = 0;
		pr_err("%s: Couldn't find valid icc_level = %d", "ufshcd_get_max_icc_level", i);
	}

	return (u32)i;
}

static u32 ufshcd_find_max_sup_active_icc_level(struct ufs_hba *hba,
						const u8 *desc_buf)
{
	u32 icc_level = 0;

	if (!hba->vreg_info.vcc || !hba->vreg_info.vccq ||
						!hba->vreg_info.vccq2) {
		/*
		 * Using dev_dbg to avoid messages during runtime PM to avoid
		 * never-ending cycles of messages written back to storage by
		 * user space causing runtime resume, causing more messages and
		 * so on.
		 */
		dev_dbg(hba->dev,
			"%s: Regulator capability was not set, actvIccLevel=%d",
							"ufshcd_find_max_sup_active_icc_level", icc_level);
		goto out;
	}

	if (hba->vreg_info.vcc->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vcc->max_uA,
				POWER_DESC_MAX_ACTV_ICC_LVLS - 1,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCC_0]);

	if (hba->vreg_info.vccq->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vccq->max_uA,
				icc_level,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCCQ_0]);

	if (hba->vreg_info.vccq2->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vccq2->max_uA,
				icc_level,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCCQ2_0]);
out:
	return icc_level;
}

static void ufshcd_set_active_icc_lvl(struct ufs_hba *hba)
{
	int ret;
	u8 *desc_buf;
	u32 icc_level;

	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf)
		return;

	ret = os44_mi_ufshcd_read_desc_param(hba, QUERY_DESC_IDN_POWER, 0, 0,
				     desc_buf, QUERY_DESC_MAX_SIZE);
	if (ret) {
		dev_err(hba->dev,
			"%s: Failed reading power descriptor ret = %d",
			"ufshcd_set_active_icc_lvl", ret);
		goto out;
	}

	icc_level = ufshcd_find_max_sup_active_icc_level(hba, desc_buf);
	dev_dbg(hba->dev, "%s: setting icc_level 0x%x", "ufshcd_set_active_icc_lvl", icc_level);

	ret = local_ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
		QUERY_ATTR_IDN_ACTIVE_ICC_LVL, 0, 0, &icc_level);

	if (ret)
		dev_err(hba->dev,
			"%s: Failed configuring bActiveICCLevel = %d ret = %d",
			"ufshcd_set_active_icc_lvl", icc_level, ret);

out:
	kfree(desc_buf);
}

static void ufshcd_configure_wb(struct ufs_hba *hba)
{
	if (!ufshcd_is_wb_allowed(hba))
		return;

	os44_ufshcd_wb_toggle(hba, true);

	os44_ufshcd_wb_toggle_buf_flush_during_h8(hba, true);

	if (ufshcd_is_wb_buf_flush_allowed(hba))
		os44_ufshcd_wb_toggle_buf_flush(hba, true);
}

static int local_ufshcd_write_ee_control(struct ufs_hba *hba)
{
	int err;

	mutex_lock(&hba->ee_ctrl_mutex);
	err = __ufshcd_write_ee_control(hba, hba->ee_ctrl_mask);
	mutex_unlock(&hba->ee_ctrl_mutex);
	if (err)
		dev_err(hba->dev, "%s: failed to write ee control %d\n",
			"ufshcd_write_ee_control", err);
	return err;
}

static void ufshcd_configure_auto_hibern8(struct ufs_hba *hba)
{
	if (!ufshcd_is_auto_hibern8_supported(hba))
		return;

	ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);
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

static void ufshcd_configure_dss(struct ufs_hba *hba)
{
 int ret;
 if (hba->dev_info.wmanufacturerid != 0x12c) return;
 ret = local_ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG, 0x80, 0, NULL);
 if (ret) {
  dev_err(hba->dev, "%s: setting dssen flag failed with error %d\n", "ufshcd_configure_dss", ret);
  return;
 }
 ret = local_ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG, 0x85, 0, NULL);
 if (ret) {
  dev_err(hba->dev, "%s: setting dssmod flag failed with error %d\n", "ufshcd_configure_dss", ret);
  return;
 }
 dev_err(hba->dev, "%s: enable dss done.\n", "ufshcd_configure_dss");
}
int os44_ufshcd_probe_hba(struct ufs_hba *hba, bool init_dev_params)
{
	ktime_t start = ktime_get();
	unsigned long flags;
	int ret;

	ret = os44_ufshcd_device_init(hba, init_dev_params);
	if (ret)
		goto out;

	dev_err(hba->dev, "mi_ufs:ufshcd_probe_hba\n");
	if (!hba->pm_op_in_progress &&
	    (hba->quirks & UFSHCD_QUIRK_REINIT_AFTER_MAX_GEAR_SWITCH)) {
		/* Reset the device and controller before doing reinit */
		os44_ufshcd_device_reset(hba);
		ufs_put_device_desc(hba);
		os44_ufshcd_hba_stop(hba);
		ret = os44_ufshcd_hba_enable(hba);
		if (ret) {
			dev_err(hba->dev, "Host controller enable failed\n");
			os44_ufshcd_print_evt_hist(hba);
			os44_ufshcd_print_host_state(hba);
			goto out;
		}

		/* Reinit the device */
		ret = os44_ufshcd_device_init(hba, init_dev_params);
		if (ret)
			goto out;
	}

	ufshcd_print_pwr_info(hba);

	/*
	 * bActiveICCLevel is volatile for UFS device (as per latest v2.1 spec)
	 * and for removable UFS card as well, hence always set the parameter.
	 * Note: Error handler may issue the device reset hence resetting
	 * bActiveICCLevel as well so it is always safe to set this here.
	 */
	ufshcd_set_active_icc_lvl(hba);

	/* Enable UFS Write Booster if supported */
	ufshcd_configure_wb(hba);

	if (hba->ee_usr_mask)
		local_ufshcd_write_ee_control(hba);
	ufshcd_configure_auto_hibern8(hba);

	ufshcd_configure_dss(hba);
	trace_android_rvh_ufs_complete_init(hba);
out:
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (ret)
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
	else if (hba->ufshcd_state == UFSHCD_STATE_RESET)
		hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	trace_ufshcd_init(hba, ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	return ret;
}
