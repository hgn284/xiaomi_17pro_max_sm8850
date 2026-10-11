// SPDX-License-Identifier: GPL-2.0-only
#include <linux/slab.h>
#include <linux/sysfs.h>
#include "os44_snapshot_v2.h"
ssize_t os44_ufssnapshotV2_sysfs_show_range(struct os44_snapshot_v2_view *s,char *buf)
{
 u8 slot=s->selected; char *tmp,*p; ssize_t len,ret; unsigned long i;
 struct os44_snapshot_v2_record *record;
 if(slot>=6)return sysfs_emit(buf,"slot out of range\n");
 tmp=kzalloc(1024,GFP_KERNEL);if(!tmp)return -ENOMEM;
 record=&s->records[slot];
 if(s->debug==1)printk("\0016[info]%s:%d:ufssnapshotV2_sysfs_show_range %p, %p\n","ufssnapshotV2_sysfs_show_range",134,tmp,buf);
 len=snprintf(tmp,1024,"%u;%llu;%u;",slot,record->address,record->count);
 if(!len)return -EINVAL; /* Stock leaks tmp on formatting failure; retained. */
 if(s->debug==1)printk("\0016[info]%s:%d:ufssnapshotV2_sysfs_show_range %u;%llu;%u;%llu\n","ufssnapshotV2_sysfs_show_range",141,slot,record->address,record->count,(u64)len);
 p=tmp+len;
 if(record->count>0 && record->count<=16)
  for(i=0;i<record->count;i++) {
   len=snprintf(p,1024,"%u,%u,%u;",record->ranges[i].lun,record->ranges[i].first,record->ranges[i].last);
   if(!len)return -EINVAL;
   p+=len;
  }
 ret=sysfs_emit(buf,"%s\n",tmp);kfree(tmp);return ret;
}
ssize_t os44_ufssnapshotV2_sysfs_store_range(struct os44_snapshot_v2_view *s,const char *buf,size_t count)
{
 char *tmp,*cursor,*token,*part; unsigned long slot,n=0,i;
 struct os44_snapshot_v2_record *r;
 cursor=tmp=kmalloc(strlen(buf)+1,GFP_KERNEL);if(!tmp)return -ENOMEM;
 strncpy(tmp,buf,strlen(buf)+1);
 token=strsep(&cursor,";");if(!token)goto invalid;
 slot=simple_strtoul(token,NULL,10);if(slot>=6)goto invalid;
 r=&s->records[slot];s->selected=slot;r->slot=slot;
 token=strsep(&cursor,";");if(token)r->address=simple_strtoul(token,NULL,10);
 token=strsep(&cursor,";");if(token)n=simple_strtoul(token,NULL,10);
 r->count=n;
 for(i=0;i<16;i++) {
  token=strsep(&cursor,";");if(!token)break;
  part=strsep(&token,",");if(part)r->ranges[i].lun=simple_strtoul(part,NULL,16);
  part=strsep(&token,",");if(part)r->ranges[i].first=simple_strtoul(part,NULL,16);
  part=strsep(&token,",");if(part)r->ranges[i].last=simple_strtoul(part,NULL,16);
 }
 kfree(tmp);return i>n ? -EINVAL : count;
invalid:
 kfree(tmp);return -EINVAL;
}
