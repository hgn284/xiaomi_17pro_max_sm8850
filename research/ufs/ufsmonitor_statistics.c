/* SPDX-License-Identifier: GPL-2.0-only */
/* Instruction-derived whole callbacks; source-pair private type only.
 * Original units, no-lock reads, reset order and misleading window log retained.
 */
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include "os44_ufsmonitor.h"
static const char * const os44_monitor_cmd_type_str[] = {"READ", "WRITE"};
ssize_t os44_ufsmonitor_sysfs_show_monitor_daily_cmd_lat(struct os44_ufsmonitor *m, char *buf)
{
 ssize_t len=0; int cmd;
 len += sysfs_emit_at(buf,len,"Normal:\n");
 len += sysfs_emit_at(buf,len,"Size         Sum(ms)          Count            Peak(us)\n");
 for(cmd=0;cmd<2;cmd++) {
 len += sysfs_emit_at(buf,len,"[%s]\n",os44_monitor_cmd_type_str[cmd]);
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","16", (unsigned long)(m->stats.cumulative[cmd][0].sum/1000000UL), m->stats.cumulative[cmd][0].count, (unsigned long)(m->stats.cumulative[cmd][0].maximum/1000UL));
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","128", (unsigned long)(m->stats.cumulative[cmd][1].sum/1000000UL), m->stats.cumulative[cmd][1].count, (unsigned long)(m->stats.cumulative[cmd][1].maximum/1000UL));
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","256", (unsigned long)(m->stats.cumulative[cmd][2].sum/1000000UL), m->stats.cumulative[cmd][2].count, (unsigned long)(m->stats.cumulative[cmd][2].maximum/1000UL));
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","512", (unsigned long)(m->stats.cumulative[cmd][3].sum/1000000UL), m->stats.cumulative[cmd][3].count, (unsigned long)(m->stats.cumulative[cmd][3].maximum/1000UL));
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","512+", (unsigned long)(m->stats.cumulative[cmd][4].sum/1000000UL), m->stats.cumulative[cmd][4].count, (unsigned long)(m->stats.cumulative[cmd][4].maximum/1000UL));
 }
 len += sysfs_emit_at(buf,len,"\nAbnormal:\n");
 len += sysfs_emit_at(buf,len,"Size         Sum(ms)          Count           Peak(us)      Threshold(us)\n");
 for(cmd=0;cmd<2;cmd++) {
 len += sysfs_emit_at(buf,len,"[%s]\n",os44_monitor_cmd_type_str[cmd]);
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-16u%-14lu%-14lu\n","16", (unsigned long)(m->stats.cumulative_slow[cmd][0].sum/1000000UL), m->stats.cumulative_slow[cmd][0].count, (unsigned long)(m->stats.cumulative_slow[cmd][0].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][0]);
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-16u%-14lu%-14lu\n","128", (unsigned long)(m->stats.cumulative_slow[cmd][1].sum/1000000UL), m->stats.cumulative_slow[cmd][1].count, (unsigned long)(m->stats.cumulative_slow[cmd][1].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][1]);
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-16u%-14lu%-14lu\n","256", (unsigned long)(m->stats.cumulative_slow[cmd][2].sum/1000000UL), m->stats.cumulative_slow[cmd][2].count, (unsigned long)(m->stats.cumulative_slow[cmd][2].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][2]);
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-16u%-14lu%-14lu\n","512", (unsigned long)(m->stats.cumulative_slow[cmd][3].sum/1000000UL), m->stats.cumulative_slow[cmd][3].count, (unsigned long)(m->stats.cumulative_slow[cmd][3].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][3]);
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-16u%-14lu%-14lu\n","512+", (unsigned long)(m->stats.cumulative_slow[cmd][4].sum/1000000UL), m->stats.cumulative_slow[cmd][4].count, (unsigned long)(m->stats.cumulative_slow[cmd][4].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][4]);
 }
 len += sysfs_emit_at(buf,len,"Delay:\n");
 len += sysfs_emit_at(buf,len,"Range(ms)         Sum(ms)          Count            Peak(ms)\n");
 for(cmd=0;cmd<2;cmd++) {
 len += sysfs_emit_at(buf,len,"[%s]\n",os44_monitor_cmd_type_str[cmd]);
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","1", (unsigned long)(m->stats.histogram[cmd][0].sum/1000000UL), m->stats.histogram[cmd][0].count, (unsigned long)(m->stats.histogram[cmd][0].maximum/1000000UL));
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","5", (unsigned long)(m->stats.histogram[cmd][1].sum/1000000UL), m->stats.histogram[cmd][1].count, (unsigned long)(m->stats.histogram[cmd][1].maximum/1000000UL));
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","10", (unsigned long)(m->stats.histogram[cmd][2].sum/1000000UL), m->stats.histogram[cmd][2].count, (unsigned long)(m->stats.histogram[cmd][2].maximum/1000000UL));
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","50", (unsigned long)(m->stats.histogram[cmd][3].sum/1000000UL), m->stats.histogram[cmd][3].count, (unsigned long)(m->stats.histogram[cmd][3].maximum/1000000UL));
 len += sysfs_emit_at(buf,len,"%-12s%-18lu%-18u%-12lu\n","50+", (unsigned long)(m->stats.histogram[cmd][4].sum/1000000UL), m->stats.histogram[cmd][4].count, (unsigned long)(m->stats.histogram[cmd][4].maximum/1000000UL));
 }
 return len;
}
ssize_t os44_ufsmonitor_sysfs_store_monitor_daily_cmd_lat(struct os44_ufsmonitor *m, const char *buf, size_t count)
{
 if (sysfs_streq(buf,"reset")) {
  WRITE_ONCE(((u64 *)m->stats.cumulative)[29],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[28],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[27],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[26],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[25],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[24],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[23],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[22],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[21],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[20],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[19],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[18],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[17],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[16],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[15],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[14],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[13],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[12],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[11],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[10],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[9],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[8],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[7],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[6],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[5],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[4],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[3],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[2],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[1],0);
  WRITE_ONCE(((u64 *)m->stats.cumulative)[0],0);
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
  pr_info("ufs_monitor: daily_stats reset via sysfs\n");
 }
 return count;
}
ssize_t os44_ufsmonitor_sysfs_show_monitor_window_cmd_lat(struct os44_ufsmonitor *m, char *buf)
{
 ssize_t len=0; int cmd, w, bank;
 len += sysfs_emit_at(buf,len,"Normal:\n");
 len += sysfs_emit_at(buf,len,"Window   Size(k)      Sum(ms)          Count            Peak(us)\n");
 for(cmd=0;cmd<2;cmd++) {
 len += sysfs_emit_at(buf,len,"[%s]\n",os44_monitor_cmd_type_str[cmd]);
  for(w=0;w<2;w++) {
   bank=(m->stats.bank-w)&1;
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-16u %-12lu\n",bank,"16", (unsigned long)(m->stats.normal[bank][cmd][0].sum/1000000UL), m->stats.normal[bank][cmd][0].count, (unsigned long)(m->stats.normal[bank][cmd][0].maximum/1000UL));
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-16u %-12lu\n",bank,"128", (unsigned long)(m->stats.normal[bank][cmd][1].sum/1000000UL), m->stats.normal[bank][cmd][1].count, (unsigned long)(m->stats.normal[bank][cmd][1].maximum/1000UL));
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-16u %-12lu\n",bank,"256", (unsigned long)(m->stats.normal[bank][cmd][2].sum/1000000UL), m->stats.normal[bank][cmd][2].count, (unsigned long)(m->stats.normal[bank][cmd][2].maximum/1000UL));
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-16u %-12lu\n",bank,"512", (unsigned long)(m->stats.normal[bank][cmd][3].sum/1000000UL), m->stats.normal[bank][cmd][3].count, (unsigned long)(m->stats.normal[bank][cmd][3].maximum/1000UL));
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-16u %-12lu\n",bank,"512+", (unsigned long)(m->stats.normal[bank][cmd][4].sum/1000000UL), m->stats.normal[bank][cmd][4].count, (unsigned long)(m->stats.normal[bank][cmd][4].maximum/1000UL));
  }
 }
 len += sysfs_emit_at(buf,len,"\nAbnormal:\n");
 len += sysfs_emit_at(buf,len,"Window   Size(k)      Sum(ms)          Count           Peak(us)      Threshold(us)\n");
 for(cmd=0;cmd<2;cmd++) {
 len += sysfs_emit_at(buf,len,"[%s]\n",os44_monitor_cmd_type_str[cmd]);
  for(w=0;w<2;w++) {
   bank=(m->stats.bank-w)&1;
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-15u %-12lu %-14lu\n",bank,"16", (unsigned long)(m->stats.slow[bank][cmd][0].sum/1000000UL), m->stats.slow[bank][cmd][0].count, (unsigned long)(m->stats.slow[bank][cmd][0].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][0]);
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-15u %-12lu %-14lu\n",bank,"128", (unsigned long)(m->stats.slow[bank][cmd][1].sum/1000000UL), m->stats.slow[bank][cmd][1].count, (unsigned long)(m->stats.slow[bank][cmd][1].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][1]);
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-15u %-12lu %-14lu\n",bank,"256", (unsigned long)(m->stats.slow[bank][cmd][2].sum/1000000UL), m->stats.slow[bank][cmd][2].count, (unsigned long)(m->stats.slow[bank][cmd][2].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][2]);
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-15u %-12lu %-14lu\n",bank,"512", (unsigned long)(m->stats.slow[bank][cmd][3].sum/1000000UL), m->stats.slow[bank][cmd][3].count, (unsigned long)(m->stats.slow[bank][cmd][3].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][3]);
 len += sysfs_emit_at(buf,len,"%-8d %-12s %-16lu %-15u %-12lu %-14lu\n",bank,"512+", (unsigned long)(m->stats.slow[bank][cmd][4].sum/1000000UL), m->stats.slow[bank][cmd][4].count, (unsigned long)(m->stats.slow[bank][cmd][4].maximum/1000UL), (unsigned long)m->stats.slow_us[cmd][4]);
  }
 }
 return len;
}
ssize_t os44_ufsmonitor_sysfs_store_monitor_window_cmd_lat(struct os44_ufsmonitor *m, const char *buf, size_t count)
{
 if (sysfs_streq(buf,"reset")) {
  memset(m->stats.normal,0,sizeof(m->stats.normal));
  memset(m->stats.slow,0,sizeof(m->stats.slow));
  pr_info("ufs_monitor: daily_stats reset via sysfs\n");
 }
 return count;
}
