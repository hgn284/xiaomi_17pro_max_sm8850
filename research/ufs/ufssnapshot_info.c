// SPDX-License-Identifier: GPL-2.0-only
/* Whole OS44 reconstruction, natural compiler metadata, real query providers. */
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <scsi/scsi_device.h>
#include "ufsswap_query_reconstruction.h"
#include "os44_snapshot.h"

#include <linux/unaligned.h>
struct os44_snapshot_params {
 u8 vendor, version_idn, version_offset, version_size;
 u8 support_idn, support_offset, support_size, support_bit;
};
static const struct os44_snapshot_params params_vendor0 = {0,0xfa,2,2,0,0x4f,4,31};
static const struct os44_snapshot_params params_vendor2 = {2,0,0xff,2,0,0x4f,4,31};
static const struct os44_snapshot_params params_vendor3 = {3,0xf0,0xe1,2,0xf0,0xfb,4,9};
static const struct os44_snapshot_params params_vendor4 = {4,0,0x4b,2,0,0x4f,4,31};
int os44_ufssnapshot_get_info(struct os44_snapshot_view *snap, u32 *supported, u16 *version)
{
 struct ufs_hba *hba = snap->hba;
 const struct os44_snapshot_params *p;
 u8 (*buf)[255];
 int ret;
 buf = kmalloc(255, GFP_KERNEL);
 if (!buf) { ret = -ENOMEM; goto out; }
 switch (snap->vendor) {
 case 0: p=&params_vendor0; break;
 case 2: p=&params_vendor2; break;
 case 3: p=&params_vendor3; break;
 case 4: p=&params_vendor4; break;
 default: ret=-1; goto out;
 }
 ret = os44_ufssnapshot_read_desc(hba, p->version_idn, *buf, 255);
 if (ret) {
  printk("\0013[err]%s: %d:[ufssnapshot] Failed read device desc, idn %u, error %d.\n", "ufssnapshot_get_info", 903, p->version_idn, ret);
  goto out;
 }
 if (version) {
  if (p->version_size == 2) *version = get_unaligned_be16(&(*buf)[p->version_offset]);
  else if (p->version_size == 4) *version = get_unaligned_be32(&(*buf)[p->version_offset]);
 }
 if (p->support_idn != p->version_idn) {
  ret = os44_ufssnapshot_read_desc(hba, p->support_idn, *buf, 255);
  if (ret) {
   printk("\0013[err]%s: %d:[ufssnapshot] Failed read device desc, idn %u, error %d.\n", "ufssnapshot_get_info", 915, p->support_idn, ret);
   goto out;
  }
 }
 if (supported) *supported = (get_unaligned_be32(&(*buf)[p->support_offset]) >> p->support_bit) & 1;
 ret=0;
out:
 kfree(buf);
 return ret;
}
