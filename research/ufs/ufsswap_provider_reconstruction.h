/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_PROVIDER_H
#define OS44_PROVIDER_H
#include "ufsswap_query_reconstruction.h"
#include <linux/bug.h>
#include <linux/spinlock.h>
/* Source-owned contracts; NONE bind same-name stock/GKI symbols implicitly. */
extern void os44_core_hold(struct ufs_hba *);
extern void os44_core_release_locked(struct ufs_hba *);
extern int os44_core_issue_dev_cmd(struct ufs_hba *,struct ufshcd_lrb *,u32,int);
extern unsigned int os44_core_dev_cmd_timeout;
int os44_core_exec_dev_cmd(struct ufs_hba *,unsigned int,int);
int os44_query_attr_once(struct ufs_hba *,unsigned int,u8,u8,u8,u32 *);
int os44_query_descriptor_once(struct ufs_hba *,unsigned int,u8,u8,u8,u8 *,int *);
#endif
