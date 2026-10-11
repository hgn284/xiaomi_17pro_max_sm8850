/* SPDX-License-Identifier: GPL-2.0-only */
/* Full-function reconstruction from OS44 ufs-qcom, not original private types. */
#include <linux/slab.h>
#include <linux/unaligned.h>
#include "os44_xiaomi_owner.h"
#include "os44_fbo_query.h"

int os44_ufsfbo_get_fbo_desc_info(struct os44_fbo_lifetime *fbo)
{
 struct ufs_hba *hba = fbo->hba;
 u8 *desc = kmalloc(255, GFP_KERNEL);
 int ret;
 if (!desc) { ret = -ENOMEM; goto out; }
 ret = os44_ufsfbo_read_desc(hba, 10, desc);
 if (ret) {
  printk(KERN_ERR "[err]%s:%d:Failed reading FBO Desc. ret(%d)\n",
         "ufsfbo_get_fbo_desc_info", 929, ret);
  goto out;
 }
 fbo->recommended_lrs = get_unaligned_be32(desc + 3);
 fbo->maximum_lrs = get_unaligned_be32(desc + 7);
 fbo->minimum_lrs = get_unaligned_be32(desc + 11);
 fbo->version = get_unaligned_be16(desc + 1);
 fbo->maximum_lrc = desc[15];
 fbo->lra = get_unaligned_be16(desc + 16);
out:
 kfree(desc);
 return ret;
}

int os44_ufsfbo_get_fbo_support_state(struct os44_fbo_lifetime *fbo)
{
 struct ufs_hba *hba = fbo->hba;
 u32 mask = fbo->vendor_selector == 0 ? 0x400 :
            fbo->vendor_selector == 1 ? 0x40000000 :
            fbo->vendor_selector == 2 ? 0xffffffff : 0;
 u32 features;
 u8 *desc;
 int ret;
 if (fbo->debug == 1)
  printk(KERN_INFO "[info]%s:%d:Support Mask(0x%x)\n",
         "ufsfbo_get_fbo_support_state", 960, mask);
 desc = kmalloc(255, GFP_KERNEL);
 if (!desc) { ret = -ENOMEM; goto out; }
 ret = os44_ufsfbo_read_desc(hba, 0, desc);
 if (ret) {
  printk(KERN_ERR "[err]%s:%d:Failed reading Device Desc. ret(%d)\n",
         "ufsfbo_get_fbo_support_state", 970, ret);
  goto out;
 }
 features = get_unaligned_be32(desc + 0x4f);
 if (hba->dev_info.wspecversion == 0x310) {
  fbo->is_ufs31 = 1;
  if (!strncmp(fbo->model, "HN8T05FEHKX073", 14)) {
   fbo->is_ufs31 = 0;
   if (!(features & BIT(17))) {
    printk(KERN_ERR "[err]%s:%d:dExtendedUFSFeaturesSupport: SK Hynix UFS31 V8 FBO not support\n",
           "ufsfbo_get_fbo_support_state", 983);
    goto unsupported;
   }
  } else if (!(features & mask)) {
   printk(KERN_ERR "[err]%s:%d:dExtendedUFSFeaturesSupport: UFS31 FBO not support\n",
          "ufsfbo_get_fbo_support_state", 989);
   goto unsupported;
  }
 } else if (hba->dev_info.wspecversion > 0x310) {
  if (!(features & BIT(17))) {
   printk(KERN_ERR "[err]%s:%d:dExtendedUFSFeaturesSupport: UFS41 FBO not support\n",
          "ufsfbo_get_fbo_support_state", 996);
   goto unsupported;
  }
 } else {
  printk(KERN_ERR "[err]%s:%d:dExtendedUFSFeaturesSupport: FBO not support\n",
         "ufsfbo_get_fbo_support_state", 1001);
  goto unsupported;
 }
 fbo->supported = 1;
 if (fbo->debug == 1)
  printk(KERN_INFO "[info]%s:%d:dExtendedUFSFeaturesSupport: FBO support\n",
         "ufsfbo_get_fbo_support_state", 1006);
 ret = 0;
 goto out;
unsupported:
 ret = -ENOTSUPP;
out:
 kfree(desc);
 return ret;
}
