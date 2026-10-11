// SPDX-License-Identifier: GPL-2.0-only
#ifndef OS44_ASYNC_SCAN_H
#define OS44_ASYNC_SCAN_H
#include <linux/async.h>
#include <linux/workqueue.h>
void os44_ufshcd_async_scan(void *, async_cookie_t);
void os44_debug_load_work(struct work_struct *);
void os44_debug_save_work(struct work_struct *);
#endif
