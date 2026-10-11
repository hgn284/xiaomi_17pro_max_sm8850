/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_DEBUG_H
#define OS44_DEBUG_H
#include <linux/types.h>
#include <linux/workqueue.h>
/* Reconstruction-owned byte layout, NOT the recovered private C declaration. */
struct os44_debug_view {
 union {
  struct {
   u8 before_suspend[0xa58];
   s64 last_runtime_suspend;
   u64 max_runtime_suspend_gap;
   u8 before_suspend_alltime[0x1f30-0xa68];
   u64 max_runtime_suspend_gap_alltime;
  };
  struct {
   u8 before_works[0x1f50];
   struct delayed_work load_work;
   struct delayed_work save_work;
  };
  u8 bytes[0x2020];
  struct {
   u8 before_enter[0xa28];
   s64 last_hibern8_enter;
   u64 max_hibern8_gap;
   u8 before_enter_alltime[0x1f00-0xa38];
   u64 max_hibern8_gap_alltime;
  };
  struct {
   u8 before_gate[0xa38];
   s64 last_gate;
   u64 max_gate_gap;
   u8 before_gate_alltime[0x1f10-0xa48];
   u64 max_gate_gap_alltime;
  };
  struct {
   u8 before_last[0xa48];
   s64 last_completion;
   u64 max_completion_gap;
   u8 before_alltime[0x1f20 - 0xa58];
   u64 max_completion_gap_alltime;
  };
 };
} __aligned(8);
static_assert(offsetof(struct os44_debug_view,last_completion)==0xa48);
static_assert(offsetof(struct os44_debug_view,max_completion_gap)==0xa50);
static_assert(offsetof(struct os44_debug_view,max_completion_gap_alltime)==0x1f20);
static_assert(sizeof(struct os44_debug_view)==0x2020);
static_assert(offsetof(struct os44_debug_view,last_hibern8_enter)==0xa28);
static_assert(offsetof(struct os44_debug_view,max_hibern8_gap)==0xa30);
static_assert(offsetof(struct os44_debug_view,max_hibern8_gap_alltime)==0x1f00);
static_assert(offsetof(struct os44_debug_view,last_gate)==0xa38);
static_assert(offsetof(struct os44_debug_view,max_gate_gap)==0xa40);
static_assert(offsetof(struct os44_debug_view,max_gate_gap_alltime)==0x1f10);
static_assert(offsetof(struct os44_debug_view,last_runtime_suspend)==0xa58);
static_assert(offsetof(struct os44_debug_view,max_runtime_suspend_gap)==0xa60);
static_assert(offsetof(struct os44_debug_view,max_runtime_suspend_gap_alltime)==0x1f30);
static_assert(offsetof(struct os44_debug_view,load_work)==0x1f50);
static_assert(offsetof(struct os44_debug_view,save_work)==0x1fb8);
static_assert(sizeof(struct delayed_work)==0x68);
extern struct os44_debug_view os44_ufs_data;
/* Actual static owner; original teardown does not drain these private works. */
extern struct os44_debug_view *os44_debug_owner(void);
void os44_debug_load(void);
struct scsi_device;
struct os44_partition_span { u64 start, length; };
/* Reconstructed paired declaration; original private typedef identity unknown. */
extern void os44_debug_partition_info(struct scsi_device **, int lun,
 const char *name, struct os44_partition_span *);
extern int os44_debug_read_partition(struct scsi_device *, void *, u32, u32);
extern int os44_debug_write_partition(struct scsi_device *device, void *buffer, u32 lba, u32 count);
void os44_query_error(unsigned int, unsigned int, int, unsigned int);
void os44_debug_save(void);
void os44_debug_init_works(struct os44_debug_view *);
#endif
