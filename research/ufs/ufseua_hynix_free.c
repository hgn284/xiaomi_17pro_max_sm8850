// SPDX-License-Identifier: GPL-2.0-only
#include <linux/slab.h>
#include <scsi/scsi_host.h>
#include "os44_eua.h"
#include <scsi/scsi_eh.h>
#include <linux/blk-mq.h>
#include "ufseua_executor.h"

/* Distinct EUA static hr_lock: stock .data section9+0x6d60; snapshot has its OWN lock at0x7d68. */
static DEFINE_MUTEX(hr_lock);
int os44_hynix_eua_get_free_space(struct os44_eua_view *s, u32 *value)
{
 struct ufs_hba *hba = s->hba;
 struct scsi_device *sdev = s->sdev;
 unsigned long flags;
 u8 *buf;
 int ret;
 buf = kzalloc(512, GFP_KERNEL);
 if (!buf) {
  printk("\0013[UFSEUA]kzalloc fail\n");
  return -ENOMEM;
 }
 mutex_lock(&hr_lock);
 spin_lock_irqsave(hba->host->host_lock, flags);
 ret = scsi_device_get(sdev);
 if (!ret && !scsi_device_online(sdev)) {
  scsi_device_put(sdev);
  printk("\0016[UFSEUA]get device fail\n");
  ret = -ENODEV;
 }
 spin_unlock_irqrestore(hba->host->host_lock, flags);
 if (ret) goto unlock;
 hba->host->eh_noresume = 1;
 ret = os44_ufseua_read_desc(hba, 9, buf, 37);
 if (!ret) {
  u8 cdb[16] = {0xd0, 0x0f, 0x53, [11] = 0x6f};
  struct scsi_sense_hdr sense = {0};
  const struct scsi_exec_args args = {.sshdr = &sense, .req_flags = BLK_MQ_REQ_PM};
  ret = os44_eua_scsi_execute(sdev, cdb, REQ_OP_DRV_IN, buf+128, 111, 7500, &args);
  if (ret) {
   printk("\0013[UFSEUA]get skhynix ret error 0x%x\n", ret);
   ret = -EIO;
  } else *value = buf[238];
 }
 scsi_device_put(sdev);
 hba->host->eh_noresume = 0;
unlock:
 mutex_unlock(&hr_lock);
 kfree(buf);
 return ret;
}
