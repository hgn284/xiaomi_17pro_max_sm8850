/* SPDX-License-Identifier: GPL-2.0-only */
/* Original whole-driver inline iterator over the existing, shared tracepoint.
 * Uses real callback types; does not define another tracepoint/registry. */
#include "os44_clock_trace.h"
static inline void local_profile_clk_scaling(struct ufs_hba *hba,
 const char *profile_info, s64 time_us, int err)
{
 struct tracepoint_func *it;
 void (*probe)(void *, struct ufs_hba *, const char *, s64, int);
 if (static_key_false(&__tracepoint_ufshcd_profile_clk_scaling.key)) {
  if (!cpu_online(raw_smp_processor_id())) return;
  preempt_disable_notrace();
  it=rcu_dereference_raw(__tracepoint_ufshcd_profile_clk_scaling.funcs);
  if (it) do {
   probe=READ_ONCE(it->func);
   probe(it->data,hba,profile_info,time_us,err);
  } while ((++it)->func);
  preempt_enable_notrace();
 }
}
