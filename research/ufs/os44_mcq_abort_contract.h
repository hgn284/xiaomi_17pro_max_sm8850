/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_MCQ_ABORT_CONTRACT_H
#define OS44_MCQ_ABORT_CONTRACT_H
#include <scsi/scsi_cmnd.h>
bool os44_ufshcd_cmd_inflight(struct scsi_cmnd *);
int os44_ufshcd_mcq_abort(struct scsi_cmnd *);
#endif
