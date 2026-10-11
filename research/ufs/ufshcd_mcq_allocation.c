// SPDX-License-Identifier: GPL-2.0-only
/* Qualcomm public scaffold, OS44 whole-function reconstruction.
 * Real queue parameter owners and coherent/devm allocation, not fake globals.
 * This preserves stock semantics, including DMA-address failure checks. */
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/bitfield.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_mcq_allocation.h"
#define MAX_QUEUE_SUP GENMASK(7, 0)
#define UFS_MCQ_MIN_RW_QUEUES 2
#define UFS_MCQ_MIN_READ_QUEUES 0
#define UFS_MCQ_MIN_POLL_QUEUES 0
static_assert(sizeof(struct ufs_hw_queue)==0x90);
static_assert(sizeof(struct ufshcd_lrb)==0x98);
static_assert(offsetof(struct ufs_hba,uhq)==0x13d0);
static_assert(offsetof(struct ufs_hba,nutrs)==0x8c);
static_assert(offsetof(struct ufs_hba_variant_ops,get_hba_mac)==0xd0);
static_assert(offsetof(struct ufs_hba_variant_ops,mcq_config_resource)==0xc8);
static_assert(offsetof(struct ufs_hba_variant_ops,op_runtime_config)==0xd8);


static int rw_queue_count_set(const char *val, const struct kernel_param *kp)
{
	return param_set_uint_minmax(val, kp, UFS_MCQ_MIN_RW_QUEUES,
				     num_possible_cpus());
}

static const struct kernel_param_ops rw_queue_count_ops = {
	.set = rw_queue_count_set,
	.get = param_get_uint,
};

static unsigned int rw_queues;
module_param_cb(rw_queues, &rw_queue_count_ops, &rw_queues, 0644);
MODULE_PARM_DESC(rw_queues,
		 "Number of interrupt driven I/O queues used for rw. Default value is nr_cpus");

static int read_queue_count_set(const char *val, const struct kernel_param *kp)
{
	return param_set_uint_minmax(val, kp, UFS_MCQ_MIN_READ_QUEUES,
				     num_possible_cpus());
}

static const struct kernel_param_ops read_queue_count_ops = {
	.set = read_queue_count_set,
	.get = param_get_uint,
};

static unsigned int read_queues;
module_param_cb(read_queues, &read_queue_count_ops, &read_queues, 0644);
MODULE_PARM_DESC(read_queues,
		 "Number of interrupt driven read queues used for read. Default value is 0");

static int poll_queue_count_set(const char *val, const struct kernel_param *kp)
{
	return param_set_uint_minmax(val, kp, UFS_MCQ_MIN_POLL_QUEUES,
				     num_possible_cpus());
}

static const struct kernel_param_ops poll_queue_count_ops = {
	.set = poll_queue_count_set,
	.get = param_get_uint,
};

static unsigned int poll_queues = 1;
module_param_cb(poll_queues, &poll_queue_count_ops, &poll_queues, 0644);
MODULE_PARM_DESC(poll_queues,
		 "Number of poll queues used for r/w. Default value is 1");



int os44_ufshcd_mcq_decide_queue_depth(struct ufs_hba *hba)
{
	int mac;

	if (!hba->vops || !hba->vops->get_hba_mac) {
		/*
		 * Extract the maximum number of active transfer tasks value
		 * from the host controller capabilities register. This value is
		 * 0-based.
		 */
		hba->capabilities =
			ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES);
		mac = hba->capabilities & MASK_TRANSFER_REQUESTS_SLOTS_MCQ;
		mac++;
	} else {
		mac = hba->vops->get_hba_mac(hba);
	}
	if (mac < 0)
		goto err;

	WARN_ON_ONCE(!hba->dev_info.bqueuedepth);
	/*
	 * max. value of bqueuedepth = 256, mac is host dependent.
	 * It is mandatory for UFS device to define bQueueDepth if
	 * shared queuing architecture is enabled.
	 */
	return min_t(int, mac, hba->dev_info.bqueuedepth);

err:
	dev_err(hba->dev, "Failed to get mac, err=%d\n", mac);
	return mac;
}

static int ufshcd_mcq_config_nr_queues(struct ufs_hba *hba)
{
	int i;
	u32 hba_maxq, rem, tot_queues;
	struct Scsi_Host *host = hba->host;

	/* maxq is 0 based value */
	hba_maxq = FIELD_GET(MAX_QUEUE_SUP, hba->mcq_capabilities) + 1;

	tot_queues = read_queues + poll_queues + rw_queues;

	if (hba_maxq < tot_queues) {
		dev_err(hba->dev, "Total queues (%d) exceeds HC capacity (%d)\n",
			tot_queues, hba_maxq);
		return -EOPNOTSUPP;
	}

	/*
	 * Device should support at least one I/O queue to handle device
	 * commands via hba->dev_cmd_queue.
	 */
	if (hba_maxq == poll_queues) {
		dev_err(hba->dev, "At least one non-poll queue required\n");
		return -EOPNOTSUPP;
	}

	rem = hba_maxq;

	if (rw_queues) {
		hba->nr_queues[HCTX_TYPE_DEFAULT] = rw_queues;
		rem -= hba->nr_queues[HCTX_TYPE_DEFAULT];
	} else {
		rw_queues = num_possible_cpus();
	}

	if (poll_queues) {
		hba->nr_queues[HCTX_TYPE_POLL] = poll_queues;
		rem -= hba->nr_queues[HCTX_TYPE_POLL];
	}

	if (read_queues) {
		hba->nr_queues[HCTX_TYPE_READ] = read_queues;
		rem -= hba->nr_queues[HCTX_TYPE_READ];
	}

	if (!hba->nr_queues[HCTX_TYPE_DEFAULT])
		hba->nr_queues[HCTX_TYPE_DEFAULT] = min3(rem, rw_queues,
							 num_possible_cpus());

	for (i = 0; i < HCTX_MAX_TYPES; i++)
		host->nr_hw_queues += hba->nr_queues[i];

	hba->nr_hw_queues = host->nr_hw_queues;
	return 0;
}

int os44_ufshcd_mcq_init(struct ufs_hba *hba)
{
	struct Scsi_Host *host = hba->host;
	struct ufs_hw_queue *hwq;
	int ret, i;

	ret = ufshcd_mcq_config_nr_queues(hba);
	if (ret)
		return ret;

	ret = ufshcd_vops_mcq_config_resource(hba);
	if (ret)
		return ret;

	ret = ufshcd_mcq_vops_op_runtime_config(hba);
	if (ret) {
		dev_err(hba->dev, "Operation runtime config failed, ret=%d\n",
			ret);
		return ret;
	}
	hba->uhq = devm_kzalloc(hba->dev,
				hba->nr_hw_queues * sizeof(struct ufs_hw_queue),
				GFP_KERNEL);
	if (!hba->uhq) {
		dev_err(hba->dev, "ufs hw queue memory allocation failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < hba->nr_hw_queues; i++) {
		hwq = &hba->uhq[i];
		hwq->max_entries = hba->nutrs + 1;
		spin_lock_init(&hwq->sq_lock);
		spin_lock_init(&hwq->cq_lock);
		mutex_init(&hwq->sq_mutex);
	}

	/* The very first HW queue serves device commands */
	hba->dev_cmd_queue = &hba->uhq[0];

	host->host_tagset = 1;
	return 0;
}

int os44_ufshcd_mcq_memory_alloc(struct ufs_hba *hba)
{
	struct ufs_hw_queue *hwq;
	size_t utrdl_size, cqe_size;
	int i;

	for (i = 0; i < hba->nr_hw_queues; i++) {
		hwq = &hba->uhq[i];

		utrdl_size = sizeof(struct utp_transfer_req_desc) *
			     hwq->max_entries;
		hwq->sqe_base_addr = dmam_alloc_coherent(hba->dev, utrdl_size,
							 &hwq->sqe_dma_addr,
							 GFP_KERNEL);
		if (!hwq->sqe_dma_addr) {
			dev_err(hba->dev, "SQE allocation failed\n");
			return -ENOMEM;
		}

		cqe_size = sizeof(struct cq_entry) * hwq->max_entries;
		hwq->cqe_base_addr = dmam_alloc_coherent(hba->dev, cqe_size,
							 &hwq->cqe_dma_addr,
							 GFP_KERNEL);
		if (!hwq->cqe_dma_addr) {
			dev_err(hba->dev, "CQE allocation failed\n");
			return -ENOMEM;
		}
	}

	return 0;
}
