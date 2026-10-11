/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_DEVICE_INIT_H
#define OS44_DEVICE_INIT_H
#include <ufs/ufshcd.h>
extern bool os44_use_mcq_mode;
int os44_ufshcd_device_init(struct ufs_hba *, bool);
#endif
