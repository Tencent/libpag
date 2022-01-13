# libpag Web

> **当前版本为 Alpha 版本，更多特性持续开发中**

## 特性

- 支持播放矢量、位图、视频序列帧 PAG 文件

- 基于 WebAssembly

## 快速开始

可以用 locateFile 函数返回 wasm 文件的路径，默认为 libpag.js 同目录下。

### EsModule

```bash
$ npm i libpag
```

```js
import { PAGInit } from 'libpag';

PAGInit({
  locateFile: (file) => './node_modules/libpag/lib/' + file,
}).then((PAG) => {
  fetch('https://pag.io/file/particle_video.pag')
    .then((response) => response.blob())
    .then(async (blob) => {
      const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
      // Do Something.
    });
});
```

### Browser

```html
<div>
  <canvas class="canvas" id="pag"></canvas>
</div>
<script src="./node_modules/libpag/lib/libpag.umd.js"></script>
<script>
  window.libpag
    .PAGInit({
      locateFile: (file) => './node_modules/libpag/lib/' + file,
    })
    .then((PAG) => {
      fetch('https://pag.io/file/particle_video.pag')
        .then((response) => response.blob())
        .then(async (blob) => {
          const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
          // Do Something.
        });
    });
</script>
```

### PAG Demo

```javascript
// <canvas class="canvas" id="pag"></canvas>
const pagFile = await PAG.PAGFile.load(file);
document.getElementById('pag').width = await pagFile.width();
document.getElementById('pag').height = await pagFile.height();
const pagView = await PAG.PAGView.init(pagFile, '#pag');
pagView.setRepeatCount(0);
await pagView.play();
```

npm package 中提供了多种构建产物，可以阅读 [这里](./doc/develop-install.md) 了解不同目录下产物的差别。

更多的API接口可以阅读 [API文档](./doc/api.md)。

## 浏览器兼容性

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari |
| ------------------------------------------------------------ | ------------------------------------------------------------ |
| Chrome >= 87                                                 | Safari >= 11.1                                               |

## 前置工作

需要确保已经可编译 C++ libpag 库，并且安装 [Emscripten 套件](https://emscripten.org/docs/getting_started/downloads.html) 和 Node 依赖

```bash
// 安装Node依赖
$ npm install
```

## 生产流程

执行 `build.sh` 脚本

```bash
// web/script目录下
$ cd script
// 添加执行权限
$ chmod +x ./build.sh
// 打包
$ ./build.sh
```

## 开发流程

删除项目根目录的 `cmake-build-debug`，执行 `build.sh debug` 打包 C++ 代码，每次改动 C++ 代码都需要重新打包新的 `libpag.wasm` 文件，执行完成之后可以通过 `Tools->CMake->Reload CMake Project` 刷新项目

```bash
// web/script目录下
$ cd script
// 添加执行权限
$ chmod +x ./build.sh
// 打包
$ ./build.sh debug
```

打包 Typescript 文件，修改 Typescript 文件会自动打包到 Javascript 文件

```bash
// web目录下
$ npm run dev
```

启动 HTTP 服务

```bash
// libpag根目录下
$ emrun --browser chrome --serve_root . --port 8081 ./web/demo/index.html
```
