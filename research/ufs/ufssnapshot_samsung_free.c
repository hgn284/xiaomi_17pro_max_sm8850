// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include "os44_snapshot.h"
int os44_samsung_get_free_space(struct os44_snapshot_view *s, u32 *value)
{
 int ret = os44_ufssnapshot_query_attr(s->hba, 3, 0xa1, value);
 if (ret)
  printk("\0013[err]%s: %d:[ufssnapshot] samsung query freespace failed\n", "samsung_get_free_space", 241);
 return ret;
}
