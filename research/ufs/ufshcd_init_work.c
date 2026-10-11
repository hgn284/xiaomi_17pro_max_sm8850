/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole OS44 callbacks; public control-flow checked against original ELF.
 * Real query source bound explicitly; no private owner storage supplied. */
#include <linux/devfreq.h>
#include <linux/timekeeping.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "ufsswap_provider_reconstruction.h"
#include "os44_init_work.h"
static bool ufshcd_is_ufs_dev_busy(struct ufs_hba *hba)
{
 return scsi_host_busy(hba->host) || hba->outstanding_tasks ||
        hba->active_uic_cmd || hba->uic_async_done;
}
void os44_ufshcd_clk_scaling_suspend_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
					   clk_scaling.suspend_work);
	unsigned long irq_flags;

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (hba->clk_scaling.active_reqs || hba->clk_scaling.is_suspended) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return;
	}
	hba->clk_scaling.is_suspended = true;
	hba->clk_scaling.window_start_t = 0;
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	devfreq_suspend_device(hba->devfreq);
}

void os44_ufshcd_clk_scaling_resume_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
					   clk_scaling.resume_work);
	unsigned long irq_flags;

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (!hba->clk_scaling.is_suspended) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return;
	}
	hba->clk_scaling.is_suspended = false;
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	devfreq_resume_device(hba->devfreq);
}

void os44_ufshcd_rpm_dev_flush_recheck_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(to_delayed_work(work),
					   struct ufs_hba,
					   rpm_dev_flush_recheck_work);
	/*
	 * To prevent unnecessary VCC power drain after device finishes
	 * WriteBooster buffer flush or Auto BKOPs, force runtime resume
	 * after a certain delay to recheck the threshold by next runtime
	 * suspend.
	 */
	ufshcd_rpm_get_sync(hba);
	ufshcd_rpm_put_sync(hba);
}

static void ufshcd_update_rtc(struct ufs_hba *hba)
{
	struct timespec64 ts64;
	int err;
	u32 val;

	ktime_get_real_ts64(&ts64);

	if (ts64.tv_sec < hba->dev_info.rtc_time_baseline) {
		dev_warn_once(hba->dev, "%s: Current time precedes previous setting!\n", __func__);
		return;
	}

	/*
	 * The Absolute RTC mode has a 136-year limit, spanning from 2010 to 2146. If a time beyond
	 * 2146 is required, it is recommended to choose the relative RTC mode.
	 */
	val = ts64.tv_sec - hba->dev_info.rtc_time_baseline;

	/* Skip update RTC if RPM state is not RPM_ACTIVE */
	if (ufshcd_rpm_get_if_active(hba) <= 0)
		return;

	err = os44_query_attr_once(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, QUERY_ATTR_IDN_SECONDS_PASSED,
				0, 0, &val);
	ufshcd_rpm_put(hba);

	if (err)
		dev_err(hba->dev, "%s: Failed to update rtc %d\n", __func__, err);
	else if (hba->dev_info.rtc_type == UFS_RTC_RELATIVE)
		hba->dev_info.rtc_time_baseline = ts64.tv_sec;
}

void os44_ufshcd_rtc_work(struct work_struct *work)
{
	struct ufs_hba *hba;

	hba = container_of(to_delayed_work(work), struct ufs_hba, ufs_rtc_update_work);

	 /* Update RTC only when there are no requests in progress and UFSHCI is operational */
	if (!ufshcd_is_ufs_dev_busy(hba) &&
	    hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL &&
	    !hba->clk_gating.active_reqs)
		ufshcd_update_rtc(hba);

	if (ufshcd_is_ufs_dev_active(hba) && hba->dev_info.rtc_update_period)
		schedule_delayed_work(&hba->ufs_rtc_update_work,
				      msecs_to_jiffies(hba->dev_info.rtc_update_period));
}

#define CHECK(m,o) static_assert(offsetof(struct ufs_hba,m)==o)
CHECK(host,0x38);
CHECK(dev,0x40);
CHECK(ufs_device_wlun,0x48);
CHECK(curr_dev_pwr_mode,0x50);
CHECK(outstanding_tasks,0x70);
CHECK(active_uic_cmd,0x1b0);
CHECK(uic_async_done,0x1e8);
CHECK(ufshcd_state,0x1f0);
CHECK(devfreq,0xbc0);
CHECK(clk_scaling.active_reqs,0xbc8);
CHECK(clk_scaling.window_start_t,0xbd8);
CHECK(clk_scaling.suspend_work,0xc30);
CHECK(clk_scaling.resume_work,0xc50);
CHECK(clk_scaling.is_suspended,0xc84);
CHECK(rpm_dev_flush_recheck_work,0x10b0);
CHECK(ufs_rtc_update_work,0x1440);
CHECK(dev_info.rtc_type,0xa14);
CHECK(dev_info.rtc_time_baseline,0xa18);
CHECK(dev_info.rtc_update_period,0xa20);

blk_status_t os44_ufshcd_queue_tmf(struct blk_mq_hw_ctx *hctx,
				     const struct blk_mq_queue_data *qd)
{
	WARN_ON_ONCE(true);
	return BLK_STS_NOTSUPP;
}

const struct blk_mq_ops os44_ufshcd_tmf_ops = {
	.queue_rq = os44_ufshcd_queue_tmf,
};

static_assert(sizeof(struct blk_mq_ops)==152);
