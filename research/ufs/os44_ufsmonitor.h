/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_UFSMONITOR_H
#define OS44_UFSMONITOR_H
#include "os44_completion_stats.h"
/* Instruction-derived source-pair view; NOT recovered stock private type. */
struct ufs_hba;
struct os44_ufsmonitor { struct ufs_hba *hba; struct os44_completion_stats stats; };
static_assert(offsetof(struct os44_ufsmonitor,stats)==8);
static_assert(offsetof(struct os44_ufsmonitor,stats.interval)==0x18);
static_assert(offsetof(struct os44_ufsmonitor,stats.slow_us)==0x20);
static_assert(offsetof(struct os44_ufsmonitor,stats.normal)==0x80);
static_assert(offsetof(struct os44_ufsmonitor,stats.slow)==0x260);
static_assert(offsetof(struct os44_ufsmonitor,stats.cumulative)==0x440);
static_assert(offsetof(struct os44_ufsmonitor,stats.cumulative_slow)==0x530);
static_assert(offsetof(struct os44_ufsmonitor,stats.histogram)==0x620);
static_assert(sizeof(struct os44_ufsmonitor)==0x710);
ssize_t os44_ufsmonitor_sysfs_show_monitor_enable(struct os44_ufsmonitor *m, char *buf);
ssize_t os44_ufsmonitor_sysfs_store_monitor_enable(struct os44_ufsmonitor *m, const char *buf, size_t count);
ssize_t os44_ufsmonitor_sysfs_show_monitor_latency_read_threshold_us(struct os44_ufsmonitor *m, char *buf);
ssize_t os44_ufsmonitor_sysfs_store_monitor_latency_read_threshold_us(struct os44_ufsmonitor *m, const char *buf, size_t count);
ssize_t os44_ufsmonitor_sysfs_show_monitor_latency_write_threshold_us(struct os44_ufsmonitor *m, char *buf);
ssize_t os44_ufsmonitor_sysfs_store_monitor_latency_write_threshold_us(struct os44_ufsmonitor *m, const char *buf, size_t count);
ssize_t os44_ufsmonitor_sysfs_show_monitor_window_period_s(struct os44_ufsmonitor *m, char *buf);
ssize_t os44_ufsmonitor_sysfs_store_monitor_window_period_s(struct os44_ufsmonitor *m, const char *buf, size_t count);
ssize_t os44_ufsmonitor_sysfs_show_monitor_daily_cmd_lat(struct os44_ufsmonitor *m, char *buf);
ssize_t os44_ufsmonitor_sysfs_store_monitor_daily_cmd_lat(struct os44_ufsmonitor *m, const char *buf, size_t count);
ssize_t os44_ufsmonitor_sysfs_show_monitor_window_cmd_lat(struct os44_ufsmonitor *m, char *buf);
ssize_t os44_ufsmonitor_sysfs_store_monitor_window_cmd_lat(struct os44_ufsmonitor *m, const char *buf, size_t count);
#endif
