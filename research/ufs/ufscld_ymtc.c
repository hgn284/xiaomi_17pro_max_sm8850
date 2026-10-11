// SPDX-License-Identifier: GPL-2.0-only
/* Equivalent source reconstruction, not original private declarations. */
#include <ufs/ufshcd.h>
#include "os44_cld_ymtc.h"
#include "os44_bkops_contract.h"
#include "os44_device_init_helpers.h"
static_assert(offsetof(struct ufs_hba, ufs_version) == 0xa0);
/* Stock stores bytes 0/1 and maps them to actual IDNs at use sites. */
static bool os44_ymtc_query_flag_idn_cld_enable;
static bool os44_ymtc_query_attr_idn_cld_level;
static bool os44_ymtc_query_attr_idn_defrag;
static const u32 os44_ymtc_status_map[] = { 2, 3, 1 };
int os44_ymtc_init_idn(struct os44_cld_view *cld)
{
 if (cld->hba->ufs_version >= 0x400) {
  printk(KERN_INFO "%s:%d info: [ufscld] YMTC UFS is UFS4.0 or later.\n", "ymtc_init_idn", 31);
  os44_ymtc_query_flag_idn_cld_enable = true;
  os44_ymtc_query_attr_idn_cld_level = true;
  os44_ymtc_query_attr_idn_defrag = true;
  return 0;
 }
 printk(KERN_ERR "%s:%d err: [ufscld] Please check YMTC datasheet.\n", "ymtc_init_idn", 36);
 return -1; /* Failed re-initialization preserves preceding valid values. */
}
int os44_ymtc_get_frag_level(struct os44_cld_view *cld, u32 *level)
{
 u32 attr = ~0U;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR,
   os44_ymtc_query_attr_idn_cld_level ? 0x34 : 0, 0, 0, &attr);
 if (ret) {
  printk(KERN_ERR "%s:%d err: [ufscld] ymtc get_frag_level failed, ret=%d\n", "ymtc_get_frag_level", 50, ret);
  return ret;
 }
 switch (attr) {
 case 0: *level = 0; break;
 case 1: *level = 1; break;
 case 2: *level = 2; break;
 case 3: *level = 3; break;
 default:
  printk(KERN_INFO "%s:%d info: [ufscld] ymtc cld unknown level %d\n", "ymtc_get_frag_level", 63, attr);
  return -1;
 }
 return 0;
}
int os44_ymtc_cld_set_trigger(struct os44_cld_view *cld, unsigned int enable)
{
 int ret = os44_ufshcd_query_flag_retry(cld->hba,
   enable ? UPIU_QUERY_OPCODE_SET_FLAG : UPIU_QUERY_OPCODE_CLEAR_FLAG,
   os44_ymtc_query_flag_idn_cld_enable ? 0x14 : 0, 0, NULL);
 if (ret) printk(KERN_ERR "%s:%d err: [ufscld] ymtc cld_set_trigger %d failed, ret=%d\n", "ymtc_cld_set_trigger", 81, enable, ret);
 return ret;
}
int os44_ymtc_cld_get_trigger(struct os44_cld_view *cld, u32 *trigger)
{
 bool flag = false;
 /* Preserve original SET_FLAG opcode 6, despite function's get name. */
 int ret = os44_ufshcd_query_flag_retry(cld->hba, UPIU_QUERY_OPCODE_SET_FLAG,
   os44_ymtc_query_flag_idn_cld_enable ? 0x14 : 0, 0, &flag);
 if (ret) printk(KERN_ERR "%s:%d err: [ufscld] ymtc cld_get_trigger failed, ret=%d\n", "ymtc_cld_get_trigger", 94, ret);
 *trigger = flag;
 return ret;
}
int os44_ymtc_cld_operation_status(struct os44_cld_view *cld, u32 *status)
{
 u32 attr = 0;
 int ret = local_attr_retry(cld->hba, UPIU_QUERY_OPCODE_READ_ATTR,
   os44_ymtc_query_attr_idn_defrag ? 0x35 : 0, 0, 0, &attr);
 if (ret) {
  printk(KERN_ERR "%s:%d err: [ufscld] ymtc cld_get_defragprogress failed ret=%d\n", "ymtc_cld_get_defragprogress", 108, ret);
  printk(KERN_ERR "%s:%d err: [ufscld] ymtc get defragprogress failed ret=%d\n", "ymtc_cld_operation_status", 120, ret);
 }
 *status = attr >= 1 && attr <= 3 ? os44_ymtc_status_map[attr - 1] : 0;
 return ret;
}
const struct os44_cld_ops os44_ymtc_cld_ops = {
 .get_frag_level = os44_ymtc_get_frag_level,
 .set_trigger = os44_ymtc_cld_set_trigger,
 .get_trigger.word = os44_ymtc_cld_get_trigger,
 .operation_status = os44_ymtc_cld_operation_status,
};
