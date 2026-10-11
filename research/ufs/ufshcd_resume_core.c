// SPDX-License-Identifier: GPL-2.0-only
/* Whole stock resume core: regulator failures roll back VCC/VCCQ;
 * clock failure records the event WITHOUT public-kernel LPM rollback. */
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>
#include <linux/interrupt.h>
#include "os44_clock_gating.h"
#include "os44_pm_contract.h"
#include "os44_resume_pm.h"
#define ufshcd_toggle_vreg(dev, vreg, on) ((on) ? ufshcd_enable_vreg((dev),(vreg)) : ufshcd_disable_vreg((dev),(vreg)))
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

static inline int ufshcd_config_vreg_lpm(struct ufs_hba *hba,
					 struct ufs_vreg *vreg)
{
	return ufshcd_config_vreg_load(hba->dev, vreg, UFS_VREG_LPM_LOAD_UA);
}

static inline int ufshcd_config_vreg_hpm(struct ufs_hba *hba,
					 struct ufs_vreg *vreg)
{
	if (!vreg)
		return 0;

	return ufshcd_config_vreg_load(hba->dev, vreg, vreg->max_uA);
}

static int ufshcd_config_vreg(struct device *dev,
		struct ufs_vreg *vreg, bool on)
{
	if (regulator_count_voltages(vreg->reg) <= 0)
		return 0;

	return ufshcd_config_vreg_load(dev, vreg, on ? vreg->max_uA : 0);
}

static int ufshcd_enable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg || vreg->enabled)
		goto out;

	ret = ufshcd_config_vreg(dev, vreg, true);
	if (!ret)
		ret = regulator_enable(vreg->reg);

	if (!ret)
		vreg->enabled = true;
	else
		dev_err(dev, "%s: %s enable failed, err=%d\n",
				__func__, vreg->name, ret);
out:
	return ret;
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

static int ufshcd_vreg_set_hpm(struct ufs_hba *hba)
{
	int ret = 0;

	if (ufshcd_is_ufs_dev_poweroff(hba) && ufshcd_is_link_off(hba) &&
	    !hba->dev_info.is_lu_power_on_wp) {
		ret = os44_ufshcd_setup_vreg(hba, true);
	} else if (!ufshcd_is_ufs_dev_active(hba)) {
		if (!ufshcd_is_link_active(hba)) {
			ret = ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq);
			if (ret)
				goto vcc_disable;
			ret = ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq2);
			if (ret)
				goto vccq_lpm;
		}
		ret = ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, true);
	}
	goto out;

vccq_lpm:
	ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq);
vcc_disable:
	ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, false);
out:
	return ret;
}

static inline void resume_update_evt_hist(struct ufs_hba *hba, u32 id, u32 val)
{
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];
	e->val[e->pos] = val;
	e->tstamp[e->pos] = local_clock();
	e->cnt += 1;
	e->pos = (e->pos + 1) % UFS_EVENT_HIST_LENGTH;

	if (hba->vops && hba->vops->event_notify)
		hba->vops->event_notify(hba, id, &val);
}

static inline void resume_enable_irq(struct ufs_hba *hba)
{
	if (!hba->is_irq_enabled) {
		enable_irq(hba->irq);
		hba->is_irq_enabled = true;
	}
}

int os44_ufshcd_resume_core(struct ufs_hba *hba)
{
	int ret;

	if (!hba->is_powered)
		return 0;

	os44_ufshcd_hba_vreg_set_hpm(hba);
	ret = ufshcd_vreg_set_hpm(hba);
	if (ret)
		goto out;

	/* Make sure clocks are enabled before accessing controller */
	ret = os44_ufshcd_setup_clocks(hba, true);
	if (ret)
		goto out;

	/* enable the host irq as host controller would be active soon */
	resume_enable_irq(hba);

	goto out;

out:
	if (ret)
		resume_update_evt_hist(hba, UFS_EVT_RESUME_ERR, (u32)ret);
	return ret;
}

static_assert(offsetof(struct ufs_hba,is_powered)==0x238);
static_assert(offsetof(struct ufs_hba,vreg_info.vcc)==0xa48);
static_assert(offsetof(struct ufs_hba,ufs_stats.event[UFS_EVT_RESUME_ERR].pos)==0x650);
