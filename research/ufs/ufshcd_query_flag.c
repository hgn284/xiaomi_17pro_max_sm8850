/* SPDX-License-Identifier: GPL-2.0-only */
#include "ufsswap_provider_reconstruction.h"
#include "os44_bkops_contract.h"
#define MASK_QUERY_UPIU_FLAG_LOC 0xFF /* ufshcd.c:94 */
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

int os44_ufshcd_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
			enum flag_idn idn, u8 index, bool *flag_res)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err, selector = 0;
	int timeout = os44_core_dev_cmd_timeout;

	BUG_ON(!hba);

	lock_query(hba);

	init_query(hba, &request, &response, opcode, idn, index,
			selector);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_SET_FLAG:
	case UPIU_QUERY_OPCODE_CLEAR_FLAG:
	case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		if (!flag_res) {
			/* No dummy reads */
			dev_err(hba->dev, "%s: Invalid argument for read request\n",
					"ufshcd_query_flag");
			err = -EINVAL;
			goto out_unlock;
		}
		break;
	default:
		dev_err(hba->dev,
			"%s: Expected query flag opcode but got = %d\n",
			"ufshcd_query_flag", opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = os44_core_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, timeout);

	if (err) {
		dev_err(hba->dev,
			"%s: Sending flag query for idn %d failed, err = %d\n",
			"ufshcd_query_flag", idn, err);
		goto out_unlock;
	}

	if (flag_res)
		*flag_res = (be32_to_cpu(response->upiu_res.value) &
				MASK_QUERY_UPIU_FLAG_LOC) & 0x1;

out_unlock:
	unlock_query(hba);
	return err;
}

int os44_ufshcd_query_flag_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum flag_idn idn, u8 index, bool *flag_res)
{
	int ret;
	int retries;

	for (retries = 0; retries < 3; retries++) {
		ret = os44_ufshcd_query_flag(hba, opcode, idn, index, flag_res);
		if (ret)
			dev_dbg(hba->dev,
				"%s: failed with error %d, retries %d\n",
				"ufshcd_query_flag_retry", ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query flag, opcode %d, idn %d, failed with error %d after %d retries\n",
			"ufshcd_query_flag_retry", opcode, idn, ret, retries);
	return ret;
}
