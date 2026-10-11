/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_CLD_DRAIN_H
#define OS44_CLD_DRAIN_H
#include <linux/types.h>
struct ufs_hba;
void os44_ufshcd_scsi_block_requests(struct ufs_hba *);
void os44_ufshcd_scsi_unblock_requests(struct ufs_hba *);
int os44_ufshcd_wait_for_doorbell_clr(struct ufs_hba *, u64);
#endif
