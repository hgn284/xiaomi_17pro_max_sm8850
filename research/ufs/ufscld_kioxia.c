// SPDX-License-Identifier: GPL-2.0-only
/* Equivalent reconstruction; original private C type identity unclaimed.
 * Original Kioxia IDNs are distinct .bss objects at 0x380c/10/14.
 * No fabricated owner or generic dispatcher; real init_ops remains pending. */
#include <ufs/ufshcd.h>
#include "os44_cld_kioxia.h"
#include "os44_bkops_contract.h"
#include "os44_device_init_helpers.h"
static unsigned int os44_kioxia_query_attr_flag_cld_enable;
static unsigned int os44_kioxia_query_attr_idn_cld_level;
static unsigned int os44_kioxia_query_attr_idn_defrag;
int os44_kioxia_init_idn(struct os44_cld_view *cld)
{
 struct os44_cld_identity id = cld->identity;
 if (!strncmp(id.vendor, "KIOXIA", 6)) {
  if (!strncmp(id.product, "THGJFL", 6)) {
   printk(KERN_INFO "%s:%d info: KIOXIA UFS is Gen12\n", "kioxia_init_idn", 42);
   os44_kioxia_query_attr_flag_cld_enable = 0xa0;
   os44_kioxia_query_attr_idn_cld_level = 0xa0;
   os44_kioxia_query_attr_idn_defrag = 0xa1;
  } else if (!strncmp(id.product, "THGJFJ", 6)) {
   printk(KERN_INFO "%s:%d info: KIOXIA UFS is Gen11\n", "kioxia_init_idn", 47);
   os44_kioxia_query_attr_flag_cld_enable = 0x13;
   os44_kioxia_query_attr_idn_cld_level = 0x34;
   os44_kioxia_query_attr_idn_defrag = 0x35;
  } else {
   printk(KERN_ERR "%s:%d err: Please check KIOXIA datasheet.\n", "kioxia_init_idn", 52);
   return -1;
  }
 }
 return 0;
}
int os44_kioxia_get_frag_level(struct os44_cld_view *cld, u32 *level)
{
 u32 attr = ~0U;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR,
  os44_kioxia_query_attr_idn_cld_level, 0, 0, &attr);
 if (ret) return ret;
 if (attr == 1) *level=2;
 else if (attr == 0) *level=1;
 else if (attr == 2 || attr == 3) *level=3;
 else {
  printk(KERN_ERR "%s:%d err: kioxia cld unknown level %d\n\n", "kioxia_get_frag_level", 74, attr);
  return -1;
 }
 return 0;
}
int os44_kioxia_cld_set_trigger(struct os44_cld_view *cld, unsigned int enable)
{
 int ret;
 if (enable) {
  ret = os44_ufshcd_query_flag_retry(cld->hba, UPIU_QUERY_OPCODE_SET_FLAG,
   os44_kioxia_query_attr_flag_cld_enable, 0, NULL);
  if (ret) {
   printk(KERN_ERR "%s:%d err: %s:failed. flag_idn=%d, trigger=%u\n\n", "kioxia_cld_set_trigger", 89, "kioxia_cld_set_trigger", os44_kioxia_query_attr_flag_cld_enable, enable);
   return ret;
  }
 } else {
  ret = os44_ufshcd_query_flag_retry(cld->hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
   os44_kioxia_query_attr_flag_cld_enable, 0, NULL);
  if (ret) {
   printk(KERN_ERR "%s:%d err: %s:failed. flag_idn=%d, trigger=%u\n\n", "kioxia_cld_set_trigger", 94, "kioxia_cld_set_trigger", os44_kioxia_query_attr_flag_cld_enable, enable);
   return ret;
  }
 }
 return 0;
}
int os44_kioxia_cld_get_trigger(struct os44_cld_view *cld, u32 *trigger)
{
 /* Explicit byte address within a declared four-byte caller object. No mixed
  * callback pointer cast. Preserve original low-byte write and full-u32 log. */
 int ret = os44_ufshcd_query_flag_retry(cld->hba, UPIU_QUERY_OPCODE_READ_FLAG,
  os44_kioxia_query_attr_flag_cld_enable, 0, (bool *)trigger);
 if (ret) printk(KERN_ERR "%s:%d err: %s:failed. flag_idn=%d, trigger=%u\n\n", "kioxia_cld_get_trigger", 108, "kioxia_cld_get_trigger", os44_kioxia_query_attr_flag_cld_enable, *trigger);
 return ret;
}
int os44_kioxia_cld_operation_status(struct os44_cld_view *cld, u32 *status)
{
 u32 attr = 0;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR,
  os44_kioxia_query_attr_idn_defrag, 0, 0, &attr);
 if (ret) printk(KERN_ERR "%s:%d err: get defragprogress failed ret=%d\n\n", "kioxia_cld_operation_status", 133, ret);
 if (attr == 0 || attr == 3 || attr == 4) *status=1;
 else if (attr == 1 || attr == 2) *status=2;
 else *status=0;
 printk(KERN_INFO "%s:%d info: %s:defragprogress=%d, op_status=%d\n\n", "kioxia_cld_operation_status", 144, "kioxia_cld_operation_status", attr, *status);
 return ret;
}
const struct os44_cld_ops os44_kioxia_cld_ops = {
 .get_frag_level=os44_kioxia_get_frag_level,
 .set_trigger=os44_kioxia_cld_set_trigger,
 .get_trigger.word =os44_kioxia_cld_get_trigger,
 .operation_status=os44_kioxia_cld_operation_status,
};
