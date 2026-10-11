/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction from complete stock function + exact policy data. */
#include "os44_fastdiscard_contract.h"
#include <linux/string.h>
#include <linux/printk.h>
struct os44_fastdiscard_policy_entry { char vendor[9]; char model[18]; char revision[5]; u32 capacity; };
static_assert(sizeof(struct os44_fastdiscard_policy_entry)==36);
static struct os44_fastdiscard_policy_entry policy[] = {
 { "SKhynix", "HN8T171EJKX151", "", 256 },
 { "SKhynix", "HN8T271EJKX152", "", 512 },
 { "SKhynix", "HN8T371ZJKX154", "", 1024 },
 { "SAMSUNG", "KLUEG4RHKF-F0H1", "", 256 },
 { "SAMSUNG", "KLUFG8RHKF-F0H1", "", 512 },
 { "SAMSUNG", "KLUGG8NHKB-F0H1", "", 1024 },
 { "MICRON", "MT256GBGBB2U41", "", 256 },
 { "MICRON", "MT512GBGBB4U41", "", 512 },
 { "XBSTOR", "XBUSC1B5AAATH3", "", 512 },
};
int os44_check_fastdiscard_policy(struct ufs_hba *hba)
{
 struct os44_fastdiscard_oem_view *owner = (void *)hba->android_oem_data1;
 struct os44_fastdiscard_state_view *state;
 char inquiry[28];
 int i;
 if (!owner)
  return -EOPNOTSUPP;
 if (!hba->ufs_device_wlun) {
  printk(KERN_ERR "ufs_device_wlun init fail, maybe UFS had issues before this.");
  return -EOPNOTSUPP;
 }
 state = owner->fastdiscard;
 memcpy(inquiry, hba->ufs_device_wlun->inquiry + 8, sizeof(inquiry));
 for (i = 0; i < ARRAY_SIZE(policy); i++) {
  if (strncmp(inquiry, policy[i].vendor, strlen(policy[i].vendor)))
   continue;
  if (strncmp(inquiry + 8, policy[i].model, strlen(policy[i].model)))
   continue;
  if (strncmp(inquiry + 24, policy[i].revision, strlen(policy[i].revision)))
   continue;
  printk(KERN_ERR "[FASTDISCARD debug]policy match[%d]\n", i + 1);
  state->enabled = 1;
  printk(KERN_INFO "[FASTDISCARD debug] fastdiscard_enable=[%d]\n", state->enabled);
  return 0;
 }
 return -EOPNOTSUPP;
}
