/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_COMMAND_TRACE_CONTRACT_H
#define OS44_COMMAND_TRACE_CONTRACT_H
#include <ufs/ufshcd.h>
struct request;
struct ufs_hw_queue *os44_ufshcd_mcq_req_to_hwq(struct ufs_hba *hba,
                                              struct request *req);
void os44_ufshcd_add_command_trace(struct ufs_hba *hba, unsigned int tag,
                                  enum ufs_trace_str_t str_t);
#endif
