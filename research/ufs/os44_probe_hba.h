/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_PROBE_HBA_H
#define OS44_PROBE_HBA_H
#include <ufs/ufshcd.h>
int os44_ufshcd_probe_hba(struct ufs_hba *, bool);
#endif
