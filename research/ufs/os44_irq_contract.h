/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_IRQ_CONTRACT_H
#define OS44_IRQ_CONTRACT_H
#include <linux/interrupt.h>
#include <ufs/ufshcd.h>
/* Instruction-derived view of hba->priv, NOT Xiaomi singleton or original C type. */
struct os44_irq_private_view {
 u8 before_pa[0xc9c];
 u32 pa_total, pa_bits[5], dl_total, dl_bits[15], dme_total;
};
static_assert(offsetof(struct os44_irq_private_view,pa_total)==0xc9c);
static_assert(offsetof(struct os44_irq_private_view,pa_bits)==0xca0);
static_assert(offsetof(struct os44_irq_private_view,dl_total)==0xcb4);
static_assert(offsetof(struct os44_irq_private_view,dl_bits)==0xcb8);
static_assert(offsetof(struct os44_irq_private_view,dme_total)==0xcf4);
irqreturn_t os44_ufshcd_intr(int, void *);
u32 os44_ufshcd_mcq_read_cqis(struct ufs_hba *, int);
void os44_ufshcd_mcq_write_cqis(struct ufs_hba *, u32, int);
#endif
