# Z03Ultra

Z03Ultra 是基于 Z03Plus 扩展项目发展而来的分支，保留其 overlay 注入与 QRC 覆盖方案。

紫光展锐 Linux/Qt 词典笔 Z03（P010/P011 兼容环境）的扩展功能原型。

本项目通过外部 Qt RCC 资源覆盖，在原厂“设置”页面加入：

```text
设置 -> Z03Ultra 扩展
```

## 一键安装 / 更新

设备联网并具备 root 权限时，在设备终端执行：

```sh
curl -fsSL https://github.com/OpenZybTeam/zyb_P010-P011_PenUltra/releases/latest/download/install.sh | sh
```

脚本会读取 GitHub 最新 Release，校验 `SHA256SUMS`，把旧文件备份到 `/home/user/z03plus/backup/`，安装 `libZ03Plus.so`、`z03plus.rcc`、`Z03Plus`，把一行 bind mount 钩子写入 `/etc/init.d/zybz03qt-init`，然后重启主界面并检查注入结果。安装脚本通过管道执行，不会在设备上留下 `.sh` 文件。

- 无法访问 GitHub 时，可指定下载目录：`curl -fsSL <install.sh 地址> | Z03ULTRA_BASE=<release 下载目录> sh`
- 指定其他仓库：`curl -fsSL <install.sh 地址> | Z03ULTRA_REPO=<owner/repo> sh`
- 回退见下文“回退”一节。


## 重要提示

- 本项目仅面向具备相同软件环境的 Z03 / P010 / P011 设备，其他型号未经测试。
- 操作前请完整备份设备数据和启动配置。使用者须自行承担刷写、部署及启动失败的风险。
- 不要直接修改或覆盖原厂 `/usr/bin/Z03`；始终使用 `/home/user/z03plus/Z03Plus` 中的副本。
- 源码仓库不包含原厂二进制、Qt 库、工具链与设备导出资源；可直接安装的产物通过 GitHub Release 发布。
- 一键安装会临时把 rootfs 改为可写，以便写入 `/etc/init.d/zybz03qt-init` 启动钩子；完成后恢复只读。
- 为兼容既有设备安装，动态库、启动器与部署目录沿用 `libZ03Plus.so`、`Z03Plus`、`/home/user/z03plus/` 命名；源码项目与界面统一使用 Z03Ultra 名称。

## 已实现

1. ARMv7 动态库注入：将 `DT_NEEDED` 添加到 Z03 的副本，不覆盖原始 `/usr/bin/Z03`；开机由 `zybz03qt-init` 在启动守护脚本前把副本 bind mount 到 `/usr/bin/Z03`，自动加载 `libZ03Plus.so`。
2. Hook `qRegisterResourceData()`，在原厂 QRC 注册前加载 Z03Ultra overlay。
3. 从运行中的 Z03 导出完整 Qt 资源，设备端导出目录为 `/home/user/z03plus/qrc/`。
4. 使用兼容的外部 QRC 覆盖：`qrc:/ZybQmlFiles/ZybSet/SetMainPage.qml`。
5. 设置页新增“Z03Ultra 扩展”入口。
6. Z03Ultra 首页采用 Material 风格卡片（含点击反馈与彩色强调条），提供文件管理、文本阅读、存储空间、音乐播放、录音、终端 Shell、应用商店（Nix）、网页浏览、关于设备、关于 Z03Ultra、显示与灯光、ADB/SSH、AI 对话入口；右上角可切换横屏/竖屏（仅影响 Ultra 页面及其子页面）。
   - 文件管理：从 `/` 浏览整个文件系统，支持新建目录、复制、剪切/移动、重命名、删除、压缩（tar、tar.gz、tar.xz、zip）、解压与属性查看。
   - 文本阅读/编辑：支持任意文本文件预览与编辑，保存使用 `QSaveFile` 原子写入，并自动识别 UTF-8 与 GB18030。
   - 存储空间：读取各挂载分区用量，显示文件系统与只读状态，并提供缓存清理。
   - 音乐播放：递归扫描 `/mnt/data` 与 `/home/user`，通过 `MediaPlayer` 播放并支持进度拖动。
   - 录音：按 `arecord` / `tinycap` / `gst-launch-1.0` 顺序选择后端，保存 16 kHz 单声道 WAV 到 `/home/user/z03plus/recordings`。
   - 终端 Shell：`/bin/sh -c` 执行命令，输出实时回显，支持停止与 60 秒超时。
   - 应用商店：使用 Nix（安装目录 `/home/nix`，启动时自动 bind mount 到 `/nix`；通过清华 TUNA/NJU 镜像下载 Nix 2.9.2 armv7l，复制 89 MB store 后手动执行 `nix-store --init/--load-db` 与 `nix-env -i`，并添加 nixos-22.05 频道；支持 HTTP 与跳过证书校验回退），支持搜索、按属性安装、查看已安装与更新频道。
   - 网页浏览：基于系统自带 QtWebEngine 5.10（Chromium 61）的完整网页渲染，支持 JavaScript、CSS、图片与视频、点按链接、触摸滚动、前进后退、地址栏与搜索；使用移动版 User-Agent 适配窄屏，显示加载进度并在失败时提供重试。
   - 关于设备：展示设备型号、发行版、内核、ARM 架构、CPU 型号与特性、内存、运行时间、根分区与 Qt 版本。
   - 关于 Z03Ultra：展示 Powered By OpenZybTeam 与 Based On PenPlus 项目地址，并可在内置浏览器打开。
   - “显示与灯光”会打开原厂 `SetLightPage.qml`。

## 设备环境

- 主程序：`/usr/bin/Z03`
- CPU：ARMv7 / 32-bit ARM EABI
- 系统：Linux 4.14，glibc 2.27
- Qt：Qt 5.10.1 / Qt Quick / Wayland
- 外部可写目录：`/home/user/z03plus/`

## 目录说明

- `z03plus_overlay/`：实际生效的 overlay。
  - `SetMainPage.qml`：原厂设置主页的覆盖版，添加入口。
  - `Z03UltraPage.qml`：扩展主页。
  - `z03plus.qrc`：QML 的外部资源清单。
  - `z03plus_overlay.cpp`：注入库，抢先注册外部 QRC。
- `z03plus_exporter/`：资源导出器源码。在 Z03 注册原始 QRC 后遍历 `:/` 并导出运行时资源。
- `z03plus_tools/`：离线提取 Z03 嵌入 QML 文本块的 Python 工具及设备侧辅助脚本。
- `z03mods_poc/`：初期 ARMv7 注入与 QRC Hook 的验证源码，仅供参考。

## 构建与部署

需要 ARMv7 Linux 交叉编译器、QtCore 开发头文件，以及与设备兼容的 `libstdc++` ABI。

推荐直接运行构建脚本；手工步骤列在下方作为参考。

```sh
./build_armhf.sh /path/to/original/Z03
```

脚本会编译 `libZ03Plus.so`、`libZ03PlusExporter.so`，生成 RCC v2，并检查 GLIBC 与架构。

1. 编译 `libZ03Plus.so`（ARMv7 shared object）。
2. 生成 RCC v2：

   ```sh
   rcc --binary --format-version 2 --no-compress z03plus.qrc -o z03plus.rcc
   ```

3. 从原始 Z03 创建副本并加入依赖：

   ```sh
   patchelf --add-needed /home/user/z03plus/libZ03Plus.so Z03Plus
   ```

4. 部署到设备：

   ```text
   /home/user/z03plus/Z03Plus
   /home/user/z03plus/libZ03Plus.so
   /home/user/z03plus/z03plus.rcc
   ```

5. 确认开机钩子把 `/home/user/z03plus/Z03Plus` bind mount 到 `/usr/bin/Z03`，守护脚本启动的即是带注入的副本。

完整的依赖安装、moc 版本、部署、验证与回退步骤见 [docs/BUILD_DEPLOY_zh.md](docs/BUILD_DEPLOY_zh.md)。

### RCC 格式

设备上的 Qt 5.10.1 无法加载 Qt 5.15 默认生成的 RCC v3，必须生成 RCC v2；否则 `QResource::registerResource()` 会返回 `false`，设置页不会覆盖。

## 回退

恢复原厂启动钩子并重启：

```sh
mount -o remount,rw /
if [ -f /home/user/z03plus/zybz03qt-init.orig ]; then
  cp -a /home/user/z03plus/zybz03qt-init.orig /etc/init.d/zybz03qt-init
else
  sed -i '/mount --bind.*Z03Plus/d' /etc/init.d/zybz03qt-init
fi
sync
mount -o remount,ro /
kill $(pidof Z03) 2>/dev/null
```

重启后守护脚本恢复启动原厂 `/usr/bin/Z03`，bind mount 随重启自动消失。旧版本文件位于 `/home/user/z03plus/backup/`。

## 卸载

完全移除 Ultra（恢复原厂启动并删除 `/home/user/z03plus/`）：

```sh
G=$(ps w | grep '[z]yb_z03_launch.sh' | awk '{print $1}' | head -n 1)
[ -n "$G" ] && kill -STOP $G
kill $(pidof Z03) 2>/dev/null
sleep 1
umount /usr/bin/Z03 2>/dev/null || umount -l /usr/bin/Z03 2>/dev/null
mount -o remount,rw /
if [ -f /home/user/z03plus/zybz03qt-init.orig ]; then
  cp -a /home/user/z03plus/zybz03qt-init.orig /etc/init.d/zybz03qt-init
else
  awk '!/mount --bind.*Z03Plus/ && !/z03ultra-boot\.sh/' /etc/init.d/zybz03qt-init > /etc/init.d/zybz03qt-init.new
  chmod 755 /etc/init.d/zybz03qt-init.new
  mv -f /etc/init.d/zybz03qt-init.new /etc/init.d/zybz03qt-init
fi
sync
mount -o remount,ro /
rm -rf /home/user/z03plus
[ -n "$G" ] && kill -CONT $G
```

先 `umount` 再删目录：bind mount 仍挂载时删除源文件会报 `Device or resource busy`。只想关闭注入、保留 AI 配置与录音时，跳过 `rm -rf`，只删除 `Z03Plus`、`libZ03Plus.so`、`z03plus.rcc`。


## 注意事项

- 修改 QML 或 RCC 后重启主界面即可。库在启动时设置 `QML_DISABLE_DISK_CACHE=1`，不再依赖手工清理 `/home/.cache/Z03Plus/`。
- 原厂 `BasicButton` 默认 `checkable: true`。Z03Plus 功能入口必须显式设为 `checkable: false`，否则点击后会保持绿色选中状态。
## 真实设备适配记录与已踩坑

本节记录 Z03Plus 实机适配过程中已经验证过的路径、失败原因与当前安全边界，避免后续重复走弯路。下列内容针对当前设备镜像；不同固件、不同二进制哈希或不同 Qt/GLIBC 版本均必须重新验证。

### 1. 启动入口是 `/usr/bin/Z03`，副本通过 bind mount 接管

守护脚本 `/usr/bin/zyb_z03_launch.sh` 固定循环启动 `/usr/bin/Z03`。持久化注入不覆盖原文件，而是在 `/etc/init.d/zybz03qt-init` 拉起守护脚本之前执行：

```sh
mount --bind /home/user/z03plus/Z03Plus /usr/bin/Z03
```

之后守护脚本每次重启，`/usr/bin/Z03` 实际解析到带 `DT_NEEDED` 的副本，扫描、OCR、翻译/搜题跳转逻辑也随副本一起加载。检查方法：

```sh
pidof Z03
grep -c '/home/user/z03plus/libZ03Plus.so' /proc/$(pidof Z03)/maps
cat /home/user/z03plus/boot-inject.log
```

### 2. QML 修改必须重新生成并部署 RCC

编辑 `z03plus_overlay/*.qml` 不会直接作用于设备。正确流程是：

```sh
cd z03plus_overlay
rcc --binary --format-version 2 --no-compress z03plus.qrc -o z03plus.rcc
```

然后将新资源包覆盖到设备：

```sh
# 设备端目标路径
/home/user/z03plus/z03plus.rcc
```

注意事项：

- 必须使用 **RCC format version 2**。设备 Qt 5.10.1 无法可靠读取新版本 `rcc` 默认生成的 v3 资源包。
- 建议在部署前检查 QML 括号匹配、资源清单和生成文件大小；部署后核对 MD5，并在 `/proc/<Z03Plus-pid>/maps` 中确认进程确实映射了新的 `z03plus.rcc`。
- QML 有缓存。替换 RCC 后须清理 `/home/.cache/Z03Plus/`（如该固件实际使用此缓存目录）并重启主 UI，否则可能仍显示旧界面。
- 当前已实现 AI 对话页的 NDJSON 真流式显示；桥接端接口为 `POST /api/chat/stream`，不要误改回仅等待最终结果的 `/api/chat` 调用。

### 3. 不要替换原厂 `.so` 或直接修改系统程序

设备的 `/usr/bin` 位于只读 rootfs。不改写 `/usr/bin/Z03` 与 `/usr/bin/zyb_z03_launch.sh`，只通过 bind mount 改变 `/usr/bin/Z03` 的挂载视图。以下做法仍然不可取：

- 直接覆盖 `/usr/bin/Z03`、系统 Qt 库或 OCR `.so`；
- 修改 `/usr/bin/zyb_z03_launch.sh` 或长期依赖 `LD_PRELOAD`；
- 将其他设备的 PenMods 注入库直接放到本机运行。

当前可写、可回退的部署位置应保持在：

```text
/home/user/z03plus/
```

主 UI 副本、外部 RCC、桥接服务以及临时测试库都应从该目录加载。替换 `libZ03Plus.so` 或 `z03plus.rcc` 前应保留已验证版本，并用哈希核对上传结果。

### 4. 原进程 Hook 仅能临时测试，必须先暂停守护循环

扫描链路位于主 UI 原进程，不能通过普通 QML 直接拦截。测试 native hook 时不能修改只读启动脚本，只能进行一次性临时启动：

1. `kill -STOP <守护脚本 PID>`，防止守护循环立即拉起普通实例；
2. 结束当前 `Z03Plus`；
3. 手动以 `LD_PRELOAD=/home/user/z03plus/<probe>.so` 启动 `Z03Plus -platform wayland`；
4. 通过 `/proc/<pid>/maps` 确认探针库已加载；
5. 测试结束后杀掉临时实例，执行 `kill -CONT <守护脚本 PID>`，由系统恢复普通实例。

这类测试必须保持短时、可逆。启动失败或临时实例崩溃时，恢复守护循环即可回到未注入的正常 UI；不得把探针写入系统分区或持久化到启动脚本。

### 5. 探针 ABI/GLIBC 兼容性是硬约束

设备为 ARM32 hard-float，系统 glibc 为 2.27。第一版观测探针曾误链接 Ubuntu glibc，导致设备报：

```text
GLIBC_2.34 not found
```

另一次探针使用 `time()`，交叉工具链引入设备不存在的 `__time64`，扫描触发时临时 UI 崩溃并回到 Logo。

探针必须：

- 使用 `arm-linux-gnueabihf-gcc/g++`；
- 链接从设备提取的 sysroot，并使用 `-nostdlib` 控制依赖；
- 在部署前用 `readelf --version-info` 检查，当前观测库只允许依赖至 `GLIBC_2.4`；
- 避免无意引入 `time()`、`snprintf()` 或宿主机新 glibc 符号；
- 先做“只记录、立即转发”的观测，不可直接上拦截逻辑。

### 6. 扫描 Hook 的当前分析结论与边界

已完成的纯观测结果：

- hook `zuoyebang::QtSetOcrScanMode(OcrInstruct&)` 可观测扫描模式切换，扫描一次记录到 4 次调用；
- OCR 最终文本通过 Qt 槽进入：`ScanManager::slot_ocrGetText(bool, QString)`；
- 实测日志示例：`slot_ocrGetText: "班主任签名" bFinished: true`；
- 该槽之后才发生翻译接口请求和页面跳转。

离线分析还确认了当前二进制中该 Qt 槽的分发/实现关系（仅适用于已分析的二进制哈希）：

```text
Qt 元对象分发：0x1964a4
slot_ocrGetText 方法索引：9
内部实现入口：0x133f1c
ABI：r0=this，r1=bool finished，r2=const QString &text
```

**不要直接在 `slot_ocrGetText` 实现入口“吞掉返回”。**该函数同时负责 OCR 分段累积、完成状态和 Qt 信号转发；粗暴阻断可能破坏 OCR 状态机，影响后续正常扫描。

安全目标仍是“一次性 AI 扫描令牌”：只有用户在 AI 输入框主动点“扫”后、且令牌未超时时，才处理下一次完成结果；所有未授权扫描必须完整走原厂搜题/翻译流程。后续应优先在 OCR 完成后的翻译/搜题**路由调用点**实现条件分流，而不是替换 OCR 槽本体。

### 7. 当前持久化状态

持久化注入已落地：`/etc/init.d/zybz03qt-init` 在每次开机拉起守护脚本前执行一行 bind mount，把副本挂到 `/usr/bin/Z03`。升级固件会覆盖该 init 脚本，OTA 后重新执行 `install.sh` 即可恢复。扫描输入的“令牌拦截 + 回填 AI 输入框”尚未部署，不应把它描述为已完成。



本项目的词典笔扩展思路及部分早期实现，参考并基于 [PenUniverse/PenMods](https://github.com/PenUniverse/PenMods) 演进而来。感谢 PenMods 项目及其贡献者。

本项目依照 GNU General Public License v3.0 或更高版本（GPL-3.0-or-later）发布。完整许可证见 [LICENSE](LICENSE)，上游来源及适配说明见 [NOTICE](NOTICE)。