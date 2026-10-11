/* SPDX-License-Identifier: GPL-2.0-only */
/* Original-equivalent key reprogramming and restricted vendor hook. */
#include <ufs/ufshcd.h>
#include "os44_hba_enable_contract.h"
#undef CREATE_TRACE_POINTS
#include <trace/hooks/ufshcd.h>
bool os44_ufshcd_crypto_enable(struct ufs_hba *hba)
{
	int err = -EOPNOTSUPP;

	if (!(hba->caps & UFSHCD_CAP_CRYPTO))
		return false;

	/* Reset might clear all keys, so reprogram all the keys. */
	trace_android_rvh_ufs_reprogram_all_keys(hba, &err);
	if (err == -EOPNOTSUPP)
		blk_crypto_reprogram_all_keys(&hba->crypto_profile);

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_CRYPTO_ENABLE)
		return false;

	return true;
}
