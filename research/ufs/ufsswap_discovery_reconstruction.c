/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole stock ufs_get_partation_info 3378c..3391c. Reconstruction-owned
 * paired declaration; no claim of original private typedef recovery.
 * Faithful original host-reference leak on failed device lookup is retained
 * for equivalence, not approved for live deployment. */
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/xarray.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_driver.h>
#include "drivers/scsi/sd.h"
#include "ufsswap_debug_reconstruction.h"
static_assert(offsetof(struct scsi_device,sdev_gendev)+offsetof(struct device,driver_data)==0x268);
static_assert(offsetof(struct scsi_disk,disk)==0x398);
static_assert(offsetof(struct gendisk,part_tbl)==0x30);
static_assert(offsetof(struct block_device,bd_meta_info)==0xd8);
static_assert(offsetof(struct block_device,bd_start_sect)==0);
static_assert(offsetof(struct block_device,bd_nr_sectors)==8);
static_assert(offsetof(struct partition_meta_info,volname)==0x25);
void os44_debug_partition_info(struct scsi_device **out,int lun,
 const char *name,struct os44_partition_span *span)
{
 struct Scsi_Host *host;
 struct scsi_disk *sdk;
 struct block_device *part;
 unsigned long index=0;
 int retries=10;
 do {
  host=scsi_host_lookup(0);
  if (host) {
   *out=scsi_device_lookup(host,0,0,(u64)(s64)lun);
   if (*out) {scsi_host_put(host);break;}
  }
  msleep(100);
 } while (--retries);
 if (!*out || !(sdk=dev_get_drvdata(&(*out)->sdev_gendev))) {
  pr_err("os44 partition: SCSI device unavailable\n");return;
 }
 if (!sdk->disk) {pr_err("os44 partition: disk unavailable\n");return;}
 part=xa_find(&sdk->disk->part_tbl,&index,ULONG_MAX,XA_PRESENT);
 while (part) {
  if (part->bd_meta_info && !strncmp(part->bd_meta_info->volname,name,strlen(name))) {
   pr_info("os44 partition %s index=%lu\n",part->bd_meta_info->volname,index);
   /* Match original UBFX bits3..54, including high-bit boundary behavior. */
   span->start=(part->bd_start_sect>>3)&((1ULL<<52)-1);
   span->length=(part->bd_nr_sectors>>3)&((1ULL<<52)-1);
   pr_info("os44 partition %s start=%llu length=%llu\n",part->bd_meta_info->volname,span->start,span->length);
   break;
  }
  part=xa_find_after(&sdk->disk->part_tbl,&index,ULONG_MAX,XA_PRESENT);
 }
}
