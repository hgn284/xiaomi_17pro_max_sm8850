/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalence reconstruction checked against the whole stock ELF functions. */
#include "ufsswap_provider_reconstruction.h"
#include "os44_pm_helpers_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#include <scsi/scsi_eh.h>
#include <scsi/scsi_dbg.h>
#include <linux/sched/clock.h>
static inline void local_update_evt_hist(struct ufs_hba *hba, u32 id, u32 val)
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
static int ufshcd_execute_start_stop(struct scsi_device *sdev,
				     enum ufs_dev_pwr_mode pwr_mode,
				     struct scsi_sense_hdr *sshdr)
{
	const unsigned char cdb[6] = { START_STOP, 0, 0, 0, pwr_mode << 4, 0 };
	struct scsi_failure failure_defs[] = {
		{
			.allowed = 2,
			.result = SCMD_FAILURE_RESULT_ANY,
		},
	};
	struct scsi_failures failures = {
		.failure_definitions = failure_defs,
	};
	const struct scsi_exec_args args = {
		.failures = &failures,
		.sshdr = sshdr,
		.req_flags = BLK_MQ_REQ_PM,
		.scmd_flags = SCMD_FAIL_IF_RECOVERING,
	};

	return scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_IN, /*buffer=*/NULL,
			/*bufflen=*/0, /*timeout=*/10 * HZ, /*retries=*/0,
			&args);
}
void os44_ufshcd_suspend_clkscaling(struct ufs_hba *hba)
{
	unsigned long flags;
	bool suspend = false;

	cancel_work_sync(&hba->clk_scaling.suspend_work);
	cancel_work_sync(&hba->clk_scaling.resume_work);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!hba->clk_scaling.is_suspended) {
		suspend = true;
		hba->clk_scaling.is_suspended = true;
		hba->clk_scaling.window_start_t = 0;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (suspend)
		devfreq_suspend_device(hba->devfreq);
}

void os44_ufshcd_device_reset(struct ufs_hba *hba)
{
	int err;

	err = ufshcd_vops_device_reset(hba);

	if (!err) {
		ufshcd_set_ufs_dev_active(hba);
		if (ufshcd_is_wb_allowed(hba)) {
			hba->dev_info.wb_enabled = false;
			hba->dev_info.wb_buf_flush_enabled = false;
		}
		if (hba->dev_info.rtc_type == UFS_RTC_RELATIVE)
			hba->dev_info.rtc_time_baseline = 0;
	}
	if (err != -EOPNOTSUPP)
		local_update_evt_hist(hba, UFS_EVT_DEV_RESET, err);
}

int os44_ufshcd_set_dev_pwr_mode(struct ufs_hba *hba,
				     enum ufs_dev_pwr_mode pwr_mode)
{
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdp;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = hba->ufs_device_wlun;
	if (sdp && scsi_device_online(sdp))
		ret = scsi_device_get(sdp);
	else
		ret = -ENODEV;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	/*
	 * If scsi commands fail, the scsi mid-layer schedules scsi error-
	 * handling, which would wait for host to be resumed. Since we know
	 * we are functional while we are here, skip host resume in error
	 * handling context.
	 */
	hba->host->eh_noresume = 1;

	/*
	 * Current function would be generally called from the power management
	 * callbacks hence set the RQF_PM flag so that it doesn't resume the
	 * already suspended childs.
	 */
	ret = ufshcd_execute_start_stop(sdp, pwr_mode, &sshdr);
	if (ret) {
		sdev_printk(KERN_WARNING, sdp,
			    "START_STOP failed for power mode: %d, result %x\n",
			    pwr_mode, ret);
		if (ret > 0) {
			if (scsi_sense_valid(&sshdr))
				scsi_print_sense_hdr(sdp, NULL, &sshdr);
			ret = -EIO;
		}
	} else {
		hba->curr_dev_pwr_mode = pwr_mode;
	}

	scsi_device_put(sdp);
	hba->host->eh_noresume = 0;
	return ret;
}
