/* SPDX-License-Identifier: GPL-2.0-only */
#include "os44_irq_contract.h"
static void __iomem *mcq_opr_base(struct ufs_hba *hba, enum ufshcd_mcq_opr n, int i)
{ struct ufshcd_mcq_opr_info_t *opr = &hba->mcq_opr[n]; return opr->base + opr->stride * i; }
u32 os44_ufshcd_mcq_read_cqis(struct ufs_hba *hba, int i)
{ return readl(mcq_opr_base(hba, OPR_CQIS, i)); }
void os44_ufshcd_mcq_write_cqis(struct ufs_hba *hba, u32 val, int i)
{ writel(val, mcq_opr_base(hba, OPR_CQIS, i)); }
static_assert(offsetof(struct ufs_hba,mcq_opr[OPR_CQIS].stride)==0x1430);
static_assert(offsetof(struct ufs_hba,mcq_opr[OPR_CQIS].base)==0x1438);
