// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include "os44_xiaomi_owner.h"
#include "os44_exception_contract.h"
void os44_ufssnapshot_inc_eecount(struct ufs_hba *hba)
{
 struct os44_remove_owner_view *owner = os44_get_ufs_xiaomi_for_remove();
 if (!hba) {
  printk(KERN_ERR "[err]%s: %d:[ufssnapshot] hba ptr is NULL!!\n", "ufssnapshot_inc_eecount", 984);
  return;
 }
 owner->snapshot_ee_count++;
}
