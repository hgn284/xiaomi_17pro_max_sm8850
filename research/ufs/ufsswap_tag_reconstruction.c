/* Original ufshcd_queuecommand .text[0x257f0,0x25878), no DMA submission.
 * Stock SHA256 02da6ea7fba6977bb78d5c22a92ee4b687de145acad6cf12ebc4bad1680ca235.
 * Preserves unsigned inclusive START-only matching, WRITE10-only CDB6 overwrite,
 * configuration byte propagation and ARM64 variable-shift semantics.
 * Valid lists/config lifetime and external serialization are caller obligations;
 * original queuecommand reader synchronization has not been established.
 */
#include "ufsswap_tag_reconstruction.h"
_Static_assert(__builtin_offsetof(struct os44_swap_view, config)==0x18, "config");
_Static_assert(__builtin_offsetof(struct os44_swap_config_view, write10_cdb6)==7, "tag");
_Static_assert(__builtin_offsetof(struct os44_swap_view, enabled)==0x24, "enabled");
_Static_assert(__builtin_offsetof(struct os44_swap_view, head)==0x28, "head");
void os44_ufsswap_tag(struct os44_swap_view *s, os44_u64 sector,
                      os44_u32 sector_bytes, unsigned char *cdb)
{
    struct os44_list_view *p;
    os44_u32 leading, shift;
    os44_u64 lba;
    if (!s->enabled) return;
    p=s->head.next;
    if (p==&s->head) return;
    /* clz32(0)=32 in ARM64. Explicit &63 avoids C undefined oversized shifts.
     * Real SCSI caller normally supplies a power-of-two size >=512 bytes. */
    leading=sector_bytes ? (os44_u32)__builtin_clz(sector_bytes) : 32;
    shift=(22u-leading)&63u;
    lba=sector>>shift;
    for (;p!=&s->head;p=p->next) {
        struct os44_range_view *r=(void *)p;
        if (lba<r->start || lba>r->end) continue;
        if (cdb[0]==0x2a) cdb[6]=s->config->write10_cdb6;
        return;
    }
}
