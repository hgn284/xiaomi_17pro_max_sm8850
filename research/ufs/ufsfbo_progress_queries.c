// SPDX-License-Identifier: GPL-2.0-only
/* Whole stock reconstruction; PM failures intentionally retain stock semantics. */
#include <linux/pm_runtime.h>
#include "os44_fbo_progress.h"
#include "ufsswap_query_reconstruction.h"
static __always_inline int query(struct ufs_hba *hba,u8 opcode,u8 idn,u32 *value)
{
 int ret=os44_query_attr(hba,opcode,idn,0,0,value);
 if(ret) {
  printk("\0013[err]%s:%d:Query attr [0x%.2X] failed. opcode(%d) (%d)\n","ufsfbo_query_attr",55,idn,opcode,ret);
  os44_query_error(1,0,-1,0);
 }
 pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
 return ret;
}
int os44_ufsfbo_get_fbo_prog_state(struct os44_fbo_lifetime *fbo,u32 *state)
{
 struct ufs_hba *hba=fbo->hba;
 u32 value=~0U;
 int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 ret=query(hba,3,0x33,&value);
 if(ret) {
  printk("\0013[err]%s:%d:Get fbo_prog_state failed, ret(%d)\n","ufsfbo_get_fbo_prog_state",362,ret);
  return ret;
 }
 if(value<=16 || value==255) {*state=value;return 0;}
 if(fbo->debug==1)printk("\0016[info]%s:%d:fbo unknown prog state attr(%d)\n","ufsfbo_get_fbo_prog_state",388,value);
 return -1;
}
int os44_ufsfbo_get_exe_level(struct os44_fbo_lifetime *fbo,u32 *level)
{
 struct ufs_hba *hba=fbo->hba;
 u8 is_ufs31=fbo->is_ufs31;
 int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 if(is_ufs31)ret=query(hba,3,0x38,level);
 else ret=query(hba,3,0x32,level);
 if(ret)printk("\0013[err]%s:%d:Get_exe_level failed, ret(%d)\n","ufsfbo_get_exe_level",248,ret);
 return ret;
}
int os44_ufsfbo_set_exe_level(struct os44_fbo_lifetime *fbo,u32 *level)
{
 struct ufs_hba *hba=fbo->hba;
 u8 is_ufs31=fbo->is_ufs31;
 int ret;
 pm_runtime_get_sync(&hba->ufs_device_wlun->sdev_gendev);
 if(is_ufs31)ret=query(hba,4,0x38,level);
 else ret=query(hba,4,0x32,level);
 if(ret)printk("\0013[err]%s:%d:Set_exe_level failed, ret(%d)\n","ufsfbo_set_exe_level",263,ret);
 return ret;
}
