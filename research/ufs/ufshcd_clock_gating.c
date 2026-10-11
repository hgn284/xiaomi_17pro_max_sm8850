/* SPDX-License-Identifier: GPL-2.0-only */
/* OS44 reconstructed whole functions, stock ELF contracts; no new owner storage. */
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_pm_contract.h"
#include "os44_clock_gating.h"
#include "os44_uic_contract.h"
#include "os44_link_transition_contract.h"
#include "os44_clock_trace.h"
#include "ufsswap_debug_reconstruction.h"
int os44_ufshcd_config_vreg(struct device *, struct ufs_vreg *, bool);
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
static void local_delay_set(struct device *dev, unsigned long value)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 guard(spinlock_irqsave)(&hba->clk_gating.lock);
 hba->clk_gating.delay_ms = value;
}
static bool ufshcd_has_pending_tasks(struct ufs_hba *hba)
{
	return hba->outstanding_tasks || hba->active_uic_cmd ||
	       hba->uic_async_done;
}
static bool ufshcd_is_ufs_dev_busy(struct ufs_hba *hba)
{
	return scsi_host_busy(hba->host) || ufshcd_has_pending_tasks(hba);
}
static int ufshcd_enable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg || vreg->enabled)
		goto out;

	ret = os44_ufshcd_config_vreg(dev, vreg, true);
	if (!ret)
		ret = regulator_enable(vreg->reg);

	if (!ret)
		vreg->enabled = true;
	else
		dev_err(dev, "%s: %s enable failed, err=%d\n",
				"ufshcd_enable_vreg", vreg->name, ret);
out:
	return ret;
}

void os44_ufshcd_hba_vreg_set_hpm(struct ufs_hba *hba)
{
	if (ufshcd_is_link_off(hba) || ufshcd_can_aggressive_pc(hba))
		ufshcd_enable_vreg(hba->dev, hba->vreg_info.vdd_hba);
}

void os44_ufshcd_ungate_work(struct work_struct *work)
{
	int ret;
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
			clk_gating.ungate_work);

	cancel_delayed_work_sync(&hba->clk_gating.gate_work);

	scoped_guard(spinlock_irqsave, &hba->clk_gating.lock) {
		if (hba->clk_gating.state == CLKS_ON)
			return;
	}

	os44_ufshcd_hba_vreg_set_hpm(hba);
	os44_ufshcd_setup_clocks(hba, true);

	local_enable_irq(hba);

	/* Exit from hibern8 */
	if (ufshcd_can_hibern8_during_gating(hba)) {
		/* Prevent gating in this path */
		hba->clk_gating.is_suspended = true;
		if (ufshcd_is_link_hibern8(hba)) {
			ret = os44_ufshcd_uic_hibern8_exit(hba);
			if (ret)
				dev_err(hba->dev, "%s: hibern8 exit failed %d\n",
					"ufshcd_ungate_work", ret);
			else
				ufshcd_set_link_active(hba);
		}
		hba->clk_gating.is_suspended = false;
	}
}

void os44_ufshcd_gate_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
			clk_gating.gate_work.work);
	int ret;
	ktime_t start = ktime_get();

	scoped_guard(spinlock_irqsave, &hba->clk_gating.lock) {
		/*
		 * In case you are here to cancel this work the gating state
		 * would be marked as REQ_CLKS_ON. In this case save time by
		 * skipping the gating work and exit after changing the clock
		 * state to CLKS_ON.
		 */
		if (hba->clk_gating.is_suspended ||
		    hba->clk_gating.state != REQ_CLKS_OFF) {
			hba->clk_gating.state = CLKS_ON;
			trace_ufshcd_clk_gating(hba,
						hba->clk_gating.state);
			return;
		}

		if (hba->clk_gating.active_reqs)
			return;
	}

	scoped_guard(spinlock_irqsave, hba->host->host_lock) {
		if (ufshcd_is_ufs_dev_busy(hba) ||
		    hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL)
			return;
	}

	/* put the link into hibern8 mode before turning off clocks */
	if (ufshcd_can_hibern8_during_gating(hba)) {
		ret = os44_ufshcd_uic_hibern8_enter(hba);
		if (ret) {
			hba->clk_gating.state = CLKS_ON;
			dev_err(hba->dev, "%s: hibern8 enter failed %d\n",
					"ufshcd_gate_work", ret);
			trace_ufshcd_clk_gating(hba,
						hba->clk_gating.state);
			return;
		}
		ufshcd_set_link_hibern8(hba);
	}

	local_disable_irq(hba);

	os44_ufshcd_setup_clocks(hba, false);

	/* Put the host controller in low power mode if possible */
	os44_ufshcd_hba_vreg_set_lpm(hba);
	/*
	 * In case you are here to cancel this work the gating state
	 * would be marked as REQ_CLKS_ON. In this case keep the state
	 * as REQ_CLKS_ON which would anyway imply that clocks are off
	 * and a request to turn them on is pending. By doing this way,
	 * we keep the state machine in tact and this would ultimately
	 * prevent from doing cancel work multiple times when there are
	 * new requests arriving before the current cancel work is done.
	 */
	guard(spinlock_irqsave)(&hba->clk_gating.lock);
	if (hba->clk_gating.state == REQ_CLKS_OFF) {
		hba->clk_gating.state = CLKS_OFF;
		trace_ufshcd_clk_gating(hba,
					hba->clk_gating.state);
	}
 if (os44_ufs_data.last_gate) {
  u64 delta = start - os44_ufs_data.last_gate;
  os44_ufs_data.max_gate_gap = max(delta, os44_ufs_data.max_gate_gap);
  os44_ufs_data.max_gate_gap_alltime = max(delta, os44_ufs_data.max_gate_gap_alltime);
 }
 os44_ufs_data.last_gate = start;

}

ssize_t os44_ufshcd_clkgate_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%lu\n", hba->clk_gating.delay_ms);
}

ssize_t os44_ufshcd_clkgate_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	local_delay_set(dev, value);
	return count;
}

ssize_t os44_ufshcd_clkgate_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", hba->clk_gating.is_enabled);
}

ssize_t os44_ufshcd_clkgate_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	value = !!value;

	guard(spinlock_irqsave)(&hba->clk_gating.lock);

	if (value == hba->clk_gating.is_enabled)
		return count;

	if (value)
		os44_core_release_locked(hba);
	else
		hba->clk_gating.active_reqs++;

	hba->clk_gating.is_enabled = value;

	return count;
}
#define CHECK(m,o) static_assert(offsetof(struct ufs_hba,m)==o)
CHECK(clk_gating.gate_work,0xac0);
CHECK(clk_gating.ungate_work,0xb28);
CHECK(clk_gating.lock,0xb50);
CHECK(clk_gating.state,0xb54);
CHECK(clk_gating.delay_ms,0xb58);
CHECK(clk_gating.is_suspended,0xb60);
CHECK(clk_gating.is_enabled,0xba8);
CHECK(clk_gating.active_reqs,0xbac);
CHECK(vreg_info.vdd_hba,0xa60);
