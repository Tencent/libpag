# QT Demo 编译指南

请务必先参考根目录的 [README.md](./../README.zh_CN.md) 中编译指南一节，成功编译出 libpag 的库文件后再进行 QT 平台的 Demo 工程编译和运行。
QT 环境推荐安装使用 **Qt5.13.0** 版本。

## macOS

直接用 CLion 打开根目录下的 qt 文件夹，首次刷新会提示 QT SDK 找不到，请打开自动生成的 qt/QTCMAKE.cfg 配置文件，修改其中的 QT 路径为本地安装路径即可。然后直接编译运行 PAGViewer。

## Windows

- 请提前安装好 VS2019 版本，至少必须同时安装 **[使用 C++ 的桌面开发]** 和 **[通用 Windows 平台开发]** 两个子模块。
- 在 CLion 的选项菜单里搜索 **ToolChain** ，设置默认编译工具为 Visual Studio，并选择 **amd64** 架构。
- 用 CLion 打开根目录下的 qt 文件夹，首次刷新会提示 QT SDK 找不到，请打开自动生成的 qt/QTCMAKE.cfg 配置文件，修改其中的 QT 路径为本地安装路径即可。例如：`C:/Qt/Qt5.13.0/5.13.0/msvc2017_64/lib/cmake`。
- 在 CLion 中打开 PAGViewer 目标的配置面板，在 Environment Variables 一行填入本地 QT 的 DLL 库路径，例如：`PATH=C:\Qt\Qt5.13.0\5.13.0\msvc2017_64\bin`。
- 最后编译并运行 PAGViewer 目标即可。
