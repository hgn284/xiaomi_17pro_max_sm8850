/* SPDX-License-Identifier: GPL-2.0-only */
#include <ufs/ufshcd.h>
#include "os44_cld_sysfs.h"
#include "ufsswap_query_reconstruction.h"
#include "os44_device_init_helpers.h"
ssize_t os44_ufscld_sysfs_show_frag_level(struct os44_cld_view *cld, char *buf)
{
 u32 value = 0;
 os44_ufscld_get_frag_level(cld, &value);
 return snprintf(buf, PAGE_SIZE, "%d\n", value);
}
ssize_t os44_ufscld_sysfs_show_cld_operation_status(struct os44_cld_view *cld, char *buf)
{
 u32 value = 0;
 os44_ufscld_get_operation_status(cld, &value);
 return snprintf(buf, PAGE_SIZE, "%d\n", value);
}
ssize_t os44_ufscld_sysfs_show_hid_available_size(struct os44_cld_view *cld, char *buf)
{
 struct ufs_hba *hba = cld->hba;
 u32 available = 0, operation = 1;
 int ret, retries = 3;
 if (!strncmp(cld->identity.product, "XBUSC1B19A9TF2", 14) ||
     !strncmp(cld->identity.product, "XBUSC1B2AA9TF3", 14))
  return snprintf(buf, PAGE_SIZE, "NOT SUPPORT\n");
 do {
  ret = local_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, 0x35, 0, 0, &operation);
 } while (ret && retries--);
 if (ret) return snprintf(buf, PAGE_SIZE, "%d\n", -1);
 ret = local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, 0x36, 0, 0, &available);
 if (ret) available = -1;
 return snprintf(buf, PAGE_SIZE, "%d\n", available);
}
ssize_t os44_ufscld_sysfs_show_hid_state(struct os44_cld_view *cld, char *buf)
{
 struct ufs_hba *hba = cld->hba;
 u32 state = 15;
 if (!strncmp(cld->identity.product, "XBUSC1B19A9TF2", 14) ||
     !strncmp(cld->identity.product, "XBUSC1B2AA9TF3", 14))
  return snprintf(buf, PAGE_SIZE, "NOT SUPPORT\n");
 local_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, 0x39, 0, 0, &state);
 return snprintf(buf, PAGE_SIZE, "%d\n", state);
}
ssize_t os44_ufscld_sysfs_show_trigger(struct os44_cld_view *cld, char *buf)
{
 printk(KERN_INFO "%s:%d info: cld_trigger %d\n", "ufscld_sysfs_show_trigger", 22, cld->trigger);
 return snprintf(buf, PAGE_SIZE, "%d\n", cld->trigger);
}
ssize_t os44_ufscld_sysfs_show_trigger_interval(struct os44_cld_view *cld, char *buf)
{
 printk(KERN_INFO "%s:%d info: cld_trigger_interval %d\n", "ufscld_sysfs_show_trigger_interval", 54, cld->interval_ms);
 return snprintf(buf, PAGE_SIZE, "%d\n", cld->interval_ms);
}
ssize_t os44_ufscld_sysfs_show_debug(struct os44_cld_view *cld, char *buf)
{
 printk(KERN_INFO "%s:%d info: debug %d\n", "ufscld_sysfs_show_debug", 84, cld->debug);
 return snprintf(buf, PAGE_SIZE, "%d\n", cld->debug);
}
ssize_t os44_ufscld_sysfs_show_block_suspend(struct os44_cld_view *cld, char *buf)
{
 printk(KERN_INFO "%s:%d info: block suspend %d\n", "ufscld_sysfs_show_block_suspend", 178, cld->block_suspend);
 return snprintf(buf, PAGE_SIZE, "%d\n", cld->block_suspend);
}
ssize_t os44_ufscld_sysfs_show_auto_hibern8_enable(struct os44_cld_view *cld, char *buf)
{
 printk(KERN_INFO "%s:%d info: HCI auto hibern8 %d\n", "ufscld_sysfs_show_auto_hibern8_enable", 212, cld->is_auto_enabled);
 return snprintf(buf, PAGE_SIZE, "%d\n", cld->is_auto_enabled);
}
ssize_t os44_ufscld_sysfs_show_work_state(struct os44_cld_view *cld, char *buf)
{
 return snprintf(buf, PAGE_SIZE, "%d\n", delayed_work_pending(&cld->idle_work) ? 1 : 0);
}
ssize_t os44_ufscld_sysfs_store_trigger(struct os44_cld_view *cld, const char *buf, size_t count)
{
 unsigned long value = 0;
 if (kstrtoul(buf, 0, &value) || value > 1) return -EINVAL;
 printk(KERN_INFO "%s:%d info: cld_trigger %lu\n", "ufscld_sysfs_store_trigger", 38, value);
 if (value == (u32)cld->trigger) return count;
 if (value) os44_ufscld_trigger_on(cld);
 else os44_ufscld_trigger_off(cld);
 return count;
}
ssize_t os44_ufscld_sysfs_store_trigger_interval(struct os44_cld_view *cld, const char *buf, size_t count)
{
 unsigned long value = 0;
 if (kstrtoul(buf, 0, &value)) return -EINVAL;
 if (value < 100 || value > 10000) {
  printk(KERN_INFO "%s:%d info: cld_trigger_interval (min) %4dms ~ (max) %4dms\n", "ufscld_sysfs_store_trigger_interval", 72, 100, 10000);
  return -EINVAL;
 }
 cld->interval_ms = value;
 printk(KERN_INFO "%s:%d info: cld_trigger_interval %d\n", "ufscld_sysfs_store_trigger_interval", 77, cld->interval_ms);
 return count;
}
ssize_t os44_ufscld_sysfs_store_debug(struct os44_cld_view *cld, const char *buf, size_t count)
{
 unsigned long value = 0;
 if (kstrtoul(buf, 0, &value) || value > 1) return -EINVAL;
 cld->debug = value;
 printk(KERN_INFO "%s:%d info: debug %d\n", "ufscld_sysfs_store_debug", 161, cld->debug);
 return count;
}
ssize_t os44_ufscld_sysfs_store_block_suspend(struct os44_cld_view *cld, const char *buf, size_t count)
{
 unsigned long value = 0;
 if (kstrtoul(buf, 0, &value) || value > 1) return -EINVAL;
 printk(KERN_INFO "%s:%d info: cld_block_suspend %d, val %lu\n", "ufscld_sysfs_store_block_suspend", 194, cld->block_suspend, value);
 if (value == cld->block_suspend) return count;
 if (value) os44_ufscld_block_enter_suspend(cld);
 else os44_ufscld_allow_enter_suspend(cld);
 cld->block_suspend = !!value;
 return count;
}
ssize_t os44_ufscld_sysfs_store_auto_hibern8_enable(struct os44_cld_view *cld, const char *buf, size_t count)
{
 unsigned long value = 0;
 if (kstrtoul(buf, 0, &value) || value > 1) return -EINVAL;
 os44_ufscld_auto_hibern8_enable(cld, value);
 return count;
}
