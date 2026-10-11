#include "ufsswap_query_reconstruction.h"
int os44_flush_priority(struct os44_swap_view *swap)
{
 struct ufs_hba *hba=swap->hba;
 os44_u32 value=swap->config->flush_value;
 unsigned char attr=swap->config->attr_flush;
 int ret;
 __pm_runtime_resume(&hba->ufs_device_wlun->sdev_gendev,4);
 ret=os44_query_attr(hba,4,attr,0,0,&value);
 if(ret) os44_query_error(1,2,-1,0);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
int os44_get_swap_descriptor(struct os44_swap_view *swap)
{
 struct ufs_hba *hba=swap->hba;
 unsigned char *buf=os44_descriptor_alloc();
 int size=255,ret;
 if(!buf) return -12;
 __pm_runtime_resume(&hba->ufs_device_wlun->sdev_gendev,4);
 ret=os44_query_descriptor(hba,1,240,0,0,buf,&size);
 if(ret) os44_query_error(1,2,-1,0);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 if(!ret) {
  /* Original reads this field regardless of returned descriptor length. */
  swap->max_buf=((os44_u32)buf[0xe7]<<24)|((os44_u32)buf[0xe8]<<16)|((os44_u32)buf[0xe9]<<8)|buf[0xea];
  /* Descriptor branch returns flush error; attribute branch ignores it. */
  ret=os44_flush_priority(swap);
 }
 os44_descriptor_free(buf);
 return ret;
}
