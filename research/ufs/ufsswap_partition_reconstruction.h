/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_PARTITION_H
#define OS44_PARTITION_H
#include <scsi/scsi_device.h>
/* Reconstruction-owned paired contract; NOT an alias to GKI's executor.
 * Implementation remains a real unresolved dependency until reconstructed. */
int os44_mi_scsi_execute_cmd(struct scsi_device *, const unsigned char *,
 blk_opf_t, void *, unsigned int, int, int, const struct scsi_exec_args *);
#endif
