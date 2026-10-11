// SPDX-License-Identifier: GPL-2.0-only
#include "ufssnapshot_flow.h"
struct os44_flow { u32 status,ctrl,action; os44_snapshot_flow_fn fn; };
struct os44_flow_map { u8 vendor; struct os44_flow *flows; u8 count; };
static_assert(sizeof(struct os44_flow)==24);
static_assert(sizeof(struct os44_flow_map)==24);
static struct os44_flow flows_hynix[] = {
 {99,5,1,os44_ufssnapshot_start},
 {0,5,1,os44_ufssnapshot_start},
 {3,5,0,os44_ufssnapshot_stop},
 {0,1,1,os44_ufssnapshot_start},
 {1,1,1,os44_ufssnapshot_start},
 {1,1,0,os44_ufssnapshot_stop},
 {1,5,0,os44_ufssnapshot_stop},
 {3,1,0,os44_ufssnapshot_stop},
 {0,0,1,os44_ufssnapshot_start},
};
static struct os44_flow flows_micron[] = {
 {99,5,1,os44_ufssnapshot_start},
 {0,5,1,os44_ufssnapshot_start},
 {0,1,1,os44_ufssnapshot_start},
 {1,1,0,os44_ufssnapshot_stop},
 {1,5,0,os44_ufssnapshot_stop},
 {2,0,1,os44_ufssnapshot_start},
 {0,0,1,os44_ufssnapshot_start},
 {1,5,0,os44_ufssnapshot_stop},
 {2,1,0,os44_ufssnapshot_stop},
 {2,0,0,os44_ufssnapshot_stop},
 {2,5,0,os44_ufssnapshot_stop},
};
static struct os44_flow flows_kioxia[] = {
 {99,5,1,os44_ufssnapshot_start},
 {0,5,1,os44_ufssnapshot_start},
 {0,1,1,os44_ufssnapshot_start},
 {1,1,0,os44_ufssnapshot_drop},
 {2,0,1,os44_ufssnapshot_start},
 {2,5,1,os44_ufssnapshot_start},
 {2,1,1,os44_ufssnapshot_start},
 {0,0,1,os44_ufssnapshot_start},
 {1,5,0,os44_ufssnapshot_drop},
};
static struct os44_flow flows_samsung[] = {
 {99,5,1,os44_ufssnapshot_active},
 {0,5,1,os44_ufssnapshot_active},
 {4,5,0,os44_ufssnapshot_stop},
 {0,1,1,os44_ufssnapshot_active},
 {4,1,0,os44_ufssnapshot_stop},
 {0,0,1,os44_ufssnapshot_active},
};
static struct os44_flow flows_ymtc[] = {
 {99,5,1,os44_ufsnapshot_backup_flow_ymtc},
 {0,5,1,os44_ufsnapshot_backup_flow_ymtc},
 {2,5,0,os44_ufssnapshot_stop},
 {0,1,1,os44_ufsnapshot_backup_flow_ymtc},
 {2,1,0,os44_ufssnapshot_stop},
 {0,0,1,os44_ufsnapshot_backup_flow_ymtc},
};
static const struct os44_flow_map flow_map[] = {
 {0,flows_hynix,9},
 {1,flows_micron,11},
 {2,flows_kioxia,9},
 {3,flows_samsung,6},
 {4,flows_ymtc,6},
};
static const struct os44_flow_map *const flow_switch[5]={&flow_map[0],&flow_map[1],&flow_map[2],&flow_map[3],&flow_map[4]};
os44_snapshot_flow_fn os44_ufssnapshot_find_flows(struct os44_snapshot_view *s,u32 action)
{
 const struct os44_flow_map *m; unsigned int i; os44_snapshot_flow_fn fn=NULL;
 if(s->vendor>4) {printk("\0013[err]%s: %d:[ufssnapshot] query flow failed, vendid %u\n","ufssnapshot_find_flows",707,s->vendor);return NULL;}
 m=flow_switch[s->vendor];
 for(i=0;i<m->count;i++) {
  struct os44_flow *f=&m->flows[i];
  if(s->debug==1) {printk("\0016[info]%s:%d:[ufssnapshot] sctrl %d status %d sactive %d\n","ufssnapshot_find_flows",714,s->ctrl,s->status,action);if(!(s->debug & 1))goto compare;}
  else if(!s->debug)goto compare;
  printk("\0016[info]%s:%d:[ufssnapshot] tctrl %d ttatus %d tactive %d\n","ufssnapshot_find_flows",716,f->ctrl,f->status,f->action);
compare:
  if(s->ctrl==f->ctrl && s->status==f->status && action==f->action) {
   fn=f->fn;
   if(s->debug==1) printk("\0016[info]%s:%d:[ufssnapshot] ctrl %d change to %d\n","ufssnapshot_find_flows",721,s->ctrl,action);
   s->ctrl=action;break;
  }
 }
 return fn;
}
long os44_ufssnapshot_do_ctrl(struct os44_snapshot_view *s,u32 ctrl)
{
 os44_snapshot_flow_fn fn;
 if(!s->supported) {printk("\0013[err]%s: %d:[ufssnapshot] function is not support.\n","ufssnapshot_do_ctrl",766);return -EIO;}
 if(ctrl==4) {
  s->ctrl=5;s->status=99;
  os44_ufssnapshot_stop(s);
  if(s->debug==1)printk("\0016[info]%s:%d:[ufssnapshot] enforce stop\n","ufssnapshot_do_ctrl",774);
  return 0;
 }
 if(os44_ufssnapshot_query_status(s))return -EIO;
 fn=os44_ufssnapshot_find_flows(s,ctrl);
 if(fn)return fn(s);
 printk("\0013[err]%s: %d:[ufssnapshot] ctrl is invalid, ctrl %d\n","ufssnapshot_do_ctrl",786,ctrl);return -EIO;
}
