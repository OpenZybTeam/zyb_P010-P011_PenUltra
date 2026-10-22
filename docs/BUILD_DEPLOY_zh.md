# Z03Ultra ARMv7 构建与部署

Z03Ultra 基于 Z03Plus 扩展项目，本文对应 Z03 / P010 / P011 词典笔，目标环境为 ARMv7 hard-float、glibc 2.27、Qt 5.10.1。固件版本或主程序哈希变化后，文中的地址、符号和兼容性结论都需要重新验证。

## 1. 安全边界

- 原厂 `/usr/bin/Z03` 保持不动，所有扩展文件放在 `/home/user/z03plus/`。
- 部署前备份现有 `/home/user/z03plus/`，并记录当前启动入口。
- 主程序副本使用原厂 Z03 的字节级拷贝，只修改 `DT_NEEDED`。
- 每次替换 `.so` 或 `.rcc` 前保留上一版，出现黑屏或崩溃时可立即回退。

## 2. 产物

| 文件 | 用途 | 参考大小 |
| --- | --- | --- |
| `libZ03Plus.so` | 注入库，负责 QRC 覆盖与 QML 类型注册 | 570420 B |
| `libZ03PlusExporter.so` | 可选，导出运行时 QRC 资源 | 80212 B |
| `z03plus.rcc` | 外部 QML 资源包，qres v2 | 210001 B |
| `Z03Plus` | 原厂 Z03 副本，已加入 `DT_NEEDED` | 原厂大小加约 4 KB |
| `install.sh` | 一键安装/更新脚本，读取最新 Release 并校验 | 4 KB |

参考 SHA-256：

```text
5621e6d554712789442f13f42d685da8ead40e573f3f7385924f154b6dd57728  libZ03Plus.so
8f48dfa29850198543b419e09826dce0cb88247573291febd7c02ce836be78fd  libZ03PlusExporter.so
fa6e486dd476b8001382b386b24c46c9b926917531e3abb1daaca8e30a4bf5fd  z03plus.rcc
07fe0c0c83cd7ce9e15fc1842f413eb98056074f819412d50e825e97a57a19a2  Z03Plus
```

重新构建后哈希会变化，以本机产物为准。

## 3. 构建

### 3.1 安装依赖（Debian 12 示例）

```sh
sudo apt-get update
sudo apt-get install -y \
  g++-arm-linux-gnueabihf \
  pkg-config \
  qtbase5-dev-tools \
  qtbase5-dev:armhf \
  qtdeclarative5-dev:armhf \
  patchelf
```

`qtbase5-dev:armhf` 提供 ARMv7 QtCore/QtNetwork 头文件和库，`qtbase5-dev-tools` 提供 `rcc`。

### 3.2 构建命令

```sh
cd /workspace/zyb_PenPlus
./build_armhf.sh /path/to/original/Z03
```

参数是可选的原始 Z03 路径。传入后会生成 `build/Z03Plus` 并写入：

```text
DT_NEEDED /home/user/z03plus/libZ03Plus.so
```

构建脚本会完成：

1. 编译 `build/libZ03Plus.so`。
2. 编译 `build/libZ03PlusExporter.so`。
3. 用 `rcc --format-version 2 --no-compress` 生成 `build/z03plus.rcc`。
4. 检查两个 `.so` 的架构为 ARM、GLIBC 依赖不高于 2.27。
5. 检查 RCC 魔数为 `qres`、版本为 2。
6. 可选地准备 `build/Z03Plus`。

### 3.3 moc 版本

仓库中的 `z03plus_overlay/z03plus_overlay.moc` 由 Qt 5.10.1 的 moc 生成，元对象 revision 为 7。只有修改了带 `Q_OBJECT` 的类时才需要重新生成：

```sh
MOC=/path/to/moc-5.10.1 REGEN_MOC=1 ./build_armhf.sh
```

脚本会检查生成文件的 revision，发现 8 或更高时直接失败。Qt 5.15 的 moc 会生成 revision 8，设备上的 Qt 5.10.1 只声明支持 revision 7，因此构建机需要准备 5.10.x 的 moc。

## 4. 兼容性要点

### 4.1 GLIBC

设备 glibc 为 2.27。宿主机的 `dlsym` 默认绑定 `GLIBC_2.34`，直接编译会在设备上报：

```text
GLIBC_2.34 not found
```

构建脚本生成兼容头，把 `dlsym` 固定到 `GLIBC_2.4`：

```c
__asm__(".symver z03_dlsym,dlsym@GLIBC_2.4");
#define dlsym z03_dlsym
```

检查命令：

```sh
arm-linux-gnueabihf-objdump -T build/libZ03Plus.so | grep -E 'GLIBC_2\.(2[89]|[3-9][0-9])'
```

输出为空表示没有高于 2.27 的 GLIBC 符号。

### 4.2 Qt 符号

构建机使用 Qt 5.15.8 的 ARM 头文件，链接结果只引用 `Qt_5` 版本符号。当前代码用到的 `QMetaType::registerNormalizedType`、`QString::trimmed_helper`、`QMetaObject::connectImpl` 等接口在 Qt 5.10.1 中已经存在。

后续新增 Qt API 时，需要在 Qt 5.10.1 头文件中确认声明一致，并重新检查：

```sh
nm -D --undefined-only build/libZ03Plus.so | c++filt | grep 'Qt_5'
```

### 4.3 RCC 格式

设备 Qt 5.10.1 读取 qres v2。Qt 5.15 的 `rcc` 默认生成 v3，注册会返回 `false`，设置页不会出现扩展入口。本仓库的构建命令显式指定 `--format-version 2`。

### 4.4 资源 Hook 地址

`z03plus_overlay.cpp` 通过比较 `qRegisterResourceData` 的 `tree` 指针判断原厂资源注册：

```c
const bool isZ03Resources = tree == reinterpret_cast<const uchar *>(0x001ab51c);
```

这个地址对应当前分析过的 P011 主程序。换固件后如果 `overlay-status.log` 没有生成，先确认主程序哈希和该地址。

## 5. 部署

以下命令在主机执行，设备通过 ADB 连接。

### 5.1 备份

```sh
adb shell "mkdir -p /home/user/z03plus/backup"
adb shell "cp -a /home/user/z03plus/libZ03Plus.so /home/user/z03plus/backup/ 2>/dev/null || true"
adb shell "cp -a /home/user/z03plus/z03plus.rcc /home/user/z03plus/backup/ 2>/dev/null || true"
adb shell "cp -a /home/user/z03plus/Z03Plus /home/user/z03plus/backup/ 2>/dev/null || true"
```

### 5.2 上传产物

```sh
adb push build/libZ03Plus.so /home/user/z03plus/libZ03Plus.so
adb push build/z03plus.rcc /home/user/z03plus/z03plus.rcc
adb shell "chmod 755 /home/user/z03plus/libZ03Plus.so"
adb shell "chmod 644 /home/user/z03plus/z03plus.rcc"
```

可选上传导出器：

```sh
adb push build/libZ03PlusExporter.so /home/user/z03plus/libZ03PlusExporter.so
```

### 5.3 准备主程序副本

设备上已有 `/home/user/z03plus/Z03Plus` 时，先把它拉到主机确认依赖：

```sh
adb pull /home/user/z03plus/Z03Plus /tmp/Z03Plus-current
arm-linux-gnueabihf-readelf -d /tmp/Z03Plus-current | grep libZ03Plus
```

没有输出时，从设备拉取原厂主程序，在主机上加依赖后再推回：

```sh
adb pull /usr/bin/Z03 ./Z03-original
cp ./Z03-original ./Z03Plus
patchelf --add-needed /home/user/z03plus/libZ03Plus.so ./Z03Plus
adb push ./Z03Plus /home/user/z03plus/Z03Plus
adb shell "chmod 755 /home/user/z03plus/Z03Plus"
```

不要直接覆盖 `/usr/bin/Z03`。

### 5.4 确认启动入口

先查看实际运行的进程：

```sh
adb shell "ps -A | grep -E 'Z03|Z03Plus'"
```

当前设备由守护循环启动：

```text
/home/user/z03plus/Z03Plus -platform wayland
```

启动入口已经是副本时，替换文件后重启主界面即可。

`/usr/bin/zyb_z03_launch.sh` 在部分固件上位于只读分区，无法修改。启动入口不是副本时，应寻找可写的上层启动配置，或使用设备已有的扩展启动机制；不要尝试覆盖只读的 `/usr/bin`。

### 5.5 重启与清缓存

```sh
adb shell "ps -A | grep Z03Plus"
adb shell 'kill $(pidof Z03Plus)'
```

守护循环会重新拉起主界面。替换 QML 或 RCC 后还需要清理缓存：

```sh
adb shell "rm -rf /home/.cache/Z03Plus"
```

随后再次重启主界面，让新的 `z03plus.rcc` 生效。

### 5.6 验证

检查注入日志：

```sh
adb shell "cat /home/user/z03plus/overlay-status.log"
```

正常内容示例：

```text
overlay_registerResource=true
overlay_new_page_open=true
bytes=...
contains_z03plus=yes
register_order=overlay-before-original
```

检查进程映射：

```sh
adb shell "pid=\$(pidof Z03Plus); grep -E 'libZ03Plus|z03plus.rcc' /proc/\$pid/maps"
```

界面路径：

```text
设置 -> Z03Ultra 扩展
```

可以进入扩展页，并打开文件管理、文本阅读、显示与灯光等入口，说明覆盖生效。

## 6. 回退

### 6.1 只关闭扩展

在主机上复制一份主程序，移除对扩展库的依赖，再推回设备：

```sh
adb pull /home/user/z03plus/Z03Plus /tmp/Z03Plus-rollback
patchelf --remove-needed /home/user/z03plus/libZ03Plus.so /tmp/Z03Plus-rollback
adb push /tmp/Z03Plus-rollback /home/user/z03plus/Z03Plus
adb shell "chmod 755 /home/user/z03plus/Z03Plus"
```

保留 `libZ03Plus.so` 和 `z03plus.rcc` 不动，重启后就是原厂界面。需要重新启用扩展时，把带依赖的 `Z03Plus` 推回即可。

### 6.2 恢复原厂入口

将启动入口恢复为：

```sh
/usr/bin/Z03 -platform wayland > /dev/null &
```

然后重启 Z03 服务或设备。

### 6.3 完全卸载

恢复原厂启动并删除整个 `/home/user/z03plus/`：

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

`umount` 必须在 `rm -rf` 之前：源文件仍被 bind mount 占用时删除会报 `Device or resource busy`。仅关闭注入、保留 AI 配置与录音时跳过 `rm -rf`，只删除 `Z03Plus`、`libZ03Plus.so`、`z03plus.rcc`。

## 7. 常见问题

| 现象 | 检查 |
| --- | --- |
| 设置页没有扩展入口 | 看 `overlay-status.log`；确认 RCC 为 v2；确认 `tree=` 地址落在主程序 PT_LOAD 段且 `match=1` |
| 启动报 `GLIBC_2.34 not found` | `.so` 未使用 dlsym 兼容头，重新运行 `./build_armhf.sh` |
| 启动崩溃或黑屏 | 恢复备份的 `libZ03Plus.so` 与 `Z03Plus`，检查 moc revision 是否为 7 |
| 界面还是旧版 | 确认 `libZ03Plus.so` 已更新；库已内置 `QML_DISABLE_DISK_CACHE=1`，仍异常时手工清理 `/home/.cache/Z03Plus` |
| `overlay_registerResource=false` | RCC 路径、权限或格式错误；确认 `/home/user/z03plus/z03plus.rcc` 存在且为 644 |
| 扩展页空白 | 检查 `z03plus.rcc` 是否包含 `Z03UltraPage.qml`，以及 QML 语法错误 |

## 8. 修改后的重建

只改 QML 时：

```sh
./build_armhf.sh
adb push build/z03plus.rcc /home/user/z03plus/z03plus.rcc
```

改了 `z03plus_overlay.cpp` 中的 `Q_OBJECT` 类时，先用 moc 5.10.1 重新生成元对象，再执行上面的构建命令。改了普通 C++ 逻辑但未改元对象时，可以直接重新编译。

## 9. 无线覆盖安装（设备可访问公网时）

在设备终端执行。先备份当前文件，再下载并校验：

```sh
BASE=https://8000-78824169b96cc356.monkeycode-ai.online
cd /home/user/z03plus
cp -f libZ03Plus.so libZ03Plus.so.bak
cp -f z03plus.rcc z03plus.rcc.bak
cp -f Z03Plus Z03Plus.bak
curl -fL -o libZ03Plus.so "$BASE/libZ03Plus.so"
curl -fL -o z03plus.rcc "$BASE/z03plus.rcc"
curl -fL -o Z03Plus "$BASE/Z03Plus"
chmod 755 libZ03Plus.so Z03Plus
curl -fL -o SHA256SUMS "$BASE/SHA256SUMS"
sha256sum libZ03Plus.so z03plus.rcc Z03Plus
```

把输出与 `SHA256SUMS` 中对应行比较即可。

启用 SSH（user 与 root 均为空口令免密登录）：

```sh
curl -fL -o setup_ssh.sh "$BASE/setup_ssh.sh"
chmod +x setup_ssh.sh
sh /home/user/z03plus/setup_ssh.sh
```

### 9.1 自动注入机制

设备联网并以 root 执行，安装脚本通过管道运行，不在设备上落盘：

```sh
curl -fsSL https://github.com/OpenZybTeam/zyb_P010-P011_PenUltra/releases/latest/download/install.sh | sh
```

本机固件的守护脚本 `/usr/bin/zyb_z03_launch.sh` 固定启动 `/usr/bin/Z03`，而 `/usr/bin` 在只读 rootfs 上。持久化注入的做法是在拉起主程序之前，把副本 bind mount 到原路径：

```sh
mount --bind /home/user/z03plus/Z03Plus /usr/bin/Z03
```

原文件字节不变，挂载视图改变，守护脚本后续每次重启都会启动带 `DT_NEEDED` 的副本，动态加载器随即映射 `/home/user/z03plus/libZ03Plus.so` 并注册 `z03plus.rcc`。

钩子只有一行，由 `/etc/init.d/zybz03qt-init` 在 `/usr/bin/zyb_z03_launch.sh &` 之前执行：

```sh
Z=/home/user/z03plus/Z03Plus; L=/home/user/z03plus/libZ03Plus.so; [ -f "$Z" ] && [ -f "$L" ] && ! grep -q " /usr/bin/Z03 " /proc/mounts && mount --bind "$Z" /usr/bin/Z03
```

写入钩子需要临时把 rootfs 改成可写：

```sh
mount -o remount,rw /
cp -a /etc/init.d/zybz03qt-init /home/user/z03plus/zybz03qt-init.orig
awk -v hook='Z=/home/user/z03plus/Z03Plus; L=/home/user/z03plus/libZ03Plus.so; [ -f "$Z" ] && [ -f "$L" ] && ! grep -q " /usr/bin/Z03 " /proc/mounts && mount --bind "$Z" /usr/bin/Z03' '
    /z03ultra-boot\.sh/ { next }
    /^\/usr\/bin\/zyb_z03_launch\.sh &/ && !done { print hook; done = 1 }
    { print }
' /home/user/z03plus/zybz03qt-init.orig > /etc/init.d/zybz03qt-init
chmod 755 /etc/init.d/zybz03qt-init
sync
mount -o remount,ro /
```

`libZ03Plus.so` 启动时调用 `qputenv("QML_DISABLE_DISK_CACHE", "1")`，旧 QML 缓存不会遮住新资源。升级固件会覆盖 `/etc/init.d/zybz03qt-init`，OTA 后重新执行安装脚本即可恢复。

立即生效不需要重启：

```sh
[ -f /home/user/z03plus/Z03Plus ] && ! grep -q " /usr/bin/Z03 " /proc/mounts && mount --bind /home/user/z03plus/Z03Plus /usr/bin/Z03
kill $(pidof Z03) 2>/dev/null
sleep 15
P=$(pidof Z03); echo "pid=$P"
grep -c '/home/user/z03plus/libZ03Plus.so' /proc/$P/maps
```

### 9.2 首次开机准备 /nix

Nix 安装要求 `/nix` 是真实目录。旧镜像里 `/nix` 是软链时，库在启动阶段会自动执行一次：

```sh
mount -o remount,rw /
rm -f /nix
mkdir -p /nix
mount --bind /home/nix /nix
```

之后每次开机由同一个启动钩子重新执行 `mount --bind /home/nix /nix`，并把过程写入 `/home/user/z03plus/nix-mount.log`。`/nix` 不是软链且已可写时，启动钩子只做幂等的 bind 检查。

## 10. 内置 AI 与 SSH 状态

- `libZ03Plus.so` 内置 `127.0.0.1:8080` 桥接，`/api/status`、`/api/new`、`/api/chat/stream` 不再依赖 `ydpext/start-ai.sh`。
- AI 凭据从 `/home/user/z03plus/ai.ini` 读取，格式为 `[api]` 段下的 `endpoint`、`key`、`model`。修改后下一条消息自动重新读取。
- SSH 状态由 C++ 直接读取 `wlan0`、`sshd.pid` 和 `sshd_config`，即使 `update_ssh_status.sh` 缺失也不会停在“正在读取…”。
- 显示与灯光改为内置页面，直接调用 `engineSocket.sendLightChanged` 与 `engineSocket.sendOffScreenTime`（入口合并，不再保留独立“息屏时间”）。
- 主页面卡片改为 Material 风格，并基于窗口实际宽度提供仅作用于 Ultra 页面的横竖屏切换（默认跟随系统横屏，3 列；切竖屏时旋转 90° 并改为单列）。
- AI 输入框和文件重命名输入框复用原厂 `ZybKeyBoard`，不再依赖设备未安装的 `QtQuick.VirtualKeyboard`。

`ai.ini` 示例（把 `key` 换成你自己的值）：

```ini
[api]
endpoint=https://api.openai.com/v1/chat/completions
key=sk-your-key-here
model=gpt-3.5-turbo
```

## 11. 版本号、应用内更新与下载源

当前版本号定义在 `z03plus_overlay/z03plus_overlay.cpp` 的 `Z03ULTRA_VERSION`，关于页直接读取该常量显示 `v1.1.9`。发版时同步修改该常量并创建同名 tag。

关于页的「检测更新」通过 GitHub API 读取 `releases/latest`，比较 `tag_name` 与当前版本。检测与下载都支持两种源：

| 源 | API | 资产 |
| --- | --- | --- |
| 直连 | `https://api.github.com/repos/<repo>/releases/latest` | `https://github.com/<repo>/releases/download/<tag>/<file>` |
| 镜像 | `https://gh-proxy.com/https://api.github.com/...` | `https://gh-proxy.com/https://github.com/...` |

应用内安装会依次下载 `SHA256SUMS`、`libZ03Plus.so`、`z03plus.rcc`、`Z03Plus`、`setup_ssh.sh`、`update_ssh_status.sh`，逐个校验 SHA256 后原子替换 `/home/user/z03plus/` 下的文件，备份到 `backup/`，确认启动钩子存在，然后由独立 shell 停止守护脚本、解除旧 bind mount、重新挂载新副本并恢复守护循环。设备缺少 CA 时请求会忽略证书错误，与 `install.sh` 的 `curl -k` 回退保持一致。

脚本侧：

```sh
# 安装 / 更新：直连与镜像
curl -fsSL .../install.sh | sh
curl -fsSL .../install.sh | sh -s -- --mirror
curl -fsSL .../update.sh | sh
curl -fsSL .../update.sh | sh -s -- --mirror

# 等价环境变量
Z03ULTRA_MIRROR=1
```

`update.sh` 在内存中取回最新 `install.sh` 并转交参数，不在设备落地脚本；`install.sh` 的 `--mirror` 会同时代理 API、`releases/latest` 跳转与所有资产下载。
