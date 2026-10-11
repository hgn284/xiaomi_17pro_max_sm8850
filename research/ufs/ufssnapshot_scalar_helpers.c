// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include "os44_snapshot.h"
struct os44_snapshot_idns { u8 vendor,idn1,idn2,drop; };
static const struct os44_snapshot_idns idn_maps[] = {
 {0x0,0x8a,0x8b,0x1d},
 {0x1,0x89,0x8b,0x8c},
 {0x2,0xa1,0xa3,0x1d},
 {0x3,0x9f,0xa0,0xa1},
 {0x4,0x22,0x23,0x24},
};
int os44_ufssnapshot_set_drop_freesize(struct os44_snapshot_view *s, u32 value)
{
 struct ufs_hba *hba=s->hba;
 const struct os44_snapshot_idns *p;
 int ret;
 switch(s->vendor) {
 case 1: p=&idn_maps[1]; break;
 case 4: p=&idn_maps[4]; break;
 default:
 printk("\0013[err]%s: %d:[ufssnapshot] set drop vendor %u is not support\n", "ufssnapshot_set_drop_freesize",800,s->vendor);
 return -1;
 }
 ret=os44_ufssnapshot_query_attr(hba,4,p->drop,&value);
 if (ret) {
 printk("\0013[err]%s: %d:[ufssnapshot] set drop free size attr failed, \n", "ufssnapshot_set_drop_freesize",810);
 return -1;
 }
 s->drop_freesize=value;
 return 0;
}
int os44_ufssnapshot_get_bBackupCancelled(struct os44_snapshot_view *s, u32 *value)
{
 int ret;
 if (s->vendor!=3) {
 printk("\0013[err]%s: %d:[ufssnapshot] get bBackupCancelled vendor %u is not support\n", "ufssnapshot_get_bBackupCancelled",965,s->vendor);
 return -1;
 }
 ret=os44_ufssnapshot_query_attr(s->hba,3,0xa2,value);
 if (ret) {
 printk("\0013[err]%s: %d:[ufssnapshot] samsung get bBackupCancelled failed, ret=%d.\n", "ufssnapshot_get_bBackupCancelled",972,ret);
 return -1;
 }
 return 0;
}
