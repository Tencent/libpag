[English](./README.md) | 简体中文

# QT Demo 编译指南

请务必先参考根目录的 [README.md](./../README.zh_CN.md) 中编译指南一节，成功编译出 libpag 的库文件后再进行 QT 平台的 Demo 工程编译和运行。
QT 环境推荐安装使用 **Qt5.13.0** 版本。

## macOS

直接用 CLion 打开根目录下的 qt 文件夹，首次刷新会提示 QT SDK 找不到，请打开自动生成的 qt/QTCMAKE.cfg 配置文件，修改其中的 QT 路径为本地安装路径即可。然后直接编译运行 PAGViewer。

## Windows

- 请参考 [README.md](./../README.zh_CN.md) 中编译指南一节，提前安装好 VS2019 版本的必要模块并确保 CLion 的 ToolChain 是 Visual Studio，并选中 **amd64** 架构。
- 用 CLion 打开根目录下的 qt 文件夹，首次刷新会提示 QT SDK 找不到，请打开自动生成的 qt/QTCMAKE.cfg 配置文件，修改其中的 QT 路径为本地安装路径即可。例如：`C:/Qt/Qt5.13.0/5.13.0/msvc2017_64/lib/cmake`。
- 在 CLion 中打开 PAGViewer 目标的配置面板，在 Environment Variables 一行填入本地 QT 的 DLL 库路径，例如：`PATH=C:\Qt\Qt5.13.0\5.13.0\msvc2017_64\bin`。
- 最后编译并运行 PAGViewer 目标即可。


# QT Demo

Please make sure you have built the libpag library successfully before building the QT demo project.

## macOS

Open the `qt` folder with CLion, and then open the `qt/QTCMAKE.cfg` file to modify the QT path to 
your local QT installation path. Then you can build and run the `PAGViewer` target.

## Windows

- Please refer to the build guide in [README.md](./../README.md) to install the necessary modules of VS2019 and make sure the ToolChain of CLion is Visual Studio with **amd64** architecture.
- Open the `qt` folder with CLion, and then open the `qt/QTCMAKE.cfg` file to modify the QT path to your local QT installation path. For example: `C:/Qt/Qt5.13.0/5.13.0/msvc2017_64/lib/cmake`.
- Open the configuration panel of `PAGViewer` target in CLion, and then fill in the local QT DLL library path in the `Environment Variables` row. For example: `PATH=C:\Qt\Qt5.13.0\5.13.0\msvc2017_64\bin`.
- Finally, build and run the `PAGViewer` target.




