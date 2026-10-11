/* SPDX-License-Identifier: GPL-2.0-only */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs
#if !defined(OS44_UIC_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define OS44_UIC_TRACE_H
#include <ufs/ufs.h>
#include <linux/tracepoint.h>
#define UFS_CMD_TRACE_STRINGS					\
	EM(UFS_CMD_SEND,	"send_req")			\
	EM(UFS_CMD_COMP,	"complete_rsp")			\
	EM(UFS_DEV_COMP,	"dev_complete")			\
	EM(UFS_QUERY_SEND,	"query_send")			\
	EM(UFS_QUERY_COMP,	"query_complete")		\
	EM(UFS_QUERY_ERR,	"query_complete_err")		\
	EM(UFS_TM_SEND,		"tm_send")			\
	EM(UFS_TM_COMP,		"tm_complete")			\
	EMe(UFS_TM_ERR,		"tm_complete_err")

#undef EM
#undef EMe
#define EM(a,b) TRACE_DEFINE_ENUM(a);
#define EMe(a,b) TRACE_DEFINE_ENUM(a);
UFS_CMD_TRACE_STRINGS
#undef EM
#undef EMe
#define EM(a,b) {a,b},
#define EMe(a,b) {a,b}
#define show_ufs_cmd_trace_str(str_t) __print_symbolic(str_t, UFS_CMD_TRACE_STRINGS)
TRACE_EVENT(ufshcd_uic_command,
	TP_PROTO(struct ufs_hba *hba, enum ufs_trace_str_t str_t, u32 cmd,
		 u32 arg1, u32 arg2, u32 arg3),

	TP_ARGS(hba, str_t, cmd, arg1, arg2, arg3),

	TP_STRUCT__entry(
		__field(struct ufs_hba *, hba)
		__field(enum ufs_trace_str_t, str_t)
		__field(u32, cmd)
		__field(u32, arg1)
		__field(u32, arg2)
		__field(u32, arg3)
	),

	TP_fast_assign(
		__entry->hba = hba;
		__entry->str_t = str_t;
		__entry->cmd = cmd;
		__entry->arg1 = arg1;
		__entry->arg2 = arg2;
		__entry->arg3 = arg3;
	),

	TP_printk(
		"%s: %s: cmd: 0x%x, arg1: 0x%x, arg2: 0x%x, arg3: 0x%x",
		show_ufs_cmd_trace_str(__entry->str_t), dev_name(__entry->hba->dev),
		__entry->cmd, __entry->arg1, __entry->arg2, __entry->arg3
	)
);
#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE os44_uic_trace
#include <trace/define_trace.h>
