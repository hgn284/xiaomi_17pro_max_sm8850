/* Experimental layout views recovered from OS44 instructions, NOT original
 * C type identities. No exported symbol/CRC/KCFI or sysfs ABI compatibility claim. */
#ifndef OS44_UFSSWAP_RANGE_RECONSTRUCTION_H
#define OS44_UFSSWAP_RANGE_RECONSTRUCTION_H
typedef unsigned long os44_size;
typedef unsigned long long os44_u64;
typedef unsigned int os44_u32;
struct os44_list_view { struct os44_list_view *next, *prev; };
struct os44_range_view {
    struct os44_list_view link;
    os44_u32 start, end;
};
struct os44_swap_config_view { unsigned char attr_max, attr_second, attr_avail, attr_total, reserved4, attr_flush, flush_value, write10_cdb6, revision_quirk; };
struct os44_swap_view {
    void *hba;                   /* +0x00; original hba->dev at +0x40 */
    os44_u32 max_buf, second_value;
    os44_u32 input_pair_count;   /* +0x10; NOT capped to retained list size */
    os44_u32 model_index;        /* +0x14; probe table value */
    struct os44_swap_config_view *config; /* +0x18; original global+0x188 */
    os44_u32 support_state;     /* +0x20; original global+0x190 */
    unsigned char enabled;      /* +0x24; not cleared by parser failure */
    unsigned char opaque25[3];
    struct os44_list_view head; /* +0x28 */
};
long os44_ufsswap_store_reconstructed(struct os44_swap_view *, const char *, os44_size);
#endif
