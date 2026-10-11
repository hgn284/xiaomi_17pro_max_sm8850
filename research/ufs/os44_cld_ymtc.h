/* Source-owned typed union: word getter requires a full u32 backing object. */
#ifndef OS44_CLD_YMTC_H
#define OS44_CLD_YMTC_H
#include "os44_cld_ops.h"
/* Source-owned typed union: word getter requires a full u32 backing object. */
int os44_ymtc_init_idn(struct os44_cld_view *);
int os44_ymtc_get_frag_level(struct os44_cld_view *, u32 *);
int os44_ymtc_cld_set_trigger(struct os44_cld_view *, unsigned int);
int os44_ymtc_cld_get_trigger(struct os44_cld_view *, u32 *);
int os44_ymtc_cld_operation_status(struct os44_cld_view *, u32 *);
extern const struct os44_cld_ops os44_ymtc_cld_ops;
#endif
