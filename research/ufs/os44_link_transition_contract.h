/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_LINK_TRANSITION_H
#define OS44_LINK_TRANSITION_H
#include <ufs/ufshcd.h>
int os44_ufshcd_uic_hibern8_enter(struct ufs_hba *);
int os44_ufshcd_link_state_transition(struct ufs_hba *, enum uic_link_state, bool);
#endif
