/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole interrupt reconstruction; no storage fabricated and no error bypass. */
#include <linux/blk-mq.h>
#include <linux/sched/clock.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include <trace/hooks/ufshcd.h>
#include "os44_irq_contract.h"
#include "os44_uic_contract.h"
#include "os44_reset_prereqs.h"
#undef INT_FATAL_ERRORS
#define INT_FATAL_ERRORS 0x70880
#undef UFSHCD_ERROR_MASK
#define UFSHCD_ERROR_MASK 0x70884
extern int os44_ufshcd_poll(struct Scsi_Host *, unsigned int);
extern unsigned long os44_ufshcd_mcq_poll_cqe_lock(struct ufs_hba *, struct ufs_hw_queue *);

enum { UFSHCD_UIC_DL_PA_INIT_ERROR=1, UFSHCD_UIC_DL_NAC_RECEIVED_ERROR=2,
 UFSHCD_UIC_DL_TCx_REPLAY_ERROR=4, UFSHCD_UIC_NL_ERROR=8,
 UFSHCD_UIC_TL_ERROR=16, UFSHCD_UIC_DME_ERROR=32, UFSHCD_UIC_PA_GENERIC_ERROR=64 };
#define UFSHCD_POLL_FROM_INTERRUPT_CONTEXT (-1)
#define ufshcd_eh_in_progress(h) ((h)->eh_flags & 1)
static inline u8 ufshcd_get_upmcrs(struct ufs_hba *hba)
{ return (ufshcd_readl(hba, REG_CONTROLLER_STATUS) >> 8) & 7; }
static inline void ufshcd_reset_intr_aggr(struct ufs_hba *hba)
{ ufshcd_writel(hba, 0x80010000, REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL); }

static inline void local_update_evt_hist(struct ufs_hba *hba, u32 id, u32 val)
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

static bool ufshcd_is_auto_hibern8_error(struct ufs_hba *hba,
					 u32 intr_mask)
{
	if (!ufshcd_is_auto_hibern8_supported(hba) ||
	    !ufshcd_is_auto_hibern8_enabled(hba))
		return false;

	if (!(intr_mask & UFSHCD_UIC_HIBERN8_MASK))
		return false;

	if (hba->active_uic_cmd &&
	    (hba->active_uic_cmd->command == UIC_CMD_DME_HIBER_ENTER ||
	    hba->active_uic_cmd->command == UIC_CMD_DME_HIBER_EXIT))
		return false;

	return true;
}

static irqreturn_t ufshcd_uic_cmd_compl(struct ufs_hba *hba, u32 intr_status)
{
	irqreturn_t retval = IRQ_NONE;
	struct uic_command *cmd;

	spin_lock(hba->host->host_lock);
	cmd = hba->active_uic_cmd;
	if (!cmd)
		goto unlock;

	if (ufshcd_is_auto_hibern8_error(hba, intr_status))
		hba->errors |= (UFSHCD_UIC_HIBERN8_MASK & intr_status);

	if (intr_status & UIC_COMMAND_COMPL) {
		/*
		 * Store the UIC command result in the lowest byte of
		 * cmd->argument2.
		 */
		cmd->argument2 |= ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2) &
				  MASK_UIC_COMMAND_RESULT;
		/* Store the DME attribute value in cmd->argument3. */
		cmd->argument3 = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);
		if (!hba->uic_async_done)
			cmd->cmd_active = 0;
		complete(&cmd->done);
		retval = IRQ_HANDLED;
	}

	if (intr_status & UFSHCD_UIC_PWR_MASK && hba->uic_async_done) {
		cmd->cmd_active = 0;
		complete(hba->uic_async_done);
		retval = IRQ_HANDLED;
	}

	if (retval == IRQ_HANDLED)
		os44_ufshcd_add_uic_command_trace(hba, cmd, UFS_CMD_COMP);

unlock:
	spin_unlock(hba->host->host_lock);

	return retval;
}

static inline bool ufshcd_is_saved_err_fatal(struct ufs_hba *hba)
{
	return (hba->saved_uic_err & UFSHCD_UIC_DL_PA_INIT_ERROR) ||
	       (hba->saved_err & (INT_FATAL_ERRORS | UFSHCD_UIC_HIBERN8_MASK));
}

static inline void local_schedule_eh_work(struct ufs_hba *hba)
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

static irqreturn_t ufshcd_update_uic_error(struct ufs_hba *hba)
{
	u32 reg;
	irqreturn_t retval = IRQ_NONE;

	/* PHY layer error */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER);
	if ((reg & UIC_PHY_ADAPTER_LAYER_ERROR) &&
	    (reg & UIC_PHY_ADAPTER_LAYER_ERROR_CODE_MASK)) {
		struct os44_irq_private_view *p = ufshcd_get_variant(hba);
        unsigned long bits = reg & 0x1f; unsigned int i;
        for_each_set_bit(i, &bits, 5) { p->pa_total++; p->pa_bits[i]++; }
        local_update_evt_hist(hba, UFS_EVT_PA_ERR, reg);
		/*
		 * To know whether this error is fatal or not, DB timeout
		 * must be checked but this error is handled separately.
		 */
		if (reg & UIC_PHY_ADAPTER_LAYER_LANE_ERR_MASK)
			dev_dbg(hba->dev, "%s: UIC Lane error reported\n",
					__func__);

		/* Got a LINERESET indication. */
		if (reg & UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR) {
			struct uic_command *cmd = NULL;

			hba->uic_error |= UFSHCD_UIC_PA_GENERIC_ERROR;
			if (hba->uic_async_done && hba->active_uic_cmd)
				cmd = hba->active_uic_cmd;
			/*
			 * Ignore the LINERESET during power mode change
			 * operation via DME_SET command.
			 */
			if (cmd && (cmd->command == UIC_CMD_DME_SET))
				hba->uic_error &= ~UFSHCD_UIC_PA_GENERIC_ERROR;
		}
		retval |= IRQ_HANDLED;
	}

	/* PA_INIT_ERROR is fatal and needs UIC reset */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_DATA_LINK_LAYER);
	if ((reg & UIC_DATA_LINK_LAYER_ERROR) &&
	    (reg & UIC_DATA_LINK_LAYER_ERROR_CODE_MASK)) {
		struct os44_irq_private_view *p = ufshcd_get_variant(hba);
        unsigned long bits = reg & 0x7fff; unsigned int i;
        for_each_set_bit(i, &bits, 15) { p->dl_total++; p->dl_bits[i]++; }
        local_update_evt_hist(hba, UFS_EVT_DL_ERR, reg);

		if (reg & UIC_DATA_LINK_LAYER_ERROR_PA_INIT)
			hba->uic_error |= UFSHCD_UIC_DL_PA_INIT_ERROR;
		else if (hba->dev_quirks &
				UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS) {
			if (reg & UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED)
				hba->uic_error |=
					UFSHCD_UIC_DL_NAC_RECEIVED_ERROR;
			else if (reg & UIC_DATA_LINK_LAYER_ERROR_TCx_REPLAY_TIMEOUT)
				hba->uic_error |= UFSHCD_UIC_DL_TCx_REPLAY_ERROR;
		}
		retval |= IRQ_HANDLED;
	}

	/* UIC NL/TL/DME errors needs software retry */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_NETWORK_LAYER);
	if ((reg & UIC_NETWORK_LAYER_ERROR) &&
	    (reg & UIC_NETWORK_LAYER_ERROR_CODE_MASK)) {
		local_update_evt_hist(hba, UFS_EVT_NL_ERR, reg);
		hba->uic_error |= UFSHCD_UIC_NL_ERROR;
		retval |= IRQ_HANDLED;
	}

	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_TRANSPORT_LAYER);
	if ((reg & UIC_TRANSPORT_LAYER_ERROR) &&
	    (reg & UIC_TRANSPORT_LAYER_ERROR_CODE_MASK)) {
		local_update_evt_hist(hba, UFS_EVT_TL_ERR, reg);
		hba->uic_error |= UFSHCD_UIC_TL_ERROR;
		retval |= IRQ_HANDLED;
	}

	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_DME);
	if ((reg & UIC_DME_ERROR) &&
	    (reg & UIC_DME_ERROR_CODE_MASK)) {
		struct os44_irq_private_view *p = ufshcd_get_variant(hba);
        p->dme_total++;
        local_update_evt_hist(hba, UFS_EVT_DME_ERR, reg);
		hba->uic_error |= UFSHCD_UIC_DME_ERROR;
		retval |= IRQ_HANDLED;
	}

	dev_dbg(hba->dev, "%s: UIC error flags = 0x%08x\n",
			__func__, hba->uic_error);
	return retval;
}

static irqreturn_t ufshcd_check_errors(struct ufs_hba *hba, u32 intr_status)
{
	bool queue_eh_work = false;
	irqreturn_t retval = IRQ_NONE;

	spin_lock(hba->host->host_lock);
	hba->errors |= UFSHCD_ERROR_MASK & intr_status;

	if (hba->errors & INT_FATAL_ERRORS) {
		local_update_evt_hist(hba, UFS_EVT_FATAL_ERR,
				       hba->errors);
		queue_eh_work = true;
	}

	if (hba->errors & UIC_ERROR) {
		hba->uic_error = 0;
		retval = ufshcd_update_uic_error(hba);
		if (hba->uic_error)
			queue_eh_work = true;
	}

	if (hba->errors & UFSHCD_UIC_HIBERN8_MASK) {
		dev_err(hba->dev,
			"%s: Auto Hibern8 %s failed - status: 0x%08x, upmcrs: 0x%08x\n",
			__func__, (hba->errors & UIC_HIBERNATE_ENTER) ?
			"Enter" : "Exit",
			hba->errors, ufshcd_get_upmcrs(hba));
		local_update_evt_hist(hba, UFS_EVT_AUTO_HIBERN8_ERR,
				       hba->errors);
		ufshcd_set_link_broken(hba);
		queue_eh_work = true;
	}

	trace_android_vh_ufs_check_int_errors(hba, queue_eh_work);

	if (queue_eh_work) {
		/*
		 * update the transfer error masks to sticky bits, let's do this
		 * irrespective of current ufshcd_state.
		 */
		hba->saved_err |= hba->errors;
		hba->saved_uic_err |= hba->uic_error;

		/* dump controller state before resetting */
		if ((hba->saved_err &
		     (INT_FATAL_ERRORS | UFSHCD_UIC_HIBERN8_MASK)) ||
		    (hba->saved_uic_err &&
		     (hba->saved_uic_err != UFSHCD_UIC_PA_GENERIC_ERROR))) {
			dev_err(hba->dev, "%s: saved_err 0x%x saved_uic_err 0x%x\n",
					__func__, hba->saved_err,
					hba->saved_uic_err);
			os44_ufshcd_dump_regs(hba, 0, UFSHCI_REG_SPACE_SIZE,
					 "host_regs: ");
		}
		local_schedule_eh_work(hba);
		retval |= IRQ_HANDLED;
	}
	/*
	 * if (!queue_eh_work) -
	 * Other errors are either non-fatal where host recovers
	 * itself without s/w intervention or errors that will be
	 * handled by the SCSI core layer.
	 */
	hba->errors = 0;
	hba->uic_error = 0;
	spin_unlock(hba->host->host_lock);
	return retval;
}

static irqreturn_t ufshcd_transfer_req_compl(struct ufs_hba *hba)
{
	/* Resetting interrupt aggregation counters first and reading the
	 * DOOR_BELL afterward allows us to handle all the completed requests.
	 * In order to prevent other interrupts starvation the DB is read once
	 * after reset. The down side of this solution is the possibility of
	 * false interrupt if device completes another request after resetting
	 * aggregation and before reading the DB.
	 */
	if (ufshcd_is_intr_aggr_allowed(hba) &&
	    !(hba->quirks & UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR))
		ufshcd_reset_intr_aggr(hba);

	/*
	 * Ignore the ufshcd_poll() return value and return IRQ_HANDLED since we
	 * do not want polling to trigger spurious interrupt complaints.
	 */
	os44_ufshcd_poll(hba->host, UFSHCD_POLL_FROM_INTERRUPT_CONTEXT);

	return IRQ_HANDLED;
}

static irqreturn_t ufshcd_handle_mcq_cq_events(struct ufs_hba *hba)
{
	struct ufs_hw_queue *hwq;
	unsigned long outstanding_cqs;
	unsigned int nr_queues;
	int i, ret;
	u32 events;

	ret = ufshcd_vops_get_outstanding_cqs(hba, &outstanding_cqs);
	if (ret)
		outstanding_cqs = (1U << hba->nr_hw_queues) - 1;

	/* Exclude the poll queues */
	nr_queues = hba->nr_hw_queues - hba->nr_queues[HCTX_TYPE_POLL];
	for_each_set_bit(i, &outstanding_cqs, nr_queues) {
		hwq = &hba->uhq[i];

		events = os44_ufshcd_mcq_read_cqis(hba, i);
		if (events)
			os44_ufshcd_mcq_write_cqis(hba, events, i);

		if (events & UFSHCD_MCQ_CQIS_TAIL_ENT_PUSH_STS)
			os44_ufshcd_mcq_poll_cqe_lock(hba, hwq);
	}

	return IRQ_HANDLED;
}

static irqreturn_t ufshcd_sl_intr(struct ufs_hba *hba, u32 intr_status)
{
	irqreturn_t retval = IRQ_NONE;

	if (intr_status & UFSHCD_UIC_MASK)
		retval |= ufshcd_uic_cmd_compl(hba, intr_status);

	if (intr_status & UFSHCD_ERROR_MASK || hba->errors)
		retval |= ufshcd_check_errors(hba, intr_status);

	if (intr_status & UTP_TASK_REQ_COMPL)
		retval |= os44_ufshcd_tmc_handler(hba);

	if (intr_status & UTP_TRANSFER_REQ_COMPL)
		retval |= ufshcd_transfer_req_compl(hba);

	if (intr_status & MCQ_CQ_EVENT_STATUS)
		retval |= ufshcd_handle_mcq_cq_events(hba);

	return retval;
}

irqreturn_t os44_ufshcd_intr(int irq, void *__hba)
{
	u32 intr_status, enabled_intr_status = 0;
	irqreturn_t retval = IRQ_NONE;
	struct ufs_hba *hba = __hba;
	int retries = hba->nutrs;

	intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
	hba->ufs_stats.last_intr_status = intr_status;
	hba->ufs_stats.last_intr_ts = local_clock();

	/*
	 * There could be max of hba->nutrs reqs in flight and in worst case
	 * if the reqs get finished 1 by 1 after the interrupt status is
	 * read, make sure we handle them by checking the interrupt status
	 * again in a loop until we process all of the reqs before returning.
	 */
	while (intr_status && retries--) {
		enabled_intr_status =
			intr_status & ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
		ufshcd_writel(hba, intr_status, REG_INTERRUPT_STATUS);
		if (enabled_intr_status)
			retval |= ufshcd_sl_intr(hba, enabled_intr_status);

		intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
	}

	if (enabled_intr_status && retval == IRQ_NONE &&
	    (!(enabled_intr_status & UTP_TRANSFER_REQ_COMPL) ||
	     hba->outstanding_reqs) && !ufshcd_eh_in_progress(hba)) {
		dev_err(hba->dev, "%s: Unhandled interrupt 0x%08x (0x%08x, 0x%08x)\n",
					"ufshcd_intr",
					intr_status,
					hba->ufs_stats.last_intr_status,
					enabled_intr_status);
		os44_ufshcd_dump_regs(hba, 0, UFSHCI_REG_SPACE_SIZE, "host_regs: ");
	}

	return retval;
}
