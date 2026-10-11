// SPDX-License-Identifier: GPL-2.0-only
/* Whole stock WDC callbacks. No owner allocation/publication or substitute IO. */
#include <ufs/ufshcd.h>
#include "os44_cld_ops.h"
#include "os44_bkops_contract.h"
#include "os44_device_init_helpers.h"
int os44_wdc_get_frag_level(struct os44_cld_view *cld, u32 *level)
{
 u32 attr = ~0U;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR, 0xff, 0, 0, &attr);
 if (!ret) *level = (attr & 0x80) ? 3 : 1;
 return ret;
}
int os44_wdc_cld_set_trigger(struct os44_cld_view *cld, unsigned int enable)
{
 return os44_ufshcd_query_flag_retry(cld->hba, enable ? UPIU_QUERY_OPCODE_SET_FLAG : UPIU_QUERY_OPCODE_CLEAR_FLAG, 0xff, 0, NULL);
}
int os44_wdc_cld_get_trigger(struct os44_cld_view *cld, bool *trigger)
{
 return os44_ufshcd_query_flag_retry(cld->hba, UPIU_QUERY_OPCODE_READ_FLAG, 0xff, 0, trigger);
}
int os44_wdc_cld_operation_status(struct os44_cld_view *cld, u32 *status)
{
 u32 attr = 0;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR, 0xfe, 0, 0, &attr);
 if (ret) printk(KERN_ERR "%s:%d err: get defragprogress failed ret=%d\n\n", "wdc_cld_operation_status", 77, ret);
 *status = (attr == 3 || attr == 4) ? 1 : attr == 2 ? 2 : 0;
 return ret;
}
const struct os44_cld_ops os44_wdc_cld_ops = {
 .get_frag_level = os44_wdc_get_frag_level,
 .set_trigger = os44_wdc_cld_set_trigger,
 .get_trigger.boolean = os44_wdc_cld_get_trigger,
 .operation_status = os44_wdc_cld_operation_status,
};
