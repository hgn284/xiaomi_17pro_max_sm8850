/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstructed stock device_configure; typed unresolved Xiaomi owner, no fake definition. */
#include <linux/blk-mq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "drivers/ufs/core/ufshcd-crypto.h"
#include "os44_xiaomi_lifetime_view.h"
inline u16 os44_ufshcd_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}
inline bool os44_is_device_wlun(struct scsi_device *sdev)
{
	return sdev->lun ==
		os44_ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_UFS_DEVICE_WLUN);
}
int os44_ufshcd_device_configure(struct scsi_device *sdev,
		struct queue_limits *lim)
{
	struct ufs_hba *hba = shost_priv(sdev->host);
	struct request_queue *q = sdev->request_queue;

	lim->dma_pad_mask = PRDT_DATA_BYTE_COUNT_PAD - 1;
	if (!sdev->lun)
		os44_ufs_xiaomi.sdev_598 = sdev;
	if (!sdev->lun)
		os44_ufs_xiaomi.sdev_660 = sdev;
	if (!sdev->lun) {
		os44_ufs_xiaomi.sdev_8b8 = sdev;
		os44_ufs_xiaomi.sdev_980 = sdev;
	}

	/*
	 * Block runtime-pm until all consumers are added.
	 * Refer ufshcd_setup_links().
	 */
	if (os44_is_device_wlun(sdev))
		pm_runtime_get_noresume(&sdev->sdev_gendev);
	else if (ufshcd_is_rpm_autosuspend_allowed(hba))
		sdev->rpm_autosuspend = 1;
	/*
	 * Do not print messages during runtime PM to avoid never-ending cycles
	 * of messages written back to storage by user space causing runtime
	 * resume, causing more messages and so on.
	 */
	sdev->silence_suspend = 1;

	if (hba->vops && hba->vops->config_scsi_dev)
		hba->vops->config_scsi_dev(sdev);

	ufshcd_crypto_register(hba, q);

	return 0;
}
