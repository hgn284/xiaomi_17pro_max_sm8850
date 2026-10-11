#include "ufsswap_query_reconstruction.h"
#define OFFSET(t,m) __builtin_offsetof(t,m)
_Static_assert(OFFSET(struct ufs_hba,ufs_device_wlun)==0x48,"HBA WLUN");
_Static_assert(OFFSET(struct scsi_device,sdev_gendev)==0x1d0,"SCSI device");
_Static_assert(OFFSET(struct device,power)+OFFSET(struct dev_pm_info,usage_count)==0x1e0,"PM counter");
_Static_assert(OFFSET(struct os44_swap_view,max_buf)==8,"max value");
_Static_assert(OFFSET(struct os44_swap_view,second_value)==12,"second value");
_Static_assert(sizeof(struct os44_swap_config_view)==9,"config extent");
static int read_attr(struct ufs_hba *hba,unsigned char attr,os44_u32 *value)
{
 int ret;
 /* Stock ignores resume return; retain rather than silently repair. */
 __pm_runtime_resume(&hba->ufs_device_wlun->sdev_gendev,4);
 ret=os44_query_attr(hba,3,attr,0,0,value);
 if(ret) os44_query_error(1,2,-1,0);
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
int os44_ufsswap_get_dev_info(struct ufs_hba *hba,struct os44_swap_view *swap)
{
 os44_u32 value=0; int ret;
 if(!hba->ufs_device_wlun) return -1;
 if(!swap->config->attr_max) return os44_get_swap_descriptor(swap);
 ret=read_attr(hba,swap->config->attr_max,&value);
 if(ret) return ret;
 swap->max_buf=value;
 if(swap->config->attr_second) {
  ret=read_attr(hba,swap->config->attr_second,&value);
  if(ret) return ret;
 }
 /* config1==0 reuses first query value, not zero. */
 swap->second_value=value;
 if(swap->max_buf && swap->config->attr_flush) (void)os44_flush_priority(swap);
 return 0;
}
