// SPDX-License-Identifier: GPL-2.0-only
/* Reconstructed OS44 complete recovery functions; no owner storage or ABI bypass. */
#include <linux/blk-mq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_err_handler.h"
#include "os44_abort_contract.h"
#include "os44_completion_contract.h"
#include "os44_command_trace_contract.h"
#include "os44_reset_recovery.h"
#include "os44_pm_helpers_contract.h"
#include "os44_pm_contract.h"
#include "os44_uic_contract.h"
#include "os44_tm_contract.h"
#include "os44_reset_prereqs.h"
#include "os44_device_init_helpers.h"
#include "ufsswap_provider_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
extern int os44_ufshcd_poll(struct Scsi_Host *, unsigned int);
extern unsigned long os44_ufshcd_mcq_poll_cqe_lock(struct ufs_hba *, struct ufs_hw_queue *);
extern int os44_ufshcd_setup_vreg(struct ufs_hba *, bool);
bool os44_ufshcd_abort_one(struct request *rq, void *priv)
{
	int *ret = priv;
	u32 tag = rq->tag;
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(rq);
	struct scsi_device *sdev = cmd->device;
	struct Scsi_Host *shost = sdev->host;
	struct ufs_hba *hba = shost_priv(shost);

	*ret = os44_ufshcd_try_to_abort_task(hba, tag);
	dev_err(hba->dev, "Aborting tag %d / CDB %#02x %s\n", tag,
		hba->lrb[tag].cmd ? hba->lrb[tag].cmd->cmnd[0] : -1,
		*ret ? "failed" : "succeeded");

	return *ret == 0;
}

bool os44_ufshcd_print_tr_iter(struct request *req, void *priv)
{
	struct scsi_device *sdev = req->q->queuedata;
	struct Scsi_Host *shost = sdev->host;
	struct ufs_hba *hba = shost_priv(shost);

	os44_ufshcd_print_tr(hba, req->tag, *(bool *)priv);

	return true;
}

bool os44_ufshcd_mcq_compl_one(struct request *rq, void *priv)
{
	struct scsi_device *sdev = rq->q->queuedata;
	struct Scsi_Host *shost = sdev->host;
	struct ufs_hba *hba = shost_priv(shost);
	struct ufs_hw_queue *hwq = os44_ufshcd_mcq_req_to_hwq(hba, rq);

	if (hwq)
		os44_ufshcd_mcq_poll_cqe_lock(hba, hwq);

	return true;
}
