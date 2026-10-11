// SPDX-License-Identifier: GPL-2.0-only
/* Instruction-derived source pair; not an original private-type declaration. */
#include <ufs/ufshcd.h>
#include "os44_cld_hid_write.h"
#include "os44_bkops_contract.h"
#include "os44_device_init_helpers.h"
#define HID_LOG(fn, line, fmt, ...) printk(KERN_ERR "%40s:%3d [%01d%02d%02d] " fmt, fn, line, cld->trigger, atomic_read(&cld->hba->dev->power.usage_count), cld->hba->clk_gating.active_reqs, ##__VA_ARGS__)
int os44_ufshid_defrag(struct os44_cld_view *cld, int level)
{
 struct ufs_hba *hba=cld->hba;
 int size=0, ret;
 u32 operation;
 ret=local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, 0x36, 0, 0, (u32 *)&size);
 if (ret) { printk(KERN_ERR "%s:%d err: Get dHIDAvailableSize failed ret=%d\n\n", "ufshid_defrag",99,ret); return -1; }
 if (cld->debug) HID_LOG("ufshid_defrag",104,"query dHIDAvailableSize = %d\n\n",size);
 if (level==3 && size<1) return 2;
 if (strncmp(cld->identity.vendor,"SKhynix",7)) {
  ret=local_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, 0x37, 0, 0, (u32 *)&size);
  if (ret) { printk(KERN_ERR "%s:%d err: Set dHIDAvailableSize fail, ret=%d\n\n", "ufshid_defrag",118,ret); return -1; }
 }
 operation=2;
 if (cld->debug) HID_LOG("ufshid_defrag",125,"Set dHIDDefragOperation=%d\n\n",operation);
 ret=local_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, 0x35, 0, 0, &operation);
 if (ret) { printk(KERN_ERR "%s:%d err: Set HIDDefragOperation fail, ret=%d\n\n", "ufshid_defrag",130,ret); return -1; }
 return 0;
}
int os44_ufshid_disable(struct os44_cld_view *cld)
{
 struct ufs_hba *hba=cld->hba;
 u32 operation=0;
 int ret;
 if (cld->debug) HID_LOG("ufshid_disable",151,"Set dHIDDefragOperation=%d\n\n",operation);
 ret=local_attr_retry(hba,UPIU_QUERY_OPCODE_WRITE_ATTR,0x35,0,0,&operation);
 if(ret) printk(KERN_ERR "%s:%d err: Set dHIDDefragOperation fail, ret=%d\n\n","ufshid_disable",156,ret);
 return ret;
}
int os44_ufshid_check_progress(struct os44_cld_view *cld,int level)
{
 int val=0,ret=local_attr_retry(cld->hba,UPIU_QUERY_OPCODE_READ_ATTR,0x38,0,0,(u32 *)&val);
 if(ret) { printk(KERN_ERR "%s:%d err: Get val failed ret=%d\n\n","ufshid_check_progress",178,ret); return -1; }
 if(cld->debug) HID_LOG("ufshid_check_progress",183,"defrag progressratio val = %d\n\n",val);
 return val<1 && level==3 ? 2:0;
}
int os44_hid_set_trigger(struct os44_cld_view *cld,u32 trigger)
{
 u32 level=0;
 int retry,ret;
 if (!trigger) { os44_ufshid_disable(cld); return 0; }
 for (retry=1;retry<=10;retry++) {
  if (cld->debug) HID_LOG("hid_set_trigger",266,"hid trigger retry = %d\n\n",retry);
  ret=os44_hid_get_frag_level(cld,&level);
  if(ret) { printk(KERN_ERR "%s:%d err: hid_get_frag_level fail\n\n","hid_set_trigger",269);return ret; }
  switch(level) {
  case 1:
   if(cld->debug) HID_LOG("hid_set_trigger",276,"not need defrag\n\n");
   return 0;
  case 2: break;
  case 3:
   ret=os44_ufshid_defrag(cld,level);
   if(ret==2) continue;
   if(ret) printk(KERN_ERR "%s:%d err: ufshid_defrag fail.\n\n","hid_set_trigger",288);
   break;
  default:
   printk(KERN_ERR "%s:%d err: frag_level error, not support.\n\n","hid_set_trigger",293); return 0;
  }
  ret=os44_ufshid_check_progress(cld,level);
  if(ret && ret!=2) { printk(KERN_ERR "%s:%d err: ufshid_check_progress fail\n\n","hid_set_trigger",303); return ret; }
 }
 return 0;
}
const struct os44_cld_ops os44_hid_ops = {
 .get_frag_level=os44_hid_get_frag_level,
 .set_trigger=os44_hid_set_trigger,
 .get_trigger.word =os44_hid_get_trigger,
 .operation_status=os44_hid_operation_status,
};
