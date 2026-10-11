/* SPDX-License-Identifier: GPL-2.0-only */
/* Stock clear0x17948..17a64, actual MMIO polling; MCQ provider NOT stubbed. */
#include "ufsswap_provider_reconstruction.h"
#include <linux/delay.h>
#include <scsi/scsi_host.h>
extern int os44_core_mcq_sq_cleanup(struct ufs_hba *,int);
static_assert(offsetof(struct ufs_hba,quirks)==0xd4);
static_assert(UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR==BIT(6));
int os44_core_clear_cmd(struct ufs_hba *hba,u32 tag)
{
 unsigned long flags,deadline;
 u32 mask;
 int err;
 if (hba->mcq_enabled) {
  err=os44_core_mcq_sq_cleanup(hba,tag);
  if (err) dev_err(hba->dev,"%s: failed tag=%d. err=%d\n","ufshcd_clear_cmd",tag,err);
  return err;
 }
 mask=1U<<tag;
 spin_lock_irqsave(hba->host->host_lock,flags);
 ufshcd_writel(hba,(hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR) ? mask : ~mask,REG_UTP_TRANSFER_REQ_LIST_CLEAR);
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 /* Stock OS44 uses +1250 ticks with CONFIG_HZ=250 (5s), not scaffold1s. */
 deadline=jiffies+msecs_to_jiffies(5000);
 for (;;) {
  if (!(ufshcd_readl(hba,REG_UTP_TRANSFER_REQ_DOOR_BELL)&mask)) return 0;
  usleep_range(1000,1050);
  if (time_after(jiffies,deadline))
   return (ufshcd_readl(hba,REG_UTP_TRANSFER_REQ_DOOR_BELL)&mask) ? -ETIMEDOUT : 0;
 }
}
