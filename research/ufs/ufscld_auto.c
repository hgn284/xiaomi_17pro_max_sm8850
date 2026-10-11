/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole OS44 CLD auto-hibern8 control, source-owned CLD layout.
 * Original ignores PM-get and drain errors; no behavioral repair implied. */
#include <ufs/ufshcd.h>
#include <linux/pm_runtime.h>
#include "os44_cld_view.h"
#include "os44_cld_drain.h"
#include "os44_pm_contract.h"
#include "ufsswap_provider_reconstruction.h"
void os44_ufscld_auto_hibern8_enable(struct os44_cld_view *cld, unsigned int enable)
{
 struct ufs_hba *hba = cld->hba;
 unsigned long flags;
 u32 ahit;
 if (!ufshcd_is_auto_hibern8_supported(hba)) return;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 os44_core_hold(hba);
 down_write(&hba->clk_scaling_lock);
 os44_ufshcd_scsi_block_requests(hba);
 os44_ufshcd_wait_for_doorbell_clr(hba, ~0ULL);
 spin_lock_irqsave(hba->host->host_lock, flags);
 ahit = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
 printk(KERN_INFO "%s:%d info: ufscld ahit-reg 0x%X\n", "ufscld_auto_hibern8_enable", 148, ahit);
 if (!!enable ^ !!ahit) {
  if (enable) hba->ahit = cld->saved_ahit;
  else { cld->saved_ahit = ahit; hba->ahit = 0; }
  ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);
  mb();
  printk(KERN_INFO "%s:%d info: ufshcd before is_auto_enabled %d\n", "ufscld_auto_hibern8_enable", 168, cld->is_auto_enabled);
  cld->is_auto_enabled = !!enable;
  printk(KERN_INFO "%s:%d info: ufshcd after is_auto_enabled %d ahit-reg 0x%X\n", "ufscld_auto_hibern8_enable", 173, cld->is_auto_enabled, ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER));
 } else {
  printk(KERN_INFO "%s:%d info: ufshcd is_auto_enabled %d. so it does not changed\n", "ufscld_auto_hibern8_enable", 176, cld->is_auto_enabled);
 }
 spin_unlock_irqrestore(hba->host->host_lock, flags);
 os44_ufshcd_scsi_unblock_requests(hba);
 up_write(&hba->clk_scaling_lock);
 os44_ufshcd_release(hba);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
}
