/* SPDX-License-Identifier: GPL-2.0-only */
/* Source-equivalent completion helpers; real private completion owner remains unresolved. */
#include <linux/blk-mq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "drivers/ufs/core/ufshcd-crypto.h"
#include "os44_completion_contract.h"
extern void os44_core_release_locked(struct ufs_hba *);
static inline void os44_release_inline(struct ufs_hba *hba)
{
 guard(spinlock_irqsave)(&hba->clk_gating.lock);
 os44_core_release_locked(hba);
}
static void ufshcd_clk_scaling_update_busy(struct ufs_hba *hba)
{
	struct ufs_clk_scaling *scaling = &hba->clk_scaling;
	unsigned long flags;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_scaling.active_reqs--;
	if (!scaling->active_reqs && scaling->is_busy_started) {
		scaling->tot_busy_t += ktime_to_us(ktime_sub(ktime_get(),
					scaling->busy_start_t));
		scaling->busy_start_t = 0;
		scaling->is_busy_started = false;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static inline int ufshcd_monitor_opcode2dir(u8 opcode)
{
	if (opcode == READ_6 || opcode == READ_10 || opcode == READ_16)
		return READ;
	else if (opcode == WRITE_6 || opcode == WRITE_10 || opcode == WRITE_16)
		return WRITE;
	else
		return -EINVAL;
}

void os44_ufshcd_update_monitor(struct ufs_hba *hba, const struct ufshcd_lrb *lrbp)
{
	int dir = ufshcd_monitor_opcode2dir(*lrbp->cmd->cmnd);
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (dir >= 0 && hba->monitor.nr_queued[dir] > 0) {
		const struct request *req = scsi_cmd_to_rq(lrbp->cmd);
		struct ufs_hba_monitor *m = &hba->monitor;
		ktime_t now, inc, lat;

		now = lrbp->compl_time_stamp;
		inc = ktime_sub(now, m->busy_start_ts[dir]);
		m->total_busy[dir] = ktime_add(m->total_busy[dir], inc);
		m->nr_sec_rw[dir] += blk_rq_sectors(req);

		/* Update latencies */
		m->nr_req[dir]++;
		lat = ktime_sub(now, lrbp->issue_time_stamp);
		m->lat_sum[dir] += lat;
		if (m->lat_max[dir] < lat || !m->lat_max[dir])
			m->lat_max[dir] = lat;
		if (m->lat_min[dir] > lat || !m->lat_min[dir])
			m->lat_min[dir] = lat;

		m->nr_queued[dir]--;
		/* Push forward the busy start of monitor */
		m->busy_start_ts[dir] = now;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

void os44_ufshcd_release_scsi_cmd(struct ufs_hba *hba,
			     struct ufshcd_lrb *lrbp)
{
	struct scsi_cmnd *cmd = lrbp->cmd;

	scsi_dma_unmap(cmd);
	ufshcd_crypto_clear_prdt(hba, lrbp);
	os44_release_inline(hba);
	ufshcd_clk_scaling_update_busy(hba);
}

#define ufshcd_hex_dump(prefix_str, buf, len) do {                       \
	size_t __len = (len);                                            \
	print_hex_dump(KERN_ERR, prefix_str,                             \
		       __len > 4 ? DUMP_PREFIX_OFFSET : DUMP_PREFIX_NONE,\
		       16, 4, buf, __len, false);                        \
} while (0)

void os44_ufshcd_print_tr(struct ufs_hba *hba, int tag, bool pr_prdt)
{
	const struct ufshcd_lrb *lrbp;
	int prdt_length;

	lrbp = &hba->lrb[tag];

	dev_err(hba->dev, "UPIU[%d] - issue time %lld us\n",
			tag, div_u64(lrbp->issue_time_stamp_local_clock, 1000));
	dev_err(hba->dev, "UPIU[%d] - complete time %lld us\n",
			tag, div_u64(lrbp->compl_time_stamp_local_clock, 1000));
	dev_err(hba->dev,
		"UPIU[%d] - Transfer Request Descriptor phys@0x%llx\n",
		tag, (u64)lrbp->utrd_dma_addr);

	ufshcd_hex_dump("UPIU TRD: ", lrbp->utr_descriptor_ptr,
			sizeof(struct utp_transfer_req_desc));
	dev_err(hba->dev, "UPIU[%d] - Request UPIU phys@0x%llx\n", tag,
		(u64)lrbp->ucd_req_dma_addr);
	ufshcd_hex_dump("UPIU REQ: ", lrbp->ucd_req_ptr,
			sizeof(struct utp_upiu_req));
	dev_err(hba->dev, "UPIU[%d] - Response UPIU phys@0x%llx\n", tag,
		(u64)lrbp->ucd_rsp_dma_addr);
	ufshcd_hex_dump("UPIU RSP: ", lrbp->ucd_rsp_ptr,
			sizeof(struct utp_upiu_rsp));

	prdt_length = le16_to_cpu(
		lrbp->utr_descriptor_ptr->prd_table_length);
	if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN)
		prdt_length /= ufshcd_sg_entry_size(hba);

	dev_err(hba->dev,
		"UPIU[%d] - PRDT - %d entries  phys@0x%llx\n",
		tag, prdt_length,
		(u64)lrbp->ucd_prdt_dma_addr);

	if (pr_prdt)
		ufshcd_hex_dump("UPIU PRDT: ", lrbp->ucd_prdt_ptr,
			ufshcd_sg_entry_size(hba) * prdt_length);
}

