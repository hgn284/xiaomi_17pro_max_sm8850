// SPDX-License-Identifier: GPL-2.0-only
/* Actual WLUN registration owner reconstructed from stock .bss+0x37e1,
 * size 1, initially false. Not a stand-in for an unimplemented provider.
 * The only original accessors are whole ufshcd_init and ufshcd_hba_exit.
 * Init registers the genuine shared scsi driver and publishes true only
 * after successful __scsi_register_driver. Exit unregisters that same driver
 * and clears this flag while is_powered. See evidence/registration_owner.json.
 * Preserve the original pre-powered failure retention; this does NOT repair
 * multi-HBA concurrency, registration leaks, or any other stock lifetime risk.
 * Two unrelated larger Xiaomi/debug storage owners remain unresolved.
 */
#include <linux/types.h>
#include <linux/build_bug.h>
bool os44_ufs_dev_wlun_registered;
static_assert(sizeof(os44_ufs_dev_wlun_registered) == 1);
