# @tencent/pagx-viewer-miniprogram

微信小程序 PAGX 查看器 SDK，提供在微信小程序中渲染 PAGX 动画文件的能力。基于 TGFX 和 WebAssembly 实现高性能 WebGL 渲染。

> 当前版本：**v1.6**

## 目录结构

```
pagx-viewer-miniprogram/
├── package.json
├── lib/
│   ├── pagx-viewer.js        # PAGX 核心库（含 WASM 绑定）
│   └── pagx-viewer.wasm.br   # 压缩后的 WASM 模块（~954KB）
├── types/                    # TypeScript 类型定义
│   ├── pagx/wechat/ts/       # PAGX 核心类型
│   └── third_party/tgfx/     # TGFX 图形库类型
├── CHANGELOG.md
└── README.md
```

## 环境要求

- 微信开发者工具
- 微信小程序基础库 ≥ **2.24.4**
- 支持 WebGL 的真机或模拟器
- 微信小程序需使用 `WXWebAssembly`（非标准 `WebAssembly`）
- 必须使用**单线程**版本 WASM（微信不支持 `SharedArrayBuffer`）

## 安装使用

### 1. 引入 SDK

将 `lib/` 目录复制到小程序项目的 `utils/` 目录下，或通过 npm 引入：

```json
{
  "dependencies": {
    "@tencent/pagx-viewer-miniprogram": "1.4.0"
  }
}
```

### 2. 配置合法域名

在 [微信小程序后台](https://mp.weixin.qq.com/) → 开发 → 开发管理 → 开发设置 → 服务器域名中，添加 `downloadFile` 合法域名：`https://pag.qq.com`

> 如使用三步加载含外部图片的 PAGX 文件，还需额外添加以下合法域名：
> - Emoji 图片：`https://dev-static.d.gtimg.com`
> - Hash 图片：COS 存储桶对应的域名

## 快速开始

```javascript
import { PAGXInit } from './utils/pagx-viewer';

Page({
  View: null,
  module: null,
  canvas: null,
  dpr: 2,
  fontData: null,
  emojiFontData: null,

  async onLoad() {
    this.dpr = wx.getSystemInfoSync().pixelRatio || 2;

    // 字体下载与 Viewer 初始化并行执行
    await Promise.all([
      this.loadFonts(),
      this.initializeViewer()
    ]);

    await this.loadCurrentFile();

    // 设置渲染回调并开始渲染
    this.View.setRenderCallbacks(null, () => {
      // 帧渲染完成回调
    });
    this.View.startRendering();
  },

  async initializeViewer() {
    // 初始化 WASM 模块
    this.module = await PAGXInit({
      locateFile: (file) => '/utils/' + file
    });

    // 获取 Canvas 节点
    const query = wx.createSelectorQuery();
    const canvas = await new Promise((resolve) => {
      query.select('#pagx-canvas')
        .node()
        .exec((res) => resolve(res[0].node));
    });
    this.canvas = canvas;

    // 设置 Canvas 物理像素尺寸
    const rect = await new Promise((resolve) => {
      query.select('#pagx-canvas')
        .boundingClientRect()
        .exec((res) => resolve(res[0]));
    });
    this.canvas.width = Math.floor(rect.width * this.dpr);
    this.canvas.height = Math.floor(rect.height * this.dpr);

    // 创建 View 实例
    this.View = await this.module.View.init(this.module, this.canvas, {
      autoRender: false
    });
  },

  // 从项目 CDN 下载字体文件
  async loadFonts() {
    const [fontData, emojiFontData] = await Promise.all([
      this.downloadFile('https://pag.qq.com/wx_pagx_demo/fonts/NotoSansSC-Regular.otf'),
      this.downloadFile('https://pag.qq.com/wx_pagx_demo/fonts/NotoColorEmoji.ttf')
    ]);
    this.fontData = fontData;
    this.emojiFontData = emojiFontData;
  },

  async loadCurrentFile() {
    const url = 'https://pag.qq.com/wx_pagx_demo/ColorPicker.pagx';
    const data = await this.downloadFile(url);

    // 加载 PAGX 前注册字体，只需要注册一次
    const font = this.fontData || new Uint8Array(0);
    const emoji = this.emojiFontData || new Uint8Array(0);
    this.View.registerFonts(font, emoji);

    this.View.loadPAGX(data);
  },

  downloadFile(url) {
    return new Promise((resolve, reject) => {
      wx.downloadFile({
        url,
        success: (res) => {
          const fs = wx.getFileSystemManager();
          const data = fs.readFileSync(res.tempFilePath);
          resolve(new Uint8Array(data));
        },
        fail: reject
      });
    });
  },

  onUnload() {
    if (this.View) {
      this.View.destroy();
      this.View = null;
    }
  }
});
```

### 页面模板 (WXML)

```xml
<view class="page-container">
  <canvas type="webgl" id="pagx-canvas" class="canvas">
  </canvas>
</view>
```

### 加载流程

```
[loadFonts() + PAGXInit()] 并行 → View.init() → View.registerFonts() → View.loadPAGX() → View.setRenderCallbacks() → View.startRendering()

含外部图片时：
[loadFonts() + PAGXInit()] 并行 → View.init() → View.registerFonts() → View.parsePAGX() → getExternalFilePaths() → loadFileData() × N → buildLayers() → View.setRenderCallbacks() → View.startRendering()
```

## 核心 API

### PAGXInit(options?)

初始化 PAGX 模块，加载 WASM 文件。

```javascript
const module = await PAGXInit({
  locateFile: (file) => '/utils/' + file  // 指定 WASM 文件路径
});
```

### View

| 方法 | 说明 |
|------|------|
| `View.init(module, canvas, options?)` | 创建 View 实例 |
| `View.registerFonts(fontData, emojiFontData)` | 注册字体，需在 `loadPAGX()` 前调用 |
| `View.loadPAGX(data: Uint8Array)` | 一步加载 PAGX 文件（解析+构建层树） |
| `View.parsePAGX(data: Uint8Array)` | 仅解析 PAGX 文件，不构建层树（三步加载第 1 步） |
| `View.getExternalFilePaths()` | 获取外部图片资源路径列表（三步加载第 2 步） |
| `View.loadFileData(filePath, fileData)` | 加载外部文件数据到对应 Image 节点（三步加载第 2 步） |
| `View.buildLayers()` | 构建层树，完成渲染内容准备（三步加载第 3 步） |
| `View.contentWidth()` | 获取 PAGX 内容宽度（物理像素） |
| `View.contentHeight()` | 获取 PAGX 内容高度（物理像素） |
| `View.updateZoomScaleAndOffset(zoom, offsetX, offsetY)` | 更新缩放比例和偏移量 |
| `View.onZoomEnd()` | ~~缩放结束通知~~（已弃用，内部自动检测） |
| `View.getContentTransform()` | 获取坐标转换参数，用于评论浮层定位 |
| `View.getNodePosition(nodeId)` | 通过节点 ID 查询节点相对于画布的位置 |
| `View.setBoundsOrigin(x, y)` | 手动设置 boundsOrigin，覆盖 PAGX 文件中的值 |
| `View.setRenderCallbacks(onBefore?, onAfter?)` | 设置帧渲染回调 |
| `View.startRendering()` | 开始持续渲染 |
| `View.stopRendering()` | 停止渲染 |
| `View.isFirstFrameRendered()` | 首帧是否已渲染 |
| `View.isCurrentlyRendering()` | 是否正在渲染 |
| `View.updateSize(width?, height?)` | 更新视图尺寸 |
| `View.destroy()` | 销毁实例并释放资源 |

#### View.init 的 options 参数

```typescript
interface PAGXViewOptions {
  autoRender?: boolean;       // 是否自动渲染，默认 false
  onBeforeRender?: () => void;
  onAfterRender?: () => void;
  onFirstFrame?: () => void;  // 首帧渲染回调
}
```

#### View.registerFonts 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `fontData` | `Uint8Array` | 主字体文件数据（如 NotoSansSC-Regular.otf） |
| `emojiFontData` | `Uint8Array` | Emoji 字体文件数据（如 NotoColorEmoji.ttf） |

> **注意**：`registerFonts()` 只需在首次 `loadPAGX()` 前调用一次，后续切换文件无需重复注册。

> **字体文件来源**：请自行下载字体文件后上传至项目自有 CDN，通过 `wx.downloadFile` 下载。SDK 不内置字体文件。

## 加载本地 PAGX 文件

除了从 CDN 下载，也可以加载小程序本地的 PAGX 文件：

```javascript
async loadLocalFile() {
  const fs = wx.getFileSystemManager();
  const data = fs.readFileSync('/assets/your-animation.pagx');
  this.View.loadPAGX(new Uint8Array(data));
}
```

## 切换多个 PAGX 文件

```javascript
const SAMPLE_FILES = [
  { name: 'File1', url: 'https://pag.qq.com/wx_pagx_demo/file1.pagx' },
  { name: 'File2', url: 'https://pag.qq.com/wx_pagx_demo/file2.pagx' }
];

async switchFile(index) {
  const data = await this.downloadFile(SAMPLE_FILES[index].url);
  this.View.loadPAGX(data);
}
```

## 加载含外部图片的 PAGX 文件（三步加载）

对于包含外部图片资源引用的 PAGX 文件，需要使用三步加载流程：

### getExternalFilePaths 路径类型

`View.getExternalFilePaths()` 返回的路径有两种类型：

| 前缀 | 示例 | 说明 | 下载方式 |
|------|------|------|---------|
| `emoji:` | `emoji:1F44B.png` | Emoji 图片资源 | 去掉 `emoji:` 前缀，拼接公开 CDN 地址直接下载 |
| `hash:` | `hash:51b01fc353d219abcc5ef2054abb832f6ec87001` | COS 存储的图片资源 | 去掉 `hash:` 前缀，通过 COS SDK 下载 |

- **Emoji 类型**：拼接 CDN 地址 `https://dev-static.d.gtimg.com/emoji/apple/medium/` + 文件名即可直接下载，为公开资源
- **Hash 类型**：需要集成 [COS 小程序 SDK](https://cloud.tencent.com/document/product/436/31953) 进行下载

> **参考文档**：
> - [COS 小程序 SDK 接入文档](https://cloud.tencent.com/document/product/436/31953)
> - [Cocraft COS Client 实现参考](https://git.woa.com/tencent-design/cocraft/blob/master/packages/common/frontend-common/src/services/cos-client.ts)

### 示例代码

```javascript
const EMOJI_CDN = 'https://dev-static.d.gtimg.com/emoji/apple/medium/';

async loadPAGXWithExternalImages(url) {
  const data = await this.downloadFile(url);

  // 第 1 步：解析 PAGX 文件（不构建层树）
  this.View.parsePAGX(data);

  // 第 2 步：获取外部文件路径并逐一下载加载
  const paths = this.View.getExternalFilePaths();
  for (const filePath of paths) {
    let fileData;
    if (filePath.startsWith('emoji:')) {
      // Emoji 图片：拼接公开 CDN 地址直接下载
      const fileName = filePath.slice('emoji:'.length);
      fileData = await this.downloadFile(EMOJI_CDN + fileName);
    } else if (filePath.startsWith('hash:')) {
      // Hash 图片：通过 COS SDK 下载
      const cosKey = filePath.slice('hash:'.length);
      fileData = await this.downloadFromCOS(cosKey);
    } else {
      console.warn('Unknown file path type:', filePath);
      continue;
    }
    const success = this.View.loadFileData(filePath, fileData);
    if (!success) {
      console.warn('Failed to load external file:', filePath);
    }
  }

  // 第 3 步：构建层树，完成渲染准备
  this.View.buildLayers();
}
```

> **说明**：`loadPAGX()` 等价于 `parsePAGX()` + `buildLayers()`。如果 PAGX 文件不包含外部图片引用，使用 `loadPAGX()` 一步加载即可。

### 加载流程对比

```
简单模式：  View.loadPAGX(data)                    ← 一步完成（无外部资源）
三步模式：  View.parsePAGX(data)                    ← 解析文件
           → View.getExternalFilePaths()            ← 获取外部资源列表
           → View.loadFileData(path, data) × N      ← 逐一加载资源
           → View.buildLayers()                     ← 构建层树
```

## 评论浮层坐标转换

PAGX Viewer 提供 `getContentTransform()` API，支持将 Cocraft 画布坐标系中的评论坐标转换为小程序屏幕像素位置，实现评论标记与 PAGX 内容的精确叠加显示。

### 坐标系概览

整个转换涉及三个坐标系：

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Cocraft 画布坐标系                               │
│                     （无限画布，坐标可为负数）                         │
│                                                                     │
│          boundsOrigin                                               │
│          (-900, -193)                                               │
│               ┌──────────────┐                                      │
│               │  PAGX 设计稿   │  ● 评论A (-381, 69)                │
│               │  内容          │     在设计稿右上方外侧               │
│               │  (450 × 920)  │                                     │
│               │               │                                     │
│               │          ● 评论B (-719, 301)                        │
│               │            在设计稿内部                              │
│               │               │                                     │
│               └──────────────┘                                      │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

         ↓  坐标转换（位置关系保持不变）

┌───────────────────────────────────────┐
│     小程序 Canvas 画布                  │
│  ┌─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐  │
│  │  centerOffset                    │  │
│  │  ┌──────────────┐                │  │
│  │  │ PAGX 内容     │  ● 评论A       │  │
│  │  │ (缩放+居中)   │    右上方外侧   │  │
│  │  │               │                │  │
│  │  │          ● 评论B               │  │
│  │  │           内部  │               │  │
│  │  │               │                │  │
│  │  └──────────────┘                │  │
│  └─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘  │
└───────────────────────────────────────┘
```

**关键概念：**

| 概念 | 说明 |
|------|------|
| **Cocraft 画布** | 无限大的设计画布，坐标可以为任意正负数 |
| **boundsOrigin** | PAGX 设计稿内容左上角在 Cocraft 画布中的坐标 |
| **fitScale** | PAGX 内容等比缩放到 Canvas 的缩放因子（contain 模式） |
| **centerOffset** | 缩放后居中显示产生的偏移量 |

### 转换公式

#### 阶段一：初始定位（加载文件后计算一次）

```javascript
const t = View.getContentTransform();
// t = { boundsOriginX, boundsOriginY, fitScale, centerOffsetX, centerOffsetY }
// 新版本中 boundsOriginX, boundsOriginY 保存在 pagx 文件中 
// 旧版本中 boundsOriginX, boundsOriginY 由小程序端自行获取，可忽略 getContentTransform返回的 boundsOriginX, boundsOriginY
// 对每个评论计算 Canvas 物理像素坐标（静态值，缓存后续复用）
const baseX = (cocraftX - t.boundsOriginX) * t.fitScale + t.centerOffsetX;
const baseY = (cocraftY - t.boundsOriginY) * t.fitScale + t.centerOffsetY;
```

#### 阶段二：动态定位（缩放/平移时每帧更新）

```javascript
// zoom/panOffsetX/panOffsetY 来自用户手势回调
const screenX = (baseX * zoom + panOffsetX) / dpr;
const screenY = (baseY * zoom + panOffsetY) / dpr;
```

| 参数 | 初始值 | 说明 |
|------|--------|------|
| zoom | 1 | 用户捏合缩放倍率 |
| panOffsetX | 0 | 水平平移量（物理像素） |
| panOffsetY | 0 | 垂直平移量（物理像素） |
| dpr | 设备像素比 | 物理像素 → CSS 像素的换算因子 |

> **为什么除以 dpr？** Canvas 使用物理像素坐标（确保高清渲染），而 WXML 元素使用 CSS 像素定位。

### 完整流程

```
  const t = View.getContentTransform()     ← 加载 PAGX 后调用一次
  ┌───────────────────────────────────────────────────┐
  │  t.boundsOriginX/Y   ← PAGX 内容在 Cocraft 的位置  │
  │  t.fitScale          ← 等比缩放因子                 │
  │  t.centerOffsetX/Y   ← 居中偏移                    │
  └──────────────────────────┬────────────────────────┘
                             │
  Cocraft 坐标               ▼
  (cocraftX, cocraftY)
        │
        ▼
  Canvas 物理像素       baseX = (cocraftX - t.boundsOriginX) × t.fitScale + t.centerOffsetX
  (baseX, baseY)        baseY = (cocraftY - t.boundsOriginY) × t.fitScale + t.centerOffsetY
        │                                                         ↑ 静态，缓存
        │
        ▼             screenX = (baseX × zoom + panOffsetX) / dpr
  屏幕 CSS 像素        screenY = (baseY × zoom + panOffsetY) / dpr
  (screenX, screenY)                                              ↑ 动态，每帧
        │
        ▼
  设置为评论元素的 left / top
```

### 接入示例

#### 1. 准备 Page data

```javascript
Page({
  data: {
    commentPins: [],        // [{id, baseX, baseY, text, color}]
    commentTransform: null  // {zoom, panOffsetX, panOffsetY, dpr}
  },
  dpr: 2,
})
```

#### 2. 加载 PAGX 后计算评论基准坐标

```javascript
initCommentOverlays(comments) {
  if (!this.View) return;

  const t = this.View.getContentTransform();
  const pins = comments.map(function(c) {
    return {
      id: c.id,
      text: c.text,
      color: c.color,
      baseX: (c.cocraftX - t.boundsOriginX) * t.fitScale + t.centerOffsetX,
      baseY: (c.cocraftY - t.boundsOriginY) * t.fitScale + t.centerOffsetY,
    };
  });

  this.setData({
    commentPins: pins,
    commentTransform: {
      zoom: 1, panOffsetX: 0, panOffsetY: 0, dpr: this.dpr
    }
  });
}
```

#### 3. WXS 渲染线程动态定位

创建 `comment.wxs` 在渲染线程上直接更新评论位置，避免 `setData` 延迟：

```javascript
// comment.wxs
function onTransformChanged(newVal, oldVal, ownerInstance, instance) {
  if (!newVal || !newVal.zoom) return;
  var dataset = instance.getDataset();
  var screenX = (dataset.basex * newVal.zoom + newVal.panOffsetX) / newVal.dpr;
  var screenY = (dataset.basey * newVal.zoom + newVal.panOffsetY) / newVal.dpr;
  instance.setStyle({ left: screenX + 'px', top: screenY + 'px' });
}

module.exports = { onTransformChanged: onTransformChanged };
```

#### 4. WXML 模板

```xml
<wxs src="./comment.wxs" module="commentWxs" />

<canvas id="pagx-canvas" type="webgl2" ... />

<view
  wx:for="{{commentPins}}"
  wx:key="id"
  class="comment-pin"
  data-basex="{{item.baseX}}"
  data-basey="{{item.baseY}}"
  change:prop="{{commentWxs.onTransformChanged}}"
  prop="{{commentTransform}}"
>
  <view class="comment-dot" style="background:{{item.color}};"></view>
  <view class="comment-bubble">
    <text class="comment-text">{{item.text}}</text>
  </view>
</view>
```

> **注意：** Canvas 和评论浮层需要在同一个 `position: relative` 的容器内，评论使用 `position: absolute` 定位。

#### 5. 手势回调中同步更新评论变换

```javascript
applyGestureState(state) {
  if (!state || !this.View) return;

  this.View.updateZoomScaleAndOffset(state.zoom, state.offsetX, state.offsetY);

  this.setData({
    commentTransform: {
      zoom: state.zoom,
      panOffsetX: state.offsetX,
      panOffsetY: state.offsetY,
      dpr: this.dpr
    }
  });
}
```

### View.getContentTransform() 返回值

| 字段 | 类型 | 说明 |
|------|------|------|
| `boundsOriginX` | number | PAGX 内容左上角在 Cocraft 画布中的 X 坐标 |
| `boundsOriginY` | number | PAGX 内容左上角在 Cocraft 画布中的 Y 坐标 |
| `fitScale` | number | 等比缩放因子（contain 模式） |
| `centerOffsetX` | number | 水平居中偏移（物理像素） |
| `centerOffsetY` | number | 垂直居中偏移（物理像素） |

### View.getNodePosition(nodeId)

通过节点 ID 查询节点相对于画布的位置坐标。用于获取特定节点在渲染画布中的位置，可用于交互定位、节点高亮、锚点跳转等场景。

```javascript
const result = View.getNodePosition('node-12345');
if (result.found) {
  console.log('Node position:', result.x, result.y);
} else {
  console.log('Node not found or has no position data');
}
```

#### 返回值

| 字段 | 类型 | 说明 |
|------|------|------|
| `found` | boolean | 是否找到节点且包含有效的位置数据 |
| `x` | number | 节点相对于画布的 X 坐标（未找到时为 0） |
| `y` | number | 节点相对于画布的 Y 坐标（未找到时为 0） |

### View.setBoundsOrigin(x, y)

手动设置 boundsOrigin，覆盖 PAGX 文件中的值。当 PAGX 文件未嵌入 `bounds-origin` 数据时使用。

## 常见问题

### Canvas 显示黑屏或空白

- 检查 WASM 文件路径是否正确：`locateFile: (file) => '/utils/' + file`
- 检查 PAGX 文件是否加载成功
- 检查 Canvas 尺寸是否正确设置了 DPR

### 报错：WebAssembly is not defined

微信小程序使用 `WXWebAssembly` 而非标准 `WebAssembly`，需确保使用适配微信环境的构建产物。

### WASM 加载失败

- 检查 WASM 文件是否存在
- 确保使用单线程版本（微信不支持 `SharedArrayBuffer`）

### 下载文件失败

- 检查 CDN URL 是否可访问
- 确认已在小程序后台配置 `downloadFile` 合法域名
- 确保 CDN 支持 HTTPS

### 手势操作不生效

- `View.updateZoomScaleAndOffset()` 需要传入正确的缩放比例和偏移量（物理像素）
- 确保 DPR 参数正确：`this.dpr = wx.getSystemInfoSync().pixelRatio || 2`

### 渲染模糊

需设置 Canvas 物理像素尺寸：

```javascript
this.canvas.width = Math.floor(rect.width * this.dpr);
this.canvas.height = Math.floor(rect.height * this.dpr);
```

### 内存占用过高

页面卸载时及时清理资源：

```javascript
onUnload() {
  if (this.View) { this.View.destroy(); this.View = null; }
}
```

### 为什么用 WXS 而不是 setData 更新评论位置

`setData` 是异步的（逻辑层 → 渲染层通信），在快速缩放/平移时会导致评论标记跟不上 PAGX 画面。WXS 直接运行在渲染线程上，可以同步更新 DOM 样式，保证评论标记与画面同步移动。

### 评论显示在画布外面

这是正常的。评论在 Cocraft 中可能就在设计稿外面（比如标注在旁边），转换后自然也在 PAGX 渲染区域外面。用户缩小画布后就能看到。如果不希望显示画布外的评论，可以对 `baseX`/`baseY` 做范围判断后过滤。

### boundsOriginX/Y 返回 0

说明 PAGX 文件中没有嵌入 `bounds-origin` 数据。需要：
- 确认 Cocraft 导出时已将 `bounds-origin-x` / `bounds-origin-y` 写入 PAGX 文件根节点的 `data-*` 属性
- 或者在 JS 端手动调用 `View.setBoundsOrigin(x, y)` 设置

> **版本兼容说明**：现有版本中，小程序端可自行获取 bounds-origin 参数；后续新版本中 PAGX 文件自身会携带 bounds-origin 相关参数。

### 切换 PAGX 文件后评论位置不对

每次调用 `View.loadPAGX()` 后，`getContentTransform()` 返回的参数可能不同（不同文件的 boundsOrigin、内容尺寸不同），必须重新计算所有评论的 `baseX`/`baseY`。

### Canvas 尺寸变化后评论位置不对

Canvas 尺寸变化会影响 `fitScale` 和 `centerOffset`。调用 `View.updateSize()` 后需要重新调用 `getContentTransform()` 并重新计算所有评论的 `baseX`/`baseY`。

## 技术限制

- **不支持多线程 WASM**：微信小程序不支持 `SharedArrayBuffer`，必须使用单线程版本
- **包体积限制**：单个分包不超过 2MB，总包不超过 20MB
- **域名白名单**：需在小程序后台配置 `downloadFile` 合法域名
- **内存限制**：iOS 约 300MB，Android 约 500MB
- **文件大小建议**：PAGX 文件不超过 10MB

## 调试技巧

```javascript
// 在 onLoad 中添加
console.log('DPR:', this.dpr);
console.log('Canvas size:', this.canvas.width, 'x', this.canvas.height);
console.log('Content size:', this.View.contentWidth(), 'x', this.View.contentHeight());

// 监控渲染帧率
let frameCount = 0;
let lastTime = Date.now();
this.View.setRenderCallbacks(null, () => {
  frameCount++;
  const now = Date.now();
  if (now - lastTime >= 1000) {
    console.log('FPS:', frameCount);
    frameCount = 0;
    lastTime = now;
  }
});
```

## 相关资源

- [PAGX Viewer 文档](https://pag.io/pagx/1.4/zh/)
- [PAG 官网](https://pag.io)
- [libpag GitHub](https://github.com/Tencent/libpag)
- [TGFX 项目](https://github.com/Tencent/tgfx)
- [微信小程序官方文档](https://developers.weixin.qq.com/miniprogram/dev/framework/)
- [WXWebAssembly API](https://developers.weixin.qq.com/miniprogram/dev/framework/performance/wasm.html)
