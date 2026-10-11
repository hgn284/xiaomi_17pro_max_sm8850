/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_MCQ_ALLOCATION_H
#define OS44_MCQ_ALLOCATION_H
#include <ufs/ufshcd.h>
int os44_ufshcd_mcq_decide_queue_depth(struct ufs_hba *);
int os44_ufshcd_mcq_init(struct ufs_hba *);
int os44_ufshcd_mcq_memory_alloc(struct ufs_hba *);
int os44_ufshcd_memory_alloc(struct ufs_hba *);
int os44_ufshcd_alloc_mcq(struct ufs_hba *);
#endif
