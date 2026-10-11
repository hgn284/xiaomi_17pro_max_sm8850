#include <linux/slab.h>
#include <linux/device.h>
#include <linux/string.h>
#include "ufsswap_range_reconstruction.h"
#define OFF(type, field) __builtin_offsetof(type, field)
_Static_assert(OFF(struct os44_swap_view, input_pair_count)==0x10, "count");
_Static_assert(OFF(struct os44_swap_view, enabled)==0x24, "enabled");
_Static_assert(OFF(struct os44_swap_view, head)==0x28, "head");
_Static_assert(sizeof(struct os44_range_view)==24, "range");
long os44_ufsswap_store_reconstructed(struct os44_swap_view *s,
                                     const char *buf, os44_size count)
{
    struct os44_list_view *h=&s->head;
    char *p, *copy, *cursor, *tok;
    os44_u64 *start, *end, *length, value;
    int *index, ntokens=1, n, i, j, token=0;
    void *dev;
    /* list_del_init(head), not list_for_each_entry_safe/kfree. */
    h->next->prev=h->prev;
    h->prev->next=h->next;
    h->next=h; h->prev=h;
    if (!buf) return -22;
    p=strchr(buf, ',');
    if (!p || buf[strlen(buf)-1]==',') return -22;
    do { ++ntokens; p=strchr(p+1, ','); } while (p);
    if (ntokens & 1) return -22;
    n=ntokens/2;
    s->input_pair_count=n;
    /* Reading this member is evidence-backed; the hba C type is NOT recovered. */
    dev=*(void **)((unsigned char *)s->hba+0x40);
    copy=kstrdup(buf, 0xcc0);
    if (!copy) return -22;
    start=__kmalloc_noprof((os44_size)n*8, 0xdc0);
    if (!start) goto free_copy;
    end=__kmalloc_noprof((os44_size)n*8, 0xdc0);
    if (!end) goto free_start;
    length=__kmalloc_noprof((os44_size)n*8, 0xdc0);
    if (!length) goto free_end;
    index=__kmalloc_noprof((os44_size)n*4, 0xdc0);
    if (!index) goto free_length;
    for (i=0; i<n; ++i) index[i]=i;
    cursor=copy; i=0;
    while ((tok=strsep(&cursor, ","))) {
        if (kstrtoull(tok,16,&value)) goto free_index;
        if (!(token & 1)) start[i]=value;
        else {
            if (value<start[i]) goto free_index;
            end[i]=value; length[i]=value-start[i]; ++i;
        }
        ++token;
    }
    /* Original stable descending bubble sort of difference (not +1 length). */
    for (i=0; i<n; ++i)
        for (j=0; j<n-i-1; ++j)
            if (length[j]<length[j+1]) {
                os44_u64 v=length[j]; int k=index[j];
                length[j]=length[j+1]; length[j+1]=v;
                index[j]=index[j+1]; index[j+1]=k;
            }
    for (i=0; i<n && i<5; ++i) {
        struct os44_range_view *r=devm_kmalloc(dev,24,0xdc0);
        if (!r) goto free_index; /* prior devm nodes stay linked; no rollback */
        r->start=(os44_u32)start[index[i]];
        r->end=(os44_u32)end[index[i]];
        r->link.next=h; r->link.prev=h->prev;
        h->prev->next=&r->link; h->prev=&r->link;
    }
    if (h->next!=h) s->enabled=1;
    kfree(index); kfree(length); kfree(end); kfree(start); kfree(copy);
    return (long)count;
free_index: kfree(index);
free_length: kfree(length);
free_end: kfree(end);
free_start: kfree(start);
free_copy: kfree(copy);
    return -22;
}
