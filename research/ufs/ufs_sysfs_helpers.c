// SPDX-License-Identifier: GPL-2.0-only
#include <linux/unaligned.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "ufsswap_provider_reconstruction.h"
#include "os44_sysfs_helpers.h"
static inline void release_locked(struct ufs_hba *hba)
{
 guard(spinlock_irqsave)(&hba->clk_gating.lock);
 os44_core_release_locked(hba);
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
static void ufshcd_configure_auto_hibern8(struct ufs_hba *hba)
{
	if (!ufshcd_is_auto_hibern8_supported(hba))
		return;

	ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);
}
void os44_ufshcd_auto_hibern8_update(struct ufs_hba *hba, u32 ahit)
{
	const u32 cur_ahit = READ_ONCE(hba->ahit);

	if (!ufshcd_is_auto_hibern8_supported(hba) || cur_ahit == ahit)
		return;

	WRITE_ONCE(hba->ahit, ahit);
	if (!pm_runtime_suspended(&hba->ufs_device_wlun->sdev_gendev)) {
		ufshcd_rpm_get_sync(hba);
		os44_core_hold(hba);
		ufshcd_configure_auto_hibern8(hba);
		release_locked(hba);
		ufshcd_rpm_put_sync(hba);
	}
}
int os44_ufshcd_read_device_lvl_exception_id(struct ufs_hba *hba, u64 *exception_id)
{
	struct utp_upiu_query_v4_0 *upiu_resp;
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;

	if (hba->dev_info.wspecversion < 0x410)
		return -EOPNOTSUPP;

	os44_core_hold(hba);
	mutex_lock(&hba->dev_cmd.lock);

	init_query(hba, &request, &response,
			  UPIU_QUERY_OPCODE_READ_ATTR,
			  QUERY_ATTR_IDN_DEV_LVL_EXCEPTION_ID, 0, 0);

	request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;

	err = os44_core_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, os44_core_dev_cmd_timeout);

	if (err) {
		dev_err(hba->dev, "%s: failed to read device level exception %d\n",
			"ufshcd_read_device_lvl_exception_id", err);
		goto out;
	}

	upiu_resp = (struct utp_upiu_query_v4_0 *)response;
	*exception_id = get_unaligned_be64(&upiu_resp->osf3);
out:
	mutex_unlock(&hba->dev_cmd.lock);
	release_locked(hba);

	return err;
}
void os44_ufshcd_pm_qos_exit(struct ufs_hba *hba)
{
	guard(mutex)(&to_hba_priv(hba)->pm_qos_mutex);

	if (*(u8 *)&hba->pm_qos_enabled != 1)
		return;

	cpu_latency_qos_remove_request(&hba->pm_qos_req);
	hba->pm_qos_enabled = false;
}
