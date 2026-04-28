# Changelog

## v1.4

### 新增

- 新增 `View.getNodePosition(nodeId)` API，支持通过节点 ID 查询节点相对于画布的位置坐标
  - 返回值：`{ found: boolean, x: number, y: number }`
  - 用于获取 PAGX 文件中特定节点的位置信息，可用于交互定位、节点高亮等场景
- 新增 `NodePosition` 类型定义

### 优化

- 优化渲染性能

### 变更

- WASM 文件大小从 ~941KB 增长至 ~954KB

---

## v1.3

> 本次更新为内部渲染模块优化，**对外 API 无任何变化，调用方无需修改代码**，仅需更新 WASM 文件即可。

### 内部优化

- 新增内部模块 `ImagePatternMatrixCalculator`，支持 CoCraft 导出的 PAGX 文件中 ImagePattern 图案填充的变换矩阵自动计算（STRETCH / FIT / FILL / TILE 四种缩放模式）
- 在 `buildLayers()` 阶段自动完成矩阵解算，对调用方完全透明

### 变更

- WASM 文件大小从 ~937KB 增长至 ~941KB（+3.76KB）

---

## v1.2

### 新增

- 新增三步加载流程，支持含外部图片资源的 PAGX 文件：
  - `View.parsePAGX(data)` — 仅解析 PAGX 文件，不构建层树
  - `View.getExternalFilePaths()` — 获取外部文件引用路径列表，根据 path 自行下载图片
  - `View.loadFileData(filePath, fileData)` — 加载外部文件数据到对应 Image 节点
  - `View.buildLayers()` — 构建层树，完成渲染内容准备
- 新增 `View.getContentTransform()` API，返回坐标转换参数（`boundsOriginX`、`boundsOriginY`、`fitScale`、`centerOffsetX`、`centerOffsetY`），用于评论浮层定位
- 新增 `View.setBoundsOrigin(x, y)` API，手动设置 boundsOrigin，覆盖 PAGX 文件中的值
- 新增 `ContentTransform` 类型定义

### 变更

- `View.onZoomEnd()` 标记为已弃用（`@deprecated`），内部已自动检测缩放结束
- WASM 文件大小从 ~746KB 增长至 ~937KB

---

## v1.1

### 新增

- 新增 `View.registerFonts(fontData, emojiFontData)` API，支持注册自定义字体和 Emoji 字体
- 新增 `View.isFirstFrameRendered()` API，查询首帧是否已渲染
- `PAGXViewOptions` 新增 `onFirstFrame` 回调，在首帧渲染完成时触发

### 变更

- 新增 `View.registerFonts()` 要在 `View.loadPAGX()` 前注册自定义字体
- CDN 域名从 `pag.io` 变更为 `pag.qq.com`
- WASM 文件大小从 ~470KB 增长至 ~746KB

---

## v1.0

首个发布版本。

### 功能

- 支持在微信小程序中加载和渲染 PAGX 文件
- 基于 WebAssembly + WebGL 的高性能渲染引擎
- 提供 `PAGXInit` 初始化接口，支持自定义 WASM 文件路径
- 提供 `View` 类，支持以下能力：
  - 加载 PAGX 文件数据（`loadPAGX`）
  - 获取内容尺寸（`contentWidth` / `contentHeight`）
  - 缩放与偏移控制（`updateZoomScaleAndOffset` / `onZoomEnd`）
  - 渲染控制（`startRendering` / `stopRendering` / `isCurrentlyRendering`）
  - 渲染回调（`setRenderCallbacks`）
  - 视图尺寸更新（`updateSize`）
  - 资源销毁（`destroy`）
- WASM 文件采用 Brotli 压缩（~470KB）
