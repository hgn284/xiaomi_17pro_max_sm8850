// SPDX-License-Identifier: GPL-2.0-only
/* Whole OS44 reconstruction, natural compiler metadata, real query providers. */
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <scsi/scsi_device.h>
#include "ufsswap_provider_reconstruction.h"
#include "os44_snapshot.h"
#include "os44_snapshot_v2.h"

int os44_ufssnapshotV2_query_attr(struct ufs_hba *hba, unsigned int op, unsigned int idn, u32 *value)
{
 int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ret = os44_query_attr(hba, op, idn, 0, 0, value);
 if (ret) {
  printk("\0013[err]%s: %d:[ufssnapshot] query attr [0x%.2X] failed. opcode(%d) (%d)\n", "ufssnapshot_query_attr", 50, (u8)idn, op, ret);
  os44_query_error(1, 4, -1, 0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}

int os44_ufssnapshot_stop_V2(struct os44_snapshot_v2_view *s)
{
 u32 value=3;
 struct ufs_hba *hba=s->hba;
 int ret=os44_ufssnapshotV2_rw_buffer(s,s->vendor==1 ? 0x1e : 0x1d,3,s->buffer_id,0,NULL,false);
 if(ret) {
  printk("\0013[err]%s: %d:[ufssnapshotV2] write buffer failed, ret %d\n\n","ufssnapshot_stop_V2",324,ret);
  return ret;
 }
 if(s->vendor!=3)return 0;
 ret=os44_ufssnapshotV2_query_attr(hba,4,0x9f,&value);
 if(ret)printk("\0013[err]%s: %d:[ufssnapshotV2] write ctrl attr failed, err %d\n","ufssnapshot_stop_V2",331,ret);
 return ret;
}
