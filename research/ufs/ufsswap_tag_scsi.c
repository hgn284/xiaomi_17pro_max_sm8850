/* Typed Linux adapter. No casts to invented scsi layout and no UFS registration. */
#include <linux/blk-mq.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include "ufsswap_tag_reconstruction.h"
static_assert(sizeof(struct request)==0x118);
static_assert(offsetof(struct request,__sector)==0x30);
static_assert(offsetof(struct scsi_cmnd,device)==0);
static_assert(offsetof(struct scsi_cmnd,cmnd)==0xb4);
static_assert(offsetof(struct scsi_device,sector_size)==0xa4);
void os44_ufsswap_tag_scsi(struct os44_swap_view *s, struct scsi_cmnd *cmd)
{
    os44_ufsswap_tag(s,blk_rq_pos(scsi_cmd_to_rq(cmd)),
                   cmd->device->sector_size,cmd->cmnd);
}
