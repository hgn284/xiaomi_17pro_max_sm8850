// SPDX-License-Identifier: GPL-2.0-only
/* Actual accessor and adapters from stock get_ufs_data and debug work funcs.
 * os44_ufs_data remains undefined until the real owner lifecycle is restored. */
#include "ufsswap_debug_reconstruction.h"
#include "os44_async_scan.h"
struct os44_debug_view *os44_debug_owner(void) { return &os44_ufs_data; }
void os44_debug_load_work(struct work_struct *work) { os44_debug_load(); }
void os44_debug_save_work(struct work_struct *work) { os44_debug_save(); }
