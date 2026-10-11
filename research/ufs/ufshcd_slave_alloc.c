/* SPDX-License-Identifier: GPL-2.0-only */
/* Full candidate from frozen public source; accepted only after stock audit. */
#include <linux/blk-mq.h>
#include <linux/slab.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include <trace/hooks/ufshcd.h>
#include "os44_descriptor_owner.h"
#include "drivers/ufs/core/ufshcd-priv.h"
static inline int ufshcd_read_unit_desc_param(struct ufs_hba *hba,
					      int lun,
					      enum unit_desc_param param_offset,
					      u8 *param_read_buf,
					      u32 param_size)
{
	/*
	 * Unit descriptors are only available for general purpose LUs (LUN id
	 * from 0 to 7) and RPMB Well known LU.
	 */
	if (!ufs_is_valid_unit_desc_lun(&hba->dev_info, lun))
		return -EOPNOTSUPP;

	return os44_mi_ufshcd_read_desc_param(hba, QUERY_DESC_IDN_UNIT, lun,
				      param_offset, param_read_buf, param_size);
}
static void ufshcd_setup_links(struct ufs_hba *hba, struct scsi_device *sdev)
{
	struct device_link *link;

	/*
	 * Device wlun is the supplier & rest of the luns are consumers.
	 * This ensures that device wlun suspends after all other luns.
	 */
	if (hba->ufs_device_wlun) {
		link = device_link_add(&sdev->sdev_gendev,
				       &hba->ufs_device_wlun->sdev_gendev,
				       DL_FLAG_PM_RUNTIME | DL_FLAG_RPM_ACTIVE);
		if (!link) {
			dev_err(&sdev->sdev_gendev, "Failed establishing link - %s\n",
				dev_name(&hba->ufs_device_wlun->sdev_gendev));
			return;
		}
		hba->luns_avail--;
		/* Ignore REPORT_LUN wlun probing */
		if (hba->luns_avail == 1) {
			ufshcd_rpm_put(hba);
			return;
		}
	} else {
		/*
		 * Device wlun is probed. The assumption is that WLUNs are
		 * scanned before other LUNs.
		 */
		hba->luns_avail--;
	}
}
static void ufshcd_lu_init(struct ufs_hba *hba, struct scsi_device *sdev)
{
	int len = QUERY_DESC_MAX_SIZE;
	u8 lun = ufshcd_scsi_to_upiu_lun(sdev->lun);
	u8 lun_qdepth = hba->nutrs;
	u8 *desc_buf;
	int ret;

	desc_buf = kzalloc(len, GFP_KERNEL);
	if (!desc_buf)
		goto set_qdepth;

	ret = ufshcd_read_unit_desc_param(hba, lun, 0, desc_buf, len);
	if (ret < 0) {
		if (ret == -EOPNOTSUPP)
			/* If LU doesn't support unit descriptor, its queue depth is set to 1 */
			lun_qdepth = 1;
		kfree(desc_buf);
		goto set_qdepth;
	}

	if (desc_buf[UNIT_DESC_PARAM_LU_Q_DEPTH]) {
		/*
		 * In per-LU queueing architecture, bLUQueueDepth will not be 0, then we will
		 * use the smaller between UFSHCI CAP.NUTRS and UFS LU bLUQueueDepth
		 */
		lun_qdepth = min_t(int, desc_buf[UNIT_DESC_PARAM_LU_Q_DEPTH], hba->nutrs);
	}
	/*
	 * According to UFS device specification, the write protection mode is only supported by
	 * normal LU, not supported by WLUN.
	 */
	if (hba->dev_info.f_power_on_wp_en && lun < hba->dev_info.max_lu_supported &&
	    !hba->dev_info.is_lu_power_on_wp &&
	    desc_buf[UNIT_DESC_PARAM_LU_WR_PROTECT] == UFS_LU_POWER_ON_WP)
		hba->dev_info.is_lu_power_on_wp = true;

	/* In case of RPMB LU, check if advanced RPMB mode is enabled */
	if (desc_buf[UNIT_DESC_PARAM_UNIT_INDEX] == UFS_UPIU_RPMB_WLUN &&
	    desc_buf[RPMB_UNIT_DESC_PARAM_REGION_EN] & BIT(4))
		hba->dev_info.b_advanced_rpmb_en = true;


	kfree(desc_buf);
set_qdepth:
	/*
	 * For WLUNs that don't support unit descriptor, queue depth is set to 1. For LUs whose
	 * bLUQueueDepth == 0, the queue depth is set to a maximum value that host can queue.
	 */
	dev_dbg(hba->dev, "Set LU %x queue depth %d\n", lun, lun_qdepth);
	scsi_change_queue_depth(sdev, lun_qdepth);
}
int os44_ufshcd_slave_alloc(struct scsi_device *sdev)
{
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);

	/* Mode sense(6) is not supported by UFS, so use Mode sense(10) */
	sdev->use_10_for_ms = 1;

	/* DBD field should be set to 1 in mode sense(10) */
	sdev->set_dbd_for_ms = 1;

	/* allow SCSI layer to restart the device in case of errors */
	sdev->allow_restart = 1;

	/* REPORT SUPPORTED OPERATION CODES is not supported */
	sdev->no_report_opcodes = 1;

	/* WRITE_SAME command is not supported */
	sdev->no_write_same = 1;

	ufshcd_lu_init(hba, sdev);

	ufshcd_setup_links(hba, sdev);

	trace_android_vh_ufs_update_sdev(sdev);

	return 0;
}
