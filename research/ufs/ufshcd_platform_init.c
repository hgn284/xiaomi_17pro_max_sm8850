// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Universal Flash Storage Host controller Platform bus based glue driver
 * Copyright (C) 2011-2013 Samsung India Software Operations
 *
 * Authors:
 *	Santosh Yaraganavi <santosh.sy@samsung.com>
 *	Vinayak Holikatti <h.vinayak@samsung.com>
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>

#include <ufs/ufshcd.h>
#include "drivers/ufs/host/ufshcd-pltfrm.h"
#include <ufs/unipro.h>

#define UFSHCD_DEFAULT_LANES_PER_DIRECTION		2

static int ufshcd_parse_clock_info(struct ufs_hba *hba)
{
	int ret = 0;
	int cnt;
	int i;
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	const char *name;
	u32 *clkfreq = NULL;
	struct ufs_clk_info *clki;
	ssize_t sz = 0;

	if (!np)
		goto out;

	cnt = of_property_count_strings(np, "clock-names");
	if (!cnt || (cnt == -EINVAL)) {
		dev_info(dev, "%s: Unable to find clocks, assuming enabled\n",
				__func__);
	} else if (cnt < 0) {
		dev_err(dev, "%s: count clock strings failed, err %d\n",
				__func__, cnt);
		ret = cnt;
	}

	if (cnt <= 0)
		goto out;

	sz = of_property_count_u32_elems(np, "freq-table-hz");
	if (sz <= 0) {
		dev_info(dev, "freq-table-hz property not specified\n");
		goto out;
	}

	if (sz != 2 * cnt) {
		dev_err(dev, "%s len mismatch\n", "freq-table-hz");
		ret = -EINVAL;
		goto out;
	}

	clkfreq = devm_kcalloc(dev, sz, sizeof(*clkfreq),
			       GFP_KERNEL);
	if (!clkfreq) {
		ret = -ENOMEM;
		goto out;
	}

	ret = of_property_read_u32_array(np, "freq-table-hz",
			clkfreq, sz);
	if (ret && (ret != -EINVAL)) {
		dev_err(dev, "%s: error reading array %d\n",
				"freq-table-hz", ret);
		return ret;
	}

	for (i = 0; i < sz; i += 2) {
		ret = of_property_read_string_index(np,	"clock-names", i/2,
						    &name);
		if (ret)
			goto out;

		clki = devm_kzalloc(dev, sizeof(*clki), GFP_KERNEL);
		if (!clki) {
			ret = -ENOMEM;
			goto out;
		}

		clki->min_freq = clkfreq[i];
		clki->max_freq = clkfreq[i+1];
		clki->name = devm_kstrdup(dev, name, GFP_KERNEL);
		if (!clki->name) {
			ret = -ENOMEM;
			goto out;
		}

		if (!strcmp(name, "ref_clk"))
			clki->keep_link_active = true;
		dev_dbg(dev, "%s: min %u max %u name %s\n", "freq-table-hz",
				clki->min_freq, clki->max_freq, clki->name);
		list_add_tail(&clki->list, &hba->clk_list_head);
	}
out:
	return ret;
}

static int ufshcd_parse_regulator_info(struct ufs_hba *hba)
{
	int err;
	struct device *dev = hba->dev;
	struct ufs_vreg_info *info = &hba->vreg_info;

	err = ufshcd_populate_vreg(dev, "vdd-hba", &info->vdd_hba, true);
	if (err)
		goto out;

	err = ufshcd_populate_vreg(dev, "vcc", &info->vcc, false);
	if (err)
		goto out;

	err = ufshcd_populate_vreg(dev, "vccq", &info->vccq, false);
	if (err)
		goto out;

	err = ufshcd_populate_vreg(dev, "vccq2", &info->vccq2, false);
out:
	return err;
}

static void ufshcd_init_lanes_per_dir(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	int ret;

	ret = of_property_read_u32(dev->of_node, "lanes-per-direction",
		&hba->lanes_per_direction);
	if (ret) {
		dev_dbg(hba->dev,
			"%s: failed to read lanes-per-direction, ret=%d\n",
			__func__, ret);
		hba->lanes_per_direction = UFSHCD_DEFAULT_LANES_PER_DIRECTION;
	}
}

/**
 * ufshcd_parse_clock_min_max_freq  - Parse MIN and MAX clocks freq
 * @hba: per adapter instance
 *
 * This function parses MIN and MAX frequencies of all clocks required
 * by the host drivers.
 *
 * Returns 0 for success and non-zero for failure
 */
static int ufshcd_parse_clock_min_max_freq(struct ufs_hba *hba)
{
	struct list_head *head = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	struct dev_pm_opp *opp;
	unsigned long freq;
	u8 idx = 0;

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		clki->clk = devm_clk_get(hba->dev, clki->name);
		if (IS_ERR(clki->clk))
			continue;

		/* Find Max Freq */
		freq = ULONG_MAX;
		opp = dev_pm_opp_find_freq_floor_indexed(hba->dev, &freq, idx);
		if (IS_ERR(opp)) {
			dev_err(hba->dev, "Failed to find OPP for MAX frequency\n");
			return PTR_ERR(opp);
		}
		clki->max_freq = dev_pm_opp_get_freq_indexed(opp, idx);
		dev_pm_opp_put(opp);

		/* Find Min Freq */
		freq = 0;
		opp = dev_pm_opp_find_freq_ceil_indexed(hba->dev, &freq, idx);
		if (IS_ERR(opp)) {
			dev_err(hba->dev, "Failed to find OPP for MIN frequency\n");
			return PTR_ERR(opp);
		}
		clki->min_freq = dev_pm_opp_get_freq_indexed(opp, idx++);
		dev_pm_opp_put(opp);
	}

	return 0;
}

static int ufshcd_parse_operating_points(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct dev_pm_opp_config config = {};
	struct ufs_clk_info *clki;
	const char **clk_names;
	int cnt, i, ret;

	if (!of_property_present(np, "operating-points-v2"))
		return 0;

	if (of_property_present(np, "freq-table-hz")) {
		dev_err(dev, "%s: operating-points and freq-table-hz are incompatible\n",
			 __func__);
		return -EINVAL;
	}

	cnt = of_property_count_strings(np, "clock-names");
	if (cnt <= 0) {
		dev_err(dev, "%s: Missing clock-names\n",  __func__);
		return -ENODEV;
	}

	/* OPP expects clk_names to be NULL terminated */
	clk_names = devm_kcalloc(dev, cnt + 1, sizeof(*clk_names), GFP_KERNEL);
	if (!clk_names)
		return -ENOMEM;

	/*
	 * We still need to get reference to all clocks as the UFS core uses
	 * them separately.
	 */
	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(np, "clock-names", i,
						    &clk_names[i]);
		if (ret)
			return ret;

		clki = devm_kzalloc(dev, sizeof(*clki), GFP_KERNEL);
		if (!clki)
			return -ENOMEM;

		clki->name = devm_kstrdup(dev, clk_names[i], GFP_KERNEL);
		if (!clki->name)
			return -ENOMEM;

		if (!strcmp(clk_names[i], "ref_clk"))
			clki->keep_link_active = true;

		list_add_tail(&clki->list, &hba->clk_list_head);
	}

	config.clk_names = clk_names,
	config.config_clks = ufshcd_opp_config_clks;

	ret = devm_pm_opp_set_config(dev, &config);
	if (ret)
		return ret;

	ret = devm_pm_opp_of_add_table(dev);
	if (ret) {
		dev_err(dev, "Failed to add OPP table: %d\n", ret);
		return ret;
	}

	ret = ufshcd_parse_clock_min_max_freq(hba);
	if (ret)
		return ret;

	hba->use_pm_opp = true;

	return 0;
}


#include "os44_core_init.h"
int os44_ufshcd_pltfrm_init(struct platform_device *pdev,
		       const struct ufs_hba_variant_ops *vops)
{
	struct ufs_hba *hba = NULL;
	void __iomem *mmio_base;
	int irq, err;
	struct device *dev = &pdev->dev;

	mmio_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mmio_base))
		return PTR_ERR(mmio_base);

	dev_err(dev, "mi_ufs:ufshcd_pltfrm_init\n");

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	err = ufshcd_alloc_host(dev, &hba);
	if (err) {
		dev_err(dev, "Allocation failed\n");
		return err;
	}

	hba->vops = vops;

	err = ufshcd_parse_clock_info(hba);
	if (err) {
		dev_err(dev, "%s: clock parse failed %d\n",
				"ufshcd_pltfrm_init", err);
		return err;
	}
	err = ufshcd_parse_regulator_info(hba);
	if (err) {
		dev_err(dev, "%s: regulator init failed %d\n",
				"ufshcd_pltfrm_init", err);
		return err;
	}

	ufshcd_init_lanes_per_dir(hba);

	err = ufshcd_parse_operating_points(hba);
	if (err) {
		dev_err(dev, "%s: OPP parse failed %d\n", "ufshcd_pltfrm_init", err);
		return err;
	}

	err = os44_ufshcd_init(hba, mmio_base, irq);
	if (err) {
		dev_err_probe(dev, err, "Initialization failed with error %d\n",
			      err);
		return err;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}
