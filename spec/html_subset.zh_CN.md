# PAGX 转换的 HTML 子集

本文件定义了 PAGX HTML 转换器（`pagx import --format html` / `pagx::HTMLImporter`）所
接受的受限 HTML/CSS 子集。该子集保证子集内的每一种写法都能稳定、近无损地映射为
一份特定形态的 PAGX。它同时也是大模型生成 HTML 作为首轮视觉草稿时的提示约束：
只要 LLM 产出严格遵循本子集，转换器就能稳定生成 PAGX，后续可以使用 PAGX 编辑工具
（MCP、`pagx` CLI）精修，再通过 PAGX 的导出能力无损发布到其它目标（Ardot 画布、
原生运行时等）。

姊妹文件 `spec/pagx_spec.zh_CN.md` 是 PAGX 的权威参考。

## 1. 文档骨架

合法输入是符合 XHTML（XML 严格模式）的 HTML，形态如下：

```html
<!DOCTYPE html>
<html>
  <head>
    <title>画布标题</title>
    <style>
      /* 可选的类选择器（见 3.3） */
    </style>
  </head>
  <body style="width: 800px; height: 600px;">
    <!-- 视觉内容 -->
  </body>
</html>
```

- `<html>` 与 `<body>` 必填，根节点上的其它元素一律拒收。
- 画布尺寸来自 `<body>` `style` 中的 `width`/`height`。缺省时由
  `HTMLImporter::Options::targetWidth/Height` 兜底；两者皆缺，导入失败并报
  `failed to determine canvas size`。
- `<head>` 仅 `<title>`（写入根节点的 `data-title`）、`<meta>`（忽略）与
  `<style>`（按 3.3 解析）受支持，其他 head 子节点告警并跳过。
- 所有标签均为小写并显式闭合（`<br/>`、`<img/>`）。

## 2. 允许的元素

| 标签 | PAGX 映射 | 备注 |
|------|-----------|------|
| `<html>` | 文档根 | 必填 |
| `<head>` | 元数据容器 | 仅 `<title>` / `<meta>` / `<style>` 起作用 |
| `<title>` | 写入根 `<pagx>` 的 `data-title` | 可选 |
| `<style>` | CSS 类规则注册表 | 仅类选择器（见 3.3） |
| `<body>` | 含画布尺寸的顶层 `<Layer>` | 必填 |
| `<div>` / `<section>` / `<header>` / `<footer>` / `<main>` / `<aside>` / `<nav>` / `<article>` | `<Layer>`（容器） | 语义中立，布局由 CSS 决定 |
| `<span>` / `<a>` | 最近的文本容器内的内联 run | `<a>` 视觉等同 `<span>`，`href` 转写为 `data-href` |
| `<p>` / `<h1>` … `<h6>` | 文本容器（`<Layer>` + `<TextBox>` 或裸 `<Text>`） | 默认字号见 4.5 |
| `<br/>` | 文本内换行 (`&#10;`) | 必须自闭合 |
| `<img/>` | `<Layer>` + `<Rectangle>` + `<Fill>` `<ImagePattern>` | `src` 自动注册为 `<Image>` 资源 |
| 内联 `<svg>` | 透传为宿主 `<Layer>` 的 `<svg>` 导入指令 | 由 `pagx verify` 时的 resolve 展开 |

其余标签（`<table>`、`<canvas>`、`<form>`、`<input>`、`<button>`、`<script>`、
`<iframe>`、`<video>`、`<audio>`、自定义元素等）一律拒收。导入时跳过该节点，不
再递归其子节点，并发出一条 warning。

## 3. 样式来源

### 3.1 内联 `style="…"` 属性

内联样式优先级最高，也是推荐 AI 生成的形态。`style` 内容按 CSS 声明序列解析
（容忍空白、支持 `/* */` 注释、识别括号嵌套）。

### 3.2 元素默认样式

转换器内置一份默认样式表：

| 元素 | 默认样式 |
|------|----------|
| `<body>` | `font-family: Arial; font-size: 14px; color: #1E293B;` |
| `<h1>` | `font-size: 32px; font-weight: bold;` |
| `<h2>` | `font-size: 24px; font-weight: bold;` |
| `<h3>` | `font-size: 20px; font-weight: bold;` |
| `<h4>` | `font-size: 18px; font-weight: bold;` |
| `<h5>` | `font-size: 16px; font-weight: bold;` |
| `<h6>` | `font-size: 14px; font-weight: bold;` |
| `<a>` | `color: #2563EB; text-decoration: underline;` |

用户提供的样式总是覆盖默认值。

### 3.3 `<style>` 块

支持 `<head>` 内的 `<style>` 元素；多个 `<style>` 块会被拼接后一起解析。选择器形态限定：

- 类选择器：`.cls { … }`
- 逗号分隔的类列表：`.cls-a, .cls-b { … }`
- 元素（类型）选择器：`body { … }`、`h1 { … }`、`p { … }`。语法上接受任意标签名；针对子集
  外标签的规则永远不会命中（那些元素会在归一化时被丢弃）。
- **不**允许通配 `*`、后代选择器、伪类、属性选择器；遇到时声明保留、选择器忽略并告警。

样式优先级（实际生效）：内联 `style` > 类规则 > 元素规则 > 元素默认 > 继承。

### 3.4 外部 CSS

`<link rel="stylesheet">` 一律禁止。转换器不执行任何网络 I/O。

## 4. 允许的 CSS 属性

长度均为像素；百分比仅在 `width`/`height` 上接受（按 PAGX 语义参考父节点的内 padding
框）。相对单位 `em`、`rem`、`pt`、`vw`、`vh` 在归一化时会**换算**为像素（`em` 基于父元素
计算后的 `font-size`，`rem` 基于 16px，`pt` 按 `pt × 4/3`，`vw`/`vh` 基于画布尺寸），并发出
`subset:unit-coerced` 警告。画布尺寸尚未确定时，`vw`/`vh` 会以 `subset:unsupported-property`
警告丢弃。`calc()`、`var()`、`min()`、`max()`、`clamp()` 会以 `subset:unsupported-property`
警告拒收。

### 4.1 布局

| CSS | PAGX | 备注 |
|-----|------|------|
| `display: block` | 默认（无 `layout` 的 `Layer`） | 默认按块流堆叠 |
| `display: flex` | 在父 Layer 开启 `layout` | flex 布局的前提 |
| `display: inline` / `inline-block` | 降级为 `block`（告警） | 不模拟真正的内联流 |
| `flex-direction: row` | `layout="horizontal"` | flex 默认 |
| `flex-direction: column` | `layout="vertical"` | |
| `flex-direction: row-reverse` / `column-reverse` | 回退为 `row` / `column`（告警） | 不支持反向排序 |
| `gap: N` | `gap="N"` | 单值 |
| `padding: …` | `padding="…"` | 支持 `N`、`T R`、`T R B L` |
| `margin: …` / `margin-*` | 折进定位锚点或一层透明 padding 包裹层（见下文） | PAGX 无逐子 margin；受支持,不丢弃 |
| `align-items: stretch | center | flex-start | flex-end | baseline` | `alignment="stretch|center|start|end"`（`baseline` 透传） | |
| `justify-content: flex-start | center | flex-end | space-between | space-evenly | space-around` | `arrangement="start|center|end|spaceBetween|spaceEvenly|spaceAround"` | |
| `flex: N` | `flex="N"` | 仅整数/无量纲 grow；`none`→`0`,`auto`/`initial`→`1` |
| `flex-shrink: 0` | 静默丢弃（PAGX 从不收缩） | 其它值告警 —— 请用 `flex: N` |

`margin` 属于子集：PAGX 没有逐子 margin,转换器会复现它。`position: absolute` 元素的 margin
折进已有的 `left/right/top/bottom` 锚点;流式/flex 子元素则被包进一层透明外层 Layer,其
`padding` 等于四边 margin。flex 容器上统一的逐子主轴 margin 还会进一步被提升到容器的 `gap`
（见 §11）。

禁用（告警并跳过）：`flex-wrap`、`flex-grow`、`flex-basis`
（用 `flex: N` 缩写）、`display: grid`、`grid-*`、`float`、
`order`、`align-content`、`align-self`、`direction`。

### 4.2 尺寸

| CSS | PAGX |
|-----|------|
| `width: N px` / `height: N px` | `width="N"` / `height="N"` |
| `width: N%` / `height: N%` | `width="N%"` / `height="N%"` |
| `box-sizing: border-box` | 默认且唯一行为 |

`min-*` / `max-*`、`aspect-ratio` 告警并忽略。

### 4.3 定位

| CSS | PAGX |
|-----|------|
| `position: absolute` + `left/right/top/bottom: N` | `includeInLayout="false"` + Layer 上的 `left|right|top|bottom="N"` |
| `position: relative` | 静默丢弃（无操作） |
| 负值偏移（如 `top: -6px`） | 原样保留 |

`position: relative` 被静默丢弃 —— PAGX 没有 containing-block 概念（子 Layer 始终锚定到直接
父节点），该声明在 PAGX 侧无任何效果;与之搭配的 `left/right/top/bottom` 偏移也随之无效。

`position: fixed`、`position: sticky` 及在文本叶（`<p>`、`<span>`）上的绝对定位均告警,并
降级为父 Layer 上的绝对定位。

### 4.4 绘制与效果

| CSS | PAGX |
|-----|------|
| `background-color: <color>` | `<Rectangle width="100%" height="100%" roundness="…"/>` + `<Fill color="…"/>` 背景对（见 §5） |
| `background-image: linear-gradient(angle, c1 [p], c2 [p], …)` | `<Fill>` 内嵌 `<LinearGradient>`（`startPoint`/`endPoint` 由角度推得） |
| `background-image: radial-gradient(…)` | 内嵌 `<RadialGradient>` |
| `background-image: conic-gradient(from angle, …)` | 内嵌 `<ConicGradient>`（CSS 0° = 顶，PAGX 0° = 右，自动 −90°） |
| `background-clip: text`（别名 `-webkit-background-clip: text`） | 与 gradient `background-image` 同时设置时，渐变下沉到后代文字节点（`<TextBox>` / `<Text>` 的 `<Fill>` 内嵌渐变），本元素的矩形背景被抑制。无 gradient `background-image` 时该属性为无操作。 |
| `background-image: url(...)` | 还原为背景矩形上的 `<ImagePattern>` 填充；`background-size` / `background-repeat` / `background-position` 决定该 pattern 的 `scaleMode` / 平铺模式 / 矩阵 |
| `background-blend-mode: <mode>` | 在渐变 / 图像填充上设置 `Fill.blendMode`，使其与 `background-color` 混合；此时会保留底部的纯色 `<Fill>` 作为混合所需的背景。`normal`（默认）为空操作，不透明的渐变 / 图像仍会覆盖底色 |
| `mask-image: url(data:image/svg+xml,...)`（+ `mask-mode` / `mask-size` / `mask-position` / `mask-repeat`） | 引用的 SVG 变成一层 PAGX mask；`mask-mode` 选择 Alpha 还是 Luminance，`mask-size` / `mask-position` 决定其缩放/偏移 |
| `clip-path: url(#id)` | 解析引用的隐藏 `<clipPath>` 为一层轮廓 mask。静态几何形式（`inset()`/`circle()`/`ellipse()`/`polygon()`/`path()`）在 PAGX 无对应原语，告警丢弃——但*动画*的几何 `clip-path` 作为轮廓 mask 变形受支持（见 §13.2） |
| `border-radius: N`（px）、`N%`（按 `min(width, height)` 解析；固定 px 宽高且 `border-radius: 50%` 的元素会变成 `Ellipse`），或 1–4 值缩写（`T`、`T R`、`T R B`、`T R B L`） | `Rectangle.roundness = N`（`50%` 输出 `Ellipse`）。椭圆 `W / H` 双半径形式告警并忽略 |
| `border: W <style> C` | `<Stroke color="C" width="W" align="inside"/>`（`solid`/`dashed`/`dotted` 一等公民；其它样式告警并降级为 `solid`） |
| `box-shadow: X Y B C`（多重、可加 `inset`） | 每个阴影一份 `<DropShadowStyle>` 或 `<InnerShadowStyle>` |
| `opacity: A` | `Layer.alpha = A` |
| `mix-blend-mode: <mode>` | `Layer.blendMode = <mode>` |
| `filter: blur(X) drop-shadow(X Y B C)` | `<BlurFilter>` / `<DropShadowFilter>` 链 |
| `backdrop-filter: blur(X)` | `<BackgroundBlurStyle>` |
| `transform: <fn>` | 映射到 `Layer.matrix`。支持单函数形式（`skewX`/`skewY`/`rotate`/`scale[X\|Y]`/`translate[X\|Y]`/`matrix(a,b,c,d,tx,ty)`）以及 `matrix3d(...)`（投影为其 2D 仿射分量）；复合链与其它 3D 变体（`rotate3d`/`perspective`）告警丢弃 |
| `transform-origin` | 透传；当其等于盒子中心（`50% 50%`、`center`、`center center`，或等于盒心的 px 值）时被尊重，其它原点告警 |
| Layer 上的 `overflow: hidden` | `Layer.clipToBounds = true` |

`background-clip: border-box` / `padding-box` / `content-box` 均为静默无操作（仅 `text` 关键字
在 PAGX 有效,见上）。

禁用（告警并跳过）：单边 `border-*`、单角 `border-*-radius`、`outline`、`perspective`、
几何 `clip-path` 形式（`inset`/`circle`/`ellipse`/`polygon`/`path`），以及复合链与非
`matrix3d` 的 3D `transform`。

### 4.5 文本

| CSS | PAGX |
|-----|------|
| `color: <color>` | 文本旁的 `<Fill color="…"/>`（继承） |
| `text-decoration-color: <color>` | 下划线/删除线叠加矩形的颜色（继承,见 §6） |
| `font-family: name` | `Text.fontFamily` |
| `font-size: N px` | `Text.fontSize` |
| `font-weight: bold` / `600+` | 粗体（`bold` 或数值 ≥ 600）映射为粗体样式；当解析到的字体没有原生粗体时，导入器施加合成 `fauxBold`，使字重仍能呈现 |
| `font-style: italic | oblique` | `Text.fontStyle = "Italic"`（与粗体组合为 `"Bold Italic"`） |
| `letter-spacing: N px` | `Text.letterSpacing` |
| `-webkit-text-stroke: W C`（或 `-webkit-text-stroke-width` / `-webkit-text-stroke-color` 长写） | 为字形 run 添加文本 `<Stroke color="C" width="W"/>` |
| `text-align: start | left | center | right | justify` | `TextBox.textAlign = "start|center|end|justify"`（`left` → `start`，`right` → `end`） |
| `line-height: N px` / `line-height: N`（无量纲倍数）/ `N%` | `TextBox.lineHeight`（无量纲与百分比按元素 `font-size` 解析） |
| `text-decoration: underline | line-through` | 1px `<Rectangle>` 叠加（`bottom="0"` / `centerY="0"`），见 §6 |
| `white-space: nowrap` | `TextBox.wordWrap = false` |
| `writing-mode: vertical-rl | vertical-lr` | `TextBox.writingMode = "Vertical"`（水平模式为默认） |
| 文本容器上的 `overflow: hidden` | `TextBox.overflow = "hidden"` |
| `text-overflow: ellipsis` | 告警（PAGX 暂未实现） |

禁用（告警并跳过）：`text-transform`、`text-indent`、`word-spacing`、`direction`、
`unicode-bidi`、`font-variant`、`font-stretch`、`font` 缩写、web fonts (`@font-face`)。

**行内文本自适应宽度。** 不换行的行内文本叶子（`<span>` / `<a>`，且 `white-space: nowrap`
或 `pre`）会丢弃其内联轴上的显式尺寸，改由排版后的文本撑起盒子，对应 CSS 的 shrink-to-fit。
这样盒子始终与 PAGX 实际渲染的字形一致，而不是冻结 `tools/html-snapshot` 烘焙进来的浏览器测量
像素宽度（该宽度绑定的是浏览器的字体度量，一旦渲染端替换了字体——中文/网络字体很常见——就会导致
文本居中偏移或被裁剪）。在以下情况下尺寸会被保留而非丢弃，因为此时它是有意义的：块级叶子
（`<p>` / `<h1>`…`<h6>`）、可换行叶子（宽度即换行边界）、盒子绘制了背景/边框/阴影的叶子、以远边
锚定（`right` / `bottom`）的叶子，或带 flex 伸展的子项。

### 4.6 自定义数据

`data-*` 前缀的属性按 PAGX `data-*` 约定（见 `spec/pagx_spec.zh_CN.md` §2.3）原样保留
到对应 PAGX 节点。HTML 的 `id` 属性在去重后透传为 Layer 的 `id`。

## 5. 容器与背景

当一个 HTML 元素同时拥有绘制背景（颜色/渐变/边框/阴影/圆角）和需要 padding/layout
的子节点时，转换器输出 PAGX 标准的"外层背景 + 内层 padding 容器"形态（参考
`spec/pagx_spec.zh_CN.md` §4.2 容器布局 → 带 Padding 的背景）：

```xml
<Layer width="100%" height="100%">
  <Rectangle width="100%" height="100%" roundness="…"/>
  <Fill color="…"/>
  <Layer width="100%" height="100%" layout="vertical" padding="…">
    <!-- 子内容 -->
  </Layer>
</Layer>
```

无背景也无 padding 的元素只产出单层 Layer。

## 6. 文本装饰

下划线与删除线在同一个 Layer 内输出为叠加矩形：

- `text-decoration: underline` → 1px `<Rectangle>` 置于 `bottom="0"`
- `text-decoration: line-through` → 1px `<Rectangle>` 置于 `centerY="0"`

装饰颜色与文本颜色不同时，矩形包入 Group 以隔离其 `<Fill>`。

## 7. 图像与内联 SVG

- `<img src="path" width="W" height="H"/>` 将 `src` 注册为 `<Image>` 资源（接受 data URI；
  相对路径相对输入文件目录解析），并输出 `<Rectangle width="100%" height="100%"/>` +
  `<ImagePattern image="@id"/>`。直接设在 `<img>` 上的 `border-radius` 会圆角化该矩形。
- `<img>` 上的 `object-fit` 设置 `ImagePattern.scaleMode`：`fill`→`Stretch`、
  `contain`→`LetterBox`、`cover`→`Zoom`；`none` 与 `scale-down` 告警并降级为 `contain`。
  省略 `object-fit` 时默认 `Stretch`（CSS `fill`）。
- 内联 `<svg>...</svg>` 被原样捕获，写入宿主 `Layer` 的 `<svg>` 导入指令；`pagx verify`
  / `pagx resolve` 时展开。
- `<img src="path.svg"/>` 视作外部导入：`Layer import="path.svg"/>`。

### 7.1 圆角图片包装容器（CSS 头像模式）

CSS 圆角头像的常见写法是用 `border-radius` + `overflow: hidden` 的容器把 `<img>` 裁剪成圆形
（或圆角矩形）：

```html
<div style="border-radius: 9999px; overflow: hidden">
  <img src="avatar.png" style="width: 64px; height: 64px"/>
</div>
```

如果按字面翻译，转换器会输出一个带 `clipToBounds="true"` 的外层 Layer 加一个图片子 Layer，但
`clipToBounds` 只能裁剪到**矩形**边界（见 `spec/pagx_spec.zh_CN.md` §5.5.2），导致图片的方形几何
会越过包装容器的圆角，渲染成"方形头像装在圆环里"的视觉错误。为对齐 CSS 语义，当**所有**下列条件
同时成立时，转换器会把 `<img>` 折叠到包装容器的圆角 `Rectangle` 上、由它直接作为图片填充几何：

- 包装容器同时设置了 `border-radius: N` 与 `overflow: hidden`；
- 包装容器没有 `padding` / `display: flex` / `gap`（否则需要走标准"外层背景 + 内层布局宿主"
  双层结构）；
- 包装容器只有唯一一个元素子节点（其余只允许是空白文本），且该子节点为 `<img>`；
- 该 `<img>` 完全覆盖包装容器的内容区域（`width` / `height` 与父对齐，定位在 `(0, 0)`）。

折叠后输出的是 PAGX 中圆角图片的标准模式：

```xml
<Layer width="64" height="64">
  <Rectangle width="100%" height="100%" roundness="9999"/>
  <Fill><ImagePattern image="@avatar"/></Fill>
</Layer>
```

包装容器上声明的背景色 / 渐变 / 描边 / 阴影会保留在折叠后的图片填充之下（在图片透明像素处透出
来，与 CSS 绘制顺序一致）。SVG 图片源（`.svg`）不会被折叠 —— 它们仍然走自己的外部导入指令通道。

导入器最多会穿过三层仅用于布局的包装 `<div>`（自身无绘制、只透传布局的包装层）去寻找 `<img>`，
因此常见的 `div > div > img` 头像结构仍能被折叠。

## 8. 坐标 / 角度约定

- HTML 坐标系与 PAGX 一致（左上原点，y 向下）。
- CSS 渐变角度从上方顺时针计；PAGX 渐变角度从 +X 轴计。转换公式：
  `pagx_angle = css_angle − 90°`，对 `linear-gradient` 与 `conic-gradient` 都适用。
- `linear-gradient` 关键字（如 `to bottom right`）会折算为数值角度。

## 9. 常见陷阱

- `margin` 已受支持（折进定位/padding 包裹层,或被提升为 `gap`），但 `padding`、`gap`、
  `flex: N` 的映射更直接 —— 布局 flex 子元素时优先使用它们。
- 不要在 flex 主轴上给子节点显式设置 width/height —— 用 `flex: N` 分配剩余空间。
- 不要把 `position: absolute` 子节点与已有 `gap` / `align-items` 的 flex 父混用。
- 不要用文本字符（`+`、`×`、`→` 等）替代图标 —— 改用内联 `<svg>`。

## 10. 诊断

转换器通过 `PAGXDocument::errors` 输出诊断，共两级：

- **error**：阻断式问题（缺少 `<body>`、画布尺寸缺失）。CLI 中通过 `ImportResult::error`
  返回。
- **warning**：元素或属性被跳过或降级。经 `ImportResult::warnings` 输出。`pagx import` CLI
  **默认抑制**这些警告；加 `--verbose` / `-v` 才打印到 stderr。API 调用方始终可见。

行为由 `HTMLImporter::Options`（API 层）控制；`pagx import` **不**暴露任何 `--html-*` 命令行
选项——HTML 路径在代码中固定：

- `strict`（默认 false）：把 warning 当作 error（用于 CI）。
- `preserveUnknownElements`（默认 false）：把未知标签保留为带 `data-html-unknown="<tag>"`
  的空 Layer，便于排查。
- `autoNormalize`（默认 true）：运行子集归一化前置流程（见 §11）。关闭它可在已满足子集的
  HTML 上直接调试导入器。
- `inferFlexFromAbsolute`（**默认 true**）：运行 `HTMLFlexInference` Pass（见 §11），从
  flat `position: absolute` 输入（典型场景：`tools/html-snapshot/snapshot.js` 输出）反推
  `display: flex`。`autoNormalize` 为 false 时无效。

## 11. 自动归一化（Auto-normalization）

导入器在遍历 DOM 之前会先运行 `HTMLSubsetTransformer`（参见
`src/pagx/html/importer/HTMLSubsetTransformer.h`），把任意输入改写为严格符合本子集的 DOM，让后续
代码只接触合规的 HTML。该过程默认开启；通过
`HTMLImporter::Options::autoNormalize = false` 可以关闭（仅 API 层——`pagx import` 未暴露
对应的 `--html-*` 命令行选项）。

转换器按固定顺序执行八个核心 Pass，外加一个可选 Pass（可选的 `HTMLFlexInference`
运行在 PropertyFilter 与 MarginToGapPromotion 之间），行为如下：

| Pass | 静默改写 | 警告并丢弃 |
|------|----------|------------|
| DocumentSkeleton | 合并重复的 `<head>` / `<body>`、小写化标签、去掉注释与处理指令、丢弃 `<head>` 中除 `<title>` / `<meta>` / `<style>` 之外的元素 | `<script>` 等不允许的 `<head>` 内容（`subset:unsupported-tag`）；外部 `<link rel="stylesheet">`（`subset:external-stylesheet`）；`<html>` 与 `<body>` 之间的多余顶层元素（`subset:unsupported-tag`） |
| StyleSheetCollector | 把 `<style>` 中的类选择器与元素选择器规则展开到每个元素的内联级联里，并移除 `<style>` 元素本身 | `*`、后代/伪类/属性选择器（`subset:unsupported-selector`）；`@media`、`@font-face`、`@keyframes` 等 at-rule（`subset:unsupported-at-rule`） |
| ComputedStyle | 完成 CSS 级联（继承 → 元素默认 → 元素规则 → 类规则 → 内联 `style`），并把合并后的属性写回内联 style；把 `-webkit-` 前缀声明合并到标准名（如 `-webkit-background-clip` → `background-clip`） | — |
| PropertyFilter | 把 `em` 转 px（基准为父元素计算后的 `font-size`），`rem` 转 px（基准 16px），`vw`/`vh` 转 px（基准为画布尺寸），`pt` 转 px（`subset:unit-coerced`）；把 `flex: <grow> <shrink> <basis>` 收敛为 `flex: <grow>`（`subset:flex-shorthand-collapsed`），`flex: none`→`0`、`auto`/`initial`→`1`；丢弃 `flex-shrink: 0`（PAGX 从不收缩）；把 `display: inline \| inline-block` 降级为 `block`、`flex-direction: *-reverse` 降为 `row`/`column`；静默丢弃 `position: relative`（PAGX 无操作,见 §4.3）；把 `position: fixed \| sticky` 降级为 `position: absolute`；保留 `margin*`（后续折叠）、`transform`/`transform-origin`（仅单函数/`matrix()`/`matrix3d()`）、`object-fit`、`writing-mode`、`background-image` + `background-size`/`background-repeat`/`background-position`（url 背景）、`mask-*`、`clip-path: url(#id)`、`-webkit-text-stroke*` | `transform` 复合链与非 `matrix3d` 的 3D 形式、`flex-shrink`（非零）、几何 `clip-path` 形式、`outline`、`float`、`order`、`align-content`、`align-self`、`direction`、`unicode-bidi`、`flex-wrap`、`flex-grow`、`flex-basis`、`min-*`、`max-*`、`aspect-ratio`、`text-transform`、`text-indent`、`word-*`、`overflow-wrap`、`font-variant`、`font-stretch`、`font` 简写、`grid-*`、单边 `border-*`、单角 `border-*-radius`、`z-index`、`cursor`、`pointer-events`、`user-select`、`visibility`（`subset:unsupported-property`）；`var()`、`calc()`、`min/max/clamp()`（`subset:unsupported-property`）；其它不支持的单位（`subset:unsupported-property`） |
| HTMLFlexInference *（默认开启：`Options::inferFlexFromAbsolute`；`autoNormalize` 为 false 时无效）* | 当某容器的所有子元素都是 `position: absolute` 且能整齐拼成一行/一列时，把容器改写为 `display: flex` 并填入推断出的 `gap`、`padding`、`align-items`、`flex-direction`；当容器有显式主轴尺寸且内容两端留白（近似）相等时，用 `justify-content: center` 取代对称的主轴 padding；同时移除子元素的 `position` / `left` / `right` / `top` / `bottom`（`subset:flex-inferred`） | 子元素在两个轴向都重叠、跨轴对齐方式不一致、或主轴间距不均匀的容器一律保留绝对定位（`subset:flex-inference-skipped`） |
| MarginToGapPromotion | 当 `display: flex` 容器的在流子元素带有统一的逐子主轴 margin（leading 或 trailing 模式）时，把该 margin 提升到容器的 `gap` 并清除逐子 margin（`subset:margin-promoted-to-gap`） | 容器换行、已有正 `gap`、参与子元素少于两个、子元素带 `flex` grow、或 margin 非 px 时跳过（margin 留给 `wrapForMargin` 折叠） |
| SpaceJustifyOverflowCollapse | 当使用 `space-between` / `space-around` / `space-evenly` 的 `display: flex` 容器子元素在主轴溢出时，把 `justify-content` 改写为 `flex-start`，避免 PAGX flex 引擎重叠（`subset:space-justify-collapsed-on-overflow`） | 尺寸数据不全（无显式 px 主轴尺寸、padding/gap 非 px、子元素尺寸无法解析、或子元素带 `flex` grow）时保持原样 |
| StructureNormalization | 把容器中的散落文本包进 `<p>`（`subset:text-wrapped`）；丢掉元素之间纯空白的文本节点；保持 `<svg>` 子树原样以供 SVG 解析器使用 | 未知标签（`<table>`、`<form>`、`<input>`、`<button>`、自定义元素等）被移除（`subset:unsupported-tag`）；启用 `HTMLImporter::Options::preserveUnknownElements` 时会被保留为 `<div data-html-unknown="<tag>">` |
| InlineStyleEmitter | 把每个元素解析后的属性集合按字母顺序重新写回 `style="…"`（保证可重复输出）；同时丢掉已经被级联吸收的 `class` 属性（`Options::preserveClassAttribute` 为真时保留） | — |

所有诊断都使用统一的 `subset:<category>` 前缀，并经由 `PAGXDocument::errors`（CLI 下经
`ImportResult::warnings`）输出。在严格模式（`HTMLImporter::Options::strict`）下，任一警告都会
立即升级为错误并终止导入。

## 12. 完整示例

输入：

```html
<!DOCTYPE html>
<html>
  <body style="width: 320px; height: 96px;">
    <div style="display: flex; flex-direction: row; align-items: center; gap: 12px; padding: 12px;
                background-color: #FFFFFF; border-radius: 12px;
                box-shadow: 0 2px 6px #00000026;">
      <div style="width: 48px; height: 48px; border-radius: 24px; background-color: #6366F1;"></div>
      <div style="display: flex; flex-direction: column; gap: 4px;">
        <span style="font-size: 16px; font-weight: bold; color: #1E293B;">Alice Chen</span>
        <span style="font-size: 13px; color: #64748B;">alice@example.com</span>
      </div>
    </div>
  </body>
</html>
```

输出（经 `PAGXOptimizer` 后）：

```xml
<pagx width="320" height="96">
  <Layer width="100%" height="100%">
    <Rectangle width="100%" height="100%" roundness="12"/>
    <Fill color="#FFFFFF"/>
    <Layer width="100%" height="100%" layout="horizontal" gap="12" padding="12" alignment="center">
      <Layer width="48" height="48">
        <Ellipse width="100%" height="100%"/>
        <Fill color="#6366F1"/>
      </Layer>
      <Layer layout="vertical" gap="4">
        <Layer>
          <Text text="Alice Chen" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
          <Fill color="#1E293B"/>
        </Layer>
        <Layer>
          <Text text="alice@example.com" fontFamily="Arial" fontSize="13"/>
          <Fill color="#64748B"/>
        </Layer>
      </Layer>
    </Layer>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000026"/>
  </Layer>
</pagx>
```

## 13. 动画

子集支持一种声明式动画形式：CSS `@keyframes` 规则配合元素上的 `animation` 简写。这是
`tools/html-snapshot` 捕获层与导入器之间的规范契约：快照工具把来自多种来源（原生 CSS
`@keyframes`、Web Animations API，以及 GSAP / anime.js 等 JS 动画库——参见
`tools/html-snapshot/README.md`）的动画归一化为这一种形式，导入器再把它映射到 PAGX 动画模型
（`<Animations>` + `Animation` / `Object` / `Channel` / `Key`）。

### 13.1 接受的形式

```html
<head>
  <style>
    @keyframes fadeMove {
      0%   { opacity: 0; transform: translateX(0px); }
      100% { opacity: 1; transform: translateX(40px); }
    }
  </style>
</head>
<body style="width: 200px; height: 100px;">
  <div id="card" style="animation: fadeMove 2s linear infinite;"></div>
</body>
```

- `@keyframes <name> { <stop> { … } … }` 块定义一条命名时间线。停靠点是百分比（`0%` … `100%`）
  或关键字 `from`（= `0%`）/ `to`（= `100%`）；单个停靠点可列出多个以逗号分隔的选择器。
- `@keyframes` 块在 `<style>`-内联归一化中被保留（一个只承载幸存 `@keyframes` 的 `<style>`
  元素会被保留，供导入器读取）。
- `animation` 简写把一条 `@keyframes` 时间线附着到元素上。长写属性 `animation-name` /
  `animation-duration` / `animation-timing-function` / `animation-iteration-count` /
  `animation-direction` / `animation-delay` 也被接受，并在归一化时折叠进简写。
- 每个元素一个动画。以逗号分隔的动画列表只保留第一项（`subset:animation-multiple` 警告）。

### 13.2 通道映射

只有能映射到 PAGX 运行时可实际回放的通道的属性才会被输出。`@keyframes` 停靠点里的其它一切都会
被警告并丢弃（`subset:animation-unsupported-property`）。

| CSS 动画属性 | PAGX 通道 | 目标节点 | 值类型 |
|--------------|-----------|----------|--------|
| `opacity` | `alpha` | 元素的 `Layer` | float |
| `transform`（仅纯平移） | `x` / `y` | 元素的 `Layer` | float |
| `transform`（scale / rotate / skew / 任意非纯平移） | `matrix` | 元素的 `Layer` | matrix |
| `color` / `background-color` | `color` | 元素 `Fill` 内的 `SolidColor` | color |
| `filter: drop-shadow(...)` | `offsetX` / `offsetY` / `blurX` / `blurY` / `color` | 元素 `Layer` 上的 `DropShadowFilter` | float / color |
| `filter: blur(...)` | `blurX` / `blurY` | 元素 `Layer` 上的 `BlurFilter` | float |
| `clip-path`（几何形式 `inset`/`circle`/`ellipse`/`polygon`/`path`） | `point{i}.x` / `point{i}.y` | 轮廓 mask `Layer` 内的 `Path` | float |

`transform` 会逐关键帧解析为一个 2D 仿射矩阵（单函数与空格分隔的复合链——`translate[X|Y]` /
`scale[X|Y]` / `rotate` / `skewX|Y|skew` / `matrix(...)`）。当*每一个*关键帧都是纯平移时，动画使用更
廉价的 `x` / `y` 通道（它们叠加在布局分配的位置之上）。一旦任一关键帧带有 scale / rotate / skew
（或一个非平移的 `matrix(...)`），整个仿射就被路由到单个 `matrix` 通道，并像静态 `transform` 路径一样
围绕元素的 `transform-origin` 取轴（`T(cx,cy) · M · T(-cx,-cy)`），使静态与动画的变换一致。3D 形式
（`matrix3d` / `rotate3d` / `perspective`）没有 2D 仿射表示，被丢弃并告警
（`subset:animation-unsupported-property`）。影响布局的属性（`width`、`height`、`margin`、
`padding`、`gap`、`flex` …）永远不可动画（见 §4.4），同样被丢弃。

> matrix 通道插值会把每个关键帧分解为 平移 / 缩放 / 旋转 / 错切 并对这些分量插值
> （`pagx/runtime/MatrixDecompose.h`）；它无法在相邻两个关键帧之间还原整圈或跨越 ±180° 的旋转，因此
> 非常大的单段旋转会走近路。补充中间 `@keyframes` 停靠点（常见做法）可避免这个问题。

当动画元素没有作者 `id` 时，其 `Layer` 会被赋予一个生成的 `id`（前缀 `anim`），使输出的
`<Object target="…">` 能引用它。`color` 动画要求元素绘制了一个实色 `background-color`（对文本则是实色
`color`）；当没有 `SolidColor` 填充时，`color` 通道被丢弃（`subset:animation-unsupported-property`）。

**滤镜动画。** 在 `none` 与 `drop-shadow(...)` / `blur(...)` 之间做动画的 `filter` 链（或做动画的
`box-shadow`，捕获层会把它折叠为等价的 `drop-shadow`）会被下沉到运行时可动画的滤镜节点。链中的每个
`drop-shadow` 都按作者顺序保留——一组由多个纯偏移阴影组成的"色差"堆叠会变成每个槽一个可动画的
`DropShadowFilter`，从而整叠一起合成，而非只取一个代表。某个停靠点若省略了某个阴影槽，会把该槽的
`color` 透明度驱动为零，使残影淡入淡出而非突变。`blur(...)` 折叠到单个 `BlurFilter` 半径。元素上已有的
静态滤镜节点会被复用作动画目标；否则会新建一个零值/透明的基线节点，以便 fill-mode 能恢复"关闭"状态。

**clip-path 动画。** 在几何形状（`inset()` / `circle()` / `ellipse()` / `polygon()` / `path()`）之间做
动画的 `clip-path` 会变成一个可动画的轮廓 mask——擦除 / 揭示 / 光圈效果。这比静态路径更宽泛：静态路径
只接受 `clip-path: url(#id)` 并丢弃几何形式（§4.4）；捕获层会把每个关键帧的形状归一化为边框盒像素下的
规范 `path("d")`，导入器再用逐点 `point{i}.x` / `.y` 通道驱动 mask 的 `Path`。所有关键帧必须解析为
**相同**的形状结构（相同的 verb / 点数）；CSS 本身就拒绝对不匹配的形状插值，因此不匹配会丢弃 clip 通道
并告警（`subset:animation-unsupported-property`）。只有在整条时间线上真正变化的点坐标才会输出通道。

### 13.3 时间映射

- 帧率固定为 60 fps。`@keyframes` 百分比换算为帧时间：
  `time = round(percent / 100 * duration_seconds * 60)`。`animation-duration` 接受 `s` 和 `ms` 单位。
- `animation-timing-function`：`linear` → `linear`；`ease` / `ease-in` / `ease-out` / `ease-in-out`
  与 `cubic-bezier(x1, y1, x2, y2)` → `bezier`（缓动写到关键帧的 `bezier-out` / 下一关键帧的
  `bezier-in` 手柄上）。`steps(n, <jump>)` / `step-start` / `step-end` 不是运行时插值类型；每个
  `@keyframes` 段会被展开为 `n` 个 `hold` 子关键帧以复现 CSS 阶梯（四种 jump——`jump-start` /
  `jump-end` / `jump-none` / `jump-both`——都被支持）。
- `animation-iteration-count`：`infinite` → `Animation.loop="loop"`；任何有限次数 → `once`
  （PAGX 没有有限重复次数——次数 > 1 会以 `subset:animation-finite-count` 告警）。非正次数会抑制
  回放（以同一诊断丢弃）。
- `animation-direction`：`alternate` → `Animation.loop="pingPong"`**仅当次数为 infinite 时**（有限
  `alternate` 无法表达，被降级为 `once` 并告警 `subset:animation-finite-count`）；`reverse` /
  `alternate-reverse` 会同时反转关键帧顺序与缓动（`ease-in` 变 `ease-out`，step 的 jump 互换）；
  `normal` 保持原序。
- `animation-delay`：正延迟把每个关键帧时间前移；负延迟告警并钳制为 0。对于循环动画，延迟只作用于
  第一次迭代（不计入 `Animation.duration`，因此不会每个周期重放该间隙）。
- `animation-fill-mode`：`none`（默认）在延迟结束前以及 `once` 动画结束后显示元素的非动画基线值；
  `forwards` 在结束后保持最后一个关键帧；`backwards` 在延迟期间显示第一个关键帧；`both` 两者兼有。
  通过在边界注入基线 `hold` 关键帧实现（`Loop` / `PingPong` 没有"结束之后"的区域，因此那里只有前导
  基线生效）。

### 13.4 结果 PAGX

§13.1 的示例转换为：

```xml
<pagx width="200" height="100">
  <Layer id="card" width="100%" height="100%"/>
  <Animations>
    <Animation id="card_anim" duration="120" frameRate="60" loop="loop">
      <Object target="card">
        <Channel name="alpha" type="float">
          <Key time="0" value="0"/>
          <Key time="120" value="1"/>
        </Channel>
        <Channel name="x" type="float">
          <Key time="0" value="0"/>
          <Key time="120" value="40"/>
        </Channel>
      </Object>
    </Animation>
  </Animations>
</pagx>
```

### 13.5 内联 SVG 形状动画

内联 `<svg>` 形状上的动画会被下沉到逐形状的绘制节点，而非元素 `Layer`，从而解锁图标与线条画动效：

| SVG 动画属性 | PAGX 通道 | 目标节点 |
|--------------|-----------|----------|
| `opacity` | `alpha` | 形状的 `Layer` |
| `transform`（仅纯平移） | `x` / `y` | 形状的 `Layer` |
| `fill` | `color` | 形状的 `Fill` |
| `fill-opacity` | `alpha` | 形状的 `Fill` |
| `stroke` | `color` | 形状的 `Stroke` |
| `stroke-opacity` | `alpha` | 形状的 `Stroke` |
| `stroke-dashoffset` | `dashOffset` | 形状的 `Stroke` |

`stroke-dashoffset` 驱动路径描摹的**线条绘制**惯用法（`stroke-dasharray: 1; stroke-dashoffset: 1 → 0`
配合 `pathLength="1"`）；捕获到的作者空间值会像静态 dash 导入一样重新缩放到用户单位。SVG 形状上的
非平移 `transform` 没有 `matrix` 通道（与普通 HTML 元素不同），会被丢弃并告警
（`subset:animation-unsupported-property`）。与普通元素不同，一个内联 SVG 形状可携带**以逗号分隔的
`animation` 列表**——每一项都成为独立的 `Animation`（线条绘制 + 淡入填充的惯用法
`animation: draw 1s …, fill 0.4s …` 是常见场景）。

### 13.6 诊断

| 代码 | 含义 |
|------|------|
| `subset:animation-unsupported-property` | 某个 `@keyframes` 声明面向运行时无法回放的属性/通道；已丢弃 |
| `subset:animation-unknown-keyframes` | `animation` 引用了未定义的 `@keyframes` 名称；该动画被丢弃 |
| `subset:animation-finite-count` | `animation-iteration-count` 是有限值 > 1；被强制为 `once` |
| `subset:animation-multiple` | 以逗号分隔的 `animation` 列表被截断为第一项 |
