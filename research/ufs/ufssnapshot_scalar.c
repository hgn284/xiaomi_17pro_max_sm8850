// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include "os44_snapshot.h"
#include "os44_xiaomi_owner.h"
ssize_t os44_ufssnapshot_sysfs_show_dropfreesize(struct os44_snapshot_view *s, char *buf)
{ return sysfs_emit(buf, "%u\n", s->drop_freesize); }
ssize_t os44_ufssnapshot_sysfs_show_debug(struct os44_snapshot_view *s, char *buf)
{ return sysfs_emit(buf, "%d\n", s->debug); }
ssize_t os44_ufssnapshot_sysfs_show_support(struct os44_snapshot_view *s, char *buf)
{ return sysfs_emit(buf, "%s\n", s->supported ? "true" : "false"); }
ssize_t os44_ufssnapshot_sysfs_show_version(struct os44_snapshot_view *s, char *buf)
{ return sysfs_emit(buf, "%04x\n", s->version); }
ssize_t os44_ufssnapshot_sysfs_show_eecount(struct os44_snapshot_view *s, char *buf)
{ return sysfs_emit(buf, "%llu\n", s->ee_count); }
ssize_t os44_ufssnapshot_sysfs_store_dropfreesize(struct os44_snapshot_view *s, const char *buf, size_t count)
{
 unsigned long long value=0;
 if (kstrtoull(buf,0,&value) || value>U32_MAX) return -EINVAL;
 if (os44_ufssnapshot_set_drop_freesize(s,value)) return -EINVAL;
 return count;
}
ssize_t os44_ufssnapshot_sysfs_store_debug(struct os44_snapshot_view *s, const char *buf, size_t count)
{
 unsigned long long value=0;
 struct os44_remove_owner_view *owner=os44_get_ufs_xiaomi_for_remove();
 if (kstrtoull(buf,0,&value) || value>1) return -EINVAL;
 s->debug=value;
 owner->snapshot_v2_debug=value;
 return count;
}
ssize_t os44_ufssnapshot_sysfs_show_backupcancelled(struct os44_snapshot_view *s, char *buf)
{
 u32 value=0;
 if (os44_ufssnapshot_get_bBackupCancelled(s,&value)) return sysfs_emit(buf,"%s\n","invalid");
 return sysfs_emit(buf,"%u\n",value);
}
