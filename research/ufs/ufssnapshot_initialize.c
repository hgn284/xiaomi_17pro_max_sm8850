// SPDX-License-Identifier: GPL-2.0-only
#include <linux/delay.h>
#include <linux/string.h>
#include <scsi/scsi_device.h>
#include "os44_xiaomi_owner.h"
#include "os44_snapshot_sysfs.h"
#include "os44_snapshot_v2_sysfs.h"
/* Whole init reconstruction. No synthetic owner definition; original quirks retained. */
void os44_ufssnapshot_init(struct ufs_hba *hba)
{
 struct os44_remove_owner_view *o=os44_get_ufs_xiaomi_for_remove(),*o2;
 struct os44_snapshot_view *s=&o->snapshot;
 unsigned int i;u8 vendor;u32 value=0;int ret;char inquiry[28];
 if(!hba) { printk("\0013[err]%s: %d:[ufssnapshot] init hba ptr is NULL!!\n","ufssnapshot_init",1001);return; }
 s->hba=hba;s->ctrl=5;s->status=99;s->ee_count=0;
 o2=os44_get_ufs_xiaomi_for_remove();s->vendor=5;
 for(i=0;!hba->ufs_device_wlun && i<10;i++)msleep(200);
 if(!hba->ufs_device_wlun) { printk("\0013[err]%s: %d:[ufssnapshot] ufs_device_wlun init fail, maybe UFS had issues before this.\n","ufssnapshot_get_vendor",836);return; }
 memcpy(inquiry,hba->ufs_device_wlun->inquiry+8,sizeof(inquiry));
 if(!memcmp(inquiry,"SKhynix",7))vendor=0;
 else if(!memcmp(inquiry,"MICRON",6))vendor=1;
 else if(!memcmp(inquiry,"KIOXIA",6))vendor=2;
 else if(!memcmp(inquiry,"SAMSUNG",7))vendor=3;
 else if(!memcmp(inquiry,"XBSTOR",6))vendor=4;
 else vendor=5;
 s->vendor=vendor;o2->snapshot_v2.vendor=vendor;s->version=0xffff;s->supported=0;
 switch(s->vendor) {
 case 0:case 3:case 4:
  ret=os44_ufssnapshot_get_info(s,&s->supported,&s->version);break;
 case 2:ret=os44_ufssnapshot_get_info(s,&s->supported,NULL);break;
 case 1:
  ret=os44_ufssnapshot_query_attr(s->hba,3,0xe0,&value);
  if(ret) {printk("\0013[err]%s: %d:[ufssnapshot] micron get support attr failed, ret=%d.\n","ufssnapshot_get_support_micron",941,ret);return;}
  if(value & (1<<5)) {s->supported=1;s->version=0x100;}
  else if(value & (1<<10)) {s->supported=1;s->version=0x200;}
  break;
 default:printk("\0013[err]%s: %d:[ufssnapshot] vendor is unknown.\n","ufssnapshot_init",1029);return;
 }
 if(ret)return;
 if(os44_ufssnapshot_create_sysfs(s))return;
 os44_ufssnapshotV2_init(hba);
}
