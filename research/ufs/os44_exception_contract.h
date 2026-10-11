/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_EXCEPTION_CONTRACT_H
#define OS44_EXCEPTION_CONTRACT_H
#include <ufs/ufshcd.h>
void os44_ufshcd_exception_event_handler(struct work_struct *);
void os44_ufssnapshot_inc_eecount(struct ufs_hba *);
void os44_ufs_debugfs_exception_event(struct ufs_hba *, u16);
#endif
