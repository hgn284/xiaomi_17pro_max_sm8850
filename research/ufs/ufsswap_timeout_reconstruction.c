/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/module.h>
#include <linux/moduleparam.h>
/* Stock .data+0x36c0=5500, although its stale help text says1500. */
unsigned int os44_core_dev_cmd_timeout=5500;
int os44_core_timeout_set(const char *val,const struct kernel_param *kp)
{
 return param_set_uint_minmax(val,kp,1,30000);
}
static const struct kernel_param_ops os44_timeout_ops={
 .set=os44_core_timeout_set, .get=param_get_uint,
};
module_param_cb(dev_cmd_timeout,&os44_timeout_ops,&os44_core_dev_cmd_timeout,0644);
MODULE_PARM_DESC(dev_cmd_timeout,"UFS Device command timeout in milliseconds, initial5500, range1..30000");
