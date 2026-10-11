/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_INIT_CONTROLS_H
#define OS44_INIT_CONTROLS_H
#include <ufs/ufshcd.h>
void os44_ufshcd_config_mcq(struct ufs_hba *);
void os44_ufshcd_mcq_disable(struct ufs_hba *);
void os44_ufshcd_tune_unipro_params(struct ufs_hba *);
void os44_ufshcd_mcq_config_mac(struct ufs_hba *, u32);
void os44_ufshcd_mcq_make_queues_operational(struct ufs_hba *);
#endif
