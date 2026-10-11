/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent stock diagnostic/power-control source, no skipped diagnostics. */
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "ufsswap_provider_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
#include "os44_uic_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#define ufshcd_hex_dump(prefix_str, buf, len) do {                       \
	size_t __len = (len);                                            \
	print_hex_dump(KERN_ERR, prefix_str,                             \
		       __len > 4 ? DUMP_PREFIX_OFFSET : DUMP_PREFIX_NONE,\
		       16, 4, buf, __len, false);                        \
} while (0)

bool os44_uicerr_flag; /* stock .bss one-byte owner; no reset in all4 refs */

static void ufshcd_print_clk_freqs(struct ufs_hba *hba)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		return;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk) && clki->min_freq &&
				clki->max_freq)
			dev_err(hba->dev, "clk: %s, rate: %u\n",
					clki->name, clki->curr_freq);
	}
}

void os44_ufshcd_print_host_state(struct ufs_hba *hba)
{
	const struct scsi_device *sdev_ufs = hba->ufs_device_wlun;

	dev_err(hba->dev, "UFS Host state=%d\n", hba->ufshcd_state);
	dev_err(hba->dev, "%d outstanding reqs, tasks=0x%lx\n",
		scsi_host_busy(hba->host), hba->outstanding_tasks);
	dev_err(hba->dev, "saved_err=0x%x, saved_uic_err=0x%x\n",
		hba->saved_err, hba->saved_uic_err);
	dev_err(hba->dev, "Device power mode=%d, UIC link state=%d\n",
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	dev_err(hba->dev, "PM in progress=%d, sys. suspended=%d\n",
		hba->pm_op_in_progress, hba->is_sys_suspended);
	dev_err(hba->dev, "Auto BKOPS=%d, Host self-block=%d\n",
		hba->auto_bkops_enabled, hba->host->host_self_blocked);
	dev_err(hba->dev, "Clk gate=%d\n", hba->clk_gating.state);
	dev_err(hba->dev,
		"last_hibern8_exit_tstamp at %lld us, hibern8_exit_cnt=%d\n",
		div_u64(hba->ufs_stats.last_hibern8_exit_tstamp, 1000),
		hba->ufs_stats.hibern8_exit_cnt);
	dev_err(hba->dev, "last intr at %lld us, last intr status=0x%x\n",
		div_u64(hba->ufs_stats.last_intr_ts, 1000),
		hba->ufs_stats.last_intr_status);
	dev_err(hba->dev, "error handling flags=0x%x, req. abort count=%d\n",
		hba->eh_flags, hba->req_abort_count);
	dev_err(hba->dev, "hba->ufs_version=0x%x, Host capabilities=0x%x, caps=0x%x\n",
		hba->ufs_version, hba->capabilities, hba->caps);
	dev_err(hba->dev, "quirks=0x%x, dev. quirks=0x%x\n", hba->quirks,
		hba->dev_quirks);
	if (sdev_ufs)
		dev_err(hba->dev, "UFS dev info: %.8s %.16s rev %.4s\n",
			sdev_ufs->vendor, sdev_ufs->model, sdev_ufs->rev);

	ufshcd_print_clk_freqs(hba);
}

int os44_ufshcd_dump_regs(struct ufs_hba *hba, size_t offset, size_t len,
		     const char *prefix)
{
	u32 *regs;
	size_t pos;

	if (offset % 4 != 0 || len % 4 != 0) /* keep readl happy */
		return -EINVAL;

	regs = kzalloc(len, GFP_ATOMIC);
	if (!regs)
		return -ENOMEM;

	for (pos = 0; pos < len; pos += 4) {
		if (offset == 0 &&
		    pos >= REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER &&
		    pos <= REG_UIC_ERROR_CODE_DME)
			continue;
		regs[pos / 4] = ufshcd_readl(hba, offset + pos);
	}

	ufshcd_hex_dump(prefix, regs, len);
	kfree(regs);

	return 0;
}

static void ufshcd_print_evt(struct ufs_hba *hba, u32 id,
			     const char *err_name)
{
	int i;
	bool found = false;
	const struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];

	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		if (e->tstamp[p] == 0)
			continue;
		dev_err(hba->dev, "%s[%d] = 0x%x at %lld us\n", err_name, p,
			e->val[p], div_u64(e->tstamp[p], 1000));
		found = true;
	}

	if (!found)
		dev_err(hba->dev, "No record of %s\n", err_name);
	else
		dev_err(hba->dev, "%s: total cnt=%llu\n", err_name, e->cnt);
}

void os44_ufshcd_print_evt_hist(struct ufs_hba *hba)
{
	os44_ufshcd_dump_regs(hba, 0, UFSHCI_REG_SPACE_SIZE, "host_regs: ");

	ufshcd_print_evt(hba, UFS_EVT_PA_ERR, "pa_err");
	ufshcd_print_evt(hba, UFS_EVT_DL_ERR, "dl_err");
	ufshcd_print_evt(hba, UFS_EVT_NL_ERR, "nl_err");
	ufshcd_print_evt(hba, UFS_EVT_TL_ERR, "tl_err");
	ufshcd_print_evt(hba, UFS_EVT_DME_ERR, "dme_err");
	ufshcd_print_evt(hba, UFS_EVT_AUTO_HIBERN8_ERR,
			 "auto_hibern8_err");
	ufshcd_print_evt(hba, UFS_EVT_FATAL_ERR, "fatal_err");
	ufshcd_print_evt(hba, UFS_EVT_LINK_STARTUP_FAIL,
			 "link_startup_fail");
	ufshcd_print_evt(hba, UFS_EVT_RESUME_ERR, "resume_fail");
	ufshcd_print_evt(hba, UFS_EVT_SUSPEND_ERR,
			 "suspend_fail");
	ufshcd_print_evt(hba, UFS_EVT_WL_RES_ERR, "wlun resume_fail");
	ufshcd_print_evt(hba, UFS_EVT_WL_SUSP_ERR,
			 "wlun suspend_fail");
	ufshcd_print_evt(hba, UFS_EVT_DEV_RESET, "dev_reset");
	ufshcd_print_evt(hba, UFS_EVT_HOST_RESET, "host_reset");
	ufshcd_print_evt(hba, UFS_EVT_ABORT, "task_abort");

	/* Stock sticky timeout flag suppresses variant register dump. */
	if (!os44_uicerr_flag)
		ufshcd_vops_dbg_register_dump(hba);
}
