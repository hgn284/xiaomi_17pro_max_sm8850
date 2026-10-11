// SPDX-License-Identifier: GPL-2.0-or-later
/* OS44 whole init controls reconstructed from public scaffold and stock ELF.
 * No owner storage, callback cast or binary/type-word substitution. */
#include "os44_uic_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_init_controls.h"
#define UFS_UIC_COMMAND_RETRIES 3
/* Stock error-mask variant, not the later public UFSHCD_ERROR_MASK. */
#define UFSHCD_ENABLE_MCQ_INTRS 0x170a84
static_assert(offsetof(struct ufs_hba,vops)==0xa8);
static_assert(offsetof(struct ufs_hba,quirks)==0xd4);
static_assert(offsetof(struct ufs_hba,dev_quirks)==0xdc);
static_assert(offsetof(struct ufs_hba,mcq_enabled)==0x131d);
static_assert(offsetof(struct ufs_hba,nr_hw_queues)==0x1308);
static_assert(offsetof(struct ufs_hba_variant_ops,config_esi)==0xe8);
static_assert(offsetof(struct ufs_hba_variant_ops,apply_dev_quirks)==0x70);


static inline int local_set_attr(struct ufs_hba *hba, u32 attr_sel,
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

static inline int local_get_attr(struct ufs_hba *hba, u32 attr_sel,
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

	if (mib_val)
		*mib_val = ret == 0 ? uic_cmd.argument3 : 0;

	if (peer && (hba->quirks & UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE)
	    && pwr_mode_change)
		os44_ufshcd_change_power_mode(hba, &orig_pwr_info);
out:
	return ret;
}


static inline int local_set(struct ufs_hba *h,u32 a,u32 v) { return local_set_attr(h,a,ATTR_SET_NOR,v,DME_LOCAL); }
static inline int local_peer_set(struct ufs_hba *h,u32 a,u32 v) { return local_set_attr(h,a,ATTR_SET_NOR,v,DME_PEER); }
static inline int local_get(struct ufs_hba *h,u32 a,u32 *v) { return local_get_attr(h,a,v,DME_LOCAL); }
static inline int local_peer_get(struct ufs_hba *h,u32 a,u32 *v) { return os44_ufshcd_dme_get_attr(h,a,v,DME_PEER); }


static void ufshcd_enable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	set |= intrs;
	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

static int ufshcd_quirk_tune_host_pa_tactivate(struct ufs_hba *hba)
{
	int ret = 0;
	u32 granularity, peer_granularity;
	u32 pa_tactivate, peer_pa_tactivate;
	u32 pa_tactivate_us, peer_pa_tactivate_us;
	static const u8 gran_to_us_table[] = {1, 4, 8, 16, 32, 100};

	ret = local_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &granularity);
	if (ret)
		goto out;

	ret = local_peer_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &peer_granularity);
	if (ret)
		goto out;

	if ((granularity < PA_GRANULARITY_MIN_VAL) ||
	    (granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid host PA_GRANULARITY %d",
			"ufshcd_quirk_tune_host_pa_tactivate", granularity);
		return -EINVAL;
	}

	if ((peer_granularity < PA_GRANULARITY_MIN_VAL) ||
	    (peer_granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid device PA_GRANULARITY %d",
			"ufshcd_quirk_tune_host_pa_tactivate", peer_granularity);
		return -EINVAL;
	}

	ret = local_get(hba, UIC_ARG_MIB(PA_TACTIVATE), &pa_tactivate);
	if (ret)
		goto out;

	ret = local_peer_get(hba, UIC_ARG_MIB(PA_TACTIVATE),
				  &peer_pa_tactivate);
	if (ret)
		goto out;

	pa_tactivate_us = pa_tactivate * gran_to_us_table[granularity - 1];
	peer_pa_tactivate_us = peer_pa_tactivate *
			     gran_to_us_table[peer_granularity - 1];

	if (pa_tactivate_us >= peer_pa_tactivate_us) {
		u32 new_peer_pa_tactivate;

		new_peer_pa_tactivate = pa_tactivate_us /
				      gran_to_us_table[peer_granularity - 1];
		new_peer_pa_tactivate++;
		ret = local_peer_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
					  new_peer_pa_tactivate);
	}

out:
	return ret;
}

void os44_ufshcd_config_mcq(struct ufs_hba *hba)
{
	int ret;
	u32 intrs;

	ret = ufshcd_mcq_vops_config_esi(hba);
	dev_info(hba->dev, "ESI %sconfigured\n", ret ? "is not " : "");

	intrs = UFSHCD_ENABLE_MCQ_INTRS;
	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_INTR)
		intrs &= ~MCQ_CQ_EVENT_STATUS;
	ufshcd_enable_intr(hba, intrs);
	os44_ufshcd_mcq_make_queues_operational(hba);
	os44_ufshcd_mcq_config_mac(hba, hba->nutrs);

	dev_info(hba->dev, "MCQ configured, nr_queues=%d, io_queues=%d, read_queue=%d, poll_queues=%d, queue_depth=%d\n",
		 hba->nr_hw_queues, hba->nr_queues[HCTX_TYPE_DEFAULT],
		 hba->nr_queues[HCTX_TYPE_READ], hba->nr_queues[HCTX_TYPE_POLL],
		 hba->nutrs);
}

void os44_ufshcd_mcq_disable(struct ufs_hba *hba)
{
	ufshcd_rmwl(hba, MCQ_MODE_SELECT, 0, REG_UFS_MEM_CFG);
	hba->mcq_enabled = false;
}

void os44_ufshcd_tune_unipro_params(struct ufs_hba *hba)
{
	ufshcd_vops_apply_dev_quirks(hba);

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_PA_TACTIVATE)
		/* set 1ms timeout for PA_TACTIVATE */
		local_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 10);

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE)
		ufshcd_quirk_tune_host_pa_tactivate(hba);

}
