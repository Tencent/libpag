# WeChat PAGXView → PAGScene 实现计划

> 日期：2026-07-09
> 依据：本目录 `PAGScene_ADAPTATION.md`（已评审确认）
> 方案：采用**方案A**——将 bounds 迁移合并进 Phase 1，直接拆掉 `contentLayer`/`builderSession` 双树，
> 原三阶段压缩为**两阶段**（Phase 1 = 渲染管线 + bounds + 拆旧树；Phase 2 = 动画）。
> 脏标记策略：暂时去掉空闲短路 / fit 快路，**每帧全量渲染**（先跑通，性能后续再优化）。

---

## 方案A 决策记录（相对原设计的偏离）

原设计的「双树共存、旧树继续供 bounds」前提在实现时被证伪：移除 `displayList` 后，`buildLayers` 不再执行
`displayList.root()->addChild(contentLayer)`，`contentLayer->root()` 恒为 `nullptr`，导致依赖它的
`computeFullPathBounds` / JS `getImageBounds` 全部返回空；且 `draw()` 空闲短路依赖的
`displayList.hasContentChanged()` 在 PAGScene 侧无等价脏标记。据此确认的两点调整：

1. **直接切 `scene->getImageBounds()`**，viewport 计算改读 `PAGDisplayOptions`，一步拆掉双树（不再有过渡期）。
2. **去掉空闲短路 / fit 快路**，`draw()` 每帧全量渲染。`setGestureActive` / `setSnapshotEnabled` /
   `resetForFreshCapture` 及相关快照成员**保留**（binding.cpp 注册、JS 依赖、编译需要），但 `draw()` 不再走快路；
   `fitSnapshot` 恒为 `null`，使 `setGestureActive` 自然 no-op。快路/脏标记留作后续性能 pass。

---

## 实施前已核实的接口事实

| 事项 | 结论 |
|------|------|
| `PAGScene` 成员 `_rootComposition` / `document` | 存在（`PAGScene.h:213-214`），§6 实现思路可直接落地 |
| friend 关系 | `PAGScene` 是 `PAGXDocument` friend（可访问 private `findLayersByImageFilePath`）；`PAGScene` 已 `friend class PAGLayer`，且 `PAGLayer` 声明 `friend class PAGScene`（可访问 protected `node/runtimeLayer/children`）。无需改可见性 |
| `PAGDisplayOptions` setter/getter | `setRenderMode/setTileUpdateMode/setMaxTileCount/setMaxTilesRefinedPerFrame/setSubtreeCacheMaxSize/setBackgroundColor/setZoomScale/setContentOffset`、`getZoomScale/getContentOffset` 均存在 |
| `getContentOffset()` 返回类型 | `pagx::Point`（非 `tgfx::Point`），保存用 `Point savedOffset` |
| `Record` friend 签名 | `Record(tgfx::Context*, const shared_ptr<PAGScene>&, const shared_ptr<PAGSurface>&, bool autoClear)`，内部 flush 但不 submit/present，返回 `Recording` 由调用方 submit |
| `PAGSurface::MakeFrom(shared_ptr<tgfx::Surface>)` | `Drawable::getSurface(context)` 返回**同一** tgfx::Surface（带 context-match 保护），故手动背景 + `Record(autoClear=false)` 单次 flush 按序合并，与旧 `displayList.render(surface,false)` 等价 |
| `pagx::Color{}` 默认值 | 默认 `alpha=1`（不透明黑），透明须显式 `{0,0,0,0}`；tgfx DisplayList 背景默认已是 Transparent |
| `root runtimeLayer` 矩阵 | `LayerBuilderContext::build` root 由 `tgfx::Layer::Make()` 创建，仅 `setScrollRect` 裁剪，无 `setMatrix`，恒为单位矩阵；§5.7b 坐标系自洽假设成立 |
| 是否复用 `getGlobalBounds` | 否。`getGlobalBounds` 返回 **surface 空间**（含 zoom/offset），不满足内部 LRU 需要的「文档原始空间 bounds」，故新增 `getImageBounds` 属必要，不重复 |

---

## Phase 1：渲染管线 + bounds + 拆旧树（方案A 合并）

> 目标：主渲染走 PAGScene，bounds 查询走 `scene->getImageBounds()`，一步移除 `contentLayer`/`builderSession`。
> 单树、无过渡期；每帧全量渲染。

### 已完成改动

**A. `PAGScene::getImageBounds`（核心库新增 API）**
- `include/pagx/PAGScene.h`：public 区新增 `std::vector<Rect> getImageBounds(const std::string& filePath) const;`（含详细注释：root 空间 / 未含 display zoom-offset / 供 host texture cache viewport-eviction 打分）。
- `src/pagx/PAGScene.cpp`：`document->findLayersByImageFilePath(filePath)` 取 `const Layer*` 集合 → 以 `_rootComposition->runtimeLayer.get()` 为参考系，非递归栈遍历 `_rootComposition` 全树，对 `child->node ∈ targetSet` 者调 `runtimeLayer->getBounds(rootLayer)`，收集非空 bounds（`FromTGFX` 转 `pagx::Rect`）。

**B. `PAGXView.h` 成员/include 增删**
- include：移除 `tgfx/layers/DisplayList.h`、`LayerBuilder.h`；新增 `tgfx/core/Rect.h`、`tgfx/core/Surface.h`、`tgfx/gpu/Device.h`、`pagx/PAGScene.h`、`pagx/PAGSurface.h`。
  （注：`tgfx::Device`/`tgfx::Surface` 原经 DisplayList/LayerBuilder 间接引入，移除后须显式补回。）
- 移除：`tgfx::DisplayList displayList`、`shared_ptr<tgfx::Layer> contentLayer`、`unique_ptr<LayerBuilderSession> builderSession`。
- 新增：`shared_ptr<PAGScene> scene`、`shared_ptr<PAGTimeline> defaultTimeline`、`shared_ptr<PAGSurface> pagSurface`、`float userZoom{1.0f}`、`float userOffsetX{0}`、`float userOffsetY{0}`。
- 保留（快照 API scaffolding）：`gestureActive` / `fitSnapshot` / `fitSnapshotPixelScale` / `zoomedOutFrameSettled` / `snapshotEnabled` / `lastGestureEndMs`，及 `setGestureActive` / `setSnapshotEnabled` / `resetForFreshCapture`。

**C. `buildLayers()`**
- `ResolveAllImagePatternMatrices` → `document->applyLayout(&fontConfig)`（先于 `Make`，规避风险 #1 fontConfig 丢失）→ `scene = PAGScene::Make(document)`（判空 return）→ `defaultTimeline = scene->getDefaultTimeline()`。
- `getDisplayOptions()` 配置 Tiled / Fast / maxTileCount / maxTilesRefinedPerFrame / subtreeCacheMaxSize，并 `setBackgroundColor({0,0,0,0})` 显式透明（风险 #8）。
- `applyMergedTransform()` 替代旧 `applyCenteringTransform`。

**D. `parsePAGX()` / `updateSize()`**
- `parsePAGX`：`scene / defaultTimeline / pagSurface / document = nullptr` + `userZoom=1 / userOffset=0`。
- `updateSize`：`surface / pagSurface / fitSnapshot = nullptr` 后 `if(scene) applyMergedTransform()`。

**E. 主渲染 `draw()`（每帧全量渲染）**
- 移除空闲短路块、fit blit 快路、`dirty` 门控、离屏 capture 块。
- 每帧：`lockContext` → `flushPendingUploads` → surface 创建/重建时同步 `pagSurface = pagx::MakeFrom(surface)` → `canvas->clear()` + `DrawSolidBackground/DrawBackground` → `recording = (scene && pagSurface) ? Record(context, scene, pagSurface, false) : context->flush()` → GPU probe → `submit`（`hasRenderedFirstFrame` 仅在 `scene != nullptr` 时置 true）→ `drainPendingTextureDeletes` → `unlock`。
- 移除 `flushMs` 计时；slow-frame / first-frame log 改从 `scene->getDisplayOptions()` 读 zoom/offset。

**F. 变换合并 `applyMergedTransform()`（两级变换合一）**
- `effectiveZoom = fitScale·userZoom`、`effectiveOffsetX = centerOffset·userZoom + userOffset` → 写入 `getDisplayOptions()->setZoomScale/setContentOffset`。
- `computeFitScale()` / `getContentTransform()` 保持纯计算，不依赖 scene（JS cocraft 坐标映射需逻辑 fitScale）。

**G. 手势/tile refinement 走 options**
- `updateZoomScaleAndOffset()`：subtreeCache 动态调整走 `scene->getDisplayOptions()`（判 scene）；末尾存 `userZoom/userOffset` 后 `applyMergedTransform()`。
- `onZoomEnd()` / `updateAdaptiveTileRefinement()` / tryUpgrade 块：`setMaxTilesRefinedPerFrame` 走 options（判 scene）。

**H. bounds 迁移（方案A 一步到位）**
- `computeFullPathBounds()`：`scene->getImageBounds(path)` union（`pagx::Rect` → `tgfx::Rect::MakeXYWH`）。
- JS `getImageBounds()`：`fitMatrix = Matrix::MakeTrans(centerOffset).preScale(fitScale)`，对每 filePath `scene->getImageBounds` → `fitMatrix.mapRect` → union/largest → `RectToJSObject`（还原到画布像素空间、不含 userZoom/userOffset，符合 `types.ts` 契约）。
- `computeViewportInRootCoords()`：读 `scene->getDisplayOptions()->getZoomScale()/getContentOffset()`（判 scene），反算 `doc=(canvas-effOffset)/effZoom` + 1.2x 扩展；与 bounds 同在文档原始空间（风险 #7 闭合）。
- 全局搜索 `displayList` / `contentLayer` / `builderSession` 清残留（仅历史说明性注释保留）。

### Phase 1 编译验证

核心库（`getImageBounds` 等）：**已通过**（`PAGFullTest` 891/891 链接成功）。
```bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest
```

WeChat WASM 端（`PAGXView.cpp` / `PAGXView.h` / `binding.cpp`）：
```bash
cd /Users/billyjin/Desktop/project/libpag-main/pagx/wechat && npm run build:release
```

人工验证：渲染 / 手势缩放 / 图片渐进加载 / LRU 淘汰均正常。通过后提交。

---

## Phase 2：动画支持（按需）

### 任务
- 合适时机（如 raf 帧回调）调 `defaultTimeline->advanceAndApply(deltaMicroseconds)`；**调用前判空**（无 top-level 动画时 `getDefaultTimeline()` 返回 `nullptr`）。

**Phase 2 验证**：编译通过 + 动画帧驱动正常。通过后提交。

---

## 后续性能 pass（非阻塞）

- 依据风险 #3/#4/#9 真机埋点，按需补回：空闲短路 / fit 快路 / PAGLayer 级 bounds 缓存 / 图片变更增量刷新。

---

## 需实测收口的风险

| # | 风险 | 收口方式 |
|---|------|---------|
| 3 | `getImageBounds` 全树遍历 + `findLayersByImageFilePath` 索引高频失效 | 真机埋点对比耗时；若不达标加 PAGLayer 级缓存（`buildRuntimeTree` 时建、`onNodesChanged` 时失效） |
| 4 | PAGScene 图片刷新（子树重建）比旧方案（原地更新 vector contents）开销更大 | 真机小程序实测帧时，必要时加图片变更增量刷新路径 |
| 9 | `onNodesChanged` foreign 分支全树重建 | 加断言/日志确认 Cocraft 图片刷新走常规 ImagePattern 分支、不误落 foreign 全树重建 |
