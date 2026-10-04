# Z03Plus

紫光展锐 Linux/Qt 词典笔 Z03（P010/P011 兼容环境）的扩展功能原型。

本项目通过外部 Qt RCC 资源覆盖，在原厂“设置”页面加入：

```text
设置 -> Z03Plus 扩展
```

## 重要提示

- 本项目仅面向具备相同软件环境的 Z03 / P010 / P011 设备，其他型号未经测试。
- 操作前请完整备份设备数据和启动配置。使用者须自行承担刷写、部署及启动失败的风险。
- 不要直接修改或覆盖原厂 `/usr/bin/Z03`；始终使用 `/home/user/z03plus/Z03Plus` 中的副本。
- 本仓库只提供源码，不包含原厂二进制、Qt 库、工具链、设备导出资源或编译产物。

## 已实现

1. ARMv7 动态库注入：将 `DT_NEEDED` 添加到 Z03 的副本，不覆盖原始 `/usr/bin/Z03`。
2. Hook `qRegisterResourceData()`，在原厂 QRC 注册前加载 Z03Plus overlay。
3. 从运行中的 Z03 导出完整 Qt 资源，设备端导出目录为 `/home/user/z03plus/qrc/`。
4. 使用兼容的外部 QRC 覆盖：`qrc:/ZybQmlFiles/ZybSet/SetMainPage.qml`。
5. 设置页新增“Z03Plus 扩展”入口。
6. Z03Plus 首页包含文件管理、文本阅读、存储空间、显示与灯光、ADB/SSH、播放器、录音/息屏等入口。“显示与灯光”会打开原厂 `SetLightPage.qml`；其余项目目前为稳定的功能占位入口。

## 设备环境

- 主程序：`/usr/bin/Z03`
- CPU：ARMv7 / 32-bit ARM EABI
- 系统：Linux 4.14，glibc 2.27
- Qt：Qt 5.10.1 / Qt Quick / Wayland
- 外部可写目录：`/home/user/z03plus/`

## 目录说明

- `z03plus_overlay/`：实际生效的 overlay。
  - `SetMainPage.qml`：原厂设置主页的覆盖版，添加入口。
  - `Z03PlusPage.qml`：扩展主页。
  - `z03plus.qrc`：QML 的外部资源清单。
  - `z03plus_overlay.cpp`：注入库，抢先注册外部 QRC。
- `z03plus_exporter/`：资源导出器源码。在 Z03 注册原始 QRC 后遍历 `:/` 并导出运行时资源。
- `z03plus_tools/`：离线提取 Z03 嵌入 QML 文本块的 Python 工具及设备侧辅助脚本。
- `z03mods_poc/`：初期 ARMv7 注入与 QRC Hook 的验证源码，仅供参考。

## 构建与部署

需要 ARMv7 Linux 交叉编译器、QtCore 开发头文件，以及与设备兼容的 `libstdc++` ABI。

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

5. 配置启动脚本，使其启动 `Z03Plus` 副本。

### RCC 格式

设备上的 Qt 5.10.1 无法加载 Qt 5.15 默认生成的 RCC v3，必须生成 RCC v2；否则 `QResource::registerResource()` 会返回 `false`，设置页不会覆盖。

## 回退

将启动脚本恢复为：

```sh
/usr/bin/Z03 -platform wayland > /dev/null &
```

然后重启 Z03 服务。

## 注意事项

- 修改 QML 或 RCC 后，删除 `/home/.cache/Z03Plus/` 并重启 Z03Plus，QML 改动才会重新加载。
- 原厂 `BasicButton` 默认 `checkable: true`。Z03Plus 功能入口必须显式设为 `checkable: false`，否则点击后会保持绿色选中状态。
## 真实设备适配记录与已踩坑

本节记录 Z03Plus 实机适配过程中已经验证过的路径、失败原因与当前安全边界，避免后续重复走弯路。下列内容针对当前设备镜像；不同固件、不同二进制哈希或不同 Qt/GLIBC 版本均必须重新验证。

### 1. 实际运行进程不是 `/usr/bin/Z03`

早期分析曾把扫描逻辑归因到 `/usr/bin/Z03`。实机确认后，系统守护脚本实际循环启动的是：

```sh
/home/user/z03plus/Z03Plus -platform wayland
```

扫描、OCR、翻译/搜题跳转等逻辑也在这个主 UI 副本内。不要仅根据旧项目路径或其他设备的 PenMods 代码判断目标进程；应先通过 `ps`、`/proc/<pid>/maps` 和启动脚本确认运行中的二进制。

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

设备的 `/usr/bin` 位于只读分区。实测无法修改、备份或覆盖：

```text
/usr/bin/zyb_z03_launch.sh
```

因此以下做法不可取或风险很高：

- 直接覆盖 `/usr/bin/Z03`、系统 Qt 库或 OCR `.so`；
- 试图长期修改只读启动脚本加入 `LD_PRELOAD`；
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

### 7. 当前恢复状态

最近一次观测测试已经完成并恢复：系统守护循环重新拉起普通 `Z03Plus` 实例，未加载探针；系统启动脚本没有修改。扫描输入的“令牌拦截 + 回填 AI 输入框”尚未部署，不应把它描述为已完成。



本项目的词典笔扩展思路及部分早期实现，参考并基于 [PenUniverse/PenMods](https://github.com/PenUniverse/PenMods) 演进而来。感谢 PenMods 项目及其贡献者。

本项目依照 GNU General Public License v3.0 或更高版本（GPL-3.0-or-later）发布。完整许可证见 [LICENSE](LICENSE)，上游来源及适配说明见 [NOTICE](NOTICE)。