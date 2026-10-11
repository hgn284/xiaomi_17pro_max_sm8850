/* SPDX-License-Identifier: GPL-2.0-only */
/* Original public implementations: actual queue teardown and crypto profile registration.
 * Not crypto capability initialization, not hardware crypto verification. */
#include <linux/bsg-lib.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufs_bsg.h"
#include "drivers/ufs/core/ufshcd-crypto.h"
void ufs_bsg_remove(struct ufs_hba *hba)
{
	struct device *bsg_dev = &hba->bsg_dev;

	if (!hba->bsg_queue)
		return;

	bsg_remove_queue(hba->bsg_queue);
	hba->bsg_queue = NULL;

	device_del(bsg_dev);
	put_device(bsg_dev);
}

void ufshcd_crypto_register(struct ufs_hba *hba, struct request_queue *q)
{
	if (hba->caps & UFSHCD_CAP_CRYPTO)
		blk_crypto_register(&hba->crypto_profile, q);
}
