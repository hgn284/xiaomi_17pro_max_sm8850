// SPDX-License-Identifier: GPL-2.0-only
/* Whole hibernation entries; actual shared resume provider and ordered MMIO. */
#include "os44_resume_pm.h"
int os44_ufshcd_system_restore(struct device *dev)
{

	struct ufs_hba *hba = dev_get_drvdata(dev);
	int ret;

	ret = os44_ufshcd_system_resume(dev);
	if (ret)
		return ret;

	/* Configure UTRL and UTMRL base address registers */
	ufshcd_writel(hba, lower_32_bits(hba->utrdl_dma_addr),
			REG_UTP_TRANSFER_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utrdl_dma_addr),
			REG_UTP_TRANSFER_REQ_LIST_BASE_H);
	ufshcd_writel(hba, lower_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_H);
	/*
	 * Make sure that UTRL and UTMRL base address registers
	 * are updated with the latest queue addresses. Only after
	 * updating these addresses, we can queue the new commands.
	 */
	ufshcd_readl(hba, REG_UTP_TASK_REQ_LIST_BASE_H);

	return 0;

}
int os44_ufshcd_system_thaw(struct device *dev)
{
	return os44_ufshcd_system_resume(dev);
}
static_assert(offsetof(struct ufs_hba,utrdl_dma_addr)==0x28);
static_assert(offsetof(struct ufs_hba,utmrdl_dma_addr)==0x30);
