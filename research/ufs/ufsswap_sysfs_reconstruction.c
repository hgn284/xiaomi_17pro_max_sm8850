/* Stock behavior preserved. Not production registered until owner/query cluster
 * is ready. No stock private KCFI identity claim and no silent lifetime repair. */
#include "ufsswap_sysfs_reconstruction.h"
#include <linux/kernel.h>
#include <linux/string.h>
#define CTX(s) container_of(s,struct os44_swap_sysfs,swap)
_Static_assert(offsetof(struct os44_swap_sysfs,kobj)==0x38,"kobject");
_Static_assert(offsetof(struct os44_swap_sysfs,lock)==0x98,"mutex");
_Static_assert(offsetof(struct os44_swap_sysfs,attrs)==0xc8,"attributes");
_Static_assert(offsetof(struct os44_swap_sysfs,partition_start)==0xd0,"partition starts");
_Static_assert(offsetof(struct os44_swap_sysfs,partition_end)==0x150,"partition ends");
_Static_assert(offsetof(struct os44_swap_sysfs,partition_lun)==0x1d0,"partition LUN");
_Static_assert(offsetof(struct os44_swap_sysfs,partition_name)==0x210,"partition names");
_Static_assert(offsetof(struct os44_swap_sysfs,partition_count)==0x410,"partition count");
_Static_assert(sizeof(struct os44_swap_attribute)==32,"attribute stride");
static int tp_bit(struct os44_swap_view *s)
{
 struct ufs_hba *hba=s->hba;
 return !!(((const unsigned char *)&hba->dev_info)[54]&4);
}
long os44_show_support(struct os44_swap_view *s,char *buf)
{
 return sysfs_emit(buf,"%d\n",!!(s->max_buf || tp_bit(s) || os44_ufsswap_support_tp(s)));
}
long os44_show_max(struct os44_swap_view *s,char *buf)
{ return sysfs_emit(buf,"%d\n",(int)s->max_buf); }
static long show_attr(struct os44_swap_view *s,char *buf,unsigned char id)
{
 struct ufs_hba *hba=s->hba;
 os44_u32 value=~0U;
 int ret;
 if(tp_bit(s) || os44_ufsswap_support_tp(s)) return 0;
 __pm_runtime_resume(&hba->ufs_device_wlun->sdev_gendev,4);
 ret=os44_query_attr(hba,3,id,0,0,&value);
 if(ret) os44_query_error(1,2,-1,0);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 /* Original emits signed value even on query failure, rather than errno. */
 return sysfs_emit(buf,"%d\n",(int)value);
}
long os44_show_avail(struct os44_swap_view *s,char *buf)
{ return show_attr(s,buf,s->config->attr_avail); }
long os44_show_total(struct os44_swap_view *s,char *buf)
{ return show_attr(s,buf,s->config->attr_total); }
static long show_ranges(struct os44_swap_view *s,char *buf,int end)
{
 struct os44_list_view *h=&s->head,*p;
 os44_u64 *items;
 unsigned int i=0,n=s->input_pair_count;
 int length=0;
 if(h->next==h) return 0;
 items=kzalloc((os44_size)n*8,GFP_KERNEL);
 if(!items) return -ENOMEM;
 for(p=h->next;p!=h;p=p->next) {
  struct os44_range_view *r=container_of(p,struct os44_range_view,link);
  BUG_ON(i>=n);
  items[i++]=end?r->end:r->start;
 }
 for(i=0;i<n && i<5;i++) length+=snprintf(buf+length,PAGE_SIZE-length,end?"0x%llxx\n":"0x%llx\n",items[i]);
 kfree(items);
 return length;
}
long os44_show_pre(struct os44_swap_view *s,char *buf)
{ return show_ranges(s,buf,0); }
long os44_show_post(struct os44_swap_view *s,char *buf)
{ return show_ranges(s,buf,1); }
long os44_show_enable(struct os44_swap_view *s,char *buf)
{ return sysfs_emit(buf,"%d\n",s->enabled); }
long os44_store_enable(struct os44_swap_view *s,const char *buf,os44_size count)
{
 unsigned long long value=0;
 if(kstrtoull(buf,0,&value) || value>1) return -EINVAL;
 s->enabled=value;
 return count;
}
long os44_show_partitions(struct os44_swap_view *s,char *buf)
{
 struct os44_swap_sysfs *ctx=CTX(s);
 unsigned int i;
 long length=0;
 for(i=0;i<ctx->partition_count;i++) {
  BUG_ON(i>=16);
  length+=scnprintf(buf+length,PAGE_SIZE-length,
    "Partition %s (LUN %u): start=%llu, end=%llu\n",
    ctx->partition_name[i],ctx->partition_lun[i],ctx->partition_start[i],ctx->partition_end[i]);
 }
 return length;
}
static ssize_t dispatch_show(struct kobject *kobj,struct attribute *attr,char *buf)
{
 struct os44_swap_sysfs *ctx=container_of(kobj,struct os44_swap_sysfs,kobj);
 struct os44_swap_attribute *a=container_of(attr,struct os44_swap_attribute,attr);
 long ret;
 int state;
 if(!a->show) return -EIO;
 state=os44_ufsswap_singleton()->support_state;
 if(state!=1) {
  pr_info("[UFSSWAP]state != swap_PRESENT (%d)",state);
  return -ENODEV;
 }
 mutex_lock(&ctx->lock);ret=a->show(&ctx->swap,buf);mutex_unlock(&ctx->lock);
 return ret;
}
static ssize_t dispatch_store(struct kobject *kobj,struct attribute *attr,const char *buf,size_t count)
{
 struct os44_swap_sysfs *ctx=container_of(kobj,struct os44_swap_sysfs,kobj);
 struct os44_swap_attribute *a=container_of(attr,struct os44_swap_attribute,attr);
 long ret;
 int state;
 if(!a->store) return -EIO;
 state=os44_ufsswap_singleton()->support_state;
 if(state!=1) {
  pr_info("[UFSSWAP]state != swap_PRESENT (%d)",state);
  return -ENODEV;
 }
 mutex_lock(&ctx->lock);ret=a->store(&ctx->swap,buf,count);mutex_unlock(&ctx->lock);
 return ret;
}
static const struct sysfs_ops ops={.show=dispatch_show,.store=dispatch_store};
/* Stock release is NULL, not an invented successful cleanup stub. */
static const struct kobj_type ktype={.sysfs_ops=&ops};
static const struct os44_swap_attribute attrs[]={
 {{.name="swap_support",.mode=0444},os44_show_support,NULL},
 {{.name="swap_max_buf_size",.mode=0444},os44_show_max,NULL},
 {{.name="swap_avail_buf_size",.mode=0444},os44_show_avail,NULL},
 {{.name="swap_total_write_cnt",.mode=0444},os44_show_total,NULL},
 {{.name="swap_file_lba_pre",.mode=0444},os44_show_pre,NULL},
 {{.name="swap_file_lba_post",.mode=0444},os44_show_post,NULL},
 {{.name="swap_file_lba_info",.mode=0200},NULL,os44_ufsswap_store_reconstructed},
 {{.name="swap_enable",.mode=0644},os44_show_enable,os44_store_enable},
 {{.name="partation_info",.mode=0444},os44_show_partitions,NULL},
 {{.name=NULL,.mode=0},NULL,NULL}
};
int os44_ufsswap_create_sysfs(struct os44_swap_view *s)
{
 struct os44_swap_sysfs *ctx=CTX(s);
 struct ufs_hba *hba=s->hba;
 const struct os44_swap_attribute *a;
 int ret;
 ctx->attrs=attrs;kobject_init(&ctx->kobj,&ktype);mutex_init(&ctx->lock);
 ret=kobject_add(&ctx->kobj,kobject_get(&hba->dev->kobj),"ufsswap");
 if(ret) return ret;
 for(a=ctx->attrs;a->attr.name;a++) {
  ret=sysfs_create_file(&ctx->kobj,&a->attr);
  if(ret) {
   (void)kobject_uevent(&ctx->kobj,KOBJ_REMOVE);
   kobject_del(&ctx->kobj);return -EINVAL;
  }
 }
 (void)kobject_uevent(&ctx->kobj,KOBJ_ADD);
 return 0;
}
void os44_ufsswap_remove(struct os44_swap_view *s)
{
 struct os44_swap_sysfs *ctx=CTX(s);
 mutex_lock(&ctx->lock);
 (void)kobject_uevent(&ctx->kobj,KOBJ_REMOVE);
 kobject_del(&ctx->kobj);
 mutex_unlock(&ctx->lock);
 /* Original scoped body has no state reset/list drain/put. */
}
