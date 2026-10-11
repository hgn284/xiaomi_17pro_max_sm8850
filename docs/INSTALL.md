# 9838 安装说明

适用：小米17 Pro Max / popsicle / OS4.0.0.44.XPBCNXM。其他机型和系统不在此版本的支持声明中。

1. 从Release下载 `OS44_QPACE_FORCE_9838.zip`。
2. 用此前已成功的Root内核刷写工具安装。
3. 出现 `BOOT_ONLY_9838_INSTALLED_VERIFIED` 后手动重启，不运行旧启用脚本。

安装器只写当前槽boot；自动备份在 `/data/adb/os44-boot9838-backups/BOOT-9838-*/boot-before.img`。不自动重启、不切槽、不修改vendor_boot/vbmeta/init_boot/dtbo或模块分区。若报告 `RECOVERY_REQUIRED`，保留现场并先恢复boot。

`OS44_INSTALL_FORCE_9838.txt` 是可选直接安装入口，只用于Root刷写工具私有缓存权限问题。ZIP和TXT按原名放在Download，进入Root终端后运行：

```sh
su
sh /sdcard/Download/OS44_INSTALL_FORCE_9838.txt
```

回退时优先使用安装前保存的boot，或自己已确认能启动的前一版本。不要把其他用户的分区备份当作本机备份。

当前9838不包含下一版的扩展容量映射。它仍由原厂初始化路径决定容量，内核只强制选择QPaCE算法。不可压缩页面按zRAM原有原页处理，不冒充压缩成功。
