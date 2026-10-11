/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef OS44_DEVICE_IDENTITY_H
#define OS44_DEVICE_IDENTITY_H
#include <ufs/ufshcd.h>
#include <ufs/ufs_quirks.h>
int os44_ufshcd_read_string_desc(struct ufs_hba *, u8, u8 **, bool);
void os44_ufshcd_fixup_dev_quirks(struct ufs_hba *, const struct ufs_dev_quirk *);
extern const struct ufs_dev_quirk os44_ufs_fixups[8];
#endif
