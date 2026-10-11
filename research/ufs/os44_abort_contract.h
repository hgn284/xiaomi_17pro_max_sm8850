/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_ABORT_CONTRACT_H
#define OS44_ABORT_CONTRACT_H
#include <ufs/ufshcd.h>
int os44_ufshcd_try_to_abort_task(struct ufs_hba *, int);
#endif
