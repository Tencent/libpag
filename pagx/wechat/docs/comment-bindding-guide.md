# PAGX 评论浮层坐标转换指南

## 背景

在 Cocraft 中，用户可以在设计稿上添加评论标记。当设计稿导出为 PAGX 文件并在小程序端渲染时，需要将评论标记准确地叠加显示在对应位置上。

本文档说明如何将 Cocraft 画布坐标系中的评论坐标转换为小程序屏幕坐标。

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
| **PAGX 内容坐标系** | 以设计稿左上角为原点 (0,0)，范围为 (0\~contentWidth, 0\~contentHeight) |
| **Canvas 物理像素坐标系** | 小程序 Canvas 的像素坐标，PAGX 内容在其中等比缩放并居中显示 |

---

## 转换步骤

整个坐标转换分为两个阶段：

### 阶段一：初始定位（加载文件时计算一次）

加载 PAGX 文件后，调用 `View.getContentTransform()` 获取所有变换参数：

```js
const t = View.getContentTransform();
// t = { boundsOriginX, boundsOriginY, fitScale, centerOffsetX, centerOffsetY }
```

然后对每个评论计算 canvas 物理像素坐标：

```
baseX = (cocraftX - t.boundsOriginX) * t.fitScale + t.centerOffsetX
baseY = (cocraftY - t.boundsOriginY) * t.fitScale + t.centerOffsetY
```

这个公式做了两件事：

**① 减去 boundsOrigin：** 将 Cocraft 绝对坐标转换为相对于 PAGX 内容左上角的偏移

```
            boundsOrigin
            (-900, -193)
                 ┌──────────────┐
                 │              │  ● 评论A (pagxX=519, pagxY=262)
                 │              │    pagxX > 450，在内容右侧外面
                 │         ● 评论B (pagxX=181, pagxY=494)
                 │           在内容内部
                 └──────────────┘

评论A: cocraftX = -381, cocraftY = 69
pagxX = -381 - (-900) = 519    ← 超出 contentWidth(450)，在右侧外面
pagxY =   69 - (-193) = 262

评论B: cocraftX = -719, cocraftY = 301
pagxX = -719 - (-900) = 181    ← 在 contentWidth(450) 内
pagxY =  301 - (-193) = 494
```

> **说明：** 结果可以为负数（评论在内容左侧/上方）或超出内容尺寸（评论在内容右侧/下方），这都是正常的。

**② 乘以 fitScale + 加 centerOffset：** PAGX 内容在 Canvas 中等比缩放并居中显示，评论坐标需要同样的变换

```
Canvas (1264 × 2237 物理像素)
┌────────────────────────────────────┐
│← centerOffsetX →│                  │
│  ┌──────────────┐                  │
│  │              │  ● 评论A (baseX=1346.9, baseY=637.1)
│  │  PAGX 内容    │    在 Canvas 中也在内容右侧外面
│  │  (缩放后)     │                  │
│  │         ● 评论B (baseX=525.0, baseY=1201.4)
│  │          在内容内部               │
│  └──────────────┘                  │
│                                    │
└────────────────────────────────────┘

评论A: baseX = 519 × 2.43 + 84.9 = 1346.9（在内容右侧外面）
        baseY = 262 × 2.43 + 0    = 637.1

评论B: baseX = 181 × 2.43 + 84.9 = 525.0（在内容内部）
        baseY = 494 × 2.43 + 0    = 1201.4
```

`baseX` / `baseY` 是静态值，**只需在加载文件时计算一次**，后续缓存使用。

---

### 阶段二：动态定位（缩放/平移时每帧更新）

用户手势操作（缩放、平移）会产生 `zoom`、`panOffsetX`、`panOffsetY` 三个动态参数。将 base 坐标转换为最终屏幕 CSS 像素位置：

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

> **为什么除以 dpr？** Canvas 使用物理像素坐标（确保高清渲染），而 WXML 元素使用 CSS 像素定位。`dpr` 是二者之间的换算比例。

---

## 完整公式总结

```
  加载 PAGX 后调用一次
  const t = View.getContentTransform()
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

## 参数获取方式

| 参数 | 获取方式 | 更新时机 |
|------|----------|----------|
| `boundsOriginX/Y` | `View.getContentTransform()` | 加载 PAGX 文件后 |
| `fitScale` | `View.getContentTransform()` | 加载 PAGX 文件后 |
| `centerOffsetX/Y` | `View.getContentTransform()` | 加载 PAGX 文件后 |
| `dpr` | `wx.getSystemInfoSync().pixelRatio` | 页面加载时 |
| `zoom, panOffsetX/Y` | 手势管理器输出 | 每次手势事件 |
| `cocraftX/Y` | Cocraft 评论服务 | 业务数据 |

---

## 代码示例

### JS 端：计算 baseX/baseY（加载时调用一次）

```js
function calcCommentBasePositions(View, comments) {
  var t = View.getContentTransform();

  return comments.map(function (c) {
    return {
      id: c.id,
      baseX: (c.cocraftX - t.boundsOriginX) * t.fitScale + t.centerOffsetX,
      baseY: (c.cocraftY - t.boundsOriginY) * t.fitScale + t.centerOffsetY,
    };
  });
}
```

### WXS 端：动态更新屏幕位置（每帧调用）

```js
function onTransformChanged(newVal, oldVal, ownerInstance, instance) {
  if (!newVal || !newVal.zoom) return;
  var dataset = instance.getDataset();
  var screenX = (dataset.basex * newVal.zoom + newVal.panOffsetX) / newVal.dpr;
  var screenY = (dataset.basey * newVal.zoom + newVal.panOffsetY) / newVal.dpr;
  instance.setStyle({ left: screenX + 'px', top: screenY + 'px' });
}
```

### WXML 模板

```xml
<view
  wx:for="{{commentPins}}"
  wx:key="id"
  class="comment-pin"
  style="position:absolute;"
  data-basex="{{item.baseX}}"
  data-basey="{{item.baseY}}"
  change:prop="{{commentWxs.onTransformChanged}}"
  prop="{{commentTransform}}"
>
  <!-- 评论内容 -->
</view>
```

---

## 常见问题

### Q: 评论显示在画布外面怎么办？

这是正常的。评论在 Cocraft 中可能就在设计稿外面（比如标注在旁边），转换后自然也在 PAGX 渲染区域外面。用户缩小画布后就能看到。

### Q: boundsOriginX/Y 返回 0 怎么办？

说明 PAGX 文件中没有嵌入 `bounds-origin` 数据。需要：
- 确认 Cocraft 导出时已将 `bounds-origin-x` / `bounds-origin-y` 写入 PAGX 文件根 SVG 元素的 `data-*` 属性
- 或者在 JS 端手动调用 `View.setBoundsOrigin(x, y)` 设置

### Q: 为什么用 WXS 而不是 setData 更新位置？

`setData` 是异步的（逻辑层 → 渲染层通信），在快速缩放/平移时会导致评论标记跟不上画面。WXS 直接运行在渲染线程上，可以同步更新 DOM 样式，保证评论标记与画面同步移动。
