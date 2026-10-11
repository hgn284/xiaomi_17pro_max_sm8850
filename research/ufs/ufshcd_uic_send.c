/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent source; real UIC command, MMIO, trace and configurable timeout. */
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include "ufsswap_provider_reconstruction.h"
#include "os44_uic_contract.h"
#include "drivers/ufs/core/ufshcd-priv.h"
#include <trace/hooks/ufshcd.h>
#define CREATE_TRACE_POINTS
#include "os44_uic_trace.h"
#define UIC_CMD_TIMEOUT_DEFAULT 500
#define UIC_CMD_TIMEOUT_MAX 5000
unsigned int os44_uic_cmd_timeout = 5000; /* stock .data 88 13 00 00 */

static int uic_cmd_timeout_set(const char *val, const struct kernel_param *kp)
{
	return param_set_uint_minmax(val, kp, UIC_CMD_TIMEOUT_DEFAULT,
				     UIC_CMD_TIMEOUT_MAX);
}

static const struct kernel_param_ops uic_cmd_timeout_ops = {
	.set = uic_cmd_timeout_set,
	.get = param_get_uint,
};

module_param_cb(uic_cmd_timeout, &uic_cmd_timeout_ops, &os44_uic_cmd_timeout, 0644);
MODULE_PARM_DESC(uic_cmd_timeout,
		 "UFS UIC command timeout in milliseconds. Defaults to 500ms. Supported values range from 500ms to 5 seconds inclusively");

void os44_ufshcd_add_uic_command_trace(struct ufs_hba *hba,
					 const struct uic_command *ucmd,
					 enum ufs_trace_str_t str_t)
{
	u32 cmd, arg1, arg2, arg3;

	trace_android_vh_ufs_send_uic_command(hba, ucmd, (int)str_t);

	if (!trace_ufshcd_uic_command_enabled())
		return;

	if (str_t == UFS_CMD_SEND) {
		cmd = ucmd->command;
		arg1 = ucmd->argument1;
		arg2 = ucmd->argument2;
		arg3 = ucmd->argument3;
	} else {
		cmd = ufshcd_readl(hba, REG_UIC_COMMAND);
		arg1 = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_1);
		arg2 = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2);
		arg3 = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);
	}

	trace_ufshcd_uic_command(hba, str_t, cmd, arg1, arg2, arg3);
}

static inline bool ufshcd_ready_for_uic_cmd(struct ufs_hba *hba)
{
	u32 val;
	int ret = read_poll_timeout(ufshcd_readl, val, val & UIC_COMMAND_READY,
				    500, os44_uic_cmd_timeout * 1000, false, hba,
				    REG_CONTROLLER_STATUS);
	return ret == 0;
}

static void
__ufshcd_dispatch_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	ufshcd_writel(hba, uic_cmd->argument1, REG_UIC_COMMAND_ARG_1);
	ufshcd_writel(hba, uic_cmd->argument2, REG_UIC_COMMAND_ARG_2);
	ufshcd_writel(hba, uic_cmd->argument3, REG_UIC_COMMAND_ARG_3);
	ufshcd_writel(hba, uic_cmd->command & COMMAND_OPCODE_MASK,
		      REG_UIC_COMMAND);
}

static inline void
ufshcd_dispatch_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	lockdep_assert_held(&hba->uic_cmd_mutex);

	WARN_ON(hba->active_uic_cmd);
	WARN_ON_ONCE(uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT);

	hba->active_uic_cmd = uic_cmd;

	os44_ufshcd_add_uic_command_trace(hba, uic_cmd, UFS_CMD_SEND);

	if (hba->android_quirks & UFSHCD_ANDROID_QUIRK_AH8_BREAKS_DME) {
		unsigned long flags;

		local_irq_save(flags);
		preempt_disable();
		__ufshcd_dispatch_uic_cmd(hba, uic_cmd);
		preempt_enable();
		local_irq_restore(flags);
	} else {
		__ufshcd_dispatch_uic_cmd(hba, uic_cmd);
	}
}

int os44___ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	lockdep_assert_held(&hba->uic_cmd_mutex);

	if (!ufshcd_ready_for_uic_cmd(hba)) {
		dev_err(hba->dev,
			"Controller not ready to accept UIC commands\n");
		return -EIO;
	}

	init_completion(&uic_cmd->done);

	uic_cmd->cmd_active = 1;
	ufshcd_dispatch_uic_cmd(hba, uic_cmd);

	return 0;
}
