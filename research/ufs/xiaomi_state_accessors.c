// SPDX-License-Identifier: GPL-2.0-only
/* Retained standalone stock entrypoints omitted from prior inlined paths.
 * HBA argument is unused by stock bodies. Reconstruction-owned signatures;
 * no original private KCFI identity is asserted. No duplicate storage. */
#include "os44_xiaomi_owner.h"
int os44_ufscld_is_not_present(struct ufs_hba *hba)
{
 int state=atomic_read(&os44_get_ufs_xiaomi_for_remove()->cld.state);
 if(state==1)return 0;
 printk(KERN_INFO "%s:%d info: cld_state != cld_PRESENT (%d)\n","ufscld_is_not_present",105,state);
 return -ENODEV;
}
void os44_ufscld_set_state(struct ufs_hba *hba,int state)
{ atomic_set(&os44_get_ufs_xiaomi_for_remove()->cld.state,state); }
int os44_ufsfbo_get_state(struct ufs_hba *hba)
{ return os44_get_ufs_xiaomi_for_remove()->fbo.state; }
void os44_ufsfbo_set_state(struct ufs_hba *hba,int state)
{ os44_get_ufs_xiaomi_for_remove()->fbo.state=state; }
int os44_ufsswap_get_state(struct ufs_hba *hba)
{ return os44_get_ufs_xiaomi_for_remove()->swap.swap.support_state; }
void os44_ufsswap_set_state(struct ufs_hba *hba,int state)
{ os44_get_ufs_xiaomi_for_remove()->swap.swap.support_state=state; }
static_assert(offsetof(struct os44_remove_owner_view,cld.state)==0x90);
static_assert(offsetof(struct os44_remove_owner_view,fbo.state)==0x6e4);
static_assert(offsetof(struct os44_remove_owner_view,swap.swap.support_state)==0x190);
