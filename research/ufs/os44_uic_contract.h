/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_UIC_CONTRACT_H
#define OS44_UIC_CONTRACT_H
#include <ufs/ufshcd.h>
extern unsigned int os44_uic_cmd_timeout;
int os44___ufshcd_send_uic_cmd(struct ufs_hba *, struct uic_command *);
void os44_ufshcd_add_uic_command_trace(struct ufs_hba *, const struct uic_command *, enum ufs_trace_str_t);
int os44_ufshcd_wait_for_uic_cmd(struct ufs_hba *, struct uic_command *);
int os44_ufshcd_send_uic_cmd(struct ufs_hba *, struct uic_command *);
extern bool os44_uicerr_flag;
void os44_ufshcd_print_host_state(struct ufs_hba *);
void os44_ufshcd_print_evt_hist(struct ufs_hba *);
int os44_ufshcd_dump_regs(struct ufs_hba *, size_t, size_t, const char *);
int os44_ufshcd_uic_pwr_ctrl(struct ufs_hba *, struct uic_command *);
int os44_ufshcd_dme_get_attr(struct ufs_hba *, u32, u32 *, u8);
int os44_ufshcd_change_power_mode(struct ufs_hba *, struct ufs_pa_layer_attr *);
int os44_ufshcd_dme_set_attr(struct ufs_hba *, u32, u8, u32, u8);
int os44_ufshcd_uic_change_pwr_mode(struct ufs_hba *, u8);
int os44_ufshcd_dme_configure_adapt(struct ufs_hba *, int, int);
int os44_ufshcd_config_pwr_mode(struct ufs_hba *, struct ufs_pa_layer_attr *);
int os44_ufshcd_uic_hibern8_exit(struct ufs_hba *);
#endif
