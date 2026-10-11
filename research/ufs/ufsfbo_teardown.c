/* SPDX-License-Identifier: GPL-2.0-only */
/* Faithful stock teardown, NOT a lifetime repair or deployable owner closure.
 * Original passes static owner+0x658 to kfree and reads debug after free.
 * All frees are intercepted in offline tests; do not load this candidate.
 * Source provider/getter remains unresolved until full real owner lifecycle. */
#include <linux/slab.h>
#include "os44_xiaomi_owner.h"
#include "drivers/ufs/core/ufshcd-priv.h"
static_assert(offsetof(struct ufs_hba,ufs_device_wlun)==0x48);
static_assert(offsetof(struct scsi_device,sdev_gendev.power.usage_count)==0x3b0);
static_assert(offsetof(struct device,power.usage_count)==0x1e0);
static_assert(offsetof(struct ufs_hba,clk_gating.active_reqs)==0xbac);
void os44_ufsfbo_allow_enter_suspend(struct os44_fbo_lifetime *fbo)
{
 struct ufs_hba *hba;
 unsigned long flags;
 if(fbo->block_suspend != 1) return;
 hba=fbo->hba;
 fbo->block_suspend=0;
 ufshcd_release(hba);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 spin_lock_irqsave(hba->host->host_lock,flags);
 if(fbo->debug==1)
  printk(KERN_INFO "[info]%s:%d:power.usage_count:%d,clk_gating.active_reqs:%d\n", "ufsfbo_allow_enter_suspend",233,atomic_read(&hba->dev->power.usage_count),hba->clk_gating.active_reqs);
 spin_unlock_irqrestore(hba->host->host_lock,flags);
}
noinline void os44_ufsfbo_remove_sysfs(struct os44_fbo_lifetime *fbo)
{
 int ret=kobject_uevent(&fbo->kobj,KOBJ_REMOVE);
 if(fbo->debug==1)
  printk(KERN_INFO "[info]%s:%d:Kobject removed (%d)\n", "ufsfbo_remove_sysfs",18,ret);
 kobject_del(&fbo->kobj);
}
void os44_ufsfbo_remove(struct ufs_hba *hba)
{
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 struct os44_fbo_lifetime *fbo=&owner->fbo;
 if(!fbo) return;
 if(fbo->debug==1)
  printk(KERN_INFO "[info]%s:%d:Star fbo release\n", "ufsfbo_remove",1228);
 os44_get_ufs_xiaomi_for_remove()->fbo.state=-2;
 mutex_lock(&fbo->lock);
 os44_ufsfbo_allow_enter_suspend(fbo);
 os44_ufsfbo_remove_sysfs(fbo);
 mutex_unlock(&fbo->lock);
 kfree(fbo);
 if(fbo->debug==1)
  printk(KERN_INFO "[info]%s:%d:End fbo release\n", "ufsfbo_remove",1239);
}
