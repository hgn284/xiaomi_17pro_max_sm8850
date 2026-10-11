/* SPDX-License-Identifier: GPL-2.0-only */
/* Public-source cleanup plus original WLUN/global-debugfs teardown sequence.
 * New owners remain genuinely unresolved; no fake registration or provider. */
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
void os44_core_hold(struct ufs_hba *);
void os44_core_release_locked(struct ufs_hba *);
void os44_debugfs_hba_exit(struct ufs_hba *);
void os44_ufs_debugfs_exit(void);
int os44_ufshcd_setup_vreg(struct ufs_hba *, bool);
int os44_ufshcd_setup_clocks(struct ufs_hba *, bool);
extern struct scsi_driver os44_ufs_dev_wlun_template;
extern bool os44_ufs_dev_wlun_registered;
static_assert(offsetof(struct ufs_hba, is_powered)==0x238);
static_assert(offsetof(struct ufs_hba, eh_wq)==0x258);
static_assert(offsetof(struct ufs_hba_priv, pm_qos_mutex)==0x1500);
static void os44_pm_qos_exit(struct ufs_hba *hba)
{
	guard(mutex)(&to_hba_priv(hba)->pm_qos_mutex);

	if (!hba->pm_qos_enabled)
		return;

	cpu_latency_qos_remove_request(&hba->pm_qos_req);
	hba->pm_qos_enabled = false;
}

static void ufshcd_remove_clk_scaling_sysfs(struct ufs_hba *hba)
{
	if (hba->clk_scaling.enable_attr.attr.name)
		device_remove_file(hba->dev, &hba->clk_scaling.enable_attr);
}

static void ufshcd_devfreq_remove(struct ufs_hba *hba)
{
	struct list_head *clk_list = &hba->clk_list_head;

	if (!hba->devfreq)
		return;

	devfreq_remove_device(hba->devfreq);
	hba->devfreq = NULL;

	if (!hba->use_pm_opp) {
		struct ufs_clk_info *clki;

		clki = list_first_entry(clk_list, struct ufs_clk_info, list);
		dev_pm_opp_remove(hba->dev, clki->min_freq);
		dev_pm_opp_remove(hba->dev, clki->max_freq);
	}
}

static void ufshcd_exit_clk_scaling(struct ufs_hba *hba)
{
	if (!hba->clk_scaling.is_initialized)
		return;

	ufshcd_remove_clk_scaling_sysfs(hba);
	destroy_workqueue(hba->clk_scaling.workq);
	ufshcd_devfreq_remove(hba);
	hba->clk_scaling.is_initialized = false;
}

static void ufshcd_remove_clk_gating_sysfs(struct ufs_hba *hba)
{
	if (hba->clk_gating.delay_attr.attr.name)
		device_remove_file(hba->dev, &hba->clk_gating.delay_attr);
	if (hba->clk_gating.enable_attr.attr.name)
		device_remove_file(hba->dev, &hba->clk_gating.enable_attr);
}

static void os44_release_for_exit(struct ufs_hba *hba)
{
	guard(spinlock_irqsave)(&hba->clk_gating.lock);
	os44_core_release_locked(hba);
}

static void ufshcd_exit_clk_gating(struct ufs_hba *hba)
{
	if (!hba->clk_gating.is_initialized)
		return;

	ufshcd_remove_clk_gating_sysfs(hba);

	/* Ungate the clock if necessary. */
	os44_core_hold(hba);
	hba->clk_gating.is_initialized = false;
	os44_release_for_exit(hba);

	destroy_workqueue(hba->clk_gating.clk_gating_workq);
}

static void ufshcd_variant_hba_exit(struct ufs_hba *hba)
{
	if (!hba->vops)
		return;

	ufshcd_vops_exit(hba);
}

static int ufshcd_config_vreg_load(struct device *dev, struct ufs_vreg *vreg,
				   int ua)
{
	int ret;

	if (!vreg)
		return 0;

	/*
	 * "set_load" operation shall be required on those regulators
	 * which specifically configured current limitation. Otherwise
	 * zero max_uA may cause unexpected behavior when regulator is
	 * enabled or set as high power mode.
	 */
	if (!vreg->max_uA)
		return 0;

	ret = regulator_set_load(vreg->reg, ua);
	if (ret < 0) {
		dev_err(dev, "%s: %s set load (ua=%d) failed, err=%d\n",
				__func__, vreg->name, ua, ret);
	}

	return ret;
}

static int ufshcd_config_vreg(struct device *dev,
		struct ufs_vreg *vreg, bool on)
{
	if (regulator_count_voltages(vreg->reg) <= 0)
		return 0;

	return ufshcd_config_vreg_load(dev, vreg, on ? vreg->max_uA : 0);
}

static int ufshcd_disable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg || !vreg->enabled || vreg->always_on)
		goto out;

	ret = regulator_disable(vreg->reg);

	if (!ret) {
		/* ignore errors on applying disable config */
		ufshcd_config_vreg(dev, vreg, false);
		vreg->enabled = false;
	} else {
		dev_err(dev, "%s: %s disable failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

static void ufs_put_device_desc(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;

	kfree(dev_info->model);
	dev_info->model = NULL;
}

void os44_ufshcd_hba_exit(struct ufs_hba *hba)
{
	if (hba->is_powered) {
		os44_pm_qos_exit(hba);
		ufshcd_exit_clk_scaling(hba);
		ufshcd_exit_clk_gating(hba);
		if (hba->eh_wq)
			destroy_workqueue(hba->eh_wq);
		os44_ufs_debugfs_exit();
		driver_unregister(&os44_ufs_dev_wlun_template.gendrv);
		os44_ufs_dev_wlun_registered = false;
		os44_debugfs_hba_exit(hba);
		ufshcd_variant_hba_exit(hba);
		os44_ufshcd_setup_vreg(hba, false);
		os44_ufshcd_setup_clocks(hba, false);
		ufshcd_disable_vreg(hba->dev, hba->vreg_info.vdd_hba);
		hba->is_powered = false;
		ufs_put_device_desc(hba);
	}
}
