/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/blk-mq.h>
#include "os44_mcq_abort_contract.h"
bool os44_ufshcd_cmd_inflight(struct scsi_cmnd *cmd)
{
 return cmd && blk_mq_rq_state(scsi_cmd_to_rq(cmd)) == MQ_RQ_IN_FLIGHT;
}
