// SPDX-License-Identifier: GPL-2.0-only
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <scsi/scsi_host.h>
#include "os44_eua.h"
#include "ufseua_executor.h"
/* Original complete 8-byte MOV w0,#0;RET functions; intentionally do not touch output. */
int os44_samsung_eua_get_mapping_status(struct os44_eua_view *e,u8 *value){return 0;}
int os44_samsung_eua_get_total_used_lba(struct os44_eua_view *e,u32 *value){return 0;}
int os44_samsung_eua_get_free_space(struct os44_eua_view *e,u32 *value){return 0;}
int os44_micron_eua_get_mapping_status(struct os44_eua_view *e,u8 *value){return 0;}
int os44_micron_eua_get_total_used_lba(struct os44_eua_view *e,u32 *value){return 0;}
int os44_micron_eua_get_free_space(struct os44_eua_view *e,u32 *value){return 0;}
int os44_ymtc_eua_get_mapping_status(struct os44_eua_view *e,u8 *value){return 0;}
int os44_ymtc_eua_get_total_used_lba(struct os44_eua_view *e,u32 *value){return 0;}
int os44_ymtc_eua_get_free_space(struct os44_eua_view *e,u32 *value){return 0;}
int os44_hynix_eua_get_mapping_status(struct os44_eua_view *,u8 *);
int os44_hynix_eua_get_total_used_lba(struct os44_eua_view *,u32 *);
int os44_hynix_eua_get_free_space(struct os44_eua_view *,u32 *);
const struct os44_eua_ops os44_hynix_eua_ops={os44_hynix_eua_get_mapping_status,os44_hynix_eua_get_total_used_lba,os44_hynix_eua_get_free_space};
const struct os44_eua_ops os44_samsung_eua_ops={os44_samsung_eua_get_mapping_status,os44_samsung_eua_get_total_used_lba,os44_samsung_eua_get_free_space};
const struct os44_eua_ops os44_micron_eua_ops={os44_micron_eua_get_mapping_status,os44_micron_eua_get_total_used_lba,os44_micron_eua_get_free_space};
const struct os44_eua_ops os44_ymtc_eua_ops={os44_ymtc_eua_get_mapping_status,os44_ymtc_eua_get_total_used_lba,os44_ymtc_eua_get_free_space};
