/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FBO_QUERY_H
#define OS44_FBO_QUERY_H
#include "ufsswap_provider_reconstruction.h"
#include "os44_bkops_contract.h"
/* Reconstructed source-pair contracts; original private enum/type identity unknown. */
int os44_ufsfbo_read_desc(struct ufs_hba *,unsigned int,u8 *);
int os44_ufsfbo_query_flag(struct ufs_hba *,unsigned int,unsigned int,bool *);
#endif
