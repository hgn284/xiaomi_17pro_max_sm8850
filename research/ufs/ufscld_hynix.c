// SPDX-License-Identifier: GPL-2.0-only
/* Equivalent reconstruction from stock .text section11 addresses35cac..35efc.
 * Display names collide in supplied ELF; audit uses section+address identity. */
#include <ufs/ufshcd.h>
#include "os44_cld_hynix.h"
#include "os44_device_init_helpers.h"
int os44_sk_get_frag_level(struct os44_cld_view *cld, u32 *level)
{
 u32 attr = ~0U;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR, 0x13, 0, 0, &attr);
 if (ret) return ret;
 switch (attr) {
 case 0: *level = 1; break;
 case 1: *level = 2; break;
 case 2: *level = 3; break;
 default:
  printk("\0016sk cld unknown level %d\n", *level);
  return -1;
 }
 return 0;
}
int os44_sk_cld_set_trigger(struct os44_cld_view *cld, unsigned int enable)
{
 u32 trigger = enable;
 return local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_WRITE_ATTR, 0x12, 0, 0, &trigger);
}
int os44_sk_cld_get_trigger(struct os44_cld_view *cld, u32 *trigger)
{
 return local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR, 0x12, 0, 0, trigger);
}
int os44_sk_cld_operation_status(struct os44_cld_view *cld, u32 *status)
{
 u32 level = 0;
 int ret = os44_sk_get_frag_level(cld, &level);
 if (ret) {
  printk("\0013%s:%d err: get frag level failed ret=%d\n\n", "sk_cld_operation_status", 57, ret);
  *status = 0;
  return ret;
 }
 switch (level) {
 case 1: *status = 1; break;
 case 2: case 3: *status = 2; break;
 default: *status = 0; break;
 }
 return ret;
}
const struct os44_cld_ops os44_hynix_cld_ops = {
 .get_frag_level = os44_sk_get_frag_level,
 .set_trigger = os44_sk_cld_set_trigger,
 .get_trigger.word = os44_sk_cld_get_trigger,
 .operation_status = os44_sk_cld_operation_status,
};
