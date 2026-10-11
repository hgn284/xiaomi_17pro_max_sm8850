// SPDX-License-Identifier: GPL-2.0-only
/* Experimental Canoe urgent protocol, derived from MiCode 45705be1.
 * Serial single-slot prototype; not a performance implementation.
 * Builtin OS44 activation defaults on; exact target/resource gates remain.
 * Probe queues lifecycle-owned delayed selftest rounds; DT stays disabled.
 */
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/interconnect.h>
#include <linux/iommu.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/wait.h>
#include <linux/os44_qpace.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <linux/bitfield.h>
#include <dt-bindings/interconnect/qcom,icc.h>
#include "canoe_api.h"
#include <linux/scatterlist.h>

#define GEN 0x1000
#define COMP 0x2000
#define URG 0x10000
#define RUN 0x0
#define READY 0x4
#define CTX_SIZE 0x40
#define CTX_MISC 0x48
#define STATUS 0x18
#define CODE GENMASK(23, 20)
#define SIZE GENMASK(19, 0)
#define ONGOING 15
#define COMMAND_TIMEOUT_US 1000
#define POWER_TIMEOUT_US 50000

/* Transport observations include isolated selftest traffic. Use deltas after
 * selftest for zram traffic; submission is not hardware completion/acceptance. */
#define HW_COUNTER(name) static atomic64_t name = ATOMIC64_INIT(0)
HW_COUNTER(hw_submit_comp); HW_COUNTER(hw_submit_decomp);
HW_COUNTER(hw_complete_comp); HW_COUNTER(hw_complete_decomp);
HW_COUNTER(hw_comp_input_bytes); HW_COUNTER(hw_comp_output_bytes);
HW_COUNTER(hw_decomp_output_bytes); HW_COUNTER(selftest_lz4_pages);
HW_COUNTER(selftest_format_errors);
static int hw_counter_get(char *buf, const struct kernel_param *kp)
{ return scnprintf(buf, PAGE_SIZE, "%lld\n", (long long)atomic64_read(kp->arg)); }
static const struct kernel_param_ops hw_counter_ops = { .get = hw_counter_get };
#define HW_EXPOSE(name) module_param_cb(name, &hw_counter_ops, &name, 0444)
HW_EXPOSE(hw_submit_comp); HW_EXPOSE(hw_submit_decomp);
HW_EXPOSE(hw_complete_comp); HW_EXPOSE(hw_complete_decomp);
HW_EXPOSE(hw_comp_input_bytes); HW_EXPOSE(hw_comp_output_bytes);
HW_EXPOSE(hw_decomp_output_bytes); HW_EXPOSE(selftest_lz4_pages);
HW_EXPOSE(selftest_format_errors);

/* Forced mode skips the diagnostic batch, NOT actual hardware errors.
 * selftest_result remains ENODATA: skipped is never reported as PASS. */
static bool force_mode = true;
module_param(force_mode, bool, 0400);
static bool hardware_ready;
module_param(hardware_ready, bool, 0400);
static DECLARE_WAIT_QUEUE_HEAD(hardware_wait);
int os44_qpace_wait_ready(void)
{
 return wait_event_timeout(hardware_wait, READ_ONCE(hardware_ready),
                          msecs_to_jiffies(90000)) ? 0 : -ETIMEDOUT;
}
static int selftest_result = -ENODATA;
module_param(selftest_result, int, 0400);
MODULE_PARM_DESC(selftest_result, "0 only after completed selftest and stop; negative means untested or failed");
static bool test_mode = IS_ENABLED(CONFIG_OS44_QPACE);
module_param(test_mode, bool, 0400);
MODULE_PARM_DESC(test_mode, "Allow isolated-memory protocol test; builtin OS44 defaults on");
static bool client_mode = IS_ENABLED(CONFIG_OS44_QPACE);
module_param(client_mode, bool, 0400);
MODULE_PARM_DESC(client_mode, "Development client admission; not yet a phone swap release");
struct canoe_engine {
 struct device *dev;
 void __iomem *regs;
 struct icc_path *icc;
 struct dev_pm_qos_request qos;
 struct kref refs;
 raw_spinlock_t lock;
 void *in, *out;
 dma_addr_t in_dma, out_dma;
 bool online, broken, quarantine, voted, qos_added, tested;
 unsigned int clients;
 bool suspended, removing, test_cancel, test_pending;
 struct delayed_work selftest_work;
 unsigned int test_round, passed[6], incompressible[6];
 unsigned long irq_disabled;
 int irqs[2], irq_count;
};
static struct workqueue_struct *selftest_wq;
static unsigned int selftest_completed_rounds;
module_param(selftest_completed_rounds, uint, 0400);
static DEFINE_MUTEX(registry_lock);
static struct canoe_engine *engine;
static bool hardware_quarantined;
module_param_named(quarantined, hardware_quarantined, bool, 0400);

/* Disable ownership shared with IRQ handler; never nest disable depth. */
static void pause_irqs(struct canoe_engine *e)
{
 int i;
 for (i = 0; i < e->irq_count; i++) {
  if (!test_and_set_bit(i, &e->irq_disabled)) disable_irq(e->irqs[i]);
  else synchronize_irq(e->irqs[i]);
 }
}
static void resume_irqs(struct canoe_engine *e)
{
 int i;
 for (i = 0; i < e->irq_count; i++)
  if (test_and_clear_bit(i, &e->irq_disabled)) enable_irq(e->irqs[i]);
}
static int start_engine(struct canoe_engine *e)
{
 u32 v;
 int ret;
 ret = icc_set_bw(e->icc, 0, 1);
 if (ret) return ret;
 e->voted = true;
 writel(1, e->regs + GEN + RUN);
 ret = readl_poll_timeout(e->regs + GEN + READY, v, v != 0,
                         10, POWER_TIMEOUT_US);
 if (ret) return ret;
 /* Vendor CPU SCID1; no LLCC95/96 or urgent index36 override. */
 v = readl(e->regs + 0x14);
 writel((v & ~GENMASK(17, 12)) | FIELD_PREP(GENMASK(17, 12), 1), e->regs + 0x14);
 v = readl(e->regs + COMP + 4);
 writel(v | GENMASK(4, 0), e->regs + COMP + 4);
 v = readl(e->regs + GEN + 8);
 writel(v | BIT(3), e->regs + GEN + 8);
 /* Only slot0 is used, serialized independently of Linux CPU numbering. */
 writel(PAGE_SIZE - 1, e->regs + URG + CTX_SIZE);
 writel((1 << 4) | BIT(21), e->regs + URG + CTX_MISC);
 writel(0, e->regs + URG + CTX_SIZE + 0x10);
 writel((2 << 4) | BIT(21), e->regs + URG + CTX_MISC + 0x10);
 return 0;
}
static int stop_engine(struct canoe_engine *e)
{
 u32 v;
 int ret;
 if (!e->voted) return 0;
 writel(0, e->regs + GEN + RUN);
 ret = readl_poll_timeout(e->regs + GEN + READY, v, v == 0,
                         10, POWER_TIMEOUT_US);
 if (ret) {
  /* No READY acknowledgement: never return possibly-DMA-owned RAM. */
  e->quarantine = true;
  if (!hardware_quarantined) {
   hardware_quarantined = true;
   /* Preserve the fault latch across attempted module unload/reload. */
   __module_get(THIS_MODULE);
  }
  return ret;
 }
 ret = icc_set_bw(e->icc, 0, 0);
 if (!ret) e->voted = false;
 return ret;
}
static void release_engine(struct kref *ref)
{
 struct canoe_engine *e = container_of(ref, struct canoe_engine, refs);
 if (e->quarantine) {
  dev_err(e->dev, "DMA quiescence unproved: retaining buffers/device/ICC; reboot required\n");
  return;
 }
 if (e->in) dma_free_coherent(e->dev, 2 * PAGE_SIZE, e->in, e->in_dma);
 if (e->out) dma_free_coherent(e->dev, 2 * PAGE_SIZE, e->out, e->out_dma);
 if (e->qos_added) dev_pm_qos_remove_request(&e->qos);
 if (!IS_ERR_OR_NULL(e->icc)) icc_put(e->icc);
 put_device(e->dev);
 kfree(e);
}
struct canoe_engine *canoe_qpace_open(void)
{
 struct canoe_engine *e;
 unsigned long flags;
 int ret = -ENODEV;
 mutex_lock(&registry_lock);
 e = engine;
 if (!client_mode) { ret = -EOPNOTSUPP; goto fail; }
 if (!e) goto fail;
 /* EAGAIN makes Crypto API shoot/retry the registered algorithm. */
 if (!force_mode && !e->tested) { ret = -EBUSY; goto fail; }
 if (e->suspended) { ret = -EBUSY; goto fail; }
 if (READ_ONCE(e->broken) || e->quarantine) { ret = -EIO; goto fail; }
 if (!try_module_get(THIS_MODULE)) goto fail;
 if (!e->clients && !e->voted) {
  ret = start_engine(e);
  if (ret) { stop_engine(e); module_put(THIS_MODULE); goto fail; }
  resume_irqs(e);
 }
 raw_spin_lock_irqsave(&e->lock, flags);
 e->online = true;
 raw_spin_unlock_irqrestore(&e->lock, flags);
 kref_get(&e->refs);
 e->clients++;
 mutex_unlock(&registry_lock);
 return e;
fail:
 mutex_unlock(&registry_lock);
 return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(canoe_qpace_open);
void canoe_qpace_close(struct canoe_engine *e)
{
 unsigned long flags;
 if (IS_ERR_OR_NULL(e)) return;
 mutex_lock(&registry_lock);
 if (WARN_ON(!e->clients)) { mutex_unlock(&registry_lock); return; }
 if (!--e->clients) {
  raw_spin_lock_irqsave(&e->lock, flags);
  e->online = false;
  raw_spin_unlock_irqrestore(&e->lock, flags);
  if (!e->removing) { pause_irqs(e); stop_engine(e); }
 }
 kref_put(&e->refs, release_engine);
 mutex_unlock(&registry_lock);
 module_put(THIS_MODULE);
}
EXPORT_SYMBOL_GPL(canoe_qpace_close);

int canoe_qpace_page(struct canoe_engine *e, bool decompress,
 const void *src, size_t len, void *dst, size_t capacity)
{
 unsigned long flags;
 u32 status, code, size;
 int ret;
 if (!e || !src || !dst || !len || len > PAGE_SIZE ||
     (!decompress && len != PAGE_SIZE) || capacity < PAGE_SIZE)
  return -EINVAL;
 raw_spin_lock_irqsave(&e->lock, flags);
 if (!e->online || READ_ONCE(e->broken)) { ret = -EIO; goto out; }
 memset(e->in, 0, 2 * PAGE_SIZE);
 memcpy(e->in, src, len);
 memset(e->out, 0, PAGE_SIZE);
 memset(e->out + PAGE_SIZE, 0xa5, PAGE_SIZE);
 if (decompress)
  writel(len, e->regs + URG + CTX_SIZE + 0x10);
 dma_wmb();
 /* MiCode disjoint-write sequence. Destination alignment>=256 bytes. */
 writel(lower_32_bits(e->out_dma) | (decompress ? 1 : 0), e->regs + URG);
 writel(upper_32_bits(e->out_dma), e->regs + URG + 4);
 writel(lower_32_bits(e->in_dma), e->regs + URG + 8);
 /* Final source-address write triggers the request (MiCode protocol). */
 if (decompress) atomic64_inc(&hw_submit_decomp);
 else atomic64_inc(&hw_submit_comp);
 writel(upper_32_bits(e->in_dma), e->regs + URG + 12);
 ret = readl_poll_timeout_atomic(e->regs + URG + STATUS, status,
                FIELD_GET(CODE, status) != ONGOING, 1, COMMAND_TIMEOUT_US);
 if (ret) { WRITE_ONCE(e->broken, true); goto out; }
 code = FIELD_GET(CODE, status);
 dma_rmb();
 if (memchr_inv(e->out + PAGE_SIZE, 0xa5, PAGE_SIZE)) {
  WRITE_ONCE(e->broken, true); ret = -EOVERFLOW; goto out;
 }
 if (code == 2 && !decompress) { ret = -E2BIG; goto out; }
 if (code || READ_ONCE(e->broken)) {
  WRITE_ONCE(e->broken, true); ret = -EIO; goto out;
 }
 size = FIELD_GET(SIZE, status);
 if (!size || size > PAGE_SIZE || (decompress && size != PAGE_SIZE)) {
  WRITE_ONCE(e->broken, true); ret = -EIO; goto out;
 }
 memcpy(dst, e->out, size);
 if (decompress) {
  atomic64_inc(&hw_complete_decomp);
  atomic64_add(size, &hw_decomp_output_bytes);
 } else {
  atomic64_inc(&hw_complete_comp);
  atomic64_add(len, &hw_comp_input_bytes);
  atomic64_add(size, &hw_comp_output_bytes);
 }
 ret = size;
out:
 raw_spin_unlock_irqrestore(&e->lock, flags);
 return ret;
}
EXPORT_SYMBOL_GPL(canoe_qpace_page);

static irqreturn_t fault_irq(int irq, void *data)
{
 struct canoe_engine *e = data;
 /* Prototype: no ring machinery, any unexpected interrupt fences requests.
  * Do not guess an acknowledgement sequence or panic in the handler.
  */
 WRITE_ONCE(e->broken, true);
 { int i; for (i = 0; i < e->irq_count; i++)
   if (e->irqs[i] == irq && !test_and_set_bit(i, &e->irq_disabled))
    disable_irq_nosync(irq); }
 dev_err_ratelimited(e->dev, "unexpected QPaCE IRQ%d; engine fenced\n", irq);
 return IRQ_HANDLED;
}
/* One CPU-hotplug-protected round. Scheduling/pacing lives outside probe. */
static long memory_selftest_round(void *data)
{
 struct canoe_engine *e = data;
 u8 *src, *compressed, *decoded;
 unsigned int round = e->test_round, pattern, j;
 int ret = -ENOMEM, n, m;
 src = kmalloc(PAGE_SIZE * 3, GFP_KERNEL);
 if (!src) return ret;
 compressed = src + PAGE_SIZE;
 decoded = compressed + PAGE_SIZE;
  for (pattern = 0; pattern < 6; pattern++) {
   if (pattern == 3) get_random_bytes(src, PAGE_SIZE);
   else for (j = 0; j < PAGE_SIZE; j++) {
    switch (pattern) {
    case 0: src[j] = 0; break;
    case 1: src[j] = 0xff; break;
    case 2: src[j] = 0x5a; break;
    case 4: src[j] = (u8)(j + round); break;
    default: src[j] = j % 257 == round ? 0xa5 : 0; break;
    }
   }
   n = canoe_qpace_page(e, false, src, PAGE_SIZE, compressed, PAGE_SIZE);
   if (n == -E2BIG && pattern == 3) { e->incompressible[pattern]++; continue; }
   if (n < 0) { ret = n; goto done; }
   memset(decoded, 0x69, PAGE_SIZE);
   m = canoe_qpace_page(e, true, compressed, n, decoded, PAGE_SIZE);
   if (m != PAGE_SIZE || memcmp(src, decoded, PAGE_SIZE)) {
    ret = m < 0 ? m : -EILSEQ; goto done;
   }
   /* Native COMP/DECOMP format: no software-LZ4 interpretation. */
   if (n >= PAGE_SIZE && pattern != 3) { ret = -E2BIG; goto done; }
   e->passed[pattern]++;
  }
 ret = 0;
done:
 if (ret) dev_err(e->dev, "batch9192 failed round=%u pattern=%u error=%d\n", round, pattern, ret);
 kfree(src);
 return ret;
}
static int validate_resources(struct platform_device *pdev)
{
 struct resource *r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
 struct of_phandle_args arg;
 int i, ret;
 if (!r || r->start != 0x31400000 || resource_size(r) != 0x80000)
  return -EINVAL;
 if (of_count_phandle_with_args(pdev->dev.of_node, "iommus", "#iommu-cells") != 4)
  return -EINVAL;
 for (i = 0; i < 4; i++) {
  ret = of_parse_phandle_with_args(pdev->dev.of_node, "iommus", "#iommu-cells", i, &arg);
  if (ret) return ret;
  ret = arg.args_count == 2 && arg.args[0] == 0x2020 + i * 4 && arg.args[1] == 0x13 ? 0 : -EINVAL;
  of_node_put(arg.np);
  if (ret) return ret;
 }
 for (i = 0; i < 2; i++) {
  int idx = of_property_match_string(pdev->dev.of_node, "interrupt-names",
               i ? "urgent" : "ring-and-bus-err");
  if (idx < 0) return idx;
  ret = of_irq_parse_one(pdev->dev.of_node, idx, &arg);
  if (ret) return ret;
  ret = arg.args_count == 3 && arg.args[0] == 0 &&
        arg.args[1] == (i ? 226 : 224) && arg.args[2] == 4 ? 0 : -EINVAL;
  of_node_put(arg.np);
  if (ret) return ret;
 }
 return 0;
}
/* New translated-DMA experiment. No private Qualcomm registry/query.
 * This is NOT the former downstream-bypass configuration. The ordinary DMA
 * API creates mappings through the original modular IOMMU provider. Neither
 * this gate nor a software page-table query proves physical hardware routing.
 */
static int validate_dma_mode(struct device *dev, struct iommu_domain *domain)
{
 const char *mode;
 if (of_property_read_string(dev->of_node, "qcom,iommu-dma", &mode) ||
     strcmp(mode, "default"))
  return -EOPNOTSUPP;
 if (!domain) return -EPROBE_DEFER;
 if (domain->type != IOMMU_DOMAIN_DMA && domain->type != IOMMU_DOMAIN_DMA_FQ)
  return -EOPNOTSUPP;
 /* This kernel uses dma_iommu, not a non-NULL ops table, for generic DMA. */
 if (!dev->dma_iommu || get_dma_ops(dev)) return -EOPNOTSUPP;
 return 0;
}
/* Validate every page of both coherent buffers BEFORE requesting interrupts
 * or starting the QPaCE engine. The scatterlist is allocation metadata, not a
 * second DMA mapping. Never derive physical addresses from a CPU remap pointer.
 * Failure is terminal for this probe, including providers without this query.
 */
static int validate_dma_buffer(struct device *dev, struct iommu_domain *domain,
                               void *cpu, dma_addr_t dma, size_t size)
{
 struct sg_table sgt = {0};
 struct scatterlist *sg;
 size_t pos = 0, off;
 unsigned int i;
 int ret;
 if (!size || ((dma | size) & (PAGE_SIZE - 1)) ||
     dma > DMA_BIT_MASK(36) || size - 1 > DMA_BIT_MASK(36) - dma)
  return -EINVAL;
 ret = dma_get_sgtable(dev, &sgt, cpu, dma, size);
 if (ret) return ret;
 ret = -EFAULT;
 if (!sgt.sgl || !sgt.orig_nents) goto out;
 for_each_sg(sgt.sgl, sg, sgt.orig_nents, i) {
  if (!sg->length || (sg->length & (PAGE_SIZE - 1)) ||
      sg->offset || sg->length > size - pos)
   goto out;
  for (off = 0; off < sg->length; off += PAGE_SIZE) {
   phys_addr_t phys = sg_phys(sg) + off;
   dma_addr_t addr = dma + pos + off;
   if (!phys || iommu_iova_to_phys(domain, addr) != phys ||
       iommu_iova_to_phys(domain, addr + PAGE_SIZE - 1) != phys + PAGE_SIZE - 1)
    goto out;
  }
  pos += sg->length;
 }
 if (pos == size) ret = 0;
out:
 sg_free_table(&sgt);
 return ret;
}
/* Called under registry_lock. A failed probe triggers bus DMA cleanup.
 * On unproved quiescence retain the binding as well as the module and RAM.
 * Bound does NOT mean tested: selftest_result remains a negative errno.
 */
static int finish_failed_probe(struct platform_device *pdev,
                               struct canoe_engine *e, int error)
{
 int i;
 WRITE_ONCE(selftest_result, error);
 e->online = false;
 if (e->quarantine) {
  platform_set_drvdata(pdev, e);
  engine = e;
  dev_err(e->dev, "selftest failed %d; binding retained for DMA quarantine; reboot required\n", error);
  return 0;
 }
 for (i = 0; i < e->irq_count; i++)
  devm_free_irq(e->dev, e->irqs[i], e);
 kref_put(&e->refs, release_engine);
 return error;
}
/* registry_lock held; called only after the per-round CPU work completes. */
static void finish_selftest(struct canoe_engine *e, int result)
{
 unsigned long flags;
 int stop_ret, i;
 raw_spin_lock_irqsave(&e->lock, flags);
 e->online = false;
 raw_spin_unlock_irqrestore(&e->lock, flags);
 pause_irqs(e);
 stop_ret = stop_engine(e);
 if (!result && stop_ret) result = stop_ret;
 if (!result && READ_ONCE(e->broken)) result = -EIO;
 if (!result && e->test_round != 600) result = -ECANCELED;
 e->test_pending = false;
 e->tested = !result;
 WRITE_ONCE(selftest_result, result);
 if (e->qos_added && !e->quarantine) {
  dev_pm_qos_remove_request(&e->qos);
  e->qos_added = false;
 }
 for (i = 0; i < 6; i++)
  dev_info(e->dev, "selftest pattern=%d roundtrips=%u incompressible=%u\n",
           i, e->passed[i], e->incompressible[i]);
 dev_info(e->dev, "selftest stopped: rounds=%u/600 result=%d quarantine=%d\n",
          e->test_round, result, e->quarantine);
}
static void canoe_selftest_worker(struct work_struct *work)
{
 struct canoe_engine *e = container_of(to_delayed_work(work), struct canoe_engine, selftest_work);
 unsigned long flags;
 int ret = 0;
 mutex_lock(&registry_lock);
 if (e->removing || e->suspended || READ_ONCE(e->test_cancel)) goto unlock;
 if (!e->test_round) {
  ret = dev_pm_qos_add_request(get_cpu_device(0), &e->qos, DEV_PM_QOS_RESUME_LATENCY, 300);
  if (ret < 0) goto finish;
  e->qos_added = true;
  ret = start_engine(e);
  if (ret) goto finish;
  resume_irqs(e);
  raw_spin_lock_irqsave(&e->lock, flags);
  e->online = true;
  raw_spin_unlock_irqrestore(&e->lock, flags);
 }
 mutex_unlock(&registry_lock);
 /* Protect only six bounded requests, not the 60-second pacing interval. */
 cpus_read_lock();
 ret = cpu_online(0) ? work_on_cpu(0, memory_selftest_round, e) : -ENODEV;
 cpus_read_unlock();
 mutex_lock(&registry_lock);
 if (ret || READ_ONCE(e->broken)) { if (!ret) ret = -EIO; goto finish; }
 e->test_round++;
 WRITE_ONCE(selftest_completed_rounds, e->test_round);
 if (e->test_round == 600) goto finish;
 /* cancel_delayed_work_sync races safely with this self-requeue. */
 if (!READ_ONCE(e->test_cancel))
  queue_delayed_work(selftest_wq, &e->selftest_work, msecs_to_jiffies(100));
 goto unlock;
finish:
 finish_selftest(e, ret);
unlock:
 mutex_unlock(&registry_lock);
}
static void queue_selftest(struct canoe_engine *e)
{
 e->test_pending = true;
 e->test_round = 0;
 memset(e->passed, 0, sizeof(e->passed));
 memset(e->incompressible, 0, sizeof(e->incompressible));
 WRITE_ONCE(selftest_completed_rounds, 0);
 WRITE_ONCE(selftest_result, -EINPROGRESS);
 WRITE_ONCE(e->test_cancel, false);
 queue_delayed_work(selftest_wq, &e->selftest_work, 0);
}
static int canoe_probe_impl(struct platform_device *pdev)
{
 struct device *dev = &pdev->dev;
 struct iommu_domain *domain;
 struct canoe_engine *e;
 int ret, irq, i;
 const char * const irq_names[] = {"ring-and-bus-err", "urgent"};
 if (!test_mode) return dev_err_probe(dev, -EOPNOTSUPP, "isolated test mode is off\n");
 if (PAGE_SIZE != 4096) return -EOPNOTSUPP;
 ret = validate_resources(pdev);
 if (ret) return ret;
 domain = iommu_get_domain_for_dev(dev);
 ret = validate_dma_mode(dev, domain);
 if (ret)
  return dev_err_probe(dev, ret, "Canoe translated DMA prerequisites missing\n");
 mutex_lock(&registry_lock);
 if (engine || hardware_quarantined) { ret = -EBUSY; goto unlock; }
 e = kzalloc(sizeof(*e), GFP_KERNEL);
 if (!e) { ret = -ENOMEM; goto unlock; }
 e->dev = get_device(dev);
 kref_init(&e->refs);
 raw_spin_lock_init(&e->lock);
 INIT_DELAYED_WORK(&e->selftest_work, canoe_selftest_worker);
 e->regs = devm_platform_ioremap_resource(pdev, 0);
 if (IS_ERR(e->regs)) { ret = PTR_ERR(e->regs); goto release; }
 e->icc = of_icc_get(dev, "qpace-mem");
 if (IS_ERR(e->icc)) { ret = PTR_ERR(e->icc); goto release; }
 icc_set_tag(e->icc, QCOM_ICC_TAG_ACTIVE_ONLY);
 ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));
 if (ret) goto release;
 e->in = dma_alloc_coherent(dev, 2 * PAGE_SIZE, &e->in_dma, GFP_KERNEL);
 e->out = dma_alloc_coherent(dev, 2 * PAGE_SIZE, &e->out_dma, GFP_KERNEL);
 if (!e->in || !e->out) { ret = -ENOMEM; goto release; }
 ret = validate_dma_buffer(dev, domain, e->in, e->in_dma, 2 * PAGE_SIZE);
 if (ret) goto release;
 ret = validate_dma_buffer(dev, domain, e->out, e->out_dma, 2 * PAGE_SIZE);
 if (ret) goto release;
 if (!get_cpu_device(0)) { ret = -ENODEV; goto release; }
 for (i = 0; i < ARRAY_SIZE(irq_names); i++) {
  irq = platform_get_irq_byname(pdev, irq_names[i]);
  if (irq < 0) { ret = irq; goto release; }
  ret = devm_request_irq(dev, irq, fault_irq, 0, irq_names[i], e);
  if (ret) goto release;
  e->irqs[e->irq_count++] = irq;
 }
 platform_set_drvdata(pdev, e);
 engine = e;
 if (force_mode) {
  ret = start_engine(e);
  if (ret) {
   stop_engine(e);
   engine = NULL;
   platform_set_drvdata(pdev, NULL);
   goto release;
  }
  e->online = true;
  WRITE_ONCE(hardware_ready, true);
  wake_up_all(&hardware_wait);
  dev_info(dev, "forced native QPaCE enabled; diagnostic batch skipped, not passed\n");
 } else {
  queue_selftest(e);
  dev_info(dev, "native codec selftest queued\n");
 }
 mutex_unlock(&registry_lock);
 return 0;
release:
 ret = finish_failed_probe(pdev, e, ret);
unlock:
 mutex_unlock(&registry_lock);
 return ret;
}
static int canoe_probe(struct platform_device *pdev)
{
 int ret = canoe_probe_impl(pdev);
 if (ret) WRITE_ONCE(selftest_result, ret);
 return ret;
}
static void canoe_remove(struct platform_device *pdev)
{
 struct canoe_engine *e = platform_get_drvdata(pdev);
 unsigned long flags;
 int irq;
 WRITE_ONCE(hardware_ready, false);
 WRITE_ONCE(e->test_cancel, true);
 cancel_delayed_work_sync(&e->selftest_work);
 mutex_lock(&registry_lock);
 e->removing = true;
 engine = NULL;
 raw_spin_lock_irqsave(&e->lock, flags);
 e->online = false;
 raw_spin_unlock_irqrestore(&e->lock, flags);
 irq = platform_get_irq_byname(pdev, "ring-and-bus-err");
 devm_free_irq(&pdev->dev, irq, e);
 irq = platform_get_irq_byname(pdev, "urgent");
 devm_free_irq(&pdev->dev, irq, e);
 stop_engine(e);
 kref_put(&e->refs, release_engine);
 mutex_unlock(&registry_lock);
}
/* Sleep callbacks run after tasks freeze; a stopped device rejects requests.
 * The crypto adapter accepts only byte-validated LZ4; stored pages decode
 * in software even while this transport is unavailable.
 */
static int canoe_suspend(struct device *dev)
{
 struct canoe_engine *e = dev_get_drvdata(dev);
 unsigned long flags;
 int ret = 0;
 if (!e) return 0;
 WRITE_ONCE(e->test_cancel, true);
 cancel_delayed_work_sync(&e->selftest_work);
 mutex_lock(&registry_lock);
 if (e->test_pending) WRITE_ONCE(selftest_result, -ECANCELED);
 raw_spin_lock_irqsave(&e->lock, flags);
 e->online = false;
 e->suspended = true;
 raw_spin_unlock_irqrestore(&e->lock, flags);
 pause_irqs(e);
 ret = stop_engine(e);
 if (e->qos_added && !e->quarantine) {
  dev_pm_qos_remove_request(&e->qos); e->qos_added = false;
 }
 if (ret) {
  WRITE_ONCE(e->broken, true);
  e->suspended = false;
 }
 mutex_unlock(&registry_lock);
 return ret;
}
static int canoe_resume(struct device *dev)
{
 struct canoe_engine *e = dev_get_drvdata(dev);
 unsigned long flags;
 int ret = 0;
 if (!e) return 0;
 mutex_lock(&registry_lock);
 if (!e->suspended) goto out;
 if (READ_ONCE(e->broken) || e->quarantine) { ret = -EIO; goto done; }
 if (e->clients) {
  ret = start_engine(e);
  if (ret) { WRITE_ONCE(e->broken, true); stop_engine(e); goto done; }
  resume_irqs(e);
  raw_spin_lock_irqsave(&e->lock, flags);
  if (!READ_ONCE(e->broken)) e->online = true;
  else ret = -EIO;
  raw_spin_unlock_irqrestore(&e->lock, flags);
 }
done:
 e->suspended = false;
 if (!ret && e->test_pending) queue_selftest(e);
out:
 mutex_unlock(&registry_lock);
 return ret;
}
static const struct dev_pm_ops canoe_pm_ops = {
 .suspend = canoe_suspend, .resume = canoe_resume,
 .freeze = canoe_suspend, .thaw = canoe_resume,
 .poweroff = canoe_suspend, .restore = canoe_resume,
};
static const struct of_device_id canoe_match[] = {
 { .compatible = "qcom,canoe-qpace-experimental" }, {}
};
MODULE_DEVICE_TABLE(of, canoe_match);
static struct platform_driver canoe_driver = {
 /* Probe returns before any selftest; delayed work is outside async init. */
 .probe = canoe_probe, .remove = canoe_remove,
 .driver = { .name = "canoe-qpace-experimental", .of_match_table = canoe_match,
             .probe_type = PROBE_PREFER_ASYNCHRONOUS, .pm = &canoe_pm_ops, .suppress_bind_attrs = true },
};
static bool create_test_device = IS_ENABLED(CONFIG_OS44_QPACE);
module_param(create_test_device, bool, 0400);
MODULE_PARM_DESC(create_test_device, "Explicitly instantiate the disabled early-FDT test node; requires test_mode=1");
static struct platform_device *test_device;
static struct device_node *test_node;

static int create_isolated_device(void)
{
 struct device_node *root, *np;
 struct platform_device *pdev, *parent, *existing;
 struct resource resource;
 const char *status;
 int ret;
 root = of_find_node_by_path("/");
 ret = root && of_device_is_compatible(root, "qcom,canoe");
 of_node_put(root);
 if (!ret) return -ENODEV;
 np = of_find_node_by_path("/soc/compression@31400000");
 if (!np) return -ENODEV;
 if (!of_device_is_compatible(np, "qcom,canoe-qpace-experimental") ||
     of_property_read_string(np, "status", &status) || strcmp(status, "disabled")) {
  ret = -EINVAL; goto put_node;
 }
 existing = of_find_device_by_node(np);
 if (existing) { put_device(&existing->dev); ret = -EBUSY; goto put_node; }
 if (of_node_test_and_set_flag(np, OF_POPULATED)) { ret = -EBUSY; goto put_node; }
 ret = of_address_to_resource(np, 0, &resource);
 if (ret) goto clear_flag;
 pdev = platform_device_alloc("canoe-qpace-explicit-test", PLATFORM_DEVID_NONE);
 if (!pdev) { ret = -ENOMEM; goto clear_flag; }
 device_set_node(&pdev->dev, of_fwnode_handle(of_node_get(np)));
 pdev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
 pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
 parent = of_find_device_by_node(np->parent);
 if (!parent) { ret = -ENODEV; goto put_pdev; }
 pdev->dev.parent = &parent->dev;
 ret = platform_device_add_resources(pdev, &resource, 1);
 if (!ret) ret = validate_resources(pdev);
 if (!ret) ret = platform_device_add(pdev);
 put_device(&parent->dev);
 if (ret) goto put_pdev;
 /* device_add success is not probe/selftest success. Read selftest_result. */
 test_device = pdev;
 test_node = np;
 return 0;
put_pdev:
 platform_device_put(pdev);
clear_flag:
 of_node_clear_flag(np, OF_POPULATED);
put_node:
 of_node_put(np);
 return ret;
}
static int __init canoe_init(void)
{
 int ret;
 if (create_test_device && !test_mode) return -EINVAL;
 selftest_wq = alloc_ordered_workqueue("os44-qpace-selftest", WQ_MEM_RECLAIM);
 if (!selftest_wq) return -ENOMEM;
 ret = platform_driver_register(&canoe_driver);
 if (ret) { destroy_workqueue(selftest_wq); return ret; }
 if (create_test_device) {
  ret = create_isolated_device();
  if (ret) { platform_driver_unregister(&canoe_driver); destroy_workqueue(selftest_wq); return ret; }
 }
 return 0;
}
static void __exit canoe_exit(void)
{
 /* Quarantine holds THIS_MODULE: normal module removal cannot reach here. */
 if (test_device) {
  platform_device_unregister(test_device);
  of_node_clear_flag(test_node, OF_POPULATED);
  of_node_put(test_node);
 }
 platform_driver_unregister(&canoe_driver);
 destroy_workqueue(selftest_wq);
}
module_init(canoe_init);
module_exit(canoe_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OS44 forced native QPaCE COMP/DECOMP; hardware trial9838");
