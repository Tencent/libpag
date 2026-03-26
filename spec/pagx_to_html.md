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
<div class="pagx-root" style="width:{w}px; height:{h}px; position:relative; overflow:hidden;">
  <svg style="position:absolute; width:0; height:0; overflow:hidden;">
    <defs><!-- 共享 SVG 定义：滤镜、遮罩、裁剪路径、渐变 --></defs>
  </svg>
  <!-- 图层树 -->
</div>
```

---

## 3. 根文档与画布

### 3.1 `<pagx>` → 根 `<div>`

```xml
<pagx version="1.0" width="400" height="400">
```
→
```html
<div class="pagx-root" data-pagx-version="1.0"
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

每个 Layer 映射为 `<div class="pagx-layer">`，按渲染流水线六阶段组织子内容：

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

**dashAdaptive**：CSS/SVG 无直接等效。降级：预计算路径长度，缩放 dasharray 值使段等长分布。

**CSS border 适用条件**：几何为 Rectangle + `align=center` + 无 dash + 默认 cap/join。注意 CSS border 是 inside stroke，需调整定位补偿。**通常推荐直接用 SVG stroke**。

**文本描边**：运行时文本的 SolidColor 描边使用 `-webkit-text-stroke: {width}px {color}` + `paint-order: stroke fill`。非 SolidColor 描边暂不支持。预排版文本（GlyphRun）使用 SVG `<path>` 的标准 `stroke` 属性。

---

## 9. 形状修改器

遇到形状修改器时**必须使用 SVG**。

### 9.1 TrimPath

策略 1（仅 Stroke）：SVG `stroke-dasharray`/`stroke-dashoffset` 模拟。

策略 2（通用）：预计算裁剪后路径数据。

| `type` | 处理 |
|--------|------|
| `separate` | 每个形状独立裁剪 |
| `continuous` | 所有形状视为一条连续路径，按总长度比例裁剪 |

**边界情况**：`start > end` → 镜像+反转路径方向取互补段；范围超 [0,1] → 环绕；offset 以角度计（360°=一个周期）；路径长度=0 → 不操作。

### 9.2 RoundCorner

预计算：将尖角替换为贝塞尔曲线圆弧。radius 自动限制不超过相邻边长度一半。`radius <= 0` 不操作。

### 9.3 MergePath

| `mode` | 运算 |
|--------|------|
| `append` | 多子路径合并为一个 `<path d>` |
| `union` | 并集 |
| `intersect` | 交集 |
| `xor` | 异或 |
| `difference` | 差集 |

**重要**：MergePath **清除之前所有累积的 Fill/Stroke**。合并后变换重置为单位矩阵。HTML 中 MergePath 之前的渲染层需移除。

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

**position.y 是基线**：CSS 文本从顶部开始，需减去 ascent。近似 `top = y - fontSize × 0.8`。

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

逐字形计算 factor（由 RangeSelector 的 start/end/offset/shape/weight/mode 决定），应用变换和样式覆盖。

变换公式中 factor 线性缩放 position/rotation，指数缩放 scale。alpha 用 |factor|。颜色覆盖用 |factor| 做 alpha blending。

HTML：每个字形包装为独立 `<span>` 或 SVG `<g>`，计算独立 transform 和样式。

### 10.5 TextPath

PAGX TextPath 比 SVG `<textPath>` 语义更丰富（baseline 概念、forceAlignment）。推荐**预计算每个字形位置和旋转角度**，逐字形定位输出 SVG。

baseline 映射：字形沿 baseline 的距离→曲线弧长位置，垂直偏移→法向偏移。

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

`true` 时子图层样式/滤镜不参与计算。CSS 无直接等效。降级：大多数场景忽略。

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
  <!-- mask layer 的渲染内容 -->
  <path d="..." fill="#FFFFFF"/>
</mask>
```

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
| `dashAdaptive` | ❌ | 暂不支持（TODO） |
| `plusDarker` blend mode | ❌ | 近似为 CSS `darken` |
| ImagePattern `mirror` | ⚠️ 部分 | 降级为 `repeat` |
| ImagePattern `clamp` | ❌ | 降级为 no-repeat |
| TextPath (完整语义) | ❌ | 暂不支持（TODO） |
| TextModifier | ❌ | 暂不支持（TODO） |
| RoundCorner | ❌ | 暂不支持（TODO） |
| MergePath (非 Append) | ❌ | 暂不支持（TODO） |
| TrimPath `continuous` | ⚠️ 部分 | 当作 `separate` 处理 |
| Repeater 小数 copies | ✅ | 最后副本 opacity × 小数部分 |
| `antiAlias="false"` | ⚠️ 部分 | `shape-rendering: crispEdges; image-rendering: pixelated` |
| `excludeChildEffects` | ❌ | 暂不支持（TODO） |
| `groupOpacity="false"` | ✅ | opacity 分配到每个子渲染元素 |
| ConicGradient + matrix | ⚠️ 部分 | SVG `<pattern>` + `<foreignObject>` + CSS conic-gradient |
| TileMode (blur filters) | ⚠️ 部分 | 浏览器固定边界行为 |
| DisplayP3 in SVG | ⚠️ 部分 | P3 通道值近似为 sRGB hex |
| LayerStyle.blendMode | ❌ | 暂不支持（CSS filter 架构限制） |
| Luminance mask 渐变 | ⚠️ 部分 | mask 内容统一白色填充（TODO） |

---

## 18. 转换决策流程图

```
对每个 Layer：
  │
  ├─ visible=false 且非 mask 引用？→ display:none，跳过
  │
  ├─ 有 composition 引用？→ 展开 Composition 内容
  │
  ├─ 处理 VectorElement 序列（累积-渲染模型）：
  │   │
  │   ├─ 遇到 Geometry（Rect/Ellipse/Polystar/Path/Text）→ 累积到列表
  │   │
  │   ├─ 遇到 Modifier（TrimPath/RoundCorner/MergePath）：
  │   │   ├─ 列表中有 Text？→ 触发文本到路径转换
  │   │   └─ 变换所有累积 Path → 标记为 SVG 渲染
  │   │
  │   ├─ 遇到 Text Modifier（TextModifier/TextPath/TextBox）→ 变换字形列表
  │   │
  │   ├─ 遇到 Repeater → 复制所有累积内容 + 已渲染样式
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
  │   ├─ Background Content
  │   ├─ Child Layers（递归）
  │   ├─ Layer Styles (above): InnerShadow
  │   ├─ Foreground Content
  │   └─ Layer Filters: filter 属性链
  │
  └─ 应用 Layer 属性：transform/opacity/blend-mode/mask/scrollRect
```
