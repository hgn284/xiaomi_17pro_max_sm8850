/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_INIT_WORK_H
#define OS44_INIT_WORK_H
#include <ufs/ufshcd.h>
void os44_ufshcd_clk_scaling_suspend_work(struct work_struct *);
void os44_ufshcd_clk_scaling_resume_work(struct work_struct *);
void os44_ufshcd_rpm_dev_flush_recheck_work(struct work_struct *);
void os44_ufshcd_rtc_work(struct work_struct *);
#include <linux/blk-mq.h>
blk_status_t os44_ufshcd_queue_tmf(struct blk_mq_hw_ctx *, const struct blk_mq_queue_data *);
extern const struct blk_mq_ops os44_ufshcd_tmf_ops;
#endif
