/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_COMPLETION_STATS_H
#define OS44_COMPLETION_STATS_H
#include <linux/spinlock.h>
/* Instruction-derived view, NOT the original private C declaration. */
struct os44_latency_cell { u64 sum, maximum; u32 count; u32 padding; };
struct os44_completion_stats {
 u8 enabled;
 u8 reserved[7];
 spinlock_t lock;
 u8 lock_padding[8-sizeof(spinlock_t)];
 u64 interval;
 u64 slow_us[2][5];
 int bank;
 u32 bank_padding;
 u64 timestamp;
 struct os44_latency_cell normal[2][2][5];
 struct os44_latency_cell slow[2][2][5];
 struct os44_latency_cell cumulative[2][5];
 struct os44_latency_cell cumulative_slow[2][5];
 struct os44_latency_cell histogram[2][5];
};
static_assert(sizeof(struct os44_latency_cell)==24);
static_assert(offsetof(struct os44_completion_stats,lock)==8);
static_assert(offsetof(struct os44_completion_stats,interval)==0x10);
static_assert(offsetof(struct os44_completion_stats,slow_us)==0x18);
static_assert(offsetof(struct os44_completion_stats,bank)==0x68);
static_assert(offsetof(struct os44_completion_stats,timestamp)==0x70);
static_assert(offsetof(struct os44_completion_stats,normal)==0x78);
static_assert(offsetof(struct os44_completion_stats,slow)==0x258);
static_assert(offsetof(struct os44_completion_stats,cumulative)==0x438);
static_assert(offsetof(struct os44_completion_stats,cumulative_slow)==0x528);
static_assert(offsetof(struct os44_completion_stats,histogram)==0x618);
#endif
