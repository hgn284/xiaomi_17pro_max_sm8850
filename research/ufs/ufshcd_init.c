// SPDX-License-Identifier: GPL-2.0-only
/* Candidate equivalent reconstruction of whole OS44 ufshcd_init.
 * Real shared owner storage deliberately remains UNDEFINED pending acceptance.
 * Public helper bodies are bound to the frozen source by code/prepare.py.
 * Not a claim of private original C type compatibility or target validation. */
#include <linux/async.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pm_opp.h>
#include <linux/property.h>
#include <linux/regulator/consumer.h>
#include <linux/blk-mq.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "os44_core_init.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#include "drivers/ufs/core/ufs-sysfs.h"
#include <trace/hooks/ufshcd.h>
#include "os44_xiaomi_owner.h"
#include "os44_init_work.h"
#include "os44_devfreq.h"
#include "os44_async_scan.h"
void os44_ufs_debugfs_init(void);
void os44_ufs_debugfs_exit(void);
void os44_ufs_debugfs_hba_init(struct ufs_hba *);
extern struct scsi_driver os44_ufs_dev_wlun_template;
extern bool os44_ufs_dev_wlun_registered;
extern bool os44_use_mcq_mode;

int os44_ufshcd_setup_clocks(struct ufs_hba *hba, bool on);
int os44_ufshcd_setup_vreg(struct ufs_hba *hba, bool on);
/**
 * os44_ufshcd_memory_alloc - allocate memory for host memory space data structures
 * @hba: per adapter instance
 *
 * 1. Allocate DMA memory for Command Descriptor array
 *	Each command descriptor consist of Command UPIU, Response UPIU and PRDT
 * 2. Allocate DMA memory for UTP Transfer Request Descriptor List (UTRDL).
 * 3. Allocate DMA memory for UTP Task Management Request Descriptor List
 *	(UTMRDL)
 * 4. Allocate memory for local reference block(lrb).
 *
 * Return: 0 for success, non-zero in case of failure.
 */
int os44_ufshcd_memory_alloc(struct ufs_hba *hba);
int os44_ufshcd_hba_init_crypto_capabilities(struct ufs_hba *);
void os44_ufshcd_device_reset(struct ufs_hba *hba);
void os44_ufshcd_init_crypto(struct ufs_hba *);
int os44_ufshcd_hba_enable(struct ufs_hba *hba);
void os44_ufshcd_hba_exit(struct ufs_hba *hba);
void os44_ufshcd_print_evt_hist(struct ufs_hba *hba);
void os44_ufshcd_print_host_state(struct ufs_hba *hba);
/**
 * os44_ufshcd_pm_qos_init - initialize PM QoS request
 * @hba: per adapter instance
 */
void os44_ufshcd_pm_qos_init(struct ufs_hba *hba);
/**
 * os44_ufshcd_err_handler - handle UFS errors that require s/w attention
 * @work: pointer to work structure
 */
void os44_ufshcd_err_handler(struct work_struct *work);
/**
 * os44_ufshcd_exception_event_handler - handle exceptions raised by device
 * @work: pointer to work data
 *
 * Read bExceptionEventStatus attribute from the device and handle the
 * exception event accordingly.
 */
void os44_ufshcd_exception_event_handler(struct work_struct *work);
/**
 * os44_ufshcd_intr - Main interrupt service routine
 * @irq: irq number
 * @__hba: pointer to adapter instance
 *
 * Return:
 *  IRQ_HANDLED - If interrupt is valid
 *  IRQ_NONE    - If invalid interrupt
 */
irqreturn_t os44_ufshcd_intr(int irq, void *__hba);
void os44_ufshcd_gate_work(struct work_struct *work);
void os44_ufshcd_ungate_work(struct work_struct *work);
ssize_t os44_ufshcd_clkgate_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t os44_ufshcd_clkgate_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
ssize_t os44_ufshcd_clkgate_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t os44_ufshcd_clkgate_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
void os44_ufshcd_clk_scaling_resume_work(struct work_struct *work);
void os44_ufshcd_clk_scaling_suspend_work(struct work_struct *work);
void os44_ufshcd_rpm_dev_flush_recheck_work(struct work_struct *work);
void os44_ufshcd_rtc_work(struct work_struct *work);
/**
 * os44_ufshcd_async_scan - asynchronous execution for probing hba
 * @data: data pointer to pass to this function
 * @cookie: cookie data
 */
void os44_ufshcd_async_scan(void *data, async_cookie_t cookie);
struct ufs_ref_clk {
	unsigned long freq_hz;
	enum ufs_ref_clk_freq val;
};
static const struct ufs_ref_clk ufs_ref_clk_freqs[] = {
	{19200000, REF_CLK_FREQ_19_2_MHZ},
	{26000000, REF_CLK_FREQ_26_MHZ},
	{38400000, REF_CLK_FREQ_38_4_MHZ},
	{52000000, REF_CLK_FREQ_52_MHZ},
	{0, REF_CLK_FREQ_INVAL},
};

int ufshcd_get_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg)
		goto out;

	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		dev_err(dev, "%s: %s get failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

static int ufshcd_init_hba_vreg(struct ufs_hba *hba)
{
	struct ufs_vreg_info *info = &hba->vreg_info;

	return ufshcd_get_vreg(hba->dev, info->vdd_hba);
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

static int ufshcd_setup_hba_vreg(struct ufs_hba *hba, bool on)
{
	struct ufs_vreg_info *info = &hba->vreg_info;

	return on ? ufshcd_enable_vreg(hba->dev, info->vdd_hba) : ufshcd_disable_vreg(hba->dev, info->vdd_hba);
}

static enum ufs_ref_clk_freq
ufs_get_bref_clk_from_hz(unsigned long freq)
{
	int i;

	for (i = 0; ufs_ref_clk_freqs[i].freq_hz; i++)
		if (ufs_ref_clk_freqs[i].freq_hz == freq)
			return ufs_ref_clk_freqs[i].val;

	return REF_CLK_FREQ_INVAL;
}

static void local_parse_dev_ref_clk_freq(struct ufs_hba *hba, struct clk *refclk)
{
	unsigned long freq;

	freq = clk_get_rate(refclk);

	hba->dev_ref_clk_freq =
		ufs_get_bref_clk_from_hz(freq);

	if (hba->dev_ref_clk_freq == REF_CLK_FREQ_INVAL)
		dev_err(hba->dev,
		"invalid ref_clk setting = %ld\n", freq);
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

static int ufshcd_init_clocks(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct device *dev = hba->dev;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		clki->clk = devm_clk_get(dev, clki->name);
		if (IS_ERR(clki->clk)) {
			ret = PTR_ERR(clki->clk);
			dev_err(dev, "%s: %s clk get failed, %d\n",
					__func__, clki->name, ret);
			goto out;
		}

		/*
		 * Parse device ref clk freq as per device tree "ref_clk".
		 * Default dev_ref_clk_freq is set to REF_CLK_FREQ_INVAL
		 * in ufshcd_alloc_host().
		 */
		if (!strcmp(clki->name, "ref_clk"))
			local_parse_dev_ref_clk_freq(hba, clki->clk);

		if (clki->max_freq) {
			ret = clk_set_rate(clki->clk, clki->max_freq);
			if (ret) {
				dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
					__func__, clki->name,
					clki->max_freq, ret);
				goto out;
			}
			clki->curr_freq = clki->max_freq;
		}
		dev_dbg(dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
	}

	/* Set Max. frequency for all clocks */
	if (hba->use_pm_opp) {
		ret = ufshcd_opp_set_rate(hba, ULONG_MAX);
		if (ret) {
			dev_err(hba->dev, "%s: failed to set OPP: %d", __func__,
				ret);
			goto out;
		}
	}

out:
	return ret;
}

static enum ufs_ref_clk_freq ufshcd_parse_ref_clk_property(struct ufs_hba *hba)
{
	u32 freq;
	int ret = device_property_read_u32(hba->dev, "ref-clk-freq", &freq);

	if (ret) {
		dev_dbg(hba->dev, "Cannot query 'ref-clk-freq' property = %d", ret);
		return REF_CLK_FREQ_INVAL;
	}

	return ufs_get_bref_clk_from_hz(freq);
}

static int ufshcd_init_vreg(struct ufs_hba *hba)
{
	int ret = 0;
	struct device *dev = hba->dev;
	struct ufs_vreg_info *info = &hba->vreg_info;

	ret = ufshcd_get_vreg(dev, info->vcc);
	if (ret)
		goto out;

	ret = ufshcd_get_vreg(dev, info->vccq);
	if (!ret)
		ret = ufshcd_get_vreg(dev, info->vccq2);
out:
	return ret;
}

static int ufshcd_variant_hba_init(struct ufs_hba *hba)
{
	int err = 0;

	if (!hba->vops)
		goto out;

	err = ufshcd_vops_init(hba);
	if (err)
		dev_err_probe(hba->dev, err,
			      "%s: variant %s init failed with err %d\n",
			      __func__, ufshcd_get_var_name(hba), err);
out:
	return err;
}

static int ufshcd_hba_init(struct ufs_hba *hba)
{
	int err;

	/*
	 * Handle host controller power separately from the UFS device power
	 * rails as it will help controlling the UFS host controller power
	 * collapse easily which is different than UFS device power collapse.
	 * Also, enable the host controller power before we go ahead with rest
	 * of the initialization here.
	 */
	err = ufshcd_init_hba_vreg(hba);
	if (err)
		goto out;

	err = ufshcd_setup_hba_vreg(hba, true);
	if (err)
		goto out;

	err = ufshcd_init_clocks(hba);
	if (err)
		goto out_disable_hba_vreg;

	if (hba->dev_ref_clk_freq == REF_CLK_FREQ_INVAL)
		hba->dev_ref_clk_freq = ufshcd_parse_ref_clk_property(hba);

	err = os44_ufshcd_setup_clocks(hba, true);
	if (err)
		goto out_disable_hba_vreg;

	err = ufshcd_init_vreg(hba);
	if (err)
		goto out_disable_clks;

	err = os44_ufshcd_setup_vreg(hba, true);
	if (err)
		goto out_disable_clks;

	err = ufshcd_variant_hba_init(hba);
	if (err)
		goto out_disable_vreg;

	os44_ufs_debugfs_hba_init(hba);
	os44_ufs_xiaomi.queue_hba = hba;

	hba->is_powered = true;
	goto out;

out_disable_vreg:
	os44_ufshcd_setup_vreg(hba, false);
out_disable_clks:
	os44_ufshcd_setup_clocks(hba, false);
out_disable_hba_vreg:
	ufshcd_setup_hba_vreg(hba, false);
out:
	return err;
}

/**
 * ufshcd_hba_capabilities - Read controller capabilities
 * @hba: per adapter instance
 *
 * Return: 0 on success, negative on error.
 */
static inline int ufshcd_hba_capabilities(struct ufs_hba *hba)
{
	int err;

	hba->capabilities = ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES);

	/* nutrs and nutmrs are 0 based values */
	hba->nutrs = (hba->capabilities & MASK_TRANSFER_REQUESTS_SLOTS_SDB) + 1;
	hba->nutmrs =
	((hba->capabilities & MASK_TASK_MANAGEMENT_REQUEST_SLOTS) >> 16) + 1;
	hba->reserved_slot = hba->nutrs - 1;

	hba->nortt = FIELD_GET(MASK_NUMBER_OUTSTANDING_RTT, hba->capabilities) + 1;

	/* Read crypto capabilities */
	err = os44_ufshcd_hba_init_crypto_capabilities(hba);
	if (err) {
		dev_err(hba->dev, "crypto setup failed\n");
		return err;
	}

	/*
	 * The UFSHCI 3.0 specification does not define MCQ_SUPPORT and
	 * LSDB_SUPPORT, but [31:29] as reserved bits with reset value 0s, which
	 * means we can simply read values regardless of version.
	 */
	hba->mcq_sup = FIELD_GET(MASK_MCQ_SUPPORT, hba->capabilities);
	/*
	 * 0h: legacy single doorbell support is available
	 * 1h: indicate that legacy single doorbell support has been removed
	 */
	if (!(hba->quirks & UFSHCD_QUIRK_BROKEN_LSDBS_CAP))
		hba->lsdb_sup = !FIELD_GET(MASK_LSDB_SUPPORT, hba->capabilities);
	else
		hba->lsdb_sup = true;

	if (!hba->mcq_sup)
		return 0;

	hba->mcq_capabilities = ufshcd_readl(hba, REG_MCQCAP);
	hba->ext_iid_sup = FIELD_GET(MASK_EXT_IID_SUPPORT,
				     hba->mcq_capabilities);

	return 0;
}

/**
 * ufshcd_get_ufs_version - Get the UFS version supported by the HBA
 * @hba: Pointer to adapter instance
 *
 * Return: UFSHCI version supported by the controller
 */
static inline u32 ufshcd_get_ufs_version(struct ufs_hba *hba)
{
	u32 ufshci_ver;

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION)
		ufshci_ver = ufshcd_vops_get_ufs_hci_version(hba);
	else
		ufshci_ver = ufshcd_readl(hba, REG_UFS_VERSION);

	/*
	 * UFSHCI v1.x uses a different version scheme, in order
	 * to allow the use of comparisons with the ufshci_version
	 * function, we convert it to the same scheme as ufs 2.0+.
	 */
	if (ufshci_ver & 0x00010000)
		return ufshci_version(1, ufshci_ver & 0x00000100);

	return ufshci_ver;
}

/**
 * ufshcd_get_intr_mask - Get the interrupt bit mask
 * @hba: Pointer to adapter instance
 *
 * Return: interrupt bit mask per version
 */
static inline u32 ufshcd_get_intr_mask(struct ufs_hba *hba)
{
	if (hba->ufs_version <= ufshci_version(2, 0))
		return INTERRUPT_MASK_ALL_VER_11;

	return INTERRUPT_MASK_ALL_VER_21;
}

/**
 * ufshcd_set_dma_mask - Set dma mask based on the controller
 *			 addressing capability
 * @hba: per adapter instance
 *
 * Return: 0 for success, non-zero for failure.
 */
static int ufshcd_set_dma_mask(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->set_dma_mask)
		return hba->vops->set_dma_mask(hba);
	if (hba->capabilities & MASK_64_ADDRESSING_SUPPORT) {
		if (!dma_set_mask_and_coherent(hba->dev, ~0ULL))
			return 0;
	}
	return dma_set_mask_and_coherent(hba->dev, DMA_BIT_MASK(32));
}

static void ufshcd_init_lrb(struct ufs_hba *hba, struct ufshcd_lrb *lrb, int i)
{
	struct utp_transfer_cmd_desc *cmd_descp = (void *)hba->ucdl_base_addr +
		i * ufshcd_get_ucd_size(hba);
	struct utp_transfer_req_desc *utrdlp = hba->utrdl_base_addr;
	dma_addr_t cmd_desc_element_addr = hba->ucdl_dma_addr +
		i * ufshcd_get_ucd_size(hba);
	u16 response_offset = le16_to_cpu(utrdlp[i].response_upiu_offset);
	u16 prdt_offset = le16_to_cpu(utrdlp[i].prd_table_offset);

	lrb->utr_descriptor_ptr = utrdlp + i;
	lrb->utrd_dma_addr = hba->utrdl_dma_addr +
		i * sizeof(struct utp_transfer_req_desc);
	lrb->ucd_req_ptr = (struct utp_upiu_req *)cmd_descp->command_upiu;
	lrb->ucd_req_dma_addr = cmd_desc_element_addr;
	lrb->ucd_rsp_ptr = (struct utp_upiu_rsp *)cmd_descp->response_upiu;
	lrb->ucd_rsp_dma_addr = cmd_desc_element_addr + response_offset;
	lrb->ucd_prdt_ptr = (struct ufshcd_sg_entry *)cmd_descp->prd_table;
	lrb->ucd_prdt_dma_addr = cmd_desc_element_addr + prdt_offset;
}

/**
 * ufshcd_host_memory_configure - configure local reference block with
 *				memory offsets
 * @hba: per adapter instance
 *
 * Configure Host memory space
 * 1. Update Corresponding UTRD.UCDBA and UTRD.UCDBAU with UCD DMA
 * address.
 * 2. Update each UTRD with Response UPIU offset, Response UPIU length
 * and PRDT offset.
 * 3. Save the corresponding addresses of UTRD, UCD.CMD, UCD.RSP and UCD.PRDT
 * into local reference block.
 */
static void ufshcd_host_memory_configure(struct ufs_hba *hba)
{
	struct utp_transfer_req_desc *utrdlp;
	dma_addr_t cmd_desc_dma_addr;
	dma_addr_t cmd_desc_element_addr;
	u16 response_offset;
	u16 prdt_offset;
	int cmd_desc_size;
	int i;

	utrdlp = hba->utrdl_base_addr;

	response_offset =
		offsetof(struct utp_transfer_cmd_desc, response_upiu);
	prdt_offset =
		offsetof(struct utp_transfer_cmd_desc, prd_table);

	cmd_desc_size = ufshcd_get_ucd_size(hba);
	cmd_desc_dma_addr = hba->ucdl_dma_addr;

	for (i = 0; i < hba->nutrs; i++) {
		/* Configure UTRD with command descriptor base address */
		cmd_desc_element_addr =
				(cmd_desc_dma_addr + (cmd_desc_size * i));
		utrdlp[i].command_desc_base_addr =
				cpu_to_le64(cmd_desc_element_addr);

		/* Response upiu and prdt offset should be in double words */
		if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN) {
			utrdlp[i].response_upiu_offset =
				cpu_to_le16(response_offset);
			utrdlp[i].prd_table_offset =
				cpu_to_le16(prdt_offset);
			utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE);
		} else {
			utrdlp[i].response_upiu_offset =
				cpu_to_le16(response_offset >> 2);
			utrdlp[i].prd_table_offset =
				cpu_to_le16(prdt_offset >> 2);
			utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE >> 2);
		}

		ufshcd_init_lrb(hba, &hba->lrb[i], i);
	}
}

static void ufshcd_init_clk_gating_sysfs(struct ufs_hba *hba)
{
	hba->clk_gating.delay_attr.show = os44_ufshcd_clkgate_delay_show;
	hba->clk_gating.delay_attr.store = os44_ufshcd_clkgate_delay_store;
	sysfs_attr_init(&hba->clk_gating.delay_attr.attr);
	hba->clk_gating.delay_attr.attr.name = "clkgate_delay_ms";
	hba->clk_gating.delay_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &hba->clk_gating.delay_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkgate_delay\n");

	hba->clk_gating.enable_attr.show = os44_ufshcd_clkgate_enable_show;
	hba->clk_gating.enable_attr.store = os44_ufshcd_clkgate_enable_store;
	sysfs_attr_init(&hba->clk_gating.enable_attr.attr);
	hba->clk_gating.enable_attr.attr.name = "clkgate_enable";
	hba->clk_gating.enable_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &hba->clk_gating.enable_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkgate_enable\n");
}

static void ufshcd_init_clk_gating(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkgating_allowed(hba))
		return;

	hba->clk_gating.state = CLKS_ON;

	hba->clk_gating.delay_ms = 150;
	INIT_DELAYED_WORK(&hba->clk_gating.gate_work, os44_ufshcd_gate_work);
	INIT_WORK(&hba->clk_gating.ungate_work, os44_ufshcd_ungate_work);

	hba->clk_gating.clk_gating_workq = alloc_ordered_workqueue(
		"ufs_clk_gating_%d", WQ_MEM_RECLAIM | WQ_HIGHPRI,
		hba->host->host_no);

	ufshcd_init_clk_gating_sysfs(hba);

	hba->clk_gating.is_enabled = true;
	hba->clk_gating.is_initialized = true;
}

static void ufshcd_init_clk_scaling(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	if (!hba->clk_scaling.min_gear)
		hba->clk_scaling.min_gear = UFS_HS_G1;

	if (!hba->clk_scaling.wb_gear)
		/* Use intermediate gear speed HS_G3 as the default wb_gear */
		hba->clk_scaling.wb_gear = UFS_HS_G3;

	INIT_WORK(&hba->clk_scaling.suspend_work,
		  os44_ufshcd_clk_scaling_suspend_work);
	INIT_WORK(&hba->clk_scaling.resume_work,
		  os44_ufshcd_clk_scaling_resume_work);

	hba->clk_scaling.workq = alloc_ordered_workqueue(
		"ufs_clkscaling_%d", WQ_MEM_RECLAIM, hba->host->host_no);

	hba->clk_scaling.is_initialized = true;
}

/**
 * ufshcd_init - Driver initialization routine
 * @hba: per-adapter instance
 * @mmio_base: base register address
 * @irq: Interrupt line of device
 *
 * Return: 0 on success, non-zero value on failure.
 */
int os44_ufshcd_init(struct ufs_hba *hba, void __iomem *mmio_base, unsigned int irq)
{
	int err;
	struct Scsi_Host *host = hba->host;
	struct device *dev = hba->dev;

	/*
	 * dev_set_drvdata() must be called before any callbacks are registered
	 * that use dev_get_drvdata() (frequency scaling, clock scaling, hwmon,
	 * sysfs).
	 */
	dev_set_drvdata(dev, hba);

	if (!mmio_base) {
		dev_err(hba->dev,
		"Invalid memory reference for mmio_base is NULL\n");
		err = -ENODEV;
		goto out_error;
	}

	hba->mmio_base = mmio_base;
	hba->irq = irq;
	hba->vps = &os44_ufs_hba_vps;
 if (!os44_ufs_dev_wlun_registered) {
  os44_ufs_debugfs_init();
  err = scsi_register_driver(&os44_ufs_dev_wlun_template.gendrv);
  if (err) {
   os44_ufs_debugfs_exit();
   dev_err(hba->dev, "ufs wlun pm driver_register failed with error code: %d", err);
   err = -ENODEV;
   goto out_error;
  }
  os44_ufs_dev_wlun_registered = true;
 }

	/*
	 * Initialize clk_gating.lock early since it is being used in
	 * os44_ufshcd_setup_clocks()
	 */
	spin_lock_init(&hba->clk_gating.lock);
 mutex_init(&to_hba_priv(hba)->pm_qos_mutex);

	/*
	 * Set the default power management level for runtime and system PM.
	 * Host controller drivers can override them in their
	 * 'ufs_hba_variant_ops::init' callback.
	 *
	 * Default power saving mode is to keep UFS link in Hibern8 state
	 * and UFS device in sleep state.
	 */
	hba->rpm_lvl = 3;
	hba->spm_lvl = 3;

	err = ufshcd_hba_init(hba);
	if (err)
		goto out_error;

	/* Read capabilities registers */
	err = ufshcd_hba_capabilities(hba);
	if (err)
		goto out_disable;

	/* Get UFS version supported by the controller */
	hba->ufs_version = ufshcd_get_ufs_version(hba);

	/* Get Interrupt bit mask per version */
	hba->intr_mask = ufshcd_get_intr_mask(hba);

	err = ufshcd_set_dma_mask(hba);
	if (err) {
		dev_err(hba->dev, "set dma mask failed\n");
		goto out_disable;
	}

	/* Allocate memory for host memory space */
	err = os44_ufshcd_memory_alloc(hba);
	if (err) {
		dev_err(hba->dev, "Memory allocation failed\n");
		goto out_disable;
	}

	/* Configure LRB */
	ufshcd_host_memory_configure(hba);

	host->can_queue = hba->nutrs - UFSHCD_NUM_RESERVED;
	host->cmd_per_lun = hba->nutrs - UFSHCD_NUM_RESERVED;
	host->max_id = 1;
	host->max_lun = UFS_MAX_LUNS;
	host->max_channel = 0;
	host->unique_id = host->host_no;
	host->max_cmd_len = UFS_CDB_SIZE;
	host->queuecommand_may_block = !!(hba->caps & UFSHCD_CAP_CLK_GATING);

	/* Use default RPM delay if host not set */
	if (host->rpm_autosuspend_delay == 0)
		host->rpm_autosuspend_delay = 2000;

	hba->max_pwr_info.is_valid = false;

	/* Initialize work queues */
	hba->eh_wq = alloc_ordered_workqueue("ufs_eh_wq_%d", WQ_MEM_RECLAIM,
					     hba->host->host_no);
	if (!hba->eh_wq) {
		dev_err(hba->dev, "%s: failed to create eh workqueue\n",
			"ufshcd_init");
		err = -ENOMEM;
		goto out_disable;
	}
	INIT_WORK(&hba->eh_work, os44_ufshcd_err_handler);
	INIT_WORK(&hba->eeh_work, os44_ufshcd_exception_event_handler);

	sema_init(&hba->host_sem, 1);

	/* Initialize UIC command mutex */
	mutex_init(&hba->uic_cmd_mutex);

	/* Initialize mutex for device management commands */
	mutex_init(&hba->dev_cmd.lock);

	/* Initialize mutex for exception event control */
	mutex_init(&hba->ee_ctrl_mutex);

	mutex_init(&hba->wb_mutex);

	/* Initialize mutex for PM QoS request synchronization */

	init_rwsem(&hba->clk_scaling_lock);

	ufshcd_init_clk_gating(hba);

	ufshcd_init_clk_scaling(hba);

	/*
	 * In order to avoid any spurious interrupt immediately after
	 * registering UFS controller interrupt handler, clear any pending UFS
	 * interrupt status and disable all the UFS interrupts.
	 */
	ufshcd_writel(hba, ufshcd_readl(hba, REG_INTERRUPT_STATUS),
		      REG_INTERRUPT_STATUS);
	ufshcd_writel(hba, 0, REG_INTERRUPT_ENABLE);
	/*
	 * Make sure that UFS interrupts are disabled and any pending interrupt
	 * status is cleared before registering UFS interrupt handler.
	 */
	ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	/* IRQ registration */
	err = devm_request_irq(dev, irq, os44_ufshcd_intr, IRQF_SHARED, UFSHCD, hba);
	if (err) {
		dev_err(hba->dev, "request irq failed\n");
		goto out_disable;
	} else {
		hba->is_irq_enabled = true;
	}

	if (!(hba->mcq_sup && os44_use_mcq_mode)) {
		if (!hba->lsdb_sup) {
			dev_err(hba->dev, "%s: failed to initialize (legacy doorbell mode not supported)\n",
				"ufshcd_init");
			err = -EINVAL;
			goto out_disable;
		}
		err = scsi_add_host(host, hba->dev);
		if (err) {
			dev_err(hba->dev, "scsi_add_host failed\n");
			goto out_disable;
		}
		hba->scsi_host_added = true;
	}

	hba->tmf_tag_set = (struct blk_mq_tag_set) {
		.nr_hw_queues	= 1,
		.queue_depth	= hba->nutmrs,
		.ops		= &os44_ufshcd_tmf_ops,
		.flags		= BLK_MQ_F_NO_SCHED,
	};
	err = blk_mq_alloc_tag_set(&hba->tmf_tag_set);
	if (err < 0)
		goto out_remove_scsi_host;
	hba->tmf_queue = blk_mq_alloc_queue(&hba->tmf_tag_set, NULL, NULL);
	if (IS_ERR(hba->tmf_queue)) {
		err = PTR_ERR(hba->tmf_queue);
		goto free_tmf_tag_set;
	}
	hba->tmf_rqs = devm_kcalloc(hba->dev, hba->nutmrs,
				    sizeof(*hba->tmf_rqs), GFP_KERNEL);
	if (!hba->tmf_rqs) {
		err = -ENOMEM;
		goto free_tmf_queue;
	}

	/* Reset the attached device */
	os44_ufshcd_device_reset(hba);

	os44_ufshcd_init_crypto(hba);

	/* Host controller enable */
	err = os44_ufshcd_hba_enable(hba);
	if (err) {
		dev_err(hba->dev, "Host controller enable failed\n");
		os44_ufshcd_print_evt_hist(hba);
		os44_ufshcd_print_host_state(hba);
		goto free_tmf_queue;
	}

	INIT_DELAYED_WORK(&hba->rpm_dev_flush_recheck_work, os44_ufshcd_rpm_dev_flush_recheck_work);
	INIT_DELAYED_WORK(&hba->ufs_rtc_update_work, os44_ufshcd_rtc_work);

	/* Set the default auto-hiberate idle timer value to 150 ms */
	if (ufshcd_is_auto_hibern8_supported(hba) && !hba->ahit) {
		hba->ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 150) |
			    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);
	}

	/* Hold auto suspend until async scan completes */
	pm_runtime_get_sync(dev);
	atomic_set(&hba->scsi_block_reqs_cnt, 0);
	/*
	 * We are assuming that device wasn't put in sleep/power-down
	 * state exclusively during the boot stage before kernel.
	 * This assumption helps avoid doing link startup twice during
	 * ufshcd_probe_hba().
	 */
	ufshcd_set_ufs_dev_active(hba);

	async_schedule(os44_ufshcd_async_scan, hba);
 ufs_sysfs_add_nodes(dev);
 trace_android_vh_ufs_update_sysfs(hba);

	device_enable_async_suspend(dev);
	os44_ufshcd_pm_qos_init(hba);
	return 0;

free_tmf_queue:
	blk_mq_destroy_queue(hba->tmf_queue);
	blk_put_queue(hba->tmf_queue);
free_tmf_tag_set:
	blk_mq_free_tag_set(&hba->tmf_tag_set);
out_remove_scsi_host:
	if (hba->scsi_host_added)
		scsi_remove_host(hba->host);
out_disable:
	hba->is_irq_enabled = false;
	os44_ufshcd_hba_exit(hba);
out_error:
	return err;
}
