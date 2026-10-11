// SPDX-License-Identifier: GPL-2.0-only
/* Complete instruction-derived dispatcher; source-owned types, no original ABI claim. */
#include <ufs/ufshcd.h>
#include <scsi/scsi_device.h>
#include "os44_cld_init.h"
#include "os44_cld_samsung.h"
#include "os44_cld_hynix.h"
#include "os44_cld_kioxia.h"
#include "os44_cld_micron.h"
#include "os44_cld_ymtc.h"
#include "os44_cld_hid_write.h"
static_assert(offsetof(struct ufs_hba,ufs_device_wlun)==0x48);
static_assert(offsetof(struct scsi_device,inquiry)==0xf0);
static_assert(offsetof(struct ufs_hba,dev_info.wspecversion)==0xa00);
static_assert(offsetof(struct device,power.usage_count)==0x1e0);
static_assert(offsetof(struct ufs_hba,clk_gating.active_reqs)==0xbac);
/* Original writable 144-byte array; vendor names are inline, not pointers. */
struct os44_cld_vendor os44_cld_ops_arry[] = {
 {"SAMSUNG",1,&os44_samsung_cld_ops},
 {"SKhynix",1,&os44_hynix_cld_ops},
 {"KIOXIA",0,&os44_kioxia_cld_ops},
 {"MICRON",0,&os44_micron_cld_ops},
 {"WDC",0,&os44_wdc_cld_ops},
 {"XBSTOR",0,&os44_ymtc_cld_ops},
};
static_assert(sizeof(os44_cld_ops_arry)==144);
#define DLOG(line, fmt) printk(KERN_ERR "%40s:%3d [%01d%02d%02d] " fmt,"ufscld_init_ops",line,cld->trigger,atomic_read(&cld->hba->dev->power.usage_count),cld->hba->clk_gating.active_reqs)
#define ELOG(line, vendor) printk(KERN_ERR "%s:%d err: " vendor " init cld idn fail\n","ufscld_init_ops",line)
int os44_ufscld_init_ops(struct os44_cld_view *cld)
{
 struct ufs_hba *hba=cld->hba;
 int i,ret;
 if (!hba->ufs_device_wlun) {
  printk(KERN_ERR "%s:%d err: ufs_device_wlun init fail, maybe UFS had issues before this.\n","ufscld_init_ops",43);
  return 0;
 }
 memcpy(&cld->identity,hba->ufs_device_wlun->inquiry+8,sizeof(cld->identity));
 for(i=0;i<ARRAY_SIZE(os44_cld_ops_arry);i++) {
  if (!strncmp(cld->identity.vendor,os44_cld_ops_arry[i].vendor,strlen(os44_cld_ops_arry[i].vendor))) {
   cld->ops=os44_cld_ops_arry[i].ops;
   cld->vendor=&os44_cld_ops_arry[i];
  }
 }
 if (!strncmp(cld->identity.product,"XBUSC1B19A9TF2",14) || !strncmp(cld->identity.product,"XBUSC1B2AA9TF3",14)) {
  ret=os44_ymtc_init_idn(cld);
  if(ret) ELOG(61,"YMTC");
  return ret;
 }
 if(hba->dev_info.wspecversion>=0x410) {
  cld->ops=&os44_hid_ops;
  if (!strncmp(cld->identity.vendor,"SKhynix",7)) {
   if(cld->debug) DLOG(71,"Hynix UFS need set default defrag size\n");
   os44_ufshid_set_defragsize(cld);
  }
  return 0;
 }
 if (!strncmp(cld->identity.vendor,"SAMSUNG",7)) {
  ret=os44_Samsung_init_idn(cld);
  if(cld->debug) DLOG(78,"Samsung lower v8 savona master  UFS, execute SS HID\n");
  if(ret) ELOG(80,"SAMSUNG");
  return ret;
 }
 if (!strncmp(cld->identity.vendor,"KIOXIA",6)) {
  if(cld->debug) DLOG(84,"KIOXIA UFS is lower Gen13, execute KX HID\n");
  ret=os44_kioxia_init_idn(cld);
  if(ret) ELOG(87,"KIOXIA");
  return ret;
 }
 if (!strncmp(cld->identity.vendor,"MICRON",6)) {
  ret=os44_micron_init_idn(cld);
  if(ret) ELOG(92,"MICRON");
  return ret;
 }
 return 0;
}
