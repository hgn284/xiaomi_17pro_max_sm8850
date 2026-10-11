/* SPDX-License-Identifier: GPL-2.0-only */
/* Public-source equivalent candidate, validated against complete stock functions. */
#include <linux/blk-mq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include <linux/moduleparam.h>
static bool use_mcq_mode=true;
module_param(use_mcq_mode,bool,0644);
static bool is_mcq_supported(struct ufs_hba *hba) {return hba->mcq_sup && use_mcq_mode;}
int os44_ufshcd_change_queue_depth(struct scsi_device *sdev, int depth)
{
	return scsi_change_queue_depth(sdev, min(depth, sdev->host->can_queue));
}
void os44_ufshcd_map_queues(struct Scsi_Host *shost)
{
	struct ufs_hba *hba = shost_priv(shost);
	int i, queue_offset = 0;

	if (!is_mcq_supported(hba)) {
		hba->nr_queues[HCTX_TYPE_DEFAULT] = 1;
		hba->nr_queues[HCTX_TYPE_READ] = 0;
		hba->nr_queues[HCTX_TYPE_POLL] = 1;
		hba->nr_hw_queues = 1;
	}

	for (i = 0; i < shost->nr_maps; i++) {
		struct blk_mq_queue_map *map = &shost->tag_set.map[i];

		map->nr_queues = hba->nr_queues[i];
		if (!map->nr_queues)
			continue;
		map->queue_offset = queue_offset;
		if (i == HCTX_TYPE_POLL && !is_mcq_supported(hba))
			map->queue_offset = 0;

		blk_mq_map_queues(map);
		queue_offset += map->nr_queues;
	}
}
