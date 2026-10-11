// SPDX-License-Identifier: GPL-2.0-only
/* Source-owned adapter for the stock get_ufs_xiaomi()+0x170 contract.
 * No independent storage, NULL fallback, or private type identity claim. */
#include "os44_xiaomi_owner.h"
static_assert(offsetof(struct os44_remove_owner_view,swap.swap)==0x170);
static_assert(offsetof(struct os44_remove_owner_view,swap.swap.support_state)==0x190);
struct os44_swap_view *os44_ufsswap_singleton(void)
{
 return &os44_get_ufs_xiaomi_for_remove()->swap.swap;
}
