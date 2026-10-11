/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent source reconstruction; see evidence/stock_functions.json. */
#include "ufsswap_provider_reconstruction.h"
#include "os44_bkops_contract.h"
#include "os44_wb_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
static inline int local_flag_retry(struct ufs_hba *hba,
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
static inline int local_attr_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum attr_idn idn, u8 index, u8 selector,
	u32 *attr_val)
{
	int ret = 0;
	u32 retries;

	for (retries = 3; retries > 0; retries--) {
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
			"ufshcd_query_attr_retry", idn, ret, 3);
	return ret;
}
static __always_inline int __ufshcd_wb_toggle(struct ufs_hba *hba, bool set, enum flag_idn idn)
{
	u8 index;
	enum query_opcode opcode;

	index = ufshcd_wb_get_query_index(hba);
	opcode = set ? UPIU_QUERY_OPCODE_SET_FLAG : UPIU_QUERY_OPCODE_CLEAR_FLAG;
	return local_flag_retry(hba, opcode, idn, index, NULL);
}

int os44_ufshcd_wb_toggle(struct ufs_hba *hba, bool enable)
{
	int ret;

	if (!ufshcd_is_wb_allowed(hba) ||
	    hba->dev_info.wb_enabled == enable)
		return 0;

	ret = __ufshcd_wb_toggle(hba, enable, QUERY_FLAG_IDN_WB_EN);
	if (ret) {
		dev_err(hba->dev, "%s: Write Booster %s failed %d\n",
			"ufshcd_wb_toggle", enable ? "enabling" : "disabling", ret);
		return ret;
	}

	hba->dev_info.wb_enabled = enable;
	dev_dbg(hba->dev, "%s: Write Booster %s\n",
			"ufshcd_wb_toggle", enable ? "enabled" : "disabled");

	return ret;
}

void os44_ufshcd_wb_toggle_buf_flush_during_h8(struct ufs_hba *hba,
						 bool enable)
{
	int ret;

	ret = __ufshcd_wb_toggle(hba, enable,
			QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8);
	if (ret) {
		dev_err(hba->dev, "%s: WB-Buf Flush during H8 %s failed %d\n",
			"ufshcd_wb_toggle_buf_flush_during_h8", enable ? "enabling" : "disabling", ret);
		return;
	}
	dev_dbg(hba->dev, "%s: WB-Buf Flush during H8 %s\n",
			"ufshcd_wb_toggle_buf_flush_during_h8", enable ? "enabled" : "disabled");
}

int os44_ufshcd_wb_toggle_buf_flush(struct ufs_hba *hba, bool enable)
{
	int ret;

	if (!ufshcd_is_wb_allowed(hba) ||
	    hba->dev_info.wb_buf_flush_enabled == enable)
		return 0;

	ret = __ufshcd_wb_toggle(hba, enable, QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN);
	if (ret) {
		dev_err(hba->dev, "%s: WB-Buf Flush %s failed %d\n",
			"ufshcd_wb_toggle_buf_flush", enable ? "enabling" : "disabling", ret);
		return ret;
	}

	hba->dev_info.wb_buf_flush_enabled = enable;
	dev_dbg(hba->dev, "%s: WB-Buf Flush %s\n",
			"ufshcd_wb_toggle_buf_flush", enable ? "enabled" : "disabled");

	return ret;
}

static bool ufshcd_wb_presrv_usrspc_keep_vcc_on(struct ufs_hba *hba,
						u32 avail_buf)
{
	u32 cur_buf;
	int ret;
	u8 index;

	index = ufshcd_wb_get_query_index(hba);
	ret = local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
					      QUERY_ATTR_IDN_CURR_WB_BUFF_SIZE,
					      index, 0, &cur_buf);
	if (ret) {
		dev_err(hba->dev, "%s: dCurWriteBoosterBufferSize read failed %d\n",
			"ufshcd_wb_presrv_usrspc_keep_vcc_on", ret);
		return false;
	}

	if (!cur_buf) {
		dev_info(hba->dev, "dCurWBBuf: %d WB disabled until free-space is available\n",
			 cur_buf);
		return false;
	}
	/* Let it continue to flush when available buffer exceeds threshold */
	return avail_buf < hba->vps->wb_flush_threshold;
}

static void ufshcd_wb_force_disable(struct ufs_hba *hba)
{
	if (ufshcd_is_wb_buf_flush_allowed(hba))
		os44_ufshcd_wb_toggle_buf_flush(hba, false);

	os44_ufshcd_wb_toggle_buf_flush_during_h8(hba, false);
	os44_ufshcd_wb_toggle(hba, false);
	hba->caps &= ~UFSHCD_CAP_WB_EN;

	dev_info(hba->dev, "%s: WB force disabled\n", "ufshcd_wb_force_disable");
}

static bool ufshcd_is_wb_buf_lifetime_available(struct ufs_hba *hba)
{
	u32 lifetime;
	int ret;
	u8 index;

	index = ufshcd_wb_get_query_index(hba);
	ret = local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				      QUERY_ATTR_IDN_WB_BUFF_LIFE_TIME_EST,
				      index, 0, &lifetime);
	if (ret) {
		dev_err(hba->dev,
			"%s: bWriteBoosterBufferLifeTimeEst read failed %d\n",
			"ufshcd_is_wb_buf_lifetime_available", ret);
		return false;
	}

	if (lifetime == UFS_WB_EXCEED_LIFETIME) {
		dev_err(hba->dev, "%s: WB buf lifetime is exhausted 0x%02X\n",
			"ufshcd_is_wb_buf_lifetime_available", lifetime);
		return false;
	}

	dev_dbg(hba->dev, "%s: WB buf lifetime is 0x%02X\n",
		"ufshcd_is_wb_buf_lifetime_available", lifetime);

	return true;
}

bool os44_ufshcd_wb_need_flush(struct ufs_hba *hba)
{
	int ret;
	u32 avail_buf;
	u8 index;

	if (!ufshcd_is_wb_allowed(hba))
		return false;

	if (!ufshcd_is_wb_buf_lifetime_available(hba)) {
		ufshcd_wb_force_disable(hba);
		return false;
	}

	/*
	 * The ufs device needs the vcc to be ON to flush.
	 * With user-space reduction enabled, it's enough to enable flush
	 * by checking only the available buffer. The threshold
	 * defined here is > 90% full.
	 * With user-space preserved enabled, the current-buffer
	 * should be checked too because the wb buffer size can reduce
	 * when disk tends to be full. This info is provided by current
	 * buffer (dCurrentWriteBoosterBufferSize). There's no point in
	 * keeping vcc on when current buffer is empty.
	 */
	index = ufshcd_wb_get_query_index(hba);
	ret = local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				      QUERY_ATTR_IDN_AVAIL_WB_BUFF_SIZE,
				      index, 0, &avail_buf);
	if (ret) {
		dev_warn(hba->dev, "%s: dAvailableWriteBoosterBufferSize read failed %d\n",
			 "ufshcd_wb_need_flush", ret);
		return false;
	}

	if (!hba->dev_info.b_presrv_uspc_en)
		return avail_buf <= UFS_WB_BUF_REMAIN_PERCENT(10);

	return ufshcd_wb_presrv_usrspc_keep_vcc_on(hba, avail_buf);
}
