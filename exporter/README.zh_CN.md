[English](./README.md) | 简体中文

## 介绍
***

PAGExporter是一款AE（Adobe After Effects）导出插件，能够一键将设计师在 AE 中制作的动效内容导出成素材文件。  
在纯矢量导出方式支持更多 AE 特性的同时，还引入了BMP预合成结合矢量的混合导出能力，实现支持所有 AE 特性的同时又能保持动效运行时的可编辑性。

## 开发指南
***

**如果您希望参与到PAGExporter的源码开发中，请务必严格按照以下步骤先配置完开发环境再进行开发和调试。**

我们推荐使用 CLion 并在 macOS 平台上进行开发。

### 环境准备

首先通过如下链接下载AESDK

`https://developer.adobe.com/console/3505738/servicesandapis`

建议下载2023 windows版，然后将解压后的AESDK绝对路径写入./AESDK.cfg，文件内容示例如下

`set(AE_SDK_PATH /Users/[username]/libpag/exporter/vendor/AfterEffectsSDK/Examples)`

其次将QT cmake路径写入QTCMAKE.cfg，文件内容示例如下

`set(CMAKE_PREFIX_PATH /Users/[username]/Qt/6.6.1/macos/lib/cmake)`

其他环境依赖同[libpag](../README.zh_CN.md)

### 编译项目

请先确保libpag能正常编译，然后再尝试编译PAGExporter，直接使用 CLion 打开项目根目录即可开始编译。

**macOS 平台：**

不需要对 CLion 进行额外配置即可立即编译。

**Windows 平台：**

请参考以下步骤配置好 CLion 之后再进行编译：

- 请确保你的 VS2019 至少同时安装了 **[使用 C++ 的桌面开发]** 和 **[通用 Windows 平台开发]** 两个子模块。
- 在 CLion 的选项菜单里搜索 **ToolChain** ，设置默认编译工具为 **Visual Studio**，并选择 **amd64（推荐）** 或 **x86** 架构。

注：**如果在 CMake 编译过程中遇到报错，可以尝试更新 CMake 命令行工具到最新的版本然后重新编译。**
