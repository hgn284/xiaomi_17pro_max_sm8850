// SPDX-License-Identifier: GPL-2.0-only
#include "os44_snapshot_v2.h"
#include <linux/unaligned.h>
int os44_ufssnapshot_start_V2(struct os44_snapshot_v2_view *s)
{
 u8 data[200]={0};
 u32 value=1;
 struct ufs_hba *hba=s->hba;
 struct os44_snapshot_v2_record *r;
 unsigned int i,len;
 int ret;
 data[0]=s->selected;
 if(s->selected>=6) __builtin_trap();
 r=&s->records[s->selected];data[1]=r->count;
 data[2]=r->address>>40;data[3]=r->address>>32;data[4]=r->address>>24;
 data[5]=r->address>>16;data[6]=r->address>>8;data[7]=r->address;
 if(r->count==255) len=8;
 else {
  for(i=0;i<r->count;i++) {
   put_unaligned_be32(r->ranges[i].first,data+8+12*i);
   put_unaligned_be32(r->ranges[i].last-r->ranges[i].first+1,data+12+12*i);
   data[16+12*i]=r->ranges[i].lun;
  }
  len=8+12*r->count;
 }
 ret=os44_ufssnapshotV2_rw_buffer(s,s->vendor==1?0x1e:0x1d,1,s->selected,len,data,false);
 if(ret)printk("\0013[err]%s: %d:[ufssnapshotV2] write buffer failed, ret %d\n\n","ufssnapshot_start_V2",297,ret);
 else if(s->vendor==3) {
  int err=os44_ufssnapshotV2_query_attr(hba,4,0x9f,&value);
  if(err)printk("\0013[err]%s: %d:[ufssnapshotV2] write ctrl attr failed, err %d\n","ufssnapshot_start_V2",304,err);
 }
 return ret;
}
