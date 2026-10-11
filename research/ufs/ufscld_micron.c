// SPDX-License-Identifier: GPL-2.0-only
/* Stock Micron IDN lifecycle + callbacks. Source-owned private types.
 * The IDNs are assigned by the real version-dependent initializer, not stubs.
 * Full ufscld_init_ops dispatch/publication remains an explicit separate gate.
 */
#include <ufs/ufshcd.h>
#include "os44_cld_micron.h"
#include "os44_bkops_contract.h"
#include "os44_device_init_helpers.h"
static_assert(offsetof(struct ufs_hba, ufs_version) == 0xa0);
static unsigned int os44_micron_query_flag_idn_cld_enable;
static unsigned int os44_micron_query_attr_idn_cld_level;
static unsigned int os44_micron_query_attr_idn_defrag;
int os44_micron_init_idn(struct os44_cld_view *cld)
{
 if (cld->hba->ufs_version < 0x400) {
  /* Preserve original reversed diagnostic wording verbatim. */
  printk(KERN_INFO "%s:%d info: MICRON UFS higher UFS4.0\n", "micron_init_idn", 35);
  os44_micron_query_flag_idn_cld_enable = 0x13;
  os44_micron_query_attr_idn_cld_level = 0x31;
  os44_micron_query_attr_idn_defrag = 0x32;
 } else {
  printk(KERN_INFO "%s:%d info: MICRON UFS below UFS4.0\n", "micron_init_idn", 40);
  os44_micron_query_flag_idn_cld_enable = 0x83;
  os44_micron_query_attr_idn_cld_level = 0x81;
  os44_micron_query_attr_idn_defrag = 0x82;
 }
 return 0;
}
int os44_micron_get_frag_level(struct os44_cld_view *cld, u32 *level)
{
 u32 attr = ~0U;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR, os44_micron_query_attr_idn_cld_level, 0, 0, &attr);
 if (ret) return ret;
 if (attr == 1) *level = 3;
 else if (attr == 0) *level = 1;
 else {
  printk(KERN_INFO "micron cld unknown level %d\n", attr);
  return -1;
 }
 return 0;
}
int os44_micron_cld_set_trigger(struct os44_cld_view *cld, unsigned int enable)
{
 return os44_ufshcd_query_flag_retry(cld->hba, enable ? UPIU_QUERY_OPCODE_SET_FLAG : UPIU_QUERY_OPCODE_CLEAR_FLAG, os44_micron_query_flag_idn_cld_enable, 0, NULL);
}
int os44_micron_cld_get_trigger(struct os44_cld_view *cld, bool *trigger)
{
 return os44_ufshcd_query_flag_retry(cld->hba, UPIU_QUERY_OPCODE_READ_FLAG, os44_micron_query_flag_idn_cld_enable, 0, trigger);
}
int os44_micron_cld_operation_status(struct os44_cld_view *cld, u32 *status)
{
 u32 attr = 0;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR, os44_micron_query_attr_idn_defrag, 0, 0, &attr);
 if (ret) printk(KERN_ERR "%s:%d err: get defragprogress failed ret=%d\n\n", "micron_cld_operation_status", 103, ret);
 *status = (attr == 2 || attr == 3) ? 1 : attr == 1 ? 2 : 0;
 return ret;
}
const struct os44_cld_ops os44_micron_cld_ops = {
 .get_frag_level = os44_micron_get_frag_level,
 .set_trigger = os44_micron_cld_set_trigger,
 .get_trigger.boolean = os44_micron_cld_get_trigger,
 .operation_status = os44_micron_cld_operation_status,
};
