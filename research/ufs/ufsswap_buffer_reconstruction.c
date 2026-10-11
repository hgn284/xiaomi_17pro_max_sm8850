#include "ufsswap_query_reconstruction.h"
_Static_assert(__builtin_offsetof(struct ufs_hba,dev_info)+__builtin_offsetof(struct ufs_dev_info,wmanufacturerid)==0x9f4,"manufacturer");
static int attr(struct ufs_hba *hba,unsigned int op,unsigned char id,os44_u32 *value)
{
 int ret;
 __pm_runtime_resume(&hba->ufs_device_wlun->sdev_gendev,4);
 ret=os44_query_attr(hba,op,id,0,0,value);
 if(ret) os44_query_error(1,2,-1,0);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
int os44_ufsswap_get_wb_buf(struct ufs_hba *hba)
{
 os44_u32 value=0;
 if(attr(hba,3,0x1f,&value)) return -95;
 if(!value) return -5;
 return (int)(value<<2);
}
int os44_ufsswap_support_tp(struct os44_swap_view *swap)
{
 struct ufs_hba *hba=swap->hba;
 unsigned char *buf;
 os44_u32 flags=0;
 int len=255,ret;
 if(hba->dev_info.wmanufacturerid!=0x1ce) return 0;
 buf=os44_descriptor_alloc();
 if(!buf) return 0;
 __pm_runtime_resume(&hba->ufs_device_wlun->sdev_gendev,4);
 ret=os44_query_descriptor(hba,1,240,0,0,buf,&len);
 if(ret) os44_query_error(1,2,-1,0);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 if(!ret) {
  /* Stock reads the final four bytes irrespective of returned length. */
  flags=((os44_u32)buf[0xfb]<<24)|((os44_u32)buf[0xfc]<<16)|((os44_u32)buf[0xfd]<<8)|buf[0xfe];
  if(flags & (1U<<10)) swap->config->write10_cdb6=0x12;
 }
 os44_descriptor_free(buf);
 return (flags>>10)&1;
}
int os44_ufsswap_config_buf(struct ufs_hba *hba,struct os44_swap_view *swap)
{
 /* Preserve 32-bit unsigned compare, including negative get_wb errno. */
 os44_u32 wb=(os44_u32)os44_ufsswap_get_wb_buf(hba), value;
 if(wb<=swap->model_index*19U) return -95;
 value=0xa00;
 if(attr(hba,4,swap->config->attr_max,&value)) return -5;
 value=swap->config->attr_second==0x9b?0xa00:0x200;
 if(attr(hba,4,swap->config->attr_second,&value)) return -5;
 if(swap->config->attr_flush) (void)os44_flush_priority(swap);
 return 0;
}
int os44_ufsswap_min_buf(struct ufs_hba *hba,struct os44_swap_view *swap)
{
 os44_u32 wb=(os44_u32)os44_ufsswap_get_wb_buf(hba),value=0xa00;
 if(swap->model_index==0x100 && swap->config->attr_second==0x9b && (wb>>10)>=5)
  (void)attr(hba,4,0x9b,&value);
 return 0; /* Original ignores attribute error after recording it. */
}
