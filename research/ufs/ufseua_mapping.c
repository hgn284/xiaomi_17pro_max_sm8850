// SPDX-License-Identifier: GPL-2.0-only
/* Original behavior reconstruction, including the moving-pointer free defect.
 * Not deployed. See lifecycle_findings.json before any safety repair branch. */
#include <linux/slab.h>
#include "os44_eua.h"
ssize_t os44_ufseua_sysfs_show_eua_get_mapping_status(struct os44_eua_view *eua,char *buf)
{
 u8 *map=kzalloc(512,GFP_KERNEL);int len=0,i;u16 length;
 if(!map)return -ENOMEM;
 length=eua->mapping_length;
 if(length>512)length=eua->mapping_length=512;
 if(eua->ops && eua->ops->get_mapping_status && !eua->ops->get_mapping_status(eua,map)) {
  for(i=0;i<(length+7)/8;i++)len+=snprintf(buf+len,PAGE_SIZE-len,"%02x",map[i]);
  len+=snprintf(buf+len,PAGE_SIZE-len,"\n");
 } else printk("\0013[UFSEUA]Get mapping status failed");
 kfree(map);return len;
}
ssize_t os44_ufseua_sysfs_store_eua_get_mapping_status(struct os44_eua_view *eua,const char *buf,size_t count)
{
 char *cursor,*token;unsigned long long value=0;int ends=0,index=1;
 cursor=kstrdup(buf,GFP_KERNEL);if(!cursor)goto error;
 while((token=strsep(&cursor,",")) && ends<=1) {
  if(kstrtoull(token,16,&value))goto free_error;
  if(index&1)eua->mapping_start=value;
  else {if(value<eua->mapping_start)goto free_error;eua->mapping_end=value;ends++;}
  index++;
 }
 eua->mapping_length=eua->mapping_end-eua->mapping_start+1;
 kfree(cursor);
 printk("\0016[UFSEUA]pre[0x%llx],post[0x%llx]\n",eua->mapping_start,eua->mapping_end);
 return count;
free_error:kfree(cursor);
error:printk("\0013[UFSEUA]Get lba range level failed");return count;
}
