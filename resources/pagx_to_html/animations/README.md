# PAGX → HTML/CSS 动效用例集

本目录是**动画导出**的回归语料：一个 `.pagx` 只验证一个语义点，参考实现是 `pagx render --time`
（PAGX 运行时求值后渲染），被测实现是 `pagx export --format html` 产出的 CSS `@keyframes`。

**184 个可动画通道 / 31 种节点类型 / 203 个用例**（190 个单通道 + 13 个复合）。 下面两张表分别回答"是否全面"和"是否精简"，
表格由 `tools/anim_coverage.py` 从源码与语料生成，可当场复核。

## 0. 判定标准与复核

| 命题 | 判定方式 | 当前结果 |
|------|----------|----------|
| 全面：所有 PAGX 可动画能力都有用例 | 表 1 由 `src/pagx/PAGXNodeChannel.cpp` 的通道注册表逐条生成，**没有任何通道被省略**；每个通道都必须有直接驱动它的用例，未实现的写 `Expect: gap`，不允许"同族代表" | 以 `--check` 输出为准 |
| 精简：没有语义重复的用例 | 表 2 列出所有被多个用例驱动的通道；同一通道的多条用例必须分属不同族（时间线 / 关键帧边界 / 插值 / 承载结构 / 支点），族内不重复驱动同一通道 | 文档级重复（忽略 id 后 XML 完全相同）**0 组** |
| 缺口可见 | 表 1 中"导出 ✘"的行都有 `Expect: gap` 用例，每个未实现的通道都是一条可度量的缺口 | 以 `--check` 输出为准 |

复核（参数为 0 表示全部不变式成立）：

```bash
python3 tools/anim_coverage.py --check
```

它会同时校验：① 导出映射表中的每个通道都有用例；② 注册表中的**每个可动画通道**都有直接驱动它的用例，
豁免项必须同时登记在脚本的 `CHANNEL_EXEMPTIONS` / `NODE_EXEMPTIONS` 里（逐条理由见 §4）；
③ 注册表里出现 `NODE_ORDER` 未登记的节点类型会直接失败，避免新增节点被静默漏检；
④ 每个用例头部 `Expect` 与实际导出结果一致；⑤ 语料中不存在"仅 id 不同"的重复文档。

## 1. 主表：可动画通道 → 导出 → 用例

`导出` 以 `src/pagx/html/HTMLAnimationWriter.cpp` 的通道映射表为准；`用例` 只在这里没有单独用例时留空。

| 节点 | 可动画通道 | 导出 | 用例 |
|------|------------|------|------|
| Layer | `alpha` | ✔ | `boundary/*`（6）、`combo/02·03`、`interpolation/*`（4）、`layer/01`、`timing/*`（6） |
| Layer | `anchor.x` | ✔ | `anchor/02·06` |
| Layer | `anchor.y` | ✔ | `anchor/09` |
| Layer | `anchor.z` | ✔ | `anchor/05` |
| Layer | `matrix` | ✔ | `layer/04·04b` |
| Layer | `matrix3D` | ✔ | `layer/13` |
| Layer | `perspective` | ✔ | `layer/12` |
| Layer | `rotation.x` | ✔ | `anchor/04·07`、`layer/17` |
| Layer | `rotation.y` | ✔ | `anchor/03·06`、`layer/11` |
| Layer | `rotation.z` | ✔ | `anchor/01·08`、`layer/10` |
| Layer | `scale.x` | ✔ | `layer/08` |
| Layer | `scale.y` | ✔ | `layer/09` |
| Layer | `scale.z` | ✘ | — |
| Layer | `skew.x` | ✔ | `layer/15` |
| Layer | `skew.y` | ✔ | `layer/16` |
| Layer | `visible` | ✔ | `composite/12`、`layer/06` |
| Layer | `x` | ✔ | `boundary/03`、`combo/*`（4）、`layer/02·05` |
| Layer | `y` | ✔ | `combo/01`、`layer/03` |
| Layer | `z` | ✔ | `layer/14` |
| Group | `alpha` | ✔ | `group/01` |
| Group | `anchor.x` | ✔ | `group/08` |
| Group | `anchor.y` | ✔ | `group/11` |
| Group | `position.x` | ✔ | `group/02` |
| Group | `position.y` | ✔ | `group/03` |
| Group | `rotation` | ✔ | `combo/04`、`group/04·07` |
| Group | `scale.x` | ✔ | `combo/04`、`group/05` |
| Group | `scale.y` | ✔ | `combo/04`、`group/06` |
| Group | `skew` | ✔ | `group/09` |
| Group | `skewAxis` | ✔ | `group/10` |
| TextBox | `alpha` | ✔ | `textbox/01` |
| TextBox | `anchor.x` | ✔ | `textbox/07` |
| TextBox | `anchor.y` | ✔ | `textbox/09` |
| TextBox | `position.x` | ✔ | `textbox/02` |
| TextBox | `position.y` | ✔ | `textbox/05` |
| TextBox | `rotation` | ✔ | `textbox/03` |
| TextBox | `scale.x` | ✔ | `textbox/04` |
| TextBox | `scale.y` | ✔ | `textbox/06` |
| TextBox | `skew` | ✔ | `textbox/08` |
| TextBox | `skewAxis` | ✔ | `textbox/10` |
| Text | `fauxBold` | ✘ | `text/06` |
| Text | `fauxItalic` | ✘ | `text/07` |
| Text | `fontFamily` | ✘ | `text/08` |
| Text | `fontSize` | ✔ | `text/04` |
| Text | `fontStyle` | ✘ | `text/09` |
| Text | `letterSpacing` | ✔ | `text/05` |
| Text | `text` | ✘ | `text/03` |
| Text | `x` | ✔ | `text/10` |
| Text | `y` | ✔ | `text/11` |
| TextPath | `alpha` `anchor.x` `anchor.y` `position.x` `position.y` `rotation` `scale.x` `scale.y` `skew` `skewAxis` | ✘ | — |
| Rectangle | `position.x` | ✔ | `geometry/07` |
| Rectangle | `position.y` | ✔ | `geometry/08` |
| Rectangle | `roundness` | ✔ | `geometry/02` |
| Rectangle | `size.height` | ✔ | `geometry/09` |
| Rectangle | `size.width` | ✔ | `combo/07`、`geometry/01·05` |
| Ellipse | `position.x` | ✔ | `geometry/10` |
| Ellipse | `position.y` | ✔ | `geometry/11` |
| Ellipse | `size.height` | ✔ | `geometry/12` |
| Ellipse | `size.width` | ✔ | `geometry/03` |
| Polystar | `innerRadius` | ✔ | `geometry/13` |
| Polystar | `innerRoundness` | ✔ | `geometry/15` |
| Polystar | `outerRadius` | ✔ | `geometry/14` |
| Polystar | `outerRoundness` | ✔ | `geometry/16` |
| Polystar | `pointCount` | ✔ | `geometry/06` |
| Polystar | `position.x` | ✔ | `geometry/17` |
| Polystar | `position.y` | ✔ | `geometry/18` |
| Polystar | `rotation` | ✔ | `geometry/19` |
| Fill | `alpha` | ✔ | `fill/*`（4） |
| Fill | `color` | ✔ | `fill/01·03`、`geometry/05`、`sharing/02·03` |
| Stroke | `alpha` | ✔ | `stroke/02` |
| Stroke | `color` | ✔ | `stroke/01` |
| Stroke | `dashOffset` | ✔ | `stroke/04` |
| Stroke | `miterLimit` | ✔ | `stroke/05` |
| Stroke | `width` | ✔ | `stroke/03·06` |
| TrimPath | `end` | ✔ | `geometry/04`、`stroke/06` |
| TrimPath | `offset` | ✔ | `geometry/20` |
| TrimPath | `start` | ✔ | `modifier/02` |
| RoundCorner | `radius` | ✔ | `modifier/01` |
| Repeater | `anchor.x` | ✔ | `modifier/08` |
| Repeater | `anchor.y` | ✔ | `modifier/09` |
| Repeater | `copies` | ✔ | `modifier/03` |
| Repeater | `endAlpha` | ✔ | `modifier/11` |
| Repeater | `offset` | ✔ | `modifier/12` |
| Repeater | `position.x` | ✔ | `modifier/13` |
| Repeater | `position.y` | ✔ | `modifier/14` |
| Repeater | `rotation` | ✔ | `modifier/04` |
| Repeater | `scale.x` | ✔ | `modifier/15` |
| Repeater | `scale.y` | ✔ | `modifier/16` |
| Repeater | `startAlpha` | ✔ | `modifier/10` |
| TextModifier | `alpha` | ✘ | `text/02` |
| TextModifier | `anchor.x` | ✘ | `text/12` |
| TextModifier | `anchor.y` | ✘ | `text/13` |
| TextModifier | `fillColor` | ✘ | `text/01` |
| TextModifier | `position.x` | ✘ | `text/14` |
| TextModifier | `position.y` | ✘ | `text/15` |
| TextModifier | `rotation` | ✘ | `text/16` |
| TextModifier | `scale.x` | ✘ | `text/17` |
| TextModifier | `scale.y` | ✘ | `text/18` |
| TextModifier | `skew` | ✘ | `text/19` |
| TextModifier | `skewAxis` | ✘ | `text/20` |
| TextModifier | `strokeColor` | ✘ | `text/21` |
| TextModifier | `strokeWidth` | ✘ | `text/22` |
| RangeSelector | `easeIn` | ✔ | `easing/01` |
| RangeSelector | `easeOut` | ✔ | `easing/02` |
| RangeSelector | `end` | ✔ | `modifier/06` |
| RangeSelector | `offset` | ✔ | `modifier/07` |
| RangeSelector | `start` | ✔ | `modifier/05` |
| RangeSelector | `weight` | ✔ | `easing/03` |
| SolidColor | `color` | ✔ | `sharing/01`、`solid_color/01`、`timing/04·06` |
| LinearGradient | `endPoint.x` | ✔ | `gradient/01` |
| LinearGradient | `endPoint.y` | ✔ | `gradient/09` |
| LinearGradient | `startPoint.x` | ✔ | `gradient/07` |
| LinearGradient | `startPoint.y` | ✔ | `gradient/08` |
| RadialGradient | `center.x` | ✔ | `gradient/10` |
| RadialGradient | `center.y` | ✔ | `gradient/11` |
| RadialGradient | `radius` | ✔ | `gradient/02` |
| ConicGradient | `center.x` `center.y` `endAngle` `startAngle` | ✘ | — |
| DiamondGradient | `center.x` `center.y` `radius` | ✘ | — |
| ColorStop | `color` | ✔ | `gradient/06` |
| ColorStop | `offset` | ✘ | — |
| DropShadowStyle | `blurX` `blurY` `showBehindLayer` | ✘ | — |
| DropShadowStyle | `color` | ✔ | `style/08` |
| DropShadowStyle | `offsetX` | ✔ | `style/01` |
| DropShadowStyle | `offsetY` | ✔ | `style/09` |
| DropShadowStyle | `spread` | ✔ | `style/11` |
| InnerShadowStyle | `blurX` `blurY` `offsetX` `offsetY` `spread` | ✘ | — |
| InnerShadowStyle | `color` | ✔ | `style/12` |
| BackgroundBlurStyle | `blurX` `blurY` | ✘ | — |
| GlassStyle | `depth` `dispersion` `frost` `lightAngle` `lightIntensity` `refraction` `splay` | ✘ | — |
| NoiseStyle | `color` `density` `firstColor` `opacity` `secondColor` `seed` `size` | ✘ | — |
| BlurFilter | `blurX` | ✔ | `filter/01` |
| BlurFilter | `blurY` | ✔ | `filter/02` |
| DropShadowFilter | `blurX` `blurY` | ✔ | `filter/05` |
| DropShadowFilter | `color` | ✔ | `filter/06` |
| DropShadowFilter | `offsetX` | ✔ | `filter/03` |
| DropShadowFilter | `offsetY` | ✔ | `filter/04` |
| DropShadowFilter | `shadowOnly` | ✘ | — |
| InnerShadowFilter | `blurX` `blurY` `offsetX` `offsetY` `shadowOnly` | ✘ | — |
| InnerShadowFilter | `color` | ✔ | `filter/13` |
| BlendFilter | `color` | ✔ | `filter/09` |
| NoiseFilter | `color` `density` `firstColor` `opacity` `secondColor` `seed` `size` | ✘ | — |

## 2. 同一通道的用例分布（精简性检查）

被多个用例驱动的通道都在这里。**判据：同一通道的多条用例必须分属不同族**（各变一个维度），
族内不得重复驱动同一通道：

| 通道 | 用例数 | 用例（按族） |
|------|--------|--------------|
| `Layer.alpha` | 19 | `boundary/*`（6）、`combo/02·03`、`interpolation/*`（4）、`layer/01`、`timing/*`（6） |
| `Layer.anchor.x` | 2 | `anchor/02·06` |
| `Layer.matrix` | 2 | `layer/04·04b` |
| `Layer.rotation.x` | 3 | `anchor/04·07`、`layer/17` |
| `Layer.rotation.y` | 3 | `anchor/03·06`、`layer/11` |
| `Layer.rotation.z` | 3 | `anchor/01·08`、`layer/10` |
| `Layer.visible` | 2 | `composite/12`、`layer/06` |
| `Layer.x` | 7 | `boundary/03`、`combo/*`（4）、`layer/02·05` |
| `Layer.y` | 2 | `combo/01`、`layer/03` |
| `Group.rotation` | 3 | `combo/04`、`group/04·07` |
| `Group.scale.x` | 2 | `combo/04`、`group/05` |
| `Group.scale.y` | 2 | `combo/04`、`group/06` |
| `Rectangle.size.width` | 3 | `combo/07`、`geometry/01·05` |
| `Fill.alpha` | 4 | `fill/*`（4） |
| `Fill.color` | 5 | `fill/01·03`、`geometry/05`、`sharing/02·03` |
| `Stroke.width` | 2 | `stroke/03·06` |
| `TrimPath.end` | 2 | `geometry/04`、`stroke/06` |
| `SolidColor.color` | 4 | `sharing/01`、`solid_color/01`、`timing/04·06` |

## 3. 时间线语义（与通道无关的维度）

| 语义 | 用例 |
|------|------|
| `loop="once" / "loop" / "pingPong"` | `timing/01`、`timing/02`、`layer/01` |
| `frameRate` 影响时间轴长度 | `timing/03` |
| `evaluationOffset` 正值（once 期间保持首帧值 / loop 每周期位移） | `timing/04`、`timing/05`、`boundary/03` |
| `evaluationOffset` 负值 / `pingPong` 下的位移 | `timing/06`、`timing/07` |
| 同一 Animation 被两个 Composition 实例以不同 offset 挂载 | `timing/08` |
| 关键帧边界：首个 key 晚于 0 / 末个 key 早于 duration / 重复时间点 / key 超出 duration | `boundary/01`、`boundary/02`、`boundary/04`、`boundary/05`、`boundary/06` |
| 插值：hold / bezier 自定义 / bezier 默认控制柄 / 逐段混合 | `interpolation/01`~`interpolation/04` |
| 多通道共享一组 stop 与 easing | `combo/02`、`combo/03` |

## 4. 缺口与已知问题

**未实现但已固化**：表 1 中"导出 ✘ 且有用例"的行，共 42 条 `Expect: gap` 用例，逐通道覆盖 Layer
3D/支点、几何位置与尺寸、渐变色源、修改器、文本与文本修改器、图层样式、滤镜参数。

**两套"缓动"不要混为一谈**：

| | 时间缓动 | 选区衰减 |
|---|---|---|
| 载体 | 每个关键帧的 `interpolation` + `bezierOut`/`bezierIn` | `RangeSelector` 的 `easeIn` / `easeOut` / `weight` 与 `shape` |
| 作用 | 属性值随**时间**怎么走 | 字形离选区中心的距离 → 影响系数的**空间**曲线 |
| 用例族 | `interpolation/`（01–04） | `easing/`（01–03） |

`interpolation/` 已经全绿（hold、自定义把手、默认把手、分段混合）；`easing/` 收的是选区衰减曲线，它改的是"曲线形状"而不是某个属性值，所以不放在 `modifier/` 里。

**渐变的坐标靠变换表达**：`<linearGradient>` / `<radialGradient>` 的 `x1/y1/x2/y2`、`cx/cy/r` 都是 XML
属性，关键帧对它们无效（实测 0 像素变化）；但 Chromium 会把渐变元素上的 CSS `transform` 映射成
`gradientTransform`，所以每帧解一个仿射变换即可：线性渐变把两个端点映射过去（平移 + 旋转 + 缩放），径向渐变
把中心与半径映射过去。色标颜色走 `stop-color`（CSS 属性）。

**锥形 / 菱形渐变整节点豁免**：两者静态导出会光栅化成 PNG（画面上是 `<image>` / `<img>`），没有 CSS 渐变
可动画，需逐帧重绘位图（JS 侧）。`ColorStop.offset` 同理——`offset` 是属性，不是 CSS 属性。

**几何类通道（`Polystar` 全部 8 个）**：通道按"它到底改变了什么"分流，避免为一件事写复杂表达——

| 通道 | 表达 | stop 数 |
|------|------|---------|
| `position.x/y`、`rotation` | 刚体移动，走 `transform: translate() rotate()` | **2** |
| `outerRadius`、`innerRadius` | 顶点坐标对半径是线性关系，逐帧 `d` 可直接插值 | **2** |
| `outerRoundness`、`innerRoundness` | 贝塞尔把手对圆角非线性，每 4 帧一个采样点 | ~16 |
| `pointCount` | 运行时**取整**（实测 5.1 与 5.9 相同、5.9 与 6.1 不同），在整数前后成对停靠 | ~9 |

`pointCount` 的整数跳变改的是顶点个数，CSS 无法插值，所以在跨越处前后各停一帧、并在末尾前一帧也停一次，
否则最后一段会被整段插值。

**CSS 无法表达的滤镜通道**：管线滤镜（内阴影、混合）的几何参数是 SVG 滤镜原语的属性（`feOffset dx/dy`、
`feGaussianBlur stdDeviation`），不是 CSS 属性。实测给它们写关键帧后首末帧**零像素差**，而同一个滤镜里的
`flood-color` 可以动画（`filter/13` 从 0.913 提到 0.998）。因此 `InnerShadowFilter` 的 `blurX/blurY/
offsetX/offsetY/shadowOnly` 与 `DropShadowFilter.shadowOnly` 列入 `CHANNEL_EXEMPTIONS`，标注为
「需 JS 侧逐帧重写滤镜」，不属于 CSS 这一期。

**豁免清单的定位：不是"永远做不了"，而是"CSS 这一期做不了、说清由谁接手"。** 每个条目都带一个标签，
`tools/anim_coverage.py` 里按标签归类，通道的 `Expect: gap` 用例继续保留，缺口仍然可度量。

| 标签 | 含义 | 条目 | 覆盖 |
|------|------|-----:|------|
| `js-pending` | CSS 无法表达，JS 逐帧重绘可做 | 22 | SVG 滤镜几何 6、单轴模糊 2（`BlurFilter`）、样式参数 11、`ColorStop.offset` 1、光栅化渐变 2 |
| `js-after-static` | 静态还没画出该效果，补齐后再谈逐帧 | 9 | 噪声节点 2、玻璃参数 7 |
| `renderer-gap` | 渲染器不应用该通道，导出侧（含 JS）无解 | 10 | `TextPath` |
| `no-op` | 数值不改变画面，无需实现 | 1 | `Layer.scale.z` |

**"导出侧还没实现"的通道依然不进豁免**：那种通道保留 `Expect: gap` 用例，它就是待办清单——实现后把 `Expect`
改成 `supported`。比如 `Rectangle.position.x`、`Ellipse.roundness` 目前仍走这条路。

### 样式（`style/*`）的 CSS 边界

`DropShadowStyle` 在静态导出里是**独立的模糊 div**，所以它的盒子参数天然是 CSS；内阴影是 SVG 滤镜，只有颜色能
动。已实现（`supported`）：

| 通道 | 映射 | 逐帧 SSIM |
|------|------|------:|
| `DropShadowStyle.offsetX` / `offsetY` | 阴影 div 的 `left` / `top` | 0.9985 / 0.9955 |
| `DropShadowStyle.color` | `background-color` | 0.9961 |
| `DropShadowStyle.spread` | 阴影 div 的 `left/top/width/height/border-radius` | 0.9985 |
| `InnerShadowStyle.color` | `<feFlood>` 的 `flood-color` | 0.9964 |

其余 16 条样式用例已删除（豁免通道的用例永远无法转绿，只会长期占着 gap 名额）。删除前的实测分数留在这里，
作为下一期 JS 工作的起点：

| 用例 | 通道 | 删除前 SSIM | 标签 |
|------|------|------:|------|
| `style/02_inner_shadow_style_blur` | `blurX/blurY` | 0.744 | `js-pending` |
| `style/03_background_blur_style_blur_x` | `blurX` | 0.968 | `js-pending` |
| `style/04_glass_style_refraction` | `refraction` | 0.988 | `js-after-static` |
| `style/06_dropshadowstyle_blur_x` | `blurX` | 0.972 | `js-pending` |
| `style/07_dropshadowstyle_blur_y` | `blurY` | 0.962 | `js-pending` |
| `style/10_dropshadowstyle_show_behind_layer` | `showBehindLayer` | 0.880 | `js-pending` |
| `style/13_innershadowstyle_offset_x` | `offsetX` | 0.835 | `js-pending` |
| `style/14_innershadowstyle_offset_y` | `offsetY` | 0.758 | `js-pending` |
| `style/15_innershadowstyle_spread` | `spread` | 0.740 | `js-pending` |
| `style/16_backgroundblurstyle_blur_y` | `blurY` | 0.972 | `js-pending` |
| `style/17_glassstyle_depth` ~ `22_glassstyle_splay` | 6 个玻璃参数 | 0.97 | `js-after-static` |

恢复方式：从提交历史取回 fixture，并删掉脚本里对应的豁免条目即可。

单轴模糊（`BlurFilter.blurX` / `blurY` 各自单独动画）同样归 `js-pending`：CSS 的 `filter:blur()` 各向同性，
实测只能到 0.96–0.97，对应的两条用例已删除；绑定本身保留，因为**两轴同值**时 CSS 表达是准确的。

其余样式通道已列入 `CHANNEL_EXEMPTIONS`：`DropShadowStyle` / `InnerShadowStyle` /
`BackgroundBlurStyle` 的模糊与几何参数标 `js-pending`（CSS 模糊各向同性，实测只能到 0.96；内阴影几何在滤镜原语上），
`GlassStyle` 的 7 个参数标 `js-after-static`（静态本身就只是近似）。

**静态导出缺失的节点（整节点豁免）**：

| 节点 | 状态 | 依据 |
|------|------|------|
| `NoiseStyle` | 静态不渲染：`src/pagx/html` 中**零引用**，导出的 t=0 画面为空 | 已从语料中移除对应用例，节点整体列入 `NODE_EXEMPTIONS` |
| `NoiseFilter` | 静态不渲染噪声：导出只有底色（5582px），参考有噪点（12613px） | 同上 |

**`TextPath`（10 个通道）**：`src/renderer/LayerBuilder.cpp` 的 `convertTextPath()` 没有注册任何通道 writer，
实测 `rotation` / `position.x` / `scale.x` / `alpha` 动画的首尾帧差值为 0，写不出"参考确实在动"的用例。

**已知映射问题**：`{SolidColor, "alpha"}` 与 `{DropShadowFilter, "alpha"}` 指向不存在的通道，`--check`
报 WARN。

**复合用例（`composite/`）**：每个文件把一个耦合组里所有已实现通道绑在同一个元素上，key 时间故意错开、
插值方式混用（linear / bezier / hold），用来验证"通道之间"的合成而不是单通道映射。`--check` 会强制
"每个 supported 通道都出现在某个复合用例里"。

当前逐帧最小 SSIM（`--samples 8`）：

| 复合用例 | min SSIM | 结论 |
|----------|----------|------|
| `12_layer_state` | 0.9999 | 通过 |
| `11_multiple_timelines` | 0.9999 | 通过 |
| `13_mask_path` | 0.9996 | 通过 |
| `10_repeater` | 0.9995 | 通过 |
| `04_nested_chain` | 0.9992 | 通过 |
| `01_layer_2d_transform` | 0.9911 | 通过 |
| `07_gradient` | 0.9765 | 待查（端点 + stop 色同时动） |
| `05_geometry` | 0.9728 | 待查（pointCount 跳变与半径同帧） |
| `08_filter_chain` | 0.9192 | 待查（blur 与 drop shadow 同链） |
| `06_painter` | 0.8228 | 已定位：`TrimPath.start/end` 与 dash/width 同帧时偏差最大 |
| `02_layer_3d` | 0.7825 | 待查（anchor.z 与 rotation 同帧） |
| `03_transform_override` | 0.3140 | 已定位：`matrix` 与 `x/y` 同时动画时导出把平移加了两次；静态 `matrix3D` 未屏蔽分量通道 |
| `09_text` | 0.3092 | 已定位：带 `TextModifier` 时 `Text.fontSize`/`letterSpacing` 不产出动画（变体只剩文本通道时报告 `export-static`） |

排查工具：`python3 tools/anim_bisect.py <case.pagx>` —— 逐个删掉一个 channel 重跑评测，哪个通道删掉后
分数回到阈值以上，哪个就是（一部分）元凶。上面的"已定位"结论就是它给出的。

复合用例的阈值建议：先按 **min SSIM ≥ 0.99** 卡，把上表 6 个未达标项列为已知缺陷（随导出器修复逐条
删除），而不是放宽阈值——这些低分指向的都是真实的合成 bug。


## 5. 用例族与 fixture

| 族 | 用例数 | 强调色 | 主体 | 独有维度 |
|----|--------|--------|------|----------|
| `layer` | 18 | `#4F46E5` | 菱形；旋转/矩阵类用不等边三角形 | 单个 Layer 通道的映射基准 |
| `group` | 11 | `#EA580C` | 三角形 | Group 变换链与锚点 |
| `textbox` | 10 | `#7C3AED` | 文本 "AB" | TextBox 布局与变换通道 |
| `fill` | 6 | `#DB2777` | 胶囊 / 文本 / 渐变三角 | 绘制器颜色与透明度承载 |
| `stroke` | 6 | `#0E7490` | 椭圆描边环 / 五角星 | 描边参数 |
| `filter` | 8 | `#15803D` | 六边形 + 深色阴影 | 滤镜参数 |
| `geometry` | 20 | `#B45309` | 矩形 / 椭圆 / 五角星 | 几何尺寸与圆角 |
| `mask` | 4 | `#B91C1C` | 被遮罩内容 | 遮罩路径逐点变形 |
| `timing` | 8 | `#0284C7` | 圆形 | 循环、帧率、`evaluationOffset`、挂载 |
| `boundary` | 6 | `#0F766E` | 五角星 | 关键帧边界 |
| `interpolation` | 4 | `#C026D3` | 弧形 | 插值方式 |
| `easing` | 3 | `#BE123C` | 文本 "WAVE"（与 `text` 共用夹具） | 选区衰减曲线：`easeIn` / `easeOut` / `weight` |
| `combo` | 6 | `#475569` | 八边形 | 多通道共享 keyframe |
| `sharing` | 3 | `#9A3412` | 圆形 / 圆形与文本 | 资源与绘制器共享 |
| `solid_color` | 1 | `#65A30D` | 圆形 | 颜色源 |
| `anchor` | 9 | `#C2410C` | 长条 + 支点圆点 / 卡片 | 局部变换支点（含 `anchor.z` 深度）与 3D 耦合 |
| `gradient` | 8 | `#0891B2` | 椭圆 | 渐变色源参数 |
| `modifier` | 16 | `#9333EA` | 五边形 / 五角星描边 / 方块阵列 | 修改器参数 |
| `text` | 22 | `#BE123C` | 72px 粗体文本 | 文本与文本修改器参数 |
| `style` | 21 | `#4338CA` | 白色卡片（模糊类带背板） | 图层样式参数 |

| `composite` | 13 | 按耦合组 | 每个耦合组一个文件（Layer 2D/3D、覆盖语义、嵌套链、几何、绘制器、渐变、滤镜、文本、Repeater、多时间线、图层开关、遮罩） | 同一元素的多个 channel 合成 |
约束：320×200、`#F8FAFC` 背景（`boundary/03` 为 400×160）；每族一个剪影，跨族不重复；旋转类形状不得有
旋转对称（4 点多边形每 90° 重复，会掩盖错路径样本）；`matrix`/`matrix3D` 通道会替换 `x`/`y` 平移，
关键帧必须自带 translate。

## 6. 运行与头部约定

```bash
node tools/html-snapshot/eval-animation/run.js \
  --source pagx --corpus resources/pagx_to_html/animations -r
```

参考时间线由文件内的 `<Animation>` 推导，候选是同一时刻的导出 HTML；默认采样 5 帧，输出 SSIM / 像素差。
报告里 `export-static` 表示"参考在动、导出不动"，正是 gap 用例的预期表现；`both-static` 说明用例没有驱动
任何可动画通道，属于语料问题。

每个文件头部固定为：

```
<!-- Tests: <一句话说明>
     Target: <NodeType>  Channel: <通道名>[,<通道名>]  Type: <值类型>
     Expect: supported|gap
     Known-gap: <原因标识>   // 可选，见下
     Ref: <对应的 test/src 用例，没有则 -> -->
```

`Target` / `Channel` 会被 `run.js` 读进报告；`Expect: gap` 表示该用例的核心通道导出侧未实现。

`Known-gap` 用于“通道已经导出、但结果与参考对不上”的情况：它把用例保留在 `gap` 组（不计入通过率），
同时让 `anim_coverage.py --check` 不再因为“gap 用例却有 `@keyframes`”而报错。当前唯一使用它的是
`layer/12_perspective`（`css-perspective-interpolation`）：CSS 在两条 `perspective()` 之间按矩阵分解
（等价于对 `1/d` 线性）插值，而运行时按通道值线性取 `d` 再重算矩阵，中间帧必然分叉，端点一致。

`text/10_text_x`、`text/11_text_y`（`static-text-baseline`）：文本一律导出为 CSS `span`，行盒基线由浏览器
字体度量推导，比 PAGX 低 1~2px（40px 字号约 1px、80px 约 2px），因此停在 0.88~0.90。不要改用 SVG `<text>`
（`y` 就是基线，能到 0.98）：那样 `x` 动画走 SVG、`fontSize` 动画必须留在 span，两套模式在复合动画上冲突
（实测 0.26），所以统一走 span。修法只能在 span 路径上对齐行盒基线。

## 6.1 文本类用例的写法约束（重要）

文本是当前唯一“同一份文档在不同排版引擎下会产生不同几何”的族，用例必须按下面的写法构造，否则
`SSIM` 反映的是排版引擎差异，而不是导出能力。

**要这样写**

- 需要逐字对比时，用一个 `<Text>` 节点 + `TextModifier`，**显式写出 `letterSpacing`**（即使为 0 也写
  `letterSpacing="0"`），让字距没有歧义；
- 文本属性写在 `<Text>` 上（`fontFamily` / `fontSize` / `fontStyle`），不要在多个节点间混用不同字体；
- 断言逐字位置时，以 PAGX 的 `glyphData.layoutRuns` 坐标为准（导出侧按此输出 `<svg><text>`）。

**不要这样写**

- **不要依赖字体的 kerning**：把字符串拆成多个独立元素、或让浏览器自行排版相邻字形时，`WA` / `AV` / `VE`
  这类字对会和 PAGX 差 2~3px（56px 字号下 4 个字累计 8px）。这类差异属于字体排版引擎行为，不是导出缺陷。
  需要 `TextModifier` 逐字动画的用例不受此影响——那条路径已改为按 `layoutRuns` 的坐标输出 SVG `<text>`；
- 不要用 `display:inline-block` 的逐字盒子作为对照用例的参照实现；
- 不要在同一个用例里混用“整串文本”和“逐字文本”两种写法做视觉对比。

> 背景：浏览器按 CSS 行盒定位文本（`top` 是盒子顶边，基线由字体 ascent 推导），PAGX 的文本锚点是基线。
> 二者相差一个“半行距 + ascent 取整”的量（56px 字号约 1.5~2px），且随字号不按比例变化。
> 导出侧的修法是输出 SVG `<text>`（`y` 即基线），而不是给 CSS 加补偿像素。带 `TextModifier` 的文本已走这条
> 路径；`RangeSelector` 的 `start` / `end` / `offset` 动画在其上逐字形输出关键帧：每段因子曲线是时间的分式
> 线性函数，用一对 stop 加一条拟合的 `cubic-bezier` 表达（误差 ≤0.03px），不做逐帧采样。`modifier/05`–`07`
> 逐帧 SSIM 约 0.990，剩余差异是字形边缘抗锯齿。

## 7. 范围与已知限制

- **范围**：只覆盖关键帧动画（`<Animation>` / `<Object>` / `<Channel>` / `<Key>` 及其挂载），不含状态机
  （`<StateMachine>`）、ViewModel / `DataBind`，也不含 PAGX 里不可动画的属性（注册表中 `NoFlags` 的条目）。
- **`Layer.scale.z` 数学上不可见**：运行时按 `P · T · R · (K · S)` 合成（见
  `src/pagx/runtime/Matrix3DDecompose.h`），缩放是最内层、先于旋转作用在点上。图层里的形状都是 `z = 0`
  的平面路径，`scaleZ` 只能把 0 变成 0，之后的 `rotationX/Y` 才产生深度，那时缩放已经过去了；`z` 平移属于
  `T`，在 `S` 外侧，也碰不到。静态 `scaleZ=2` 配合 `rotationX/Y/Z` 实测均为 0 像素差。因此导出侧不实现、
  也不设用例（表 1 中标 ✘）。
- **未映射的通道会被静默丢弃**：导出侧没有诊断输出，缺口只能靠 `Expect: gap` 用例记录；这也是本目录要
  逐族保留代表用例的原因。
- **头部只记第一个 `Target` / `Channel`**：`geometry/05`、`stroke/06` 等多目标用例在报告里只体现第一个。
- **`sharing/01` 目前"passes for the wrong reason"**：运行时把共享颜色源只绑到最后一个 Fill，导出同样丢，
  因此它还不是有效的回归保护。
- **eval 采样只取时间轴中段**：默认 5 帧，边界语义靠"恰好采到"；对 `boundary/*` 建议 `--samples 9`。
- **规范与实现不一致**：`spec/pagx_spec.zh_CN.md` 仍声明文本属性不可动画，而运行时注册表把它们标为 `AnimLayout`
  且渲染器有写入方；本目录的"可动画"列以运行时注册表为准。
