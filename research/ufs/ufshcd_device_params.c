// SPDX-License-Identifier: GPL-2.0-or-later
/* Linux UFS core scaffolding; OS44 stock function reconstruction.
 * Layout views are reconstruction-owned, not recovered private declarations.
 * No stand-in HBA/ufs_xiaomi/ufs_data storage is defined. */
#include <linux/slab.h>
#include <linux/time64.h>
#include <linux/unaligned.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_descriptor_owner.h"
#include "os44_device_identity.h"
#include "os44_fastdiscard_contract.h"
#include "os44_uic_contract.h"
#include "ufsswap_provider_reconstruction.h"
#define QUERY_REQ_RETRIES 3
#define DEFAULT_MAX_NUM_RTT 2
#define UFSHCD_REF_CLK_GATING_WAIT_US 0xff
void os44_fastdiscard_init(struct ufs_hba *);
int os44_ufshcd_query_flag(struct ufs_hba *, enum query_opcode, enum flag_idn, u8, bool *);
/* Stock halfword at HBA+0xa26; reserved padding in the public dev_info. */
struct os44_dev_desc_tail_view { u8 prefix[0x36]; u16 field_4d; };
static_assert(offsetof(struct ufs_hba,dev_info)==0x9f0);
static_assert(offsetof(struct os44_dev_desc_tail_view,field_4d)==0x36);
static_assert(sizeof(struct os44_fastdiscard_oem_view)==8);
static inline void local_enable_ee(struct ufs_hba *hba, u16 set)
{
 u16 next, mask; int err=0;
 mutex_lock(&hba->ee_ctrl_mutex);
 next=hba->ee_drv_mask | set; mask=next | hba->ee_usr_mask;
 if (mask != hba->ee_ctrl_mask) err=__ufshcd_write_ee_control(hba,mask);
 if (!err) {hba->ee_ctrl_mask=mask;hba->ee_drv_mask=next;}
 mutex_unlock(&hba->ee_ctrl_mutex);
}
static inline int local_dme_peer_get(struct ufs_hba *hba,u32 sel,u32 *val)
{ return os44_ufshcd_dme_get_attr(hba,sel,val,DME_PEER); }
static inline int local_dme_get(struct ufs_hba *hba,u32 sel,u32 *val)
{
 struct uic_command cmd={.command=UIC_CMD_DME_GET,.argument1=sel};
 int ret=os44_ufshcd_send_uic_cmd(hba,&cmd);
 if (ret) dev_err(hba->dev,"%s: attr-id 0x%x failed %d retries\n","dme-get",UIC_GET_ATTR_ID(sel),0);
 else *val=cmd.argument3;
 return ret;
}
static inline int local_ufshcd_query_attr_retry(struct ufs_hba *hba,
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

static inline int local_ufshcd_query_flag_retry(struct ufs_hba *hba,
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

static inline int local_ufshcd_read_unit_desc_param(struct ufs_hba *hba,
					      int lun,
					      enum unit_desc_param param_offset,
					      u8 *param_read_buf,
					      u32 param_size)
{
	/*
	 * Unit descriptors are only available for general purpose LUs (LUN id
	 * from 0 to 7) and RPMB Well known LU.
	 */
	if (!ufs_is_valid_unit_desc_lun(&hba->dev_info, lun))
		return -EOPNOTSUPP;

	return os44_mi_ufshcd_read_desc_param(hba, QUERY_DESC_IDN_UNIT, lun,
				      param_offset, param_read_buf, param_size);
}

static inline bool local_ufshcd_is_wb_buf_lifetime_available(struct ufs_hba *hba)
{
	u32 lifetime;
	int ret;
	u8 index;

	index = ufshcd_wb_get_query_index(hba);
	ret = local_ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
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

static inline void local_ufshcd_wb_probe(struct ufs_hba *hba, const u8 *desc_buf)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u8 lun;
	u32 d_lu_wb_buf_alloc;
	u32 ext_ufs_feature;

	if (!ufshcd_is_wb_allowed(hba))
		return;

	/*
	 * Probe WB only for UFS-2.2 and UFS-3.1 (and later) devices or
	 * UFS devices with quirk UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES
	 * enabled
	 */
	if (!(dev_info->wspecversion >= 0x310 ||
	      dev_info->wspecversion == 0x220 ||
	     (hba->dev_quirks & UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES)))
		goto wb_disabled;

	ext_ufs_feature = get_unaligned_be32(desc_buf +
					DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);

	if (!(ext_ufs_feature & UFS_DEV_WRITE_BOOSTER_SUP))
		goto wb_disabled;

	/*
	 * WB may be supported but not configured while provisioning. The spec
	 * says, in dedicated wb buffer mode, a max of 1 lun would have wb
	 * buffer configured.
	 */
	dev_info->wb_buffer_type = desc_buf[DEVICE_DESC_PARAM_WB_TYPE];

	dev_info->b_presrv_uspc_en =
		desc_buf[DEVICE_DESC_PARAM_WB_PRESRV_USRSPC_EN];

	if (dev_info->wb_buffer_type == WB_BUF_MODE_SHARED) {
		if (!get_unaligned_be32(desc_buf +
				   DEVICE_DESC_PARAM_WB_SHARED_ALLOC_UNITS))
			goto wb_disabled;
	} else {
		for (lun = 0; lun < UFS_UPIU_MAX_WB_LUN_ID; lun++) {
			d_lu_wb_buf_alloc = 0;
			local_ufshcd_read_unit_desc_param(hba,
					lun,
					UNIT_DESC_PARAM_WB_BUF_ALLOC_UNITS,
					(u8 *)&d_lu_wb_buf_alloc,
					sizeof(d_lu_wb_buf_alloc));
			if (d_lu_wb_buf_alloc) {
				dev_info->wb_dedicated_lu = lun;
				break;
			}
		}

		if (!d_lu_wb_buf_alloc)
			goto wb_disabled;
	}

	if (!local_ufshcd_is_wb_buf_lifetime_available(hba))
		goto wb_disabled;

	return;

wb_disabled:
	hba->caps &= ~UFSHCD_CAP_WB_EN;
}

static inline void local_ufs_fixup_device_setup(struct ufs_hba *hba)
{
	/* fix by general quirk table */
	os44_ufshcd_fixup_dev_quirks(hba, os44_ufs_fixups);

	/* allow vendors to fix quirks */
	ufshcd_vops_fixup_dev_quirks(hba);
}

static inline void local_ufshcd_temp_notif_probe(struct ufs_hba *hba, const u8 *desc_buf)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u32 ext_ufs_feature;
	u8 mask = 0;

	if (!(hba->caps & UFSHCD_CAP_TEMP_NOTIF) || dev_info->wspecversion < 0x300)
		return;

	ext_ufs_feature = get_unaligned_be32(desc_buf + DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);

	if (ext_ufs_feature & UFS_DEV_LOW_TEMP_NOTIF)
		mask |= MASK_EE_TOO_LOW_TEMP;

	if (ext_ufs_feature & UFS_DEV_HIGH_TEMP_NOTIF)
		mask |= MASK_EE_TOO_HIGH_TEMP;

	if (mask) {
		local_enable_ee(hba, mask);
	}
}

static inline void local_ufshcd_device_lvl_exception_probe(struct ufs_hba *hba, u8 *desc_buf)
{
	u32 ext_ufs_feature;

	if (hba->dev_info.wspecversion < 0x410)
		return;

	ext_ufs_feature = get_unaligned_be32(desc_buf +
				DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);
	if (!(ext_ufs_feature & UFS_DEV_LVL_EXCEPTION_SUP))
		return;

	atomic_set(&hba->dev_lvl_exception_count, 0);
	local_enable_ee(hba, MASK_EE_DEV_LVL_EXCEPTION);
}

static inline void local_ufs_init_rtc(struct ufs_hba *hba, u8 *desc_buf)
{
	u16 periodic_rtc_update = get_unaligned_be16(&desc_buf[DEVICE_DESC_PARAM_FRQ_RTC]);
	struct ufs_dev_info *dev_info = &hba->dev_info;

	if (periodic_rtc_update & UFS_RTC_TIME_BASELINE) {
		dev_info->rtc_type = UFS_RTC_ABSOLUTE;

		/*
		 * The concept of measuring time in Linux as the number of seconds elapsed since
		 * 00:00:00 UTC on January 1, 1970, and UFS ABS RTC is elapsed from January 1st
		 * 2010 00:00, here we need to adjust ABS baseline.
		 */
		dev_info->rtc_time_baseline = mktime64(2010, 1, 1, 0, 0, 0) -
							mktime64(1970, 1, 1, 0, 0, 0);
	} else {
		dev_info->rtc_type = UFS_RTC_RELATIVE;
		dev_info->rtc_time_baseline = 0;
	}

	/*
	 * We ignore TIME_PERIOD defined in wPeriodicRTCUpdate because Spec does not clearly state
	 * how to calculate the specific update period for each time unit. And we disable periodic
	 * RTC update work, let user configure by sysfs node according to specific circumstance.
	 */
	dev_info->rtc_update_period = 0;
}

static inline void local_ufshcd_ext_iid_probe(struct ufs_hba *hba, u8 *desc_buf)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u32 ext_ufs_feature;
	u32 ext_iid_en = 0;
	int err;

	/* Only UFS-4.0 and above may support EXT_IID */
	if (dev_info->wspecversion < 0x400)
		goto out;

	ext_ufs_feature = get_unaligned_be32(desc_buf +
				     DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);
	if (!(ext_ufs_feature & UFS_DEV_EXT_IID_SUP))
		goto out;

	err = local_ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				      QUERY_ATTR_IDN_EXT_IID_EN, 0, 0, &ext_iid_en);
	if (err)
		dev_err(hba->dev, "failed reading bEXTIIDEn. err = %d\n", err);

out:
	dev_info->b_ext_iid_en = ext_iid_en;
}

static inline int local_ufs_get_device_desc(struct ufs_hba *hba)
{
	int err;
	u8 model_index;
	u8 *desc_buf;
	struct ufs_dev_info *dev_info = &hba->dev_info;
	

	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf) {
		err = -ENOMEM;
		goto out;
	}

	err = os44_mi_ufshcd_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0, 0, desc_buf,
				     QUERY_DESC_MAX_SIZE);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading Device Desc. err = %d\n",
			"ufs_get_device_desc", err);
		goto out;
	}

	/*
	 * getting vendor (manufacturerID) and Bank Index in big endian
	 * format
	 */
	dev_info->wmanufacturerid = desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];

	/* getting Specification Version in big endian format */
	dev_info->wspecversion = desc_buf[DEVICE_DESC_PARAM_SPEC_VER] << 8 |
				      desc_buf[DEVICE_DESC_PARAM_SPEC_VER + 1];
	dev_info->bqueuedepth = desc_buf[DEVICE_DESC_PARAM_Q_DPTH];


	((struct os44_dev_desc_tail_view *)dev_info)->field_4d =
        get_unaligned_be16(desc_buf + 0x4d);
	dev_info->rtt_cap = desc_buf[DEVICE_DESC_PARAM_RTT_CAP];

	model_index = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];

	err = os44_ufshcd_read_string_desc(hba, model_index,
				      &dev_info->model, SD_ASCII_STD);
	if (err < 0) {
		dev_err(hba->dev, "%s: Failed reading Product Name. err = %d\n",
			"ufs_get_device_desc", err);
		goto out;
	}

	hba->luns_avail = desc_buf[DEVICE_DESC_PARAM_NUM_LU] +
		desc_buf[DEVICE_DESC_PARAM_NUM_WLU];

	local_ufs_fixup_device_setup(hba);

	local_ufshcd_wb_probe(hba, desc_buf);

	local_ufshcd_temp_notif_probe(hba, desc_buf);

	if (dev_info->wspecversion >= 0x410) {
		hba->critical_health_count = 0;
		local_enable_ee(hba, MASK_EE_HEALTH_CRITICAL);
	}

	local_ufs_init_rtc(hba, desc_buf);

	if (hba->ext_iid_sup)
		local_ufshcd_ext_iid_probe(hba, desc_buf);

	local_ufshcd_device_lvl_exception_probe(hba, desc_buf);

	/*
	 * os44_ufshcd_read_string_desc returns size of the string
	 * reset the error value
	 */
	err = 0;

out:
	kfree(desc_buf);
	return err;
}

static inline int local_ufshcd_device_geo_params_init(struct ufs_hba *hba)
{
	int err;
	u8 *desc_buf;

	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf) {
		err = -ENOMEM;
		goto out;
	}

	err = os44_mi_ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, 0,
				     desc_buf, QUERY_DESC_MAX_SIZE);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading Geometry Desc. err = %d\n",
				"ufshcd_device_geo_params_init", err);
		goto out;
	}

	if (desc_buf[GEOMETRY_DESC_PARAM_MAX_NUM_LUN] == 1)
		hba->dev_info.max_lu_supported = 32;
	else if (desc_buf[GEOMETRY_DESC_PARAM_MAX_NUM_LUN] == 0)
		hba->dev_info.max_lu_supported = 8;

out:
	kfree(desc_buf);
	return err;
}

static inline void local_ufshcd_set_rtt(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u32 rtt = 0;
	u32 dev_rtt = 0;
	int host_rtt_cap = hba->vops && hba->vops->max_num_rtt ?
			   hba->vops->max_num_rtt : hba->nortt;

	/* RTT override makes sense only for UFS-4.0 and above */
	if (dev_info->wspecversion < 0x400)
		return;

	if (local_ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				    QUERY_ATTR_IDN_MAX_NUM_OF_RTT, 0, 0, &dev_rtt)) {
		dev_err(hba->dev, "failed reading bMaxNumOfRTT\n");
		return;
	}

	/* do not override if it was already written */
	if (dev_rtt != DEFAULT_MAX_NUM_RTT)
		return;

	rtt = min_t(int, dev_info->rtt_cap, host_rtt_cap);

	if (rtt == dev_rtt)
		return;

	if (local_ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
				    QUERY_ATTR_IDN_MAX_NUM_OF_RTT, 0, 0, &rtt))
		dev_err(hba->dev, "failed writing bMaxNumOfRTT\n");
}

static inline int local_ufshcd_get_ref_clk_gating_wait(struct ufs_hba *hba)
{
	int err = 0;
	u32 gating_wait = UFSHCD_REF_CLK_GATING_WAIT_US;

	if (hba->dev_info.wspecversion >= 0x300) {
		err = local_ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				QUERY_ATTR_IDN_REF_CLK_GATING_WAIT_TIME, 0, 0,
				&gating_wait);
		if (err)
			dev_err(hba->dev, "Failed reading bRefClkGatingWait. err = %d, use default %uus\n",
					 err, gating_wait);

		if (gating_wait == 0) {
			gating_wait = UFSHCD_REF_CLK_GATING_WAIT_US;
			dev_err(hba->dev, "Undefined ref clk gating wait time, use default %uus\n",
					 gating_wait);
		}

		hba->dev_info.clk_gating_wait_us = gating_wait;
	}

	return err;
}

static inline int local_ufshcd_get_max_pwr_mode(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr *pwr_info = &hba->max_pwr_info.info;

	if (hba->max_pwr_info.is_valid)
		return 0;

	if (hba->quirks & UFSHCD_QUIRK_HIBERN_FASTAUTO) {
		pwr_info->pwr_tx = FASTAUTO_MODE;
		pwr_info->pwr_rx = FASTAUTO_MODE;
	} else {
		pwr_info->pwr_tx = FAST_MODE;
		pwr_info->pwr_rx = FAST_MODE;
	}
	pwr_info->hs_rate = PA_HS_MODE_B;

	/* Get the connected lane count */
	local_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&pwr_info->lane_rx);
	local_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&pwr_info->lane_tx);

	if (!pwr_info->lane_rx || !pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes value. rx=%d, tx=%d\n",
				"ufshcd_get_max_pwr_mode",
				pwr_info->lane_rx,
				pwr_info->lane_tx);
		return -EINVAL;
	}

	/*
	 * First, get the maximum gears of HS speed.
	 * If a zero value, it means there is no HSGEAR capability.
	 * Then, get the maximum gears of PWM speed.
	 */
	local_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &pwr_info->gear_rx);
	if (!pwr_info->gear_rx) {
		local_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_rx);
		if (!pwr_info->gear_rx) {
			dev_err(hba->dev, "%s: invalid max pwm rx gear read = %d\n",
				"ufshcd_get_max_pwr_mode", pwr_info->gear_rx);
			return -EINVAL;
		}
		pwr_info->pwr_rx = SLOW_MODE;
	}

	local_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
			&pwr_info->gear_tx);
	if (!pwr_info->gear_tx) {
		local_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_tx);
		if (!pwr_info->gear_tx) {
			dev_err(hba->dev, "%s: invalid max pwm tx gear read = %d\n",
				"ufshcd_get_max_pwr_mode", pwr_info->gear_tx);
			return -EINVAL;
		}
		pwr_info->pwr_tx = SLOW_MODE;
	}

	hba->max_pwr_info.is_valid = true;
	return 0;
}

int os44_ufshcd_device_params_init(struct ufs_hba *hba)
{
	bool flag = false;
	struct os44_fastdiscard_oem_view *oem;
	int ret;

	oem = kmalloc(sizeof(*oem), GFP_KERNEL);
	if (!oem) {
		pr_err("%s: Failed to allocate memory for xiaomi_feature_info\n", "ufshcd_device_params_init");
		return -ENOMEM;
	}
	memset(oem, 0, sizeof(*oem));
	hba->android_oem_data1 = (unsigned long)oem;
	/* Init UFS geometry descriptor related parameters */
	ret = local_ufshcd_device_geo_params_init(hba);
	if (ret)
		goto out;

	os44_fastdiscard_init(hba);

	/* Check and apply UFS device quirks */
	ret = local_ufs_get_device_desc(hba);
	if (ret) {
		dev_err(hba->dev, "%s: Failed getting device info. err = %d\n",
			"ufshcd_device_params_init", ret);
		goto out;
	}

	local_ufshcd_set_rtt(hba);

	local_ufshcd_get_ref_clk_gating_wait(hba);

	if (!local_ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_READ_FLAG,
			QUERY_FLAG_IDN_PWR_ON_WPE, 0, &flag))
		hba->dev_info.f_power_on_wp_en = flag;

	/* Probe maximum power mode co-supported by both UFS host and device */
	if (local_ufshcd_get_max_pwr_mode(hba))
		dev_err(hba->dev,
			"%s: Failed getting max supported power mode\n",
			"ufshcd_device_params_init");
out:
	return ret;
}
