<img src="resources/readme/logo.png" alt="PAG Logo" width="474"/>

[![license](https://img.shields.io/badge/license-Apache%202-blue)](https://github.com/Tencent/libpag/blob/master/LICENSE.txt) 
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/libpag/pulls) 
[![codecov](https://codecov.io/gh/Tencent/libpag/branch/main/graph/badge.svg)](https://codecov.io/gh/Tencent/libpag)
[![Actions Status](https://github.com/Tencent/libpag/workflows/autotest/badge.svg?branch=main)](https://github.com/Tencent/libpag/actions)
[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Tencent/libpag)](https://github.com/Tencent/libpag/releases)

[English](./README.md) | 简体中文 | [官网](https://pag.io)
| [QQ群：893379574](https://qm.qq.com/cgi-bin/qm/qr?k=Wa65DTnEKo2hnPsvY-1EgJOF8tvKQ-ZT&jump_from=webapi)

## 介绍

libpag 是 PAG (Portable Animated Graphics) 动效文件的渲染 SDK，目前已覆盖几乎所有的主流平台，包括：iOS, Android, macOS,
Windows, Linux, 以及 Web 端。

PAG 方案是一套完善的动效工作流。提供从 AE（Adobe After Effects）导出插件，到桌面预览工具 PAGViewer，再到各端的跨平台渲染 SDK。
目标是降低或消除动效研发相关的成本，打通设计师创作到素材交付上线的极简流程，不断输出运行时可编辑的高质量动效内容。

PAG 方案目前已经接入了腾讯系 40 余款应用，包括微信，手机QQ，王者荣耀，腾讯视频，QQ音乐等头部产品，稳定性经过了海量用户的持续验证，可以广泛应用于UI动画、贴纸
动画、视频编辑、模板设计等场景。典型应用场景可以参考[官网案例](https://pag.io/case.html)。

详细介绍可以参考相关报道：
 
 - [王者QQ微信都在用的动画神器要开源了：把交付时间缩短90%](https://mp.weixin.qq.com/s/a8-yOp8h5LiFGKSdLE_toA)
 - [腾讯推出移动端动画组件PAG，释放设计生产力](https://mp.weixin.qq.com/s/STxOMV2lqGdGu-9mBkAz_A)

## PAG 优势

- **高效的文件格式**

<img src="resources/readme/intro_1.png" alt="intro_1" width="282"/>

采用可扩展的二进制文件格式，可单文件集成图片音频等资源，实现快速交付。导出相同的 AE 动效内容，在文件解码速度和压缩率上均大幅领先于同类型方案。

- **全 AE 特性支持**

<img src="resources/readme/intro_2.png" alt="intro_2" width="282"/>

在纯矢量导出方式上支持更多 AE 特性的同时，还引入了BMP预合成结合矢量的混合导出能力，实现支持所有 AE 特性的同时又能保持动效运行时的可编辑性。

- **性能监测可视化**

<img src="resources/readme/intro_4.png" alt="intro_4" width="282"/>

通过导出插件内置的自动优化策略，以及预览工具集成的性能监测面板，能够直观地看到每个素材的性能状态，以帮助设计师制作效果和性能俱佳的动画特效。

- **运行时可编辑**

<img src="resources/readme/intro_5.png" alt="intro_5" width="282"/>

运行时，可在保留动效效果前提下，动态修改替换文本或替换占位图内容，甚至对任意子图层进行增删改及移动，轻松实现照片和视频模板等素材的批量化生产。

## 系统要求

- iOS 9.0 版本及以上
- Android 4.4 版本及以上
- macOS 10.13 版本及以上
- Windows 7.0 版本及以上
- Chrome 69.0 版本及以上
- Safari 11.3 版本及以上

## 快速接入

由于大部分平台没有统一的上层业务框架，目前我们暂时只为 iOS, Android 和 Web 平台定期发布预编译的二进制库，其他平台的库需要通过
源码根据自己的实际需求调整参数进行编译。移动端最新的release库可以在 [这里](https://github.com/Tencent/libpag/releases) 
下载。 详细的 SDK接入文档可以参考 [SDK 接入](https://pag.io/docs/sdk.html) 。Web 平台的接入文档可以参考 [Web SDK
接入](./web/README.md)

### iOS 端接入

可以从 [release](https://github.com/Tencent/libpag/releases) 页面下载预编译的二进制库，或者通过CocoaPods接入

在Podfile中添加libpag依赖:

```
pod 'libpag'
```

然后运行:

```
pod install
```

然后在项目中引入libpag的头文件

```
#import <libpag/xxx.h>
```

### Android 端接入

可以从 [release](https://github.com/Tencent/libpag/releases) 页面下载预编译的 aar 库文件，或者通过 Maven 将 libpag 添加到你的项目中：

编辑工程根目录下的 `build.gradle` 文件, 在`repositories`下面添加 `mavenCentral()`  :

```
buildscript {
    repositories {
        mavenCentral()
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:3.2.1'
    }
}
```

在 `app/build.gradle`中添加libpag依赖 (*`4.0.5.10` 应替换为最新发布版本*):

```
dependencies {
    implementation 'com.tencent.tav:libpag:4.0.5.10'
}
```

在混淆列表里面，添加libpag的keep规则

```
  -keep class org.libpag.** {*;}
  -keep class androidx.exifinterface.** {*;}
```

配置完以后，sync一下，再编译即可。

### Web 端接入

直接拷贝如下代码，然后在浏览器中运行即可：

```html
<canvas class="canvas" id="pag"></canvas>
<script src="https://unpkg.com/libpag@latest/lib/libpag.min.js"></script>
<script>
  window.libpag.PAGInit().then((PAG) => {
    const url = 'https://pag.io/file/like.pag';
    fetch(url)
      .then((response) => response.blob())
      .then(async (blob) => {
        const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
        // Do Something.
      });
  });
</script>
```
Web 端更多接入方式请参考：[Web端接入指南](https://pag.io/docs/sdk-web.html)

### 范例工程

我们准备了如下各端的 Demo 工程，检出对应平台的 Demo 工程可以快速开始学习如何使用 libpag 的 API：

- [https://github.com/libpag/pag-ios](https://github.com/libpag/pag-ios)
- [https://github.com/libpag/pag-android](https://github.com/libpag/pag-android)
- [https://github.com/libpag/pag-web](https://github.com/libpag/pag-web)

### API 手册：

- [iOS API 参考](https://pag.io/api.html#/apis/ios/)
- [Android API 参考](https://pag.io/api.html#/apis/android/org/libpag/package-summary.html)
- [Web API 参考](https://pag.io/api.html#/apis/web/)

更多的其他文档可以访问官网  [pag.io](https://pag.io) 获得。

## 开发指南

**如果您希望参与到 libpag 项目的源码开发中，请务必严格按照以下步骤先配置完开发环境再进行开发和调试。**

我们推荐使用 CLion 并在 macOS 平台上进行开发。

### 分支介绍

`main` 分支是我们主要的开发分支，日常会有频繁的修改合入。`main` 分支稳定后会不定期创建发布分支 `release/{version}`。`release/` 下
的分支是我们推荐的在生产环境中使用的稳定版本。稳定版本一旦拉取出分支后，将不再合入新功能，只做缺陷问题的修复。

**注：这个仓库只包含从 PAG 4.0 版本开始的最新代码。PAG 3.0 及以下版本的历史记录因开源合规的要求无法对外提供，如需使用 PAG 3.0 的预编译库
可以从 [这里](https://github.com/tencent/libpag/releases) 下载。**

### 编译环境

- Xcode 11.0 版本及以上
- GCC 7.0 版本及以上
- CMake 3.10.2 版本及以上
- Visual Studio 2019
- NDK 19.2.5345600 （**其他 NDK 版本可能会存在编译报错**)

### 依赖管理

libpag 使用 depsync 命令行工具管理第三方依赖项。

**macOS 平台：**

直接在项目根目录下运行脚本即可安装所有必要工具并进行第三方仓库的同步：

```
./sync_deps.sh
```
**其他平台：**

首先确保您已经安装了最新版本的 [node.js](http://nodejs.org/)，然后使用 npm 安装 depsync 命令行工具 ：

```
npm install -g depsync
```

首次检出的仓库需要在项目根目录运行 depsync 命令来同步第三方依赖仓库：

```
depsync
```

同步过程可能出现要求输入 git 账号密码，请确保已开启了 git 的凭证存储功能，以便后续可以由 CMake 自动触发同步，无需重复输入密码。

### 编译项目

第三方依赖项都同步完成后，直接使用 CLion 打开项目根目录即可开始编译。若第三方依赖项发生改变，刷新 CMakeLists.txt 文件即可自动同步。

**macOS 平台：**

不需要对 CLion 进行额外配置即可立即编译。**目前只有 macOS 平台支持自动化测试用例的运行。**

**Windows 平台：**

请参考以下步骤配置好 CLion 之后再进行编译：

- 请确保你的 VS2019 至少同时安装了 **[使用 C++ 的桌面开发]** 和 **[通用 Windows 平台开发]** 两个子模块。
- 在 CLion 的选项菜单里搜索 **ToolChain** ，设置默认编译工具为 **Visual Studio**，并选择 **amd64（推荐）** 或 **x86** 架构。

注：**如果在 CMake 编译过程中遇到报错，可以尝试更新 CMake 命令行工具到最新的版本然后重新编译。**
另外，由于团队日常主要都在 macOS 平台上进行开发，Windows
平台偶尔可能会出现编译不通过的情况，如果遇到阻塞的问题欢迎通过提交 [issue](https://github.com/Tencent/libpag/issues/new/choose)
或 [QQ](https://qm.qq.com/cgi-bin/qm/qr?k=Wa65DTnEKo2hnPsvY-1EgJOF8tvKQ-ZT&jump_from=webapi)
群即时跟我们反馈。

## 协议

libpag 基于 [Apache-2.0](./LICENSE.txt) 协议开源.

## 隐私政策

使用 libpag SDK 时请遵守 [PAG SDK个人信息保护规则](https://privacy.qq.com/document/preview/01e79d0cc7a2427ba774b88c6beff0fd).

## 贡献

我们欢迎开发人员为腾讯的开源做出贡献，如果您对改进 libpag
项目有任何的想法或建议，欢迎给我们提交 [issue](https://github.com/Tencent/libpag/issues/new/choose)
或 [pull request](https://github.com/Tencent/libpag/pulls) 。 在发起 issue 或 pull request 前,
请确保您已经阅读 [贡献指南](./CONTRIBUTING.md) 。


