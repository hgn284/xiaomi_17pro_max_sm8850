// SPDX-License-Identifier: GPL-2.0-only
/* Real original module-lifetime BSS owner (8224 bytes). See owner_closure.json.
 * Constructor: actual async_scan initializes both embedded delayed works.
 * Getter, load/save, query-error and four timing consumers share this object.
 * Original remove/exit omit private work cancellation and retain SCSI state;
 * preserved for reconstruction, NOT claimed safe for unload or multiple HBAs.
 * This is a reconstructed layout, not the original private type declaration. */
#include "ufsswap_debug_reconstruction.h"
struct os44_debug_view os44_ufs_data;
