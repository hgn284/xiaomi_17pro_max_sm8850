// SPDX-License-Identifier: GPL-2.0-only
/* Whole OS44 task-management provider; original 7500 ticks at HZ250 =30s. */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/blk-mq.h>
#include <linux/completion.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "ufsswap_provider_reconstruction.h"
#include "os44_clock_trace.h"
#include "os44_tm_contract.h"
#include <trace/hooks/ufshcd.h>
#define TM_CMD_TIMEOUT 30000
static void local_ufshcd_release(struct ufs_hba *hba)
{
	guard(spinlock_irqsave)(&hba->clk_gating.lock);
	os44_core_release_locked(hba);
}

static int ufshcd_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms)
{
	int err = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);

	/* ignore bits that we don't intend to wait on */
	val = val & mask;

	while ((ufshcd_readl(hba, reg) & mask) != val) {
		usleep_range(interval_us, interval_us + 50);
		if (time_after(jiffies, timeout)) {
			if ((ufshcd_readl(hba, reg) & mask) != val)
				err = -ETIMEDOUT;
			break;
		}
	}

	return err;
}

static inline void ufshcd_utmrl_clear(struct ufs_hba *hba, u32 pos)
{
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		ufshcd_writel(hba, (1 << pos), REG_UTP_TASK_REQ_LIST_CLEAR);
	else
		ufshcd_writel(hba, ~(1 << pos), REG_UTP_TASK_REQ_LIST_CLEAR);
}



int os44_ufshcd_clear_tm_cmd(struct ufs_hba *hba, int tag)
{
	int err = 0;
	u32 mask = 1 << tag;
	unsigned long flags;

	if (!test_bit(tag, &hba->outstanding_tasks))
		goto out;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_utmrl_clear(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* poll for max. 1 sec to clear door bell register by h/w */
	err = ufshcd_wait_for_register(hba,
			REG_UTP_TASK_REQ_DOOR_BELL,
			mask, 0, 1000, 1000);

	dev_err(hba->dev, "Clearing task management function with tag %d %s\n",
		tag, err < 0 ? "failed" : "succeeded");

out:
	return err;
}

int os44___ufshcd_issue_tm_cmd(struct ufs_hba *hba,
		struct utp_task_req_desc *treq, u8 tm_function)
{
	struct request_queue *q = hba->tmf_queue;
	struct Scsi_Host *host = hba->host;
	DECLARE_COMPLETION_ONSTACK(wait);
	struct request *req;
	unsigned long flags;
	int task_tag, err;

	/*
	 * blk_mq_alloc_request() is used here only to get a free tag.
	 */
	req = blk_mq_alloc_request(q, REQ_OP_DRV_OUT, 0);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->end_io_data = &wait;
	os44_core_hold(hba);

	spin_lock_irqsave(host->host_lock, flags);

	task_tag = req->tag;
	hba->tmf_rqs[req->tag] = req;
	treq->upiu_req.req_header.task_tag = task_tag;

	memcpy(hba->utmrdl_base_addr + task_tag, treq, sizeof(*treq));
	ufshcd_vops_setup_task_mgmt(hba, task_tag, tm_function);

	/* send command to the controller */
	__set_bit(task_tag, &hba->outstanding_tasks);
	ufshcd_writel(hba, 1 << task_tag, REG_UTP_TASK_REQ_DOOR_BELL);

	spin_unlock_irqrestore(host->host_lock, flags);

	os44_ufshcd_add_tm_upiu_trace(hba, task_tag, UFS_TM_SEND);

	/* wait until the task management command is completed */
	err = wait_for_completion_io_timeout(&wait,
			msecs_to_jiffies(TM_CMD_TIMEOUT));
	if (!err) {
		os44_ufshcd_add_tm_upiu_trace(hba, task_tag, UFS_TM_ERR);
		dev_err(hba->dev, "%s: task management cmd 0x%.2x timed-out\n",
				"__ufshcd_issue_tm_cmd", tm_function);
		if (os44_ufshcd_clear_tm_cmd(hba, task_tag))
			dev_WARN(hba->dev, "%s: unable to clear tm cmd (slot %d) after timeout\n",
					"__ufshcd_issue_tm_cmd", task_tag);
		err = -ETIMEDOUT;
	} else {
		err = 0;
		memcpy(treq, hba->utmrdl_base_addr + task_tag, sizeof(*treq));

		os44_ufshcd_add_tm_upiu_trace(hba, task_tag, UFS_TM_COMP);
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->tmf_rqs[req->tag] = NULL;
	__clear_bit(task_tag, &hba->outstanding_tasks);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	local_ufshcd_release(hba);
	blk_mq_free_request(req);

	return err;
}

