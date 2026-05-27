# Changelog

## v1.8.2

### 变更

- 版本号更新，用于在 v1.8 发布后重新发布小程序包；对外 API 与 v1.8 保持一致。

---

## v1.8

### 破坏性变更

- **删除 `View.zoomBy(scaleDelta, focalX, focalY)`**。该 API 在 v1.7 实现为
  增量式 zoom（每帧 `newZoom = currentZoom × scaleDelta`），但与真实 pinch
  手势的"基于会话起点的累计 scale"模型不匹配——连续调用时浮点累积 + focal
  不在视野中心导致的几何挤压，会让画面在缩放过程中持续平移。请改用 v1.8 新增
  的 pinchBy 系列 API。

### 新增

- `View.beginPinchGesture()` / `View.pinchBy(scale, focalX, focalY)` /
  `View.endPinchGesture()`：基于手势会话的绝对式 pinch zoom，参考 iOS
  `UIPinchGestureRecognizer` 设计。
  - **手势模式**：`beginPinchGesture()` 抓 snapshot，后续每次 `pinchBy(scale)`
    都基于 snapshot 重算 zoom/offset，无累积、无漂移；`endPinchGesture()` 清
    snapshot
  - **单次模式**：未调用 `beginPinchGesture()` 时直接调 `pinchBy(scale, x, y)`，
    基于当前 view 状态做一次绝对 zoom，等价 v1.7 的 zoomBy 单次调用语义。适用
    于鼠标滚轮、双击、按钮"+/-"等离散输入

### 迁移指南

| v1.7 写法 | v1.8 写法 |
|---|---|
| `view.zoomBy(perFrameRatio, focalX, focalY)` 每帧调用（手势）| `beginPinch → pinchBy(cumulativeScale, focalX, focalY) × N → endPinch` |
| `view.zoomBy(1.1, mouseX, mouseY)` 单次调用 | `view.pinchBy(1.1, mouseX, mouseY)`（无 begin/end） |

`updateZoomScaleAndOffset / panBy / setPadding / getViewportInfo` 完全不变。

---

## v1.7

### 新增

- 新增 `View.setPadding(logicalPx)` API：配置单 Frame 预览的单边预留留白（逻辑像素，默认 0）
  - 影响 `panBy` / `zoomBy` 内部的 pan 上限与 zoom 下界，实现"文档加 padding 不会缩到比 canvas 还小"
  - 默认值 0 保持与历史版本兼容，不调用此接口的业务方行为不变
- 新增 `View.panBy(deltaX, deltaY)` API：增量 pan + 自动 clamp + 越界报告
  - 入参为 canvas 物理像素增量
  - 返回 `{ remainingX, remainingY }` —— 未消耗的 pan 量。非零意味着方向越界，业务层据此触发翻页 / 弹性反馈等 UI
  - `panBy(0, 0)` 是合法的"重新 clamp"调用，可在外部直接修改 zoom 后用来收回越界的 offset
- 新增 `View.zoomBy(scaleDelta, focalX, focalY)` API：增量 zoom + 锚点偏移 + 自动 clamp
  - 内部按 zoom-around-point 公式更新 offset，保证捏合中心稳定
  - 自动应用 zoom 下界（基于 `setPadding` 配置）：缩到"文档+padding"等于 canvas 时为底
  - zoom 完成后再次 clamp offset，避免 zoom 改变后 offset 越界
- 新增 `View.getViewportInfo()` API：一次性获取所有视口状态
  - 返回字段：`{ zoom, offsetX, offsetY, canvasWidth, canvasHeight, contentWidth, contentHeight, fitScale, atLeft, atRight, atTop, atBottom }`
  - `atLeft/atRight/atTop/atBottom` 表示文档边缘是否已贴到 canvas 边缘（0.5px 容差）
  - 替代多次调用 `contentWidth()` / `contentHeight()` / `getContentTransform()` 等

### 变更

- `View.updateZoomScaleAndOffset(zoom, offsetX, offsetY)` 仍保留，但内部会同步更新 SDK 缓存的 zoom/offset
- 推荐手势驱动的 viewport 修改改用 `panBy` / `zoomBy`，由 SDK 统一处理 clamp 策略；只有需要绝对跳转或自带 clamp 的调用方才直接用 `updateZoomScaleAndOffset`

### 兼容性

- 完全向后兼容：未调用 `setPadding` 的业务方，所有行为与 v1.6 一致
- 业务方可以渐进迁移：原有 `updateZoomScaleAndOffset` 调用无需修改

---

## v1.6

### 修复

- 解决加载部分 pagx 文件时 PAGXView 初始化失败的问题

---

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
