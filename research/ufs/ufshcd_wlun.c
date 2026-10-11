/* SPDX-License-Identifier: GPL-2.0-only */
/* WLUN public callbacks plus original per-LUN quiesce and CLD shutdown.
 * Core PM and CLD providers remain genuinely unresolved; no fake registration.
 * Source-pair declarations are not claims of original proprietary type names. */
#include <linux/blk-pm.h>
#include <linux/pm_runtime.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_wlun_contract.h"
#define CREATE_TRACE_POINTS
#include "os44_wlun_trace.h"
static_assert(sizeof(struct scsi_driver) == 232);
static_assert(sizeof(struct dev_pm_ops) == 192);
static inline u16 ufshcd_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

static inline bool is_device_wlun(struct scsi_device *sdev)
{
	return sdev->lun ==
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_UFS_DEVICE_WLUN);
}

static int ufshcd_wl_probe(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);

	if (!is_device_wlun(sdev))
		return -ENODEV;

	blk_pm_runtime_init(sdev->request_queue, dev);
	pm_runtime_set_autosuspend_delay(dev, 0);
	pm_runtime_allow(dev);

	return  0;
}

static int ufshcd_wl_remove(struct device *dev)
{
	pm_runtime_forbid(dev);
	return 0;
}

static void ufshcd_wl_shutdown(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ufs_hba *hba = shost_priv(sdev->host);

	down(&hba->host_sem);
	hba->shutting_down = true;
	up(&hba->host_sem);

	/* Turn on everything while shutting down */
	ufshcd_rpm_get_sync(hba);
	scsi_device_quiesce(sdev);
	shost_for_each_device(sdev, hba->host) {
		if (sdev == hba->ufs_device_wlun)
			continue;
		scsi_device_quiesce(sdev);
		mutex_lock(&sdev->state_mutex);
		scsi_device_set_state(sdev, SDEV_OFFLINE);
		mutex_unlock(&sdev->state_mutex);
	}
	os44_wl_suspend_core(hba, UFS_SHUTDOWN_PM);

	/*
	 * Next, turn off the UFS controller and the UFS regulators. Disable
	 * clocks.
	 */
	if (ufshcd_is_ufs_dev_poweroff(hba) && ufshcd_is_link_off(hba))
		os44_ufshcd_suspend_core(hba);

	if (os44_ufscld_get_state(hba) == 1)
		os44_ufscld_suspend(hba);
	hba->is_powered = false;
}

static int ufshcd_wl_poweroff(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ufs_hba *hba = shost_priv(sdev->host);

	os44_wl_suspend_core(hba, UFS_SHUTDOWN_PM);
	return 0;
}

static int ufshcd_wl_suspend(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ufs_hba *hba;
	int ret = 0;
	ktime_t start = ktime_get();

	hba = shost_priv(sdev->host);
	down(&hba->host_sem);
	hba->system_suspending = true;

	if (pm_runtime_suspended(dev))
		goto out;

	ret = os44_wl_suspend_core(hba, UFS_SYSTEM_PM);
	if (ret) {
		dev_err(&sdev->sdev_gendev, "%s failed: %d\n", __func__,  ret);
		up(&hba->host_sem);
	}

out:
	if (!ret)
		hba->is_sys_suspended = true;
	trace_ufshcd_wl_suspend(hba, ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);

	return ret;
}

static int ufshcd_wl_resume(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ufs_hba *hba;
	int ret = 0;
	ktime_t start = ktime_get();

	hba = shost_priv(sdev->host);

	if (pm_runtime_suspended(dev))
		goto out;

	ret = os44_wl_resume_core(hba, UFS_SYSTEM_PM);
	if (ret)
		dev_err(&sdev->sdev_gendev, "%s failed: %d\n", __func__, ret);
out:
	trace_ufshcd_wl_resume(hba, ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	if (!ret)
		hba->is_sys_suspended = false;
	hba->system_suspending = false;
	up(&hba->host_sem);
	return ret;
}

static int ufshcd_wl_runtime_suspend(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ufs_hba *hba;
	int ret;
	ktime_t start = ktime_get();

	hba = shost_priv(sdev->host);

	ret = os44_wl_suspend_core(hba, UFS_RUNTIME_PM);
	if (ret)
		dev_err(&sdev->sdev_gendev, "%s failed: %d\n", __func__, ret);

	trace_ufshcd_wl_runtime_suspend(hba, ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);

	return ret;
}

static int ufshcd_wl_runtime_resume(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ufs_hba *hba;
	int ret = 0;
	ktime_t start = ktime_get();

	hba = shost_priv(sdev->host);

	ret = os44_wl_resume_core(hba, UFS_RUNTIME_PM);
	if (ret)
		dev_err(&sdev->sdev_gendev, "%s failed: %d\n", __func__, ret);

	trace_ufshcd_wl_runtime_resume(hba, ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);

	return ret;
}

static const struct dev_pm_ops ufshcd_wl_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = ufshcd_wl_suspend,
	.resume = ufshcd_wl_resume,
	.freeze = ufshcd_wl_suspend,
	.thaw = ufshcd_wl_resume,
	.poweroff = ufshcd_wl_poweroff,
	.restore = ufshcd_wl_resume,
#endif
	SET_RUNTIME_PM_OPS(ufshcd_wl_runtime_suspend, ufshcd_wl_runtime_resume, NULL)
};

struct scsi_driver os44_ufs_dev_wlun_template = {
	.gendrv = {
		.name = "ufs_device_wlun",
		.probe = ufshcd_wl_probe,
		.remove = ufshcd_wl_remove,
		.pm = &ufshcd_wl_pm_ops,
		.shutdown = ufshcd_wl_shutdown,
	},
};
