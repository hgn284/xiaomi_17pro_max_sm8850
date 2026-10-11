/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_BSG_CONTRACT_H
#define OS44_BSG_CONTRACT_H
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufs_bsg.h"
int os44_ufshcd_send_bsg_uic_cmd(struct ufs_hba *, struct uic_command *);
int os44_ufshcd_advanced_rpmb_req_handler(struct ufs_hba *, struct utp_upiu_req *, struct utp_upiu_req *, struct ufs_ehs *, struct ufs_ehs *, int, struct scatterlist *, enum dma_data_direction);
int os44_ufshcd_exec_raw_upiu_cmd(struct ufs_hba *, struct utp_upiu_req *, struct utp_upiu_req *, enum upiu_request_transaction, u8 *, int *, enum query_opcode);
int os44_ufs_bsg_probe(struct ufs_hba *);
int os44_ufshcd_exec_raw_upiu_cmd(struct ufs_hba *, struct utp_upiu_req *, struct utp_upiu_req *, enum upiu_request_transaction, u8 *, int *, enum query_opcode);
int os44_ufs_bsg_probe(struct ufs_hba *);
#endif
