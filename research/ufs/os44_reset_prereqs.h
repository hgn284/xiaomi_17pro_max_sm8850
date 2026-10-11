/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_RESET_PREREQS_H
#define OS44_RESET_PREREQS_H
#include <linux/blk-mq.h>
#include <linux/interrupt.h>
#include <ufs/ufshcd.h>
bool os44_ufshcd_mcq_force_compl_one(struct request *, void *);
irqreturn_t os44_ufshcd_tmc_handler(struct ufs_hba *);
void os44_ufshcd_mcq_compl_all_cqes_lock(struct ufs_hba *, struct ufs_hw_queue *);
int os44_ufshcd_scale_clks(struct ufs_hba *, unsigned long, bool);
int os44_ufshcd_host_reset_and_restore(struct ufs_hba *);
#endif
