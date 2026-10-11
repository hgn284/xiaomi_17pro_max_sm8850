#ifndef OS44_UFSSWAP_TAG_H
#define OS44_UFSSWAP_TAG_H
#include "ufsswap_range_reconstruction.h"
/* Paired-source declaration only; not the original private UFS callback ABI. */
void os44_ufsswap_tag(struct os44_swap_view *, os44_u64 request_sector512,
                      os44_u32 device_sector_bytes, unsigned char *cdb);
#ifdef __KERNEL__
struct scsi_cmnd;
void os44_ufsswap_tag_scsi(struct os44_swap_view *, struct scsi_cmnd *);
#endif
#endif
