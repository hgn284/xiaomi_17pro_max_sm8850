/* SPDX-License-Identifier: GPL-2.0-only */
/* Full alloc/devres functions; not yet wired into rebuilt platform owner.
 * Template remains a REAL unresolved dependency, never a fake stub. */
#include <linux/device.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
extern const struct scsi_host_template os44_ufshcd_driver_template;
static_assert(sizeof(struct ufs_hba_priv)==0x1530);
static_assert(offsetof(struct Scsi_Host, hostdata)==0x9a8);
static_assert(offsetof(struct Scsi_Host, nr_maps)==0x21c);
static_assert(offsetof(struct ufs_hba, host)==0x38);
static_assert(offsetof(struct ufs_hba, dev)==0x40);
static_assert(offsetof(struct ufs_hba, outstanding_lock)==0x78);
static_assert(offsetof(struct ufs_hba, sg_entry_size)==0xc0);
static_assert(offsetof(struct ufs_hba, dev_ref_clk_freq)==0xd0);
static_assert(offsetof(struct ufs_hba, nop_out_timeout)==0x9e8);
static_assert(offsetof(struct ufs_hba, clk_list_head)==0xa68);
static void os44_host_release(void *host)
{
	scsi_host_put(host);
}

/**
 * os44_alloc_host - allocate Host Bus Adapter (HBA)
 * @dev: pointer to device handle
 * @hba_handle: driver private handle
 *
 * Return: 0 on success, non-zero value on failure.
 *
 * NOTE: There is no corresponding ufshcd_dealloc_host() because this function
 * keeps track of its allocations using devres and deallocates everything on
 * device removal automatically.
 */
int os44_alloc_host(struct device *dev, struct ufs_hba **hba_handle)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	int err = 0;

	if (!dev) {
		dev_err(dev,
		"Invalid memory reference for dev is NULL\n");
		err = -ENODEV;
		goto out_error;
	}

	host = scsi_host_alloc(&os44_ufshcd_driver_template,
				sizeof(struct ufs_hba_priv));
	if (!host) {
		dev_err(dev, "scsi_host_alloc failed\n");
		err = -ENOMEM;
		goto out_error;
	}

	err = devm_add_action_or_reset(dev, os44_host_release,
				       host);
	if (err)
		return dev_err_probe(dev, err,
				     "failed to add ufshcd dealloc action\n");

	host->nr_maps = HCTX_TYPE_POLL + 1;
	hba = shost_priv(host);
	hba->host = host;
	hba->dev = dev;
	hba->dev_ref_clk_freq = REF_CLK_FREQ_INVAL;
	hba->nop_out_timeout = 100;
	ufshcd_set_sg_entry_size(hba, sizeof(struct ufshcd_sg_entry));
	INIT_LIST_HEAD(&hba->clk_list_head);
	spin_lock_init(&hba->outstanding_lock);

	*hba_handle = hba;

out_error:
	return err;
}
