// SPDX-License-Identifier: GPL-2.0-only
/* Reconstructed original tables and whole status path; private type identity unresolved. */
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include "os44_snapshot.h"
struct os44_status_entry { u32 code; char name[50]; };
struct os44_status_map { u8 vendor,count; struct os44_status_entry *entries; };
static_assert(sizeof(struct os44_status_entry)==56);
static_assert(sizeof(struct os44_status_map)==16);
static struct os44_status_entry status_hynix[] = {
 {0,"idle"},
 {1,"idle_auto"},
 {2,"on_going_backup"},
 {3,"complete_backup"},
 {4,"on_going_restore"},
 {5,"complete_restore"},
 {6,"general_error"},
};
static struct os44_status_entry status_micron[] = {
 {0,"idle"},
 {1,"complete_backup"},
 {2,"invalid_backup"},
};
static struct os44_status_entry status_kioxia[] = {
 {0,"idle"},
 {1,"complete_backup"},
 {2,"invalid_backup"},
};
static struct os44_status_entry status_samsung[] = {
 {0,"idle"},
 {1,"wait_for_lba_range"},
 {2,"lba_range_ok"},
 {3,"on_going_backup"},
 {4,"complete_backup"},
 {5,"on_going_recovery"},
 {6,"backup_failed"},
 {7,"recovery_failed"},
};
static struct os44_status_entry status_ymtc[] = {
 {0,"idle"},
 {1,"in_progress"},
 {2,"backup_complete"},
 {3,"restore_complete"},
 {4,"drop_complete"},
 {255,"internal_error"},
};
static const struct os44_status_map vmaps[] = {
 {0,7,status_hynix},
 {1,3,status_micron},
 {2,3,status_kioxia},
 {3,8,status_samsung},
 {4,6,status_ymtc},
};
struct os44_snapshot_idns { u8 vendor,idn1,idn2,drop; };
static const struct os44_snapshot_idns idn_maps[] = {
 {0,0x8a,0x8b,0x1d},{1,0x89,0x8b,0x8c},{2,0xa1,0xa3,0x1d},{3,0x9f,0xa0,0xa1},{4,0x22,0x23,0x24}};
int os44_ufssnapshot_query_status(struct os44_snapshot_view *s)
{
 u32 value=0;
 struct ufs_hba *hba=s->hba;
 const struct os44_snapshot_idns *p;
 int ret;
 switch(s->vendor) {
 case 0:p=&idn_maps[0];break;
 case 1:p=&idn_maps[1];break;
 case 2:p=&idn_maps[2];break;
 case 3:p=&idn_maps[3];break;
 case 4:p=&idn_maps[4];break;
 default:printk("\0013[err]%s: %d:[ufssnapshot] find idn failed, vendor %d\n","ufssnapshot_find_idn",318,s->vendor);return -1;
 }
 ret=os44_ufssnapshot_query_attr(hba,3,p->idn2,&value);
 if(ret){printk("\0013[err]%s: %d:[ufssnapshot] query status failed\n","ufssnapshot_query_status",747);return ret;}
 if(value>=7){printk("\0013[err]%s: %d:[ufssnapshot] device status is invalid\n","ufssnapshot_query_status",752);return -1;}
 s->status=value;return 0;
}
ssize_t os44_ufssnapshot_sysfs_show_status(struct os44_snapshot_view *s,char *buf)
{
 char name[50];
 const struct os44_status_map *p;
 unsigned int i;
 if(s->version>=0x200)return sysfs_emit(buf,"%s\n","none");
 if(os44_ufssnapshot_query_status(s))return sysfs_emit(buf,"%s\n","invalid");
 switch(s->vendor){
 case 0:p=&vmaps[0];break;
 case 1:p=&vmaps[1];break;
 case 2:p=&vmaps[2];break;
 case 3:p=&vmaps[3];break;
 case 4:p=&vmaps[4];break;
 default:printk("\0013[err]%s: %d:[ufssnapshot] query status map failed, vendorid %u\n\n","ufssnapshot_find_status_name",255,s->vendor);return sysfs_emit(buf,"%s\n","none");
 }
 for(i=0;i<p->count;i++)if(p->entries[i].code==s->status){strncpy(name,p->entries[i].name,50);return sysfs_emit(buf,"%s\n",name);}
 printk("\0013[err]%s: %d:[ufssnapshot] Bsnapshotstatus name not find, status %d\n\n","ufssnapshot_find_status_name",267,s->status);
 return sysfs_emit(buf,"%s\n","none");
}
