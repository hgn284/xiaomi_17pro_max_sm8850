/* SPDX-License-Identifier: GPL-2.0-only */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs
#if !defined(OS44_HIBERN8_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define OS44_HIBERN8_TRACE_H
#include <linux/tracepoint.h>
DECLARE_EVENT_CLASS(ufshcd_profiling_template,
	TP_PROTO(struct ufs_hba *hba, const char *profile_info, s64 time_us,
		 int err),

	TP_ARGS(hba, profile_info, time_us, err),

	TP_STRUCT__entry(
		__field(struct ufs_hba *, hba)
		__string(profile_info, profile_info)
		__field(s64, time_us)
		__field(int, err)
	),

	TP_fast_assign(
		__entry->hba = hba;
		__assign_str(profile_info);
		__entry->time_us = time_us;
		__entry->err = err;
	),

	TP_printk("%s: %s: took %lld usecs, err %d",
		dev_name(__entry->hba->dev), __get_str(profile_info),
		__entry->time_us, __entry->err)
);

DEFINE_EVENT(ufshcd_profiling_template, ufshcd_profile_hibern8,
	TP_PROTO(struct ufs_hba *hba, const char *profile_info, s64 time_us,
		 int err),
	TP_ARGS(hba, profile_info, time_us, err));

#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE os44_hibern8_trace
#include <trace/define_trace.h>
