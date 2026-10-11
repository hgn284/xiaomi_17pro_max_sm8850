/* SPDX-License-Identifier: GPL-2.0-only */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs
#if !defined(OS44_EXCEPTION_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define OS44_EXCEPTION_TRACE_H
#include <linux/tracepoint.h>
TRACE_EVENT(ufshcd_exception_event,

	TP_PROTO(struct ufs_hba *hba, u16 status),

	TP_ARGS(hba, status),

	TP_STRUCT__entry(
		__field(struct ufs_hba *, hba)
		__field(u16, status)
	),

	TP_fast_assign(
		__entry->hba = hba;
		__entry->status = status;
	),

	TP_printk("%s: status 0x%x",
		dev_name(__entry->hba->dev), __entry->status
	)
);

#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE os44_exception_trace
#include <trace/define_trace.h>
