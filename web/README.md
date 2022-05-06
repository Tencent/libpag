<img src="https://pag.io/img/readme/logo.png" alt="PAG Logo" width="474"/>

English | [简体中文](./README.zh_CN.md) | [Homepage](https://pag.io)

## Introduction

libpag is a real-time rendering library for PAG (Portable Animated Graphics) files that renders both
vector-based and raster-based animations across most platforms, such as iOS, Android, macOS,
Windows, Linux, and Web.

## Features

- Support all libpag features on the Web environment

- Based on WebAssembly and WebGL.

## Quick start

PAG Web SDK has libpag.js and libpag.wasm.

You could use the `locateFile` function to return the path of `libpag.wasm` file, the default path is the same as `libpag.js` 's path.

### Browser (Recommend)

```html
<canvas class="canvas" id="pag"></canvas>
<script src="https://unpkg.com/libpag@latest/lib/libpag.min.js"></script>
<script>
  window.onload = async () => {
    // Initialize pag webassembly module.
    const PAG = await window.libpag.PAGInit();
    // Fetch pag file data.
    const buffer = await fetch('https://pag.io/file/like.pag').then((response) => response.arrayBuffer());
    // Load the PAGFile from data.
    const pagFile = await PAG.PAGFile.load(buffer);
    // Set canvas size from the PAGFile size.
    const canvas = document.getElementById('pag');
    canvas.width = pagFile.width();
    canvas.height = pagFile.height();
    // Create PAGView.
    const pagView = await PAG.PAGView.init(pagFile, canvas);
    // Play PAGView.
    await pagView.play();
  };
</script>
```

You could use the `locateFile` function to return the path of `libpag.wasm` file, the default path is the same as `libpag.js` 's path. Like:

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
  // Initialize pag webassembly module.
});
```

**If you use ESModule to import SDK, you have to build the web program including the libpag/lib/libpag.wasm file that is under node_modules folder.Then use the `locateFile` function to return the path of the `libpag.wasm` . You need to submit the libpag.wasm that user can load from network.**

Offer much product in the npm package after building. You could read the [doc](./doc/develop-install.md) about them.

That has a [repository](https://github.com/libpag/pag-web) that has some demo about HTML / Vue / React / PixiJS.

And you find the required interface in the [API documentation](https://pag.io/docs/apis-web.html).

## Browser

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome for Android | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari on iOS |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Chrome >= 69                                                                                                                                                                                                  | Safari >= 11.3                                                                                                                                                                                                | Android >= 7.0                                                                                                                                                                                                            | iOS >= 11.3                                                                                                                                                                                                          |

More versions will be coming soon.

## Roadmap

The [roadmap](https://github.com/Tencent/libpag/wiki/PAG-Web-roadmap) doc of the PAG web SDK.

## Development

### Dependency Management

Need installed C++ deps about [libpag](https://github.com/Tencent/libpag), [Emscripten](https://emscripten.org/docs/getting_started/downloads.html), and [Node](https://nodejs.org/).

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

Build release version

```bash
$ cd script & ./build.sh
```

Start test HTTP server.

```bash
$ npm run server
```

Start cypress test.

```bash
$ npm run test
```
