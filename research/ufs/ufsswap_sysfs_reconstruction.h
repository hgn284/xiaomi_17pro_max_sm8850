#ifndef OS44_SYSFS_H
#define OS44_SYSFS_H
#include "ufsswap_query_reconstruction.h"
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
/* Source-owned declaration, layout verified against stock instructions/BTF.
 * The owner must pass &ctx->swap to probe and supply the singleton getter. */
struct os44_swap_attribute {
 struct attribute attr;
 long (*show)(struct os44_swap_view *,char *);
 long (*store)(struct os44_swap_view *,const char *,os44_size);
};
struct os44_swap_sysfs {
 struct os44_swap_view swap;
 struct kobject kobj;
 struct mutex lock;
 const struct os44_swap_attribute *attrs;
 os44_u64 partition_start[16],partition_end[16];
 os44_u32 partition_lun[16];
 char partition_name[16][32];
 os44_u32 partition_count;
};
extern struct os44_swap_view *os44_ufsswap_singleton(void);
void os44_ufsswap_remove(struct os44_swap_view *);
#endif
