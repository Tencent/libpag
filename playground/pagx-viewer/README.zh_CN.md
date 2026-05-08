# PAGX Viewer

[English](./README.md) | 简体中文

一个轻量级的 WebAssembly SDK，用于在浏览器中渲染和查看 PAGX 文件。

## 简介

PAGX Viewer 将 libpag 的 PAGX 渲染引擎封装为一个体积小、无依赖的 JavaScript/TypeScript SDK。
它将 C++ 渲染核心编译为 WebAssembly，并提供极简的 TypeScript API，用于加载 PAGX 文件、注册字体、
处理缩放/平移，并通过 `<canvas>` 元素驱动渲染循环。

构建完成后，SDK 由两个核心文件组成：

- `pagx-viewer.esm.js`（或 `.cjs.js` / `.umd.js` / `.min.js`）— JavaScript 胶水代码
- `pagx-viewer.wasm` — WebAssembly 二进制文件

## 快速开始

```typescript
import { PAGXInit } from 'pagx-viewer';

// 初始化 WASM 模块。
const PAGX = await PAGXInit({
  locateFile: (file) => `/lib/${file}`,
});

// 通过 canvas 元素创建视图。
const view = PAGX.PAGXView.init('#pagx-canvas');

// 可选：设置自定义背景色（默认为棋盘格图案）。
// 颜色支持 #RGB / #RGBA / #RRGGBB / #RRGGBBAA，可选的 alpha 参数（0.0 - 1.0）
// 会覆盖颜色字符串中的透明度。
view.setBackgroundColor('#ffffff');

// 注册用于文本渲染的后备字体。
view.registerFonts(fontData, emojiFontData);

// 步骤 1：解析 PAGX 数据，但不构建图层。
view.parsePAGX(pagxData);

// 步骤 2：获取文档中引用的外部文件路径（例如图片）。
const paths = view.getExternalFilePaths();

// 步骤 3：下载每个外部文件，并将其数据回填到视图中。
for (const filePath of paths) {
  const response = await fetch(baseUrl + filePath);
  const fileData = new Uint8Array(await response.arrayBuffer());
  view.loadFileData(filePath, fileData);
}

// 步骤 4：在所有外部文件加载完成后构建图层树。
view.buildLayers();

// 更新画布大小以匹配当前 canvas 的尺寸。
view.updateSize();

// 可选：当用户缩放或平移时，更新缩放比例和内容偏移。
// view.updateZoomScaleAndOffset(zoom, offsetX, offsetY);

// 启动渲染循环。
view.start();
```

如果 PAGX 文件不包含任何外部资源，可以用单个 `view.loadPAGX(pagxData)` 调用替代
`parsePAGX` + `buildLayers` 流程。完整的 API 列表请参考
[`src/ts/pagx-view.ts`](./src/ts/pagx-view.ts)。

## 构建

### 前置依赖

首先，请确保已安装项目根目录 [README.md](../../README.md#Development) 中列出的所有工具和依赖，
包括 Emscripten。

### 安装依赖

```bash
# ./playground/pagx-viewer
npm install
```

### 构建

SDK 提供了两个顶层构建命令。两者都会通过 Emscripten 将 C++ 源码编译为 WebAssembly，
并通过 Rollup 打包 TypeScript 源码；它们只在优化级别和调试信息上有所不同。

```bash
# ./playground/pagx-viewer

# Debug 构建：不压缩，保留 source map 和 DWARF 调试信息。
# 适合开发阶段使用，方便在 DevTools 中单步调试 C++ 和 TS 代码。
npm run build:debug

# Release 构建：压缩 JS、剥离 WASM 调试信息，针对生产环境优化。
# 适合将 SDK 集成到正式产品时使用。
npm run build:release
```

两个命令都会在 `lib/` 目录下生成以下产物：

| 文件 | 格式 | 用途 |
|------|------|------|
| `pagx-viewer.esm.js` | ESM | `import { PAGXInit } from 'pagx-viewer'` |
| `pagx-viewer.cjs.js` | CJS | `const { PAGXInit } = require('pagx-viewer')` |
| `pagx-viewer.umd.js` | UMD | 浏览器 `<script>` 标签引入 |
| `pagx-viewer.min.js` | UMD（已压缩） | 生产环境使用 |
| `pagx-viewer.wasm` | WebAssembly | 运行时依赖 |

如需调试 C++ 端代码，请安装
[C/C++ DevTools Support (DWARF)](https://chrome.google.com/webstore/detail/cc%20%20-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb)
Chrome 扩展，然后打开 DevTools → Settings → Experiments，勾选
"WebAssembly Debugging: Enable DWARF support"。之后即可在 DevTools 中直接为 C++ 源码设置断点。

### 构建单独的产物

如果只需要构建流水线中的某一部分（例如只重新编译 WASM，不重新打包 JS），可以分别调用各步骤：

| 命令 | 说明 |
|------|------|
| `npm run build:wasm` | 仅构建 WebAssembly 二进制（release） |
| `npm run build:wasm:debug` | 仅构建 WebAssembly 二进制（debug） |
| `npm run build:js` | 仅构建 JavaScript 产物（debug） |
| `npm run build:js:release` | 仅构建 JavaScript 产物（release） |
| `npm run build:types` | 输出 TypeScript 声明文件 |
| `npm run clean` | 清理构建产物和缓存 |

## 与 PAGX Playground 配合使用

同级的 [`pagx-playground`](../pagx-playground) 项目会直接消费 `pagx-viewer/lib` 下的构建产物。
在本目录执行 `npm run build:release` 后，切换到 playground 目录即可运行其构建/发布脚本。

## 浏览器要求

- Chrome 69+
- Firefox 79+
- Safari 15+
- Edge 79+

需要浏览器支持 WebGL2 和 WebAssembly。

## 开源协议

Apache-2.0
