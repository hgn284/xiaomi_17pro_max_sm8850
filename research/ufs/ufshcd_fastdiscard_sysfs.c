/* SPDX-License-Identifier: GPL-2.0-only */
#include "os44_fastdiscard_sysfs.h"
#include <linux/kernel.h>
#include <linux/sysfs.h>
/* Whole stock control flow reconstructed; no fake owner or runtime caller. */
static ssize_t fastdiscard_sysfs_show_support(struct os44_fastdiscard_ctrl *ctrl, char *buf)
{
 struct os44_fastdiscard_oem_view *oem=(void *)ctrl->hba->android_oem_data1;
 struct os44_fastdiscard_state_view *info=oem->fastdiscard;
 pr_info("[info] fastdiscard_support=[%d]\n",info->enabled);
 return sysfs_emit(buf,"%u\n",info->enabled);
}
static ssize_t fastdiscard_sysfs_show_debug_enable(struct os44_fastdiscard_ctrl *ctrl, char *buf)
{ return sysfs_emit(buf,"%d\n",ctrl->debug_enabled); }
static ssize_t fastdiscard_sysfs_store_debug_enable(struct os44_fastdiscard_ctrl *ctrl,const char *buf,size_t count)
{
 struct ufs_hba *hba=ctrl->hba;
 unsigned long long value=0;
 unsigned long flags;
 if(kstrtoull(buf,0,&value)) return -EINVAL;
 value=!!value;
 spin_lock_irqsave(hba->host->host_lock,flags);
 ctrl->debug_enabled=!!value;
 spin_unlock_irqrestore(hba->host->host_lock,flags);
 return count;
}
static ssize_t fastdiscard_sysfs_show_unmap_count(struct os44_fastdiscard_ctrl *ctrl,char *buf)
{ return sysfs_emit(buf,"%lu\n",ctrl->unmap_count); }
static ssize_t fastdiscard_sysfs_show_unmap_time(struct os44_fastdiscard_ctrl *ctrl,char *buf)
{ return sysfs_emit(buf,"%llu\n",ctrl->unmap_time/1000); }
static ssize_t fastdiscard_sysfs_show_multi_unmap_count(struct os44_fastdiscard_ctrl *ctrl,char *buf)
{ return sysfs_emit(buf,"%lu\n",ctrl->multi_unmap_count); }
static ssize_t fastdiscard_sysfs_show_multi_unmap_time(struct os44_fastdiscard_ctrl *ctrl,char *buf)
{ return sysfs_emit(buf,"%llu\n",ctrl->multi_unmap_time/1000); }
static ssize_t fastdiscard_sysfs_show_multi_unmap_lba(struct os44_fastdiscard_ctrl *ctrl,char *buf)
{ return sysfs_emit(buf,"%lu\n",ctrl->multi_unmap_lba); }
static ssize_t fastdiscard_attrs_show(struct kobject *kobj,struct attribute *attr,char *buf)
{
 struct os44_fastdiscard_ctrl *ctrl=container_of(kobj,struct os44_fastdiscard_ctrl,kobj);
 struct os44_fastdiscard_attr *entry=container_of(attr,struct os44_fastdiscard_attr,attr);
 ssize_t result;
 if(!entry->show) return -EIO;
 mutex_lock(&ctrl->sysfs_lock);
 result=entry->show(ctrl,buf);
 mutex_unlock(&ctrl->sysfs_lock);
 return result;
}
static ssize_t fastdiscard_attrs_store(struct kobject *kobj,struct attribute *attr,const char *buf,size_t count)
{
 struct os44_fastdiscard_ctrl *ctrl=container_of(kobj,struct os44_fastdiscard_ctrl,kobj);
 struct os44_fastdiscard_attr *entry=container_of(attr,struct os44_fastdiscard_attr,attr);
 ssize_t result;
 if(!entry->store) return -EIO;
 mutex_lock(&ctrl->sysfs_lock);
 result=entry->store(ctrl,buf,count);
 mutex_unlock(&ctrl->sysfs_lock);
 return result;
}
static const struct sysfs_ops fastdiscard_sysfs_ops={.show=fastdiscard_attrs_show,.store=fastdiscard_attrs_store};
static struct kobj_type fastdiscard_ktype={.sysfs_ops=&fastdiscard_sysfs_ops};
#define FD_RO(n) { .attr={.name=#n,.mode=0444},.show=fastdiscard_sysfs_show_##n }
static struct os44_fastdiscard_attr fastdiscard_attrs[]={
 FD_RO(support),{.attr={.name="debug_enable",.mode=0644},.show=fastdiscard_sysfs_show_debug_enable,.store=fastdiscard_sysfs_store_debug_enable},
 FD_RO(unmap_count), FD_RO(unmap_time), FD_RO(multi_unmap_count), FD_RO(multi_unmap_time),FD_RO(multi_unmap_lba),{}
};
int os44_fastdiscard_create_sysfs(struct os44_fastdiscard_ctrl *fastdiscard_ctrl)
{
 struct device *dev=fastdiscard_ctrl->hba->dev;
 struct os44_fastdiscard_attr *entry;
 int err;
 fastdiscard_ctrl->attrs=fastdiscard_attrs;
 kobject_init(&fastdiscard_ctrl->kobj,&fastdiscard_ktype);
 mutex_init(&fastdiscard_ctrl->sysfs_lock);
 err=kobject_add(&fastdiscard_ctrl->kobj,&dev->kobj,"ufsfastdiscard");
 if(err) { pr_err("[ufsfastdiscard] kobject add failed, err %d",err); return err; }
 for(entry=fastdiscard_ctrl->attrs;entry->attr.name;entry++) {
  err=sysfs_create_file(&fastdiscard_ctrl->kobj,&entry->attr);
  if(err) {
   pr_err("[ufsfastdiscard] create attr file failed, attr name %s",entry->attr.name);
   kobject_uevent(&fastdiscard_ctrl->kobj,KOBJ_REMOVE);
   kobject_del(&fastdiscard_ctrl->kobj);
   return -EINVAL;
  }
  kobject_uevent(&fastdiscard_ctrl->kobj,KOBJ_ADD);
 }
 return 0;
}
