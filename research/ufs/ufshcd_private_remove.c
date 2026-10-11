/* SPDX-License-Identifier: GPL-2.0-only */
/* Faithful HBA-signature adapters, not a cleanup/lifetime repair.
 * Stock ignores HBA and uses the shared owner. Getter remains unresolved. */
#include "os44_remove_owner_view.h"
void os44_ufsswap_remove_hba(struct ufs_hba *hba)
{
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 int ret;
 mutex_lock(&owner->swap.lock);
 ret=kobject_uevent(&owner->swap.kobj,KOBJ_REMOVE);
 printk(KERN_INFO "kobject removed (%d)",ret);
 kobject_del(&owner->swap.kobj);
 mutex_unlock(&owner->swap.lock);
}
void os44_ufseua_remove(struct ufs_hba *hba)
{
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 int ret;
 mutex_lock(&owner->eua_lock);
 ret=kobject_uevent(&owner->eua_kobj,KOBJ_REMOVE);
 printk(KERN_INFO "kobject removed (%d)",ret);
 kobject_del(&owner->eua_kobj);
 mutex_unlock(&owner->eua_lock);
}
