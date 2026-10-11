#include "ufsswap_query_reconstruction.h"
_Static_assert(__builtin_offsetof(struct scsi_device,inquiry)==0xf0,"inquiry pointer");
_Static_assert(__builtin_offsetof(struct ufs_hba,dev_info)==0x9f0,"dev_info base");
_Static_assert(sizeof(struct ufs_dev_info)>=0x38,"padding view extent");
static int stock_tp_bit(struct ufs_hba *hba)
{
 /* hba+a26 is dev_info+54: unnamed padding in STOCK BTF, not a named
  * reconstructed flag. Preserve observed halfword bit2; writer not yet found. */
 const unsigned char *bytes=(const unsigned char *)&hba->dev_info;
 return !!(bytes[54]&4);
}
int os44_ufsswap_probe(struct ufs_hba *hba,struct os44_swap_view *swap)
{
 swap->hba=hba;
 if(!hba->ufs_device_wlun) return -95;
 if(os44_ufsswap_select_model(swap,hba->ufs_device_wlun->inquiry+8)) return -95;
 if(!stock_tp_bit(hba) && !os44_ufsswap_support_tp(swap)) {
  if(os44_ufsswap_get_dev_info(hba,swap)) return -95;
  if(!swap->max_buf) {
   if(os44_ufsswap_config_buf(hba,swap)) return -95;
   if(os44_ufsswap_get_dev_info(hba,swap) || !swap->max_buf) return -95;
  }
  if(swap->second_value==0x200) (void)os44_ufsswap_min_buf(hba,swap);
 }
 swap->head.next=&swap->head;
 swap->head.prev=&swap->head;
 /* Original ignores sysfs failure; no silent behavioral repair. */
 (void)os44_ufsswap_create_sysfs(swap);
 swap->support_state=1;
 return 0;
}
