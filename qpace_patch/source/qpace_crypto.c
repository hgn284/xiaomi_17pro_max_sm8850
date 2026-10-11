// SPDX-License-Identifier: GPL-2.0-only
/* User9838 native hardware codec. No software format assumption or fallback.
 * Original zram remains the owner of raw/huge pages and swap lifecycle. */
#include <linux/module.h>
#include <linux/crypto.h>
#include <crypto/internal/scompress.h>
#include <linux/mm.h>
#include <linux/atomic.h>
#include "canoe_api.h"

#define COUNTER(name) static atomic64_t name = ATOMIC64_INIT(0)
COUNTER(init_calls); COUNTER(active_tfms); COUNTER(backend_calls);
COUNTER(hw_accepted); COUNTER(hw_rejected); COUNTER(hw_reads);
COUNTER(raw_pages); COUNTER(decode_errors);
COUNTER(hw_accepted_input_bytes); COUNTER(hw_accepted_output_bytes);
COUNTER(decoded_bytes);
static int init_last_error;
module_param(init_last_error, int, 0444);
static bool native_hardware_only = true;
module_param(native_hardware_only, bool, 0444);
static int counter_get(char *buf, const struct kernel_param *kp)
{ return scnprintf(buf, PAGE_SIZE, "%lld\n", (long long)atomic64_read(kp->arg)); }
static const struct kernel_param_ops counter_ops = { .get = counter_get };
#define EXPOSE(name) module_param_cb(name, &counter_ops, &name, 0444)
EXPOSE(init_calls); EXPOSE(active_tfms); EXPOSE(backend_calls);
EXPOSE(hw_accepted); EXPOSE(hw_rejected); EXPOSE(hw_reads);
EXPOSE(raw_pages); EXPOSE(decode_errors);
EXPOSE(hw_accepted_input_bytes); EXPOSE(hw_accepted_output_bytes); EXPOSE(decoded_bytes);
struct qpace_comp_ctx { struct canoe_engine *engine; };
static int qpace_comp_init(struct crypto_tfm *tfm)
{
 struct qpace_comp_ctx *c = crypto_tfm_ctx(tfm);
 int ret;
 atomic64_inc(&init_calls);
 if (PAGE_SIZE != 4096) { ret = -EOPNOTSUPP; goto fail; }
 c->engine = canoe_qpace_open();
 if (IS_ERR(c->engine)) {
  ret = PTR_ERR(c->engine); c->engine = NULL; goto fail;
 }
 atomic64_inc(&active_tfms);
 WRITE_ONCE(init_last_error, 0);
 return 0;
fail:
 WRITE_ONCE(init_last_error, ret);
 return ret;
}
static void qpace_comp_exit(struct crypto_tfm *tfm)
{
 struct qpace_comp_ctx *c = crypto_tfm_ctx(tfm);
 canoe_qpace_close(c->engine);
 c->engine = NULL;
 atomic64_dec(&active_tfms);
}
static int qpace_comp_compress(struct crypto_tfm *tfm, const u8 *src,
 unsigned int slen, u8 *dst, unsigned int *dlen)
{
 struct qpace_comp_ctx *c = crypto_tfm_ctx(tfm);
 int n;
 if (!src || !dst || !dlen || slen != PAGE_SIZE || *dlen < PAGE_SIZE)
  return -EINVAL;
 atomic64_inc(&backend_calls);
 n = canoe_qpace_page(c->engine, false, src, PAGE_SIZE, dst, *dlen);
 if (n == -E2BIG || n == PAGE_SIZE) {
  /* Real raw bytes, not an invented successful compressed payload. Zram's
   * existing huge-page path stores original data without codec decoding. */
  memcpy(dst, src, PAGE_SIZE);
  *dlen = PAGE_SIZE;
  atomic64_inc(&raw_pages);
  return 0;
 }
 if (n <= 0 || n > PAGE_SIZE) {
  atomic64_inc(&hw_rejected);
  return n < 0 ? n : -EIO;
 }
 *dlen = n;
 atomic64_inc(&hw_accepted);
 atomic64_add(PAGE_SIZE, &hw_accepted_input_bytes);
 atomic64_add(n, &hw_accepted_output_bytes);
 return 0;
}
static int qpace_comp_decompress(struct crypto_tfm *tfm, const u8 *src,
 unsigned int slen, u8 *dst, unsigned int *dlen)
{
 struct qpace_comp_ctx *c = crypto_tfm_ctx(tfm);
 int n;
 if (!src || !dst || !dlen || !slen || slen > PAGE_SIZE || *dlen < PAGE_SIZE)
  return -EINVAL;
 /* Defensive raw convention for full-page direct Crypto API consumers. */
 if (slen == PAGE_SIZE) { memcpy(dst, src, PAGE_SIZE); *dlen = PAGE_SIZE; return 0; }
 n = canoe_qpace_page(c->engine, true, src, slen, dst, *dlen);
 if (n != PAGE_SIZE) { atomic64_inc(&decode_errors); return n < 0 ? n : -EIO; }
 *dlen = PAGE_SIZE;
 atomic64_inc(&hw_reads);
 atomic64_add(PAGE_SIZE, &decoded_bytes);
 return 0;
}
static struct crypto_alg qpace_comp_alg = {
 .cra_name = "qpace-canoe", .cra_driver_name = "qpace-canoe-native-hw",
 .cra_flags = CRYPTO_ALG_TYPE_COMPRESS,
 .cra_ctxsize = sizeof(struct qpace_comp_ctx), .cra_module = THIS_MODULE,
 .cra_init = qpace_comp_init, .cra_exit = qpace_comp_exit,
 .cra_u = { .compress = { .coa_compress = qpace_comp_compress,
                         .coa_decompress = qpace_comp_decompress } },
};
static int __init qpace_comp_register(void) { return crypto_register_alg(&qpace_comp_alg); }
static void __exit qpace_comp_unregister(void) { crypto_unregister_alg(&qpace_comp_alg); }
module_init(qpace_comp_register);
module_exit(qpace_comp_unregister);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OS44 forced native QPaCE hardware COMP/DECOMP trial9838");
MODULE_ALIAS_CRYPTO("qpace-canoe");
