# PAGX → HTML/CSS 动效用例集

本目录收录**关键帧动画**从 PAGX 导出为 HTML/CSS 的回归用例，并记录 PAGX 可动画能力与用例覆盖的对应关系。

- 用例形态：手写 `.pagx`，一个文件只验证一个语义点（少数文件验证两个通道的交互）。
- 参考实现：`pagx render --time <s>`（PAGX 运行时按时间求值后渲染），即"正确答案"。
- 被测实现：`pagx export --format html` 产出的 CSS `@keyframes` + `animation`，在浏览器中 seek 到同一时刻截图。
- 判定方式：逐帧 SSIM / 像素差 / RGB 差；**只出报告，不设逐例门禁**（见 `tools/html-snapshot/eval-animation/README.md`）。

## 运行

```bash
node tools/html-snapshot/eval-animation/run.js \
  --source pagx --corpus resources/pagx_to_html/animations --samples 5 -r
```

`--source pagx` 只接受 `.pagx`：参考时间线由文件内的 `<Animation>` 自行推导（`duration`/`frameRate`/`loop`/`evaluationOffset`），
候选时间线是同一时刻的导出 HTML。默认采样 5 帧（`(i+0.5)/5` 的等分点，不落在 0 与末端），
落在整数帧附近的采样点会被挪开半帧，避免浏览器与运行时在关键帧边界上取到不同侧。

报告里每例会带一个 `flag`，用于区分"参考也没动"和"导出丢了动画"：

| flag | 含义 |
|------|------|
| `both-static` | 参考与导出都不动 —— 该用例没有真正驱动任何可动画通道 |
| `ref-static` | 只有导出在动 —— 参考渲染异常，不要误判成导出回归 |
| `export-static` | 只在参考里动，导出不动 —— 动画在导出时丢失 |

统计口径见 `--source pagx` 分支的 `summarize()`：按 `Expect: supported|gap` 分组汇总，`gap` 组仅供参考、不计入通过率。

## 用例头部约定

每个文件的第一个 XML 注释固定为：

```
<!-- Tests: <一句话描述验证的语义>
     Target: <NodeType>  Channel: <通道名>[,<通道名>]  Type: <通道值类型>
     Expect: supported|gap
     Ref: <对应的 test/src 用例，没有则 -> -->
```

- `Expect: supported` —— 导出器应当支持，必须真的产出 `@keyframes`。
- `Expect: gap` —— PAGX 支持、导出器已知不支持，用来把缺口固化在用例集里，也用于观察缺口是否被补齐。
- `Target` / `Channel` 会被 `run.js` 的 `parsePagxCaseHeader()` 读进报告，**只取第一个 `Target:`/`Channel:`**，
  多目标的用例（`geometry/05`、`stroke/06`）需要在正文注释里补全。

## 统计口径（重要）

本目录只回答"**关键帧动画 + CSS 目标**"这一条链路的问题，因此：

- 范围内：`<Animation>` / `<Object>` / `<Channel>` / `<Key>` 以及时间线挂载（`<Timelines>`、`evaluationOffset`、`playing`）。
- 范围外：状态机（`<StateMachine>`）、ViewModel / `DataBind`、以及所有"PAGX 属性但不可动画"的属性（运行时通道注册表里 `NoFlags` 的那些）。
- 仅统计 PAGX 侧**可动画**的 `(节点类型, 通道)` 组合：`src/pagx/PAGXNodeChannel.cpp` 中标记为 `Anim` / `AnimLayout` 的条目，
  该表有 `PAGXTest.AnimatableChannelsHaveWriters` 兜底保证每个可动画通道都有渲染器写入方。
- 目标是 CSS，因此不要求导出结果包含任何 JS 运行时；CSS 表达不了的能力算 gap，不算错。

## 一、结论

| 指标 | 数值 |
|------|------|
| 用例数 | 122（`Expect: supported` 82 + `Expect: gap` 40） |
| PAGX 可动画 `(节点, 通道)` 组合 | 179，分布在 31 种节点类型 |
| 导出器已实现的通道 | 45，分布在 12 种节点类型（另有 3 条映射无效，见第七节） |
| 用例直接覆盖的通道 | 45 |
| 覆盖率（对导出器已实现集合） | 45/45 = **100%** |
| 覆盖率（对 PAGX 全部可动画通道） | 45/179 ≈ **25%** |
| 已实现但无对应用例 | 0（仅 `playing="false"` 这类时间线行为未覆盖，见第四节） |
| 已确认缺口（`Expect: gap`） | 40，逐族清单见第五节 |
| 仍无用例固化的缺口族 | 几何尺寸/位置、Polystar、NoiseStyle 等，见第五节 |

判断：**通道维度的覆盖已经补满**——导出器实现的 45 个通道都有用例直接驱动；**但能力面仍然偏窄**：
Repeater / TextModifier / RangeSelector / 渐变色源 / 图层样式这几族在导出器里完全没有实现，现在每族至少有一条
`Expect: gap` 用例把缺口固化下来，报告里的 `export-static` 就是这些缺口的可见信号。`Layer.rotation.*` 属于高频能力，
导出器目前不产出动画（映射表写错通道名），`layer/10`、`layer/11`、`layer/17` 已把三个轴都登记为 gap。

## 二、时间线语义覆盖

与通道无关、由 `HTMLAnimationSession` 统一处理的语义：

| 语义 | PAGX | 导出实现 | 用例 |
|------|------|----------|------|
| `Animation.duration` | ✔ | ✔ | 全部 |
| `Animation.frameRate` | ✔ | ✔ | `timing/04_frame_rate_30` |
| `loop="once"` | ✔ | ✔ | `layer/01`、`boundary/01` 等 |
| `loop="loop"` | ✔ | ✔ | `timing/01`、`boundary/02` |
| `loop="pingPong"` | ✔ | ✔ | `timing/02`、`timing/07` |
| `evaluationOffset` > 0（once 期间保持首帧值） | ✔ | ✔ | `timing/04`、`boundary/03` |
| `evaluationOffset` > 0（loop 每周期整体位移） | ✔ | ✔ | `timing/05` |
| `evaluationOffset` < 0 | ✔ | ✔ | `timing/06` |
| 同一 Animation 被两个 Composition 实例以不同 offset 挂载 | ✔ | ✔ | `timing/08` |
| 顶层 `<Animations>`（无需 `<Timelines>`） | ✔ | ✔ | 大多数 |
| `<Timelines><Animation ref/>` 挂载 | ✔ | ✔ | `timing/04`~`08`、`boundary/03` |
| `<Timelines playing="false">` | ✔ | ✔（导出实现已处理） | **无** |
| 同一节点上挂多条 Animation | ✔ | 未验证 | **无** |
| `interpolation="linear"` / 缺省 | ✔ | ✔ | `layer/01`（与通道基准共用） |
| `interpolation="hold"` | ✔ | ✔ | `interpolation/01` |
| `interpolation="bezier"` + 自定义 `bezierIn`/`bezierOut` | ✔ | ✔ | `interpolation/02` |
| `interpolation="bezier"` 省略控制柄（默认值） | ✔ | ✔ | `interpolation/03` |
| 同通道逐段混合插值 | ✔ | ✔ | `interpolation/04` |
| 首个 key 晚于 0（之前保持首值） | ✔ | ✔ | `boundary/01` |
| 末个 key 早于 duration（之后保持末值） | ✔ | ✔ | `boundary/02` |
| 重复时间点的 key（跳变） | ✔ | ✔ | `boundary/04` |
| key 超出 duration（once） | ✔ | ✔ | `boundary/05` |
| key 超出 duration（loop，周期仍按 duration 截断） | ✔ | ✔ | `boundary/06` |
| 同对象多通道共享一套 stop / easing | ✔ | ✔ | `combo/03` |
| 通道值类型 `float` | ✔ | ✔ | 大多数 |
| 通道值类型 `bool` | ✔ | ✔ | `layer/06` |
| 通道值类型 `color` | ✔ | ✔ | `fill/01` 等 |
| 通道值类型 `matrix` | ✔ | ✔ | `layer/04`、`layer/04b` |
| 通道值类型 `int` | ✔ | ✔ | `layer/07`（gap） |
| 通道值类型 `string` / `image` | ✔（PAGX 允许） | ✘（CSS 无法动画文本内容/图片） | `text/03`（string）；`image` 无用例 |
| 通道值类型 `matrix3D` | ✔ | ✘ | `layer/13` |

## 三、通道覆盖矩阵

图例：`PAGX` = 运行时是否可动画；`导出` = CSS 导出是否实现；`用例` = 本目录中直接驱动该通道的文件。

### Layer

| 通道 | PAGX | 导出 | 用例 |
|------|------|------|------|
| `alpha` | ✔ | ✔ | `layer/01`、`combo/02`、`interpolation/*`、`boundary/*`、`timing/*` |
| `visible` | ✔ | ✔ | `layer/06` |
| `blendMode` | ✔ | ✘ | `layer/07`（Expect: gap） |
| `x` / `y` | ✔ | ✔ | `layer/02`、`layer/03`、`combo/01`、`layer/05`、`combo/02` |
| `matrix` | ✔ | ✔ | `layer/04`、`layer/04b`（最短弧分解） |
| `scale.x` / `scale.y` | ✔ | ✔ | `layer/08`、`layer/09` |
| `scale.z` | ✔ | ✘ | **无 gap 用例** |
| `z` | ✔ | ✘ | `layer/14`（Expect: gap） |
| `rotation.x` / `rotation.y` / `rotation.z` | ✔ | ✘（映射表写了不存在的 `rotation`，见第七节） | `layer/17`（x）、`layer/11`（y）、`layer/10`（z）（Expect: gap） |
| `skew.x` / `skew.y` | ✔ | ✘ | `layer/15`、`layer/16`（Expect: gap） |
| `perspective` | ✔ | ✘ | `layer/12`（Expect: gap） |
| `matrix3D` | ✔ | ✘ | `layer/13`（Expect: gap） |
| `anchor.x` / `anchor.y` / `anchor.z`（局部变换支点） | ✔ | ✘ | `anchor/01`~`anchor/08`（Expect: gap） |

### Group / TextBox（共享变换通道）

| 通道 | PAGX | 导出 | 用例 |
|------|------|------|------|
| `alpha` | ✔ | ✔ | Group：`group/01`；TextBox：`textbox/01` |
| `position.x` | ✔ | ✔ | Group：`group/02`；TextBox：`textbox/02` |
| `position.y` | ✔ | ✔ | Group：`group/03`；TextBox：`textbox/05` |
| `rotation` | ✔ | ✔ | Group：`group/04`、`group/07`、`combo/04`；TextBox：`textbox/03` |
| `scale.x` | ✔ | ✔ | Group：`group/05`、`combo/04`；TextBox：`textbox/04` |
| `scale.y` | ✔ | ✔ | Group：`group/06`、`combo/04`；TextBox：`textbox/06` |
| `anchor.x` | ✔ | ✔ | Group：`group/08`；TextBox：`textbox/07` |
| `anchor.y` | ✔ | ✔ | Group：`group/11`；TextBox：`textbox/09` |
| `skew` | ✔ | ✔ | Group：`group/09`；TextBox：`textbox/08` |
| `skewAxis` | ✔ | ✔ | Group：`group/10`；TextBox：`textbox/10` |

### 几何元素

| 节点 / 通道 | PAGX | 导出 | 用例 |
|------|------|------|------|
| `Rectangle.size.width` | ✔ | ✔ | `geometry/01`、`geometry/05`（同时动画 Fill color） |
| `Rectangle.roundness` | ✔ | ✔ | `geometry/02` |
| `Rectangle.size.height` | ✔ | ✘ | **无 gap 用例** |
| `Rectangle.position.x` / `position.y` | ✔ | ✘ | **无 gap 用例** |
| `Ellipse.size.width` | ✔ | ✔ | `geometry/03` |
| `Ellipse.size.height` / `position.x` / `position.y` | ✔ | ✘ | **无 gap 用例** |
| `Polystar.*`（`pointCount`、`outerRadius`、`innerRadius`、`rotation`、`outerRoundness`、`innerRoundness`、`position.x/y`） | ✔ | ✘ | **无 gap 用例** |
| `Path.point{N}.x` / `.y`（遮罩路径逐点变形，规范未记载，仅代码注释有约定） | ✔（渲染器支持） | ✔ | `mask/01`、`mask/02`、`mask/03`（`.x`）、`mask/04`（`.y`） |
| `Path.position.x` / `position.y` | ✘（仅编辑） | — | — |

### 绘制器与颜色源

| 通道 | PAGX | 导出 | 用例 |
|------|------|------|------|
| `Fill.color` | ✔ | ✔ | `fill/01`、`fill/03`、`geometry/05`、`sharing/02`、`sharing/03` |
| `Fill.alpha` | ✔ | ✔ | `fill/02`、`fill/03`、`fill/04`（Text 上）、`fill/05`/`fill/06`（渐变，SVG / div 两种承载） |
| `Stroke.color` | ✔ | ✔ | `stroke/01` |
| `Stroke.alpha` | ✔ | ✔ | `stroke/02` |
| `Stroke.width` | ✔ | ✔ | `stroke/03`、`stroke/06` |
| `Stroke.dashOffset` | ✔ | ✔ | `stroke/04` |
| `Stroke.miterLimit` | ✔ | ✘ | `stroke/05`（Expect: gap） |
| `SolidColor.color`（含被多个 Fill 共享） | ✔ | ✔ | `solid_color/01`、`sharing/01` |
| `LinearGradient.startPoint.*` / `endPoint.*` | ✔ | ✘ | `gradient/01`（endPoint.x）（Expect: gap） |
| `RadialGradient.center.*` / `radius` | ✔ | ✘ | `gradient/02`（radius）（Expect: gap） |
| `ConicGradient.center.*` / `startAngle` / `endAngle` | ✔ | ✘ | `gradient/03`（startAngle）（Expect: gap） |
| `DiamondGradient.center.*` / `radius` | ✔ | ✘ | `gradient/04`（radius）（Expect: gap） |
| `ColorStop.offset` / `color` | ✔ | ✘ | `gradient/05`（offset）、`gradient/06`（color）（Expect: gap） |

### 修改器 / 文本

| 通道 | PAGX | 导出 | 用例 |
|------|------|------|------|
| `TrimPath.end`（仅 `type="separate"` 且 `start=0`、`offset=0`） | ✔ | ✔ | `geometry/04`、`stroke/06` |
| `TrimPath.start` / `offset`、continuous 类型 | ✔ | ✘ | `modifier/02`（start）；`offset` 与 continuous 类型仍无用例 |
| `RoundCorner.radius` | ✔ | ✘ | `modifier/01`（Expect: gap） |
| `Repeater.*`（`copies`、`offset`、`anchor.*`、`position.*`、`rotation`、`scale.*`、`startAlpha`、`endAlpha`） | ✔ | ✘ | `modifier/03`（copies）、`modifier/04`（rotation）；其余通道仍无用例 |
| `RangeSelector.*`（`start`、`end`、`offset`、`easeIn`、`easeOut`、`weight`） | ✔ | ✘ | `modifier/05`（start）；缓动通道仍无用例 |
| `TextModifier.*`（13 个通道，含 `fillColor`/`strokeColor`/`strokeWidth`） | ✔ | ✘ | `text/01`（fillColor）、`text/02`（alpha）（Expect: gap）；其余通道仍无用例 |
| `Text.text` / `fontFamily` / `fontStyle` / `fontSize` / `letterSpacing` / `fauxBold` / `fauxItalic` / `x` / `y` | ✔（见第七节：规范与实现不一致） | ✘ | `text/03`（text）；其余通道无用例 |

### 图层样式与滤镜

| 通道 | PAGX | 导出 | 用例 |
|------|------|------|------|
| `BlurFilter.blurX` / `blurY` | ✔ | ✔ | `filter/01`、`filter/02` |
| `DropShadowFilter.offsetX` / `offsetY` | ✔ | ✔ | `filter/03`、`filter/04` |
| `DropShadowFilter.blurX` / `blurY` | ✔ | ✔ | `filter/05` |
| `DropShadowFilter.color` | ✔ | ✔ | `filter/06` |
| `DropShadowFilter.shadowOnly` | ✔ | ✘ | `filter/07`（Expect: gap） |
| `InnerShadowFilter.*` | ✔ | ✘ | `filter/08`（offsetX）（Expect: gap） |
| `BlendFilter.color` | ✔ | ✘ | `filter/09`（Expect: gap） |
| `NoiseFilter.*` | ✔ | ✘ | `filter/10`（density）（Expect: gap） |
| `NoiseStyle.*` | ✔ | ✘ | **无 gap 用例** |
| `DropShadowStyle.*` | ✔ | ✘ | `style/01`（offsetX）（Expect: gap） |
| `InnerShadowStyle.*` | ✔ | ✘ | `style/02`（blurX/blurY）（Expect: gap） |
| `BackgroundBlurStyle.*` | ✔ | ✘ | `style/03`（blurX）（Expect: gap） |
| `GlassStyle.*`（7 个通道） | ✔ | ✘ | `style/04`（refraction）（Expect: gap） |

## 四、已实现但没有用例直接覆盖

通道维度已经清零：导出器实现的 45 个通道都有用例直接驱动。剩下的都是**非通道**的时间线行为：

| 项 | 说明 |
|----|------|
| `<Timelines playing="false">` | 导出实现里有暂停态处理（`elementPlayStates`），但没有用例；需要一条"参考与导出都静止"的用例，注意它在报告里会带 `both-static` 标记 |
| 同一节点挂多条 `<Animation>` | 未验证两条时间线写同一通道时的合成顺序 |

## 五、PAGX 可动画但 CSS 导出未实现（缺口族）

本轮为每个缺口族至少补了一条 `Expect: gap` 用例：它们在报告里表现为 `export-static`（参考在动、导出不动），
把"未支持"变成可见事实。仍标 **无** 的族建议后续补。

| 缺口族 | `Expect: gap` 用例 | 尚未覆盖 |
|--------|--------------------|----------|
| Layer 变换分量 / 3D | `layer/11`（rotation.y）、`layer/17`（rotation.x）、`layer/12`（perspective）、`layer/13`（matrix3D）、`layer/14`（z） | `scale.z` |
| Layer 支点（`anchor.*`） | `anchor/01`~`anchor/08` | — |
| Layer 混合模式 | `layer/07` | — |
| Stroke 尖角限制 | `stroke/05` | — |
| 几何尺寸 / 位置 | **无** | `size.height`、Rectangle/Ellipse 的 `position.x/y` |
| Polystar | **无** | `pointCount`、`outerRadius`、`innerRadius`、`rotation`、内外圆角 |
| 渐变色源 | `gradient/01`~`gradient/06` | 中心点通道 |
| 修改器 | `modifier/01`（RoundCorner.radius）、`modifier/03`（Repeater.copies）、`modifier/04`（Repeater.rotation）、`modifier/05`（RangeSelector.start） | `TrimPath.offset`、continuous、Repeater 其余通道、RangeSelector 缓动 |
| 文本 | `text/01`（TextModifier.fillColor）、`text/02`（TextModifier.alpha）、`text/03`（Text.text） | 其余 TextModifier / Text 通道 |
| 图层样式 | `style/01`（DropShadowStyle.offsetX）、`style/02`（InnerShadowStyle.blur）、`style/03`（BackgroundBlurStyle.blurX）、`style/04`（GlassStyle.refraction） | `NoiseStyle`、`GlassStyle` 其余通道 |
| 滤镜 | `filter/07`（shadowOnly）、`filter/08`（InnerShadowFilter.offsetX）、`filter/09`（BlendFilter.color）、`filter/10`（NoiseFilter.density） | 其余滤镜通道 |

`Layer.rotation.z`、`skew.x/y` 与 `TrimPath.start` 已补齐：前两者原先因绑定表只登记了 `rotation`、缺少 skew 条目而
静默丢弃，`start` 只有 `end` 有实现。补齐后 `layer/10`、`layer/15`、`layer/16`、`modifier/02` 的 SSIM 从
0.49 / 0.79 / 0.86 / 0.85 提升到 0.999 / 0.999 / 0.999 / 0.996，语料的 `Expect` 已改为 `supported`。

## 六、冗余与重叠分析

### 已合并的完全重复（本轮处理）

`interpolation/01_linear`、`layer/01_alpha`、`timing/01_loop_once` 曾经逐字节一致（仅 id 不同）：Layer `alpha`
0.2→1、`duration=60`、`frameRate=60`、`loop="once"`、两个线性关键帧。现整合为 `layer/01_alpha` 一个用例，
header 里注明它同时是线性插值与 `loop="once"` 的基准；`interpolation` 与 `timing` 两族不再重复这条语义，
两族编号已顺移、没有空位。原来散在目录根部的 `keyframe_boundary_hold.pagx` 也移入
`boundary/03_offset_once_hold.pagx`，顺带补齐了 `boundary` 缺失的 03。

### 动画签名相同、载体不同（保留）

| 组 | 用例 | 差异点 |
|----|------|--------|
| `Fill.color` | `fill/01`、`sharing/02`、`sharing/03` | 普通矩形 / Repeater 内 / 矩形与 Text 共享同一个 Fill |
| `SolidColor.color` | `solid_color/01`、`sharing/01` | 单个 Fill / 两个 Fill 共享同一颜色资源 |
| `Fill.alpha` | `fill/02`、`fill/04` | 普通图层 / Text 上的 Fill（`fill-opacity` vs `opacity` 分支不同） |
| `Fill.alpha`（渐变） | `fill/05`、`fill/06` | 渐变由内联 SVG 承载 / 由 div 承载，输出结构不同 |
| `Group.rotation` | `group/04`、`group/07` | 零锚点 / 非零静态锚点（`transform-origin` 组合） |
| `Layer.x` | `layer/02`、`layer/05` | 纯平移 / 平移叠静态 `rotationZ`（走矩阵分支） |
| `Layer.matrix` | `layer/04`、`layer/04b` | 常规矩阵 / 含反射需最短弧分解 |
| `Path.point*.x` | `mask/01`、`mask/02`、`mask/03` | contour 收缩 / alpha 软边 / 遮罩图层带位移 |

这些不是重复：断言比较的是整帧画面，上述差异会真实改变输出结构或像素，属于必要的"同一通道 × 不同承载"覆盖。

### 同一通道跨族复用（保留）

`Layer.alpha` 仍被 5 个族复用（`layer`、`interpolation`、`boundary`、`timing`、`combo`），现为 13 个文件。
每个族只改一个语义维度（关键帧边界 / 插值 / 循环 / `evaluationOffset` / 多通道共享 bundle），对应
`HTMLAnimationWriter` 里不同分支，因此不算冗余；视觉上靠下一节的配色与主体形状区分，不再"一张图反复测"。

## 七、其它风险点

1. **映射表有 3 条无效条目**（`HTMLAnimationWriter.cpp` 的 `kChannelBindingTable`）：
   - `{Layer, "rotation"}`：PAGX 的 Layer 通道是 `rotation.x/y/z`，该条目永不命中（见第五节）。
   - `{SolidColor, "alpha"}`：`SolidColor` 只有 `color`，没有 `alpha` 通道。
   - `{DropShadowFilter, "alpha"}`：`DropShadowFilter` 只有 `offsetX/offsetY/blurX/blurY/color/shadowOnly`，没有 `alpha`。
   后两条属于无效代码，`Layer.rotation` 属于功能缺陷，建议一并清理/修正。
2. **未实现通道一律静默丢弃**：导出不产生任何告警，只能靠 `Expect` 注释表达。建议导出时对"目标节点上存在但无法映射的通道"
   输出一次诊断日志，否则使用者无法察觉动画消失（`Layer.rotation.z` 就是活例）。
3. **规范与实现不一致**：`spec/pagx_spec.zh_CN.md` §D.3 声明 `fontSize`、`letterSpacing` 等文本属性"不支持动画驱动"，
   但 `PAGXNodeChannel.cpp` 把 `text`/`fontFamily`/`fontStyle`/`fontSize`/`letterSpacing`/`fauxBold`/`fauxItalic`
   标为 `AnimLayout`，且 `LayerBuilder` 有对应写入方；另外规范完全没有记录遮罩路径的 `point{N}.x/y` 通道
   （只在 `LayerBuilder.cpp` 的注释里有约定）。本文档的"PAGX 可动画"列以运行时注册表为准。
4. **`sharing/01` 自述"passes for the wrong reason"**：运行时把共享颜色源只绑到最后一个 Fill，导出也丢同一处，
   因此该用例当前不构成有效回归保护 —— 要么修实现，要么把断言收紧到"两个形状都变色"。
5. **header 元数据只记第一个 `Target`/`Channel`**：`geometry/05`、`stroke/06` 在报告里只体现第一个通道。
6. **采样只覆盖时间轴中段**：默认 5 帧取 `(i+0.5)/5` 的等分点，落在 0 与末端之外。边界语义靠"恰好采到"，
   建议对 `boundary/*` 这类用例显式提高 `--samples`（如 9），否则语义变化可能落在两个采样点之间。
7. **两处 fixture 缺陷（本轮已修）**：`layer/05_x_with_static_rotation` 原来把静态变换写成非单位 `matrix`，
   按规范 §5.2 `matrix` 会覆盖 `x`/`y`，参考画面整段静止、用例空转，已改成静态 `rotationZ="30"`；
   `layer/04_matrix`、`layer/04b_matrix_shortest_arc` 的 `matrix` 关键帧原本不带平移，`matrix` 通道替换掉
   `x`/`y` 后内容落到画布左上角甚至画布外（`04b` 的参考帧只剩背景），已给关键帧补上 translate。

## 八、fixture 视觉规范

所有用例统一 320×200（`boundary/03` 为容纳两个主体用 400×160）、`#F8FAFC` 浅灰白背景，只用**强调色 + 主体形状**
区分族，节省读者在报告里辨认"这是不是同一个用例"的成本：

| 族 | 强调色 | 主体形状 |
|----|--------|----------|
| `layer` | `#4F46E5` 靛蓝 | 菱形（`Polystar` 4 边）；旋转/矩阵类改用不等边三角形，避免 90° 旋转对称把错路径样本掩盖 |
| `group` | `#EA580C` 橙 | 三角形（`Polystar` 3 边，旋转/斜切可见） |
| `textbox` | `#7C3AED` 紫 | 文本 "AB" |
| `fill` | `#DB2777` 玫红 | 胶囊（`roundness` 取高度一半）/ 文本 / 渐变三角 |
| `stroke` | `#0E7490` 青 | 椭圆描边环 / 五角星 / 折线（描边相关通道） |
| `filter` | `#15803D` 绿 | 六边形（`Polystar` 6 边）+ 深色阴影 |
| `geometry` | `#B45309` 琥珀 | 矩形 / 椭圆 / 描边轮廓（按用例而定） |
| `mask` | `#B91C1C` 红 | 被遮罩的主体内容 |
| `timing` | `#0284C7` 天蓝 | 圆形（`Ellipse`） |
| `boundary` | `#0F766E` 青绿 | 五角星（`Polystar` star） |
| `interpolation` | `#C026D3` 品红 | 弧形（`Path` 曲线闭合） |
| `combo` | `#475569` 石板灰 | 八边形（`Polystar` 8 边）+ 双通道 |
| `sharing` | `#9A3412` 棕 | 圆形（共享颜色的两个点）/ 圆形与文本 |
| `solid_color` | `#65A30D` 黄绿 | 圆形 |
| `anchor` | `#C2410C` 砖橙 | 长条 + 深色支点圆点；3D 用例用带内层小卡的卡片 |

每族一个剪影：同一族内形状固定，跨族不重复，报告里一眼能认出"这是另一类用例"，而不是同一张图换个通道名。

约定：

- 关键帧里出现的 `value="#..."` 属于动画起止色，同一族的起始色与该族强调色保持一致。
- 形状必须让被测语义可见：旋转/斜切类不能用圆（旋转前后像素完全一致），因此 `group` 用三角形、
  `boundary` 用五角星；`matrix` 通道会覆盖 `x`/`y` 平移，其关键帧必须自带 translate，否则内容会落到画布外。
- 新增用例沿用上表；如果需要新族，在同一张表里登记，不要临时起色。

## 九、新增族的 fixture

本轮新增的四个族沿用第八节约定：320×200、`#F8FAFC` 背景、每族一个剪影，颜色只在族内保持一致。

| 族 | 强调色 | 主体 |
|----|--------|------|
| `gradient` | `#0891B2` 青 | 椭圆 + 渐变填充（线性/径向/锥形/菱形），方向变化一眼可辨 |
| `modifier` | `#9333EA` 紫 | 五边形（RoundCorner）/ 五角星描边（TrimPath）/ Repeater 方块阵列 / 文本 |
| `text` | `#BE123C` 玫红 | 72px 粗体 "AB" |
| `style` | `#4338CA` 靛紫 | 白色卡片；模糊类用例自带星形+圆形背板，否则看不出模糊效果 |

与 `layer` 族同色系的分支：新增的 Layer 通道用例（`layer/08`~`layer/17`）继续用 `#4F46E5` 与菱形主体；
`matrix` / `matrix3D` 通道会替换 `x`/`y` 平移，关键帧必须自带 translate（见 `layer/13`）。

## 十、anchor 族

`anchor/` 描述 `Layer.anchor`（`anchor.x` / `anchor.y` / `anchor.z`）作为局部变换支点的行为，与 `layer/` 的 3D 用例互补：

| 用例 | 覆盖点 |
|------|--------|
| `anchor/01_rotation_z_offcenter` | 支点落在长条左缘，`rotation.z` 让长条摆动而不是绕原点公转 |
| `anchor/02_anchor_x_animated` | `anchor.x` 逐帧滑动，同一姿态在每一帧被重新支点化 |
| `anchor/03_rotation_y_anchor_x` | 3D × 支点耦合：`rotation.y` 绕左缘竖轴翻转，投影必须钉住该边 |
| `anchor/04_rotation_x_anchor_depth` | `anchor` 第三个值给深度，`rotation.x` 让平面整体前后移动、投影尺寸随之变化 |
| `anchor/05_anchor_z_animated` | 静态 `rotationX` 下动画 `anchor.z`，支点沿深度移动 |
| `anchor/06_rotation_y_with_anchor_x` | 3D 旋转与 `anchor.x` 在同一 Object 内同时动画，逐帧合成 |
| `anchor/07_rotation_x_anchor_y` | 支点贴在卡片下缘，`rotation.x` 像翻盖一样打开 |
| `anchor/08_anchor_center_vs_default` | 同一角度下"绕图层原点"与"绕中心（`anchor`）"两根长条并排对照 |

`anchor` 把支点做成矩阵的不动点、投影留在支点之外，因此线性部分为单位阵时 `anchor` 不改变姿态（`anchor/08` 的对照就是这条）；
`anchor` 写三个值时第三个是支点深度，只有存在 Z 向分量（`rotationX` / `rotationY` / `matrix3D` 等）才生效（`anchor/04`、`anchor/05`）。
把支点烘焙进每个 `matrix3D` 关键帧会被逐帧独立插值，卡片翻转时会整体漂移，这正是 `anchor` 要消除的写法。
