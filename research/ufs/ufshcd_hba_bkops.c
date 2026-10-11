/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction from whole stock functions; real MMIO, no stubs. */
#include <linux/delay.h>
#include <linux/jiffies.h>
#include "ufsswap_provider_reconstruction.h"
#include "os44_bkops_contract.h"
#include "os44_hba_bkops_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
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

static int ufshcd_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms)
{
	int err = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);

	/* ignore bits that we don't intend to wait on */
	val = val & mask;

	while ((ufshcd_readl(hba, reg) & mask) != val) {
		usleep_range(interval_us, interval_us + 50);
		if (time_after(jiffies, timeout)) {
			if ((ufshcd_readl(hba, reg) & mask) != val)
				err = -ETIMEDOUT;
			break;
		}
	}

	return err;
}

void os44_ufshcd_hba_stop(struct ufs_hba *hba)
{
	unsigned long flags;
	int err;

	/*
	 * Obtain the host lock to prevent that the controller is disabled
	 * while the UFS interrupt handler is active on another CPU.
	 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_writel(hba, CONTROLLER_DISABLE,  REG_CONTROLLER_ENABLE);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	err = ufshcd_wait_for_register(hba, REG_CONTROLLER_ENABLE,
					CONTROLLER_ENABLE, CONTROLLER_DISABLE,
					10, 1);
	if (err)
		dev_err(hba->dev, "%s: Controller disable failed\n", "ufshcd_hba_stop");
}

static inline int ufshcd_get_bkops_status(struct ufs_hba *hba, u32 *status)
{
	return local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_BKOPS_STATUS, 0, 0, status);
}

int os44_ufshcd_bkops_ctrl(struct ufs_hba *hba)
{
	enum bkops_status status = hba->urgent_bkops_lvl;
	u32 curr_status = 0;
	int err;

	err = ufshcd_get_bkops_status(hba, &curr_status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get BKOPS status %d\n",
				"ufshcd_bkops_ctrl", err);
		goto out;
	} else if (curr_status > BKOPS_STATUS_MAX) {
		dev_err(hba->dev, "%s: invalid BKOPS status %d\n",
				"ufshcd_bkops_ctrl", curr_status);
		err = -EINVAL;
		goto out;
	}

	if (curr_status >= status)
		err = os44_ufshcd_enable_auto_bkops(hba);
	else
		err = os44_ufshcd_disable_auto_bkops(hba);
out:
	return err;
}

/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole stock remove entry, reconstructed from frozen public source plus
 * proprietary calls in original order. NOT a lifetime repair; dependencies
 * intentionally remain undefined until their genuine providers are restored. */
#include <linux/blk-mq.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "drivers/ufs/core/ufs_bsg.h"
#include "drivers/ufs/core/ufs-sysfs.h"
void os44_ufscld_remove(struct ufs_hba *);
void os44_ufsswap_remove_hba(struct ufs_hba *);
void os44_ufseua_remove(struct ufs_hba *);
void os44_ufsfbo_remove(struct ufs_hba *);
void os44_ufshcd_hba_exit(struct ufs_hba *);
static void ufshcd_disable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	set &= ~intrs;
	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}
void os44_ufshcd_remove(struct ufs_hba *hba)
{
	if (hba->ufs_device_wlun)
		ufshcd_rpm_get_sync(hba);
	ufs_bsg_remove(hba);
	os44_ufscld_remove(hba);
	os44_ufsswap_remove_hba(hba);
	os44_ufseua_remove(hba);
	os44_ufsfbo_remove(hba);
	ufs_sysfs_remove_nodes(hba->dev);
	cancel_delayed_work_sync(&hba->ufs_rtc_update_work);
	blk_mq_destroy_queue(hba->tmf_queue);
	blk_put_queue(hba->tmf_queue);
	blk_mq_free_tag_set(&hba->tmf_tag_set);
	if (hba->scsi_host_added)
		scsi_remove_host(hba->host);
	/* disable interrupts */
	ufshcd_disable_intr(hba, hba->intr_mask);
	os44_ufshcd_hba_stop(hba);
	os44_ufshcd_hba_exit(hba);
}

/* SPDX-License-Identifier: GPL-2.0-only */
/* OS44 host-reset full original flow; no same-named GKI substitutions. */
#include <linux/blk-mq.h>
#include <linux/sched/clock.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_reset_prereqs.h"
#include "os44_hba_enable_contract.h"
#include "os44_hba_bkops_contract.h"
#include "os44_probe_hba.h"
int os44_ufshcd_poll(struct Scsi_Host *, unsigned int);
static void local_update_evt_hist(struct ufs_hba *hba, u32 id, u32 val)
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

int os44_ufshcd_host_reset_and_restore(struct ufs_hba *hba)
{
 int err;
 os44_ufshcd_hba_stop(hba);
 hba->silence_err_logs = true;
 if (hba->mcq_enabled)
  blk_mq_tagset_busy_iter(&hba->host->tag_set, os44_ufshcd_mcq_force_compl_one, NULL);
 else {
  if (ufshcd_is_intr_aggr_allowed(hba) && !(hba->quirks & UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR))
   ufshcd_writel(hba, INT_AGGR_ENABLE | INT_AGGR_COUNTER_AND_TIMER_RESET, REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
  os44_ufshcd_poll(hba->host, -1);
 }
 os44_ufshcd_tmc_handler(hba);
 hba->silence_err_logs = false;
 os44_ufshcd_scale_clks(hba, ULONG_MAX, true);
 err = os44_ufshcd_hba_enable(hba);
 if (!err) err = os44_ufshcd_probe_hba(hba, false);
 if (err) dev_err(hba->dev, "%s: Host init failed %d\n", "ufshcd_host_reset_and_restore", err);
 local_update_evt_hist(hba, UFS_EVT_HOST_RESET, (u32)err);
 return err;
}
