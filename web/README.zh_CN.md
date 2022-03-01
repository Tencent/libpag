<img src="https://pag.io/img/readme/logo.png" alt="PAG Logo" width="474"/>

[English](./README.md) | 简体中文 | [Homepage](https://pag.io)

> **当前版本为 Alpha 版本，部分功能不够稳定**
>
> **有问题可到[Issues](https://github.com/Tencent/libpag/issues)，会尽快修复**
>
> **更多特性持续开发中**

## 介绍

libpag 是 PAG (Portable Animated Graphics) 动画文件的渲染 SDK，目前已覆盖几乎所有的主流平台，包括：iOS, Android, macOS,
Windows, Linux, 以及 Web 端。

## 特性

- Web 平台能力适配，支持 libpag 全能力

- 基于 WebAssembly

## 快速开始

可以用 `locateFile` 函数返回 `.wasm` 文件的路径，默认为 libpag.js 文件同目录下。

### Browser（推荐）

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

### EsModule

```bash
$ npm i libpag
```

```js
import { PAGInit } from 'libpag';

PAGInit({
  locateFile: (file) => './node_modules/libpag/lib/' + file,
}).then((PAG) => {
  const url = 'https://pag.io/file/like.pag';
  fetch(url)
    .then((response) => response.blob())
    .then(async (blob) => {
      const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
      // Do Something.
    });
});
```

ESModule 引入的方式需要打包构建的时候，需要把 node_modules 下的 libpag/lib 中的 libpag.wasm 文件打包到最终产物中。并使用 `locateFile` 函数指向 libpag.wasm 文件

### PAG Demo

```javascript
// <canvas class="canvas" id="pag"></canvas>
const pagFile = await PAG.PAGFile.load(file);
document.getElementById('pag').width = pagFile.width();
document.getElementById('pag').height = pagFile.height();
const pagView = await PAG.PAGView.init(pagFile, '#pag');
pagView.setRepeatCount(0);
await pagView.play();
```

npm package 中提供了多种构建产物，可以阅读 [这里](./doc/develop-install.md) 了解不同目录下产物的差别。

demo 文件夹中提供了简单的接入示例， 可以点击 [这里](https://github.com/libpag/pag-web) 查看。

更多的 API 接口可以阅读 [API 文档](https://pag.io/api.html#/apis/web/)。

## 浏览器兼容性

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome for Android | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari on iOS |
| ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| Chrome >= 69                                                 | Safari >= 11.3                                               | Android >= 7.0                                               | iOS >= 11.3                                                  |

更多版本的兼容工作正在进行中

**因受到微信浏览器“用户与页面交互之后才可以使用 Video 标签进行视频播放”规则的限制，PAG Web SDK无法在微信浏览器下自动播放带有视频序列帧的PAG动画，建议设计师使用矢量导出。计划后续版本中提供一个解码器注入的接口，以及对应的h264解码器插件去解决这个问题。**

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

启动测试 HTTP 服务

```bash
$ npm run server
```

启动 cypress 测试

```bash
$ npm run test
```
