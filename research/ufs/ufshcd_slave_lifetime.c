/* SPDX-License-Identifier: GPL-2.0-only */
/* Public-source equivalent candidate, validated against complete stock functions. */
#include <linux/blk-mq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
void os44_ufshcd_slave_destroy(struct scsi_device *sdev)
{
	struct ufs_hba *hba;
	unsigned long flags;

	hba = shost_priv(sdev->host);

	/* Drop the reference as it won't be needed anymore */
	if (ufshcd_scsi_to_upiu_lun(sdev->lun) == UFS_UPIU_UFS_DEVICE_WLUN) {
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->ufs_device_wlun = NULL;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	} else if (hba->ufs_device_wlun) {
		struct device *supplier = NULL;

		/* Ensure UFS Device WLUN exists and does not disappear */
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (hba->ufs_device_wlun) {
			supplier = &hba->ufs_device_wlun->sdev_gendev;
			get_device(supplier);
		}
		spin_unlock_irqrestore(hba->host->host_lock, flags);

		if (supplier) {
			/*
			 * If a LUN fails to probe (e.g. absent BOOT WLUN), the
			 * device will not have been registered but can still
			 * have a device link holding a reference to the device.
			 */
			device_link_remove(&sdev->sdev_gendev, supplier);
			put_device(supplier);
		}
	}
}
