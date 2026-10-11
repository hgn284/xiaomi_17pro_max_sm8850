/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent reconstruction of stock 3330c..33788 / 32ec0..3301c.
 * NO standalone global, module-init or mock persistence implementation.
 * Full HBA owner and load/partition providers remain explicit link gaps. */
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include "ufsswap_debug_reconstruction.h"
#define U32(d,o) (*(u32 *)((d)->bytes+(o)))
#define U64(d,o) (*(u64 *)((d)->bytes+(o)))
#define LOAD_OFF 0x1f50
#define SAVE_OFF 0x1fb8
static struct delayed_work *work_at(struct os44_debug_view *d, unsigned int off)
{ return (struct delayed_work *)(d->bytes+off); }
static void ring_count(struct os44_debug_view *d, unsigned int base,
                       unsigned int counter, u8 opcode, u8 detail, bool special)
{
 unsigned int i;
 /* Stock lookup tests type==1 even for special 0x3b/0x3c insertions (type0).
  * Stock does NOT advance the ring position in this function. */
 for (i=0;i<32;i++)
  if (U32(d,base+i*12)==1 && d->bytes[base+i*12+4]==opcode &&
      d->bytes[base+i*12+5]==detail) break;
 if (i==32) {
  int index=(s32)U32(d,counter)%32;
  BUG_ON((u32)index>32);
  i=index;
  U32(d,base+i*12)=special?0:1;
  d->bytes[base+i*12+4]=opcode;
  d->bytes[base+i*12+5]=detail;
 }
 U32(d,base+i*12+8)++;
}
void os44_query_error(unsigned int group,unsigned int category,int value,
                      unsigned int detail)
{
 struct os44_debug_view *d=os44_debug_owner();
 unsigned long delay=0;
 bool slow=(category & ~2U)==8;
 if (!d->bytes[0x1f48]) {
  delay=slow?7500:0; /* exact stock HZ250 30 seconds */
  if (!delayed_work_pending(work_at(d,LOAD_OFF))) {
   queue_delayed_work_on(32,system_wq,work_at(d,LOAD_OFF),delay);
   d->bytes[0x1f48]=1;
  }
 }
 if ((u32)value-0x3bU<=1) {
  bool special=true;
  ring_count(d,0x8a0,0xa20,(u8)value,(u8)detail,special);
  ring_count(d,0x1d70,0x1ef0,(u8)value,(u8)detail,special);
 } else if (group==1) {
  BUG_ON(category>5);
  U64(d,0x878+category*8)++;
  U64(d,0x1d48+category*8)++;
 } else {
  if (category==1) {
   ring_count(d,0x8a0,0xa20,(u8)value,(u8)detail,false);
   ring_count(d,0x1d70,0x1ef0,(u8)value,(u8)detail,false);
  }
  /* category1 continues into aggregate counters: stock33650..3365c. */
  U64(d,0)++;
  BUG_ON(category>14);
  U64(d,8+category*8)++;
  if (value!=-1) {
   BUG_ON((u32)value>256);
   U64(d,0x78+(u32)value*8)++;
  }
  U64(d,0x14d0)++;
  U64(d,0x14d8+category*8)++;
  if (value!=-1) U64(d,0x1548+(u32)value*8)++;
 }
 if (!delayed_work_pending(work_at(d,SAVE_OFF)))
  queue_delayed_work_on(32,system_wq,work_at(d,SAVE_OFF),slow?7500:delay);
 dump_stack();
}
void os44_debug_save(void)
{
 struct os44_debug_view *d=os44_debug_owner();
 struct scsi_device *dev=(struct scsi_device *)(unsigned long)U64(d,0x1f38);
 void *buf;
 int ret;
 if (!dev) { pr_err("os44_debug_save: device not loaded\n"); return; }
 buf=kmalloc(4096,GFP_KERNEL);
 if (!buf) { pr_err("os44_debug_save: allocation failed\n"); return; }
 memset(buf,0,4096);
 memcpy(buf,d->bytes+0x14d0,0xa68);
 ret=os44_debug_write_partition((struct scsi_device *)(unsigned long)U64(d,0x1f38),buf,U32(d,0x1f40)+0x800,1);
 if (ret) pr_err("os44_debug_save: cumulative write failed %d\n",ret);
 /* First write error does NOT suppress second write in the stock provider. */
 memset(buf,0,4096);
 memcpy(buf,d->bytes,0xa68);
 ret=os44_debug_write_partition((struct scsi_device *)(unsigned long)U64(d,0x1f38),buf,U32(d,0x1f40)+0x801,1);
 if (ret) pr_err("os44_debug_save: current write failed %d\n",ret);
 kfree(buf);
}
static void load_work(struct work_struct *w) { os44_debug_load(); }
static void save_work(struct work_struct *w) { os44_debug_save(); }
void os44_debug_init_works(struct os44_debug_view *d)
{
 /* Stock async_scan 1d4c0..1d55c, AFTER scsi_scan_host/wp/probe.
  * This slice is not yet registered into reconstructed async_scan. */
 static_assert(sizeof(struct delayed_work)==0x68);
 static_assert(SAVE_OFF+sizeof(struct delayed_work)==sizeof(*d));
 INIT_DELAYED_WORK(work_at(d,LOAD_OFF),load_work);
 INIT_DELAYED_WORK(work_at(d,SAVE_OFF),save_work);
}
