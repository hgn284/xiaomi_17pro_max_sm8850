/* SPDX-License-Identifier: GPL-2.0-only */
/* Real template declaration reconstructed from public source and checked
 * against the complete stock object (all bytes and relocations).
 * Callbacks/groups remain REAL unresolved providers; no fake functions.
 * typeof derives each callback's exact declaration from its typed field. */
#include <linux/module.h>
#include <linux/sizes.h>
#include <linux/blkdev.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshci.h>
#define UFSHCD "ufshcd"
static_assert(sizeof(struct scsi_host_template)==0x190);
extern typeof(*((struct scsi_host_template *)0)->map_queues) os44_ufshcd_map_queues;
extern typeof(*((struct scsi_host_template *)0)->queuecommand) os44_ufshcd_queuecommand;
extern typeof(*((struct scsi_host_template *)0)->mq_poll) os44_ufshcd_poll;
extern typeof(*((struct scsi_host_template *)0)->slave_alloc) os44_ufshcd_slave_alloc;
extern typeof(*((struct scsi_host_template *)0)->device_configure) os44_ufshcd_device_configure;
extern typeof(*((struct scsi_host_template *)0)->slave_destroy) os44_ufshcd_slave_destroy;
extern typeof(*((struct scsi_host_template *)0)->change_queue_depth) os44_ufshcd_change_queue_depth;
extern typeof(*((struct scsi_host_template *)0)->eh_abort_handler) os44_ufshcd_abort;
extern typeof(*((struct scsi_host_template *)0)->eh_device_reset_handler) os44_ufshcd_eh_device_reset_handler;
extern typeof(*((struct scsi_host_template *)0)->eh_host_reset_handler) os44_ufshcd_eh_host_reset_handler;
extern typeof(*((struct scsi_host_template *)0)->eh_timed_out) os44_ufshcd_eh_timed_out;
extern const struct attribute_group *os44_ufshcd_driver_groups[];
const struct scsi_host_template os44_ufshcd_driver_template = {
	.module			= THIS_MODULE,
	.name			= UFSHCD,
	.proc_name		= UFSHCD,
	.map_queues		= os44_ufshcd_map_queues,
	.queuecommand		= os44_ufshcd_queuecommand,
	.mq_poll		= os44_ufshcd_poll,
	.slave_alloc		= os44_ufshcd_slave_alloc,
	.device_configure	= os44_ufshcd_device_configure,
	.slave_destroy		= os44_ufshcd_slave_destroy,
	.change_queue_depth	= os44_ufshcd_change_queue_depth,
	.eh_abort_handler	= os44_ufshcd_abort,
	.eh_device_reset_handler = os44_ufshcd_eh_device_reset_handler,
	.eh_host_reset_handler   = os44_ufshcd_eh_host_reset_handler,
	.eh_timed_out		= os44_ufshcd_eh_timed_out,
	.this_id		= -1,
	.sg_tablesize		= SG_ALL,
	.max_segment_size	= PRDT_DATA_BYTE_COUNT_MAX,
	.max_sectors		= SZ_1M / SECTOR_SIZE,
	.max_host_blocked	= 1,
	.track_queue_depth	= 1,
	.skip_settle_delay	= 1,
	.sdev_groups		= os44_ufshcd_driver_groups,
};
