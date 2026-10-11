/* SPDX-License-Identifier: GPL-2.0-only */
/* Shared reconstructed core/platform contract; signature follows the actual
 * core definition and frozen public ufshcd_init declaration. */
#ifndef OS44_CORE_INIT_H
#define OS44_CORE_INIT_H
#include <ufs/ufshcd.h>
int os44_ufshcd_init(struct ufs_hba *hba, void __iomem *mmio_base,
                    unsigned int irq);
#endif
