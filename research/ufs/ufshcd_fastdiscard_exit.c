/* SPDX-License-Identifier: GPL-2.0-only */
/* Stock-equivalent teardown. OEM slot is NOT cleared by the original.
 * This is not a lifecycle repair and is not safe to invoke twice. */
#include "os44_fastdiscard_contract.h"
#include <linux/slab.h>
void os44_fastdiscard_exit(struct ufs_hba *hba)
{
 struct os44_fastdiscard_oem_view *owner = (void *)hba->android_oem_data1;
 if (owner) {
  kfree(owner->fastdiscard);
  owner->fastdiscard = NULL;
  kfree(owner);
 }
}
