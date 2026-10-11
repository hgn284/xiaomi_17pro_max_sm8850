// SPDX-License-Identifier: GPL-2.0-only
#include "os44_snapshot.h"
struct os44_stop_idns { u8 vendor,idn1,idn2,drop; };
static const struct os44_stop_idns idn_maps[] = {
 {0,0x8a,0x8b,0x1d},{1,0x89,0x8b,0x8c},{2,0xa1,0xa3,0x1d},{3,0x9f,0xa0,0xa1},{4,0x22,0x23,0x24}};
int os44_ufssnapshot_stop(struct os44_snapshot_view *s)
{
 u32 value=0;
 struct ufs_hba *hba=s->hba;
 const struct os44_stop_idns *p;
 int ret;
 switch(s->vendor) {
 case 0:p=&idn_maps[0];break;
 case 1:p=&idn_maps[1];break;
 case 2:p=&idn_maps[2];break;
 case 3:p=&idn_maps[3];break;
 case 4:p=&idn_maps[4];break;
 default:printk("\0013[err]%s: %d:[ufssnapshot] find idn failed, vendor %d\n","ufssnapshot_find_idn",318,s->vendor);return -1;
 }
 ret=os44_ufssnapshot_query_attr(hba,4,p->idn1,&value);
 if(ret)printk("\0013[err]%s: %d:[ufssnapshot] write ctrl attr failed, \n","ufssnapshot_stop",442);
 return ret;
}
