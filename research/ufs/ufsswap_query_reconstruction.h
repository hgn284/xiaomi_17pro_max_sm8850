#ifndef OS44_QUERY_H
#define OS44_QUERY_H
#include "ufsswap_range_reconstruction.h"
#ifdef __KERNEL__
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <scsi/scsi_device.h>
#include <ufs/ufshcd.h>
#else
/* Standalone instruction-test layouts only; actual Kbuild uses Linux types. */
struct dev_pm_info { unsigned char opaque[0xd8]; unsigned int usage_count; };
struct device { unsigned char opaque[0x108]; struct dev_pm_info power; };
struct scsi_device { unsigned char opaque[0xf0]; unsigned char *inquiry; unsigned char pad_f8[0xd8]; struct device sdev_gendev; };
struct ufs_dev_info { unsigned char opaque[4]; unsigned short wmanufacturerid; unsigned char pad6[74]; };
struct ufs_hba { unsigned char opaque[0x48]; struct scsi_device *ufs_device_wlun; unsigned char pad50[0x9a0]; struct ufs_dev_info dev_info; };
extern int __pm_runtime_resume(struct device *, int);
static inline void pm_runtime_put_noidle(struct device *dev) {
 unsigned int *p=&dev->power.usage_count, old=__atomic_load_n(p,__ATOMIC_RELAXED);
 while(old && !__atomic_compare_exchange_n(p,&old,old-1,0,__ATOMIC_SEQ_CST,__ATOMIC_RELAXED)) {}
}
#endif
#ifdef __KERNEL__
static inline void *os44_descriptor_alloc(void) { return kmalloc(255,GFP_KERNEL); }
static inline void os44_descriptor_free(void *p) { kfree(p); }
#else
extern void *os44_descriptor_alloc(void);
extern void os44_descriptor_free(void *);
#endif
extern int os44_query_descriptor(struct ufs_hba *,unsigned int,unsigned char,unsigned char,unsigned char,unsigned char *,int *);
/* Reconstruction-owned service boundary; unresolved until whole provider paired. */
extern int os44_query_attr(struct ufs_hba *, unsigned int, unsigned char, unsigned char, unsigned char, os44_u32 *);
extern void os44_query_error(unsigned int, unsigned int, int, unsigned int);
extern int os44_get_swap_descriptor(struct os44_swap_view *);
extern int os44_flush_priority(struct os44_swap_view *);
int os44_ufsswap_get_dev_info(struct ufs_hba *,struct os44_swap_view *);
int os44_ufsswap_select_model(struct os44_swap_view *,const unsigned char [28]);
/* Paired-source signatures; not asserted to be stock private C types. */
int os44_ufsswap_get_wb_buf(struct ufs_hba *);
int os44_ufsswap_support_tp(struct os44_swap_view *);
int os44_ufsswap_config_buf(struct ufs_hba *,struct os44_swap_view *);
int os44_ufsswap_min_buf(struct ufs_hba *,struct os44_swap_view *);
int os44_ufsswap_probe(struct ufs_hba *,struct os44_swap_view *);
extern int os44_ufsswap_create_sysfs(struct os44_swap_view *);
#endif
