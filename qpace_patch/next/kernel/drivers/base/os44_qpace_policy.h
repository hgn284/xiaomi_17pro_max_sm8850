/* SPDX-License-Identifier: GPL-2.0-only */
/* Private to drivers/base/core.c; no vendor-private layout or text patching. */
#include <linux/os44_qpace.h>
static DEFINE_MUTEX(os44_qpace_policy_lock);
static bool os44_qpace_policy_applies(struct device *dev, struct attribute *attr)
{
 const char *name = dev_name(dev), *p;
 if (!IS_ENABLED(CONFIG_OS44_QPACE) || !dev->class ||
     strcmp(dev->class->name, "block") || strncmp(name, "zram", 4))
  return false;
 p = name + 4;
 if (!*p) return false;
 for (; *p; p++) if (*p < '0' || *p > '9') return false;
 return !strcmp(attr->name, "comp_algorithm") ||
        !strcmp(attr->name, "disksize") || !strcmp(attr->name, "reset");
}
static int os44_qpace_policy_prepare(struct device *dev, struct attribute *attr,
                                    const char **buf, size_t *count)
{
#if IS_ENABLED(CONFIG_OS44_QPACE)
 static const char codec[] = "qpace-hgn284\n";
 const struct attribute_group **groups;
 struct attribute **attributes;
 struct device_attribute *algorithm = NULL;
 ssize_t ret;
 if (!strcmp(attr->name, "comp_algorithm")) {
  *buf = codec;
  *count = sizeof(codec) - 1;
  return 0;
 }
 if (strcmp(attr->name, "disksize")) return 0;
 /* device_add_disk publishes the original driver's attribute groups here.
  * The ongoing sysfs device callback pins the device/driver lifetime. */
 for (groups = dev->groups; groups && *groups; groups++) {
  if ((*groups)->name || !(*groups)->attrs) continue;
  for (attributes = (*groups)->attrs; *attributes; attributes++) {
   if (!strcmp((*attributes)->name, "comp_algorithm")) {
    algorithm = container_of(*attributes, struct device_attribute, attr);
    break;
   }
  }
  if (algorithm) break;
 }
 if (!algorithm || !algorithm->store) return -EOPNOTSUPP;
 ret = algorithm->store(dev, algorithm, codec, sizeof(codec) - 1);
 if (ret < 0) return ret;
 ret = os44_qpace_wait_ready();
 if (ret) return ret;
 dev_info(dev, "OS44 forced qpace-hgn284 native hardware backend\n");
#endif
 return 0;
}
