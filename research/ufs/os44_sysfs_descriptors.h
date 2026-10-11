#ifndef OS44_SYSFS_DESCRIPTORS_H
#define OS44_SYSFS_DESCRIPTORS_H
#include <ufs/ufshcd.h>
ssize_t os44_sysfs_read_desc_param(struct ufs_hba *, enum desc_idn, u8, u8, u8 *, u8);
extern const struct attribute_group ufs_sysfs_device_descriptor_group;
extern const struct attribute_group ufs_sysfs_interconnect_descriptor_group;
extern const struct attribute_group ufs_sysfs_geometry_descriptor_group;
extern const struct attribute_group ufs_sysfs_health_descriptor_group;
extern const struct attribute_group ufs_sysfs_power_descriptor_group;
#endif
