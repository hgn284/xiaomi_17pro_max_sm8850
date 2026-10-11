// SPDX-License-Identifier: GPL-2.0
// Derived from Western Digital 2018 sysfs implementation, adapted against OS44 whole ELF.
// Reconstruction-owned local provider names; no original ABI hash overrides.
#include <linux/unaligned.h>
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_descriptor_owner.h"
#include "ufsswap_query_reconstruction.h"
static ssize_t ufs_sysfs_read_desc_param(struct ufs_hba *hba,
				  enum desc_idn desc_id,
				  u8 desc_index,
				  u8 param_offset,
				  u8 *sysfs_buf,
				  u8 param_size)
{
	u8 desc_buf[8] = {0};
	int ret;

	if (param_size > 8)
		return -EINVAL;

	down(&hba->host_sem);
	if (!ufshcd_is_user_access_allowed(hba)) {
		ret = -EBUSY;
		goto out;
	}

	ufshcd_rpm_get_sync(hba);
	ret = os44_mi_ufshcd_read_desc_param(hba, desc_id, desc_index,
				param_offset, desc_buf, param_size);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}

	switch (param_size) {
	case 1:
		ret = sysfs_emit(sysfs_buf, "0x%02X\n", *desc_buf);
		break;
	case 2:
		ret = sysfs_emit(sysfs_buf, "0x%04X\n",
			get_unaligned_be16(desc_buf));
		break;
	case 4:
		ret = sysfs_emit(sysfs_buf, "0x%08X\n",
			get_unaligned_be32(desc_buf));
		break;
	case 8:
		ret = sysfs_emit(sysfs_buf, "0x%016llX\n",
			get_unaligned_be64(desc_buf));
		break;
	}

out:
	up(&hba->host_sem);
	return ret;
}

#define UFS_LUN_DESC_PARAM(_pname, _puname, _duname, _size)		\
static ssize_t _pname##_show(struct device *dev,			\
	struct device_attribute *attr, char *buf)			\
{									\
	struct scsi_device *sdev = to_scsi_device(dev);			\
	struct ufs_hba *hba = shost_priv(sdev->host);			\
	u8 lun = ufshcd_scsi_to_upiu_lun(sdev->lun);			\
	if (!ufs_is_valid_unit_desc_lun(&hba->dev_info, lun))		\
		return -EINVAL;						\
	return ufs_sysfs_read_desc_param(hba, QUERY_DESC_IDN_##_duname,	\
		lun, _duname##_DESC_PARAM##_puname, buf, _size);	\
}									\
static DEVICE_ATTR_RO(_pname)

#define UFS_UNIT_DESC_PARAM(_name, _uname, _size)			\
	UFS_LUN_DESC_PARAM(_name, _uname, UNIT, _size)

UFS_UNIT_DESC_PARAM(lu_enable, _LU_ENABLE, 1);
UFS_UNIT_DESC_PARAM(boot_lun_id, _BOOT_LUN_ID, 1);
UFS_UNIT_DESC_PARAM(lun_write_protect, _LU_WR_PROTECT, 1);
UFS_UNIT_DESC_PARAM(lun_queue_depth, _LU_Q_DEPTH, 1);
UFS_UNIT_DESC_PARAM(psa_sensitive, _PSA_SENSITIVE, 1);
UFS_UNIT_DESC_PARAM(lun_memory_type, _MEM_TYPE, 1);
UFS_UNIT_DESC_PARAM(data_reliability, _DATA_RELIABILITY, 1);
UFS_UNIT_DESC_PARAM(logical_block_size, _LOGICAL_BLK_SIZE, 1);
UFS_UNIT_DESC_PARAM(logical_block_count, _LOGICAL_BLK_COUNT, 8);
UFS_UNIT_DESC_PARAM(erase_block_size, _ERASE_BLK_SIZE, 4);
UFS_UNIT_DESC_PARAM(provisioning_type, _PROVISIONING_TYPE, 1);
UFS_UNIT_DESC_PARAM(physical_memory_resourse_count, _PHY_MEM_RSRC_CNT, 8);
UFS_UNIT_DESC_PARAM(context_capabilities, _CTX_CAPABILITIES, 2);
UFS_UNIT_DESC_PARAM(large_unit_granularity, _LARGE_UNIT_SIZE_M1, 1);
UFS_UNIT_DESC_PARAM(wb_buf_alloc_units, _WB_BUF_ALLOC_UNITS, 4);

static struct attribute *ufs_sysfs_unit_descriptor[] = {
	&dev_attr_lu_enable.attr,
	&dev_attr_boot_lun_id.attr,
	&dev_attr_lun_write_protect.attr,
	&dev_attr_lun_queue_depth.attr,
	&dev_attr_psa_sensitive.attr,
	&dev_attr_lun_memory_type.attr,
	&dev_attr_data_reliability.attr,
	&dev_attr_logical_block_size.attr,
	&dev_attr_logical_block_count.attr,
	&dev_attr_erase_block_size.attr,
	&dev_attr_provisioning_type.attr,
	&dev_attr_physical_memory_resourse_count.attr,
	&dev_attr_context_capabilities.attr,
	&dev_attr_large_unit_granularity.attr,
	&dev_attr_wb_buf_alloc_units.attr,
	NULL,
};

static umode_t ufs_unit_descriptor_is_visible(struct kobject *kobj, struct attribute *attr, int n)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct scsi_device *sdev = to_scsi_device(dev);
	u8 lun = ufshcd_scsi_to_upiu_lun(sdev->lun);
	umode_t mode = attr->mode;

	if (lun == UFS_UPIU_BOOT_WLUN || lun == UFS_UPIU_UFS_DEVICE_WLUN)
		/* Boot and device WLUN have no unit descriptors */
		mode = 0;
	if (lun == UFS_UPIU_RPMB_WLUN && attr == &dev_attr_wb_buf_alloc_units.attr)
		mode = 0;

	return mode;
}


const struct attribute_group ufs_sysfs_unit_descriptor_group = {
	.name = "unit_descriptor",
	.attrs = ufs_sysfs_unit_descriptor,
	.is_visible = ufs_unit_descriptor_is_visible,
};

static ssize_t dyn_cap_needed_attribute_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 value = 0;
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ufs_hba *hba = shost_priv(sdev->host);
	u8 lun = ufshcd_scsi_to_upiu_lun(sdev->lun);
	int ret;

	down(&hba->host_sem);
	if (!ufshcd_is_user_access_allowed(hba)) {
		ret = -EBUSY;
		goto out;
	}

	ufshcd_rpm_get_sync(hba);
	ret = os44_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
		QUERY_ATTR_IDN_DYN_CAP_NEEDED, lun, 0, &value);
	ufshcd_rpm_put_sync(hba);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}

	ret = sysfs_emit(buf, "0x%08X\n", value);

out:
	up(&hba->host_sem);
	return ret;
}
static DEVICE_ATTR_RO(dyn_cap_needed_attribute);

static struct attribute *ufs_sysfs_lun_attributes[] = {
	&dev_attr_dyn_cap_needed_attribute.attr,
	NULL,
};

const struct attribute_group ufs_sysfs_lun_attributes_group = {
	.attrs = ufs_sysfs_lun_attributes,
};


const struct attribute_group *os44_ufshcd_driver_groups[] = {
 &ufs_sysfs_unit_descriptor_group,
 &ufs_sysfs_lun_attributes_group,
 NULL,
};
