// SPDX-License-Identifier: GPL-2.0-only
/* Complete stock constructor, .text35710..35878. No storage/getter stand-in.
 * The original async-scan caller and three CLD providers are not yet restored.
 * The constructor initializes work; it does not queue it or zero the whole owner. */
#include <ufs/ufshcd.h>
#include "os44_xiaomi_owner.h"
#include "os44_cld_init.h"
static_assert(offsetof(struct os44_remove_owner_view,cld)==8);
static_assert(offsetof(struct ufs_hba,ahit)==0x64);
static_assert(offsetof(struct ufs_hba,capabilities)==0x88);
static_assert(offsetof(struct ufs_hba,quirks)==0xd4);
void os44_ufscld_init(struct ufs_hba *hba)
{
 struct os44_cld_view *cld=&os44_get_ufs_xiaomi_for_remove()->cld;
 cld->trigger=0;
 cld->hba=hba;
 printk(KERN_INFO "%s:%d info: start ufscld_init\n", "ufscld_init",543);
 if (os44_ufscld_init_ops(cld)) {
  printk(KERN_ERR "%s:%d err: CLD get cld ops fail.\n\n","ufscld_init",546);
  return;
 }
 printk(KERN_INFO "%s:%d info: CLD init ops succeed\n\n","ufscld_init",549);
 cld->interval_ms=2000;
 INIT_DELAYED_WORK(&cld->idle_work,os44_ufscld_trigger_work_fn);
 cld->debug=false;
 cld->block_suspend=0;
 cld->last_status_ms=0;
 cld->trigger_start_ms=0;
 cld->saved_ahit=hba->ahit;
 cld->is_auto_enabled=ufshcd_is_auto_hibern8_supported(cld->hba);
 if (os44_ufscld_create_sysfs(cld)) {
  printk(KERN_ERR "%s:%d err: sysfs init fail. so cld driver disabled\n","ufscld_init",571);
  atomic_set(&os44_get_ufs_xiaomi_for_remove()->cld.state,-2);
 } else {
  printk(KERN_INFO "%s:%d info: UFS cld create sysfs finished\n","ufscld_init",576);
  atomic_set(&os44_get_ufs_xiaomi_for_remove()->cld.state,1);
 }
}
