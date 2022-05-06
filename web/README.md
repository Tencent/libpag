<img src="https://pag.io/img/readme/logo.png" alt="PAG Logo" width="474"/>

English | [简体中文](./README.zh_CN.md) | [Homepage](https://pag.io)

## Introduction

libpag is a real-time rendering library for PAG (Portable Animated Graphics) files that renders both
vector-based and raster-based animations across most platforms, such as iOS, Android, macOS,
Windows, Linux, and Web. 

## Features

PAG Web SDK is built on WebAssembly and WebGL which supports all of the PAG features.

## Quick start

PAG Web SDK consists of two files: `libpag.js` and `libpag.wasm`.

You can use the `locateFile` function to get the path of `libpag.wasm` file. By default, the `libpag.wasm` file is located next to the `libpag.js` file.

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

You can use the `locateFile` function to get the path of `libpag.wasm` file. By default, the `libpag.wasm` file is located next to the `libpag.js` file. For example:

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

**Note: If you are using ESModule to import PAG Web SDK, you must pack the `libpag.wasm` file manually into the final web program, because the common packing tools usually ignore the wasm file. Then use the `locateFile` function to get the path of the `libpag.wasm` . You also need to upload the `libpag.wasm` file to a server so that users can load it from network.**

There are many kinds of products in the npm package after building. You could read the [doc](./doc/develop-install.md) to know about them.

There is also a [repository](https://github.com/libpag/pag-web) that contains some demos about using PAG Web SDK with HTML / Vue / React / PixiJS.

You can find the API documentation [here](https://pag.io/docs/apis-web.html).

## Browser

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome for Android | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari on iOS |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Chrome >= 69                                                                                                                                                                                                  | Safari >= 11.3                                                                                                                                                                                                | Android >= 7.0                                                                                                                                                                                                            | iOS >= 11.3                                                                                                                                                                                                          |

More compatible versions are coming soon.

## Roadmap

Please visit [here](https://github.com/Tencent/libpag/wiki/PAG-Web-roadmap) to see the roadmap of the PAG Web SDK.

## Development

First, make sure you have installed all the tools and dependencies listed in the [README.md](../README.md#Development) located in the project root directory.

### Dependency Management

Then run the following command to install required node modules:

```bash
$ npm install
```

### Build (Debug)

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

### Build (Release)

```bash
# ./web/script
$ cd script
$ chmod +x ./build.sh
$ ./build.sh
```

### Build with CLion

Create a new build target in CLion, and use the following **CMake options**（find them under **CLion** > **Preferences** > **Build, Execution, Deployment** > **CMake**）

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
