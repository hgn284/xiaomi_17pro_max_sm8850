// SPDX-License-Identifier: GPL-2.0-only
/* Equivalent reconstruction. PM/drain errors are ignored by the original.
 * No owner storage/getter substitute; no target MMIO operation authorized. */
#include <linux/pm_runtime.h>
#include "os44_xiaomi_owner.h"
#include "os44_fbo_power.h"
#include "os44_fbo_query.h"
#include "os44_cld_drain.h"
#include "os44_pm_contract.h"
int os44_ufsfbo_is_not_present(struct os44_fbo_lifetime *fbo)
{
 int state=os44_get_ufs_xiaomi_for_remove()->fbo.state;
 if(state==1)return 0;
 if(fbo->debug==1)
  printk(KERN_INFO "[info]%s:%d:fbo_state != fbo_PRESENT (%d)\n","ufsfbo_is_not_present",91,state);
 return -ENODEV;
}
void os44_ufsfbo_block_enter_suspend(struct os44_fbo_lifetime *fbo)
{
 struct ufs_hba *hba;
 unsigned long flags;
 if(fbo->block_suspend & 1)return;
 hba=fbo->hba;
 fbo->block_suspend=1;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 os44_core_hold(hba);
 spin_lock_irqsave(hba->host->host_lock,flags);
 if(fbo->debug==1)
  printk(KERN_INFO "[info]%s:%d:power.usage_count:%d,clk_gating.active_reqs:%d\n","ufsfbo_block_enter_suspend",212,atomic_read(&hba->dev->power.usage_count),hba->clk_gating.active_reqs);
 spin_unlock_irqrestore(hba->host->host_lock,flags);
}
void os44_ufsfbo_auto_hibern8_enable(struct os44_fbo_lifetime *fbo,unsigned int enable)
{
 struct ufs_hba *hba=fbo->hba;
 unsigned long flags;
 u32 ahit,after;
 if(!ufshcd_is_auto_hibern8_supported(hba))return;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 os44_core_hold(hba);
 down_write(&hba->clk_scaling_lock);
 os44_ufshcd_scsi_block_requests(hba);
 os44_ufshcd_wait_for_doorbell_clr(hba,~0ULL);
 spin_lock_irqsave(hba->host->host_lock,flags);
 ahit=ufshcd_readl(hba,REG_AUTO_HIBERNATE_IDLE_TIMER);
 if(fbo->debug==1)
  printk(KERN_INFO "[info]%s:%d:ahit-reg:0x%X\n","ufsfbo_auto_hibern8_enable",159,ahit);
 if(!!enable ^ !!ahit) {
  if(enable)hba->ahit=fbo->saved_ahit;
  else {fbo->saved_ahit=ahit;hba->ahit=0;}
  ufshcd_writel(hba,hba->ahit,REG_AUTO_HIBERNATE_IDLE_TIMER);
  mb();
  if(fbo->debug==1)
   printk(KERN_INFO "[info]%s:%d:Before is_auto_enabled:%d\n","ufsfbo_auto_hibern8_enable",177,fbo->is_auto_enabled);
  fbo->is_auto_enabled=!!enable;
  after=ufshcd_readl(hba,REG_AUTO_HIBERNATE_IDLE_TIMER);
  if(fbo->debug==1)
   printk(KERN_INFO "[info]%s:%d:After is_auto_enabled:%d,ahit-reg:0x%X\n","ufsfbo_auto_hibern8_enable",182,fbo->is_auto_enabled,after);
 } else if(fbo->debug==1)
  printk(KERN_INFO "[info]%s:%d:is_auto_enabled:%d. so it does not changed\n","ufsfbo_auto_hibern8_enable",185,fbo->is_auto_enabled);
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 os44_ufshcd_scsi_unblock_requests(hba);
 up_write(&hba->clk_scaling_lock);
 os44_ufshcd_release(hba);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
}
int os44_ufsfbs_frag_level_check_enable(struct os44_fbo_lifetime *fbo,bool enable)
{
 unsigned int idn=fbo->vendor_selector==1 ? 0x14:0x13;
 return os44_ufsfbo_query_flag(fbo->hba,enable?6:7,idn,enable?&enable:NULL);
}
int os44_ufsfbs_defrag_enable(struct os44_fbo_lifetime *fbo,bool enable)
{
 unsigned int idn=fbo->vendor_selector==1 ? 0x15:0x14;
 return os44_ufsfbo_query_flag(fbo->hba,enable?6:7,idn,enable?&enable:NULL);
}
