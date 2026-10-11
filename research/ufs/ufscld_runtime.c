/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole original PM/status behavior. PM resume return is intentionally ignored,
 * as in stock; no fake owner, runtime repair, or private type compatibility claim. */
#include <ufs/ufshcd.h>
#include <linux/pm_runtime.h>
#include "os44_xiaomi_owner.h"
#include "os44_cld_work.h"
#include "ufsswap_query_reconstruction.h"
static inline bool not_present(void)
{
 int state = atomic_read(&os44_get_ufs_xiaomi_for_remove()->cld.state);
 if (state == 1) return false;
 printk(KERN_INFO "%s:%d info: cld_state != cld_PRESENT (%d)\n", "ufscld_is_not_present", 105, state);
 return true;
}
int os44_ufscld_hold_runtime_pm(struct os44_cld_view *cld)
{
 struct ufs_hba *hba = cld->hba;
 struct scsi_device *sdev = hba->ufs_device_wlun;
 if (atomic_read(&os44_get_ufs_xiaomi_for_remove()->cld.state) == 2) {
  if (!hba->pm_op_in_progress && sdev->sdev_gendev.power.is_suspended)
   return -EACCES;
  pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
  down(&hba->host_sem);
  up(&hba->host_sem);
  if (atomic_read(&os44_get_ufs_xiaomi_for_remove()->cld.state) != 1 ||
      hba->curr_dev_pwr_mode != UFS_ACTIVE_PWR_MODE ||
      hba->uic_link_state != UIC_LINK_ACTIVE_STATE) {
   printk(KERN_ERR "%s:%d err: RPM resume failed. Maybe it was SPM suspend\n", "ufscld_hold_runtime_pm", 335);
   printk(KERN_ERR "%s:%d err: UFS state (POWER = %d LINK = %d)\n", "ufscld_hold_runtime_pm", 337, hba->curr_dev_pwr_mode, hba->uic_link_state);
   pm_runtime_put_noidle(&sdev->sdev_gendev);
   return -ENODEV;
  }
 } else {
  if (not_present()) return -ENODEV;
  pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 }
 return 0;
}
int os44_ufscld_get_operation_status(struct os44_cld_view *cld, u32 *status)
{
 int ret = cld->ops->operation_status(cld, status);
 if (ret < 0) os44_query_error(1, 1, -1, 0);
 return ret;
}
