// SPDX-License-Identifier: GPL-2.0-only
/* Equivalent source reconstruction. Retains stock error/PM/lifetime semantics;
 * does not establish original private type identity or repair stock leaks. */
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include "os44_xiaomi_owner.h"
#include "os44_fbo_constructor.h"
#include "os44_fbo_query.h"
#include "ufsswap_query_reconstruction.h"
static __always_inline int read_attr(struct ufs_hba *hba,u8 idn,u32 *value)
{
 int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ret=os44_query_attr(hba,3,idn,0,0,value);
 if(ret) {
  printk("\0013[err]%s:%d:Query attr [0x%.2X] failed. opcode(%d) (%d)\n","ufsfbo_query_attr",55,idn,3,ret);
  os44_query_error(1,0,-1,0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
void os44_ufsfbo_init(struct ufs_hba *hba)
{
 struct os44_fbo_lifetime *fbo=&os44_get_ufs_xiaomi_for_remove()->fbo;
 unsigned char inquiry[28];
 struct device_node *node;
 u64 start=0,end=0;
 int ret,i;
 fbo->hba=hba;fbo->control_010=0;fbo->debug=0;fbo->block_suspend=0;
 fbo->is_ufs31=0;fbo->pin_check_pending=0;fbo->total_blocks=0;fbo->vendor_selector=6;
 for(i=0;!hba->ufs_device_wlun && i<10;i++)msleep(200);
 if(!hba->ufs_device_wlun) {
  printk("\0013[err]%s:%d:ufs_device_wlun init fail, maybe UFS had issues before this.\n","ufsfbo_get_vendor_info",1028);
  printk("\0013[err]%s:%d:Get vendor info fail\n","ufsfbo_init",1166);return;
 }
 memcpy(inquiry,hba->ufs_device_wlun->inquiry+8,28);
 if(!memcmp(inquiry,"SKhynix",7))fbo->vendor_selector=0;
 else if(!memcmp(inquiry,"MICRON",6))fbo->vendor_selector=1;
 else if(!memcmp(inquiry,"WDC",3))fbo->vendor_selector=2;
 else if(!memcmp(inquiry,"KIOXIA",6))fbo->vendor_selector=3;
 else if(!memcmp(inquiry,"SAMSUNG",7))fbo->vendor_selector=4;
 else if(!memcmp(inquiry,"XBSTOR",6))fbo->vendor_selector=5;
 memcpy(fbo->model,inquiry+8,15);fbo->model[15]=0;strim(fbo->model);
 if(fbo->debug==1)printk("\0016[info]%s:%d:vendor:%d\n","ufsfbo_get_vendor_info",1051,fbo->vendor_selector);
 ret=os44_ufsfbo_get_fbo_support_state(fbo);
 if(ret) {printk("\0013[err]%s:%d:NOT Support FBO. ret(%d)\n","ufsfbo_init",1172,ret);return;}
 if(fbo->is_ufs31) {
  hba=fbo->hba;
  ret=read_attr(hba,0x33,&fbo->recommended_lrs);
  if(ret){printk("\0013[err]%s:%d:Get recommanded LBA range size failed\n","ufsfbo_get_fbs_attr_info",1063);goto fbs_error;}
  ret=read_attr(hba,0x34,&fbo->maximum_lrs);
  if(ret){printk("\0013[err]%s:%d:Get max LBA range size failed\n","ufsfbo_get_fbs_attr_info",1068);goto fbs_error;}
  ret=read_attr(hba,0x35,&fbo->minimum_lrs);
  if(ret){printk("\0013[err]%s:%d:Get min LBA range size failed\n","ufsfbo_get_fbs_attr_info",1073);goto fbs_error;}
  ret=read_attr(hba,0x36,&fbo->maximum_lrc);
  if(ret){printk("\0013[err]%s:%d:Get max LBA range count failed\n","ufsfbo_get_fbs_attr_info",1078);goto fbs_error;}
  ret=read_attr(hba,0x37,&fbo->lra);
  if(ret){printk("\0013[err]%s:%d:Get LBA range alignment failed\n","ufsfbo_get_fbs_attr_info",1083);goto fbs_error;}
 } else {
  ret=os44_ufsfbo_get_fbo_desc_info(fbo);
  if(ret){printk("\0013[err]%s:%d:Failed getting fbo info. ret(%d)\n","ufsfbo_init",1186,ret);return;}
 }
 fbo->scan_interval_ms=2000;fbo->is_auto_enabled=ufshcd_is_auto_hibern8_supported(fbo->hba);
 if(os44_ufsfbo_create_sysfs(fbo)) {
  printk("\0013[err]%s:%d:Sysfs init fail, disabled fbo driver\n","ufsfbo_init",1201);
  os44_get_ufs_xiaomi_for_remove()->fbo.state=-2;return;
 }
 if(fbo->vendor_selector==5)printk("\0013[err]%s:%d:vendor:%d, not support FBO2\n","ufsfbo_get_linear_mapping_info",1120,5);
 else {
  node=of_find_node_by_path("/memory/superinfo");
  if(node) {
   ret=of_property_read_u64(node,"super_start_lba",&start);
   if(ret)printk("\0013[err]%s:%d:read linear mapping start lba err\n\n","ufsfbo_anal_fdt_info",1101);
   else {
    ret=of_property_read_u64(node,"super_end_lba",&end);
    if(ret)printk("\0013[err]%s:%d:read linear mapping end lba err\n\n","ufsfbo_anal_fdt_info",1106);
    else {fbo->linear_start=start;fbo->linear_end=end;if(start && start<end)fbo->linear_state=1;}
   }
   /* Stock retains the node reference: no unverified lifetime repair here. */
  }
 }
 if(fbo->vendor_selector==3 && fbo->version>=0x200) {
  hba=fbo->hba;
  if(fbo->debug==1)printk("\0016[info]%s:%d:Enable fbo2 for kioxia\n","ufsfbo_enable_fbo2",1142);
  if(os44_ufsfbo_query_flag(hba,6,0xa4,NULL)) {
   printk("\0013[err]%s:%d:Enable FBO2.0 For Some Vendor Fail\n","ufsfbo_init",1209);return;
  }
 }
 if(fbo->debug==1)printk("\0016[info]%s:%d:FBO Init and create sysfs finished\n","ufsfbo_init",1214);
 {static struct lock_class_key __key;__mutex_init(&fbo->range_lock,"&fbo->range_check_lock",&__key);}
 os44_get_ufs_xiaomi_for_remove()->fbo.state=1;
 return;
fbs_error:
 printk("\0013[err]%s:%d:Failed getting fbs info. ret(%d)\n","ufsfbo_init",1179,ret);
}
