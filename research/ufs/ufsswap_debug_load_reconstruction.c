/* SPDX-License-Identifier: GPL-2.0-only */
/* Equivalent source reconstruction: stock ufsdbg_load_err_reason 325d0..32bd4.
 * Partition discovery/physical IO and full-driver owner are REAL unresolved
 * providers, not success mocks. No module entry or artificial global state. */
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include "ufsswap_debug_reconstruction.h"
static void merge_snapshot(u8 *dst, const u8 *src)
{
 unsigned int off, i;
 /* 15 aggregate + 256 opcode + 5 group counters, unsigned wrapping adds. */
 for (off=0;off<0x8a0;off+=8)
  *(u64 *)(dst+off) += *(const u64 *)(src+off);
 /* Stock signed count, not clamped/modulo. Entry32 is allowed by the
  * instruction guard; entry33 traps. Count/remaining metadata ordinarily
  * remain untouched: NOT a memcpy of the entire 0xa68-byte snapshot. */
 for (i=0;(s32)i<*(const s32 *)(src+0xa20);i++) {
  BUG_ON(i==33);
  memcpy(dst+0x8a0+i*12,src+0x8a0+i*12,12);
 }
}
void os44_debug_load(void)
{
 struct os44_debug_view *d=os44_debug_owner();
 struct scsi_device *dev=NULL;
 struct os44_partition_span part={0,0};
 void *buf,*history;
 int lun,ret;
 for (lun=0;lun<0xc17f;lun++) {
  os44_debug_partition_info(&dev,lun,"gsort",&part);
  if (dev && part.length) break;
 }
 if (lun==0xc17f) {
  pr_err("os44_debug_load: partition not found\n");
  pr_err("os44_debug_load: unavailable device\n");
  return;
 }
 pr_info("os44_debug_load: partition found at lun %d\n",lun);
 if (!d) { pr_err("os44_debug_load: unavailable owner\n"); return; }
 pr_info("os44_debug_load: start %llu length %llu\n",part.start,part.length);
 buf=kmalloc(4096,GFP_KERNEL);
 if (!buf) { pr_err("os44_debug_load: buffer allocation failed\n"); return; }
 memset(buf,0,4096);
 ret=os44_debug_read_partition(dev,buf,(u32)part.start+0x800,1);
 if (ret) { pr_err("os44_debug_load: first read failed %d\n",ret); goto free_buf; }
 history=kmalloc(0xa68,GFP_KERNEL);
 if (!history) { pr_err("os44_debug_load: history allocation failed\n"); goto free_buf; }
 memcpy(history,buf,0xa68);
 merge_snapshot(d->bytes+0x14d0,history);
 memset(buf,0,4096);
 ret=os44_debug_read_partition(dev,buf,(u32)part.start+0x801,1);
 if (ret) { pr_err("os44_debug_load: second read failed %d\n",ret); goto free_both; }
 memcpy(history,buf,0xa68);
 merge_snapshot(d->bytes+0xa68,history);
 /* Publish ONLY after both reads; second failure retains the first merge. */
 *(struct scsi_device **)(d->bytes+0x1f38)=dev;
 *(u64 *)(d->bytes+0x1f40)=part.start;
free_both:
 kfree(buf);
 kfree(history);
 return;
free_buf:
 kfree(buf);
}
