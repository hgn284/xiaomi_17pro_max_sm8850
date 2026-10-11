// SPDX-License-Identifier: GPL-2.0-only
/* Instruction-derived source pairing. Original private types unclaimed.
 * No owner storage, fake dispatch table, or same-name kernel fallback. */
#include <ufs/ufshcd.h>
#include "os44_cld_hid.h"
#include "os44_bkops_contract.h"
#include "os44_device_init_helpers.h"
#include "os44_hba_enable_contract.h"
#define HID_LOG(fn, line, fmt, ...) printk(KERN_ERR "%40s:%3d [%01d%02d%02d] " fmt, fn, line, cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs, ##__VA_ARGS__)
static_assert(offsetof(struct ufs_hba, dev)==0x40);
static_assert(offsetof(struct device, power.usage_count)==0x1e0);
static_assert(offsetof(struct ufs_hba, clk_gating.active_reqs)==0xbac);
int os44_ufshid_get_state(struct os44_cld_view *cld, u32 *state)
{
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR, 0x39, 0, 0, state);
 if (ret) printk(KERN_ERR "%s:%d err: Get bHIDState fail, ret=%d\n\n", "ufshid_get_state", 25, ret);
 else if (cld->debug) HID_LOG("ufshid_get_state", 29, "Get bHIDState=%d\n\n", *state);
 return ret;
}
int os44_ufshid_analyze(struct os44_cld_view *cld)
{
 struct ufs_hba *hba=cld->hba;
 u32 operation=1;
 int ret;
 if (cld->debug) HID_LOG("ufshid_analyze", 47, "Set dHIDDefragOperation=%d\n\n", operation);
 ret=local_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, 0x35, 0, 0, &operation);
 if (ret) printk(KERN_ERR "%s:%d err: Set dHIDDefragOperation fail, ret=%d\n\n", "ufshid_analyze", 52, ret);
 return ret;
}
int os44_ufshid_set_defragsize(struct os44_cld_view *cld)
{
 u32 size=~0U;
 int ret=local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_WRITE_ATTR, 0x37, 0, 0, &size);
 if (ret) {
  printk(KERN_ERR "%s:%d err: Set dHIDAvailableSize fail, ret=%d\n\n", "ufshid_set_defragsize", 73, ret);
  return -1;
 }
 return 0;
}
int os44_hid_get_frag_level(struct os44_cld_view *cld, u32 *level)
{
 u32 state=0;
 int ret, retry;
 *level=0;
 for (retry=1; retry<=10; retry++) {
  if (cld->debug) HID_LOG("hid_get_frag_level", 209, "hid get state retry = %d\n\n", retry);
  ret=os44_ufshid_get_state(cld, &state);
  if (ret) {
   printk(KERN_ERR "%s:%d err: ufshid_get_state fail.\n\n", "hid_get_frag_level", 213);
   return ret;
  }
  switch (state) {
  case 0:
   ret=os44_ufshid_analyze(cld);
   if (ret) {
    printk(KERN_ERR "%s:%d err: ufshid_analyze fail.\n\n", "hid_get_frag_level", 243);
    return ret;
   }
   break;
  case 1: os44_ufshcd_delay_us(1000,1000); break;
  case 2: *level=3; return 0;
  case 3: *level=2; return 0;
  case 4: case 5: *level=1; return 0;
  default:
   printk(KERN_ERR "%s:%d err: HIDState error, not support.\n\n", "hid_get_frag_level", 236);
   return 0;
  }
 }
 return 0;
}
int os44_hid_get_trigger(struct os44_cld_view *cld, u32 *trigger)
{
 u32 operation=0;
 int ret=local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR, 0x35, 0, 0, &operation);
 if (ret) {
  printk(KERN_ERR "%s:%d err: Set HIDDefragOperation fail, ret=%d\n\n", "hid_get_trigger", 334, ret);
  return ret;
 }
 printk(KERN_ERR "%s:%d err: Get dHIDDefragOperation is 0x%x\n\n", "hid_get_trigger", 338, operation);
 *trigger=!!(operation&3);
 if (cld->debug) HID_LOG("hid_get_trigger", 343, "Get trigger=%d\n\n", *trigger);
 return ret;
}
int os44_hid_operation_status(struct os44_cld_view *cld, u32 *status)
{
 u32 level=0;
 int ret;
 *status=0;
 ret=os44_hid_get_frag_level(cld, &level);
 switch (level) {
 case 0: *status=3; break;
 case 1: *status=1; break;
 case 2: case 3: *status=4; break;
 default: printk(KERN_ERR "%s:%d err: frag_level error, not support.\n\n", "hid_operation_status", 376); break;
 }
 return ret;
}
