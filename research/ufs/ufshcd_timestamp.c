/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent stock timestamp query. Original lacks newer NO_TIMESTAMP quirk guard. */
#include <linux/unaligned.h>
#include <linux/timekeeping.h>
#include "ufsswap_provider_reconstruction.h"
#include "os44_pm_contract.h"
static_assert(offsetof(struct ufs_hba, dev_info.wspecversion)==0xa00);
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

void os44_ufshcd_set_timestamp_attr(struct ufs_hba *hba)
{
	int err;
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	struct ufs_dev_info *dev_info = &hba->dev_info;
	struct utp_upiu_query_v4_0 *upiu_data;

	if (dev_info->wspecversion < 0x400)
		return;

	lock_query(hba);

	init_query(hba, &request, &response,
			  UPIU_QUERY_OPCODE_WRITE_ATTR,
			  QUERY_ATTR_IDN_TIMESTAMP, 0, 0);

	request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;

	upiu_data = (struct utp_upiu_query_v4_0 *)&request->upiu_req;

	put_unaligned_be64(ktime_get_real_ns(), &upiu_data->osf3);

	err = os44_core_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, os44_core_dev_cmd_timeout);

	if (err)
		dev_err(hba->dev, "%s: failed to set timestamp %d\n",
			"ufshcd_set_timestamp_attr", err);

	unlock_query(hba);
}
