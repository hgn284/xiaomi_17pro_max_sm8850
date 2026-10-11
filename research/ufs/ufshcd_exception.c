// SPDX-License-Identifier: GPL-2.0-only
/* OS44 whole exception callback. Source-pair layout, not original private typedefs. */
#include <linux/module.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "ufsswap_provider_reconstruction.h"
#include "os44_bkops_contract.h"
#include "os44_xiaomi_owner.h"
#include "os44_exception_contract.h"
#define CREATE_TRACE_POINTS
#include "os44_exception_trace.h"
#define QUERY_REQ_RETRIES 3
static inline void ufshcd_scsi_block_requests(struct ufs_hba *hba)
{ if (atomic_inc_return(&hba->scsi_block_reqs_cnt)==1) scsi_block_requests(hba->host); }
static inline void ufshcd_scsi_unblock_requests(struct ufs_hba *hba)
{ if (atomic_dec_return(&hba->scsi_block_reqs_cnt)==0) scsi_unblock_requests(hba->host); }
static int local_query_attr_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum attr_idn idn, u8 index, u8 selector,
	u32 *attr_val)
{
	int ret = 0;
	u32 retries;

	for (retries = QUERY_REQ_RETRIES; retries > 0; retries--) {
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
			"ufshcd_query_attr_retry", idn, ret, QUERY_REQ_RETRIES);
	return ret;
}
static inline int ufshcd_get_bkops_status_local(struct ufs_hba *hba, u32 *status)
{ return local_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, QUERY_ATTR_IDN_BKOPS_STATUS, 0, 0, status); }
static void ufshcd_bkops_exception_event_handler(struct ufs_hba *hba)
{
	int err;
	u32 curr_status = 0;

	if (hba->is_urgent_bkops_lvl_checked)
		goto enable_auto_bkops;

	err = ufshcd_get_bkops_status_local(hba, &curr_status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get BKOPS status %d\n",
				"ufshcd_bkops_exception_event_handler", err);
		goto out;
	}

	/*
	 * We are seeing that some devices are raising the urgent bkops
	 * exception events even when BKOPS status doesn't indicate performace
	 * impacted or critical. Handle these device by determining their urgent
	 * bkops status at runtime.
	 */
	if (curr_status < BKOPS_STATUS_PERF_IMPACT) {
		dev_err(hba->dev, "%s: device raised urgent BKOPS exception for bkops status %d\n",
				"ufshcd_bkops_exception_event_handler", curr_status);
		/* update the current status as the urgent bkops level */
		hba->urgent_bkops_lvl = curr_status;
		hba->is_urgent_bkops_lvl_checked = true;
	}

enable_auto_bkops:
	err = os44_ufshcd_enable_auto_bkops(hba);
out:
	if (err < 0)
		dev_err(hba->dev, "%s: failed to handle urgent bkops %d\n",
				"ufshcd_bkops_exception_event_handler", err);
}
static void local_temp_event(struct ufs_hba *hba)
{
 u32 value = 0;
 if (!local_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, QUERY_ATTR_IDN_CASE_ROUGH_TEMP, 0, 0, &value))
  dev_info(hba->dev, "exception Tcase %d\n", value - 80);
}
/* Original 5x6-byte table, including sentinel; middle word retained. */
struct os44_snapshot_ee { u16 manufacturer; u16 control; u16 status; };
static const struct os44_snapshot_ee ufs_snapshot_ees[] = {
 {0x1ad,0x8000,0x8000}, {0x12c,0x8000,0x8000},
 {0x198,0x0800,0x0800}, {0xf86,0x0800,0x0800}, {0,0,0}
};
static inline void local_snapshot_event(struct ufs_hba *hba, u32 *status)
{
 const struct os44_snapshot_ee *ee;
 switch (hba->dev_info.wmanufacturerid) {
 case 0x1ad: ee=&ufs_snapshot_ees[0]; break;
 case 0x12c: ee=&ufs_snapshot_ees[1]; break;
 case 0x198: ee=&ufs_snapshot_ees[2]; break;
 case 0xf86: ee=&ufs_snapshot_ees[3]; break;
 default: return;
 }
 if (*status & hba->ee_drv_mask & ee->status) {
  char *env[] = { "UFS_SNAPSHOT_DROP_EVENT", NULL };
  if (time_before(jiffies, os44_ufs_xiaomi.snapshot_last_event + 45000)) return;
  os44_ufs_xiaomi.snapshot_last_event = jiffies;
  kobject_uevent_env(&hba->dev->kobj, KOBJ_CHANGE, env);
  os44_ufssnapshot_inc_eecount(hba);
 }
}
void os44_ufshcd_exception_event_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	int err;
	u32 status = 0;
	hba = container_of(work, struct ufs_hba, eeh_work);

	ufshcd_scsi_block_requests(hba);
	err = local_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, QUERY_ATTR_IDN_EE_STATUS, 0, 0, &status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get exception status %d\n",
				"ufshcd_exception_event_handler", err);
		goto out;
	}

	trace_ufshcd_exception_event(hba, status);

	if (status & hba->ee_drv_mask & MASK_EE_URGENT_BKOPS)
		ufshcd_bkops_exception_event_handler(hba);

	if (status & hba->ee_drv_mask & MASK_EE_URGENT_TEMP)
		local_temp_event(hba);

	if (status & hba->ee_drv_mask & MASK_EE_HEALTH_CRITICAL) {
		hba->critical_health_count++;
		sysfs_notify(&hba->dev->kobj, NULL, "critical_health");
	}

	if (status & hba->ee_drv_mask & MASK_EE_DEV_LVL_EXCEPTION) {
		atomic_inc(&hba->dev_lvl_exception_count);
		sysfs_notify(&hba->dev->kobj, NULL, "device_lvl_exception_count");
	}

	local_snapshot_event(hba, &status);
	os44_ufs_debugfs_exception_event(hba, status);
out:
	ufshcd_scsi_unblock_requests(hba);
}
