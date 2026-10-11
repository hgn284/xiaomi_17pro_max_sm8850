#include "os44_reset_recovery.h"
/* SPDX-License-Identifier: GPL-2.0-only */
/* Full source-derived callbacks; acceptance requires whole stock ELF comparison. */
#include <linux/blk-mq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include <trace/hooks/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
enum {
	UFSHCD_UIC_DL_PA_INIT_ERROR = (1 << 0), /* Data link layer error */
	UFSHCD_UIC_DL_NAC_RECEIVED_ERROR = (1 << 1), /* Data link layer error */
	UFSHCD_UIC_DL_TCx_REPLAY_ERROR = (1 << 2), /* Data link layer error */
	UFSHCD_UIC_NL_ERROR = (1 << 3), /* Network layer error */
	UFSHCD_UIC_TL_ERROR = (1 << 4), /* Transport Layer error */
	UFSHCD_UIC_DME_ERROR = (1 << 5), /* DME error */
	UFSHCD_UIC_PA_GENERIC_ERROR = (1 << 6), /* Generic PA error */
};

static inline bool ufshcd_is_saved_err_fatal(struct ufs_hba *hba)
{
	return (hba->saved_uic_err & UFSHCD_UIC_DL_PA_INIT_ERROR) ||
	       (hba->saved_err & (DEVICE_FATAL_ERROR | CONTROLLER_FATAL_ERROR | SYSTEM_BUS_FATAL_ERROR | CRYPTO_ENGINE_FATAL_ERROR | UIC_LINK_LOST | UFSHCD_UIC_HIBERN8_MASK));
}

void os44_ufshcd_schedule_eh_work(struct ufs_hba *hba)
{
	lockdep_assert_held(hba->host->host_lock);

	/* handle fatal errors only when link is not in error state */
	if (hba->ufshcd_state != UFSHCD_STATE_ERROR) {
		if (hba->force_reset || ufshcd_is_link_broken(hba) ||
		    ufshcd_is_saved_err_fatal(hba))
			hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_FATAL;
		else
			hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED_NON_FATAL;
		queue_work(hba->eh_wq, &hba->eh_work);
	}
}

int os44_ufshcd_eh_host_reset_handler(struct scsi_cmnd *cmd)
{
	int err = SUCCESS;
	unsigned long flags;
	struct ufs_hba *hba;

	hba = shost_priv(cmd->device->host);

	/*
	 * If runtime PM sent SSU and got a timeout, scsi_error_handler is
	 * stuck in this function waiting for flush_work(&hba->eh_work). And
	 * ufshcd_err_handler(eh_work) is stuck waiting for runtime PM. Do
	 * ufshcd_link_recovery instead of eh_work to prevent deadlock.
	 */
	if (hba->pm_op_in_progress) {
		if (os44_ufshcd_link_recovery(hba))
			err = FAILED;

		return err;
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->force_reset = true;
	os44_ufshcd_schedule_eh_work(hba);
	dev_err(hba->dev, "%s: reset in progress - 1\n", "ufshcd_eh_host_reset_handler");
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	flush_work(&hba->eh_work);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->ufshcd_state == UFSHCD_STATE_ERROR)
		err = FAILED;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return err;
}

enum scsi_timeout_action os44_ufshcd_eh_timed_out(struct scsi_cmnd *scmd)
{
	struct ufs_hba *hba = shost_priv(scmd->device->host);

	if (!hba->system_suspending) {
		/* Activate the error handler in the SCSI core. */
		return SCSI_EH_NOT_HANDLED;
	}

	/*
	 * If we get here we know that no TMFs are outstanding and also that
	 * the only pending command is a START STOP UNIT command. Handle the
	 * timeout of that command directly to prevent a deadlock between
	 * ufshcd_set_dev_pwr_mode() and ufshcd_err_handler().
	 */
	os44_ufshcd_link_recovery(hba);
	dev_info(hba->dev, "%s() finished; outstanding_tasks = %#lx.\n",
		 "ufshcd_eh_timed_out", hba->outstanding_tasks);

	trace_android_vh_ufs_eh_timed_out(hba, scmd);

	return scsi_host_busy(hba->host) ? SCSI_EH_RESET_TIMER : SCSI_EH_DONE;
}
