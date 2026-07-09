# WeChat PAGXView 性能优化补回实现文档（建议 1 + 建议 2）

> 日期：2026-07-09
> 背景：`PAGScene_MIGRATION_DIFF.md` 记录了迁移到 `PAGScene` 后主动砍掉的 5 项性能优化路径，现每帧全量渲染。
> 本文档描述按方案补回其中两组优化的设计与落地细节：
> - **建议 1**：手势冻结 blit + 稳态 blit + 高分快照离屏采集（对应 DIFF 的 #2 #3 #4）——纯 wechat 端实现。
> - **建议 2**：空闲短路 + dirty 门控（对应 DIFF 的 #1 #5）——需核心库为 `PAGScene` 暴露脏标记。

---

## 一、能力前提（已核实）

| 能力 | 来源 | 结论 |
|------|------|------|
| 主 GL 上下文 / 主 surface | `PAGXView` 持有 `device`（`lockContext()`）与 `tgfx::Surface surface` | 可直接创建离屏 surface、blit |
| 离屏渲染 | `tgfx::Surface::Make(context,w,h)` + `pagx::MakeFrom(tgfx::Surface)` + `pagx::Record()` | 可用 |
| 快照 | `tgfx::Surface::makeImageSnapshot()` | 可用 |
| blit | `tgfx::Canvas::clear(Color)` / `translate` / `scale` / `drawImage` | 可用 |
| 脏标记 | `tgfx::DisplayList::hasContentChanged()` 已存在，但 `PAGScene` 未转发 | **需核心库加一个转发方法** |

---

## 二、关键坐标推导（务必与旧实现区分）

迁移后 `applyMergedTransform()` 把两级变换合并写入 `PAGDisplayOptions`：

```
effectiveZoom   = fitScale * userZoom
effectiveOffset = centerOffset * userZoom + userOffset
主 surface 像素:  screen = doc * effectiveZoom + effectiveOffset
```

其中 `centerOffset` 是 contain-mode 居中偏移，`fitScale = computeFitScale()`。

### 2.1 离屏快照渲染的 transform

快照捕获的是「`userZoom = 1` 的 fit 视图」并按 `pixelScale`（1 或 2）超采样。离屏尺寸 = `canvas 尺寸 * pixelScale`。要让离屏像素满足：

```
snapshot_px = doc * fitScale * pixelScale + centerOffset * pixelScale
```

因此临时写入的离屏 `PAGDisplayOptions`：

```
zoomScale     = fitScale * pixelScale
contentOffset = (centerOffsetX * pixelScale, centerOffsetY * pixelScale)
```

渲染完成后调用 `applyMergedTransform()` 恢复为用户视图。

### 2.2 blit 回主 surface 的缩放/平移

由 2.1 反解 `doc`：

```
doc * fitScale + centerOffset = snapshot_px / pixelScale
```

代入主 surface 公式：

```
screen = userZoom * (doc*fitScale + centerOffset) + userOffset
       = userZoom * (snapshot_px / pixelScale) + userOffset
```

得 blit 变换：

```
scale     = userZoom / pixelScale
translate = (userOffsetX, userOffsetY)
```

> ⚠️ 旧实现用的是 `displayList.zoomScale()`（旧架构只承载 userZoom）。新架构 `options.getZoomScale()` 是 `effectiveZoom`，**不能**直接拿来当 blit scale。blit 必须使用成员 `userZoom` / `userOffsetX/Y`。

---

## 三、建议 1：快照采集 + blit 快路径

### 3.1 新增私有方法 `captureFitSnapshot(tgfx::Context*)`

在全量渲染分支内、`submit` 之后、`device->unlock()` 之前调用。仅当 `snapshotEnabled && fitSnapshot == nullptr && scene && surface` 时执行一次：

1. 计算 `fitScale`、`pixelScale`（`fitScale < HIGH_RES_FIT_SCALE_THRESHOLD` → `HIGH_RES_PIXEL_SCALE`，否则 `DEFAULT_PIXEL_SCALE`）。
2. 显存预算判断：`external + thumbnail + snapshotBytes > fullBudget` 时把 `pixelScale` 降回 1。
3. `tgfx::Surface::Make(context, offW, offH)` → `pagx::MakeFrom` 得到离屏 `PAGSurface`。
4. 按 2.1 临时改写 `PAGDisplayOptions`，`Record(context, scene, offPagSurface, true)` + `submit`。
5. `fitSnapshot = offscreen->makeImageSnapshot()`；`fitSnapshotPixelScale = pixelScale`。
6. `context->flush()` + `submit` 让快照 copy 立即完成（避免延迟 copy 抓到脏像素）。
7. `applyMergedTransform()` 恢复用户视图。

> 副作用：步骤 4/7 会 `setZoomScale/setContentOffset`，`DisplayList` 因此被标脏。采集后的下一帧会多做一次全量渲染，再下一帧才进入 blit 稳态。此行为与旧实现一致，可接受。

### 3.2 blit 快路径（draw 顶部）

```
fitOnly = snapshotEnabled && !gestureActive && fitSnapshot &&
          userZoom <= ZOOM_DRIFT_MARGIN && !scene->hasContentChanged() && pendingUploads.empty();
if (snapshotEnabled && (gestureActive || fitOnly) && surface && fitSnapshot) {
    context = lockContext();
    canvas = surface->getCanvas();
    canvas->clear(backgroundTGFXColor);           // 纯色背景，与全量路径一致（无棋盘格）
    canvas->save();
    canvas->translate(userOffsetX, userOffsetY);
    canvas->scale(userZoom / pixelScale, userZoom / pixelScale);
    canvas->drawImage(fitSnapshot);
    canvas->restore();
    submit(flush());
    drainPendingTextureDeletes();
    unlock();
    hasRenderedFirstFrame = true;
    if (fitOnly) zoomedOutFrameSettled = true;
    return true;
}
```

- 手势期（`gestureActive`）每帧 blit，跳过昂贵的 `Record`。
- 稳态（`fitOnly`，zoom≈1 且内容未变）blit 后设 `zoomedOutFrameSettled`，供空闲短路进一步跳过。
- `setGestureActive(true)` 在 `fitSnapshot == nullptr` 时仍会自我置为 `false`（已有逻辑），快照建立后自然生效，无需改签名。

---

## 四、建议 2：空闲短路 + dirty 门控（依赖核心库）

### 4.1 核心库改动（最小侵入）

`include/pagx/PAGScene.h` 新增公开方法，`src/pagx/PAGScene.cpp` 转发到内部 `displayList`：

```cpp
// PAGScene.h
bool hasContentChanged() const;

// PAGScene.cpp
bool PAGScene::hasContentChanged() const {
  return displayList != nullptr && displayList->hasContentChanged();
}
```

### 4.2 空闲短路（draw 顶部，位于 blit 之前）

```
if (snapshotEnabled && zoomedOutFrameSettled && !gestureActive &&
    userZoom <= ZOOM_DRIFT_MARGIN && !scene->hasContentChanged() && pendingUploads.empty()) {
    return true;   // 视图静止 + 内容未变 + 无待上传 → 零 GPU 开销
}
```

### 4.3 dirty 门控（全量渲染分支入口）

```
bool dirty = scene->hasContentChanged() || !pendingUploads.empty() || !hasRenderedFirstFrame;
if (!dirty) return true;   // 内容已在屏上，无需重绘
```

> `advanceTimelines()` 仍在 `draw()` 顶部推进动画。有动画的文档每帧内容变化 → `hasContentChanged()` 恒真 → 不会被短路/门控误跳过；纯静态文档 delta 推进不改变内容 → 可被正确短路。

---

## 五、draw() 最终控制流

```
draw():
  if !device || !scene: return false
  advanceTimelines(now)                         // 动画推进（影响脏标记）
  liveOffset/effectiveZoom 读取（仅日志用）

  [空闲短路]  settled && !changed && ...  -> return true
  [blit]      (gesture || fitOnly)        -> blit fitSnapshot; return true

  dirty = changed || uploads || 首帧
  if !dirty: return true                        // dirty 门控

  lockContext
  flushPendingUploads
  (重建 surface/pagSurface if needed)
  Record(scene -> pagSurface, autoClear=true); submit
  captureFitSnapshot(context)                   // 仅 fitSnapshot==null 时采集一次
  drainPendingTextureDeletes
  unlock

  updatePerformanceState / tile refinement / enforceFullBudget / scanAndRequest ...
  return true
```

---

## 六、涉及文件

| 文件 | 改动 |
|------|------|
| `include/pagx/PAGScene.h` | 新增 `hasContentChanged()` 声明 |
| `src/pagx/PAGScene.cpp` | 新增 `hasContentChanged()` 实现（转发 displayList） |
| `pagx/wechat/src/PAGXView.h` | 新增私有方法 `captureFitSnapshot(tgfx::Context*)` 声明；更新 `setGestureActive/setSnapshotEnabled` 注释 |
| `pagx/wechat/src/PAGXView.cpp` | 重构 `draw()`（空闲短路 + blit + dirty 门控 + 快照采集）；实现 `captureFitSnapshot` |

---

## 七、验证

1. 编译：
   ```
   ./codeformat.sh 2>/dev/null; true
   cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
   cmake --build cmake-build-debug --target PAGFullTest
   ```
2. 真机（wx_demo）：
   - 静止复杂文档：确认 GPU 占用/功耗下降（空闲短路生效）。
   - 捏合缩放：确认掉帧改善（手势 blit 生效），松手后画面清晰（快照 2x 超采样 + 稳态回到全量渲染）。
   - 平移：blit 平移无错位（验证 2.2 坐标公式）。
   - 首帧、页面切换、`updateSize`：无黑屏、无残留旧内容（快照/短路状态在这些路径已重置）。

---

## 八、风险与注意

- **脏标记时机**：`Record()` 内部先 `flushDataBinds()` 再 `renderTo()`；`hasContentChanged()` 在 `draw()` 顶部读取时尚未 flush databind。对 wechat PAGX 场景（静止无 pending databind）无影响，但若未来引入静止期 databind 变更，需要把判断移到 flush 之后或让 `hasContentChanged()` 覆盖 databind 脏状态。
- **背景**：迁移后统一为 `PAGDisplayOptions` 纯色背景，blit 路径用 `canvas->clear(backgroundTGFXColor)` 保持一致，不再有棋盘格。
- **快照采集额外一次离屏渲染**：仅在 `fitSnapshot==null` 时发生一次（首帧后 / 快照被清后），成本可接受。
