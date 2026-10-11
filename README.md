# Xiaomi 17 Pro Max SM8850 专属内核

[![Stars](https://img.shields.io/github/stars/hgn284/xiaomi_17pro_max_sm8850?style=flat)](https://github.com/hgn284/xiaomi_17pro_max_sm8850/stargazers)
[![Release](https://img.shields.io/github/v/release/hgn284/xiaomi_17pro_max_sm8850?include_prereleases)](https://github.com/hgn284/xiaomi_17pro_max_sm8850/releases)
[![Build](https://img.shields.io/badge/build-manual_Actions-blue)](https://github.com/hgn284/xiaomi_17pro_max_sm8850/actions/workflows/build.yml)

面向 **小米 17 Pro Max / popsicle / SM8850（Canoe）** 的 Android 16、Linux 6.12.69 内核适配与源码重建项目。

当前以 **小米 OS4.0.0.44.XPBCNXM** 为适配基线，保留原厂小米／高通模块，集成 **BakaSU 35222、SUSFS 2.3.0、QPaCE 原生硬件压缩与解压**。项目组织形式参考 [cctv18/oppo_oplus_realme_sm8850](https://github.com/cctv18/oppo_oplus_realme_sm8850)，不是欧加真内核移植包，也不宣称适用于所有 SM8850 手机。

## 当前版本

**[下载 OS44 QPaCE FORCE 9838](https://github.com/hgn284/xiaomi_17pro_max_sm8850/releases/tag/os44-qpace-force-9838)**

| 项目 | 状态 |
| --- | --- |
| 目标设备／系统 | 小米 17 Pro Max，OS4.0.0.44.XPBCNXM |
| Root | BakaSU 35222，冻结提交 `5b76b884c75f729a220bb317aa4a4fc78f0e0e9c` |
| SUSFS | 2.3.0 |
| 压缩路径 | `qpace-canoe`：硬件 COMP＋硬件 DECOMP，无软件压缩回退 |
| 启动反馈 | 使用者报告正常启动、当前运行正常 |
| zRAM 反馈 | 系统监控显示 `qpace-canoe`，16384 MB，约 3516 MB 已用 |
| 刷写范围 | 仅当前槽 boot；不写 vendor_boot、vbmeta、init_boot、dtbo 或原厂模块分区 |
| 全厂商源码 | 重建进行中，尚不是完整原厂驱动替代 |

用户反馈与监控显示不等于全面的硬件计数、数据完整性、性能或长期稳定性验收。当前版本保留真实硬件错误返回；强制模式跳过诊断批次，**不会把未执行的自检记为通过**。

## 本项目的主要内容

- 提供当前已交付内核的完整源码快照及构建配置，不只提供刷机包。
- 保持原厂模块加载边界，推进 ABI／BTF 兼容，不要求替换 vendor_boot 中的 IOMMU 依赖。
- 提供本地编译入口和手动 GitHub Actions；使用冻结的 AOSP Clang 19 与 Rust 1.82 工具链。
- 分开维护已经交付的内核、尚未接入的系统配置，以及实验性厂商驱动重建源码。

## 已实现

- [x] 针对 popsicle OS44 基线的可启动内核交付。
- [x] BakaSU 35222 与 SUSFS 2.3.0 集成。
- [x] QPaCE 驱动和 Crypto API 适配器实际内建。
- [x] QPaCE 原生硬件编解码配对，保留原厂 IOMMU 提供者。
- [x] 初始化 zRAM 时强制选择 `qpace-canoe`，不回退软件压缩。
- [x] 当前槽 boot 安装、安装前备份，不自动重启。
- [x] 使用者报告正常运行，监控显示 QPaCE zRAM 已作为 swap 使用。
- [x] 公开完整交付源码快照、局部研究源码和版本说明。

## 下一步

- [ ] **下一版：扩展选 6／8／12／16 GB，zRAM 对应 6／8／12／16 GB。** 配置数值已准备，早期无损覆盖尚待接入；**9838 不含此改动**。
- [ ] 完善 QPaCE 的长期运行、负载和电源管理反馈。
- [ ] 完成 UFS 平台 probe、注册／移除与私有类型兼容。
- [ ] 继续补齐完整小米厂商源码，减少原厂二进制依赖。

不采用“完成百分比”推断剩余工作量。可启动／可用交付与全厂商源码完成是两条独立进度线。

## 下载与使用

在 [Releases](https://github.com/hgn284/xiaomi_17pro_max_sm8850/releases) 下载 `OS44_QPACE_FORCE_9838.zip`，沿用支持本项目 boot 安装器的 Root 刷写工具，成功后手动重启。**不再执行旧 QPaCE 启用脚本。**

安装器保留原有的槽位识别、boot 备份及写入事务处理，不附加诊断流程。详情见 [安装说明](docs/INSTALL.md)。其他系统版本与机型未在本项目声明支持。

## 本地编译 / GitHub Actions

```bash
git clone https://github.com/hgn284/xiaomi_17pro_max_sm8850.git
cd xiaomi_17pro_max_sm8850
bash local/builder_6.12.69.sh
```

也可在 **Actions → Build Xiaomi 17 Pro Max → Run workflow** 手动触发。仅编译并上传构建产物，不自动发布、不刷手机。详见 [构建说明](docs/BUILD.md)。

本仓库发布时**没有重新编译或运行新测试**。9838 来自既有成功构建；移植到 GitHub runner 的入口尚未执行，不能把“有工作流”写成“云端构建已通过”。

## 目录

```text
.github/workflows/   手动构建入口
local/              本地准备／编译脚本
lib/                BTF 元数据构建工具
config/             冻结构建配置与公开证书／符号清单
qpace_patch/        当前 QPaCE 相关源码与历史改动说明
zram_patch/         下一版容量映射（未部署）
research/ufs/       实验性 UFS 源码集群（不进入当前刷机包）
manifest/          源码／工具链／发布版本锁定清单
docs/              安装、构建、状态及来源说明
```

完整 `kernel/` 与 `root_git/` 源码由 Release 的 `OS44_FORCE_9838_source.tar.gz` 提供，本地脚本按清单下载到 `work/`。未上传整个工作区、私有日志、手机截图或厂商固件镜像。

## 鸣谢与来源

- 项目格式参考：[cctv18/oppo_oplus_realme_sm8850](https://github.com/cctv18/oppo_oplus_realme_sm8850)
- Android Common Kernel：[android.googlesource.com/kernel/common](https://android.googlesource.com/kernel/common/)
- 小米开源资料：[MiCode/Xiaomi_Kernel_OpenSource](https://github.com/MiCode/Xiaomi_Kernel_OpenSource)
- BakaSU：[Baka-SU/BakaSU](https://github.com/Baka-SU/BakaSU)
- SUSFS：[simonpunk/susfs4ksu](https://gitlab.com/simonpunk/susfs4ksu)

保留各组件原有版权、SPDX 与许可证。参见 [源码与许可边界](docs/SOURCES.md)。
