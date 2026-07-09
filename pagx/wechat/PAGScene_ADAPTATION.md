# WeChat PAGXView PAGScene 适配方案

> 日期：2026-07-09
> 原则：以最简洁的方式使用 PAGScene，不照搬旧方案的逻辑

---

## 一、PAGScene 给 WeChat 带来的简化

| 旧方案 | 新方案 |
|--------|--------|
| `builderSession->build()` → `contentLayer` | `PAGScene::Make(document)` — 层树构建和动画框架一站式完成 |
| `displayList.render(surface)` | `Record(context, scene, pagSurface)` — PAGScene 管好自己的显示列表 |
| `contentLayer->setMatrix()` + `displayList.setZoomScale/Offset` 两级变换 | `PAGDisplayOptions` 一级合并变换 |
| `displayList.setRenderMode/MaxTileCount/SubtreeCache...` | 统一走 `PAGDisplayOptions` |
| `builderSession->rebuildForFilePath()` 原地更新图片 | PAGScene 自动通过 `onNodesChanged` 响应 `document->loadFileData()` |
| 手动维护 `filePath→tgfx::Layer[]` 映射表 | **不需要** — PAGScene 的运行时树本身就是映射，图片更新自动扩散 |

**WeChat 端需要保留的高级功能**（PAGScene 不关心的外部层）：
- 渐进式图片加载：JS 解码 → `attachNativeImage()` → `flushPendingUploads()` → WebGL texture → `loadFileData(filePath, PAGImage)`
- 纹理 LRU 淘汰：多级字节预算 + viewport 距离排序
- 手势冻结：`fitSnapshot` 快照 + blit 路径
- JS 回调：`onTextureRequest` / `onTextureEvict`
- JS API：`getImageBounds` / `getImageMetadata` / `getContentTransform` / `getNodePosition`
- 性能监控 + adaptive tile refinement

---

## 二、渲染流程对比

### 改造前

```
draw()
  ├─ 短路径短路 → return
  ├─ gesture blit → return
  ├─ dirty 分支:
  │   ├─ lockContext
  │   ├─ flushPendingUploads            ← 上传 WebGL texture
  │   ├─ canvas->clear + DrawBackground
  │   ├─ displayList.render(surface)    ← 旧渲染
  │   ├─ flush → submit
  │   ├─ capture fitSnapshot            ← displayList.render(offscreen)
  │   └─ unlock
  ├─ 性能统计 / zoom 检测 / tryUpgrade
  ├─ enforceFullBudget                  ← 旧 bounds 计算（依赖 builderSession）
  └─ scanAndRequestMissingTextures
```

### 改造后

```
draw()
  ├─ 短路径短路 → return              ← 保留：zoomedOutFrameSettled + pendingUploads.empty()
  ├─ gesture blit → return             ← 保留，不涉及 PAGScene
  ├─ 渲染分支:
  │   ├─ lockContext
  │   ├─ flushPendingUploads            ← 保留
  │   ├─ canvas->clear + DrawBackground  ← 保留
  │   ├─ Record(scene, pagSurface)      ← 替换 displayList.render
  │   ├─ submit(recording)
  │   ├─ capture fitSnapshot            ← Record(scene, pagOffscreen)
  │   └─ unlock
  ├─ 性能统计 / zoom 检测 / tryUpgrade  ← 保留
  ├─ enforceFullBudget                  ← 替换 bounds 计算（PAGScene 新 API）
  └─ scanAndRequestMissingTextures      ← 保留
```

---

## 三、需要为 PAGScene 新增的 API

因为 `PAGScene` 已是 `PAGXDocument` 的友元（`PAGXDocument.h:312`），可以直接访问其私有方法。

### 3.1 `PAGScene::getImageBounds(filePath)`

```cpp
/// 返回引用指定 filePath 的所有 tgfx Layer 在 root 坐标系下的 bounds。
/// 用于 WeChat 端的 LRU 淘汰距离打分。
std::vector<tgfx::Rect> getImageBounds(const std::string& filePath) const;
```

直接遍历 PAGScene 的运行时树计算 bounds，**不维护额外的映射表**。

实现思路：调 `document->findLayersByImageFilePath(filePath)` 获取 `pagx::Layer*`，遍历 PAGComposition 树找到对应的 `PAGLayer`，用 `_rootComposition->runtimeLayer.get()` 作参考系调 `runtimeLayer->getBounds()`。

---

## 四、PAGXView 成员变更

**移除（Phase 1）**：

| 变量 | 类型 | 替代 |
|------|------|------|
| `displayList` | `tgfx::DisplayList` | PAGScene 内部管理 |

**移除（Phase 2）**：

| 变量 | 类型 | 替代 |
|------|------|------|
| `contentLayer` | `shared_ptr<tgfx::Layer>` | 由 PAGScene 管理，不再需要 PAGXView 持有引用 |
| `builderSession` | `unique_ptr<LayerBuilderSession>` | 用 `scene->getImageBounds()` 替代 |

**新增**：

| 变量 | 类型 | 用途 |
|------|------|------|
| `scene` | `shared_ptr<PAGScene>` | 核心渲染引擎 |
| `defaultTimeline` | `shared_ptr<PAGTimeline>` | 驱动动画 |
| `pagSurface` | `shared_ptr<PAGSurface>` | 包装 `tgfx::Surface`，用于 `Record()` |
| `userZoom` | `float` | 保存用户手势 zoom，与 fitScale 合并后传给 PAGDisplayOptions |
| `userOffsetX/Y` | `float` | 保存用户手势 offset，与 centerOffset 合并后传给 PAGDisplayOptions |

**保留**（不做改动）：

```
device, surface, externalTextures, thumbnailTextures, pendingUploads
pendingTextureDeletes, textureEventHandler
fitSnapshot, fitSnapshotPixelScale, zoomedOutFrameSettled
gestureActive, snapshotEnabled, lastGestureEndMs
fontConfig, canvasWidth, canvasHeight, pagxWidth, pagxHeight
hasRenderedFirstFrame, lastZoom, isZooming, isZoomingIn
currentMaxTilesRefinedPerFrame, tryUpgradeTimestampMs, lastZoomUpdateTimestampMs
imageOriginalSizes, inFlightFullRequests, evictedFullPaths
frameHistory, frameHistoryTotalTime, lastFrameSlow, gpuProbeCounter
boundsOriginX, boundsOriginY, boundsOriginOverridden
backgroundVisible, backgroundTGFXColor
imageDecodedPixelTotal, imageDecodedCount, imageSizeBuckets
fullBudget, fullBudgetHardCap, thumbnailBudget
currentFrameIndex, lastFullBudgetOverflowWarnFrame
```

---

## 五、关键方法改造

### 5.1 `buildLayers()`

```cpp
void PAGXView::buildLayers() {
  // 保留：矩阵预计算
  ResolveAllImagePatternMatrices(document.get(), &imageOriginalSizes);

  // PAGScene::Make 内部会自动 applyLayout，但不会传 fontConfig
  // 先手动应用字体配置
  document->applyLayout(&fontConfig);

  // 创建 PAGScene —— 层树构建、ViewModel、动画框架一次性完成
  scene = PAGScene::Make(document);
  if (!scene) return;

  defaultTimeline = scene->getDefaultTimeline();

  // 统一通过 PAGDisplayOptions 配置渲染参数
  // 背景色由 draw() 中的手动绘制路径控制（棋盘格/纯色），不走 PAGDisplayOptions
  // 需显式设为透明，防止 displayList 内部默认背景覆盖手动绘制内容
  auto options = scene->getDisplayOptions();
  options->setRenderMode(PAGRenderMode::Tiled);
  options->setTileUpdateMode(PAGTileUpdateMode::Fast);
  options->setMaxTileCount(DEFAULT_MAX_TILE_COUNT);
  options->setMaxTilesRefinedPerFrame(currentMaxTilesRefinedPerFrame);
  options->setSubtreeCacheMaxSize(DEFAULT_SUBTREE_CACHE_SIZE);
  options->setBackgroundColor({});  // 显式透明，避免 displayList 内部背景覆盖手动绘制的棋盘格

  // 合并居中 + 用户 zoom/offset
  applyMergedTransform();

  // 更新 document 级别数据
  pagxWidth = document->width;
  pagxHeight = document->height;

  // 标记 JS 侧等待首帧
  hasRenderedFirstFrame = false;
}
```

### 5.2 居中 + 用户手势合并

**核心变化**：不再有两级变换（`contentLayer->setMatrix` + `displayList zoom/offset`），全部合并到 `PAGDisplayOptions`。

```cpp
void PAGXView::applyMergedTransform() {
  if (!scene) return;

  float fitScale = computeFitScale();
  if (fitScale <= 0.0f) return;

  float centerOffsetX = (canvasWidth - pagxWidth * fitScale) * 0.5f;
  float centerOffsetY = (canvasHeight - pagxHeight * fitScale) * 0.5f;

  float effectiveZoom = fitScale * userZoom;
  float effectiveOffsetX = centerOffsetX * userZoom + userOffsetX;
  float effectiveOffsetY = centerOffsetY * userZoom + userOffsetY;

  scene->getDisplayOptions()->setZoomScale(effectiveZoom);
  scene->getDisplayOptions()->setContentOffset(effectiveOffsetX, effectiveOffsetY);
}
```

`computeFitScale()` 和 `getContentTransform()` **保持纯计算**，不依赖 PAGScene，因为它们被 JS 侧用于 cocraft 坐标映射（映射时需要的是「逻辑 fitScale」，而不是已经合并了用户 zoom 的值）。

### 5.3 `updateZoomScaleAndOffset()`

```cpp
void PAGXView::updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY) {
  // zoom 状态检测
  bool zoomChanged = (std::abs(zoom - lastZoom) > 0.001f);
  zoomedOutFrameSettled = false;

  // subtree cache 动态调整 — 改为通过 PAGDisplayOptions
  if (scene) {
    auto options = scene->getDisplayOptions();
    if (zoom <= 1.0f) {
      options->setSubtreeCacheMaxSize(ZOOMED_OUT_SUBTREE_CACHE_SIZE);
    } else {
      options->setSubtreeCacheMaxSize(0);
    }
  }

  if (zoomChanged) {
    if (!isZooming) {
      isZooming = true;
      updateAdaptiveTileRefinement();
    }
    isZoomingIn = (zoom > lastZoom);
    lastZoomUpdateTimestampMs = emscripten_get_now();
  }

  userZoom = zoom;
  userOffsetX = offsetX;
  userOffsetY = offsetY;
  applyMergedTransform();  // ← 替换：不再直接操作 displayList
  lastZoom = zoom;
}
```

### 5.4 `draw()` — 主渲染路径

**核心操作**：把 `displayList.render()` 替换为 `Record(scene, pagSurface)`。

```cpp
// 在 surface 创建/重建时同步重建 pagSurface（与 surface 绑定，不通过尺寸间接判断）
if (surface == nullptr || surface->width() != canvasWidth || surface->height() != canvasHeight) {
  // ... 创建 tgfx::Surface ...
  pagSurface = pagx::MakeFrom(surface);  // ← pagSurface 与 surface 同生同死
}
auto recording = Record(context, scene, pagSurface, false);
if (recording) {
  context->submit(std::move(recording));
  if (!hasRenderedFirstFrame) {
    hasRenderedFirstFrame = true;
  }
}
```

**离屏快照 capture**（替代 `displayList.render(offscreen)`）：

当前离屏 capture 会临时覆盖 displayList 的 zoom/offset（PAGXView.cpp:1659-1686），用 `pixelScale` 替代 zoom、offset 置 0，以获得无用户手势变形的快照后再恢复。改造后需对 PAGDisplayOptions 做同样的保存/恢复。

**⚠️ 关键：`Record(autoClear=false)` 不会清屏，必须保留现有离屏路径的 `offCanvas->clear()` + 背景绘制**（PAGXView.cpp:1664-1671）。旧路径在 `displayList.render(offscreen)` 前会先 `offCanvas->clear()` 再 `DrawSolidBackground/DrawBackground`，否则快照背景为脏数据/透明。改造后同样需保留这段前置绘制：

```cpp
auto options = scene->getDisplayOptions();
float savedZoom = options->getZoomScale();
Point savedOffset = options->getContentOffset();
options->setZoomScale(pixelScale);
options->setContentOffset(0.0f, 0.0f);

auto offscreen = tgfx::Surface::Make(context, offW, offH);
// 保留：离屏清屏 + 背景绘制（Record(autoClear=false) 不清屏，缺此步背景为脏数据/透明）
auto* offCanvas = offscreen->getCanvas();
offCanvas->clear();
if (backgroundVisible) {
  DrawSolidBackground(offCanvas, ...);  // 或 DrawBackground（棋盘格），与主路径一致
}
auto pagOffscreen = pagx::MakeFrom(offscreen);
auto offRecording = Record(context, scene, pagOffscreen, false);
if (offRecording) {
  context->submit(std::move(offRecording));
}
fitSnapshot = offscreen->makeImageSnapshot();

options->setZoomScale(savedZoom);
options->setContentOffset(savedOffset.x, savedOffset.y);
```

### 5.5 `computeFullPathBounds()` — 通过 PAGScene 查询

不再维护 `filePath→tgfx::Layer[]` 映射，直接通过 PAGScene 的新 API 查询：

```cpp
std::unordered_map<std::string, tgfx::Rect> PAGXView::computeFullPathBounds() const {
  if (!scene) return {};
  std::unordered_map<std::string, tgfx::Rect> result;
  for (const auto& kv : externalTextures) {
    auto boundsList = scene->getImageBounds(kv.first);
    tgfx::Rect unionBounds = tgfx::Rect::MakeEmpty();
    for (const auto& bounds : boundsList) {
      unionBounds.join(bounds);
    }
    if (!unionBounds.isEmpty()) {
      result.emplace(kv.first, unionBounds);
    }
  }
  return result;
}
```

### 5.6 `computeViewportInRootCoords()` — 替换数据源

**⚠️ 注意阶段区分，不在同一阶段混合新旧数据源**：

- **Phase 2 终态**（与 `computeFullPathBounds` 同期切换，见 Phase 2 step 12 & 15）：从 `PAGDisplayOptions` 读取，viewport 与 bounds 同在文档原始空间，自洽。
- **Phase 1 中间态**（此时 `computeFullPathBounds` 仍用旧 `builderSession`）：viewport 需从保留变量 `userZoom/userOffsetX/Y` 直接计算（同原公式，空间与旧 bounds 一致），**不能**用 `PAGDisplayOptions`（否则跨空间比较，LRU 淘汰排序错误）。

Phase 1 实现：
```cpp
tgfx::Rect PAGXView::computeViewportInRootCoords() const {
  // Phase 1：直接使用保留手势变量（与旧 displayList 空间一致）
  float zoomScale = userZoom;
  if (zoomScale <= 0.0f) return tgfx::Rect::MakeEmpty();
  auto offset = tgfx::Point{userOffsetX, userOffsetY};
  float left = (-offset.x) / zoomScale;
  // ... 其余 viewport 计算逻辑完全不变（含 expansion） ...
}
```

Phase 2 终态（与 `computeFullPathBounds` 迁移到 `scene->getImageBounds` 同期切换）：
```cpp
tgfx::Rect PAGXView::computeViewportInRootCoords() const {
  if (!scene) return tgfx::Rect::MakeEmpty();
  // 此时 bounds 和 viewport 同在文档原始空间
  auto options = scene->getDisplayOptions();
  float zoomScale = options->getZoomScale();          // fitScale * userZoom
  Point offset = options->getContentOffset();          // centerOffset * userZoom + userOffset
  // ... 其余 viewport 计算逻辑完全不变 ...
}
```

### 5.7 `getImageBounds()`(JS API) — **必须还原到画布像素空间**

**与 5.5 的内部 `computeFullPathBounds` 严格区分**，不能混为一谈。

**内部 `computeFullPathBounds`**（5.5）：在文档原始空间计算 bounds，供 `computeViewportInRootCoords`（同空间）做 LRU 相对距离打分 → 用 `scene->getImageBounds()` 直接替换 ✅

**JS API `getImageBounds`**：JS 侧 `types.ts:74` 契约要求返回 **画布像素空间**的 bounds（`ROOT-space (canvas-pixel) bounds`），用于 `ProgressiveImageUpgrader` 的 viewport 命中判断和焦点距离打分。旧实现经过 `contentLayer` 的 `fitScale+centerOffset` 矩阵，返回的是画布像素空间。**不能直接用 `scene->getImageBounds()`**，必须还原：

```cpp
emscripten::val PAGXView::getImageBounds(const val& filePathList) const {
  // ... 解析 filePathList ...
  float fitScale = computeFitScale();
  if (fitScale <= 0.0f) return result;

  float centerOffsetX = (canvasWidth - pagxWidth * fitScale) * 0.5f;
  float centerOffsetY = (canvasHeight - pagxHeight * fitScale) * 0.5f;

  // 文档原始空间 → 画布像素空间的变换：px = doc * fitScale + centerOffset
  auto m = tgfx::Matrix::MakeTrans(centerOffsetX, centerOffsetY);
  m.preScale(fitScale, fitScale);

  for (auto& path : paths) {
    auto docBoundsList = scene->getImageBounds(path);  // ← 文档原始空间
    tgfx::Rect unionBounds, largestBounds;
    // ... 计算 union/largest ...
    // 还原到画布像素空间
    unionBounds = m.mapRect(unionBounds);
    largestBounds = m.mapRect(largestBounds);
    result.set(path, /* {unionBounds, largestBounds} in 画布像素空间 */);
  }
}
```

### 5.7b 坐标系统一致性推导

取消两级变换后，需确认三个坐标系的参考系定义是否自洽：

**旧系统**：
- `contentLayer->setMatrix(fitScale, centerOffset)` 将内容坐标映射到 `displayList->root()` 空间
- `displayList.setZoomScale/Offset(zoom, offset)` 将 `displayList->root()` 空间映射到画布像素
- `computeViewportInRootCoords()` → `(canvas - offset) / zoom` → 得到 `displayList->root()` 空间下的 viewport
- `computeFullPathBounds()` → `getBounds(contentLayer->root())` → 得到 `displayList->root()` 空间下的 bounds

**新系统**（一级变换）：
- `PAGDisplayOptions` 内部 zoom 和 offset 合并为单级变换，作用在 `displayList->root()` → `_rootComposition->runtimeLayer` 路径上
- `computeViewportInRootCoords()` → `(canvas - offset) / zoom` → 得到 `displayList->root()` 空间下的 viewport（zoom/offset 含义已变，但公式不变）
- `getImageBounds()` → `getBounds(_rootComposition->runtimeLayer)` → 得到 `_rootComposition->runtimeLayer` 本地空间下的 bounds

**两系统对应关系**：
- 旧 `displayList->root()` 空间 = 新 `displayList->root()` 空间（同一个 DisplayList）
- `_rootComposition->runtimeLayer` 是 `displayList->root()` 的子节点，持有单位矩阵
- 因此 `_rootComposition->runtimeLayer` 本地空间 = `displayList->root()` 空间

**重要：两系统的绝对 bounds 数值并不同**：
- 旧系统图片 bounds 经过 `contentLayer` 的 `fitScale + centerOffset` 矩阵，数值处在**「fit 居中后」的 displayList-root 空间**
- 新系统图片 bounds 不经任何矩阵，数值处在**「文档原始」空间**（fit 居中合并到了 PAGDisplayOptions 的 zoom/offset 中）
- 两系统各自**内部** `bounds` 与 `viewport` 处于同一空间，故内部 LRU 相对距离打分等价 ✅
- 但跨系统的绝对 bounds 数值不同，**任何对外暴露绝对坐标的 API（如 JS getImageBounds）必须显式转换到约定空间**（参见 5.7）

### 5.8 `onZoomEnd()` / `updateAdaptiveTileRefinement()`

```cpp
// 原来
displayList.setMaxTilesRefinedPerFrame(targetCount);
// 改为
if (scene) {
  scene->getDisplayOptions()->setMaxTilesRefinedPerFrame(targetCount);
}
```

### 5.9 `updateSize()`

```cpp
void PAGXView::updateSize(int width, int height) {
  canvasWidth = width; canvasHeight = height;
  surface = nullptr;
  pagSurface = nullptr;  // ← 新增：下次 draw 时重建
  fitSnapshot = nullptr;
  // ...
  applyMergedTransform();  // ← 替换 applyCenteringTransform
}
```

### 5.10 `parsePAGX()` — 清空 scene 替代清空 displayList

```cpp
// 原来
displayList.root()->removeChildren();
contentLayer = nullptr;
builderSession = nullptr;
// 改为
scene = nullptr;
defaultTimeline = nullptr;
pagSurface = nullptr;
```

---

## 六、PAGScene 内部 `getImageBounds` 实现思路

```cpp
// PAGScene.cpp
std::vector<tgfx::Rect> PAGScene::getImageBounds(const std::string& filePath) const {
  if (!_rootComposition) return {};

  // 通过 PAGXDocument 友元关系获取引用该图片的 Layer 列表
  const auto& layers = document->findLayersByImageFilePath(filePath);
  if (layers.empty()) return {};

  // _rootComposition->runtimeLayer 即为内容 root 坐标系参考系
  auto* rootTgfxLayer = _rootComposition->runtimeLayer.get();
  std::vector<tgfx::Rect> results;

  // 遍历运行时树，匹配每个 pagx::Layer* 对应的 PAGLayer
  // 注意：plain PAGLayer 也可以有 children（PAGLayer.h:119-125），因此不能只对 Composition 下探
  std::vector<PAGLayer*> stack = {_rootComposition.get()};
  std::unordered_set<const Layer*> targetSet(layers.begin(), layers.end());

  while (!stack.empty()) {
    auto* layer = stack.back();
    stack.pop_back();

    for (const auto& child : layer->children) {
      if (!child || !child->runtimeLayer) continue;

      if (child->node && targetSet.count(child->node)) {
        auto bounds = child->runtimeLayer->getBounds(rootTgfxLayer);
        if (!bounds.isEmpty()) {
          results.push_back(bounds);
        }
      }

      // 始终递归 — PAGLayer 和 PAGComposition 都可以有 children
      stack.push_back(child.get());
    }
  }
  return results;
}
```

**性能说明**：`getImageBounds` 的成本由三部分组成：

1. **`findLayersByImageFilePath` 索引重建**：该索引是惰性构建的，且每次 `notifyChange`（即每次 `loadFileData`）都会置为失效（`PAGXDocument.h:302-307`）。WeChat 渐进式加载会高频调 `loadFileData`，导致索引在几乎每帧都被重建——**这与文档一「不需要维护映射表」的卖点并不矛盾**：此处重建的是 document 内部的 `filePath→pagx::Layer*` 倒排索引（不是 PAGXView 侧的映射表），重建成 O(nodes) 级别；每帧仅当有 `loadFileData` 时才会触发。
2. **全树 PAGLayer 遍历**：遍历 PAGComposition 树中的所有 PAGLayer，匹配 `pagex::Layer*` 指针。O(layers)。
3. **tgfx getBounds 计算**：首次触发 lazy 计算后走缓存，O(1)。

总成本 = O(nodes, layers) 级别的遍历。相比旧方案（索引直查 tgfx::Layer 省掉步骤 2），新增了全树遍历开销。如果实测发现性能问题，可以采用缓存策略：
1. 在 `buildRuntimeTree()` 时顺便建 `filePath→PAGLayer[]` 缓存
2. 或在首次 `getImageBounds()` 时懒加载建缓存
3. 在 `onNodesChanged` 时让缓存失效

---

## 七、分阶段实施

### Phase 1：渲染管线替换（中风险）

1. PAGScene 新增：`getImageBounds()`
2. PAGXView.h：新增 `scene`、`defaultTimeline`、`pagSurface`、`userZoom/userOffset`，移除 `displayList`
3. `buildLayers()` → `PAGScene::Make`（同时 `builderSession->build()` 仍然调用，双树共存）
4. `draw()` 主渲染 → `Record(scene, pagSurface)`
5. `draw()` 离屏 capture → PAGDisplayOptions 临时保存/恢复 + `Record(scene, pagOffscreen)`
6. `updateZoomScaleAndOffset()` → `applyMergedTransform()`
7. `computeViewportInRootCoords()` → **临时从 `userZoom/userOffsetX/Y` 直接计算**（NOT `PAGDisplayOptions`，详见 5.6 注释。因为此时 bounds 还在旧 `builderSession` 空间，viewport 必须与之同空间；Phase 2 bounds 迁移后再切到 `PAGDisplayOptions`）
8. `updateSize()` → 重建 `pagSurface`
9. `parsePAGX()` → 清空 `scene`
10. `onZoomEnd()` / `updateAdaptiveTileRefinement()` → `PAGDisplayOptions`
11. 移除所有 `displayList` 的引用（注意：`displayList.zoomScale()/contentOffset()` 不仅出现在本文档提及的方法中，还需全局搜索确保无遗漏；`displayList.root()` 在离屏 capture 中已替换，`getNodePosition` 未使用 displayList，确认无引用）

**此时 `builderSession` 和 `contentLayer` 仍保留**，仅用于 `computeFullPathBounds()` 和 `getImageBounds()`（JS API）。

**双树共存开销**：`PAGScene::Make`（内部 `BuildForRuntime`）和 `builderSession->build()`（内部 `Build`）同时运行，两棵完整的 tgfx 层树并存。内存约为单棵树的两倍，构建时间约为两倍。Phase 1 过渡期内可接受，但 Phase 2 应尽快完成迁移以回收内存。

**验证**：渲染正常、手势缩放正常、手势冻结正常、图片加载正常、LRU 淘汰正常。

### Phase 2：bounds 查询迁移（中风险）

12. `computeFullPathBounds()`(内部 LRU 淘汰) → 直接 `scene->getImageBounds()`（文档原始空间）
13. `getImageBounds()`(JS 层 API) → `scene->getImageBounds()` + `Matrix::MakeTrans(centerOffset).preScale(fitScale)` 还原到画布像素空间
14. **`computeViewportInRootCoords()` → 从 `userZoom/userOffset` 切到 `PAGDisplayOptions`**（与 bounds 同时迁移，保持同空间）
15. **清理 `draw()` 中对 `contentLayer` 的直接引用**：现有 `draw()` 有 `bool hasContent = contentLayer != nullptr;`（PAGXView.cpp:1636），移除 `contentLayer` 后此处编译失败，需改为 `bool hasContent = scene != nullptr;`（或依据首帧标记）。全局搜索 `contentLayer` 确保无其他遗漏引用。
16. 移除 `builderSession` 和 `contentLayer`

**验证**：LRU 淘汰行为与 Phase 1 一致。

### Phase 3：动画支持（按需）

17. 在适当的时机调 `defaultTimeline->advanceAndApply()`（注意：无动画的 PAGX，`getDefaultTimeline()` 返回 `nullptr`，调用前需判空）

---

## 八、风险点

| # | 风险 | 影响 | 方案 |
|---|------|------|------|
| 1 | `fontConfig` 无法传给 `PAGScene::Make` | fallback 字体丢失 | 先手动 `document->applyLayout(&fontConfig)`，再 `PAGScene::Make`（Make 内部检测已 layout 则跳过） |
| 2 | `renderTo()` 是 private | 无法在不 flush 的情况下渲染 | 继续用 `Record()` friend 函数，它内部调 `flushDataBinds + renderTo + flush`，WeChat 不需要在 render 和 flush 之间插入操作，所以可行 |
| 3 | `getImageBounds` 遍历整棵树的性能 + `findLayersByImageFilePath` 索引被高频 invalidate | draw() 中额外开销，渐进加载时每帧重建索引 | 先实现简单版本，实测后若性能不满足则加 PAGLayer 级缓存（`buildRuntimeTree` 时构建，`onNodesChanged` 时失效） |
| 4 | PAGScene 图片刷新路径（`refreshNodes` 子树重建）比旧方案（原地更新 vector contents）开销更大 | 图片升级时帧率下降 | Phase 2 后实测评估，必要时加图片变更的增量刷新路径 |
| 5 | **Phase 1 双树共存**（`builderSession->build` + `PAGScene::Make` 同时运行） | 内存翻倍，构建时间翻倍；`displayList` 移除后 `builderSession` 和 `contentLayer` 不挂在任何 displayList 上，但 bounds 计算不需要 displayList 所以不影响正确性 | 低风险但不可长期维持，Phase 2 需尽快接替 |
| 6 | **离屏快照未处理 PAGDisplayOptions 临时覆盖** | 快照带上用户手势变换，内容错误 | 离屏 capture 段已给出 PAGDisplayOptions 的保存/恢复方案 |
| 7 | **Phase 1 跨系统 viewport 与 bounds 比较**（`computeViewportInRootCoords` 若用 `PAGDisplayOptions` 则在文档原始空间，但 `computeFullPathBounds` 仍在旧 `builderSession` 的画布像素空间） | LRU 淘汰距离计算基准不同，逐出排序全错 | 5.6 已明确 Phase 1 viewport 从 `userZoom/userOffset` 直接计算（与 bounds 同空间）；Phase 2 迁移 bounds 时**同步**切 viewport 到 `PAGDisplayOptions`，确保同步 |
| 8 | **PAGDisplayOptions 内部背景色未显式透明** | displayList 绘制背景覆盖手动棋盘格 | 5.1 已加 `options->setBackgroundColor({})` 显式透明 |
| 9 | **离屏 capture 遗漏 `clear()` + 背景绘制** | `Record(autoClear=false)` 不清屏，快照背景脏数据/透明 | 5.4 离屏段已补 `offCanvas->clear()` + `DrawSolidBackground/DrawBackground` 前置绘制 |
| 10 | **`draw()` 中 `hasContent = contentLayer != nullptr` 未列改造点** | Phase 2 移除 `contentLayer` 后编译失败 | Phase 2 step 15 已补：改为 `scene != nullptr` 并全局搜 `contentLayer` |

---

## 九、第三方评审核实结论（2026-07-09）

针对一份外部评审意见，逐条核实其准确性，结论如下（已据此更新上文 5.4 / Phase 2 / 风险表）：

| 评审论点 | 判定 | 依据 |
|---------|------|------|
| 离屏 capture 遗漏 `clear()`+`DrawBackground` | ✅ 成立（P0） | PAGXView.cpp:1664-1671 现有离屏路径确有 clear+背景绘制；已补入 5.4 与风险 #9 |
| `draw()` 中 `hasContent = contentLayer != nullptr` 未列改造 | ✅ 成立（P1） | PAGXView.cpp:1636 确存在；已补入 Phase 2 step 15 与风险 #10 |
| `getContentOffset()` 返回 `pagx::Point` 而非 `tgfx::Point` | ✅ 成立 | PAGDisplayOptions.h:119；5.4 用 `Point savedOffset` 已正确 |
| `getDefaultTimeline()` 需判空 | ✅ 成立 | PAGScene.cpp:417-425，无 animations 返回 `nullptr`；已补入 Phase 3 step 17 |
| `onNodesChanged` foreign 分支触发全树重建 | ✅ 成立，但需收窄场景 | PAGScene.cpp:617-673 确有该分支；但 Cocraft 图片刷新走 `loadFileData`→普通 ImagePattern 常规分支，通常不落 foreign 全树重建 |
| 图片刷新路径可行但性能需实测 | ✅ 合理 | 与风险 #4 一致 |
| §5.7b 假设 root `runtimeLayer` 为单位矩阵但未验证 | ⚠️ 假设实际成立，非缺陷 | `LayerBuilder::build()`（LayerBuilder.cpp:633-646）root 由 `tgfx::Layer::Make()` 创建，仅 `setScrollRect` 裁剪，未设 matrix，恒为单位矩阵；5.7b 推导成立 |
| **`getNodePosition` 通过 `contentLayer` 遍历 `findByName`，Phase 2 编译失败** | ❌ **不成立（误读）** | `getNodePosition`（PAGXView.cpp:1333-1371）从 `node->customData["page-offset"]` 读取，**不引用 `contentLayer`、无 `findByName` 调用**（全局搜索零匹配），与 `contentLayer` 存废完全解耦 |

**结论**：该评审的核心价值在于发现了两处真实缺陷（离屏 clear/背景、`hasContent`）及两个类型/判空次要项，已全部纳入本方案。但其中「`getNodePosition` 依赖 `contentLayer`」一条系对代码结构的误读，实际实现不受 Phase 2 影响，不应作为改造点。
