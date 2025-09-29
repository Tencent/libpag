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

`The above steps integrate the single-threaded version of libpag. To integrate the multithreaded version, please refer to the following integration guide.`

## Multithreaded Integration Guide

### Multithreading Support Basics

[WebAssembly multithreaded](https://emscripten.org/docs/porting/pthreads.html) relies on browser support for [SharedArrayBuffer](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer). By leveraging `SharedArrayBuffer` and `Web Worker`, multithreaded Wasm modules can perform parallel computation to improve performance.

- **Key mechanisms**:
    - Wasm threads use shared memory via `SharedArrayBuffer` for data synchronization and communication.
    - Multiple thread instances of the same wasm module run in separate `Web Worker` execution contexts.

- **Requirements**:
    - Wasm must be compiled with threading-related flags enabled (e.g., Emscripten’s `-pthread` support).
    - The runtime environment must support `SharedArrayBuffer` and `Web Worker`.

### Cross-Origin Security Requirements

To mitigate side-channel attacks, modern browsers enforce strict environment restrictions on enabling [SharedArrayBuffer](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer). Enabling Wasm multithreading requires satisfying **Cross-Origin Isolation** conditions.

#### Required Response Headers

To enable cross-origin isolation, the server must set the following HTTP response headers for all relevant resources (HTML, wasm, JS, etc.):

| Header                         | Example Value       | Purpose                                      |
|-------------------------------|---------------------|----------------------------------------------|
| `Cross-Origin-Opener-Policy` (COOP)   | `same-origin`         | Isolates the current browsing context from cross-origin documents  |
| `Cross-Origin-Embedder-Policy` (COEP) | `require-corp`        | Restricts embedded resources to those complying with CORP or CORS policies |

For more details, see the [SharedArrayBuffer documentation](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer).

#### Notes

- **The page and related resources must be served over HTTPS (or localhost).**  
  Browsers enable cross-origin isolation and `SharedArrayBuffer` only in [secure contexts](https://developer.mozilla.org/en-US/docs/Web/Security/Secure_Contexts). Multithreading depends on this secure environment.
- Cross-origin isolation cannot be enabled on HTTP, and multithreading support will be disabled.

### Browser (Recommended)

The web-side code for integrating multithreading is the same as the single-threaded, but the server must meet the above requirements; otherwise, multithreading will fail to load.

#### Important Notes

- **Do NOT load wasm resources cross-origin directly from public CDNs (such as npm CDNs).**
  Most public npm CDNs (e.g., unpkg, jsDelivr) do not set COOP/COEP response headers by default and do not enable cross-origin isolation for all requests.
  As a result, loading wasm/js from these CDNs via cross-origin requests will prevent browsers from enabling shared memory, disabling multithreading features.
  It is strongly recommended to deploy wasm/js along with your webpage under the same origin to avoid cross-origin loading issues that disable multithreading.
- The multithreaded wasm/js packages are not published on the npm registry. You can download the web packages from the [release page](https://github.com/Tencent/libpag/releases) and deploy them yourself.

### Local Build

```bash
# ./web
npm run build:mt
```

This will generate wasm/js files in the `web/lib-mt` directory. You can then start a local HTTP server to run the demo:

```bash
# ./web
npm run server:mt
```

Open http://localhost:8081/index.html in Chrome to see the demo.

To build a debug version, follow these steps:

```bash
# ./web
npm run build:debug:mt
npm run server:mt
```

> ⚠️ In the multithreaded version, if you rename the compiled output file libpag.min.js, you must search within the libpag.min.js file for all occurrences of "libpag.min.js" and replace them with the new filename.
> Otherwise, the program will fail to run. The following shows an example of this modification:

Before modification:

```js
    // filename: libpag.min.js
    var worker = new Worker(new URL("libpag.min.js", import.meta.url), {
     type: "module",
     name: "em-pthread"
    });
```

After modification:

```js
    // filename: libpag.min.test.js
    var worker = new Worker(new URL("libpag.min.test.js", import.meta.url), {
     type: "module",
     name: "em-pthread"
    });
```

## Browser Compatibility

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome for Android | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari on iOS | QQ Browser Mobile |
| ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ----------------- |
| Chrome >= 69                                                 | Safari >= 11.3                                               | Android >= 7.0                                               | iOS >= 11.3                                                  | last 2 versions   |

**The compatibility table above only represents the compatibility for running the application. For users requiring mobile access, please refer to this article on [compatibility details](./doc/compatibility.md).**

## Usage Guide

### Garbage Collection

Since libpag is based on WebAssembly for cross-platform applications, the objects retrieved from WebAssembly are essentially C++ pointers, so JavaScript's garbage collector (GC) cannot release this memory. Therefore, when you no longer need the objects created by libpag, you must call the `destroy` method on the object to free the memory.

### Rendering

#### First Frame Rendering

`PAGView.init` will render the first frame by default. If you don't need this, you can pass `{ firstFrame: false }` as the third parameter to `PAGView.init` to disable first-frame rendering.

During first-frame rendering, if there are BMP pre-compositions in the PAG animation file (details on how to check for BMP pre-composition in PAG files will be explained later), the `VideoReader` module will be invoked.

Since the `VideoReader` module relies on `VideoElement` on the Web platform, on certain mobile scenarios where `PAGView.init` is not called within a user interaction chain, the video playback may not be allowed, preventing proper rendering.

In such cases, we recommend initializing `PAGView` early with `PAGView.init(pagFile, canvas, { firstFrame: false })` and disabling first-frame rendering, and then calling `pagView.play()` for rendering when the user interacts.

#### PAG Rendering Size

On the Web platform, the device pixel resolution differs from the CSS pixel resolution, and the ratio between them is called `devicePixelRatio`. Therefore, to display 1px in CSS pixels, the rendering size should be 1px * `devicePixelRatio`.

PAG will scale the Canvas based on its visible size on the screen, which may affect the Canvas width, height, and `style`. If you want to avoid scaling the Canvas, you can pass `{ useScale: false }` as the third parameter to `PAGView.init` to disable scaling.

#### Large PAGView Size

For high-definition rendering, `PAGView` will default to rendering at Canvas size * `devicePixelRatio`. Due to performance limitations of the device, the maximum renderable size for WebGL can vary. Rendering at too large a size may lead to a white screen.

For mobile devices, it is recommended that the actual renderable size not exceed 2560px.

#### Multiple PAGView Instances

Since the Web version of PAG is a single-threaded SDK, we do not recommend **playing multiple PAGView instances on the same screen**.

For scenarios involving multiple PAGView instances, you need to be aware that the number of active WebGL contexts in a browser is limited: 16 contexts for Chrome, 8 for Safari. Due to this limitation, it is important to use `destroy` to reclaim unused PAGView instances and remove Canvas references.

Here is a solution for special cases, though it is not recommended:

If you need to have multiple PAGView instances on the same screen in Chrome and do not require support in Safari, you can try using the `canvas2D` mode by passing `{ useCanvas2D: true }` when calling `PAGView.init`. In this mode, a single WebGL context is used as the renderer, and the frames are distributed to multiple canvas2D contexts, bypassing the WebGL context limit.

Since the performance of [`CanvasRenderingContext2D.drawImage()`](https://developer.mozilla.org/en-US/docs/Web/API/CanvasRenderingContext2D/drawImage) is poor on Safari, we do not recommend using this mode on Safari.

#### Registering a Decoder

To bypass the "Video tags can only be used after user interaction with the page" restriction, the PAG Web version provides a software decoder injection interface `PAG.registerSoftwareDecoderFactory()`.

After injecting a software decoder, PAG will schedule the decoder to decode and display the BMP pre-compositions, thus enabling auto-play functionality on some platforms.

Recommended decoder integration: https://github.com/libpag/ffavc/tree/main/web

Platforms known to have the "Video tags can only be used after user interaction" restriction include: mobile WeChat browsers, iPhone power-saving mode, and some Android browsers.

## About BMP Pre-composition

You can download [PAGViewer](https://pag.io/docs/install.html) to open PAG files, click "View" -> "Show Edit Panel". In the edit panel, you can see the number of Videos. If the number of Videos is greater than 0, it indicates that the PAG animation file contains BMP pre-compositions.


## Development

First, ensure you have installed all the tools and dependencies listed in the [README.md](../README.md#Development)
located in the project root directory.

> This section describes the development and testing process for the single-threaded version of libpag.

### Dependency Management

Next, run the following command to install the required Node modules:

```bash
# ./web
$ npm install
```

### Build (Debug)

Run the following command to compile the wasm/js.

```bash
# ./web
$ npm run build:debug:st
```

This will generate the wasm/js files in the `web/lib` directory.

Start the TypeScript compiler watcher (optional).

```bash
# ./web
$ npm run dev:st
```

Start the HTTP server.

```bash
# ./web
$ npm run server:st
```

Open Chrome and go to `http://localhost:8081/index-st.html` to see the demo.

To debug, install [C/C++ DevTools Support (DWARF)](https://chrome.google.com/webstore/detail/cc%20%20-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb). 
Then, open Chrome DevTools, go to Settings, Experiments, and check the "WebAssembly Debugging: Enable
DWARF support" option to enable SourceMap support. Now you can debug C++ files in Chrome DevTools.

### Build (Release)

```bash
# ./web
$ npm run build:st
```

### Build with CLion

Create a new build target in CLion and use the following **CMake options** (found under **CLion** > **Preferences** > **Build, Execution, Deployment** > **CMake**):

```
-DCMAKE_TOOLCHAIN_FILE=path/to/emscripten/emscripten/version/cmake/Modules/Platform/Emscripten.cmake
```

### Test

Build the release version

```bash
# ./web
$ npm run build:st
```

Start the HTTP server.

```bash
# ./web
$ npm run server:st
```

Start the cypress test.

```bash
# ./web
$ npm run test
```
