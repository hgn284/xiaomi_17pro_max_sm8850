// SPDX-License-Identifier: GPL-2.0-only
/* OS44 TW/TP reconstruction from complete original callbacks; no root stub. */
#include <ufs/ufshcd.h>
#include "drivers/ufs/core/ufshcd-priv.h"
#include "os44_wb_contract.h"
#include "os44_bkops_contract.h"
#include "os44_descriptor_owner.h"
#include "ufsswap_query_reconstruction.h"
#include "os44_sysfs_tw_tp.h"
/* TW uses the original full-byte ==1 check, unlike TP's boolean low bit. */
static inline bool tw_busy(struct ufs_hba *hba)
{
 return *(u8 *)&hba->shutting_down == 1;
}
static ssize_t enable_tw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 bool flag = false;
 u8 ret, index;
 down(&hba->host_sem);
 if (tw_busy(hba)) { up(&hba->host_sem); return -EBUSY; }
 index = ufshcd_wb_get_query_index(hba);
 ufshcd_rpm_get_sync(hba);
 ret = os44_ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG, 14, index, &flag);
 ufshcd_rpm_put_sync(hba);
 up(&hba->host_sem);
 if (ret) return -EINVAL;
 return snprintf(buf, PAGE_SIZE, "%s\n", flag ? "1" : "0");
}
static ssize_t enable_flush_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 bool flag = false;
 u8 ret, index;
 down(&hba->host_sem);
 if (tw_busy(hba)) { up(&hba->host_sem); return -EBUSY; }
 index = ufshcd_wb_get_query_index(hba);
 ufshcd_rpm_get_sync(hba);
 ret = os44_ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG, 15, index, &flag);
 ufshcd_rpm_put_sync(hba);
 up(&hba->host_sem);
 if (ret) return -EINVAL;
 return snprintf(buf, PAGE_SIZE, "%s\n", flag ? "1" : "0");
}
static ssize_t enable_autoflush_in_h8_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 bool flag = false;
 u8 ret, index;
 down(&hba->host_sem);
 if (tw_busy(hba)) { up(&hba->host_sem); return -EBUSY; }
 index = ufshcd_wb_get_query_index(hba);
 ufshcd_rpm_get_sync(hba);
 ret = os44_ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG, 16, index, &flag);
 ufshcd_rpm_put_sync(hba);
 up(&hba->host_sem);
 if (ret) return -EINVAL;
 return snprintf(buf, PAGE_SIZE, "%s\n", flag ? "1" : "0");
}
static ssize_t enable_tw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 unsigned int value = 0;
 if (kstrtouint(buf, 0, &value)) return -EINVAL;
 if (os44_ufshcd_wb_toggle(hba, !!value)) return -EINVAL;
 return count;
}
static DEVICE_ATTR_RW(enable_tw);
static ssize_t enable_flush_en_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 unsigned int value = 0;
 if (kstrtouint(buf, 0, &value)) return -EINVAL;
 os44_ufshcd_wb_toggle_buf_flush(hba, !!value);
 return count;
}
static DEVICE_ATTR_RW(enable_flush_en);
static ssize_t enable_autoflush_in_h8_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 unsigned int value = 0;
 if (kstrtouint(buf, 0, &value)) return -EINVAL;
 os44_ufshcd_wb_toggle_buf_flush_during_h8(hba, !!value);
 return count;
}
static DEVICE_ATTR_RW(enable_autoflush_in_h8);
static ssize_t tp_alloc_units_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 __be32 value = 0;
 int ret;
 down(&hba->host_sem);
 if (tw_busy(hba)) { up(&hba->host_sem); return -EBUSY; }
 ufshcd_rpm_get_sync(hba);
 ret = os44_mi_ufshcd_read_desc_param(hba, 0xf0, 0, 0xdb, (u8 *)&value, 4);
 ufshcd_rpm_put_sync(hba);
 up(&hba->host_sem);
 if (ret) return -EINVAL;
 return sysfs_emit(buf, "0x%08X\n", be32_to_cpu(value));
}
static DEVICE_ATTR_RO(tp_alloc_units);
static ssize_t tp_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 bool flag = false;
 int ret;
 down(&hba->host_sem);
 if (!ufshcd_is_user_access_allowed(hba)) { ret = -EBUSY; goto out; }
 ufshcd_rpm_get_sync(hba);
 ret = os44_ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG, 0x86, 0, &flag);
 ufshcd_rpm_put_sync(hba);
 if (ret) { ret = -EINVAL; goto out; }
 ret = sysfs_emit(buf, "%s\n", flag ? "true" : "false");
out:
 up(&hba->host_sem);
 return ret;
}
static DEVICE_ATTR_RO(tp_en);
static ssize_t tp_avail_buff_percentage_show(struct device *dev, struct device_attribute *attr, char *buf)
{
 struct ufs_hba *hba = dev_get_drvdata(dev);
 u32 value = 0;
 int ret;
 down(&hba->host_sem);
 if (!ufshcd_is_user_access_allowed(hba)) { ret = -EBUSY; goto out; }
 ufshcd_rpm_get_sync(hba);
 ret = os44_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR, 0x99, 0, 0, &value);
 ufshcd_rpm_put_sync(hba);
 if (ret) { ret = -EINVAL; goto out; }
 ret = sysfs_emit(buf, "0x%08X\n", value);
out:
 up(&hba->host_sem);
 return ret;
}
static DEVICE_ATTR_RO(tp_avail_buff_percentage);
static struct attribute *ufs_sysfs_device_tw[] = {
 &dev_attr_enable_tw.attr, &dev_attr_enable_flush_en.attr,
 &dev_attr_enable_autoflush_in_h8.attr, NULL
};
static struct attribute *ufs_sysfs_device_tp[] = {
 &dev_attr_tp_alloc_units.attr, &dev_attr_tp_en.attr,
 &dev_attr_tp_avail_buff_percentage.attr, NULL
};
const struct attribute_group ufs_sysfs_tw_group = { .name = "ufstw", .attrs = ufs_sysfs_device_tw };
const struct attribute_group ufs_sysfs_tp_group = { .name = "ufstp", .attrs = ufs_sysfs_device_tp };
