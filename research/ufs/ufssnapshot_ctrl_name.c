// SPDX-License-Identifier: GPL-2.0-only
#include <linux/string.h>
#include "ufssnapshot_ctrl.h"
struct os44_snapshot_ctrl_name os44_ctrl_mapsV2[3] = {{0,"stop"},{1,"start"},{2,"restore"}};
long os44_ufssnapshotV2_find_ctrl_name_default(u32 ctrl, char *name)
{
 unsigned int i;
 if(ctrl>3 || !name) {
  printk("\0013[err]%s: %d:[ufssnapshotV2] Bsnapshotctrl input error, ctrl %d\n\n","ufssnapshotV2_find_ctrl_name_default",68,ctrl);
  return -EINVAL;
 }
 for(i=0;i<3;i++) if(os44_ctrl_mapsV2[i].ctrl==ctrl) {
  strncpy(name,os44_ctrl_mapsV2[i].name,20); return 0;
 }
 printk("\0013[err]%s: %d:[ufssnapshotV2] Bsnapshotctrl name not find, ctrl %d\n\n","ufssnapshotV2_find_ctrl_name_default",80,ctrl);
 return -EIO;
}
