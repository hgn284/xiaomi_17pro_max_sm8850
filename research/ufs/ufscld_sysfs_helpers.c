/* SPDX-License-Identifier: GPL-2.0-only */
#include <ufs/ufshcd.h>
#include <linux/ktime.h>
#include <linux/pm_runtime.h>
#include "os44_cld_sysfs.h"
#include "ufsswap_query_reconstruction.h"
int os44_ufscld_get_frag_level(struct os44_cld_view *cld, u32 *level)
{
 int ret = cld->ops->get_frag_level(cld, level);
 if (ret < 0) os44_query_error(1, 1, -1, 0);
 return ret;
}
void os44_ufscld_release_runtime_pm(struct os44_cld_view *cld)
{
 pm_runtime_put_noidle(&cld->hba->ufs_device_wlun->sdev_gendev);
}
int os44_ufscld_trigger_on(struct os44_cld_view *cld)
{
 if (cld->trigger) return 0;
 cld->trigger = 1;
 os44_ufscld_block_enter_suspend(cld);
 if (cld->vendor->auto_hibern8) os44_ufscld_auto_hibern8_enable(cld, 0);
 cld->last_status_ms = ktime_to_ms(ktime_get());
 cld->trigger_start_ms = ktime_to_ms(ktime_get());
 if (cld->debug == true)
  printk(KERN_ERR "%40s:%3d [%01d%02d%02d] at %lld trigger 0 -> 1\n", "ufscld_trigger_on", 377, cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs, cld->trigger_start_ms);
 schedule_delayed_work(&cld->idle_work, 0);
 return 0;
}
