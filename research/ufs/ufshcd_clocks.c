/* SPDX-License-Identifier: GPL-2.0-only */
/* Real clock lifecycle, variant callbacks, PM QoS and trace providers. */
#include "ufsswap_provider_reconstruction.h"
#include <linux/clk.h>
#include <linux/blk-mq.h>
#include <scsi/scsi_cmnd.h>
#include "os44_command_trace_contract.h"
#include <linux/interrupt.h>
#include <linux/sched/clock.h>
#include <ufs/ufshcd.h>
#include "os44_wlun_contract.h"
#include "os44_pm_contract.h"
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#define CREATE_TRACE_POINTS
#include "os44_clock_trace.h"
static_assert(offsetof(struct ufs_hba_priv, pm_qos_mutex)==0x1500);
static void ufshcd_pm_qos_update(struct ufs_hba *hba, bool on)
{
	guard(mutex)(&to_hba_priv(hba)->pm_qos_mutex);

	if (!hba->pm_qos_enabled)
		return;

	cpu_latency_qos_update_request(&hba->pm_qos_req, on ? 0 : PM_QOS_DEFAULT_VALUE);
}

int os44_ufshcd_setup_clocks(struct ufs_hba *hba, bool on)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	ktime_t start = ktime_get();
	bool clk_state_changed = false;

	if (list_empty(head))
		goto out;

	ret = ufshcd_vops_setup_clocks(hba, on, PRE_CHANGE);
	if (ret)
		return ret;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			/*
			 * Don't disable clocks which are needed
			 * to keep the link active.
			 */
			if (ufshcd_is_link_active(hba) &&
			    clki->keep_link_active)
				continue;

			clk_state_changed = on ^ clki->enabled;
			if (on && !clki->enabled) {
				ret = clk_prepare_enable(clki->clk);
				if (ret) {
					dev_err(hba->dev, "%s: %s prepare enable failed, %d\n",
						"ufshcd_setup_clocks", clki->name, ret);
					goto out;
				}
			} else if (!on && clki->enabled) {
				clk_disable_unprepare(clki->clk);
			}
			clki->enabled = on;
			dev_dbg(hba->dev, "%s: clk: %s %sabled\n", __func__,
					clki->name, on ? "en" : "dis");
		}
	}

	ret = ufshcd_vops_setup_clocks(hba, on, POST_CHANGE);
	if (ret)
		return ret;

	if (!ufshcd_is_clkscaling_supported(hba))
		ufshcd_pm_qos_update(hba, on);
out:
	if (ret) {
		list_for_each_entry(clki, head, list) {
			if (!IS_ERR_OR_NULL(clki->clk) && clki->enabled)
				clk_disable_unprepare(clki->clk);
		}
	} else if (!ret && on && hba->clk_gating.is_initialized) {
		scoped_guard(spinlock_irqsave, &hba->clk_gating.lock)
			hba->clk_gating.state = CLKS_ON;
		trace_ufshcd_clk_gating(hba,
					hba->clk_gating.state);
	}

	if (clk_state_changed)
		trace_ufshcd_profile_clk_gating(hba,
			(on ? "on" : "off"),
			ktime_to_us(ktime_sub(ktime_get(), start)), ret);
	return ret;
}

static void local_enable_irq(struct ufs_hba *hba)
{
	if (!hba->is_irq_enabled) {
		enable_irq(hba->irq);
		hba->is_irq_enabled = true;
	}
}
static void local_disable_irq(struct ufs_hba *hba)
{
	if (hba->is_irq_enabled) {
		disable_irq(hba->irq);
		hba->is_irq_enabled = false;
	}
}
static void local_update_evt_hist(struct ufs_hba *hba, u32 id, u32 val)
{
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];
	e->val[e->pos] = val;
	e->tstamp[e->pos] = local_clock();
	e->cnt += 1;
	e->pos = (e->pos + 1) % UFS_EVENT_HIST_LENGTH;

	ufshcd_vops_event_notify(hba, id, &val);
}

int os44_ufshcd_suspend_core(struct ufs_hba *hba)
{
	int ret;

	if (!hba->is_powered)
		return 0;
	/*
	 * Disable the host irq as host controller as there won't be any
	 * host controller transaction expected till resume.
	 */
	local_disable_irq(hba);
	ret = os44_ufshcd_setup_clocks(hba, false);
	if (ret) {
		local_enable_irq(hba);
		goto out;
	}
	if (ufshcd_is_clkgating_allowed(hba)) {
		hba->clk_gating.state = CLKS_OFF;
		trace_ufshcd_clk_gating(hba,
					hba->clk_gating.state);
	}

	/* Put the host controller in low power mode if possible */
	os44_ufshcd_hba_vreg_set_lpm(hba);
	ufshcd_pm_qos_update(hba, false);
out:
	if (ret)
		local_update_evt_hist(hba, UFS_EVT_SUSPEND_ERR, (u32)ret);
	return ret;
}

#define CHECK(m,v) static_assert(offsetof(struct ufs_hba,m)==(v))
CHECK(caps,0xbb8); CHECK(clk_gating.active_reqs,0xbac);
CHECK(clk_gating.is_suspended,0xb60); CHECK(clk_gating.is_initialized,0xba9);
CHECK(clk_gating.state,0xb54); CHECK(clk_gating.delay_ms,0xb58);
CHECK(clk_gating.gate_work,0xac0); CHECK(clk_gating.ungate_work,0xb28);
CHECK(clk_gating.clk_gating_workq,0xb48); CHECK(outstanding_tasks,0x70);
CHECK(active_uic_cmd,0x1b0); CHECK(uic_async_done,0x1e8); CHECK(ufshcd_state,0x1f0);
static bool ufshcd_has_pending_tasks(struct ufs_hba *hba)
{
	return hba->outstanding_tasks || hba->active_uic_cmd ||
	       hba->uic_async_done;
}
void os44_core_hold(struct ufs_hba *hba)
{
	bool flush_result;
	unsigned long flags;

	if (!ufshcd_is_clkgating_allowed(hba) ||
	    !hba->clk_gating.is_initialized)
		return;
	spin_lock_irqsave(&hba->clk_gating.lock, flags);
	hba->clk_gating.active_reqs++;

start:
	switch (hba->clk_gating.state) {
	case CLKS_ON:
		/*
		 * Wait for the ungate work to complete if in progress.
		 * Though the clocks may be in ON state, the link could
		 * still be in hibner8 state if hibern8 is allowed
		 * during clock gating.
		 * Make sure we exit hibern8 state also in addition to
		 * clocks being ON.
		 */
		if (ufshcd_can_hibern8_during_gating(hba) &&
		    ufshcd_is_link_hibern8(hba)) {
			spin_unlock_irqrestore(&hba->clk_gating.lock, flags);
			flush_result = flush_work(&hba->clk_gating.ungate_work);
			if (hba->clk_gating.is_suspended && !flush_result)
				return;
			spin_lock_irqsave(&hba->clk_gating.lock, flags);
			goto start;
		}
		break;
	case REQ_CLKS_OFF:
		if (cancel_delayed_work(&hba->clk_gating.gate_work)) {
			hba->clk_gating.state = CLKS_ON;
			trace_ufshcd_clk_gating(hba,
						hba->clk_gating.state);
			break;
		}
		/*
		 * If we are here, it means gating work is either done or
		 * currently running. Hence, fall through to cancel gating
		 * work and to enable clocks.
		 */
		fallthrough;
	case CLKS_OFF:
		hba->clk_gating.state = REQ_CLKS_ON;
		trace_ufshcd_clk_gating(hba,
					hba->clk_gating.state);
		queue_work(hba->clk_gating.clk_gating_workq,
			   &hba->clk_gating.ungate_work);
		/*
		 * fall through to check if we should wait for this
		 * work to be done or not.
		 */
		fallthrough;
	case REQ_CLKS_ON:
		spin_unlock_irqrestore(&hba->clk_gating.lock, flags);
		flush_work(&hba->clk_gating.ungate_work);
		/* Make sure state is CLKS_ON before returning */
		spin_lock_irqsave(&hba->clk_gating.lock, flags);
		goto start;
	default:
		dev_err(hba->dev, "%s: clk gating is in invalid state %d\n",
				"ufshcd_hold", hba->clk_gating.state);
		break;
	}
	spin_unlock_irqrestore(&hba->clk_gating.lock, flags);
}
void os44_core_release_locked(struct ufs_hba *hba)
{
	lockdep_assert_held(&hba->clk_gating.lock);

	if (!ufshcd_is_clkgating_allowed(hba))
		return;

	hba->clk_gating.active_reqs--;

	if (hba->clk_gating.active_reqs || hba->clk_gating.is_suspended ||
	    !hba->clk_gating.is_initialized ||
	    hba->clk_gating.state == CLKS_OFF)
		return;

	scoped_guard(spinlock_irqsave, hba->host->host_lock) {
		if (ufshcd_has_pending_tasks(hba) ||
		    hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL)
			return;
	}

	hba->clk_gating.state = REQ_CLKS_OFF;
	trace_ufshcd_clk_gating(hba, hba->clk_gating.state);
	queue_delayed_work(hba->clk_gating.clk_gating_workq,
			   &hba->clk_gating.gate_work,
			   msecs_to_jiffies(hba->clk_gating.delay_ms));
}

void os44_ufshcd_release(struct ufs_hba *hba)
{
	guard(spinlock_irqsave)(&hba->clk_gating.lock);
	os44_core_release_locked(hba);
}

/* Shared command tracer lives with the sole generated tracepoint owner. */
static void ufshcd_add_cmd_upiu_trace(struct ufs_hba *hba, unsigned int tag,
				      enum ufs_trace_str_t str_t)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;
	struct utp_upiu_header *header;

	if (!trace_ufshcd_upiu_enabled())
		return;

	if (str_t == UFS_CMD_SEND)
		header = &rq->header;
	else
		header = &hba->lrb[tag].ucd_rsp_ptr->header;

	trace_ufshcd_upiu(hba, str_t, header, &rq->sc.cdb,
			  UFS_TSF_CDB);
}
void os44_ufshcd_add_command_trace(struct ufs_hba *hba, unsigned int tag,
				     enum ufs_trace_str_t str_t)
{
	u64 lba = 0;
	u8 opcode = 0, group_id = 0;
	u32 doorbell = 0;
	u32 intr;
	int hwq_id = -1;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct request *rq = scsi_cmd_to_rq(cmd);
	int transfer_len = -1;

	if (!cmd)
		return;

	/* trace UPIU also */
	ufshcd_add_cmd_upiu_trace(hba, tag, str_t);
	if (!trace_ufshcd_command_enabled())
		return;

	opcode = cmd->cmnd[0];

	if (opcode == READ_10 || opcode == WRITE_10 ||
				opcode == READ_16 || opcode == WRITE_16) {
		transfer_len =
		       be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
		lba = scsi_get_lba(cmd);
		if (opcode == WRITE_10)
			group_id = lrbp->cmd->cmnd[6];
		if (opcode == WRITE_16)
			group_id = lrbp->cmd->cmnd[14];
	} else if (opcode == UNMAP) {
		/*
		 * The number of Bytes to be unmapped beginning with the lba.
		 */
		transfer_len = blk_rq_bytes(rq);
		lba = scsi_get_lba(cmd);
	}

	intr = ufshcd_readl(hba, REG_INTERRUPT_STATUS);

	if (hba->mcq_enabled) {
		struct ufs_hw_queue *hwq = os44_ufshcd_mcq_req_to_hwq(hba, rq);

		hwq_id = hwq->id;
	} else {
		doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	}
	trace_ufshcd_command(cmd->device, hba, str_t, tag, doorbell, hwq_id,
			     transfer_len, intr, lba, opcode, group_id);
}

#undef CREATE_TRACE_POINTS
#include <trace/hooks/ufshcd.h>
#include "os44_tm_contract.h"
void os44_ufshcd_add_tm_upiu_trace(struct ufs_hba *hba, unsigned int tag,
				     enum ufs_trace_str_t str_t)
{
	struct utp_task_req_desc *descp = &hba->utmrdl_base_addr[tag];

	trace_android_vh_ufs_send_tm_command(hba, tag, (int)str_t);

	if (!trace_ufshcd_upiu_enabled())
		return;

	if (str_t == UFS_TM_SEND)
		trace_ufshcd_upiu(hba, str_t,
				  &descp->upiu_req.req_header,
				  &descp->upiu_req.input_param1,
				  UFS_TSF_TM_INPUT);
	else
		trace_ufshcd_upiu(hba, str_t,
				  &descp->upiu_rsp.rsp_header,
				  &descp->upiu_rsp.output_param1,
				  UFS_TSF_TM_OUTPUT);
}

#include "drivers/ufs/core/ufshcd-crypto.h"
#include "os44_bsg_contract.h"
static void local_ufshcd_release(struct ufs_hba *hba)
{
	guard(spinlock_irqsave)(&hba->clk_gating.lock);
	os44_core_release_locked(hba);
}

static void ufshcd_dev_man_lock(struct ufs_hba *hba)
{
	os44_core_hold(hba);
	mutex_lock(&hba->dev_cmd.lock);
	down_read(&hba->clk_scaling_lock);
}

static void ufshcd_dev_man_unlock(struct ufs_hba *hba)
{
	up_read(&hba->clk_scaling_lock);
	mutex_unlock(&hba->dev_cmd.lock);
	local_ufshcd_release(hba);
}

static void ufshcd_setup_dev_cmd(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
			     enum dev_cmd_type cmd_type, u8 lun, int tag)
{
	lrbp->cmd = NULL;
	lrbp->task_tag = tag;
	lrbp->lun = lun;
	lrbp->intr_cmd = true; /* No interrupt aggregation */
	ufshcd_prepare_lrbp_crypto(NULL, lrbp);
	hba->dev_cmd.type = cmd_type;
}

static void
ufshcd_prepare_req_desc_hdr(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
			    u8 *upiu_flags, enum dma_data_direction cmd_dir,
			    int ehs_length)
{
	struct utp_transfer_req_desc *req_desc = lrbp->utr_descriptor_ptr;
	struct request_desc_header *h = &req_desc->header;
	enum utp_data_direction data_direction;

	lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;

	*h = (typeof(*h)){ };

	if (cmd_dir == DMA_FROM_DEVICE) {
		data_direction = UTP_DEVICE_TO_HOST;
		*upiu_flags = UPIU_CMD_FLAGS_READ;
	} else if (cmd_dir == DMA_TO_DEVICE) {
		data_direction = UTP_HOST_TO_DEVICE;
		*upiu_flags = UPIU_CMD_FLAGS_WRITE;
	} else {
		data_direction = UTP_NO_DATA_TRANSFER;
		*upiu_flags = UPIU_CMD_FLAGS_NONE;
	}

	h->command_type = lrbp->command_type;
	h->data_direction = data_direction;
	h->ehs_length = ehs_length;

	if (lrbp->intr_cmd)
		h->interrupt = 1;

	/* Prepare crypto related dwords */
	ufshcd_prepare_req_desc_hdr_crypto(lrbp, h);

	/*
	 * assigning invalid value for command status. Controller
	 * updates OCS on command completion, with the command
	 * status
	 */
	h->ocs = OCS_INVALID_COMMAND_STATUS;

	req_desc->prd_table_length = 0;
}

static void ufshcd_add_query_upiu_trace(struct ufs_hba *hba,
					enum ufs_trace_str_t str_t,
					struct utp_upiu_req *rq_rsp)
{
	if (!trace_ufshcd_upiu_enabled())
		return;

	trace_ufshcd_upiu(hba, str_t, &rq_rsp->header,
			  &rq_rsp->qr, UFS_TSF_OSF);
}

static int ufshcd_issue_devman_upiu_cmd(struct ufs_hba *hba,
					struct utp_upiu_req *req_upiu,
					struct utp_upiu_req *rsp_upiu,
					u8 *desc_buff, int *buff_len,
					enum dev_cmd_type cmd_type,
					enum query_opcode desc_op)
{
	const u32 tag = hba->reserved_slot;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	int err = 0;
	unsigned int timeout;
	u8 upiu_flags;

	/* Protects use of hba->reserved_slot. */
	lockdep_assert_held(&hba->dev_cmd.lock);

	ufshcd_setup_dev_cmd(hba, lrbp, cmd_type, 0, tag);

	ufshcd_prepare_req_desc_hdr(hba, lrbp, &upiu_flags, DMA_NONE, 0);

	/* update the task tag in the request upiu */
	req_upiu->header.task_tag = tag;

	/* just copy the upiu request as it is */
	memcpy(lrbp->ucd_req_ptr, req_upiu, sizeof(*lrbp->ucd_req_ptr));
	if (desc_buff && desc_op == UPIU_QUERY_OPCODE_WRITE_DESC) {
		/* The Data Segment Area is optional depending upon the query
		 * function value. for WRITE DESCRIPTOR, the data segment
		 * follows right after the tsf.
		 */
		memcpy(lrbp->ucd_req_ptr + 1, desc_buff, *buff_len);
		*buff_len = 0;
	}

	timeout = os44_core_dev_cmd_timeout;
	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));

	/*
	 * ignore the returning value here - ufshcd_check_query_response is
	 * bound to fail since dev_cmd.query and dev_cmd.type were left empty.
	 * read the response directly ignoring all errors.
	 */
	os44_core_issue_dev_cmd(hba, lrbp, tag, timeout);

	/* just copy the upiu response as it is */
	memcpy(rsp_upiu, lrbp->ucd_rsp_ptr, sizeof(*rsp_upiu));
	if (desc_buff && desc_op == UPIU_QUERY_OPCODE_READ_DESC) {
		u8 *descp = (u8 *)lrbp->ucd_rsp_ptr + sizeof(*rsp_upiu);
		u16 resp_len = be16_to_cpu(lrbp->ucd_rsp_ptr->header
					   .data_segment_length);

		if (*buff_len >= resp_len) {
			memcpy(desc_buff, descp, resp_len);
			*buff_len = resp_len;
		} else {
			dev_warn(hba->dev,
				 "%s: rsp size %d is bigger than buffer size %d",
				 "ufshcd_issue_devman_upiu_cmd", resp_len, *buff_len);
			*buff_len = 0;
			err = -EINVAL;
		}
	}

	ufshcd_add_query_upiu_trace(hba, err ? UFS_QUERY_ERR : UFS_QUERY_COMP, (struct utp_upiu_req *)lrbp->ucd_rsp_ptr);
	return err;
}

int os44_ufshcd_exec_raw_upiu_cmd(struct ufs_hba *hba,
			     struct utp_upiu_req *req_upiu,
			     struct utp_upiu_req *rsp_upiu,
			     enum upiu_request_transaction msgcode,
			     u8 *desc_buff, int *buff_len,
			     enum query_opcode desc_op)
{
	int err;
	enum dev_cmd_type cmd_type = DEV_CMD_TYPE_QUERY;
	struct utp_task_req_desc treq = { };
	enum utp_ocs ocs_value;
	u8 tm_f = req_upiu->header.tm_function;

	switch (msgcode) {
	case UPIU_TRANSACTION_NOP_OUT:
		cmd_type = DEV_CMD_TYPE_NOP;
		fallthrough;
	case UPIU_TRANSACTION_QUERY_REQ:
		ufshcd_dev_man_lock(hba);
		err = ufshcd_issue_devman_upiu_cmd(hba, req_upiu, rsp_upiu,
						   desc_buff, buff_len,
						   cmd_type, desc_op);
		ufshcd_dev_man_unlock(hba);

		break;
	case UPIU_TRANSACTION_TASK_REQ:
		treq.header.interrupt = 1;
		treq.header.ocs = OCS_INVALID_COMMAND_STATUS;

		memcpy(&treq.upiu_req, req_upiu, sizeof(*req_upiu));

		err = os44___ufshcd_issue_tm_cmd(hba, &treq, tm_f);
		if (err == -ETIMEDOUT)
			break;

		ocs_value = treq.header.ocs & MASK_OCS;
		if (ocs_value != OCS_SUCCESS) {
			dev_err(hba->dev, "%s: failed, ocs = 0x%x\n", "ufshcd_exec_raw_upiu_cmd",
				ocs_value);
			break;
		}

		memcpy(rsp_upiu, &treq.upiu_rsp, sizeof(*rsp_upiu));

		break;
	default:
		err = -EINVAL;

		break;
	}

	return err;
}


#include <linux/pm_opp.h>
#include <linux/devfreq.h>
#include "os44_reset_prereqs.h"
static int ufshcd_set_clk_freq(struct ufs_hba *hba, bool scale_up)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (scale_up && clki->max_freq) {
				if (clki->curr_freq == clki->max_freq)
					continue;

				ret = clk_set_rate(clki->clk, clki->max_freq);
				if (ret) {
					dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
						"ufshcd_set_clk_freq", clki->name,
						clki->max_freq, ret);
					break;
				}
				trace_ufshcd_clk_scaling(hba,
						"scaled up", clki->name,
						clki->curr_freq,
						clki->max_freq);

				clki->curr_freq = clki->max_freq;

			} else if (!scale_up && clki->min_freq) {
				if (clki->curr_freq == clki->min_freq)
					continue;

				ret = clk_set_rate(clki->clk, clki->min_freq);
				if (ret) {
					dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
						"ufshcd_set_clk_freq", clki->name,
						clki->min_freq, ret);
					break;
				}
				trace_ufshcd_clk_scaling(hba,
						"scaled down", clki->name,
						clki->curr_freq,
						clki->min_freq);
				clki->curr_freq = clki->min_freq;
			}
		}
		dev_dbg(hba->dev, "%s: clk: %s, rate: %lu\n", "ufshcd_set_clk_freq",
				clki->name, clk_get_rate(clki->clk));
	}

out:
	return ret;
}

static int ufshcd_opp_set_rate(struct ufs_hba *hba, unsigned long freq)
{
	struct dev_pm_opp *opp;
	int ret;

	opp = dev_pm_opp_find_freq_floor_indexed(hba->dev,
						 &freq, 0);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	ret = dev_pm_opp_set_opp(hba->dev, opp);
	dev_pm_opp_put(opp);

	return ret;
}

int os44_ufshcd_scale_clks(struct ufs_hba *hba, unsigned long freq,
			     bool scale_up)
{
	int ret = 0;
	ktime_t start = ktime_get();

	ret = ufshcd_vops_clk_scale_notify(hba, scale_up, freq, PRE_CHANGE);
	if (ret)
		goto out;

	if (hba->use_pm_opp)
		ret = ufshcd_opp_set_rate(hba, freq);
	else
		ret = ufshcd_set_clk_freq(hba, scale_up);
	if (ret)
		goto out;

	ret = ufshcd_vops_clk_scale_notify(hba, scale_up, freq, POST_CHANGE);
	if (ret) {
		if (hba->use_pm_opp)
			ufshcd_opp_set_rate(hba,
					    hba->devfreq->previous_freq);
		else
			ufshcd_set_clk_freq(hba, !scale_up);
		goto out;
	}

	ufshcd_pm_qos_update(hba, scale_up);

out:
	trace_ufshcd_profile_clk_scaling(hba,
			(scale_up ? "up" : "down"),
			ktime_to_us(ktime_sub(ktime_get(), start)), ret);
	return ret;
}
