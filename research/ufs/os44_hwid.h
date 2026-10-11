/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS44_HWID_H
#define OS44_HWID_H
#include <linux/types.h>
const char *product_name_get(void);
uint32_t get_hw_project_adc(void);
uint32_t get_hw_build_adc(void);
uint32_t get_hw_version_platform(void);
uint32_t get_hw_id_value(void);
uint32_t get_hw_country_version(void);
uint32_t get_hw_version_major(void);
uint32_t get_hw_version_minor(void);
uint32_t get_hw_version_build(void);
#endif
