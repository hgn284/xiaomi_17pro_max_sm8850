// SPDX-License-Identifier: GPL-2.0-only
/* OS44 equivalent reconstruction, not a claim of original private C types.
 * Async does NOT publish the owner HBA; that belongs to ufshcd_init. */
#include <linux/async.h>
#include <linux/blk-pm.h>
#include <linux/pm_opp.h>
#include <linux/devfreq.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_xiaomi_owner.h"
#include "os44_devfreq.h"
#include "os44_probe_hba.h"
#include "os44_cld_init.h"
#include "ufsswap_query_reconstruction.h"
#include "ufsswap_debug_reconstruction.h"
#include "os44_async_scan.h"
int os44_check_fastdiscard_policy(struct ufs_hba *);
void os44_ufs_wp_getpartation(struct ufs_hba *);
void os44_ufssnapshot_init(struct ufs_hba *);
int os44_ufseua_probe(struct ufs_hba *);
int os44_ufs_bsg_probe(struct ufs_hba *);
int __ufshcd_write_ee_control(struct ufs_hba *, u32);
static inline u16 ufshcd_upiu_wlun_to_scsi_wlun(u8 id)
{ return (id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE; }
static inline void ufshcd_blk_pm_runtime_init(struct scsi_device *sdev)
{
	struct Scsi_Host *shost = sdev->host;

	scsi_autopm_get_device(sdev);
	blk_pm_runtime_init(sdev->request_queue, &sdev->sdev_gendev);
	if (sdev->rpm_autosuspend)
		pm_runtime_set_autosuspend_delay(&sdev->sdev_gendev,
						 shost->rpm_autosuspend_delay);
	scsi_autopm_put_device(sdev);
}
static int ufshcd_scsi_add_wlus(struct ufs_hba *hba)
{
	int ret = 0;
	struct scsi_device *sdev_boot, *sdev_rpmb;

	hba->ufs_device_wlun = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_UFS_DEVICE_WLUN), NULL);
	if (IS_ERR(hba->ufs_device_wlun)) {
		ret = PTR_ERR(hba->ufs_device_wlun);
		hba->ufs_device_wlun = NULL;
		goto out;
	}
	scsi_device_put(hba->ufs_device_wlun);
 os44_check_fastdiscard_policy(hba);

	sdev_rpmb = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_RPMB_WLUN), NULL);
	if (IS_ERR(sdev_rpmb)) {
		ret = PTR_ERR(sdev_rpmb);
		goto remove_ufs_device_wlun;
	}
	ufshcd_blk_pm_runtime_init(sdev_rpmb);
	scsi_device_put(sdev_rpmb);

	sdev_boot = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_BOOT_WLUN), NULL);
	if (IS_ERR(sdev_boot)) {
		dev_err(hba->dev, "%s: BOOT WLUN not found\n", __func__);
	} else {
		ufshcd_blk_pm_runtime_init(sdev_boot);
		scsi_device_put(sdev_boot);
	}
	goto out;

remove_ufs_device_wlun:
	scsi_remove_device(hba->ufs_device_wlun);
out:
	return ret;
}
static int ufshcd_devfreq_init(struct ufs_hba *hba)
{
	struct list_head *clk_list = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	struct devfreq *devfreq;
	int ret;

	/* Skip devfreq if we don't have any clocks in the list */
	if (list_empty(clk_list))
		return 0;

	if (!hba->use_pm_opp) {
		clki = list_first_entry(clk_list, struct ufs_clk_info, list);
		dev_pm_opp_add(hba->dev, clki->min_freq, 0);
		dev_pm_opp_add(hba->dev, clki->max_freq, 0);
	}

	ufshcd_vops_config_scaling_param(hba, &hba->vps->devfreq_profile,
					 &hba->vps->ondemand_data);
	devfreq = devfreq_add_device(hba->dev,
			&hba->vps->devfreq_profile,
			DEVFREQ_GOV_SIMPLE_ONDEMAND,
			&hba->vps->ondemand_data);
	if (IS_ERR(devfreq)) {
		ret = PTR_ERR(devfreq);
		dev_err(hba->dev, "Unable to register with devfreq %d\n", ret);

		if (!hba->use_pm_opp) {
			dev_pm_opp_remove(hba->dev, clki->min_freq);
			dev_pm_opp_remove(hba->dev, clki->max_freq);
		}
		return ret;
	}

	hba->devfreq = devfreq;

	return 0;
}
static void ufshcd_init_clk_scaling_sysfs(struct ufs_hba *hba)
{
	hba->clk_scaling.enable_attr.show = os44_ufshcd_clkscale_enable_show;
	hba->clk_scaling.enable_attr.store = os44_ufshcd_clkscale_enable_store;
	sysfs_attr_init(&hba->clk_scaling.enable_attr.attr);
	hba->clk_scaling.enable_attr.attr.name = "clkscale_enable";
	hba->clk_scaling.enable_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &hba->clk_scaling.enable_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkscale_enable\n");
}
static int ufshcd_add_lus(struct ufs_hba *hba)
{
	int ret;

	/* Add required well known logical units to scsi mid layer */
	ret = ufshcd_scsi_add_wlus(hba);
	if (ret)
		goto out;

	/* Initialize devfreq after UFS device is detected */
	if (ufshcd_is_clkscaling_supported(hba)) {
		memcpy(&hba->clk_scaling.saved_pwr_info,
			&hba->pwr_info,
			sizeof(struct ufs_pa_layer_attr));
		hba->clk_scaling.is_allowed = true;

		ret = ufshcd_devfreq_init(hba);
		if (ret)
			goto out;

		hba->clk_scaling.is_enabled = true;
		ufshcd_init_clk_scaling_sysfs(hba);
	}

	/*
	 * The RTC update code accesses the hba->ufs_device_wlun->sdev_gendev
	 * pointer and hence must only be started after the WLUN pointer has
	 * been initialized by ufshcd_scsi_add_wlus().
	 */
	schedule_delayed_work(&hba->ufs_rtc_update_work,
			      msecs_to_jiffies(10000));

	os44_ufs_bsg_probe(hba);
	scsi_scan_host(hba->host);

out:
	return ret;
}
static void snapshot_ee(struct ufs_hba *hba)
{
 u16 set,mask,ctrl;int ret=0;
 if (!os44_ufs_xiaomi.snapshot.supported) {
  dev_err(hba->dev,"%s: ufssnapshot not support.\n","ufshcd_set_snapshot_ee");return;
 }
 switch(hba->dev_info.wmanufacturerid) {
 case 0x1ad:case 0x12c:set=0x8000;break;
 case 0x198:case 0xf86:set=0x800;break;
 default:dev_err(hba->dev,"%s: ufssnapshot not find se.\n","ufshcd_set_snapshot_ee");return;
 }
 mutex_lock(&hba->ee_ctrl_mutex);
 mask=hba->ee_drv_mask|set;ctrl=mask|hba->ee_usr_mask;
 if(ctrl!=hba->ee_ctrl_mask)ret=__ufshcd_write_ee_control(hba,ctrl);
 if(!ret){hba->ee_ctrl_mask=ctrl;hba->ee_drv_mask=mask;}
 mutex_unlock(&hba->ee_ctrl_mutex);
 if(ret){dev_err(hba->dev,"%s: ufssnapshot failed to enable exception event %d\n","ufshcd_set_snapshot_ee",ret);return;}
 os44_ufs_xiaomi.snapshot_last_event=jiffies;
}
void os44_ufshcd_async_scan(void *data, async_cookie_t cookie)
{
 struct ufs_hba *hba=data;int ret,i,j;
 struct os44_completion_stats *s=&os44_ufs_xiaomi.stats;
 down(&hba->host_sem);ret=os44_ufshcd_probe_hba(hba,true);up(&hba->host_sem);
 if(ret)goto out;
 ret=ufshcd_add_lus(hba);
 os44_ufscld_init(hba);
 os44_ufs_wp_getpartation(hba);
 os44_ufsswap_probe(hba,&os44_ufs_xiaomi.swap.swap);
 os44_ufssnapshot_init(hba);
 snapshot_ee(hba);
 os44_ufseua_probe(hba);
 os44_ufsfbo_init(hba);
 os44_ufsmonitor_create_sysfs(hba);
 s->enabled=0;
 memset((u8 *)s+8,0,0x700);
 s->interval=60000000000ULL;
 for(i=0;i<2;i++)for(j=0;j<5;j++)s->slow_us[i][j]=100000;
 INIT_DELAYED_WORK(&os44_ufs_data.load_work,os44_debug_load_work);
 INIT_DELAYED_WORK(&os44_ufs_data.save_work,os44_debug_save_work);
out:
 pm_runtime_put_sync(hba->dev);
 if(ret)dev_err(hba->dev,"%s failed: %d\n","ufshcd_async_scan",ret);
}
static_assert(sizeof(struct os44_completion_stats)==0x708);
static_assert(offsetof(struct ufs_hba,host_sem)==0x240);
static_assert(offsetof(struct ufs_hba,ufs_rtc_update_work)==0x1440);
