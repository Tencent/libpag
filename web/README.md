<img src="../resources/readme/logo.png" alt="PAG Logo" width="474"/>

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
document.getElementById('pag').width = await pagFile.width();
document.getElementById('pag').height = await pagFile.height();
const pagView = await PAG.PAGView.init(pagFile, '#pag');
pagView.setRepeatCount(0);
await pagView.play();
```

Offer much product in the npm package after building. You could read the [doc](./doc/develop-install.md) about them.

More doc such as [demo]((./demo/)), [API](https://pag.io/api.html#/apis/web/).

## Browser

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Chrome >= 69                                                                                                                                                                                                  | Safari >= 11.1                                                                                                                                                                                                |

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

Remove `cmake-build-debug` folder that in libpag root folder, run `build.sh debug` to build `libpag.wasm` file.

If you use CLion IDE, you cloud reload the project by `Tools->CMake->Reload CMake Project`.

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
