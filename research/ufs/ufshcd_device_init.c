// SPDX-License-Identifier: GPL-2.0-only
/* OS44 whole device-init reconstruction; stock-specific errors and retry policy.
 * Public source scaffolding, real typed provider calls. No fabricated owners. */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <trace/hooks/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_hba_operational.h"
#include "os44_uic_contract.h"
#include "os44_bkops_contract.h"
#include "os44_pm_contract.h"
#include "os44_hba_enable_contract.h"
#include "os44_device_init_helpers.h"
#include "os44_device_params.h"
#include "os44_init_controls.h"
#include "os44_mcq_allocation.h"
#include "os44_device_init.h"

#define QUERY_REQ_RETRIES 3
#define UFS_UIC_COMMAND_RETRIES 3
#define DME_LINKSTARTUP_RETRIES 3
#define FDEVICEINIT_COMPL_TIMEOUT 1500
void os44_query_error(unsigned int, unsigned int, int, unsigned int);
bool os44_use_mcq_mode = true;
module_param_named(use_mcq_mode, os44_use_mcq_mode, bool, 0644);
MODULE_PARM_DESC(use_mcq_mode, "Control MCQ mode for controllers starting from UFSHCI 4.0. 1 - enable MCQ, 0 - disable MCQ. MCQ is enabled by default");
static_assert(offsetof(struct ufs_hba,ufs_stats.event[UFS_EVT_LINK_STARTUP_FAIL])==0x5e0);
static_assert(offsetof(struct ufs_hba_variant_ops,link_startup_notify)==0x48);
static_assert(offsetof(struct ufs_hba_variant_ops,event_notify)==0xc0);
static bool is_mcq_supported(struct ufs_hba *hba)
{
	return hba->mcq_sup && os44_use_mcq_mode;
}

static inline bool ufshcd_is_device_present(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_CONTROLLER_STATUS) & DEVICE_PRESENT;
}

static void local_ufshcd_update_evt_hist(struct ufs_hba *hba, u32 id, u32 val)
{
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];
	e->val[e->pos] = val;
	e->tstamp[e->pos] = local_clock();
	e->cnt += 1;
	e->pos = (e->pos + 1) % UFS_EVENT_HIST_LENGTH;

	ufshcd_vops_event_notify(hba, id, &val);
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

static int local_ufshcd_dme_set_attr(struct ufs_hba *hba, u32 attr_sel,
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

static inline int local_set(struct ufs_hba *h,u32 a,u32 v) { return local_ufshcd_dme_set_attr(h,a,ATTR_SET_NOR,v,DME_LOCAL); }
static inline int local_peer_set(struct ufs_hba *h,u32 a,u32 v) { return local_ufshcd_dme_set_attr(h,a,ATTR_SET_NOR,v,DME_PEER); }
static int ufshcd_disable_tx_lcc(struct ufs_hba *hba, bool peer)
{
	int tx_lanes = 0, i, err = 0;

	if (!peer)
		os44_ufshcd_dme_get_attr(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES), &tx_lanes, DME_LOCAL);
	else
		os44_ufshcd_dme_get_attr(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES), &tx_lanes, DME_PEER);
	for (i = 0; i < tx_lanes; i++) {
		if (!peer)
			err = local_set(hba,
				UIC_ARG_MIB_SEL(TX_LCC_ENABLE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(i)),
					0);
		else
			err = local_peer_set(hba,
				UIC_ARG_MIB_SEL(TX_LCC_ENABLE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(i)),
					0);
		if (err) {
			dev_err(hba->dev, "%s: TX LCC Disable failed, peer = %d, lane = %d, err = %d",
				"ufshcd_disable_tx_lcc", peer, i, err);
			break;
		}
	}

	return err;
}

static inline int ufshcd_disable_device_tx_lcc(struct ufs_hba *hba)
{
	return ufshcd_disable_tx_lcc(hba, true);
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

static void ufshcd_init_pwr_info(struct ufs_hba *hba)
{
	hba->pwr_info.gear_rx = UFS_PWM_G1;
	hba->pwr_info.gear_tx = UFS_PWM_G1;
	hba->pwr_info.lane_rx = UFS_LANE_1;
	hba->pwr_info.lane_tx = UFS_LANE_1;
	hba->pwr_info.pwr_rx = SLOWAUTO_MODE;
	hba->pwr_info.pwr_tx = SLOWAUTO_MODE;
	hba->pwr_info.hs_rate = 0;
}

static int ufshcd_dme_link_startup(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {
		.command = UIC_CMD_DME_LINK_STARTUP,
	};
	int ret;

	ret = os44_ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		os44_query_error(0, 6, -1, 0);
	if (ret)
		dev_dbg(hba->dev,
			"dme-link-startup: error code %d\n", ret);
	return ret;
}

static int ufshcd_link_startup(struct ufs_hba *hba)
{
	int ret;
	int retries = DME_LINKSTARTUP_RETRIES;
	bool link_startup_again = false;

	/*
	 * If UFS device isn't active then we will have to issue link startup
	 * 2 times to make sure the device state move to active.
	 */
	if (!ufshcd_is_ufs_dev_active(hba))
		link_startup_again = true;

link_startup:
	do {
		ufshcd_vops_link_startup_notify(hba, PRE_CHANGE);

		ret = ufshcd_dme_link_startup(hba);

		/* check if device is detected by inter-connect layer */
		if (!ret && !ufshcd_is_device_present(hba)) {
			local_ufshcd_update_evt_hist(hba,
					       UFS_EVT_LINK_STARTUP_FAIL,
					       0);
			os44_query_error(0, 7, -1, 0);
			dev_err(hba->dev, "%s: Device not present\n", "ufshcd_link_startup");
			ret = -ENXIO;
			goto out;
		}

		/*
		 * DME link lost indication is only received when link is up,
		 * but we can't be sure if the link is up until link startup
		 * succeeds. So reset the local Uni-Pro and try again.
		 */
		if (ret && retries && os44_ufshcd_hba_enable(hba)) {
			local_ufshcd_update_evt_hist(hba,
					       UFS_EVT_LINK_STARTUP_FAIL,
					       (u32)ret);
			goto out;
		}
	} while (ret && retries--);

	if (ret) {
		/* failed to get the link up... retire */
		local_ufshcd_update_evt_hist(hba,
				       UFS_EVT_LINK_STARTUP_FAIL,
				       (u32)ret);
		goto out;
	}

	if (link_startup_again) {
		link_startup_again = false;
		retries = DME_LINKSTARTUP_RETRIES;
		goto link_startup;
	}

	/* Mark that link is up in PWM-G1, 1-lane, SLOW-AUTO mode */
	ufshcd_init_pwr_info(hba);
	ufshcd_print_pwr_info(hba);

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_LCC) {
		ret = ufshcd_disable_device_tx_lcc(hba);
		if (ret)
			goto out;
	}

	/* Include any host controller configuration via UIC commands */
	ret = ufshcd_vops_link_startup_notify(hba, POST_CHANGE);
	if (ret)
		goto out;

	/* Clear UECPA once due to LINERESET has happened during LINK_STARTUP */
	ufshcd_readl(hba, REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER);
	ret = os44_ufshcd_make_hba_operational(hba);
out:
	if (ret) {
		dev_err(hba->dev, "link startup failed %d\n", ret);
		os44_ufshcd_print_host_state(hba);
		ufshcd_print_pwr_info(hba);
		os44_ufshcd_print_evt_hist(hba);
	}
	trace_android_vh_ufs_link_startup(hba, ret);
	return ret;
}

static void ufshcd_clear_dbg_ufs_stats(struct ufs_hba *hba)
{
	hba->ufs_stats.hibern8_exit_cnt = 0;
	hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
	hba->req_abort_count = 0;
}

static int ufshcd_complete_dev_init(struct ufs_hba *hba)
{
	int err;
	bool flag_res = true;
	ktime_t timeout;

	err = local_ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
		QUERY_FLAG_IDN_FDEVICEINIT, 0, NULL);
	if (err) {
		dev_err(hba->dev,
			"%s: setting fDeviceInit flag failed with error %d\n",
			"ufshcd_complete_dev_init", err);
		goto out;
	}

	/* Poll fDeviceInit flag to be cleared */
	timeout = ktime_add_ms(ktime_get(), FDEVICEINIT_COMPL_TIMEOUT);
	do {
		err = os44_ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG,
					QUERY_FLAG_IDN_FDEVICEINIT, 0, &flag_res);
		if (!flag_res)
			break;
		usleep_range(500, 1000);
	} while (ktime_before(ktime_get(), timeout));

	if (err) {
		dev_err(hba->dev,
				"%s: reading fDeviceInit flag failed with error %d\n",
				"ufshcd_complete_dev_init", err);
	} else if (flag_res) {
		dev_err(hba->dev,
				"%s: fDeviceInit was not cleared by the device\n",
				"ufshcd_complete_dev_init");
		err = -EBUSY;
	}
out:
	return err;
}

int os44_ufshcd_device_init(struct ufs_hba *hba, bool init_dev_params)
{
	int ret;
	struct Scsi_Host *host = hba->host;

	hba->ufshcd_state = UFSHCD_STATE_RESET;

	ret = ufshcd_link_startup(hba);
	if (ret)
		return ret;

	if (hba->quirks & UFSHCD_QUIRK_SKIP_PH_CONFIGURATION)
		return ret;

	/* Debug counters initialization */
	ufshcd_clear_dbg_ufs_stats(hba);

	/* UniPro link is active now */
	ufshcd_set_link_active(hba);

	/* Reconfigure MCQ upon reset */
	if (hba->mcq_enabled && !init_dev_params) {
		os44_ufshcd_config_mcq(hba);
		os44_ufshcd_mcq_enable(hba);
	}

	/* Verify device initialization by sending NOP OUT UPIU */
	ret = os44_ufshcd_verify_dev_init(hba);
	if (ret)
		return ret;

	/* Initiate UFS initialization, and waiting until completion */
	ret = ufshcd_complete_dev_init(hba);
	if (ret)
		return ret;

	/*
	 * Initialize UFS device parameters used by driver, these
	 * parameters are associated with UFS descriptors.
	 */
	if (init_dev_params) {
		ret = os44_ufshcd_device_params_init(hba);
		if (ret)
			return ret;
		if (is_mcq_supported(hba) && !hba->scsi_host_added) {
			os44_ufshcd_mcq_enable(hba);
			ret = os44_ufshcd_alloc_mcq(hba);
			if (!ret) {
				os44_ufshcd_config_mcq(hba);
			} else {
				/* Continue with SDB mode */
				os44_ufshcd_mcq_disable(hba);
				os44_use_mcq_mode = false;
				dev_err(hba->dev, "MCQ mode is disabled, err=%d\n",
					 ret);
			}
			ret = scsi_add_host(host, hba->dev);
			if (ret) {
				dev_err(hba->dev, "scsi_add_host failed\n");
				return ret;
			}
			hba->scsi_host_added = true;
		} else if (is_mcq_supported(hba)) {
			/* UFSHCD_QUIRK_REINIT_AFTER_MAX_GEAR_SWITCH is set */
			os44_ufshcd_config_mcq(hba);
			os44_ufshcd_mcq_enable(hba);
		}
	}

	os44_ufshcd_tune_unipro_params(hba);

	/* UFS device is also active now */
	ufshcd_set_ufs_dev_active(hba);
	os44_ufshcd_force_reset_auto_bkops(hba);

	os44_ufshcd_set_timestamp_attr(hba);

	/* Gear up to HS gear if supported */
	if (hba->max_pwr_info.is_valid) {
		/*
		 * Set the right value to bRefClkFreq before attempting to
		 * switch to HS gears.
		 */
		if (hba->dev_ref_clk_freq != REF_CLK_FREQ_INVAL)
			os44_ufshcd_set_dev_ref_clk(hba);
		ret = os44_ufshcd_config_pwr_mode(hba, &hba->max_pwr_info.info);
		if (ret) {
			dev_err(hba->dev, "%s: Failed setting power mode, err = %d\n",
					"ufshcd_device_init", ret);
			return ret;
		}
	}

	return 0;
}

