// SPDX-License-Identifier: GPL-2.0-only
/* Real teardown with source-owned callback signatures; no owner/getter stand-in. */
#include <ufs/ufshcd.h>
#include "os44_xiaomi_owner.h"
#include "os44_cld_ops.h"
void os44_ufscld_auto_hibern8_enable(struct os44_cld_view *, unsigned int);
static inline void issue_disable(struct os44_cld_view *cld)
{
 u32 level = 0;
 if (cld->ops->set_trigger(cld, 0)) return;
 if (cld->ops->get_frag_level(cld, &level)) return;
 if (cld->debug == true)
  printk(KERN_ERR "%40s:%3d [%01d%02d%02d] Frag_lv %d \n\n", "ufscld_issue_disable", 278,
   cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs, level);
}
static inline void trigger_uevent(struct os44_cld_view *cld)
{
 char *env[] = { "CLD_TRIGGER=OFF", NULL };
 kobject_uevent_env(&cld->hba->dev->kobj, KOBJ_CHANGE, env);
 printk(KERN_INFO "%s:%d info: UFSCLD: sent uevent %s\n\n", "trigger_uevent", 293, env[0]);
}
int os44_ufscld_trigger_off(struct os44_cld_view *cld)
{
 if (!cld->trigger) return 0;
 cld->trigger = 0;
 if (cld->debug == true)
  printk(KERN_ERR "%40s:%3d [%01d%02d%02d] cld_trigger 1 -> 0\n", "ufscld_trigger_off", 394,
   cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs);
 issue_disable(cld);
 if (cld->vendor->auto_hibern8) os44_ufscld_auto_hibern8_enable(cld, 1);
 os44_ufscld_allow_enter_suspend(cld);
 trigger_uevent(cld);
 return 0;
}
void os44_ufscld_remove(struct ufs_hba *hba)
{
 struct os44_cld_view *cld = &os44_get_ufs_xiaomi_for_remove()->cld;
 if (!cld) return;
 printk(KERN_INFO "%s:%d info: start cld release\n", "ufscld_remove", 589);
 atomic_set(&os44_get_ufs_xiaomi_for_remove()->cld.state, -2);
 cancel_delayed_work_sync(&cld->idle_work);
 mutex_lock(&cld->sysfs_lock);
 os44_ufscld_allow_enter_suspend(cld);
 os44_ufscld_trigger_off(cld);
 os44_ufscld_remove_sysfs(cld);
 mutex_unlock(&cld->sysfs_lock);
 printk(KERN_INFO "%s:%d info: end cld release\n", "ufscld_remove", 601);
}
