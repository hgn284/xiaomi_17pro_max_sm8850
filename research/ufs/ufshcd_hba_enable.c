/* SPDX-License-Identifier: GPL-2.0-only */
/* Full HBA-enable path reconstructed from frozen public C and stock ELF. */
#include <linux/delay.h>
#include "ufsswap_provider_reconstruction.h"
#include "os44_uic_contract.h"
#include "os44_hba_enable_contract.h"
#include "os44_hba_bkops_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#include "drivers/ufs/core/ufshcd-crypto.h"
bool os44_ufshcd_is_hba_active(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_CONTROLLER_ENABLE) & CONTROLLER_ENABLE;
}

void os44_ufshcd_delay_us(unsigned long us, unsigned long tolerance)
{
	if (!us)
		return;

	if (us < 10)
		udelay(us);
	else
		usleep_range(us, us + tolerance);
}

static void ufshcd_enable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	set |= intrs;
	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

static inline void ufshcd_hba_start(struct ufs_hba *hba)
{
	u32 val = CONTROLLER_ENABLE;

	if (os44_ufshcd_crypto_enable(hba))
		val |= CRYPTO_GENERAL_ENABLE;

	ufshcd_writel(hba, val, REG_CONTROLLER_ENABLE);
}

static int ufshcd_dme_reset(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {
		.command = UIC_CMD_DME_RESET,
	};
	int ret;

	ret = os44_ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_err(hba->dev,
			"dme-reset: error code %d\n", ret);

	return ret;
}

static int ufshcd_dme_enable(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {
		.command = UIC_CMD_DME_ENABLE,
	};
	int ret;

	ret = os44_ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_err(hba->dev,
			"dme-enable: error code %d\n", ret);

	return ret;
}

static int ufshcd_hba_execute_hce(struct ufs_hba *hba)
{
	int retry_outer = 3;
	int retry_inner;

start:
	if (os44_ufshcd_is_hba_active(hba))
		/* change controller state to "reset state" */
		os44_ufshcd_hba_stop(hba);

	/* UniPro link is disabled at this point */
	ufshcd_set_link_off(hba);

	ufshcd_vops_hce_enable_notify(hba, PRE_CHANGE);

	/* start controller initialization sequence */
	ufshcd_hba_start(hba);

	/*
	 * To initialize a UFS host controller HCE bit must be set to 1.
	 * During initialization the HCE bit value changes from 1->0->1.
	 * When the host controller completes initialization sequence
	 * it sets the value of HCE bit to 1. The same HCE bit is read back
	 * to check if the controller has completed initialization sequence.
	 * So without this delay the value HCE = 1, set in the previous
	 * instruction might be read back.
	 * This delay can be changed based on the controller.
	 */
	os44_ufshcd_delay_us(hba->vps->hba_enable_delay_us, 100);

	/* wait for the host controller to complete initialization */
	retry_inner = 50;
	while (!os44_ufshcd_is_hba_active(hba)) {
		if (retry_inner) {
			retry_inner--;
		} else {
			dev_err(hba->dev,
				"Controller enable failed\n");
			if (retry_outer) {
				retry_outer--;
				goto start;
			}
			return -EIO;
		}
		usleep_range(1000, 1100);
	}

	/* enable UIC related interrupts */
	ufshcd_enable_intr(hba, UFSHCD_UIC_MASK);

	ufshcd_vops_hce_enable_notify(hba, POST_CHANGE);

	return 0;
}

int os44_ufshcd_hba_enable(struct ufs_hba *hba)
{
	int ret;

	if (hba->quirks & UFSHCI_QUIRK_BROKEN_HCE) {
		ufshcd_set_link_off(hba);
		ufshcd_vops_hce_enable_notify(hba, PRE_CHANGE);

		/* enable UIC related interrupts */
		ufshcd_enable_intr(hba, UFSHCD_UIC_MASK);
		ret = ufshcd_dme_reset(hba);
		if (ret) {
			dev_err(hba->dev, "DME_RESET failed\n");
			return ret;
		}

		ret = ufshcd_dme_enable(hba);
		if (ret) {
			dev_err(hba->dev, "Enabling DME failed\n");
			return ret;
		}

		ufshcd_vops_hce_enable_notify(hba, POST_CHANGE);
	} else {
		ret = ufshcd_hba_execute_hce(hba);
	}

	return ret;
}
