// SPDX-License-Identifier: GPL-2.0-only
#include "os44_xiaomi_owner.h"
#include <scsi/scsi_eh.h>
#include <linux/blk-mq.h>
#include "ufssnapshot_executor.h"
#include "ufssnapshot_free.h"
int os44_micron_get_free_space(struct os44_snapshot_view *s, u32 *value)
{
 u8 write_cdb[16] = {0x3b, 0xe1, [8] = 0x20};
 u8 read_cdb[16] = {0x3c, 0xc1, [8] = 0x2c};
 u8 request[44] = {0xa8, 0x40, 0x40, [12] = 1};
 u8 response[44] = {0};
 struct scsi_device *sdev = os44_get_ufs_xiaomi_for_remove()->sdev_8b8;
 struct scsi_sense_hdr sense;
 const struct scsi_exec_args args = {.sshdr = &sense, .req_flags = BLK_MQ_REQ_PM};
 int ret;
 if (!value) return -EINVAL;
 ret = os44_ufssnapshot_scsi_execute(sdev, write_cdb, REQ_OP_DRV_OUT, request, 44, &args);
 if (ret) {
  printk("\0013[err]%s: %d:[ufssnapshot] write buffer  error 0x%x\n\n", "micron_get_free_space", 275, ret);
  return -EIO;
 }
 ret = os44_ufssnapshot_scsi_execute(sdev, read_cdb, REQ_OP_DRV_IN, response, 44, &args);
 if (ret) {
  printk("\0013[err]%s: %d:[ufssnapshot] read buffer  error 0x%x\n\n", "micron_get_free_space", 282, ret);
  return -EIO;
 }
 *value = response[0] | (response[1] << 8) | (response[2] << 16);
 return 0;
}
