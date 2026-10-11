/* SPDX-License-Identifier: GPL-2.0-only */
/* Original CLD state/suspend/idle scheduling, not proof of initialized work. */
#include <ufs/ufshcd.h>
#include <linux/pm_runtime.h>
#include "os44_remove_owner_view.h"
#include "os44_wlun_contract.h"
int os44_ufscld_get_state(struct ufs_hba *hba)
{
 return atomic_read(&os44_get_ufs_xiaomi_for_remove()->cld.state);
}
void os44_ufscld_suspend(struct ufs_hba *hba)
{
 struct os44_cld_view *cld = &os44_get_ufs_xiaomi_for_remove()->cld;
 if (!cld) return;
 if (cld->trigger)
  printk(KERN_INFO "%s:%d info: cld_trigger was set to block the suspend. so weird\n", "ufscld_suspend", 476);
 atomic_set(&os44_get_ufs_xiaomi_for_remove()->cld.state, 2);
 cancel_delayed_work_sync(&cld->idle_work);
}
void os44_ufscld_on_idle(struct ufs_hba *hba)
{
 struct os44_cld_view *cld = &os44_get_ufs_xiaomi_for_remove()->cld;
 struct os44_mcq_idle_view *host = ufshcd_get_variant(hba);
 if (!cld->trigger) return;
 if (hba->mcq_enabled) {
  if (os44_ufscld_get_state(hba) == 1 && !host->active_cmds) goto schedule;
  if (cld->debug)
   printk(KERN_ERR "%40s:%3d [%01d%02d%02d] active_cmds=%lu\n\n", "ufscld_on_idle", 513,
    cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs, host->active_cmds);
 } else {
  if (os44_ufscld_get_state(hba) == 1 && !hba->outstanding_reqs) goto schedule;
  if (cld->debug)
   printk(KERN_ERR "%40s:%3d [%01d%02d%02d] outstanding_reqs=%lu\n\n", "ufscld_on_idle", 517,
    cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs, hba->outstanding_reqs);
 }
 return;
schedule:
 if (!cld->callback_present) return;
 if (delayed_work_pending(&cld->idle_work)) cancel_delayed_work(&cld->idle_work);
 schedule_delayed_work(&cld->idle_work, msecs_to_jiffies(1000));
}
