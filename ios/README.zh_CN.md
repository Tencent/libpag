[English](./README.md) | 简体中文

# pag-ios

这是一个用于演示将 libpag 库集成到 iOS 项目中的最小化的项目。

## 前置条件

在开始构建演示项目之前，请确保仔细阅读位于根目录中的 [README.md](../README.md) 文件中的说明。该文档将指导您完成配置
开发环境的必要步骤。

## 构建和运行

在 ios/ 目录中运行以下命令，或者直接双击它：

```
./gen_ios
```

这个命令会生成一个可以运行在 iPhone 真机上的 XCode 工程。如果您希望生成一个运行在模拟器上的项目，可以用以下命令：

```
./gen_simulator
``` 

这个命令会默认生成运行在本机架构上的模拟器项目，例如在 Apple Silicon 设备上是 `arm64`，而在 Intel 设备上是 `x64`。
如果您想要指定生成项目的架构，可以使用 `-a` 参数：

```
./gen_simulator -a x64
```

此外，您也可以使用 `-D` 选项传递 cmake 选项。例如，想生成一个带有 webp 编码支持的项目，可以运行以下命令：

```
./gen_ios -DPAG_USE_WEBP_ENCODE=ON
```

最后，启动 XCode 并打开 ios/PAGViewer.xcworkspace 即可。
