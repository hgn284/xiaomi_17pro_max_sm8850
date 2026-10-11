/* SPDX-License-Identifier: GPL-2.0-only */
/* Instruction-derived source-owned CLD layout; no fake owner or getter. */
#include <ufs/ufshcd.h>
#include <linux/pm_runtime.h>
#include "os44_xiaomi_owner.h"
#include "os44_pm_contract.h"
#include "ufsswap_provider_reconstruction.h"
void os44_ufscld_block_enter_suspend(struct os44_cld_view *cld)
{
 struct ufs_hba *hba = cld->hba;
 unsigned long flags;
 if (cld->block_suspend & 1) return;
 cld->block_suspend = true;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 os44_core_hold(hba);
 spin_lock_irqsave(hba->host->host_lock, flags);
 if (cld->debug == true)
  printk(KERN_ERR "%40s:%3d [%01d%02d%02d] dev->power.usage_count %d hba->clk_gating.active_reqs %d\n", "ufscld_block_enter_suspend", 203,
   cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs,
   atomic_read(&hba->dev->power.usage_count), hba->clk_gating.active_reqs);
 spin_unlock_irqrestore(hba->host->host_lock, flags);
}
void os44_ufscld_allow_enter_suspend(struct os44_cld_view *cld)
{
 struct ufs_hba *hba = cld->hba;
 unsigned long flags;
 if (cld->block_suspend != true) return;
 cld->block_suspend = false;
 os44_ufshcd_release(hba);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 spin_lock_irqsave(hba->host->host_lock, flags);
 if (cld->debug == true)
  printk(KERN_ERR "%40s:%3d [%01d%02d%02d] dev->power.usage_count %d hba->clk_gating.active_reqs %d\n", "ufscld_allow_enter_suspend", 225,
   cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs,
   atomic_read(&hba->dev->power.usage_count), hba->clk_gating.active_reqs);
 spin_unlock_irqrestore(hba->host->host_lock, flags);
}
void os44_ufscld_remove_sysfs(struct os44_cld_view *cld)
{
 int ret = kobject_uevent(&cld->kobj, KOBJ_REMOVE);
 printk(KERN_INFO "%s:%d info: kobject removed (%d)\n", "ufscld_remove_sysfs", 382, ret);
 kobject_del(&cld->kobj);
}
void os44_ufscld_resume(struct ufs_hba *hba)
{
 struct os44_cld_view *cld = &os44_get_ufs_xiaomi_for_remove()->cld;
 if (!cld) return;
 atomic_set(&os44_get_ufs_xiaomi_for_remove()->cld.state, 1);
 if (cld->trigger) {
  printk(KERN_INFO "%s:%d info: cld_trigger still on after resume, reschedule work\n", "ufscld_resume", 493);
  schedule_delayed_work(&cld->idle_work, 0);
 }
}
