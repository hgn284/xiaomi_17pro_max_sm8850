/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_EUA_EXECUTOR_H
#define OS44_EUA_EXECUTOR_H
#include <linux/blk_types.h>
#include <scsi/scsi_device.h>
int os44_eua_scsi_execute(struct scsi_device *, const unsigned char *, blk_opf_t, void *, unsigned int, unsigned int, const struct scsi_exec_args *);
#endif
