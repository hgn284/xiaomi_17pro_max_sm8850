// SPDX-License-Identifier: GPL-2.0-only
#include "os44_snapshot_v2.h"
struct os44_flow_v2 { u32 status,action; os44_snapshot_v2_flow_fn fn; };
struct os44_flow_map_v2 { u8 vendor; struct os44_flow_v2 *flows; u8 count; };
static_assert(sizeof(struct os44_flow_v2)==16);
static_assert(sizeof(struct os44_flow_map_v2)==24);
static struct os44_flow_v2 flows_default_v2[]={
 {0,1,os44_ufssnapshot_start_V2},{1,0,os44_ufssnapshot_stop_V2},{2,0,os44_ufssnapshot_stop_V2}};
static const struct os44_flow_map_v2 flow_mapV2[]={
 {0,flows_default_v2,3},{1,flows_default_v2,3},{2,flows_default_v2,3},{3,flows_default_v2,3},{4,flows_default_v2,3}};
static const struct os44_flow_map_v2 *const flow_switch_v2[]={&flow_mapV2[0],&flow_mapV2[1],&flow_mapV2[2],&flow_mapV2[3],&flow_mapV2[4]};
os44_snapshot_v2_flow_fn os44_ufssnapshotV2_find_flows(struct os44_snapshot_v2_view *s,u32 action)
{
 const struct os44_flow_map_v2 *m; unsigned int i;
 struct os44_snapshot_v2_record *r;
 if(s->vendor>4) {printk("\0013[err]%s: %d:[ufssnapshotV2] query flow failed, vendid %u\n","ufssnapshotV2_find_flows",386,s->vendor);return NULL;}
 if(s->selected>=7)__builtin_trap();
 r=&s->records[s->selected];
 m=flow_switch_v2[s->vendor];
 for(i=0;i<m->count;i++) {
  struct os44_flow_v2 *f=&m->flows[i];
  if(s->debug==1) {printk("\0016[info]%s:%d:[ufssnapshotV2]status %d sactive %d\n","ufssnapshotV2_find_flows",393,r->ctrl,r->status);if(!(s->debug&1))goto compare;}
  else if(!s->debug)goto compare;
  printk("\0016[info]%s:%d:[ufssnapshotV2]ttatus %d tactive %d\n","ufssnapshotV2_find_flows",394,f->status,f->action);
compare:
  if(r->status==f->status && action==f->action) {r->ctrl=action;return f->fn;}
 }
 return NULL;
}
long os44_ufssnapshot_do_ctrl_V2(struct os44_snapshot_v2_view *s,u32 ctrl)
{
 u8 data[16]={0};u32 status;os44_snapshot_v2_flow_fn fn;
 if(ctrl==0 && s->buffer_id==255)return os44_ufssnapshot_stop_V2(s);
 if(os44_ufssnapshotV2_rw_buffer(s,s->vendor==3?0x1d:0x1e,0,s->selected,16,data,true))return -EIO;
 status=s->vendor==3 ? (data[15]&3) : (data[15]&1);
 if(s->vendor==3 && status>1)status=2;
 if(s->selected>=6)__builtin_trap();
 s->records[s->selected].status=status;
 fn=os44_ufssnapshotV2_find_flows(s,ctrl);
 if(fn)return fn(s);
 printk("\0013[err]%s: %d:[ufssnapshotV2] ctrl is invalid, ctrl %d\n","ufssnapshot_do_ctrl_V2",427,ctrl);
 return -EIO;
}
