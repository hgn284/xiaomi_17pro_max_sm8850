/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole work callback with source-owned shared types. Signed elapsed comparison
 * is recovered from SMULH/ASR and B.GT in stock; subtraction is bit-preserving.
 * Runtime imports/callbacks are real providers, never successful stubs. */
#include <ufs/ufshcd.h>
#include <linux/pm_runtime.h>
#include <linux/ktime.h>
#include "os44_xiaomi_owner.h"
#include "os44_cld_work.h"
#include "ufsswap_query_reconstruction.h"
#define CLD_LOG(line, fmt, ...) printk(KERN_ERR "%40s:%3d [%01d%02d%02d] " fmt, "ufscld_trigger_work_fn", line, cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs, ##__VA_ARGS__)
void os44_ufscld_trigger_work_fn(struct work_struct *work)
{
 struct os44_cld_view *cld = container_of(to_delayed_work(work), struct os44_cld_view, idle_work);
 u32 status = 0;
 s64 elapsed;
 int ret, state = atomic_read(&os44_get_ufs_xiaomi_for_remove()->cld.state);
 if (state != 1) {
  printk(KERN_INFO "%s:%d info: cld_state != cld_PRESENT (%d)\n", "ufscld_is_not_present", 105, state);
  printk(KERN_ERR "%s:%d err: ufscld state is = %d\n\n", "ufscld_trigger_work_fn", 419, atomic_read(&os44_get_ufs_xiaomi_for_remove()->cld.state));
  return;
 }
 if (os44_ufscld_hold_runtime_pm(cld)) {
  printk(KERN_ERR "%s:%d err: ufscld_hold_runtime_pm fail\n", "ufscld_trigger_work_fn", 425);
  return;
 }
 mutex_lock(&cld->sysfs_lock);
 if (!cld->trigger) {
  if (cld->debug == true) CLD_LOG(431, "cld_trigger == false, return\n");
  goto out;
 }
 ret = cld->ops->set_trigger(cld, 1);
 if (ret) { os44_query_error(1, 1, -1, 0); ret = -EINVAL; }
 printk(KERN_INFO "%s:%d info: ufscld_issue_enable ret=%d\n\n", "ufscld_trigger_work_fn", 436, ret);
 if (ret) goto resched;
 os44_ufscld_get_operation_status(cld, &status);
 elapsed = (s64)((u64)ktime_to_ms(ktime_get()) - cld->trigger_start_ms);
 printk(KERN_INFO "%s:%d info: ufscld status %d, work_delta_time %lld ms\n\n", "ufscld_trigger_work_fn", 443, status, elapsed);
 if (elapsed >= 420000 || status == 1) {
  printk(KERN_WARNING "%s:%d warn: CLD off with status (%d), duration (%lld ms)\n", "ufscld_trigger_work_fn", 446, status, elapsed);
  os44_ufscld_trigger_off(cld);
  goto out;
 }
 if (status == 2 || status == 3) {
  if (cld->debug) CLD_LOG(451, "cld_REQUIRED, so sched (%d ms)\n", cld->interval_ms);
 } else {
  if (cld->debug) CLD_LOG(454, "issue_cld ERR(%X), so resched for retry\n", 0);
 }
resched:
 schedule_delayed_work(&cld->idle_work, msecs_to_jiffies(cld->interval_ms));
out:
 mutex_unlock(&cld->sysfs_lock);
 pm_runtime_put_noidle(&cld->hba->ufs_device_wlun->sdev_gendev);
 if (cld->debug == true) CLD_LOG(465, "end cld_trigger_work_fn\n");
}
