Z03Ultra 源码包
===================

Z03Ultra 是基于 Z03Plus 扩展项目发展而来的分支，保留其 overlay 注入与 QRC 覆盖方案。

用途
----
本项目为紫光展锐 Linux/Qt 词典笔 Z03 的扩展功能原型。
当前已通过外部 Qt RCC 资源覆盖，在原厂“设置”页面中添加：

    设置 -> Z03Ultra 扩展

一键安装 / 更新
---------------
设备联网并以 root 执行：

    curl -fsSL https://github.com/OpenZybTeam/zyb_P010-P011_PenUltra/releases/latest/download/install.sh | sh

脚本通过管道执行，不落盘。读取 GitHub 最新 Release，校验 SHA256，
备份旧文件到 /home/user/z03plus/backup/，安装注入库、RCC 与 Z03Plus
副本，写入一行 bind mount 启动钩子，然后重启主界面并检查注入结果。

无法访问 GitHub 时：
    curl -fsSL <install.sh 地址> | Z03ULTRA_BASE=<release 下载目录> sh

设备环境
--------
- 主程序：/usr/bin/Z03
- CPU：ARMv7 / 32-bit ARM EABI
- 系统：Linux 4.14，glibc 2.27
- Qt：Qt 5.10.1 / Qt Quick / Wayland
- 外部可写目录：/home/user/z03plus/
- 动态库与启动器沿用 libZ03Plus.so、Z03Plus 命名，以兼容既有安装。

已实现
------
1. ARMv7 动态库注入：将 DT_NEEDED 添加到 Z03 的副本，不覆盖原始 /usr/bin/Z03；
   开机由 zybz03qt-init 在启动守护脚本前把副本 bind mount 到 /usr/bin/Z03，自动加载 libZ03Plus.so。
2. Hook qRegisterResourceData()，在原厂 QRC 注册前加载 Z03Ultra overlay。
3. 从运行中的 Z03 导出完整 Qt 资源，设备端导出目录：
       /home/user/z03plus/qrc/
4. 使用兼容的外部 QRC 覆盖：
       qrc:/ZybQmlFiles/ZybSet/SetMainPage.qml
5. 设置页新增“Z03Ultra 扩展”入口。
6. Z03Ultra 首页当前包含文件管理、文本阅读、存储空间、音乐播放、录音、终端 Shell、
   应用商店（Nix）、网页浏览、关于设备、关于 Z03Ultra、显示与灯光、ADB/SSH、AI 对话等入口。
   首页采用 Material 风格卡片，右上角可切换横屏/竖屏（仅作用于 Ultra 页面）。
   - 文件管理：从 / 浏览整个文件系统，支持新建、复制、剪切、重命名、删除、
     压缩、解压与属性查看。
   - 文本阅读/编辑：任意文本文件可预览与编辑，保存时原子写入。

   - 音乐播放：扫描 /mnt/data 与 /home/user 下的音频并通过 MediaPlayer 播放。
   - 录音：按 arecord/tinycap/gst-launch-1.0 选择后端，输出 16 kHz 单声道 WAV。
   - 终端 Shell：/bin/sh -c 执行命令，实时回显输出。
   - 应用商店：使用 Nix（安装在 /home/nix，启动时自动 bind mount 到 /nix；TUNA/NJU 镜像
     下载 2.9.2，复制 store 后手动注册并安装 profile，添加 nixos-22.05
     频道），支持搜索、安装、已安装列表与频道更新。
   - 网页浏览：使用系统自带 QtWebEngine 渲染完整网页（JS/CSS/图片/视频），
     支持点按链接、触摸滚动、前进后退；移动版 UA 适配窄屏。
   - 关于设备：显示型号、内核、架构、CPU、内存、运行时间与根分区信息。
   - 关于 Z03Ultra：显示 OpenZybTeam 与 PenPlus 项目地址。
   - “显示与灯光”会打开原厂 SetLightPage.qml。

重要：RCC 格式
--------------
设备 Qt 5.10.1 无法加载 Qt 5.15 默认生成的 RCC v3。
必须生成 RCC v2：

    rcc --binary --format-version 2 --no-compress z03plus.qrc -o z03plus.rcc

否则 QResource::registerResource() 会返回 false，设置页不会覆盖。

目录说明
--------
- z03plus_overlay/
  实际生效的 overlay：
  - SetMainPage.qml：原厂设置主页的覆盖版，添加入口。
  - Z03UltraPage.qml：扩展主页。
  - z03plus.qrc：两个 QML 的外部资源清单。
  - z03plus_overlay.cpp：注入库，抢先注册外部 QRC。

- z03plus_exporter/
  资源导出器源码。它在 Z03 注册原始 QRC 后遍历 :/ 并导出运行时资源。

- z03plus_tools/
  离线提取 Z03 嵌入 QML 文本块的 Python 工具。

- z03mods_poc/
  初期 ARMv7 注入与 QRC Hook 的验证源码，仅作参考。

构建概览
--------
需要 ARMv7 Linux 交叉编译器、QtCore 开发头文件、与设备兼容的 libstdc++ ABI。

1. 编译 libZ03Plus.so（ARMv7 shared object）。
2. 生成 RCC v2。
3. 从原始 Z03 创建副本并加入依赖：

    patchelf --add-needed /home/user/z03plus/libZ03Plus.so Z03Plus

4. 部署到设备：

    /home/user/z03plus/Z03Plus
    /home/user/z03plus/libZ03Plus.so
    /home/user/z03plus/z03plus.rcc

5. 开机钩子在守护脚本启动前执行：
       mount --bind /home/user/z03plus/Z03Plus /usr/bin/Z03
   原文件不改，守护脚本启动的就是带 DT_NEEDED 的副本。

回退
----
原厂 Z03 不应被覆盖。恢复启动钩子并重启：

    mount -o remount,rw /
    cp -a /home/user/z03plus/zybz03qt-init.orig /etc/init.d/zybz03qt-init
    sync
    mount -o remount,ro /
    kill $(pidof Z03)

bind mount 随重启消失，守护脚本恢复启动原厂 /usr/bin/Z03。
旧版本文件位于 /home/user/z03plus/backup/。

卸载
----
完全移除 Ultra（恢复原厂启动并删除 /home/user/z03plus/）：

    G=$(ps w | grep '[z]yb_z03_launch.sh' | awk '{print $1}' | head -n 1)
    [ -n "$G" ] && kill -STOP $G
    kill $(pidof Z03) 2>/dev/null
    sleep 1
    umount /usr/bin/Z03 2>/dev/null || umount -l /usr/bin/Z03 2>/dev/null
    mount -o remount,rw /
    cp -a /home/user/z03plus/zybz03qt-init.orig /etc/init.d/zybz03qt-init
    sync
    mount -o remount,ro /
    rm -rf /home/user/z03plus
    [ -n "$G" ] && kill -CONT $G

必须先 umount 再删目录，否则源文件被 bind mount 占用会报 busy。
只想关闭注入时跳过 rm -rf，保留 AI 配置与录音。

注意事项
--------
- 修改 QML 或 RCC 后重启主界面即可。库启动时设置
  QML_DISABLE_DISK_CACHE=1，不再依赖手工清理缓存。
- 原厂 BasicButton 默认 checkable: true。Z03Plus 功能入口必须显式：

    checkable: false

  否则点击后会保持绿色选中状态。
- 不要直接修改 /usr/bin/Z03，始终使用 /home/user/z03plus/Z03Plus 副本。
