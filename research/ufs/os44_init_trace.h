/* SPDX-License-Identifier: GPL-2.0-only */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs
#if !defined(OS44_INIT_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define OS44_INIT_TRACE_H
#include <linux/tracepoint.h>
#define UFS_LINK_STATES						\
	EM(UIC_LINK_OFF_STATE,		"UIC_LINK_OFF_STATE")		\
	EM(UIC_LINK_ACTIVE_STATE,	"UIC_LINK_ACTIVE_STATE")	\
	EMe(UIC_LINK_HIBERN8_STATE,	"UIC_LINK_HIBERN8_STATE")

#define UFS_PWR_MODES							\
	EM(UFS_ACTIVE_PWR_MODE,		"UFS_ACTIVE_PWR_MODE")		\
	EM(UFS_SLEEP_PWR_MODE,		"UFS_SLEEP_PWR_MODE")		\
	EM(UFS_POWERDOWN_PWR_MODE,	"UFS_POWERDOWN_PWR_MODE")	\
	EMe(UFS_DEEPSLEEP_PWR_MODE,	"UFS_DEEPSLEEP_PWR_MODE")

#define UFSCHD_CLK_GATING_STATES				\
	EM(CLKS_OFF,			"CLKS_OFF")		\
	EM(CLKS_ON,			"CLKS_ON")		\
	EM(REQ_CLKS_OFF,		"REQ_CLKS_OFF")		\
	EMe(REQ_CLKS_ON,		"REQ_CLKS_ON")


#undef EM
#undef EMe
#define EM(a,b) TRACE_DEFINE_ENUM(a);
#define EMe(a,b) TRACE_DEFINE_ENUM(a);
UFS_LINK_STATES;
UFS_PWR_MODES;
#undef EM
#undef EMe
#define EM(a,b) {a,b},
#define EMe(a,b) {a,b}
DECLARE_EVENT_CLASS(ufshcd_template,
	TP_PROTO(struct ufs_hba *hba, int err, s64 usecs,
		 int dev_state, int link_state),

	TP_ARGS(hba, err, usecs, dev_state, link_state),

	TP_STRUCT__entry(
		__field(s64, usecs)
		__field(int, err)
		__field(struct ufs_hba *, hba)
		__field(int, dev_state)
		__field(int, link_state)
	),

	TP_fast_assign(
		__entry->usecs = usecs;
		__entry->err = err;
		__entry->hba = hba;
		__entry->dev_state = dev_state;
		__entry->link_state = link_state;
	),

	TP_printk(
		"%s: took %lld usecs, dev_state: %s, link_state: %s, err %d",
		dev_name(__entry->hba->dev),
		__entry->usecs,
		__print_symbolic(__entry->dev_state, UFS_PWR_MODES),
		__print_symbolic(__entry->link_state, UFS_LINK_STATES),
		__entry->err
	)
);

DEFINE_EVENT(ufshcd_template, ufshcd_init,
 TP_PROTO(struct ufs_hba *hba, int err, s64 usecs, int dev_state, int link_state),
 TP_ARGS(hba, err, usecs, dev_state, link_state));
#endif
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE os44_init_trace
#include <trace/define_trace.h>
