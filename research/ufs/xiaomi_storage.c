// SPDX-License-Identifier: GPL-2.0-only
/* Real original 5976-byte module-lifetime NOBITS owner, not a stand-in.
 * ufshcd_init publishes queue_hba; actual async/probe constructors initialize
 * embedded CLD/swap/snapshot/EUA/FBO/monitor state. See owner_closure.json.
 * Original singleton is not cleared on remove: multi-HBA/reprobe and embedded
 * FBO kfree/reference/work lifetime defects are preserved, not deemed safe.
 * Shared layout is reconstruction-owned, NOT the original private C type. */
#include "os44_xiaomi_owner.h"
struct os44_remove_owner_view os44_ufs_xiaomi;
