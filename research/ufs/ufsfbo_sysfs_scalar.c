/* SPDX-License-Identifier: GPL-2.0-only */
/* Original callback semantics; source-owned shared private layout. */
#include <linux/sysfs.h>
#include "os44_fbo_sysfs.h"

ssize_t os44_ufsfbo_sysfs_show_fbo_rec_lrs(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:fbo_rec_lrs (%d)\n", "ufsfbo_sysfs_show_fbo_rec_lrs", 38, fbo->recommended_lrs);
 return sysfs_emit(buf, "%d\n", fbo->recommended_lrs);
}

ssize_t os44_ufsfbo_sysfs_show_fbo_max_lrs(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:fbo_max_lrs (%d)\n", "ufsfbo_sysfs_show_fbo_max_lrs", 45, fbo->maximum_lrs);
 return sysfs_emit(buf, "%d\n", fbo->maximum_lrs);
}

ssize_t os44_ufsfbo_sysfs_show_fbo_min_lrs(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:fbo_min_lrs (%d)\n", "ufsfbo_sysfs_show_fbo_min_lrs", 52, fbo->minimum_lrs);
 return sysfs_emit(buf, "%d\n", fbo->minimum_lrs);
}

ssize_t os44_ufsfbo_sysfs_show_fbo_max_lrc(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:fbo_max_lrc (%d)\n", "ufsfbo_sysfs_show_fbo_max_lrc", 59, fbo->maximum_lrc);
 return sysfs_emit(buf, "%d\n", fbo->maximum_lrc);
}

ssize_t os44_ufsfbo_sysfs_show_fbo_lra(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:fbo_lra (%d)\n", "ufsfbo_sysfs_show_fbo_lra", 66, fbo->lra);
 return sysfs_emit(buf, "%d\n", fbo->lra);
}

ssize_t os44_ufsfbo_sysfs_show_fbo_support(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:fbo_support (%d)\n", "ufsfbo_sysfs_show_fbo_support", 24, fbo->supported);
 return sysfs_emit(buf, "%d\n", fbo->supported);
}

ssize_t os44_ufsfbo_sysfs_show_fbo_version(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:fbo_version (%04x)\n", "ufsfbo_sysfs_show_fbo_version", 31, fbo->version);
 return sysfs_emit(buf, "%04x\n", fbo->version);
}

ssize_t os44_ufsfbo_sysfs_show_is_ufs31(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:is_ufs31:%d\n", "ufsfbo_sysfs_show_is_ufs31", 684, fbo->is_ufs31);
 return sysfs_emit(buf, "%d\n", fbo->is_ufs31);
}

ssize_t os44_ufsfbo_sysfs_show_debug(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:Debug:%d\n", "ufsfbo_sysfs_show_debug", 281, fbo->debug);
 return sysfs_emit(buf, "%d\n", fbo->debug);
}

ssize_t os44_ufsfbo_sysfs_show_block_suspend(struct os44_fbo_lifetime *fbo, char *buf)
{
 if (fbo->debug == 1)
  printk("\0016[info]%s:%d:Block suspend:%d\n", "ufsfbo_sysfs_show_block_suspend", 307, fbo->block_suspend);
 return sysfs_emit(buf, "%d\n", fbo->block_suspend);
}
