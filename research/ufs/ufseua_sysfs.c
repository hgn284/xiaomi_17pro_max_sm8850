// SPDX-License-Identifier: GPL-2.0-only
#include "os44_eua.h"
const struct sysfs_ops os44_ufseua_sysfs_ops={.show=os44_ufseua_attr_show,.store=os44_ufseua_attr_store};
static struct kobj_type os44_ufseua_ktype={.sysfs_ops=&os44_ufseua_sysfs_ops};
static struct os44_eua_entry os44_ufseua_sysfs_entries[]={
 {{.name="eua_get_total_free_space",.mode=0444},os44_ufseua_sysfs_show_eua_get_total_free_space,NULL},
 {{.name="eua_get_total_used_lba",.mode=0444},os44_ufseua_sysfs_show_eua_get_total_used_lba,NULL},
 {{.name="eua_support",.mode=0444},os44_ufseua_sysfs_show_eua_support,NULL},
 {{.name="eua_support_size",.mode=0444},os44_ufseua_sysfs_show_eua_support_size,NULL},
 {{.name="eua_get_mapping_status",.mode=0644},os44_ufseua_sysfs_show_eua_get_mapping_status,os44_ufseua_sysfs_store_eua_get_mapping_status},
 {{.name="eua_file_lba_unmap",.mode=0200},NULL,os44_ufseua_sysfs_store_eua_file_lba_unmap},
 {{.name="eua_enable",.mode=0444},os44_ufseua_sysfs_show_eua_enable,NULL},
 {{.name="eua_hide_size",.mode=0444},os44_ufseua_sysfs_show_eua_hide_size,NULL},
 {},
};
int os44_ufseua_create_sysfs(struct os44_eua_view *eua_ctrl)
{
 struct kobject *parent=&eua_ctrl->hba->dev->kobj;struct os44_eua_entry *entry;int ret;
 eua_ctrl->entries=os44_ufseua_sysfs_entries;
 kobject_init(&eua_ctrl->kobj,&os44_ufseua_ktype);
 {static struct lock_class_key __key;__mutex_init(&eua_ctrl->lock,"&eua_ctrl->sysfs_lock",&__key);}
 printk("\0016Creates sysfs %p dev->kobj %p",&eua_ctrl->kobj,parent);
 ret=kobject_add(&eua_ctrl->kobj,kobject_get(parent),"ufseua");
 if(ret){printk("\0013Kobject_add failed");return ret;}
 for(entry=eua_ctrl->entries;entry->attr.name;entry++) {
  if(sysfs_create_file(&eua_ctrl->kobj,&entry->attr)) {
   printk("\0013Create entry(%s) failed",entry->attr.name);
   kobject_uevent(&eua_ctrl->kobj,KOBJ_REMOVE);kobject_del(&eua_ctrl->kobj);return -EINVAL;
  }
 }
 kobject_uevent(&eua_ctrl->kobj,KOBJ_ADD);return 0;
}
