/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_WB_CONTRACT_H
#define OS44_WB_CONTRACT_H
#include <ufs/ufshcd.h>
int os44_ufshcd_wb_toggle(struct ufs_hba *, bool);
void os44_ufshcd_wb_toggle_buf_flush_during_h8(struct ufs_hba *, bool);
int os44_ufshcd_wb_toggle_buf_flush(struct ufs_hba *, bool);
bool os44_ufshcd_wb_need_flush(struct ufs_hba *);
#endif
