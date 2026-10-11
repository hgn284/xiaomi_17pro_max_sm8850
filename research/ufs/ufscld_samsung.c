// SPDX-License-Identifier: GPL-2.0-only
/* Equivalent source reconstruction, not recovered original private types. */
#include <ufs/ufshcd.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include "os44_cld_samsung.h"
#include "os44_device_init_helpers.h"
static_assert(offsetof(struct ufs_hba, ufs_version) == 0xa0);
/* Real storage: initializer writes both IDNs and selector on BOTH paths.
 * Fragment query owns request status; no fake global owner is provided. */
static u32 os44_samsung_query_attr_idn_cld_enable;
static u32 os44_samsung_query_attr_idn_cld_level;
static u8 os44_samsung_hid_selector;
int os44_Samsung_init_idn(struct os44_cld_view *cld)
{
 u32 version = cld->hba->ufs_version;
 u32 enable, level;
 if (version < 0x400) {
  printk(KERN_INFO "%s:%d info: SAMSUNG UFS is V7\n", "Samsung_init_idn", 37);
  enable = 0x20; level = 0x21;
 } else {
  printk(KERN_INFO "%s:%d info: SAMSUNG UFS is V8\n", "Samsung_init_idn", 42);
  enable = 0x80; level = 0x81;
 }
 os44_samsung_query_attr_idn_cld_enable = enable;
 os44_samsung_query_attr_idn_cld_level = level;
 os44_samsung_hid_selector = version < 0x400;
 return 0;
}
int os44_samsung_cld_set_trigger(struct os44_cld_view *cld, unsigned int enable)
{
 u32 trigger = enable ? 2 : 0;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
    os44_samsung_query_attr_idn_cld_enable, 0, os44_samsung_hid_selector, &trigger);
 if (ret)
  printk(KERN_ERR "%s:%d err: send execute afger analysis  failed. attr_idn=%d, opcode=UPIU_QUERY_OPCODE_WRITE_ATTR, trigger=%d\n",
   "samsung_cld_set_trigger", 111, os44_samsung_query_attr_idn_cld_enable, trigger);
 return ret;
}
int os44_samsung_cld_get_trigger(struct os44_cld_view *cld, u32 *trigger)
{
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR,
    os44_samsung_query_attr_idn_cld_enable, 0, os44_samsung_hid_selector, trigger);
 if (ret)
  printk(KERN_ERR "%s:%d err: read cld trigger  failed. attr_idn=%d, opcode=UPIU_QUERY_OPCODE_WRITE_ATTR, trigger=%d\n",
   "samsung_cld_get_trigger", 126, os44_samsung_query_attr_idn_cld_enable, *trigger);
 return ret;
}

static u32 os44_samsung_cld_req_stat;
int os44_samsung_get_frag_level(struct os44_cld_view *cld, u32 *level)
{
 u32 attr = ~0U;
 struct ufs_hba *hba = cld->hba;
 int ret;
 if (!cld->trigger) {
  attr = 1;
  ret = local_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
   os44_samsung_query_attr_idn_cld_enable, 0, os44_samsung_hid_selector, &attr);
  if (ret) {
   printk("\0013%s:%d err: do analysis fail. op code=UPIU_QUERY_OPCODE_WRITE_ATTR, attr_id=%d\n\n",
    "samsung_get_frag_level", 64, os44_samsung_query_attr_idn_cld_enable);
   return ret;
  }
  msleep(200);
 }
 ret = local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
  os44_samsung_query_attr_idn_cld_level, 0, os44_samsung_hid_selector, &attr);
 if (ret) {
  printk("\0013%s:%d err: read frag level  fail,op code=UPIU_QUERY_OPCODE_READ_ATTR, attr_id=%d\n\n",
   "samsung_get_frag_level", 74, os44_samsung_query_attr_idn_cld_level);
  return ret;
 }
 os44_samsung_cld_req_stat = attr >> 31;
 printk("\0016%s:%d info: get_level pass, attr=0x%x, cld_req_stat=%d.\n\n",
  "samsung_get_frag_level", 81, attr, os44_samsung_cld_req_stat);
 switch (attr & 0xf) {
 case 0: *level = 0; break;
 case 1: *level = 1; break;
 case 2: *level = 2; break;
 case 3: *level = 3; break;
 default:
  printk("\0016samsung cld unknown level %d\n", *level);
  return -1;
 }
 return 0;
}
int os44_samsung_cld_operation_status(struct os44_cld_view *cld, u32 *status)
{
 u32 level = 0, req, state;
 int ret = os44_samsung_get_frag_level(cld, &level);
 s64 now;
 if (ret)
  printk("\0013%s:%d err: get frag level failed ret=%d\n\n",
   "samsung_cld_operation_status", 141, ret);
 req = os44_samsung_cld_req_stat;
 now = ktime_to_ms(ktime_get());
 if (level >= 2 && req) {
  state = (level & ~1U) == 2 ? 2 : 0;
 } else if (now - cld->last_status_ms > 19999) {
  state = 1;
 } else {
  *status = 2;
  return ret;
 }
 cld->last_status_ms = ktime_to_ms(ktime_get());
 *status = state;
 return ret;
}
const struct os44_cld_ops os44_samsung_cld_ops = {
 .get_frag_level = os44_samsung_get_frag_level,
 .set_trigger = os44_samsung_cld_set_trigger,
 .get_trigger.word = os44_samsung_cld_get_trigger,
 .operation_status = os44_samsung_cld_operation_status,
};
