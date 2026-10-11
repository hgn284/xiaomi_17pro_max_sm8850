// SPDX-License-Identifier: GPL-2.0-only
#include "os44_xiaomi_owner.h"
#include "os44_snapshot_v2_sysfs.h"
/* Real getter intentionally remains unresolved until probe/publication closure. */
void os44_ufssnapshotV2_init(struct ufs_hba *hba)
{
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 struct os44_snapshot_v2_view *s=&owner->snapshot_v2;
 unsigned int i;
 s->hba=hba;
 for(i=0;i<6;i++) { s->records[i].ctrl=3;s->records[i].status=3; }
 if(os44_ufssnapshot_create_sysfsV2(&owner->snapshot,s))
  printk("\0013[err]%s: %d:[ufssnapshotV2] init hba ptr is NULL!!\n","ufssnapshotV2_init",453);
}
