/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_FASTDISCARD_CONTRACT_H
#define OS44_FASTDISCARD_CONTRACT_H
#include <ufs/ufshcd.h>
/* Reconstruction-owned views, not original private C type names.
 * Stock fastdiscard_init allocates 16 bytes; policy writes u32 at offset0.
 * OEM owner first field is that allocation. Remaining OEM layout unknown.
 * No owner allocation or synthetic standalone global is introduced here. */
struct os44_fastdiscard_state_view { u32 enabled; unsigned char unknown[12]; };
struct os44_fastdiscard_oem_view { struct os44_fastdiscard_state_view *fastdiscard; };
static_assert(sizeof(struct os44_fastdiscard_state_view)==16);
static_assert(offsetof(struct ufs_hba,android_oem_data1)==0x14f0);
static_assert(offsetof(struct ufs_hba,ufs_device_wlun)==0x48);
static_assert(offsetof(struct scsi_device,inquiry)==0xf0);
int os44_check_fastdiscard_policy(struct ufs_hba *);
void os44_fastdiscard_exit(struct ufs_hba *);
#endif
