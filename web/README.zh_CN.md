<img src="https://pag.io/img/readme/logo.png" alt="PAG Logo" width="474"/>

[English](./README.md) | 简体中文 | [Homepage](https://pag.io) | [lite版本](https://github.com/Tencent/libpag/tree/main/web/lite)

## 介绍

libpag 是 PAG (Portable Animated Graphics) 动效文件的渲染 SDK，目前已覆盖几乎所有的主流平台，包括：iOS, Android, macOS,
Windows, Linux, 以及 Web 端。

## 特性

- Web 平台能力适配，支持 libpag 全能力
- 基于 WebAssembly + WebGL

## 快速开始

PAG Web 端，由 libpag.js + libpag.wasm 文件组成。

### Browser（推荐）

直接使用 `<script>` 引入，`libpag` 会被注册为一个全局变量

对于生产环境我们推荐使用一个明确的版本号，以避免新版本带来不可预期的影响

```html
<script src="https://cdn.jsdelivr.net/npm/libpag@4.0.5-release.18/lib/libpag.min.js"></script>
```

你可以在公共 CDN [cdn.jsdelivr.net/npm/libpag/](https://cdn.jsdelivr.net/npm/libpag/) 浏览 NPM 包内的内容，同时你也可以使用 `@latest` 将版本指定为最新的稳定版。

也可以使用其他同步 NPM 的公共 CDN 如 [unpkg](https://unpkg.com/libpag@latest/lib/libpag.min.js)

```html
<canvas class="canvas" id="pag"></canvas>
<script src="https://cdn.jsdelivr.net/npm/libpag@latest/lib/libpag.min.js"></script>
<script>
  window.onload = async () => {
    // 实例化 PAG
    const PAG = await window.libpag.PAGInit();
    // 获取 PAG 素材数据
    const buffer = await fetch('https://pag.io/file/like.pag').then((response) => response.arrayBuffer());
    // 加载 PAG 素材为 PAGFile 对象
    const pagFile = await PAG.PAGFile.load(buffer);
    // 将画布尺寸设置为 PAGFile的尺寸
    const canvas = document.getElementById('pag');
    canvas.width = pagFile.width();
    canvas.height = pagFile.height();
    // 实例化 PAGView 对象
    const pagView = await PAG.PAGView.init(pagFile, canvas);
    // 播放 PAGView
    await pagView.play();
  };
</script>
```

调用 libpag.js 上的 `PAGInit()` 方法时，默认会加载 libpag.js 同一目录下的 libpag.wasm 文件。如果你希望把 libpag.wasm 放在其他目录下，则可以使用 `locateFile` 将 libpag.wasm 的路径返回给 `PAGInit()` 方法。如下

```js
const PAG = await window.libpag.PAGInit({ locateFile: (file) => 'https://pag.io/file/' + file });
```

### EsModule

```bash
$ npm i libpag
```

```js
import { PAGInit } from 'libpag';

PAGInit().then((PAG) => {
  // 实例化 PAG
});
```

**使用 ESModule 引入的方式需要注意，像 Webpack 和 Rollup 等打包工具是默认没有打包 .wasm 文件的。也就是说如果你的项目时 Vue / React 这类使用脚手架构建的项目需要把 node_modules 下的 libpag/lib/libpag.wasm 文件打包到最终产物中，并且使用 `locateFile` 钩子返回 libpag.wasm 文件的路径，这样才能确保在网络请求中能加载到 libpag.wasm 文件**

npm package 中提供了多种构建产物，可以阅读 [这里](./doc/develop-install.md) 了解不同目录下产物的差别。

Demo 项目提 [pag-web](https://github.com/libpag/pag-web) 供了简单的接入示例和 Vue / React / PixiJS 等配置示例， 可以点击 [这里](https://github.com/libpag/pag-web) 查看。

更多的 API 接口可以阅读 [API 文档](https://pag.io/api.html#/apis/web/)。

## 浏览器兼容性

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome for Android | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari on iOS |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Chrome >= 69                                                                                                                                                                                                  | Safari >= 11.3                                                                                                                                                                                                | Android >= 7.0                                                                                                                                                                                                            | iOS >= 11.3                                                                                                                                                                                                          |

更多版本的兼容工作正在进行中

**以上的兼容表仅代表可以运行的兼容性。对于有移动端接入需要的用户，需要阅读一下这篇[兼容性情况](./doc/compatibility.md)的文章**

## 使用指南

### 垃圾回收

因为 libpag 基于 WebAssembly 做跨端应用，所以从 WebAssembly 中取出来的对象，基本都是 C++ 的指针，所以 JavaScript 的 GC 是无法释放这一部分内存的。所以，当不需要使用 libpag 产生的对象时，需要调用对象上的 destroy 方法将其释放掉。

### 渲染相关

#### 首帧渲染

`PAGView.init` 默认会进行首帧渲染，如不需要可以在 `PAGView.init` 的第三个参数传入 `{ firstFrame: false }` 取消首帧渲染。

在进行首帧渲染的过程中，当 PAG 动画文件中存在 BMP 预合成（下文说明如何确认 PAG 文件是否存在 BMP 预合成）时，会调用 VideoReader 模块。

因为 VideoReader 模块在 Web 平台依赖于 VideoElement，所以在部分移动端场景下，PAGView.init 不是在用户交互产生的调用链中，可能会存在不允许播放导致无法正常渲染画面的问题。

当出现这种情况，我们推荐使用  `PAGView.init(pagFile, canvas, { firstFrame: false }) ` 提前初始化 PAGView 并且取消首帧渲染，然后在用户交互产生的调用链中再调用 `pagView.play()` 进行画面渲染。

#### PAG 渲染尺寸

在 Web 平台上，设备像素分辨率与 CSS 像素分辨率是不同的，而它们之比被称为 `devicePixelRatio`。所以当我们需要显示 CSS 像素 1px 时， 需要 1px \* `devicePixelRatio` 的渲染尺寸。

PAG 默认会对 Canvas 在屏幕中的可视尺寸进行缩放计算后进行渲染，因此会对 Canvas 的宽高以及 `style` 产生副作用。如果希望 PAG 不对 Canvas 产生副作用， 可以在 `PAGView.init` 的第三个参数传入 `{ useScale: false }` 来取消缩放。

#### PAGView 尺寸过大

为了高清的渲染效果，PAGView 默认会按照 Canvas 尺寸 \* `devicePixelRatio` 作为实际渲染尺寸。
受设备自身性能影响 WebGL 的最大渲染尺寸可能各不相同。会出现渲染尺寸过大导致白屏的情况。

建议移动端下，实际渲染尺寸不大于 2560px。

#### 多个 PAGView 实例场景

首先，因为 PAG Web 版是单线程的 SDK，所以我们不建议**同屏播放多个 PAGView**。

对于有多个 PAGView 实例的场景，我们需要先知道，浏览器环境中 WebGL 活跃的 context 数量是有限制的，Chrome 是 16 个，Safari 是 8 个。因为有这个限制存在，我们应当及时使用 `destroy` 回收无用的 PAGView 实例和移除 Canvas 的引用。

以下是特殊场景的解决方法，不推荐使用：

如果你需要在 Chrome 浏览器中同屏存在多个 PAGView 实例且不需要在 Safari 上使用，可以尝试使用 canvas2D 模式，需要在 `PAGView.init` 的时候传入 `{ useCanvas2D: true }` 。这个模式下，会用一个 WebGL 当作渲染器，然后往多个 canvas2D 分发画面，从而规避 WebGL 活跃 context 数量的限制。

因为 Safari 上 [`CanvasRenderingContext2D.drawImage()`](https://developer.mozilla.org/zh-CN/docs/Web/API/CanvasRenderingContext2D/drawImage) 的性能很差，所以我们不推荐在 Safari 上使用这个模式。

#### 注册解码器

对于“用户与页面交互之后才可以使用 Video 标签进行视频播放”规则的限制，PAG Web 版提供软件解码器注入接口 ` PAG.registerSoftwareDecoderFactory()`。

注入软件解码器后，PAG 会调度软件解码器去对 BMP 预合成进行解码与上屏，从而实现部分平台进入页面自动播放的功能。

推荐解码器接入：https://github.com/libpag/ffavc/tree/main/web

已知“用户与页面交互之后才可以使用 Video 标签进行视频播放”限制的平台有：移动端微信浏览器，iPhone 省电模式，部分 Android 浏览器。

## 关于 BMP 预合成

可以下载 [PAGViewer](https://pag.io/docs/install.html) 打开 PAG 文件，点击"视图"->"显示 编辑面板"，在编辑面板中我们能看到 Video 的数量，当 Video数量大于 0 时，即为 PAG 动画文件中存在 BMP 预合成。

## Roadmap

Web SDK 未来能力支持规划可以点击 [这里](https://github.com/Tencent/libpag/wiki/PAG-Web-roadmap) 查看

## 参与开发

### 前置工作

需要确保已经可编译 C++ libpag 库，并且安装 [Emscripten 套件](https://emscripten.org/docs/getting_started/downloads.html) 和 Node 依赖

```bash
# 安装Node依赖
$ npm install
```

### 开发流程

执行 `build.sh debug` 来获得 `libpag.wasm` 文件

```bash
# web/script目录下
$ cd script
# 添加执行权限
$ chmod +x ./build.sh
# 打包
$ ./build.sh debug
```

打包 Typescript 文件，修改 Typescript 文件会自动打包到 Javascript 文件

```bash
# web目录下
$ npm run dev
```

启动 HTTP 服务

```bash
# libpag根目录下
$ emrun --browser chrome --serve_root . --port 8081 ./web/demo/index.html
```

### 生产流程

执行 `build.sh` 脚本

```bash
# web/script目录下
$ cd script
# 添加执行权限
$ chmod +x ./build.sh
# 打包
$ ./build.sh
```

### CLion 编译

创建一个新的 profile，然后使用下面的 **CMake options**（位置在 **CLion** > **Preferences** > **Build, Execution, Deployment** > **CMake**）

```
CMAKE_TOOLCHAIN_FILE=path/to/emscripten/emscripten/version/cmake/Modules/Platform/Emscripten.cmake
```

### 测试流程

打包生产版本

```bash
$ cd script & ./build.sh
```

启动测试 HTTP 服务

```bash
$ npm run server
```

启动 cypress 测试

```bash
$ npm run test
```
