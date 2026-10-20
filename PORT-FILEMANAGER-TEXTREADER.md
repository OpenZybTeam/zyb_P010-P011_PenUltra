# 文件管理与文本阅读移植说明

本次实现参考 PenMods 的 `src/filemanager/FileManager.*` 和
`src/filemanager/reader/TextReader.*` 的目录遍历、TXT/MD 文件识别思路，针对
Z03Plus 的 ARMv7 / Qt 5.10 环境重新实现，未直接复制 PenMods 二进制或其完整框架。

## 新增文件

- `z03plus_overlay/FileManagerPage.qml`：目录列表与目录跳转。
- `z03plus_overlay/TextReaderPage.qml`：纯文本阅读页。

## 修改文件

- `z03plus_overlay/z03plus_overlay.cpp`
  - 注册 `FileService`：列目录、文件尺寸、目录上级跳转；
  - 注册 `TextFileService`：读取 TXT、MD、LRC；
  - 访问范围限制为 `/mnt/data`、`/home/user` 及其子目录；
  - 单文件最大读取量为 2 MiB，避免大文件耗尽 Qt Quick 内存。
- `z03plus_overlay/Z03UltraPage.qml`：两个原本的占位入口均跳转至文件管理器。
- `z03plus_overlay/z03plus.qrc`：加入两个新 QML 文件。

## 构建要点

1. **必须重新运行设备兼容 Qt 5.10.1 的 moc。** `z03plus_overlay.cpp` 新增两个
   `Q_OBJECT` 类，不能继续使用仓库里已有的 `z03plus_overlay.moc`。该文件还是 Qt
   5.15.13 生成的，原本就不适用于 Qt 5.10.1。

   ```sh
   /path/to/qt-5.10.1/bin/moc z03plus_overlay.cpp -o z03plus_overlay.moc
   ```

2. 交叉编译新的 `libZ03Plus.so`，并重新生成 RCC v2：

   ```sh
   rcc --binary --format-version 2 --no-compress z03plus.qrc -o z03plus.rcc
   ```

3. 部署新的 `.so` 和 `.rcc` 后，清理 `/home/.cache/Z03Plus/` 并重启 Z03Plus。

## 已知边界

- 支持浏览目录及打开 `.txt`、`.md`、`.lrc`；不含删除、重命名、创建目录和 Markdown 富文本渲染。
- 文本按 UTF-8 解码。GBK/GB18030 等旧编码文本需要后续增加编码探测或转换。
- 若实际数据盘不是 `/mnt/data`，请在 `Z03PlusFileService::allowed()` 和默认路径中按设备挂载点调整。
