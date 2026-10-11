/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstructed stock 0x11d54..0x1223c; Linux common source scaffold, independently checked. */
#include "ufsswap_provider_reconstruction.h"
#define OS44_QUERY_RETRIES 3
#define CHECK(m,v) static_assert(offsetof(struct ufs_hba,m)==(v))
CHECK(dev,0x40); CHECK(dev_cmd.lock,0x970); CHECK(clk_scaling_lock,0xcd0);
CHECK(clk_gating.lock,0xb50); CHECK(dev_cmd.query.request,0x9a8);
CHECK(dev_cmd.query.descriptor,0x9c0); CHECK(dev_cmd.query.response,0x9c8);
static_assert(sizeof(struct ufs_query_req)==24);
static_assert(sizeof(struct ufs_query_res)==20);
static_assert(QUERY_DESC_MIN_SIZE==2 && QUERY_DESC_MAX_SIZE==255 && OS44_QUERY_RETRIES==3);
static void lock_query(struct ufs_hba *hba)
{
 os44_core_hold(hba);
 mutex_lock(&hba->dev_cmd.lock);
 down_read(&hba->clk_scaling_lock);
}
static void unlock_query(struct ufs_hba *hba)
{
 unsigned long flags;
 up_read(&hba->clk_scaling_lock);
 mutex_unlock(&hba->dev_cmd.lock);
 spin_lock_irqsave(&hba->clk_gating.lock,flags);
 os44_core_release_locked(hba);
 spin_unlock_irqrestore(&hba->clk_gating.lock,flags);
}
static inline void init_query(struct ufs_hba *hba,
		struct ufs_query_req **request, struct ufs_query_res **response,
		unsigned int opcode, u8 idn, u8 index, u8 selector)
{
	*request = &hba->dev_cmd.query.request;
	*response = &hba->dev_cmd.query.response;
	memset(*request, 0, sizeof(struct ufs_query_req));
	memset(*response, 0, sizeof(struct ufs_query_res));
	(*request)->upiu_req.opcode = opcode;
	(*request)->upiu_req.idn = idn;
	(*request)->upiu_req.index = index;
	(*request)->upiu_req.selector = selector;
}

int os44_query_attr_once(struct ufs_hba *hba, unsigned int opcode,
		      u8 idn, u8 index, u8 selector, u32 *attr_val)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;

	BUG_ON(!hba);

	if (!attr_val) {
		dev_err(hba->dev, "%s: attribute value required for opcode 0x%x\n",
				"ufshcd_query_attr", opcode);
		return -EINVAL;
	}

	lock_query(hba);

	init_query(hba, &request, &response, opcode, idn, index,
			selector);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		request->upiu_req.value = cpu_to_be32(*attr_val);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		break;
	default:
		dev_err(hba->dev, "%s: Expected query attr opcode but got = 0x%.2x\n",
				"ufshcd_query_attr", opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = os44_core_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, os44_core_dev_cmd_timeout);

	if (err) {
		dev_err(hba->dev, "%s: opcode 0x%.2x for idn %d failed, index %d, err = %d\n",
				"ufshcd_query_attr", opcode, idn, index, err);
		goto out_unlock;
	}

	*attr_val = be32_to_cpu(response->upiu_res.value);

out_unlock:
	unlock_query(hba);
	return err;
}

int os44_query_attr(struct ufs_hba *hba,
	unsigned int opcode, u8 idn, u8 index, u8 selector,
	u32 *attr_val)
{
	int ret = 0;
	u32 retries;

	for (retries = OS44_QUERY_RETRIES; retries > 0; retries--) {
		ret = os44_query_attr_once(hba, opcode, idn, index,
						selector, attr_val);
		if (ret)
			dev_dbg(hba->dev, "%s: failed with error %d, retries %d\n",
				"ufshcd_query_attr_retry", ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query attribute, idn %d, failed with error %d after %d retries\n",
			"ufshcd_query_attr_retry", idn, ret, OS44_QUERY_RETRIES);
	return ret;
}

int os44_query_descriptor_once(struct ufs_hba *hba,
			unsigned int opcode, u8 idn, u8 index,
			u8 selector, u8 *desc_buf, int *buf_len)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;

	BUG_ON(!hba);

	if (!desc_buf) {
		dev_err(hba->dev, "%s: descriptor buffer required for opcode 0x%x\n",
				"__ufshcd_query_descriptor", opcode);
		return -EINVAL;
	}

	if (*buf_len < QUERY_DESC_MIN_SIZE || *buf_len > QUERY_DESC_MAX_SIZE) {
		dev_err(hba->dev, "%s: descriptor buffer size (%d) is out of range\n",
				"__ufshcd_query_descriptor", *buf_len);
		return -EINVAL;
	}

	lock_query(hba);

	init_query(hba, &request, &response, opcode, idn, index,
			selector);
	hba->dev_cmd.query.descriptor = desc_buf;
	request->upiu_req.length = cpu_to_be16(*buf_len);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_WRITE_DESC:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		break;
	case UPIU_QUERY_OPCODE_READ_DESC:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		break;
	default:
		dev_err(hba->dev,
				"%s: Expected query descriptor opcode but got = 0x%.2x\n",
				"__ufshcd_query_descriptor", opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = os44_core_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, os44_core_dev_cmd_timeout);

	if (err) {
		dev_err(hba->dev, "%s: opcode 0x%.2x for idn %d failed, index %d, err = %d\n",
				"__ufshcd_query_descriptor", opcode, idn, index, err);
		goto out_unlock;
	}

	*buf_len = be16_to_cpu(response->upiu_res.length);

out_unlock:
	hba->dev_cmd.query.descriptor = NULL;
	unlock_query(hba);
	return err;
}

int os44_query_descriptor(struct ufs_hba *hba,
				  unsigned int opcode,
				  u8 idn, u8 index,
				  u8 selector,
				  u8 *desc_buf, int *buf_len)
{
	int err;
	int retries;

	for (retries = OS44_QUERY_RETRIES; retries > 0; retries--) {
		err = os44_query_descriptor_once(hba, opcode, idn, index,
						selector, desc_buf, buf_len);
		if (!err || err == -EINVAL)
			break;
	}

	return err;
}
