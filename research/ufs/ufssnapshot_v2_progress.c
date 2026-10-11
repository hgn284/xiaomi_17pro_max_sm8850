// SPDX-License-Identifier: GPL-2.0-only
/* Equivalent reconstruction, not recovered original private types. */
#include <linux/sysfs.h>
#include "os44_snapshot_v2.h"
int os44_ufssnapshot_query_progress(struct os44_snapshot_v2_view *s)
{
 struct ufs_hba *hba=s->hba;
 u32 value=0; unsigned int idn; int ret;
 switch(s->vendor) {
 case 1:idn=0x8a;break;
 case 3:idn=0xa0;break;
 case 4:idn=0x23;break;
 default:
 printk("\0013[err]%s: %d:[ufssnapshotV2] invalid vendor\n","ufssnapshot_query_progress",231);
 return -1;
 }
 ret=os44_ufssnapshotV2_query_attr(hba,3,idn,&value);
 if(ret) {
 printk("\0013[err]%s: %d:[ufssnapshotV2] query progress failed\n","ufssnapshot_query_progress",237);
 return ret;
 }
 s->progress=value;return 0;
}
ssize_t os44_ufssnapshotV2_sysfs_show_dropslot(struct os44_snapshot_v2_view *s,char *buf)
{ return sysfs_emit(buf,"%u\n",s->buffer_id); }
ssize_t os44_ufssnapshotV2_sysfs_store_dropslot(struct os44_snapshot_v2_view *s,const char *buf,size_t count)
{ unsigned long value=simple_strtoul(buf,NULL,10);s->buffer_id=value;s->selected=value;return count; }
ssize_t os44_ufssnapshotV2_sysfs_show_progress(struct os44_snapshot_v2_view *s,char *buf)
{ if(os44_ufssnapshot_query_progress(s))return sysfs_emit(buf,"%s\n","invalid");return sysfs_emit(buf,"%u\n",s->progress); }
