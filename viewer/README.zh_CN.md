[English](./README.md) | 简体中文

# QT Demo 编译指南

请务必先参考根目录的 [README.md](./../README.zh_CN.md) 中编译指南一节，成功编译出 libpag 的库文件后再进行 QT 平台的 Demo 工程编译和运行。
QT 环境推荐安装使用 **Qt6.2.0** 版本。

## macOS

直接用 CLion 打开根目录下的 viewer 文件夹，首次刷新会提示 QT SDK 找不到，请打开自动生成的 viewer/QTCMAKE.cfg 配置文件，修改其中的 QT 路径为本地安装路径即可。然后直接编译运行 PAGViewer。

## Windows

- 请参考 [README.md](./../README.zh_CN.md) 中编译指南一节，提前安装好 VS2019 版本的必要模块并确保 CLion 的 ToolChain 是 Visual Studio，并选中 **amd64** 架构。
- 用 CLion 打开根目录下的 viewer 文件夹，首次刷新会提示 QT SDK 找不到，请打开自动生成的 viewer/QTCMAKE.cfg 配置文件，修改其中的 QT 路径为本地安装路径即可。例如：`C:/Qt/Qt6.2.0/6.2.0/msvc2019_64/lib/cmake`。
- 在 CLion 中打开 PAGViewer 目标的配置面板，在 Environment Variables 一行填入本地 QT 的 DLL 库路径，例如：`PATH=C:\Qt\Qt6.2.0\6.2.0\msvc2019_64\bin`。
- 最后编译并运行 PAGViewer 目标即可。



