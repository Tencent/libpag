<img src="https://pag.io/img/readme/logo.png" alt="PAG Logo" width="474"/>

English | [简体中文](./README.zh_CN.md) | [Homepage](https://pag.io)

> **The current version is Alpha version, some APIs is not stable enough.**
>
> **If there is any problem, please go to [Issues](https://github.com/Tencent/libpag/issues) reported, we will be fixed as soon as possible.**
>
> **More features are under development.**

## Introduction

libpag is a real-time rendering library for PAG (Portable Animated Graphics) files that renders both
vector-based and raster-based animations across most platforms, such as iOS, Android, macOS,
Windows, Linux, and Web.

## Features

- Support all libpag features on the Web environment

- Based on WebAssembly and WebGL.

## Quick start

You could use the `locateFile` function to return the path of `libpag.wasm` file, the default path is the same as `libpag.js` 's path.

### Browser (Recommend)

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

If you use ESModule to import SDK, you have to build the web program including the `libpag.wasm` file that is under node_modules folder.
Then use the `locateFile` function to return the path of the `libpag.wasm` .

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

Offer much product in the npm package after building. You could read the [doc](./doc/develop-install.md) about them.

More doc such as [demo](https://github.com/libpag/pag-web), [API](https://pag.io/api.html#/apis/web/).

## Browser

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome for Android | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari on iOS |
| ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| Chrome >= 69                                                 | Safari >= 11.3                                               | Android >= 7.0                                               | iOS >= 11.3                                                  |

More versions will be coming soon.

## Roadmap

The [roadmap](https://github.com/Tencent/libpag/wiki/PAG-Web-roadmap) doc of the PAG web SDK.

## Development

### Dependency Management

Need installed C++ deps about [libpag](https://github.com/Tencent/libpag),  [Emscripten](https://emscripten.org/docs/getting_started/downloads.html), and [Node](https://nodejs.org/).

```bash
$ npm install
```

### Debug

Execute `build.sh debug` to get `libpag.wasm` file.

```bash
# ./web/script/
$ cd script
$ chmod +x ./build.sh
$ ./build.sh debug
```

Build Typescript file.

```bash
# ./web/
$ npm run dev
```

Start HTTP server.

```bash
# ./
$ emrun --browser chrome --serve_root . --port 8081 ./web/demo/index.html
```

### release

```bash
# ./web/script
$ cd script
$ chmod +x ./build.sh
$ ./build.sh
```

### Build with CLion

Create a new profile, and use the following **CMake options**（find them under **CLion** > **Preferences** > **Build, Execution, Deployment** > **CMake**）

```
CMAKE_TOOLCHAIN_FILE=path/to/emscripten/emscripten/version/cmake/Modules/Platform/Emscripten.cmake
```

### Test

Start test HTTP server.

```bash
$ npm run server
```

Start cypress test.

```bash
$ npm run test
```
