/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole stock monitor controls reconstructed from run068 owner_boundary.
 * No new owner/global, no locking/validation silently added to stock behavior.
 * Source-pair private view only; stock KCFI is independently reported.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include "os44_ufsmonitor.h"
ssize_t os44_ufsmonitor_sysfs_show_monitor_enable(struct os44_ufsmonitor *m, char *buf)
{ return sysfs_emit(buf, "%s\n", m->stats.enabled != 0 ? "true" : "false"); }
ssize_t os44_ufsmonitor_sysfs_store_monitor_enable(struct os44_ufsmonitor *m, const char *buf, size_t count)
{
 unsigned long long val=0;
 if (kstrtoull(buf,0,&val)) return -EINVAL;
 val=!!val;
 if (val) { m->stats.enabled=true; pr_err("monitor_enable is set to true\n"); }
 else {
  m->stats.enabled=false;
  memset(m->stats.normal,0,sizeof(m->stats.normal));
  memset(m->stats.slow,0,sizeof(m->stats.slow));
  WRITE_ONCE(((u64 *)m->stats.cumulative)[0],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[1],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[2],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[3],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[4],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[5],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[6],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[7],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[8],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[9],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[10],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[11],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[12],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[13],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[14],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[15],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[16],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[17],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[18],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[19],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[20],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[21],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[22],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[23],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[24],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[25],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[26],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[27],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[28],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[29],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[29],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[28],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[27],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[26],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[25],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[24],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[23],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[22],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[21],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[20],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[19],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[18],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[17],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[16],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[15],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[14],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[13],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[12],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[11],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[10],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[9],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[8],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[7],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[6],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[5],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[4],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[3],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[2],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[1],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative_slow)[0],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[29],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[28],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[27],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[26],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[25],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[24],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[23],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[22],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[21],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[20],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[19],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[18],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[17],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[16],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[15],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[14],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[13],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[12],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[11],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[10],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[9],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[8],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[7],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[6],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[5],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[4],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[3],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[2],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[1],0);
  WRITE_ONCE(((u64 *)m->stats.histogram)[0],0);
  pr_err("monitor_enable is set to false\n");
 }
 return count;
}
ssize_t os44_ufsmonitor_sysfs_show_monitor_latency_read_threshold_us(struct os44_ufsmonitor *m, char *buf)
{
 ssize_t len=0; int i;
 for (i=0;i<5;i++) len+=sysfs_emit_at(buf,len,"%lu%s",(unsigned long)m->stats.slow_us[0][i],i==4 ? "\n" : ",");
 return len;
}
ssize_t os44_ufsmonitor_sysfs_store_monitor_latency_read_threshold_us(struct os44_ufsmonitor *m, const char *buf, size_t count)
{
 char *copy=kstrdup(buf,GFP_KERNEL), *cursor=copy, *token;
 int i,ret;
 if (!copy) return -ENOMEM;
 unsigned long long values[5]={0};
 if (m->stats.enabled & 1) {
  pr_err("ufs_monitor monitor is enable, store_monitor_latency_read_threshold_ns failed!\n");
  ret=-EINVAL; goto out;
 }
 for(i=0;(token=strsep(&cursor,",")) != NULL && i<5;i++) {
  if (!*token || kstrtoull(token,10,&values[i])) { ret=-EINVAL; goto out; }
 }
 if (i<5) pr_warn("ufs_monitor only %d thresholds provided, remaining set to 0\n",i);
 for(i=0;i<5;i++) m->stats.slow_us[0][i]=values[i];
 ret=count;
out: kfree(copy); return ret;
}
ssize_t os44_ufsmonitor_sysfs_show_monitor_latency_write_threshold_us(struct os44_ufsmonitor *m, char *buf)
{
 ssize_t len=0; int i;
 for (i=0;i<5;i++) len+=sysfs_emit_at(buf,len,"%lu%s",(unsigned long)m->stats.slow_us[1][i],i==4 ? "\n" : ",");
 return len;
}
ssize_t os44_ufsmonitor_sysfs_store_monitor_latency_write_threshold_us(struct os44_ufsmonitor *m, const char *buf, size_t count)
{
 char *copy=kstrdup(buf,GFP_KERNEL), *cursor=copy, *token;
 int i,ret;
 if (!copy) return -ENOMEM;
 unsigned long long values[5]={0};
 if (m->stats.enabled & 1) {
  pr_err("ufs_monitor monitor is enable, store_monitor_latency_write_threshold_ns failed!\n");
  ret=-EINVAL; goto out;
 }
 for(i=0;(token=strsep(&cursor,",")) != NULL && i<5;i++) {
  if (!*token || kstrtoull(token,10,&values[i])) { ret=-EINVAL; goto out; }
 }
 if (i<5) pr_warn("ufs_monitor only %d thresholds provided, remaining set to 0\n",i);
 for(i=0;i<5;i++) m->stats.slow_us[1][i]=values[i];
 ret=count;
out: kfree(copy); return ret;
}
ssize_t os44_ufsmonitor_sysfs_show_monitor_window_period_s(struct os44_ufsmonitor *m, char *buf)
{ return sysfs_emit(buf,"%lu\n",(unsigned long)(m->stats.interval/1000000000UL)); }
ssize_t os44_ufsmonitor_sysfs_store_monitor_window_period_s(struct os44_ufsmonitor *m, const char *buf, size_t count)
{
 unsigned long long val=0;
 if (kstrtoull(buf,10,&val)) return -EINVAL;
 if (m->stats.enabled == 1)
  pr_err("ufs_monitor monitor is disable, store_monitor_window_period_s failed!\n");
 else m->stats.interval=val*1000000000UL;
 return count;
}
