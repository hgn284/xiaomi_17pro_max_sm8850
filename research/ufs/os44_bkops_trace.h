/* SPDX-License-Identifier: GPL-2.0-only */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs
#if !defined(OS44_BKOPS_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define OS44_BKOPS_TRACE_H
#include <linux/tracepoint.h>
TRACE_EVENT(ufshcd_auto_bkops_state,

	TP_PROTO(struct ufs_hba *hba, const char *state),

	TP_ARGS(hba, state),

	TP_STRUCT__entry(
		__field(struct ufs_hba *, hba)
		__string(state, state)
	),

	TP_fast_assign(
		__entry->hba = hba;
		__assign_str(state);
	),

	TP_printk("%s: auto bkops - %s",
		dev_name(__entry->hba->dev), __get_str(state))
);
#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE os44_bkops_trace
#include <trace/define_trace.h>
