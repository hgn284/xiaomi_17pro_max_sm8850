/* SPDX-License-Identifier: GPL-2.0-only */
/* Exact complete ufs_debugfs_hba_exit; does NOT drain ufs_data save/load.
 * Full owner wiring is still required. No fake debug or singleton globals. */
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <ufs/ufshcd.h>
static_assert(offsetof(struct ufs_hba, debugfs_root)==0x1290);
static_assert(offsetof(struct ufs_hba, debugfs_ee_work)==0x1298);
void os44_debugfs_hba_exit(struct ufs_hba *hba)
{
 debugfs_remove(hba->debugfs_root);
 cancel_delayed_work_sync(&hba->debugfs_ee_work);
}
