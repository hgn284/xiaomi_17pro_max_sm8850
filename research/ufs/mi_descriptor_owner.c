/* SPDX-License-Identifier: GPL-2.0-only */
/* Stock accepts IDs through0xf0; real bounded retry provider from run025. */
#include <linux/slab.h>
#include "os44_descriptor_owner.h"
#include "ufsswap_provider_reconstruction.h"
#define OS44_DESCRIPTOR_ID_LIMIT 0xf1u
#define QUERY_REQ_RETRIES 3
static int os44_local_descriptor_retry(struct ufs_hba *hba,
				  enum query_opcode opcode,
				  enum desc_idn idn, u8 index,
				  u8 selector,
				  u8 *desc_buf, int *buf_len)
{
	int err;
	int retries;

	for (retries = QUERY_REQ_RETRIES; retries > 0; retries--) {
		err = os44_query_descriptor_once(hba, opcode, idn, index,
						selector, desc_buf, buf_len);
		if (!err || err == -EINVAL)
			break;
	}

	return err;
}
int os44_mi_ufshcd_read_desc_param(struct ufs_hba *hba,
			   enum desc_idn desc_id,
			   int desc_index,
			   u8 param_offset,
			   u8 *param_read_buf,
			   u8 param_size)
{
	int ret;
	u8 *desc_buf;
	int buff_len = QUERY_DESC_MAX_SIZE;
	bool is_kmalloc = true;

	/* Safety check */
	if (desc_id >= OS44_DESCRIPTOR_ID_LIMIT || !param_size)
		return -EINVAL;

	/* Check whether we need temp memory */
	if (param_offset != 0 || param_size < buff_len) {
		desc_buf = kzalloc(buff_len, GFP_KERNEL);
		if (!desc_buf)
			return -ENOMEM;
	} else {
		desc_buf = param_read_buf;
		is_kmalloc = false;
	}

	/* Request for full descriptor */
	ret = os44_local_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    desc_id, desc_index, 0,
					    desc_buf, &buff_len);
	if (ret) {
		dev_err(hba->dev, "%s: Failed reading descriptor. desc_id %d, desc_index %d, param_offset %d, ret %d\n",
			"mi_ufshcd_read_desc_param", desc_id, desc_index, param_offset, ret);
		goto out;
	}

	/* Update descriptor length */
	buff_len = desc_buf[QUERY_DESC_LENGTH_OFFSET];

	if (param_offset >= buff_len) {
		dev_err(hba->dev, "%s: Invalid offset 0x%x in descriptor IDN 0x%x, length 0x%x\n",
			"mi_ufshcd_read_desc_param", param_offset, desc_id, buff_len);
		ret = -EINVAL;
		goto out;
	}

	/* Sanity check */
	if (desc_buf[QUERY_DESC_DESC_TYPE_OFFSET] != desc_id) {
		dev_err(hba->dev, "%s: invalid desc_id %d in descriptor header\n",
			"mi_ufshcd_read_desc_param", desc_buf[QUERY_DESC_DESC_TYPE_OFFSET]);
		ret = -EINVAL;
		goto out;
	}

	if (is_kmalloc) {
		/* Make sure we don't copy more data than available */
		if (param_offset >= buff_len)
			ret = -EINVAL;
		else
			memcpy(param_read_buf, &desc_buf[param_offset],
			       min_t(u32, param_size, buff_len - param_offset));
	}
out:
	if (is_kmalloc)
		kfree(desc_buf);
	return ret;
}
