// SPDX-License-Identifier: GPL-2.0-only
#include "os44_eua.h"
ssize_t os44_ufseua_sysfs_show_eua_support(struct os44_eua_view *eua,char *buf)
{return sysfs_emit(buf,"%d\n",eua->support);}
ssize_t os44_ufseua_sysfs_show_eua_enable(struct os44_eua_view *eua,char *buf)
{return sysfs_emit(buf,"%d\n",eua->enable);}
ssize_t os44_ufseua_sysfs_show_eua_hide_size(struct os44_eua_view *eua,char *buf)
{return sysfs_emit(buf,"%d\n",eua->hide_size);}
ssize_t os44_ufseua_sysfs_show_eua_support_size(struct os44_eua_view *eua,char *buf)
{
 int value=0;
 switch(eua->support_size) {case 0x108:case 0x208:value=8;break;case 0x210:value=16;break;case 0x420:value=32;break;}
 return sysfs_emit(buf,"%d\n",value);
}
ssize_t os44_ufseua_sysfs_show_eua_get_total_used_lba(struct os44_eua_view *eua,char *buf)
{
 u32 value=0;
 if(eua->ops && eua->ops->get_total_used_lba && !eua->ops->get_total_used_lba(eua,&value))return sysfs_emit(buf,"%d\n",value);
 printk("\0013[UFSEUA]Get total used lba failed");return -EINVAL;
}
ssize_t os44_ufseua_sysfs_show_eua_get_total_free_space(struct os44_eua_view *eua,char *buf)
{
 u32 value=0;
 if(eua->ops && eua->ops->get_total_free_space && !eua->ops->get_total_free_space(eua,&value))return sysfs_emit(buf,"%d\n",value);
 printk("\0013[UFSEUA]Get total free space failed");return -EINVAL;
}
