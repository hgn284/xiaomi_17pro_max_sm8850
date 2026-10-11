// SPDX-License-Identifier: GPL-2.0-only
/* Whole OS44 reconstruction, natural compiler metadata, real query providers. */
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <scsi/scsi_device.h>
#include "ufsswap_provider_reconstruction.h"
#include "os44_snapshot.h"

int os44_ufssnapshot_query_attr(struct ufs_hba *hba, unsigned int op, unsigned int idn, u32 *value)
{
 int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ret = os44_query_attr(hba, op, idn, 0, 0, value);
 if (ret) {
  printk("\0013[err]%s: %d:[ufssnapshot] query attr [0x%.2X] failed. opcode(%d) (%d)\n", "ufssnapshot_query_attr", 42, (u8)idn, op, ret);
  os44_query_error(1, 4, -1, 0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
int os44_ufssnapshot_read_desc(struct ufs_hba *hba, unsigned int idn, u8 *buf, int size)
{
 int ret, i;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 for (i = 0; i < 3; i++) {
  ret = os44_query_descriptor_once(hba, 1, idn, 0, 0, buf, &size);
  if (!ret || ret == -EINVAL) break;
 }
 if (ret) {
  printk("\0013[err]%s: %d:[ufssnapshot] read desc [0x%.2X] failed. (%d)\n", "ufssnapshot_read_desc", 142, (u8)idn, ret);
  os44_query_error(1, 4, -1, 0);
 }
 pm_runtime_put_sync(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
