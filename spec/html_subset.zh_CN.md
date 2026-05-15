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

仅支持 `<head>` 内单一 `<style>` 元素，且选择器形态限定：

- 类选择器：`.cls { … }`
- 逗号分隔的类列表：`.cls-a, .cls-b { … }`
- 限定为允许标签的元素选择器：`body { … }`、`h1 { … }`、`p { … }`
- **不**允许通配 `*`、后代选择器、伪类、属性选择器；遇到时声明保留、选择器忽略并告警。

样式优先级：内联 `style` > 类规则 > 元素默认 > 继承。

### 3.4 外部 CSS

`<link rel="stylesheet">` 一律禁止。转换器不执行任何网络 I/O。

## 4. 允许的 CSS 属性

所有长度均为像素；百分比仅在 `width`/`height` 上接受（按 PAGX 语义参考父节点的
内 padding 框）。`em`、`rem`、`vw`、`vh`、`calc()` 等单位拒收并告警。

### 4.1 布局

| CSS | PAGX | 备注 |
|-----|------|------|
| `display: block` | 默认（无 `layout` 的 `Layer`） | 默认按块流堆叠 |
| `display: flex` | 在父 Layer 开启 `layout` | flex 布局的前提 |
| `flex-direction: row` | `layout="horizontal"` | flex 默认 |
| `flex-direction: column` | `layout="vertical"` | |
| `gap: N` | `gap="N"` | 单值 |
| `padding: …` | `padding="…"` | 支持 `N`、`T R`、`T R B L` |
| `align-items: stretch | center | flex-start | flex-end` | `alignment="stretch|center|start|end"` | |
| `justify-content: flex-start | center | flex-end | space-between | space-evenly | space-around` | `arrangement="start|center|end|spaceBetween|spaceEvenly|spaceAround"` | |
| `flex: N` | `flex="N"` | 仅整数/无量纲 grow 值 |

禁用（告警并跳过）：`margin*`、`flex-wrap`、`flex-grow`、`flex-shrink`、`flex-basis`
（用 `flex: N` 缩写）、`display: inline-block`、`display: grid`、`grid-*`、`float`、
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
| 负值偏移（如 `top: -6px`） | 原样保留 |

`position: relative`、`position: fixed`、`position: sticky` 及在文本叶（`<p>`、`<span>`）上
的绝对定位均告警，并降级为父 Layer 上的绝对定位。

### 4.4 绘制与效果

| CSS | PAGX |
|-----|------|
| `background-color: <color>` | `<Rectangle width="100%" height="100%" roundness="…"/>` + `<Fill color="…"/>` 背景对（见 §5） |
| `background-image: linear-gradient(angle, c1 [p], c2 [p], …)` | `<Fill>` 内嵌 `<LinearGradient>`（`startPoint`/`endPoint` 由角度推得） |
| `background-image: radial-gradient(…)` | 内嵌 `<RadialGradient>` |
| `background-image: conic-gradient(from angle, …)` | 内嵌 `<ConicGradient>`（CSS 0° = 顶，PAGX 0° = 右，自动 −90°） |
| `border-radius: N` | `Rectangle.roundness = N` |
| `border: W solid C` | `<Stroke color="C" width="W" align="inside"/>` |
| `box-shadow: X Y B C`（多重、可加 `inset`） | 每个阴影一份 `<DropShadowStyle>` 或 `<InnerShadowStyle>` |
| `opacity: A` | `Layer.alpha = A` |
| `mix-blend-mode: <mode>` | `Layer.blendMode = <mode>` |
| `filter: blur(X) drop-shadow(X Y B C)` | `<BlurFilter>` / `<DropShadowFilter>` 链 |
| `backdrop-filter: blur(X)` | `<BackgroundBlurStyle>` |
| Layer 上的 `overflow: hidden` | `Layer.clipToBounds = true` |

禁用（告警并跳过）：除渐变外的 `background-image: url(...)`（请用 `<img>`）、
`background-size`、`background-repeat`、`background-position`、单边 border、多样式
`border`、`outline`、`transform`、`transform-origin`、`perspective`、`clip-path`。

### 4.5 文本

| CSS | PAGX |
|-----|------|
| `color: <color>` | 文本旁的 `<Fill color="…"/>` |
| `font-family: name` | `Text.fontFamily` |
| `font-size: N px` | `Text.fontSize` |
| `font-weight: bold | 700+` | `Text.fontStyle = "Bold"`（与斜体组合 `"Bold Italic"`） |
| `font-style: italic | oblique` | `Text.fontStyle = "Italic"`（或 `"Bold Italic"`） |
| `letter-spacing: N px` | `Text.letterSpacing` |
| `text-align: start | left | center | right | justify` | `TextBox.textAlign = "start|center|end|justify"`（`left` → `start`，`right` → `end`） |
| `line-height: N px`（数字形式不支持） | `TextBox.lineHeight` |
| `text-decoration: underline | line-through` | 1px `<Rectangle>` 叠加（`bottom="0"` / `centerY="0"`），见 §6 |
| `white-space: nowrap` | `TextBox.wordWrap = false` |
| 文本容器上的 `overflow: hidden` | `TextBox.overflow = "hidden"` |
| `text-overflow: ellipsis` | 告警（PAGX 暂未实现） |

禁用（告警并跳过）：`text-transform`、`text-indent`、`word-spacing`、`direction`、
`unicode-bidi`、`font-variant`、`font-stretch`、`font` 缩写、web fonts (`@font-face`)。

### 4.6 自定义数据

`data-*` 前缀的属性按 PAGX `data-*` 约定（见 `spec/pagx_spec.zh_CN.md` §2.3）原样保留
到对应 PAGX 节点。HTML 的 `id` 属性在去重后透传为 Layer 的 `id`。

## 5. 容器与背景

当一个 HTML 元素同时拥有绘制背景（颜色/渐变/边框/阴影/圆角）和需要 padding/layout
的子节点时，转换器输出 PAGX 标准的"外层背景 + 内层 padding 容器"形态（参考 PAGX
guide §Container Layout）：

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

下划线与删除线按 `references/patterns.md` §Text Decoration 输出为同 Layer 内的叠加
矩形：

- `text-decoration: underline` → 1px `<Rectangle>` 置于 `bottom="0"`
- `text-decoration: line-through` → 1px `<Rectangle>` 置于 `centerY="0"`

装饰颜色与文本颜色不同时，矩形包入 Group 以隔离其 `<Fill>`。

## 7. 图像与内联 SVG

- `<img src="path" width="W" height="H"/>` 将 `src` 注册为 `<Image>` 资源（接受 data URI；
  相对路径相对输入文件目录解析），并输出 `<Rectangle width="100%" height="100%"/>` +
  `<ImagePattern image="@id"/>`。
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

## 8. 坐标 / 角度约定

- HTML 坐标系与 PAGX 一致（左上原点，y 向下）。
- CSS 渐变角度从上方顺时针计；PAGX 渐变角度从 +X 轴计。转换公式：
  `pagx_angle = css_angle − 90°`，对 `linear-gradient` 与 `conic-gradient` 都适用。
- `linear-gradient` 关键字（如 `to bottom right`）会折算为数值角度。

## 9. 常见陷阱（与 `references/guide.md` §Common Pitfalls 一致）

- 不要使用 `margin`。请用 `padding`、`gap` 或 `flex: N`。
- 不要在 flex 主轴上给子节点显式设置 width/height —— 用 `flex: N` 分配剩余空间。
- 不要把 `position: absolute` 子节点与已有 `gap` / `align-items` 的 flex 父混用。
- 不要用文本字符（`+`、`×`、`→` 等）替代图标 —— 改用内联 `<svg>`。

## 10. 诊断

转换器通过 `PAGXDocument::errors` 输出诊断，分级：

- **error**：阻断式问题（缺少 `<body>`、画布尺寸缺失）。CLI 中通过 `ImportResult::error`
  返回。
- **warning**：元素或属性被跳过或降级。CLI 中通过 `ImportResult::warnings` 输出。
- **note**：信息性提示，例如自动填充字号。

CLI 选项：

- `--html-strict`：把 warning 当作 error（用于 CI）。
- `--html-preserve-unknown`：把未知标签保留为带 `data-html-unknown="<tag>"` 的空
  Layer，便于排查。
- `--html-no-normalize`：跳过子集归一化前置流程（见 §11），用于在已经满足子集的 HTML
  上直接调试导入器。
- `--html-infer-flex`：启用 `AbsoluteToFlexInference` Pass（见 §11），从 flat
  `position: absolute` 输入（典型场景：`tools/html-snapshot/snapshot.js` 输出）反推
  `display: flex`。属于有损启发式重写，需要显式启用。

## 11. 自动归一化（Auto-normalization）

导入器在遍历 DOM 之前会先运行 `HTMLSubsetTransformer`（参见
`include/pagx/HTMLSubsetTransformer.h`），把任意输入改写为严格符合本子集的 DOM，让后续
代码只接触合规的 HTML。该过程默认开启；通过
`HTMLImporter::Options::autoNormalize = false`（或 `--html-no-normalize`）可以关闭。

转换器按固定顺序执行六个核心 Pass，外加一个可选 Pass，行为如下：

| Pass | 静默改写 | 警告并丢弃 |
|------|----------|------------|
| DocumentSkeleton | 合并重复的 `<head>` / `<body>`、小写化标签、去掉注释与处理指令、丢弃 `<head>` 中除 `<title>` / `<meta>` / `<style>` 之外的元素 | `<script>` 等不允许的 `<head>` 内容（`subset:unsupported-tag`）；外部 `<link rel="stylesheet">`（`subset:external-stylesheet`）；`<html>` 与 `<body>` 之间的多余顶层元素（`subset:unsupported-tag`） |
| StyleSheetCollector | 把 `<style>` 中的类选择器与元素选择器规则展开到每个元素的内联级联里，并移除 `<style>` 元素本身 | `*`、后代/伪类/属性选择器（`subset:unsupported-selector`）；`@media`、`@font-face`、`@keyframes` 等 at-rule（`subset:unsupported-at-rule`） |
| ComputedStyle | 完成 CSS 级联（继承 → 元素默认 → 元素规则 → 类规则 → 内联 `style`），并把合并后的属性写回内联 style | — |
| PropertyFilter | 把 `em` 转 px（基准为父元素计算后的 `font-size`），`rem` 转 px（基准 16px），`vw`/`vh` 转 px（基准为画布尺寸），`pt` 转 px；把 `flex: <grow> <shrink> <basis>` 收敛为 `flex: <grow>`（`subset:flex-shorthand-collapsed`）；把 `position: relative \| fixed \| sticky` 降级为 `position: absolute` | `margin*`、`transform*`、`clip-path`、`outline`、`float`、`order`、`align-content`、`align-self`、`direction`、`unicode-bidi`、`flex-wrap`、`flex-grow`、`flex-shrink`、`flex-basis`、`min-*`、`max-*`、`aspect-ratio`、`background-size`、`background-repeat`、`background-position`、`text-transform`、`text-indent`、`word-*`、`overflow-wrap`、`font-variant`、`font-stretch`、`font` 简写、`grid-*`、单边 `border-*`、单角 `border-*-radius`、`z-index`、`cursor`、`pointer-events`、`user-select`、`visibility`（`subset:unsupported-property`）；`var()`、`calc()`、`min/max/clamp()`（`subset:unsupported-property`）；其它不支持的单位（`subset:unsupported-property`） |
| AbsoluteToFlexInference *（可选：`Options::inferFlexFromAbsolute` / `--html-infer-flex`）* | 当某容器的所有子元素都是 `position: absolute` 且能整齐拼成一行/一列时，把容器改写为 `display: flex` 并填入推断出的 `gap`、`padding`、`align-items`、`flex-direction`，同时移除子元素的 `position` / `left` / `right` / `top` / `bottom`（`subset:flex-inferred`） | 子元素在两个轴向都重叠、跨轴对齐方式不一致、或主轴间距不均匀的容器一律保留绝对定位（`subset:flex-inference-skipped`） |
| StructureNormalization | 把容器中的散落文本包进 `<p>`（`subset:text-wrapped`）；丢掉元素之间纯空白的文本节点；保持 `<svg>` 子树原样以供 SVG 解析器使用 | 未知标签（`<table>`、`<form>`、`<input>`、`<button>`、自定义元素等）被移除（`subset:unsupported-tag`）；启用 `--html-preserve-unknown` 时会被保留为 `<div data-html-unknown="<tag>">` |
| InlineStyleEmitter | 把每个元素解析后的属性集合按字母顺序重新写回 `style="…"`（保证可重复输出）；同时丢掉已经被级联吸收的 `class` 属性（`Options::preserveClassAttribute` 为真时保留） | — |

所有诊断都使用统一的 `subset:<category>` 前缀，并经由 `PAGXDocument::errors`（CLI 下经
`ImportResult::warnings`）输出。在严格模式（`--html-strict`）下，任一警告都会立即升级为错误
并终止导入。

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
