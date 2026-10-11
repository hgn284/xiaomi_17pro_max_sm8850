// SPDX-License-Identifier: GPL-2.0-only
#include <linux/string.h>
#include "ufssnapshot_ctrl.h"
long os44_ufssnapshot_find_ctrl_by_nameV2(u32 *ctrl, const char *name)
{
 unsigned int i;
 for(i=0;i<3;i++) {
  if(!strncmp(os44_ctrl_mapsV2[i].name,name,strlen(os44_ctrl_mapsV2[i].name))) {
   *ctrl=os44_ctrl_mapsV2[i].ctrl; return 0;
  }
 }
 printk("\0013[err]%s: %d:[ufssnapshotV2] Bsnapshotctrl ctrl not find, name %s\n\n","ufssnapshot_find_ctrl_by_nameV2",99,name);
 return -EIO;
}
