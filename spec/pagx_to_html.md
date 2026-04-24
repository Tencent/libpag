# PAGX → HTML 转换技术方案

## 目录

1. [概述](#1-概述)
2. [总体架构](#2-总体架构)
3. [根文档与画布](#3-根文档与画布)
4. [资源系统](#4-资源系统)
5. [图层系统](#5-图层系统)
6. [几何元素](#6-几何元素)
7. [颜色源系统](#7-颜色源系统)
8. [画笔系统](#8-画笔系统)
9. [形状修改器](#9-形状修改器)
10. [文本系统](#10-文本系统)
11. [Repeater](#11-repeater)
12. [Group](#12-group)
13. [图层样式](#13-图层样式)
14. [图层滤镜](#14-图层滤镜)
15. [混合模式](#15-混合模式)
16. [裁剪与遮罩](#16-裁剪与遮罩)
17. [CSS 不可达特性降级表](#17-css-不可达特性降级表)
18. [转换决策流程图](#18-转换决策流程图)

---

## 1. 概述

### 1.1 目标

将 PAGX 静态帧转换为等效的 HTML/CSS/SVG 输出，在主流浏览器中实现与 PAGX 渲染引擎视觉一致的表现。

### 1.2 设计原则

| 优先级 | 原则 | 描述 |
|--------|------|------|
| 1 | **视觉保真度** | 转换结果必须与 PAGX 渲染引擎输出视觉一致 |
| 2 | **CSS 优先** | 能用 CSS 表达的优先使用 CSS，减少 SVG 内联 |
| 3 | **语义保留** | 保留 `id`、`name`、`data-*` 属性，方便后续编辑和交互 |
| 4 | **可调试性** | 生成的 HTML 结构应清晰、可读，便于开发者检查 |

### 1.3 技术栈选择

| 场景 | 技术 | 原因 |
|------|------|------|
| 矩形、圆角矩形 | CSS `border-radius` | 原生支持，性能最优 |
| 椭圆 | CSS `border-radius: 50%` | 原生支持 |
| 任意路径 | SVG `<path>` | CSS 无法描述任意贝塞尔曲线 |
| 多边形/星形 | SVG `<path>` | CSS `clip-path: polygon()` 不支持圆角和曲线 |
| 线性/径向/锥形渐变 | CSS gradient | 原生支持 |
| 菱形渐变 | SVG `<filter>` | CSS 无等效实现 |
| 图片填充 | CSS `background-image` | 支持平铺模式 |
| 模糊/阴影 | CSS `filter`/`box-shadow` | 各向同性时优先 CSS |
| 各向异性模糊 | SVG `<feGaussianBlur>` | CSS `blur()` 仅支持均匀模糊 |
| 文本 | HTML 文本节点 | 保持可选中、可编辑 |
| 遮罩 | CSS `mask`/`clip-path` | 优先 CSS，复杂情况用 SVG |

### 1.4 坐标系统

PAGX 与 HTML/CSS 使用相同的坐标系统（原点左上角，X 右正，Y 下正）。角度均为顺时针正方向。

**需要注意的差异**：

| 特性 | PAGX | CSS |
|------|------|-----|
| 锥形渐变 0° | 右（3 点钟方向） | 上（12 点钟方向） |
| 变换锚点 | 各元素有独立锚点语义 | `transform-origin` |
| 2D 矩阵格式 | `a,b,c,d,tx,ty` | `matrix(a, b, c, d, tx, ty)` — 直接映射 |
| 3D 矩阵格式 | 列优先 16 值 | `matrix3d()` 列优先 16 值 — 直接映射 |

---

## 2. 总体架构

### 2.1 转换流程

```
PAGX XML → XML 解析 → 资源预处理（两遍解析）→ 图层树构建
→ VectorElement 累积-渲染处理 → HTML/CSS/SVG 生成 → SVG Defs 汇总
```

**两遍资源解析**：PAGX 支持前向引用，转换器必须实现：
1. **预注册遍**：扫描所有带 `id` 的元素，建立 ID→类型映射
2. **完整解析遍**：解析属性值，`@id` 引用从映射中查找

### 2.2 输出结构

```html
<div data-pagx-version="1.0" style="width:{w}px; height:{h}px; position:relative; overflow:hidden;">
  <svg style="position:absolute; width:0; height:0; overflow:hidden;">
    <defs><!-- 共享 SVG 定义：滤镜、遮罩、裁剪路径、渐变 --></defs>
  </svg>
  <!-- 图层树 -->
</div>
```

> 根元素通过 `data-pagx-version` 属性标识，不再使用 `class="pagx-root"` 标记类。

---

## 3. 根文档与画布

### 3.1 `<pagx>` → 根 `<div>`

```xml
<pagx version="1.0" width="400" height="400">
```
→
```html
<div data-pagx-version="1.0"
     style="position:relative; width:400px; height:400px; overflow:hidden;">
```

画布裁剪通过 `overflow: hidden` 实现。图层按文档顺序用 `position: absolute` 叠加，后出现的覆盖先出现的。

---

## 4. 资源系统

### 4.1 Image

- 相对路径 → 保持相对路径，由构建工具解析
- Data URI → 内联到 CSS `url(...)` 或 SVG `<image href="...">`

### 4.2 PathData

存入资源映射表。`@id` 引用时查表替换为 SVG path data 字符串。

### 4.3 Font 与 Glyph

| 情况 | 策略 |
|------|------|
| 矢量字形（`path`） | SVG `<path>` 按 `fontSize / unitsPerEm` 缩放 |
| 位图字形（`image`） | `<img>` 或 SVG `<image>` + 应用 `offset` |
| GlyphID=0 | 跳过不渲染 |

### 4.4 Composition

将 Composition 图层树转换为独立 HTML 片段，在 `<Layer composition="@id">` 引用处内联展开。Composition 的 `width/height` 定义内部坐标系大小。

---

## 5. 图层系统

### 5.1 Layer → `<div>`

每个 Layer 映射为一个 `<div>`，按渲染流水线六阶段组织子内容：

1. Layer Styles (below) — DropShadowStyle, BackgroundBlurStyle
2. Background Content — `placement="background"` 的 Fill/Stroke
3. Child Layers
4. Layer Styles (above) — InnerShadowStyle
5. Foreground Content — `placement="foreground"` 的 Fill/Stroke
6. Layer Filters — 应用于整个层

### 5.2 属性映射

| PAGX 属性 | CSS |
|-----------|-----|
| `visible="false"` | `display: none`（mask layer 的 visible 被忽略） |
| `alpha` | `opacity`（groupOpacity 影响应用方式，见下） |
| `x`, `y` | `transform: translate(x, y)` |
| `matrix="a,b,c,d,tx,ty"` | `transform: matrix(a, b, c, d, tx, ty)` — 直接映射 |
| `matrix3D` (16值) | `transform: matrix3d(...)` — 直接映射 |
| `preserve3D="true"` | `transform-style: preserve-3d` |
| `blendMode` | `mix-blend-mode` |
| `scrollRect` | 见第 16 节 |
| `mask` | 见第 16 节 |
| `antiAlias="false"` | `shape-rendering: crispEdges; image-rendering: pixelated` |

**变换优先级**：`matrix3D` > `matrix` > `x/y`（高优先级覆盖低优先级）。

### 5.3 groupOpacity

| `groupOpacity` | 行为 | CSS 策略 |
|----------------|------|---------|
| `false`（默认） | alpha 独立应用到每个子元素 | 将 opacity 分别设在每个子渲染元素上，父容器 opacity=1 |
| `true` | 所有内容先合成再整体应用 alpha | opacity 设在父 `<div>` 上（CSS 自然行为） |

### 5.4 passThroughBackground

`passThroughBackground=false` 时，图层创建独立的合成上下文，效果不穿透到背景。CSS 通过 `isolation: isolate` 实现。

---

## 6. 几何元素

### 6.1 转换策略总览

| 元素 | 简单情况（CSS） | 复杂情况（SVG） |
|------|----------------|----------------|
| Rectangle | `<div>` + `border-radius` | 有修改器时 → SVG `<path>` |
| Ellipse | `<div>` + `border-radius: 50%` | 同上 |
| Polystar | 无 CSS 等效 | 始终 SVG `<path>` |
| Path | 仅裁剪时 `clip-path: path()` | 始终 SVG `<path>` |
| Text | `<span>`/`<div>` | GlyphRun 预排版 → SVG `<path>` 逐字形 |

**必须回退 SVG 的条件**：存在形状修改器、多几何共享画笔、非标准描边（inside/outside、dashAdaptive）、Repeater 复制。

### 6.2 Rectangle

**中心点定位转换**：
```
left = position.x - size.width / 2
top  = position.y - size.height / 2
```

**圆角限制**：`effectiveRoundness = min(roundness, width/2, height/2)`

CSS 路径：
```html
<div style="position:absolute; left:{left}px; top:{top}px;
            width:{w}px; height:{h}px; border-radius:{roundness}px;">
```

路径起点：右上角，顺时针。

### 6.3 Ellipse

```
left = position.x - size.width / 2
top  = position.y - size.height / 2
```
→ `border-radius: 50%`。路径起点：右中点（3 点钟位置）。

### 6.4 Polystar

始终生成 SVG `<path>`。顶点计算：

**多边形**：`angle = rotation + (i / pointCount) × 360°`，仅用 outerRadius。

**星形**：外/内顶点交替，用 outerRadius 和 innerRadius。

**边界情况**：
- `pointCount <= 0` → 不生成路径
- 小数 `pointCount`（如 5.5）→ 最后一个顶点按小数比例绘制
- `outerRoundness`/`innerRoundness` 通过贝塞尔控制点实现（0=尖角，1=圆滑）

### 6.5 Path

`data` 为 SVG 路径字符串或 `@resourceId`。`reversed="true"` 需预处理反转路径方向。直接输出 SVG `<path d="...">`。

---

## 7. 颜色源系统

### 7.1 颜色值解析

| 格式 | CSS 映射 |
|------|---------|
| `#RGB` → `#RRGGBB` | 直接使用 |
| `#RRGGBB` | 直接使用 |
| `#RRGGBBAA` | CSS Color Level 4 直接使用，或降级为 `rgba()` |
| `srgb(r,g,b)` | `color(srgb r g b)` 或 `rgb(r×255, g×255, b×255)` |
| `srgb(r,g,b,a)` | `color(srgb r g b / a)` 或 `rgba(r×255, g×255, b×255, a)` |
| `p3(r,g,b)` | `color(display-p3 r g b)` — 需 CSS Color Level 4 |
| `p3(r,g,b,a)` | `color(display-p3 r g b / a)` |
| `@resourceId` | 查资源映射表 |

**P3 降级**：不支持时将 P3 转换为 sRGB 最近色值。P3 分量可能超出 [0,1]。SVG 属性（`stop-color`、`flood-color`、`fill`、`stroke`）不支持 CSS `color()` 函数，DisplayP3 颜色会近似为 sRGB hex 值。

**属性优先级**：Fill/Stroke 同时有 `color` 属性和子颜色源元素时，**子元素优先**。

### 7.2 LinearGradient

```xml
<LinearGradient startPoint="0,0" endPoint="100,100">
  <ColorStop offset="0" color="#FF0000"/>
  <ColorStop offset="1" color="#0000FF"/>
</LinearGradient>
```

角度计算：`css_angle = atan2(dy, dx) × 180/π + 90°`

→ `background: linear-gradient(135deg, #FF0000, #0000FF)`

**带 matrix**：将 matrix 应用到 startPoint/endPoint 计算角度。若 matrix 含缩放或平移分量，CSS `linear-gradient` 无法精确表达渐变的起止位置和长度，降级为 SVG `<linearGradient gradientTransform="...">`。

**坐标系**：渐变坐标相对于几何元素本地原点。外部变换同时影响几何和渐变。

### 7.3 RadialGradient

→ `radial-gradient(circle {radius}px at {cx}px {cy}px, ...)`

**带 matrix**：可能将圆变为旋转椭圆。CSS 支持椭圆（`ellipse rx ry at cx cy`）但不支持旋转椭圆 → 降级 SVG `<radialGradient gradientTransform="...">`。

### 7.4 ConicGradient

**关键差异**：PAGX 0° 朝右，CSS 0° 朝上。转换公式：`css_angle = pagx_angle + 90°`

```css
background: conic-gradient(from {startAngle+90}deg at {cx}px {cy}px, ...);
```

**部分角度范围** (`startAngle ≠ 0` 或 `endAngle ≠ 360`)：将 offset 映射为角度 `css_stop_angle = (startAngle + 90) + offset × (endAngle - startAngle)`。

**带 matrix**：CSS `conic-gradient` 不支持渐变变换。SVG 路径中使用 `<pattern>` + `<foreignObject>` 嵌入 CSS conic-gradient 实现。

### 7.5 DiamondGradient

**CSS 无等效**。使用切比雪夫距离 `max(|dx|, |dy|) / radius`。

实现方案：SVG `<radialGradient>` 近似。DiamondGradient 在 `canCSS` 中始终返回 false，强制走 SVG 路径，使用 `<radialGradient>` + `gradientTransform` 用圆形径向渐变近似菱形渐变（视觉有差异，特别是对角线方向）。

### 7.6 ImagePattern

**CSS 路径**（`renderCSSDiv`）：

| PAGX tileModeX/Y | CSS `background-repeat` |
|-------------------|------------------------|
| `repeat` + `repeat` | `repeat` |
| `repeat` + `clamp` | `repeat no-repeat`（clamp 降级为 no-repeat） |
| `mirror` | 降级为 `repeat`（精确镜像未实现） |
| `clamp` | `no-repeat`（CSS 无原生 clamp） |
| `decal` | `no-repeat`（超出透明） |

**SVG 路径**（`colorToSVGFill`）：使用 SVG `<pattern>` + `<foreignObject>` + CSS `background-image` 实现，与 ConicGradient 的 SVG fallback 策略一致。tileMode/filterMode/matrix 通过 CSS background 属性表达。

**matrix**：CSS 通过 `background-size`/`background-position` 部分模拟缩放和平移。旋转/倾斜暂不支持。

**filterMode**：`nearest` → `image-rendering: pixelated`，`linear` → 默认（`image-rendering: auto`）。

**mipmapMode**：CSS/SVG 无直接控制，由浏览器内部处理。

**图片 MIME 类型检测**：内联 base64 图片通过 magic bytes 检测格式（PNG/JPEG/WebP/GIF），不再硬编码为 `image/png`。

### 7.7 ColorStop 边界处理

| 情况 | 处理 |
|------|------|
| `offset < 0` | 视为 0 |
| `offset > 1` | 视为 1 |
| 无 offset=0 的 stop | 第一个 stop 颜色延伸到 0 |
| 无 offset=1 的 stop | 最后一个 stop 颜色延伸到 1 |

CSS 渐变行为一致。

---

## 8. 画笔系统

### 8.1 累积-渲染模型

Fill/Stroke 渲染当前时刻所有已累积几何。画笔不清除几何列表。

**简单情况**（一几何+一画笔）：直接在 CSS div 上设 background/border。

**复杂情况**（多几何或多画笔）：每个画笔生成独立的叠加 `<div>` 或 SVG 元素。

### 8.2 Fill

| PAGX | CSS/SVG |
|------|---------|
| `color`(SolidColor) | `background-color` |
| `color`(Gradient) | `background: {gradient}` |
| `color`(ImagePattern) | `background-image` |
| `alpha` | `opacity`（渐变/图案填充时乘入 div 的 opacity） |
| `blendMode` | `mix-blend-mode` |
| `fillRule=winding` | SVG `fill-rule="nonzero"` |
| `fillRule=evenOdd` | SVG `fill-rule="evenodd"` — CSS 无直接等效，必须 SVG |
| `placement` | 决定渲染层在子图层上方或下方 |

**placement**：`background`（默认）→ 子图层之前输出；`foreground` → 子图层之后输出。

**文本填充**：SolidColor 使用 CSS `color` 属性。渐变/图案填充使用 `background-clip: text` + `-webkit-text-fill-color: transparent` 实现。

### 8.3 Stroke

| PAGX | SVG |
|------|-----|
| `color` | `stroke` |
| `width` | `stroke-width` |
| `cap`: butt/round/square | `stroke-linecap`: butt/round/square |
| `join`: miter/round/bevel | `stroke-linejoin`: miter/round/bevel |
| `miterLimit` | `stroke-miterlimit` |
| `dashes` | `stroke-dasharray`（逗号→空格） |
| `dashOffset` | `stroke-dashoffset` |

**StrokeAlign**：

| `align` | 实现 |
|---------|------|
| `center` | 标准 SVG stroke |
| `inside` | 双倍宽度 + 用原始形状 `clip-path` 裁剪 |
| `outside` | 双倍宽度 + 用原始形状反向 mask 裁剪 |

**dashAdaptive**：CSS/SVG 无直接等效。实现方式：使用 `pathLength` 属性配合 dasharray 缩放。

算法：
1. 计算路径弧长 `pathLength`
2. 计算原始 dash pattern 一个周期的总长度 `patternLength = sum(dashes)`
3. 计算路径可容纳的完整周期数 `n = round(pathLength / patternLength)`（四舍五入确保整数个周期）
4. 缩放因子 `scaleFactor = pathLength / (n × patternLength)`
5. 输出 `stroke-dasharray` = 每个 dash/gap 值乘以 `scaleFactor`

当 `n = 0`（路径过短）时不绘制 dash。

**CSS border 适用条件**：几何为 Rectangle + `align=center` + 无 dash + 默认 cap/join。注意 CSS border 是 inside stroke，需调整定位补偿。**通常推荐直接用 SVG stroke**。

**文本描边**：运行时文本的 SolidColor 描边使用 `-webkit-text-stroke: {width}px {color}` + `paint-order: stroke fill`。非 SolidColor 描边暂不支持。预排版文本（GlyphRun）使用 SVG `<path>` 的标准 `stroke` 属性。

---

## 9. 形状修改器

遇到形状修改器时**必须使用 SVG**。

### 9.1 TrimPath

策略 1（仅 Stroke）：SVG `stroke-dasharray`/`stroke-dashoffset` 模拟。

策略 2（通用）：预计算裁剪后路径数据。

#### Separate 模式

每个形状独立裁剪，每条路径用相同的 start/end 参数。实现方式：

- 每条 `<path>` 设 `pathLength="1"`
- `stroke-dasharray` = `{end - start} {1}` （可见段长度 + 足够大的间隔）
- `stroke-dashoffset` = `{-(start + offsetFrac)}`，其中 `offsetFrac = offset / 360`

#### Continuous 模式

所有累积路径视为一条连续路径，按总长度比例裁剪。算法：

1. 计算每条路径弧长：`lengths[i]`，`totalLength = sum(lengths)`
2. 计算每条路径在总长中的归一化范围：
   ```
   cumStart[0] = 0
   cumStart[i] = cumStart[i-1] + lengths[i-1] / totalLength
   cumEnd[i]   = cumStart[i] + lengths[i] / totalLength
   ```
3. 对全局 `[start, end]`（已应用 offset 偏移），与每条路径的 `[cumStart[i], cumEnd[i]]` 求交集
4. 交集为空 → 该路径不渲染；交集非空 → 映射回该路径本地的 [0,1] 范围，作为该路径的局部 trim 参数
5. 每条路径仍用 `pathLength="1"` + `stroke-dasharray`/`stroke-dashoffset` 表达局部 trim

#### 边界情况

| 情况 | 处理 |
|------|------|
| `start > end` | 交换 start/end，镜像反转所有路径方向，取互补段 |
| 范围超出 [0,1] | 环绕处理：对路径起点做模运算 |
| `offset` | 以角度计，360° = 一个完整路径周期，转换为小数偏移 `offsetFrac = offset / 360` |
| 路径长度 = 0 | 不操作 |
| Fill + TrimPath | TrimPath 仅影响 Stroke 渲染；Fill 不受 TrimPath 影响 |

### 9.2 RoundCorner

在路径数据层面预处理：将尖角替换为贝塞尔曲线圆弧。不影响 CSS/SVG 的选择——修改后的路径仍按常规路径渲染。

**算法**：

1. 遍历路径每个顶点，检测是否为尖角（前后线段非光滑连接，即不存在与顶点重合的控制点）
2. 计算有效半径：`effectiveRadius = min(radius, edge1Length / 2, edge2Length / 2)`
3. 在前后两条边上，从顶点各向前截取 `effectiveRadius` 距离，得到两个新端点 P1、P2
4. 用三次贝塞尔曲线连接 P1 → P2，控制点为：
   ```
   CP1 = P1 + (vertex - P1) × kappa
   CP2 = P2 + (vertex - P2) × kappa
   kappa = 4 × (sqrt(2) - 1) / 3 ≈ 0.5523
   ```
   kappa 值使 90° 圆弧的贝塞尔近似误差最小。对于非 90° 角的情况，kappa 可按角度调整，但 0.5523 是通用近似值
5. 替换原顶点为 `lineTo(P1) curveTo(CP1, CP2, P2)` 段

**边界情况**：

| 情况 | 处理 |
|------|------|
| `radius <= 0` | 不操作 |
| 相邻边过短 | radius 自动限制为相邻边长度的一半 |
| 光滑顶点（已有控制点） | 跳过，仅处理尖角 |
| 闭合路径的首尾顶点 | 同样处理，首尾边参与计算 |

### 9.3 MergePath

**重要**：MergePath **清除之前所有累积的 Fill/Stroke**。合并后变换重置为单位矩阵。HTML 中 MergePath 之前的渲染层需移除。

#### Append 模式

多子路径合并为一个 `<path d="{path1} {path2} ...">`，直接拼接路径数据。

#### Union 模式（并集）

将所有路径合并为一个 `<path>`，统一路径方向为顺时针，设 `fill-rule="nonzero"`。由于所有子路径方向一致，nonzero 规则产生并集效果。

**算法**：
1. 将每条路径的变换应用到顶点坐标（烘焙变换）
2. 检测每条路径的绕行方向，非顺时针的路径做反转
3. 拼接所有路径数据为一个 `d` 属性
4. 设 `fill-rule="nonzero"`

#### Xor 模式（异或）

将所有路径合并为一个 `<path>`，设 `fill-rule="evenodd"`。evenodd 规则下，重叠奇数次的区域填充，偶数次的区域不填充，产生异或效果。

**算法**：
1. 将每条路径的变换应用到顶点坐标
2. 拼接所有路径数据为一个 `d` 属性
3. 设 `fill-rule="evenodd"`

#### Intersect 模式（交集）

使用 SVG `<clipPath>` 嵌套实现。将第一个路径作为基础形状，后续路径依次作为 `clip-path` 裁剪。

```html
<svg ...>
  <defs>
    <clipPath id="mp-clip-1"><path d="{path2}"/></clipPath>
    <!-- 多个路径时继续嵌套 -->
  </defs>
  <path d="{path1}" clip-path="url(#mp-clip-1)" fill="..." />
</svg>
```

多路径交集需要嵌套 clipPath（clipPath 内的元素也可以引用另一个 clipPath）。

#### Difference 模式（差集）

用第一条路径减去后续路径。实现方式：

1. 将第一条路径设为顺时针方向，后续路径设为逆时针方向
2. 合并为一个 `<path>`，设 `fill-rule="nonzero"`
3. 顺时针区域（第一条路径）的 winding number 为正，逆时针区域（被减路径）的 winding number 为负，重叠区域 winding number 抵消为 0，不填充

**算法**：
1. 将每条路径的变换应用到顶点坐标
2. 第一条路径确保顺时针方向
3. 后续路径全部反转为逆时针方向
4. 拼接所有路径数据为一个 `d` 属性
5. 设 `fill-rule="nonzero"`

### 9.4 文本强制转换

Text 遇到 TrimPath/RoundCorner/MergePath → 所有字形合并为单一 Path（emoji 丢弃）。不可逆。原本 HTML 文本必须转为 SVG `<path>`。

---

## 10. 文本系统

### 10.1 Text（运行时排版）

```xml
<Text text="Hello" position="100,200" fontFamily="Arial" fontSize="24" textAnchor="start"/>
```
→
```html
<span style="position:absolute; left:100px; top:{200 - ascent}px;
             font-family:'Arial'; font-size:24px; white-space:pre;">Hello</span>
```

**position.y 是基线**：CSS 文本从顶部开始，需减去 ascent。近似 `top = y - fontSize × 0.8`（0.8 为经验系数，适用于大多数 Latin 字体，实际 ascent/fontSize 比值因字体而异，范围约 0.7~0.9，精度约 ±3px）。PAGX Font 资源中不含 ascent/descent 信息，无法精确计算。

**textAnchor**：`start`=不偏移，`center`=`translateX(-50%)`，`end`=`translateX(-100%)`。

**fauxBold**：`-webkit-text-stroke: 0.02em currentColor`（非 font-weight:bold）。

**fauxItalic**：`transform: skewX(-12deg)`。

**fontFamily**：输出为 CSS `font-family: '{name}'`，名称中的单引号会被转义为 `\'`。

**CDATA 内容**：HTML 转义特殊字符。多行文本用 `white-space: pre-wrap`。

### 10.2 GlyphRun（预排版）

逐字形 SVG `<path>`。位置：`finalX[i] = x + xOffsets[i] + positions[i].x`，`finalY[i] = y + positions[i].y`。缩放：`scale = fontSize / unitsPerEm`。

逐字形变换顺序（同 TextModifier）：translate(-anchor) → scale → skew → rotate → translate(anchor) → translate(position)。anchor 默认 `(advance×0.5, 0)`。

GlyphID=0 不渲染。位图字形用 `<image>`。

### 10.3 TextBox

| PAGX | CSS |
|------|-----|
| `position` | `left`/`top` |
| `size` | `width`/`height`（0 → `auto`） |
| `textAlign`: start/center/end/justify | `text-align` |
| `paragraphAlign`: near/middle/far | Flexbox `justify-content`: flex-start/center/flex-end |
| `writingMode`: horizontal/vertical | `writing-mode`: horizontal-tb/vertical-rl |
| `lineHeight`: 0(auto)/value | `line-height`: normal/value+px |
| `wordWrap` | `word-wrap: break-word` |
| `overflow`: visible/hidden | `overflow` — PAGX hidden 是整行丢弃，CSS 是裁剪 |

**size=0 边界**：对应维度设 `auto`，wordWrap/overflow 在该维度无效。

TextBox 是预排版节点，已有 GlyphRun 数据时跳过。

### 10.4 TextModifier + RangeSelector

TextModifier 对每个字形计算独立的变换和样式覆盖。核心是 RangeSelector 计算每个字形的 factor 值（范围 -1 ~ 1），然后按 factor 缩放变换参数。

#### HTML 输出结构

**运行时文本**：将文本拆分为单字符，每个字符包装为独立 `<span style="display:inline-block">`，应用独立 transform 和样式：

```html
<div class="pagx-textbox" style="...">
  <span style="display:inline-block; transform:...; opacity:...; color:...;">H</span>
  <span style="display:inline-block; transform:...; opacity:...; color:...;">e</span>
  ...
</div>
```

**GlyphRun 预排版**：每个字形已是独立 SVG 元素，在已有 `<g>` 或 `<path>` 上叠加 factor 计算的变换。

#### RangeSelector Factor 计算

1. **选区范围**：根据 `start`、`end`、`offset` 计算当前选中的字形范围
   - `unit=percentage`：start/end 为 [0,1]，映射到字形索引
   - `unit=index`：start/end 直接为字形索引
   - `offset` 偏移整个选区，支持超 [0,1] 环绕

2. **Shape 插值函数**：决定选区内 factor 的分布曲线
   | shape | 公式（t 为选区内归一化位置 [0,1]） |
   |-------|------|
   | `square` | `1`（选区内均匀） |
   | `rampUp` | `t` |
   | `rampDown` | `1 - t` |
   | `triangle` | `t < 0.5 ? 2t : 2(1-t)` |
   | `round` | `sin(t × π)` |
   | `smooth` | `0.5 - 0.5 × cos(t × π)` |

3. **easeIn/easeOut**：可选的缓动调节，在 shape 基础上进一步平滑过渡

4. **多 Selector 组合**：按 `mode` 组合各 Selector 的 rawInfluence：
   | mode | 公式 |
   |------|------|
   | `add` | `a + b` |
   | `subtract` | `b ≥ 0 ? a × (1 − b) : a × (−1 − b)` |
   | `intersect` | `a × b` |
   | `min` | `min(a, b)` |
   | `max` | `max(a, b)` |
   | `difference` | `|a − b|` |

5. **最终 factor**：`factor = clamp(combined_influence × weight, -1, 1)`

#### 变换应用

逐字形的 CSS transform（从右往左读）：

```css
transform:
  translate({position.x × f}px, {position.y × f}px)   /* 6. 位移 */
  translate({anchor.x × f}px, {anchor.y × f}px)        /* 5. 回到锚点 */
  rotate({rotation × f}deg)                             /* 4. 旋转 */
  rotate({skewAxis × f}deg) skewX({skew × f}deg) rotate({-skewAxis × f}deg)  /* 3. 斜切 */
  scale({1 + (scale.x - 1) × f}, {1 + (scale.y - 1) × f})  /* 2. 缩放 */
  translate({-anchor.x × f}px, {-anchor.y × f}px);     /* 1. 移到锚点原点 */
```

其中 `f` 为 factor，anchor 默认值为 `(advance × 0.5, 0)`（字形水平中心，基线位置）。

#### 样式覆盖

| 属性 | 公式 |
|------|------|
| alpha | `finalAlpha = originalAlpha × max(0, 1 + (alpha - 1) × |factor|)` |
| fillColor | `blendFactor = fillColor.alpha × |factor|`，与原色做 alpha blending |
| strokeColor | 同 fillColor |
| strokeWidth | `finalWidth = originalWidth + (strokeWidth - originalWidth) × |factor|` |

### 10.5 TextPath

PAGX TextPath 比 SVG `<textPath>` 语义更丰富（baseline 概念、forceAlignment、perpendicular 控制）。不使用 SVG `<textPath>` 元素，而是**预计算每个字形在路径上的位置和旋转角度**，逐字形定位输出。

#### 属性

| PAGX 属性 | 说明 |
|-----------|------|
| `path` | SVG 路径数据或 `@resourceId` 引用 |
| `baselineOrigin` | 基线起始点（默认 `0,0`） |
| `baselineAngle` | 基线角度（默认 `0`，水平从左到右） |
| `firstMargin` | 起始边距，路径起点向内偏移 |
| `lastMargin` | 结束边距，路径终点向内偏移 |
| `perpendicular` | 字形是否垂直于路径切线（默认 `true`） |
| `reversed` | 是否反转路径方向（默认 `false`） |
| `forceAlignment` | 是否拉伸字距填满路径（默认 `false`） |

#### 算法

1. **路径弧长参数化**：解析 SVG path data，对贝塞尔曲线递归细分，构建弧长→坐标的查找表（LUT）。精度要求：每段曲线至少细分 8-16 次以确保弧长误差 < 0.5px

2. **字形基线距离计算**：
   - 基线是一条从 `baselineOrigin` 出发、角度为 `baselineAngle` 的直线
   - 每个字形在基线上的位置由其排版后的水平坐标决定
   - `reversed=true` 时反转路径方向

3. **路径投影**：将字形沿基线的距离映射为路径弧长位置
   ```
   arcPos = firstMargin + glyphBaselineDistance
   ```
   在弧长 `arcPos` 处查 LUT，得到路径上的坐标 `(px, py)` 和切线方向 `tangentAngle`

4. **forceAlignment 处理**：
   - 计算有效路径长度 `effectiveLength = pathLength - firstMargin - lastMargin`
   - 计算文本总宽度 `totalAdvance = sum(glyph.advance)`
   - 字距缩放因子 `spacingScale = effectiveLength / totalAdvance`
   - 每个字形的基线距离乘以 spacingScale

5. **字形定位**：
   - 位置：路径上的 `(px, py)` + 垂直于路径的偏移（字形原始 y 偏移沿法线方向映射）
   - 旋转：`perpendicular=true` 时字形旋转 `tangentAngle`；`false` 时保持原始方向

6. **闭合路径环绕**：闭合路径上超出范围的字形环绕到路径另一端

#### HTML 输出

**运行时文本**：每个字符用 `<span style="position:absolute; transform:translate(px, py) rotate(angle);">`

**GlyphRun**：每个字形用 SVG `<g transform="translate(px,py) rotate(angle)"><path d="..."/></g>`

### 10.6 富文本

多 Text + 独立 Fill/Stroke + TextBox 统一排版 → HTML 中多 `<span>` 在同一 textbox `<div>` 内。

---

## 11. Repeater

### 11.1 转换策略

对每个副本生成独立 HTML 元素。变换计算（`progress = i + offset`）：

```
1. translate(-anchor)
2. scale(scale ^ progress)       — 指数缩放
3. rotate(rotation × progress)   — 线性旋转
4. translate(position × progress)— 线性平移
5. translate(anchor)
```

透明度插值：`alpha = lerp(startAlpha, endAlpha, progress / ceil(copies))`

### 11.2 RepeaterOrder

| `order` | HTML 输出顺序 |
|---------|-------------|
| `belowOriginal` | 副本 0 最后输出（最上层） |
| `aboveOriginal` | 副本 N-1 最后输出（最上层） |

### 11.3 小数 copies

`copies=3.5` → 生成 4 个副本，最后一个 opacity × 0.5。

### 11.4 边界情况

| 情况 | 处理 |
|------|------|
| `copies < 0` | 不操作 |
| `copies = 0` | 清除所有累积内容和已渲染样式 |
| Repeater 同时复制 Path 和字形列表 | 不触发文本到路径转换 |

---

## 12. Group

### 12.1 转换

→ `<div class="pagx-group">`，变换顺序（CSS transform **从右往左读**）：

```css
transform: translate({pos.x}px, {pos.y}px)
           rotate({rotation}deg)
           /* skew: rotate(skewAxis) skewX(skew) rotate(-skewAxis) */
           scale({scale.x}, {scale.y})
           translate({-anchor.x}px, {-anchor.y}px);
opacity: {alpha};
```

### 12.2 作用域隔离

组内几何/画笔/修改器仅在组内生效。子 Group 完成后几何向父传播。HTML 中通过将渲染元素作为组 `<div>` 子元素实现。

---

## 13. 图层样式

样式基于**不透明图层内容**计算（半透明→全不透明，全透明保持）。

### 13.1 DropShadowStyle

**均匀模糊** (blurX==blurY)：CSS `filter: drop-shadow(...)` 或矩形用 `box-shadow`。

**各向异性模糊** (blurX≠blurY)：SVG `<feGaussianBlur stdDeviation="{blurX/2} {blurY/2}">`。

**showBehindLayer**：`true`=完整阴影，`false`=用图层轮廓擦除遮挡部分（SVG `feComposite operator="out"`）。

渲染位置：图层内容**下方**。

### 13.2 InnerShadowStyle

矩形/圆角矩形：CSS `box-shadow: inset ...`。

通用：SVG filter（偏移→反向→模糊→裁剪到内部→着色）。

渲染位置：图层内容**上方**。

### 13.3 BackgroundBlurStyle

CSS `backdrop-filter: blur({blur}px)`。

**限制**：CSS 仅支持均匀模糊。各向异性降级用 `(blurX+blurY)/2` 近似。

渲染位置：图层内容**下方**。

### 13.4 excludeChildEffects

`true` 时子图层的样式和滤镜效果不参与当前图层样式的计算输入（但子图层的基本渲染结果仍包含在内）。

**CSS 无直接等效**。精确实现需要将子图层先渲染到一个不带 filter/style 的中间层，再用该中间层作为样式计算输入——这需要额外的 DOM 嵌套和渲染隔离，复杂度高且该特性在实际设计中使用极少。

**降级方案**：忽略此属性，所有场景统一按 `false`（默认值）处理。

### 13.5 LayerStyle blendMode

每个 LayerStyle（DropShadowStyle、InnerShadowStyle、BackgroundBlurStyle）可以有独立的 `blendMode` 属性。

**实现方案**：将每个带非 normal blendMode 的 LayerStyle 渲染为独立的 `<div>` 层，在该层上设置 `mix-blend-mode`：

- **DropShadowStyle**：独立 `<div>` 在图层内容下方，承载阴影效果，设 `mix-blend-mode: {blendMode}`
- **InnerShadowStyle**：独立 `<div>` 在图层内容上方，承载内阴影效果，设 `mix-blend-mode: {blendMode}`
- **BackgroundBlurStyle**：独立 `<div>` 用 `backdrop-filter`，设 `mix-blend-mode: {blendMode}`

当 `blendMode=normal` 时（默认），无需额外处理。

```html
<!-- DropShadowStyle with blendMode=multiply -->
<div style="mix-blend-mode:multiply;">
  <div style="filter:drop-shadow(...);"><!-- 阴影载体 --></div>
</div>
<!-- 图层内容 -->
<div>...</div>
```

---

## 14. 图层滤镜

滤镜是渲染最后阶段，链式应用。CSS `filter` 支持链式：`filter: f1() f2() f3()`。

### 14.1 BlurFilter

| 情况 | CSS |
|------|-----|
| blurX == blurY | `filter: blur({blur}px)` |
| blurX ≠ blurY | SVG `<feGaussianBlur stdDeviation="{blurX/2} {blurY/2}">` 通过 `filter: url(#...)` |

`tileMode`：CSS/SVG 模糊的边界行为固定，无法精确映射 PAGX 的 `decal`/`mirror` 等模式。

### 14.2 DropShadowFilter

```css
filter: drop-shadow({offsetX}px {offsetY}px {blur}px {color});
```

`shadowOnly="true"`：仅输出阴影，不保留原始内容 → 需 SVG filter 单独实现。

**与 DropShadowStyle 的区别**：Filter 基于原始渲染内容（保留半透明），Style 基于不透明图层内容。

### 14.3 InnerShadowFilter

CSS 无直接等效。SVG filter 实现，`shadowOnly` 同上处理。

### 14.4 BlendFilter

SVG filter 实现：`<feFlood>` 创建颜色层 + `<feBlend>` 与源图形混合。

```svg
<filter id="bf1">
  <feFlood flood-color="{color}" flood-opacity="{alpha}" result="bFlood"/>
  <feBlend in="SourceGraphic" in2="bFlood" mode="{blendMode}"/>
</filter>
```

通过 `filter: url(#bf1)` 应用到图层。`blendMode` 直接映射为 `<feBlend mode="">` 值，包括 `plus-lighter` 和 `darken`（PlusDarker 近似）。

### 14.5 ColorMatrixFilter

CSS `filter: url(#...)` + SVG `<feColorMatrix>`:

```svg
<filter id="cm1">
  <feColorMatrix type="matrix" values="{m[0]} {m[1]} {m[2]} {m[3]} {m[4]}
                                        {m[5]} {m[6]} {m[7]} {m[8]} {m[9]}
                                        {m[10]} {m[11]} {m[12]} {m[13]} {m[14]}
                                        {m[15]} {m[16]} {m[17]} {m[18]} {m[19]}"/>
</filter>
```

---

## 15. 混合模式

| PAGX | CSS `mix-blend-mode` |
|------|---------------------|
| `normal` | `normal` |
| `multiply` | `multiply` |
| `screen` | `screen` |
| `overlay` | `overlay` |
| `darken` | `darken` |
| `lighten` | `lighten` |
| `colorDodge` | `color-dodge` |
| `colorBurn` | `color-burn` |
| `hardLight` | `hard-light` |
| `softLight` | `soft-light` |
| `difference` | `difference` |
| `exclusion` | `exclusion` |
| `hue` | `hue` |
| `saturation` | `saturation` |
| `color` | `color` |
| `luminosity` | `luminosity` |
| `plusLighter` | `plus-lighter` |
| `plusDarker` | **无精确 CSS 等效** → 近似为 `darken` |

**plusDarker 降级**：`S + D - 1` 公式在 CSS 中无原生支持。当前实现使用 `mix-blend-mode: darken` 作为近似，包括图层、画笔、SVG feBlend 中的所有 blend mode 处理均统一使用 `BlendModeToMixBlendMode()` 函数。

---

## 16. 裁剪与遮罩

### 16.1 scrollRect

```xml
<Layer scrollRect="10,20,300,200"/>
```

实现为嵌套两层 `<div>`：外层定义可视区域大小和 `overflow:hidden`，内层用反向偏移还原内容坐标：

```html
<div style="position:absolute; left:10px; top:20px; width:300px; height:200px; overflow:hidden;">
  <div style="position:relative; left:-10px; top:-20px;">
    <!-- 图层内容 -->
  </div>
</div>
```

scrollRect 在图层本地坐标系中定义。

### 16.2 Mask

```xml
<Layer id="maskLayer" visible="false">
  <Polystar .../>
  <Fill color="#FFF"/>
</Layer>
<Layer mask="@maskLayer" maskType="alpha">
  ...
</Layer>
```

**规则**：Mask layer 自身不渲染（visible 被忽略）。Mask 的变换不影响被遮罩图层。

| `maskType` | CSS/SVG 实现 |
|------------|-------------|
| `alpha` | SVG `<mask>` 使用 mask layer 的 alpha 通道 |
| `luminance` | SVG `<mask>` + `mask-type: luminance` |
| `contour` | CSS `clip-path: url(#...)` 或 SVG `<clipPath>` |

**Alpha mask 实现**：
```css
mask: url(#mask-1);
-webkit-mask: url(#mask-1);
```

```svg
<mask id="mask-1">
  <!-- mask layer 的渲染内容，保留原始 fill 颜色源 -->
  <path d="..." fill="#FFFFFF"/>
</mask>
```

**Luminance mask 实现**：

Luminance mask 将 mask 图层的 RGB 亮度值转换为遮罩透明度（黑色=全透明，白色=全不透明）。`<mask>` 默认使用 luminance 模式，因此**必须完整渲染 mask 图层的 Fill 颜色源**（包括渐变、图案等），不能统一填充白色。

```svg
<mask id="mask-2" mask-type="luminance">
  <!-- 保留原始 Fill 的颜色源 -->
  <path d="..." fill="url(#gradient-in-mask)"/>
</mask>
```

实现要点：
1. 在 `writeMaskDef` 中，对 mask 图层的 VectorElement 执行完整的累积-渲染流程
2. 将 Fill 的颜色源（SolidColor、Gradient、ImagePattern）正确应用到 SVG mask 内容
3. Gradient 需要在 `<defs>` 中定义 SVG 渐变元素供 mask 内部引用
4. Fill 的 alpha 也需要保留，它影响亮度值

**Contour mask**：二值遮罩，可用 `<clipPath>` 更高效。

---

## 17. CSS 不可达特性降级表

| PAGX 特性 | CSS 可达性 | 降级方案 |
|-----------|-----------|---------|
| DiamondGradient | ❌ | SVG `<radialGradient>` 圆形近似 |
| 各向异性模糊 (blurX≠blurY) | ❌ | SVG `<feGaussianBlur>` |
| `showBehindLayer="false"` | ❌ | SVG filter (feComposite out) |
| InnerShadowFilter | ❌ | SVG filter 链 |
| `shadowOnly="true"` | ❌ | SVG filter |
| ColorMatrixFilter | ❌ | SVG `<feColorMatrix>` |
| `fillRule="evenOdd"` | ❌ (CSS bg) | SVG `fill-rule="evenodd"` |
| `align="inside/outside"` | ❌ | SVG 双倍宽度 + clip/mask |
| `dashAdaptive` | ⚠️ 部分 | 预计算路径长度，缩放 dasharray 值 |
| `plusDarker` blend mode | ❌ | 近似为 CSS `darken` |
| ImagePattern `mirror` | ⚠️ 部分 | 降级为 `repeat` |
| ImagePattern `clamp` | ❌ | 降级为 no-repeat |
| TextPath (完整语义) | ✅ | 预计算弧长参数化，逐字形定位输出 |
| TextModifier | ✅ | 逐字形 `<span>` 或 SVG `<g>`，独立 transform + 样式覆盖 |
| RoundCorner | ✅ | 路径数据预处理：贝塞尔曲线替换尖角 |
| MergePath (非 Append) | ✅ | union/xor 用 fill-rule；intersect 用 clipPath；difference 用路径方向反转 |
| TrimPath `continuous` | ✅ | 计算路径总弧长，按比例分配各子路径 trim 范围 |
| Repeater 小数 copies | ✅ | 最后副本 opacity × 小数部分 |
| `antiAlias="false"` | ⚠️ 部分 | `shape-rendering: crispEdges; image-rendering: pixelated` |
| `excludeChildEffects` | ❌ | 降级为忽略（按 false 处理） |
| `groupOpacity="false"` | ✅ | opacity 分配到每个子渲染元素 |
| ConicGradient + matrix | ⚠️ 部分 | SVG `<pattern>` + `<foreignObject>` + CSS conic-gradient |
| TileMode (blur filters) | ⚠️ 部分 | 浏览器固定边界行为 |
| DisplayP3 in SVG | ⚠️ 部分 | P3 通道值近似为 sRGB hex |
| LayerStyle.blendMode | ✅ | 每个 Style 渲染为独立 `<div>` 层，设 `mix-blend-mode` |
| Luminance mask 渐变 | ✅ | 完整渲染 mask 图层的 Fill 颜色源到 SVG `<mask>` 内 |

---

## 18. 转换决策流程图

```
对每个 Layer：
  │
  ├─ visible=false 且非 mask 引用？→ display:none，跳过
  │
  ├─ 有 composition 引用？→ 展开 Composition 内容（检测循环引用）
  │
  ├─ 处理 VectorElement 序列（累积-渲染模型）：
  │   │
  │   ├─ 遇到 Geometry（Rect/Ellipse/Polystar/Path/Text）→ 累积到列表
  │   │
  │   ├─ 遇到 Modifier：
  │   │   ├─ TrimPath：
  │   │   │   ├─ type=separate → 记录 trim 参数，每个几何独立裁剪
  │   │   │   └─ type=continuous → 计算所有路径弧长，按比例分配
  │   │   ├─ RoundCorner：
  │   │   │   └─ 对所有累积路径预处理：尖角→贝塞尔圆弧
  │   │   ├─ MergePath：
  │   │   │   ├─ 清除之前所有累积的 Fill/Stroke
  │   │   │   ├─ append → 拼接路径数据
  │   │   │   ├─ union → 统一路径方向 + fill-rule:nonzero
  │   │   │   ├─ xor → fill-rule:evenodd
  │   │   │   ├─ intersect → clipPath 嵌套
  │   │   │   └─ difference → 反转被减路径方向 + fill-rule:nonzero
  │   │   ├─ 列表中有 Text？→ 触发文本到路径转换
  │   │   └─ 变换所有累积 Path → 标记为 SVG 渲染
  │   │
  │   ├─ 遇到 Text Modifier：
  │   │   ├─ TextModifier → 计算 RangeSelector factor，逐字形拆分
  │   │   │   ├─ 运行时文本 → 每字符 <span style="display:inline-block; transform:...">
  │   │   │   └─ GlyphRun → 每字形 SVG <g> 叠加 factor 变换
  │   │   ├─ TextPath → 弧长参数化，逐字形计算路径上的位置和旋转
  │   │   │   ├─ 运行时文本 → 每字符 <span style="position:absolute; transform:...">
  │   │   │   └─ GlyphRun → 每字形 SVG <g transform="translate rotate">
  │   │   └─ TextBox → 应用排版属性（text-align/flexbox/writing-mode）
  │   │
  │   ├─ 遇到 Repeater → 复制所有累积内容 + 已渲染样式（不触发文本转路径）
  │   │
  │   ├─ 遇到 Group → 递归处理子作用域，完成后几何传播到父
  │   │
  │   └─ 遇到 Painter（Fill/Stroke）：
  │       │
  │       ├─ 判断能否用 CSS：
  │       │   ├─ 仅一个 Rectangle/Ellipse + 无修改器 + SolidColor/CSS 渐变
  │       │   │   + center stroke + 无 dash + 标准 fillRule？
  │       │   │   → CSS div + background/border
  │       │   └─ 否则 → SVG 渲染
  │       │
  │       └─ 按 placement 分到 background 或 foreground 输出队列
  │
  ├─ 生成 HTML：
  │   ├─ Layer Styles (below): DropShadow/BackgroundBlur
  │   │   └─ 有独立 blendMode？→ 包装为独立 <div> + mix-blend-mode
  │   ├─ Background Content
  │   ├─ Child Layers（递归）
  │   ├─ Layer Styles (above): InnerShadow
  │   │   └─ 有独立 blendMode？→ 包装为独立 <div> + mix-blend-mode
  │   ├─ Foreground Content
  │   └─ Layer Filters: filter 属性链
  │
  ├─ 应用 Layer 属性：transform/opacity/blend-mode/mask/scrollRect
  │
  └─ Mask 处理：
      ├─ alpha → SVG <mask>，完整渲染 mask 图层内容（保留颜色源）
      ├─ luminance → SVG <mask mask-type="luminance">，保留 Fill 颜色源用于亮度计算
      └─ contour → SVG <clipPath>
```

---

