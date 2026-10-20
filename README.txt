Z03Ultra 源码包
===================

Z03Ultra 是基于 Z03Plus 扩展项目发展而来的分支，保留其 overlay 注入与 QRC 覆盖方案。

用途
----
本项目为紫光展锐 Linux/Qt 词典笔 Z03 的扩展功能原型。
当前已通过外部 Qt RCC 资源覆盖，在原厂“设置”页面中添加：

    设置 -> Z03Ultra 扩展

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
1. ARMv7 动态库注入：将 DT_NEEDED 添加到 Z03 的副本，不覆盖原始 /usr/bin/Z03。
2. Hook qRegisterResourceData()，在原厂 QRC 注册前加载 Z03Ultra overlay。
3. 从运行中的 Z03 导出完整 Qt 资源，设备端导出目录：
       /home/user/z03plus/qrc/
4. 使用兼容的外部 QRC 覆盖：
       qrc:/ZybQmlFiles/ZybSet/SetMainPage.qml
5. 设置页新增“Z03Ultra 扩展”入口。
6. Z03Ultra 首页当前包含文件管理、文本阅读、存储空间、音乐播放、录音、终端 Shell、
   应用商店（Nix）、网页浏览、关于设备、显示与灯光、ADB/SSH、AI 对话与息屏等入口。
   - 存储空间：显示各分区用量与文件系统，支持清理缓存。
   - 音乐播放：扫描 /mnt/data 与 /home/user 下的音频并通过 MediaPlayer 播放。
   - 录音：按 arecord/tinycap/gst-launch-1.0 选择后端，输出 16 kHz 单声道 WAV。
   - 终端 Shell：/bin/sh -c 执行命令，实时回显输出。
   - 应用商店：使用 Nix（单用户安装，TUNA 镜像下载 2.9.2 + nixos-22.05 频道），
     支持搜索、安装、已安装列表与频道更新。
   - 网页浏览：纯文本浏览器，支持地址栏、搜索、后退与链接跳转。
   - 关于设备：显示型号、内核、架构、CPU、内存、运行时间与根分区信息。
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

5. 启动脚本将 /usr/bin/Z03 替换为 Z03Plus 副本的启动路径。

回退
----
原厂 Z03 不应被覆盖。将启动脚本恢复为：

    /usr/bin/Z03 -platform wayland > /dev/null &

然后重启 Z03 服务即可。

注意事项
--------
- 修改 QML 或 RCC 后，须删除：/home/.cache/Z03Plus/
  并重启 Z03Plus，QML 改动才会重新加载。
- 原厂 BasicButton 默认 checkable: true。Z03Plus 功能入口必须显式：

    checkable: false

  否则点击后会保持绿色选中状态。
- 不要直接修改 /usr/bin/Z03，始终使用 /home/user/z03plus/Z03Plus 副本。
