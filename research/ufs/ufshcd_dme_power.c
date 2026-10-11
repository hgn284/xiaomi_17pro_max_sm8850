/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent stock DME and power consumers; real UIC/clock owners. */
#include <linux/module.h>
#include "ufsswap_provider_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
#include "os44_uic_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#define UFS_UIC_COMMAND_RETRIES 3 /* ufshcd.c:82; all three sends visible in stock */
static inline void os44_uic_release(struct ufs_hba *hba)
{
	guard(spinlock_irqsave)(&hba->clk_gating.lock);
	os44_core_release_locked(hba);
}

int os44_ufshcd_dme_set_attr(struct ufs_hba *hba, u32 attr_sel,
			u8 attr_set, u32 mib_val, u8 peer)
{
	struct uic_command uic_cmd = {
		.command = peer ? UIC_CMD_DME_PEER_SET : UIC_CMD_DME_SET,
		.argument1 = attr_sel,
		.argument2 = UIC_ARG_ATTR_TYPE(attr_set),
		.argument3 = mib_val,
	};
	static const char *const action[] = {
		"dme-set",
		"dme-peer-set"
	};
	const char *set = action[!!peer];
	int ret;
	int retries = UFS_UIC_COMMAND_RETRIES;

	do {
		/* for peer attributes we retry upon failure */
		ret = os44_ufshcd_send_uic_cmd(hba, &uic_cmd);
		if (ret)
			dev_dbg(hba->dev, "%s: attr-id 0x%x val 0x%x error code %d\n",
				set, UIC_GET_ATTR_ID(attr_sel), mib_val, ret);
	} while (ret && peer && --retries);

	if (ret)
		dev_err(hba->dev, "%s: attr-id 0x%x val 0x%x failed %d retries\n",
			set, UIC_GET_ATTR_ID(attr_sel), mib_val,
			UFS_UIC_COMMAND_RETRIES - retries);

	return ret;
}

static inline int os44_dme_set(struct ufs_hba *hba, u32 attr_sel, u32 mib_val) { return os44_ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_NOR, mib_val, DME_LOCAL); }

int os44_ufshcd_uic_change_pwr_mode(struct ufs_hba *hba, u8 mode)
{
	struct uic_command uic_cmd = {
		.command = UIC_CMD_DME_SET,
		.argument1 = UIC_ARG_MIB(PA_PWRMODE),
		.argument3 = mode,
	};
	int ret;

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP) {
		ret = os44_dme_set(hba,
				UIC_ARG_MIB_SEL(PA_RXHSUNTERMCAP, 0), 1);
		if (ret) {
			dev_err(hba->dev, "%s: failed to enable PA_RXHSUNTERMCAP ret %d\n",
						"ufshcd_uic_change_pwr_mode", ret);
			goto out;
		}
	}

	os44_core_hold(hba);
	ret = os44_ufshcd_uic_pwr_ctrl(hba, &uic_cmd);
	os44_uic_release(hba);

out:
	return ret;
}

int os44_ufshcd_dme_configure_adapt(struct ufs_hba *hba,
			       int agreed_gear,
			       int adapt_val)
{
	int ret;

	if (agreed_gear < UFS_HS_G4)
		adapt_val = PA_NO_ADAPT;

	ret = os44_dme_set(hba,
			     UIC_ARG_MIB(PA_TXHSADAPTTYPE),
			     adapt_val);
	return ret;
}

int os44_ufshcd_dme_get_attr(struct ufs_hba *hba, u32 attr_sel,
			u32 *mib_val, u8 peer)
{
	struct uic_command uic_cmd = {
		.command = peer ? UIC_CMD_DME_PEER_GET : UIC_CMD_DME_GET,
		.argument1 = attr_sel,
	};
	static const char *const action[] = {
		"dme-get",
		"dme-peer-get"
	};
	const char *get = action[!!peer];
	int ret;
	int retries = UFS_UIC_COMMAND_RETRIES;
	struct ufs_pa_layer_attr orig_pwr_info;
	struct ufs_pa_layer_attr temp_pwr_info;
	bool pwr_mode_change = false;

	if (peer && (hba->quirks & UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE)) {
		orig_pwr_info = hba->pwr_info;
		temp_pwr_info = orig_pwr_info;

		if (orig_pwr_info.pwr_tx == FAST_MODE ||
		    orig_pwr_info.pwr_rx == FAST_MODE) {
			temp_pwr_info.pwr_tx = FASTAUTO_MODE;
			temp_pwr_info.pwr_rx = FASTAUTO_MODE;
			pwr_mode_change = true;
		} else if (orig_pwr_info.pwr_tx == SLOW_MODE ||
		    orig_pwr_info.pwr_rx == SLOW_MODE) {
			temp_pwr_info.pwr_tx = SLOWAUTO_MODE;
			temp_pwr_info.pwr_rx = SLOWAUTO_MODE;
			pwr_mode_change = true;
		}
		if (pwr_mode_change) {
			ret = os44_ufshcd_change_power_mode(hba, &temp_pwr_info);
			if (ret)
				goto out;
		}
	}

	do {
		/* for peer attributes we retry upon failure */
		ret = os44_ufshcd_send_uic_cmd(hba, &uic_cmd);
		if (ret)
			dev_dbg(hba->dev, "%s: attr-id 0x%x error code %d\n",
				get, UIC_GET_ATTR_ID(attr_sel), ret);
	} while (ret && peer && --retries);

	if (ret)
		dev_err(hba->dev, "%s: attr-id 0x%x failed %d retries\n",
			get, UIC_GET_ATTR_ID(attr_sel),
			UFS_UIC_COMMAND_RETRIES - retries);

	if (!ret && mib_val)
		*mib_val = uic_cmd.argument3;

	if (peer && (hba->quirks & UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE)
	    && pwr_mode_change)
		os44_ufshcd_change_power_mode(hba, &orig_pwr_info);
out:
	return ret;
}

int os44_ufshcd_change_power_mode(struct ufs_hba *hba,
			     struct ufs_pa_layer_attr *pwr_mode)
{
	int ret;

	/* if already configured to the requested pwr_mode */
	if (!hba->force_pmc &&
	    pwr_mode->gear_rx == hba->pwr_info.gear_rx &&
	    pwr_mode->gear_tx == hba->pwr_info.gear_tx &&
	    pwr_mode->lane_rx == hba->pwr_info.lane_rx &&
	    pwr_mode->lane_tx == hba->pwr_info.lane_tx &&
	    pwr_mode->pwr_rx == hba->pwr_info.pwr_rx &&
	    pwr_mode->pwr_tx == hba->pwr_info.pwr_tx &&
	    pwr_mode->hs_rate == hba->pwr_info.hs_rate) {
		dev_dbg(hba->dev, "%s: power already configured\n", "ufshcd_change_power_mode");
		return 0;
	}

	/*
	 * Configure attributes for power mode change with below.
	 * - PA_RXGEAR, PA_ACTIVERXDATALANES, PA_RXTERMINATION,
	 * - PA_TXGEAR, PA_ACTIVETXDATALANES, PA_TXTERMINATION,
	 * - PA_HSSERIES
	 */
	os44_dme_set(hba, UIC_ARG_MIB(PA_RXGEAR), pwr_mode->gear_rx);
	os44_dme_set(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			pwr_mode->lane_rx);
	if (pwr_mode->pwr_rx == FASTAUTO_MODE ||
			pwr_mode->pwr_rx == FAST_MODE)
		os44_dme_set(hba, UIC_ARG_MIB(PA_RXTERMINATION), true);
	else
		os44_dme_set(hba, UIC_ARG_MIB(PA_RXTERMINATION), false);

	os44_dme_set(hba, UIC_ARG_MIB(PA_TXGEAR), pwr_mode->gear_tx);
	os44_dme_set(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			pwr_mode->lane_tx);
	if (pwr_mode->pwr_tx == FASTAUTO_MODE ||
			pwr_mode->pwr_tx == FAST_MODE)
		os44_dme_set(hba, UIC_ARG_MIB(PA_TXTERMINATION), true);
	else
		os44_dme_set(hba, UIC_ARG_MIB(PA_TXTERMINATION), false);

	if (pwr_mode->pwr_rx == FASTAUTO_MODE ||
	    pwr_mode->pwr_tx == FASTAUTO_MODE ||
	    pwr_mode->pwr_rx == FAST_MODE ||
	    pwr_mode->pwr_tx == FAST_MODE)
		os44_dme_set(hba, UIC_ARG_MIB(PA_HSSERIES),
						pwr_mode->hs_rate);

	if (!(hba->quirks & UFSHCD_QUIRK_SKIP_DEF_UNIPRO_TIMEOUT_SETTING)) {
		os44_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA0),
				DL_FC0ProtectionTimeOutVal_Default);
		os44_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA1),
				DL_TC0ReplayTimeOutVal_Default);
		os44_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA2),
				DL_AFC0ReqTimeOutVal_Default);
		os44_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA3),
				DL_FC1ProtectionTimeOutVal_Default);
		os44_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA4),
				DL_TC1ReplayTimeOutVal_Default);
		os44_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA5),
				DL_AFC1ReqTimeOutVal_Default);

		os44_dme_set(hba, UIC_ARG_MIB(DME_LocalFC0ProtectionTimeOutVal),
				DL_FC0ProtectionTimeOutVal_Default);
		os44_dme_set(hba, UIC_ARG_MIB(DME_LocalTC0ReplayTimeOutVal),
				DL_TC0ReplayTimeOutVal_Default);
		os44_dme_set(hba, UIC_ARG_MIB(DME_LocalAFC0ReqTimeOutVal),
				DL_AFC0ReqTimeOutVal_Default);
	}

	ret = os44_ufshcd_uic_change_pwr_mode(hba, pwr_mode->pwr_rx << 4
			| pwr_mode->pwr_tx);

	if (ret) {
		os44_query_error(0, 5, -1, 0);
		dev_err(hba->dev,
			"%s: power mode change failed %d\n", "ufshcd_change_power_mode", ret);
	} else {
		memcpy(&hba->pwr_info, pwr_mode,
			sizeof(struct ufs_pa_layer_attr));
	}

	return ret;
}
