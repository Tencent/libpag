<img src="https://pag.io/img/readme/logo.png" alt="PAG Logo" width="474"/>

[Homepage](https://pag.io) | English | [简体中文](./README.zh_CN.md)

## Introduction

libpag is a real-time rendering library for PAG (Portable Animated Graphics) files. It supports 
rendering both vector-based and raster-based animations on various platforms, including iOS, Android,
macOS, Windows, Linux, OpenHarmony, and Web.

## Features

The PAG Web SDK is built using WebAssembly and WebGL, supporting all PAG features.

## Quick start

The PAG Web SDK includes two files: `libpag.js` and `libpag.wasm`.

### Browser (Recommend)

Use the `<script>` tag to include the library directly. The `libpag` object will be registered as a global variable.

For production use, we recommend specifying a version number for `libpag` to avoid unexpected issues from newer versions:

```html
<script src="https://cdn.jsdelivr.net/npm/libpag@4.1.8/lib/libpag.min.js"></script>
```

You can browse the files of the NPM package on the public CDN at [cdn.jsdelivr.net/npm/libpag/](https://cdn.jsdelivr.net/npm/libpag/).
You can also use the keyword `@latest` to get the latest version.

The PAG library is also available on other public CDNs that sync with NPM, such as [unpkg](https://unpkg.com/libpag@latest/lib/libpag.min.js).

```html
<canvas class="canvas" id="pag"></canvas>
<script src="https://cdn.jsdelivr.net/npm/libpag@latest/lib/libpag.min.js"></script>
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

You can use the `locateFile` function to specify the path of the `libpag.wasm` file. By default, 
the `libpag.wasm` file is located next to the `libpag.js` file. For example:

```js
const PAG = await window.libpag.PAGInit({
  locateFile: () => {
    if (location.host === 'dev.pag.io') {
      // development environment
      return 'https://dev.pag.io/file/libpag.wasm';
    } else {
      // production environment
      return 'https://pag.io/file/libpag.wasm';
    }
  },
});
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

**Note: If you are using ESModule to import the PAG Web SDK, you need to manually include the `libpag.wasm` 
file in your final web program, as common packing tools usually ignore it. Use the `locateFile` 
function to specify the path of the `libpag.wasm` file. Additionally, you need to upload the
`libpag.wasm` file to a server so users can load it from the network.**

The npm package includes various products after building. You can read the [documentation](./doc/develop-install.md) 
to learn more about them.

There is also a [repository](https://github.com/libpag/pag-web) with demos on using the PAG Web SDK 
with HTML, Vue, React, and PixiJS.

You can find the API documentation [here](https://pag.io/docs/apis-web.html).

## Browser

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome for Android | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari on iOS | QQ Browser Mobile |
| ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ----------------- |
| Chrome >= 69                                                 | Safari >= 11.3                                               | Android >= 7.0                                               | iOS >= 11.3                                                  | last 2 versions   |


## Development

First, ensure you have installed all the tools and dependencies listed in the [README.md](../README.md#Development)
located in the project root directory.

### Dependency Management

Next, run the following command to install the required Node modules:

```bash
$ npm install
```

### Build (Debug)

Execute `build.sh debug` to get `libpag.wasm` file.

```bash
# ./web
$ npm run build:debug
```

Start the TypeScript compiler watcher (optional).

```bash
# ./web
$ npm run dev
```

Start the HTTP server.

```bash
# ./
$ npm run server
```

Open Chrome and go to `http://localhost:8081/demo/index.html` to see the demo.

To debug, install [C/C++ DevTools Support (DWARF)](https://chrome.google.com/webstore/detail/cc%20%20-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb). 
Then, open Chrome DevTools, go to Settings, Experiments, and check the "WebAssembly Debugging: Enable
DWARF support" option to enable SourceMap support. Now you can debug C++ files in Chrome DevTools.

### Build (Release)

```bash
# ./web
$ npm run build
```

### Build with CLion

Create a new build target in CLion and use the following **CMake options** (found under **CLion** > **Preferences** > **Build, Execution, Deployment** > **CMake**):

```
-DCMAKE_TOOLCHAIN_FILE=path/to/emscripten/emscripten/version/cmake/Modules/Platform/Emscripten.cmake
```

### Test

Build the release version

```bash
$ npm run build
```

Start the HTTP server.

```bash
$ npm run server
```

Start the cypress test.

```bash
$ npm run test
```
