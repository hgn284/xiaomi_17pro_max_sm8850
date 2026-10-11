// SPDX-License-Identifier: GPL-2.0-only
/* Whole stock function reconstruction. Original partial publication, ignored
 * sysfs error and exhausted-checklist trap are deliberately retained. */
#include "os44_xiaomi_owner.h"
#include "os44_eua_policy.h"
#include "os44_hwid.h"
#include <linux/slab.h>
#include <linux/unaligned.h>
int os44_ufseua_probe(struct ufs_hba *hba)
{
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 struct os44_remove_owner_view *policy_owner;
 char inquiry[28];u8 *buf;u16 feature;u64 capacity;u32 platform;int i;
 bool accepted=false;
 owner->eua.hba=hba;
 policy_owner=os44_get_ufs_xiaomi_for_remove();
 if(!hba->ufs_device_wlun){printk("\0013ufs_device_wlun init fail, maybe UFS had issues before this.");goto unsupported_policy;}
 memcpy(inquiry,hba->ufs_device_wlun->inquiry+8,28);
 for(i=0;i<12;i++) {
  struct os44_eua_policy_record *p=&os44_eua_policy[i];
  if(strncmp(inquiry,p->vendor,strlen(p->vendor)))continue;
  if(strncmp(inquiry+8,p->model,strlen(p->model)))continue;
  if(strlen(p->revision) && strncmp(inquiry+24,p->revision,strlen(p->revision))) {
   printk("\0013[UFSEUA]Firmware rev check fail\n");
   printk("\0016[UFSEUA]not factory or no need for check\n");goto unsupported_policy;
  }
  policy_owner->eua.ops=p->ops;policy_owner->eua.support_size=p->support_size;
  printk("\0016[UFSEUA]policy match[%d]",i+1);break;
 }
 if(i==12)goto unsupported_policy;
 if(!hba->ufs_device_wlun){printk("\0013ufs_device_wlun init fail, maybe UFS had issues before this.");goto support_check_fail;}
 memcpy(inquiry,hba->ufs_device_wlun->inquiry+8,28);
 buf=kzalloc(255,GFP_KERNEL);
 if(!buf){printk("\0013[UFSEUA]kzalloc fail\n");goto support_check_fail;}
 if(!memcmp(inquiry+8,"THGJFMT2E46BATVB",16)) {
  if(os44_ufseua_read_desc(hba,1,buf,255))goto free_fail;
  feature=get_unaligned_be16(buf+11);printk("\0013[UFSEUA]kioxia HPB mode:0x%x",feature>>8);
  if(os44_ufseua_read_desc(hba,2,buf,255))goto free_fail;
  capacity=get_unaligned_be64(buf+11);printk("\0013[UFSEUA]kioxia lun0 size:0x%llx",capacity);
  accepted=(feature&0x200) && capacity>0x7a6b800;
 } else if(!memcmp(inquiry,"XBSTOR",6)) {
  if(os44_ufseua_read_desc(hba,1,buf,255))goto free_fail;
  feature=get_unaligned_be16(buf+11);printk("\0013[UFSEUA]ymtc HPB mode:0x%x",feature>>8);
  if(os44_ufseua_read_desc(hba,7,buf,255))goto free_fail;
  capacity=get_unaligned_be64(buf+4);printk("\0016[UFSEUA]ymtc raw device capacity:0x%llx",capacity);
  if(!memcmp(inquiry+8,"XBUSC1B5AAATH3",14) && (feature&0x200) &&
    capacity+0xffffffffc24d5fffULL<=0x7fffe) {
   printk("\0013[UFSEUA] ufs eua success!");accepted=true;
  } else accepted=(capacity+0xffffffffc24d5fffULL+0x286000<0x7fff ||
                   capacity+0xffffffffe1383fffULL+0x118000<0x7ffff);
 } else {
  if(os44_ufseua_read_desc(hba,7,buf,255))goto free_fail;
  capacity=get_unaligned_be64(buf+4);printk("\0016[UFSEUA]raw device capacity:0x%llx",capacity);
  accepted=(!memcmp(inquiry+8,"HN8T281EJKX158",14) && capacity+0xffffffffc36d3fffULL<0x7ffff) ||
   capacity+0xffffffff84d67fffULL<0x7ffff ||
   capacity+0xffffffffc24d5fffULL+0x1fe000<0x7ffff ||
   capacity+0xffffffffe1383fffULL<0x7ffff;
 }
 if(!accepted)goto free_fail;
 kfree(buf);owner->eua.support=1;
 platform=get_hw_version_platform();printk("\0016[UFSEUA]hardware platform id%d\n",platform);
 if(platform<=9 && ((1U<<platform)&0x3be))owner->eua.enable=1;
 platform=get_hw_version_platform();printk("\0016[UFSEUA]%s: hardware platform id%d\n","ufseua_set_hide_info",platform);
 memcpy(inquiry,owner->eua.hba->ufs_device_wlun->inquiry+8,28);
 owner->eua.hide_size=0;
 for(i=0;i<3;i++) {
  struct os44_eua_check_record *c=&os44_eua_checklist[i];
  if(!c->platform)goto create;
  if(platform==c->platform && !strncmp(inquiry+8,c->model,strlen(c->model)))
   owner->eua.hide_size=c->hide_size;
 }
 __builtin_trap();
create:
 os44_ufseua_create_sysfs(&owner->eua);printk("\0016[UFSEUA]Support EUA Feture");return 0;
free_fail:
 kfree(buf);
support_check_fail:
 printk("\0016[UFSEUA]Not Support EUA, Check Raw Cap Fail");
 printk("\0016[UFSEUA]not factory or no need for check\n");return -EOPNOTSUPP;
unsupported_policy:
 printk("\0016[UFSEUA]Not Support EUA, Check Policy Fail");return -EOPNOTSUPP;
}
