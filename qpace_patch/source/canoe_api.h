/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CANOE_QPACE_API_H
#define CANOE_QPACE_API_H
#include <linux/types.h>
struct canoe_engine;
struct canoe_engine *canoe_qpace_open(void);
void canoe_qpace_close(struct canoe_engine *e);
/* Atomic-context safe. No allocation, mutex, ICC call or sleeping wait. */
int canoe_qpace_page(struct canoe_engine *e, bool decompress,
 const void *src, size_t len, void *dst, size_t capacity);
#endif
