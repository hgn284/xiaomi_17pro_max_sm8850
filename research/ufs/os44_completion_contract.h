/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_COMPLETION_CONTRACT_H
#define OS44_COMPLETION_CONTRACT_H
#include <ufs/ufshcd.h>
void os44_ufshcd_update_monitor(struct ufs_hba *, const struct ufshcd_lrb *);
void os44_ufshcd_release_scsi_cmd(struct ufs_hba *, struct ufshcd_lrb *);
void os44_ufshcd_print_tr(struct ufs_hba *, int, bool);
#endif
