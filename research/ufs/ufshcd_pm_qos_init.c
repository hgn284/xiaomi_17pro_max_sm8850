/* SPDX-License-Identifier: GPL-2.0-only */
/* Real per-HBA request and mutex; initialized by the pending full init owner. */
#include "os44_crypto_init.h"
#include "drivers/ufs/core/ufshcd-priv.h"
static_assert(offsetof(struct ufs_hba_priv, pm_qos_mutex)==0x1500);
static_assert(offsetof(struct ufs_hba, pm_qos_req)==0x14a8);
static_assert(offsetof(struct ufs_hba, pm_qos_enabled)==0x14d8);
void os44_ufshcd_pm_qos_init(struct ufs_hba *hba)
{
 guard(mutex)(&to_hba_priv(hba)->pm_qos_mutex);
 if (hba->pm_qos_enabled)
  return;
 cpu_latency_qos_add_request(&hba->pm_qos_req, PM_QOS_DEFAULT_VALUE);
 if (cpu_latency_qos_request_active(&hba->pm_qos_req))
  hba->pm_qos_enabled = true;
}
