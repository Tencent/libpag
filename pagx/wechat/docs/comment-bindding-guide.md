# PAGX 评论浮层坐标转换指南

## 背景

在 Cocraft 中，用户可以在设计稿上添加评论标记。当设计稿导出为 PAGX 文件并在小程序端渲染时，需要将评论标记准确地叠加显示在对应位置上。

本文档说明如何将 Cocraft 画布坐标系中的评论坐标转换为小程序屏幕像素位置，并提供完整的小程序端接入步骤。

---

## 坐标系概览

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

---

## 转换公式

整个坐标转换分为两个阶段：

### 阶段一：初始定位（加载文件时计算一次）

加载 PAGX 文件后，调用 `View.getContentTransform()` 一次性获取所有变换参数：

```js
const t = View.getContentTransform();
// t = { boundsOriginX, boundsOriginY, fitScale, centerOffsetX, centerOffsetY }
```

然后对每个评论计算 Canvas 物理像素坐标（静态值，缓存后续复用）：

```
baseX = (cocraftX - t.boundsOriginX) * t.fitScale + t.centerOffsetX
baseY = (cocraftY - t.boundsOriginY) * t.fitScale + t.centerOffsetY
```

> **说明：** 结果可以为负数（评论在内容左侧/上方）或超出 Canvas 范围（评论在内容右侧/下方），这都是正常的。

### 阶段二：动态定位（缩放/平移时每帧更新）

用户手势产生的 `zoom`、`panOffsetX`、`panOffsetY` 实时变化，需要将 base 坐标转换为屏幕 CSS 像素位置：

```
screenX = (baseX × zoom + panOffsetX) / dpr
screenY = (baseY × zoom + panOffsetY) / dpr
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

---

## 小程序端接入步骤

### 前置条件

- 已集成 PAGX Viewer SDK（`pagx-view.ts`），能正常渲染 PAGX 文件
- 已获取评论数据（每个评论包含 `cocraftX`、`cocraftY` 坐标）

### 第 1 步：准备 Page data

在 Page `data` 中声明评论相关的状态：

```js
Page({
  data: {
    commentPins: [],        // 评论基准坐标列表：[{id, baseX, baseY, text, color}]
    commentTransform: null  // 动态变换参数：{zoom, panOffsetX, panOffsetY, dpr}
  },
  dpr: 2,   // 设备像素比，onLoad 时获取
})
```

### 第 2 步：加载 PAGX 后计算评论基准坐标

在 `View.loadPAGX()` 之后调用 `View.getContentTransform()` 计算每个评论的 `baseX`/`baseY`：

```js
// comments 来自评论服务，格式：[{id, cocraftX, cocraftY, text, color}]
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
      zoom: 1,
      panOffsetX: 0,
      panOffsetY: 0,
      dpr: this.dpr
    }
  });
}
```

### 第 3 步：创建 WXS 文件处理动态定位

新建 `comment.wxs` 文件，在渲染线程上直接更新评论位置（避免 `setData` 延迟）：

```js
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

### 第 4 步：WXML 模板

在 Canvas 同级位置添加评论浮层。通过 `wxs` 引用 WXS 模块，`change:prop` 绑定变换回调：

```xml
<!-- 引入 WXS 模块 -->
<wxs src="./comment.wxs" module="commentWxs" />

<!-- Canvas（PAGX 渲染区域） -->
<canvas id="pagx-canvas" type="webgl2" ... />

<!-- 评论浮层（与 Canvas 同级，共享同一个定位上下文） -->
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

### 第 5 步：WXSS 样式

```css
/* 包裹 Canvas 和评论的容器 */
.canvas-container {
  position: relative;
  width: 100%;
  height: 100%;
  overflow: hidden;
}

/* 评论标记 */
.comment-pin {
  position: absolute;
  z-index: 50;
  pointer-events: none;  /* 不拦截触摸事件，手势穿透到 Canvas */
}

.comment-dot {
  width: 20rpx;
  height: 20rpx;
  border-radius: 50%;
  border: 3rpx solid #fff;
  box-shadow: 0 2rpx 8rpx rgba(0, 0, 0, 0.3);
}

.comment-bubble {
  position: absolute;
  left: 24rpx;
  top: -8rpx;
  background: rgba(0, 0, 0, 0.8);
  padding: 8rpx 16rpx;
  border-radius: 8rpx;
  white-space: nowrap;
  max-width: 300rpx;
}

.comment-text {
  color: #fff;
  font-size: 20rpx;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
```

### 第 6 步：手势回调中同步更新评论变换

每次手势（缩放/平移）回调时，将最新的变换参数通过 `setData` 传给 WXS：

```js
applyGestureState(state) {
  if (!state || !this.View) return;

  // 更新 PAGX 渲染引擎
  this.View.updateZoomScaleAndOffset(state.zoom, state.offsetX, state.offsetY);

  // 同步更新评论位置（WXS 渲染线程处理）
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

---

## API 参考

### `View.getContentTransform()`

返回坐标转换所需的所有参数。加载 PAGX 文件后调用一次。

**返回值：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `boundsOriginX` | number | PAGX 内容左上角在 Cocraft 画布中的 X 坐标 |
| `boundsOriginY` | number | PAGX 内容左上角在 Cocraft 画布中的 Y 坐标 |
| `fitScale` | number | 等比缩放因子（contain 模式） |
| `centerOffsetX` | number | 水平居中偏移（物理像素） |
| `centerOffsetY` | number | 垂直居中偏移（物理像素） |

### `View.setBoundsOrigin(x, y)`

手动设置 boundsOrigin，覆盖 PAGX 文件中的值。

---

## 常见问题

### Q: 评论显示在画布外面怎么办？

这是正常的。评论在 Cocraft 中可能就在设计稿外面（比如标注在旁边），转换后自然也在 PAGX 渲染区域外面。用户缩小画布后就能看到。如果不希望显示画布外的评论，可以对 `baseX`/`baseY` 做范围判断后过滤。

### Q: boundsOriginX/Y 返回 0 怎么办？

说明 PAGX 文件中没有嵌入 `bounds-origin` 数据。需要：
- 确认 Cocraft 导出时已将 `bounds-origin-x` / `bounds-origin-y` 写入 PAGX 文件根节点的 `data-*` 属性
- 或者在 JS 端手动调用 `View.setBoundsOrigin(x, y)` 设置
    现有版本中，小程序端可以获取到 bounds-origin-x 和 bounds-origin-y，后续新版本中 pagx 文件自身会携带 bounds-origin 相关参数

### Q: 为什么用 WXS 而不是 setData 更新评论位置？

`setData` 是异步的（逻辑层 → 渲染层通信），在快速缩放/平移时会导致评论标记跟不上 PAGX 画面。WXS 直接运行在渲染线程上，可以同步更新 DOM 样式，保证评论标记与画面同步移动。

### Q: 切换 PAGX 文件后需要重新计算吗？

需要。每次调用 `View.loadPAGX()` 后，`getContentTransform()` 返回的参数可能不同（不同文件的 boundsOrigin、内容尺寸不同），必须重新调用 `initCommentOverlays()` 计算新的 `baseX`/`baseY`。

### Q: Canvas 尺寸变化（如屏幕旋转）后需要重新计算吗？

需要。Canvas 尺寸变化会影响 `fitScale` 和 `centerOffset`。调用 `View.updateSize()` 后需要重新调用 `getContentTransform()` 并重新计算所有评论的 `baseX`/`baseY`。
