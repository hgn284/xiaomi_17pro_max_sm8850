/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstructed source-owned abort prerequisite. Public baseline bound to
 * complete original ARM64 instructions; real local TM and clear providers.
 * No owner globals, no alternate kernel export substituted. */
#include <linux/blk-mq.h>
#include <linux/delay.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_tm_contract.h"
#include "os44_abort_contract.h"
extern int os44_core_clear_cmd(struct ufs_hba *, u32);

static bool local_cmd_inflight(struct scsi_cmnd *cmd)
{
	return cmd && blk_mq_rq_state(scsi_cmd_to_rq(cmd)) == MQ_RQ_IN_FLIGHT;
}

static int ufshcd_issue_tm_cmd(struct ufs_hba *hba, int lun_id, int task_id,
		u8 tm_function, u8 *tm_response)
{
	struct utp_task_req_desc treq = { };
	enum utp_ocs ocs_value;
	int err;

	/* Configure task request descriptor */
	treq.header.interrupt = 1;
	treq.header.ocs = OCS_INVALID_COMMAND_STATUS;

	/* Configure task request UPIU */
	treq.upiu_req.req_header.transaction_code = UPIU_TRANSACTION_TASK_REQ;
	treq.upiu_req.req_header.lun = lun_id;
	treq.upiu_req.req_header.tm_function = tm_function;

	/*
	 * The host shall provide the same value for LUN field in the basic
	 * header and for Input Parameter.
	 */
	treq.upiu_req.input_param1 = cpu_to_be32(lun_id);
	treq.upiu_req.input_param2 = cpu_to_be32(task_id);

	err = os44___ufshcd_issue_tm_cmd(hba, &treq, tm_function);
	if (err == -ETIMEDOUT)
		return err;

	ocs_value = treq.header.ocs & MASK_OCS;
	if (ocs_value != OCS_SUCCESS)
		dev_err(hba->dev, "%s: failed, ocs = 0x%x\n",
				"ufshcd_issue_tm_cmd", ocs_value);
	else if (tm_response)
		*tm_response = be32_to_cpu(treq.upiu_rsp.output_param1) &
				MASK_TM_SERVICE_RESP;
	return err;
}

int os44_ufshcd_try_to_abort_task(struct ufs_hba *hba, int tag)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	int err = 0;
	int poll_cnt;
	u8 resp = 0xF;
	u32 reg;

	for (poll_cnt = 100; poll_cnt; poll_cnt--) {
		err = ufshcd_issue_tm_cmd(hba, lrbp->lun, lrbp->task_tag,
				UFS_QUERY_TASK, &resp);
		if (!err && resp == UPIU_TASK_MANAGEMENT_FUNC_SUCCEEDED) {
			/* cmd pending in the device */
			dev_err(hba->dev, "%s: cmd pending in the device. tag = %d\n",
				"ufshcd_try_to_abort_task", tag);
			break;
		} else if (!err && resp == UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
			/*
			 * cmd not pending in the device, check if it is
			 * in transition.
			 */
			dev_err(hba->dev, "%s: cmd at tag %d not pending in the device.\n",
				"ufshcd_try_to_abort_task", tag);
			if (hba->mcq_enabled) {
				/* MCQ mode */
				if (local_cmd_inflight(lrbp->cmd)) {
					/* sleep for max. 200us same delay as in SDB mode */
					usleep_range(100, 200);
					continue;
				}
				/* command completed already */
				dev_err(hba->dev, "%s: cmd at tag=%d is cleared.\n",
					"ufshcd_try_to_abort_task", tag);
				goto out;
			}

			/* Single Doorbell Mode */
			reg = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
			if (reg & (1 << tag)) {
				/* sleep for max. 200us to stabilize */
				usleep_range(100, 200);
				continue;
			}
			/* command completed already */
			dev_err(hba->dev, "%s: cmd at tag %d successfully cleared from DB.\n",
				"ufshcd_try_to_abort_task", tag);
			goto out;
		} else {
			dev_err(hba->dev,
				"%s: no response from device. tag = %d, err %d\n",
				"ufshcd_try_to_abort_task", tag, err);
			if (!err)
				err = resp; /* service response error */
			goto out;
		}
	}

	if (!poll_cnt) {
		err = -EBUSY;
		goto out;
	}

	err = ufshcd_issue_tm_cmd(hba, lrbp->lun, lrbp->task_tag,
			UFS_ABORT_TASK, &resp);
	if (err || resp != UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
		if (!err) {
			err = resp; /* service response error */
			dev_err(hba->dev, "%s: issued. tag = %d, err %d\n",
				"ufshcd_try_to_abort_task", tag, err);
		}
		goto out;
	}

	err = os44_core_clear_cmd(hba, tag);
	if (err)
		dev_err(hba->dev, "%s: Failed clearing cmd at tag %d, err %d\n",
			"ufshcd_try_to_abort_task", tag, err);

out:
	return err;
}
