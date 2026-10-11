#ifndef OS44_SYSFS_HELPERS_H
#define OS44_SYSFS_HELPERS_H
#include <ufs/ufshcd.h>
void os44_ufshcd_auto_hibern8_update(struct ufs_hba *, u32);
void os44_ufshcd_pm_qos_exit(struct ufs_hba *);
int os44_ufshcd_read_device_lvl_exception_id(struct ufs_hba *, u64 *);
#endif
