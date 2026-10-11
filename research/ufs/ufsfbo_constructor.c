// SPDX-License-Identifier: GPL-2.0-only
#include "os44_fbo_constructor.h"
const struct sysfs_ops os44_ufsfbo_sysfs_ops={.show=os44_ufsfbo_attr_show,.store=os44_ufsfbo_attr_store};
static struct kobj_type os44_ufsfbo_ktype={.sysfs_ops=&os44_ufsfbo_sysfs_ops};
static struct os44_fbo_entry os44_ufsfbo_sysfs_entries[]={
 {{.name="fbo_rec_lrs",.mode=0444},os44_ufsfbo_sysfs_show_fbo_rec_lrs,NULL},
 {{.name="fbo_max_lrs",.mode=0444},os44_ufsfbo_sysfs_show_fbo_max_lrs,NULL},
 {{.name="fbo_min_lrs",.mode=0444},os44_ufsfbo_sysfs_show_fbo_min_lrs,NULL},
 {{.name="fbo_max_lrc",.mode=0444},os44_ufsfbo_sysfs_show_fbo_max_lrc,NULL},
 {{.name="fbo_lra",.mode=0444},os44_ufsfbo_sysfs_show_fbo_lra,NULL},
 {{.name="fbo_prog_state",.mode=0444},os44_ufsfbo_sysfs_show_fbo_prog_state,NULL},
 {{.name="fbo_get_lr_frag_level",.mode=0444},os44_ufsfbo_sysfs_show_fbo_get_lr_frag_level,NULL},
 {{.name="fbo_support",.mode=0444},os44_ufsfbo_sysfs_show_fbo_support,NULL},
 {{.name="fbo_version",.mode=0444},os44_ufsfbo_sysfs_show_fbo_version,NULL},
 {{.name="fbs_flc_ops",.mode=0444},os44_ufsfbo_sysfs_show_fbs_flc_ops,NULL},
 {{.name="fbs_defrag_ops",.mode=0444},os44_ufsfbo_sysfs_show_fbs_defrag_ops,NULL},
 {{.name="fbs_flc_enable",.mode=0200},NULL,os44_ufsfbo_sysfs_store_fbs_flc_enable},
 {{.name="fbs_defrag_enable",.mode=0200},NULL,os44_ufsfbo_sysfs_store_fbs_defrag_enable},
 {{.name="is_ufs31",.mode=0444},os44_ufsfbo_sysfs_show_is_ufs31,NULL},
 {{.name="fbo_app_preload",.mode=0200},NULL,os44_ufsfbo_sysfs_store_fbo_app_preload},
 {{.name="fbo_operation_control",.mode=0200},NULL,os44_ufsfbo_sysfs_store_fbo_operation_control},
 {{.name="fbo_send_lr_list",.mode=0200},NULL,os44_ufsfbo_sysfs_store_fbo_send_lr_list},
 {{.name="fbo_exe_threshold",.mode=0644},os44_ufsfbo_sysfs_show_fbo_exe_threshold,os44_ufsfbo_sysfs_store_fbo_exe_threshold},
 {{.name="fbo_wholefile_enable",.mode=0644},os44_ufsfbo_sysfs_show_fbo_wholefile_enable,os44_ufsfbo_sysfs_store_fbo_wholefile_enable},
 {{.name="fbo_err_cnt",.mode=0644},os44_ufsfbo_sysfs_show_fbo_err_cnt,os44_ufsfbo_sysfs_store_fbo_err_cnt},
 {{.name="fbo_retry_cnt",.mode=0644},os44_ufsfbo_sysfs_show_fbo_retry_cnt,os44_ufsfbo_sysfs_store_fbo_retry_cnt},
 {{.name="fbo2_linear_mapping_start",.mode=0444},os44_ufsfbo_sysfs_show_fbo2_linear_mapping_start,NULL},
 {{.name="fbo2_linear_mapping_end",.mode=0444},os44_ufsfbo_sysfs_show_fbo2_linear_mapping_end,NULL},
 {{.name="fbo2_linear_map_state",.mode=0444},os44_ufsfbo_sysfs_show_fbo2_linear_map_state,NULL},
 {{.name="fbo3_send_lr_list",.mode=0200},NULL,os44_ufsfbo_sysfs_store_fbo3_send_lr_list},
 {{.name="fbo3_get_lr_pinned_info",.mode=0444},os44_ufsfbo_sysfs_show_fbo3_get_lr_pinned_info,NULL},
 {{.name="fbo3_ss_send_pin_lr_list",.mode=0200},NULL,os44_ufsfbo_sysfs_store_fbo3_ss_send_pin_lr_list},
 {{.name="debug",.mode=0644},os44_ufsfbo_sysfs_show_debug,os44_ufsfbo_sysfs_store_debug},
 {{.name="block_suspend",.mode=0644},os44_ufsfbo_sysfs_show_block_suspend,os44_ufsfbo_sysfs_store_block_suspend},
 {{.name="auto_hibern8_enable",.mode=0644},os44_ufsfbo_sysfs_show_auto_hibern8_enable,os44_ufsfbo_sysfs_store_auto_hibern8_enable},
 {},
};
int os44_ufsfbo_create_sysfs(struct os44_fbo_lifetime *fbo)
{
 struct kobject *parent=&fbo->hba->dev->kobj;struct os44_fbo_entry *entry;int ret;
 fbo->entries=os44_ufsfbo_sysfs_entries;
 kobject_init(&fbo->kobj,&os44_ufsfbo_ktype);
 {static struct lock_class_key __key;__mutex_init(&fbo->lock,"&fbo->sysfs_lock",&__key);}
 if(fbo->debug==1)printk("\0016[info]%s:%d:Creates sysfs %p dev->kobj %p\n","ufsfbo_create_sysfs",925,&fbo->kobj,parent);
 ret=kobject_add(&fbo->kobj,kobject_get(parent),"ufsfbo");
 if(ret){printk("\0013[err]%s:%d:Kobject_add failed\n","ufsfbo_create_sysfs",944);return ret;}
 for(entry=fbo->entries;entry->attr.name;entry++) {
  if(fbo->debug==1)printk("\0016[info]%s:%d:Sysfs attr creates: %s\n","ufsfbo_create_sysfs",934,entry->attr.name);
  if(sysfs_create_file(&fbo->kobj,&entry->attr)) {
   printk("\0013[err]%s:%d:Create entry(%s) failed\n","ufsfbo_create_sysfs",938,entry->attr.name);
   ret=kobject_uevent(&fbo->kobj,KOBJ_REMOVE);
   if(fbo->debug&1)printk("\0016[info]%s:%d:Kobject removed (%d)\n","ufsfbo_create_sysfs",950,ret);
   kobject_del(&fbo->kobj);return -EINVAL;
  }
 }
 kobject_uevent(&fbo->kobj,KOBJ_ADD);return 0;
}
