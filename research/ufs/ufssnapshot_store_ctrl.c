/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/string.h>
#include "os44_xiaomi_owner.h"
#include "os44_snapshot_sysfs.h"
#include "ufssnapshot_ctrl.h"
#include "ufssnapshot_flow.h"
ssize_t os44_ufssnapshot_sysfs_store_ctrl(struct os44_snapshot_view *snapshot, const char *buf, size_t count)
{
 u32 ctrl=0; unsigned int i;
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 if(snapshot->version==0x200) {
  if(os44_ufssnapshot_find_ctrl_by_nameV2(&ctrl,buf))
   printk("\0013[err]%s: %d:[ufssnapshotV2] Ctrl not find by name\n", "ufssnapshot_sysfs_store_ctrl",219);
  else os44_ufssnapshot_do_ctrl_V2(&owner->snapshot_v2,ctrl);
  return count;
 }
 for(i=0;i<5;i++) {
  if(!strncmp(os44_ctrl_maps[i].name,buf,strlen(os44_ctrl_maps[i].name))) {
   os44_ufssnapshot_do_ctrl(snapshot,os44_ctrl_maps[i].ctrl);return count;
  }
 }
 printk("\0013[err]%s: %d:[ufssnapshot] Bsnapshotctrl ctrl not find, name %s\n\n","ufssnapshot_find_ctrl_by_name_default",173,buf);
 printk("\0013[err]%s: %d:[ufssnapshot] Ctrl not find by name\n","ufssnapshot_sysfs_store_ctrl",231);
 return count;
}
