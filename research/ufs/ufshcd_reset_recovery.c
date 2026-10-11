/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction; shared HBA, real reset/error owners. */
#include <linux/blk-mq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_eh.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_reset_prereqs.h"
#include "os44_pm_helpers_contract.h"
#include "os44_reset_recovery.h"
int os44_ufshcd_reset_and_restore(struct ufs_hba *hba)
{
	u32 saved_err = 0;
	u32 saved_uic_err = 0;
	int err = 0;
	unsigned long flags;
	int retries = 5;

	spin_lock_irqsave(hba->host->host_lock, flags);
	do {
		/*
		 * This is a fresh start, cache and clear saved error first,
		 * in case new error generated during reset and restore.
		 */
		saved_err |= hba->saved_err;
		saved_uic_err |= hba->saved_uic_err;
		hba->saved_err = 0;
		hba->saved_uic_err = 0;
		hba->force_reset = false;
		hba->ufshcd_state = UFSHCD_STATE_RESET;
		spin_unlock_irqrestore(hba->host->host_lock, flags);

		/* Reset the attached device */
		os44_ufshcd_device_reset(hba);

		err = os44_ufshcd_host_reset_and_restore(hba);

		spin_lock_irqsave(hba->host->host_lock, flags);
		if (err)
			continue;
		/* Do not exit unless operational or dead */
		if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL &&
		    hba->ufshcd_state != UFSHCD_STATE_ERROR &&
		    hba->ufshcd_state != UFSHCD_STATE_EH_SCHEDULED_NON_FATAL)
			err = -EAGAIN;
	} while (err && --retries);

	/*
	 * Inform scsi mid-layer that we did reset and allow to handle
	 * Unit Attention properly.
	 */
	scsi_report_bus_reset(hba->host, 0);
	if (err) {
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
		hba->saved_err |= saved_err;
		hba->saved_uic_err |= saved_uic_err;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return err;
}
int os44_ufshcd_link_recovery(struct ufs_hba *hba)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->ufshcd_state = UFSHCD_STATE_RESET;
	hba->eh_flags |= 1;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* Reset the attached device */
	os44_ufshcd_device_reset(hba);

	ret = os44_ufshcd_host_reset_and_restore(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (ret)
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
	hba->eh_flags &= ~1;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		dev_err(hba->dev, "%s: link recovery failed, err %d",
			"ufshcd_link_recovery", ret);

	return ret;
}
void os44_ufshcd_force_error_recovery(struct ufs_hba *hba)
{
	spin_lock_irq(hba->host->host_lock);
	hba->force_reset = true;
	/* force_reset is already true under the same lock: original inline path. */
 if (hba->ufshcd_state != UFSHCD_STATE_ERROR) {
  hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;
  queue_work(hba->eh_wq, &hba->eh_work);
 }
	spin_unlock_irq(hba->host->host_lock);
}
