/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_ERR_HANDLER_H
#define OS44_ERR_HANDLER_H
#include <linux/blk-mq.h>
#include <linux/workqueue.h>
void os44_ufshcd_err_handler(struct work_struct *);
bool os44_ufshcd_abort_one(struct request *, void *);
bool os44_ufshcd_print_tr_iter(struct request *, void *);
bool os44_ufshcd_mcq_compl_one(struct request *, void *);
#endif
