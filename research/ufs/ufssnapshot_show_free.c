// SPDX-License-Identifier: GPL-2.0-only
#include <linux/sysfs.h>
#include "ufssnapshot_free.h"
ssize_t os44_ufssnapshot_sysfs_show_freespace(struct os44_snapshot_view *s, char *buf)
{
 u32 value = 0;
 int ret;
 switch (s->vendor) {
 case 0: ret = os44_hynix_get_free_space(s, &value); break;
 case 1: ret = os44_micron_get_free_space(s, &value); break;
 case 3: ret = os44_samsung_get_free_space(s, &value); break;
 default: return sysfs_emit(buf, "%s\n", "invalid");
 }
 if (ret) return sysfs_emit(buf, "%s\n", "invalid");
 return sysfs_emit(buf, "%u\n", value);
}
