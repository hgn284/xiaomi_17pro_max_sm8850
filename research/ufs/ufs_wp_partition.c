/* SPDX-License-Identifier: GPL-2.0-only */
/* Reconstructed from original whole function. No owner storage or fake getter.
 * Preserve original refcount behavior; do not claim lifecycle repair here. */
#include <linux/of.h>
#include <ufs/ufshcd.h>
#include "os44_remove_owner_view.h"
static_assert(offsetof(struct os44_remove_owner_view,swap.partition_start)==576);
static_assert(offsetof(struct os44_remove_owner_view,swap.partition_end)==704);
static_assert(offsetof(struct os44_remove_owner_view,swap.partition_lun)==832);
static_assert(offsetof(struct os44_remove_owner_view,swap.partition_name)==896);
static_assert(offsetof(struct os44_remove_owner_view,swap.partition_count)==1408);
void os44_ufs_wp_getpartation(struct ufs_hba *hba)
{
 struct device_node *memory, *child;
 int i = 0;
 if (!hba) return;
 memory = of_find_node_by_path("/memory");
 if (!memory) {
  pr_err("[UFSWRITEPROTECT] /memory node not found\n");
  return;
 }
 for_each_child_of_node(memory, child) {
  u64 start_lba = 0, end_lba = 0;
  u32 lun = 0;
  const char *name = NULL;
  if (i >= 16) break;
  if (of_property_read_u64(child, "start_lba", &start_lba)) {
   pr_err("[UFSWRITEPROTECT] failed to read start_lba for %s\n", child->name);
   continue;
  }
  if (of_property_read_u64(child, "end_lba", &end_lba)) {
   pr_err("[UFSWRITEPROTECT] failed to read end_lba for %s\n", child->name);
   continue;
  }
  if (of_property_read_u32(child, "lun", &lun)) {
   pr_warn("[UFSWRITEPROTECT] lun missing for %s, default 0\n", child->name);
   lun = 0;
  }
  if (of_property_read_string(child, "name", &name) || !name) name = child->name;
  strncpy(os44_ufs_xiaomi.swap.partition_name[i], name, 31);
  os44_ufs_xiaomi.swap.partition_name[i][31] = '\0';
  os44_ufs_xiaomi.swap.partition_start[i] = start_lba;
  os44_ufs_xiaomi.swap.partition_end[i] = end_lba;
  os44_ufs_xiaomi.swap.partition_lun[i] = lun;
  pr_info("[UFSWRITEPROTECT] node=%s name=%s start=%llu end=%llu lun=%u\n", child->name, os44_ufs_xiaomi.swap.partition_name[i], start_lba, end_lba, lun);
  i++;
 }
 os44_ufs_xiaomi.swap.partition_count = i;
}
