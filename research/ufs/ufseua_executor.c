// SPDX-License-Identifier: GPL-2.0-only
/* Whole stock eua_scsi_execute reconstruction. Seven-argument source contract.
 * memcpy retains kernel FORTIFY field-span warning; no length truncation. */
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/err.h>
#include <linux/string.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include "ufseua_executor.h"
#include "ufsswap_debug_reconstruction.h"

/* Effective request+PDU offsets witnessed in the original complete body. */
#define CMD_OFF(member) (sizeof(struct request) + offsetof(struct scsi_cmnd, member))
static_assert(offsetof(struct scsi_device, request_queue) == 8);
static_assert(offsetof(struct request, rq_flags) == 0x1c);
static_assert(offsetof(struct request, timeout) == 0x28);
static_assert(CMD_OFF(allowed) == 0x1bc);
static_assert(CMD_OFF(cmd_len) == 0x1c4);
static_assert(CMD_OFF(cmnd) == 0x1cc);
static_assert(CMD_OFF(resid_len) == 0x218);
static_assert(CMD_OFF(sense_len) == 0x21c);
static_assert(CMD_OFF(sense_buffer) == 0x220);
static_assert(CMD_OFF(flags) == 0x228);
static_assert(CMD_OFF(result) == 0x248);
static_assert(offsetof(struct scsi_exec_args, req_flags) == 0x18);
static_assert(offsetof(struct scsi_exec_args, scmd_flags) == 0x1c);
static_assert(offsetof(struct scsi_exec_args, resid) == 0x20);
static_assert(SCSI_SENSE_BUFFERSIZE == 96);
static_assert(RQF_QUIET == 0x80);

int os44_eua_scsi_execute(struct scsi_device *sdev,
 const unsigned char *cmd, blk_opf_t opf, void *buffer, unsigned int bufflen,
 unsigned int timeout, const struct scsi_exec_args *args)
{
 static const struct scsi_exec_args default_args;
 struct request *req;
 struct scsi_cmnd *scmd;
 int ret;
 if (!args)
  args = &default_args;
 else if (WARN_ON(args->sense && args->sense_len != SCSI_SENSE_BUFFERSIZE))
  return -EINVAL;
 req = scsi_alloc_request(sdev->request_queue, opf, args->req_flags);
 if (IS_ERR(req))
  return PTR_ERR(req);
 if (bufflen) {
  ret = blk_rq_map_kern(sdev->request_queue, req, buffer, bufflen, GFP_NOIO);
  if (ret) goto out;
 }
 scmd = blk_mq_rq_to_pdu(req);
 scmd->cmd_len = COMMAND_SIZE(cmd[0]);
 if (cmd[0] == 0xc0 || cmd[0] == 0xd0)
  scmd->cmd_len = 16;
 if (cmd[0] == 0x3b || cmd[0] == 0x3c)
  scmd->cmd_len = 16;
 memcpy(scmd->cmnd, cmd, scmd->cmd_len);
 scmd->allowed = 0;
 scmd->flags |= args->scmd_flags;
 req->timeout = timeout;
 req->rq_flags |= RQF_QUIET;
 blk_execute_rq(req, true);
 /* Unsigned (resid - 1) < bufflen is the original instruction predicate. */
 if ((u32)(scmd->resid_len - 1) < bufflen)
  memset(buffer + bufflen - scmd->resid_len, 0, scmd->resid_len);
 if (args->resid)
  *args->resid = scmd->resid_len;
 if (args->sense)
  memcpy(args->sense, scmd->sense_buffer, SCSI_SENSE_BUFFERSIZE);
 if (args->sshdr)
  scsi_normalize_sense(scmd->sense_buffer, scmd->sense_len, args->sshdr);
 ret = scmd->result;
out:
 blk_mq_free_request(req);
 if (ret) os44_query_error(1, 3, -1, 0);
 return ret;
}

