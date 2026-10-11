// SPDX-License-Identifier: GPL-2.0-only
#include <linux/sysfs.h>
#include <linux/string.h>
#include "os44_xiaomi_owner.h"
#include "ufssnapshot_ctrl.h"
struct os44_snapshot_ctrl_name os44_ctrl_maps[5] = {{0,"stop"},{1,"start"},{2,"restore"},{3,"drop"},{4,"enforce"}};
ssize_t os44_ufssnapshot_sysfs_show_ctrl(struct os44_snapshot_view *s, char *buf)
{
 char v2_name[20]={0},v1_name[20];
 const char *name="none";
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 unsigned int i;
 if(s->version==0x200) {
  if(!os44_ufssnapshotV2_find_ctrl_name_default(owner->snapshot_v2_ctrl[owner->snapshot_v2_selected].ctrl,v2_name))
   name=v2_name;
 } else {
  if(s->ctrl>3)
   printk("\0013[err]%s: %d:[ufssnapshot] Bsnapshotctrl input error, ctrl %d\n\n","ufssnapshot_find_ctrl_name_default",142,s->ctrl);
  else {
   for(i=0;i<5;i++) if(os44_ctrl_maps[i].ctrl==s->ctrl) {
    strncpy(v1_name,os44_ctrl_maps[i].name,20);name=v1_name;break;
   }
   if(i==5) printk("\0013[err]%s: %d:[ufssnapshot] Bsnapshotctrl name not find, ctrl %d\n\n","ufssnapshot_find_ctrl_name_default",154,s->ctrl);
  }
 }
 return sysfs_emit(buf,"%s\n",name);
}
