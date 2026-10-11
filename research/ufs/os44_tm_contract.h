/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_TM_CONTRACT_H
#define OS44_TM_CONTRACT_H
#include <ufs/ufshcd.h>
void os44_ufshcd_add_tm_upiu_trace(struct ufs_hba *, unsigned int, enum ufs_trace_str_t);
int os44_ufshcd_clear_tm_cmd(struct ufs_hba *, int);
int os44___ufshcd_issue_tm_cmd(struct ufs_hba *, struct utp_task_req_desc *, u8);
#endif
