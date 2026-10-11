// SPDX-License-Identifier: GPL-2.0-only
/* Exact stock getter contract: return the shared owner address.
 * Storage remains external and unresolved until its real lifetime is restored. */
#include "os44_xiaomi_owner.h"
struct os44_remove_owner_view *os44_get_ufs_xiaomi_for_remove(void)
{
 return &os44_ufs_xiaomi;
}
