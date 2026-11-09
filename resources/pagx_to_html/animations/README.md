# PAGX → HTML/CSS 动效用例集

本目录是**动画导出**的回归语料：一个 `.pagx` 只验证一个语义点，参考实现是 `pagx render --time`
（PAGX 运行时求值后渲染），被测实现是 `pagx export --format html` 产出的 CSS `@keyframes`。

**194 个可动画通道 / 32 种节点类型 / 216 个用例。** 下面两张表分别回答"是否全面"和"是否精简"，
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

它会同时校验：① 导出映射表中的每个通道都有用例；② 注册表中的**每个可动画通道**都有直接驱动它的用例
（例外见脚本中的 `CHANNEL_EXEMPTIONS`：数学上不可见的 `Layer.scale.z`，以及渲染器尚未写入的
`TextPath` 全部通道）；
③ 每个用例头部 `Expect` 与实际导出结果一致；④ 语料中不存在"仅 id 不同"的重复文档。

## 1. 主表：可动画通道 → 导出 → 用例

`导出` 以 `src/pagx/html/HTMLAnimationWriter.cpp` 的通道映射表为准；`用例` 只在这里没有单独用例时留空。
用例列里 `family/*（n）` 表示该族贡献了 n 条。

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
| Layer | `visible` | ✔ | `layer/06` |
| Layer | `x` | ✔ | `boundary/03`、`combo/*`（3）、`layer/02·05` |
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
| Text | `x` | ✘ | `text/10` |
| Text | `y` | ✘ | `text/11` |
| TextPath | `alpha` `anchor.x` `anchor.y` `position.x` `position.y` `rotation` `scale.x` `scale.y` `skew` `skewAxis` | ✘ | — |
| Rectangle | `position.x` | ✘ | `geometry/07` |
| Rectangle | `position.y` | ✘ | `geometry/08` |
| Rectangle | `roundness` | ✔ | `geometry/02` |
| Rectangle | `size.height` | ✘ | `geometry/09` |
| Rectangle | `size.width` | ✔ | `geometry/01·05` |
| Ellipse | `position.x` | ✘ | `geometry/10` |
| Ellipse | `position.y` | ✘ | `geometry/11` |
| Ellipse | `size.height` | ✘ | `geometry/12` |
| Ellipse | `size.width` | ✔ | `geometry/03` |
| Polystar | `innerRadius` | ✘ | `geometry/13` |
| Polystar | `innerRoundness` | ✘ | `geometry/15` |
| Polystar | `outerRadius` | ✘ | `geometry/14` |
| Polystar | `outerRoundness` | ✘ | `geometry/16` |
| Polystar | `pointCount` | ✘ | `geometry/06` |
| Polystar | `position.x` | ✘ | `geometry/17` |
| Polystar | `position.y` | ✘ | `geometry/18` |
| Polystar | `rotation` | ✘ | `geometry/19` |
| Fill | `alpha` | ✔ | `fill/*`（4） |
| Fill | `color` | ✔ | `fill/01·03`、`geometry/05`、`sharing/02·03` |
| Stroke | `alpha` | ✔ | `stroke/02` |
| Stroke | `color` | ✔ | `stroke/01` |
| Stroke | `dashOffset` | ✔ | `stroke/04` |
| Stroke | `miterLimit` | ✔ | `stroke/05` |
| Stroke | `width` | ✔ | `stroke/03·06` |
| TrimPath | `end` | ✔ | `geometry/04`、`stroke/06` |
| TrimPath | `offset` | ✘ | `geometry/20` |
| TrimPath | `start` | ✔ | `modifier/02` |
| RoundCorner | `radius` | ✔ | `modifier/01` |
| Repeater | `anchor.x` | ✘ | `modifier/08` |
| Repeater | `anchor.y` | ✘ | `modifier/09` |
| Repeater | `copies` | ✔ | `modifier/03` |
| Repeater | `endAlpha` | ✘ | `modifier/11` |
| Repeater | `offset` | ✘ | `modifier/12` |
| Repeater | `position.x` | ✘ | `modifier/13` |
| Repeater | `position.y` | ✘ | `modifier/14` |
| Repeater | `rotation` | ✔ | `modifier/04` |
| Repeater | `scale.x` | ✘ | `modifier/15` |
| Repeater | `scale.y` | ✘ | `modifier/16` |
| Repeater | `startAlpha` | ✘ | `modifier/10` |
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
| RangeSelector | `easeIn` | ✘ | `modifier/17` |
| RangeSelector | `easeOut` | ✘ | `modifier/18` |
| RangeSelector | `end` | ✔ | `modifier/06` |
| RangeSelector | `offset` | ✔ | `modifier/07` |
| RangeSelector | `start` | ✔ | `modifier/05` |
| RangeSelector | `weight` | ✘ | `modifier/19` |
| SolidColor | `color` | ✔ | `sharing/01`、`solid_color/01`、`timing/04·06` |
| LinearGradient | `endPoint.x` | ✘ | `gradient/01` |
| LinearGradient | `endPoint.y` | ✘ | `gradient/09` |
| LinearGradient | `startPoint.x` | ✘ | `gradient/07` |
| LinearGradient | `startPoint.y` | ✘ | `gradient/08` |
| RadialGradient | `center.x` | ✘ | `gradient/10` |
| RadialGradient | `center.y` | ✘ | `gradient/11` |
| RadialGradient | `radius` | ✘ | `gradient/02` |
| ConicGradient | `center.x` | ✘ | `gradient/12` |
| ConicGradient | `center.y` | ✘ | `gradient/13` |
| ConicGradient | `endAngle` | ✘ | `gradient/14` |
| ConicGradient | `startAngle` | ✘ | `gradient/03` |
| DiamondGradient | `center.x` | ✘ | `gradient/15` |
| DiamondGradient | `center.y` | ✘ | `gradient/16` |
| DiamondGradient | `radius` | ✘ | `gradient/04` |
| ColorStop | `color` | ✘ | `gradient/06` |
| ColorStop | `offset` | ✘ | `gradient/05` |
| DropShadowStyle | `blurX` | ✘ | `style/06` |
| DropShadowStyle | `blurY` | ✘ | `style/07` |
| DropShadowStyle | `color` | ✘ | `style/08` |
| DropShadowStyle | `offsetX` | ✘ | `style/01` |
| DropShadowStyle | `offsetY` | ✘ | `style/09` |
| DropShadowStyle | `showBehindLayer` | ✘ | `style/10` |
| DropShadowStyle | `spread` | ✘ | `style/11` |
| InnerShadowStyle | `blurX` `blurY` | ✘ | `style/02` |
| InnerShadowStyle | `color` | ✘ | `style/12` |
| InnerShadowStyle | `offsetX` | ✘ | `style/13` |
| InnerShadowStyle | `offsetY` | ✘ | `style/14` |
| InnerShadowStyle | `spread` | ✘ | `style/15` |
| BackgroundBlurStyle | `blurX` | ✘ | `style/03` |
| BackgroundBlurStyle | `blurY` | ✘ | `style/16` |
| GlassStyle | `depth` | ✘ | `style/17` |
| GlassStyle | `dispersion` | ✘ | `style/18` |
| GlassStyle | `frost` | ✘ | `style/19` |
| GlassStyle | `lightAngle` | ✘ | `style/20` |
| GlassStyle | `lightIntensity` | ✘ | `style/21` |
| GlassStyle | `refraction` | ✘ | `style/04` |
| GlassStyle | `splay` | ✘ | `style/22` |
| NoiseStyle | `color` | ✘ | `style/23` |
| NoiseStyle | `density` | ✘ | `style/05` |
| NoiseStyle | `firstColor` | ✘ | `style/24` |
| NoiseStyle | `opacity` | ✘ | `style/26` |
| NoiseStyle | `secondColor` | ✘ | `style/25` |
| NoiseStyle | `seed` | ✘ | `style/27` |
| NoiseStyle | `size` | ✘ | `style/28` |
| BlurFilter | `blurX` | ✔ | `filter/01` |
| BlurFilter | `blurY` | ✔ | `filter/02` |
| DropShadowFilter | `blurX` `blurY` | ✔ | `filter/05` |
| DropShadowFilter | `color` | ✔ | `filter/06` |
| DropShadowFilter | `offsetX` | ✔ | `filter/03` |
| DropShadowFilter | `offsetY` | ✔ | `filter/04` |
| DropShadowFilter | `shadowOnly` | ✘ | `filter/07` |
| InnerShadowFilter | `blurX` | ✘ | `filter/11` |
| InnerShadowFilter | `blurY` | ✘ | `filter/12` |
| InnerShadowFilter | `color` | ✘ | `filter/13` |
| InnerShadowFilter | `offsetX` | ✘ | `filter/08` |
| InnerShadowFilter | `offsetY` | ✘ | `filter/14` |
| InnerShadowFilter | `shadowOnly` | ✘ | `filter/15` |
| BlendFilter | `color` | ✘ | `filter/09` |
| NoiseFilter | `color` | ✘ | `filter/16` |
| NoiseFilter | `density` | ✘ | `filter/10` |
| NoiseFilter | `firstColor` | ✘ | `filter/17` |
| NoiseFilter | `opacity` | ✘ | `filter/19` |
| NoiseFilter | `secondColor` | ✘ | `filter/18` |
| NoiseFilter | `seed` | ✘ | `filter/20` |
| NoiseFilter | `size` | ✘ | `filter/21` |

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
| `Layer.x` | 6 | `boundary/03`、`combo/*`（3）、`layer/02·05` |
| `Layer.y` | 2 | `combo/01`、`layer/03` |
| `Group.rotation` | 3 | `combo/04`、`group/04·07` |
| `Group.scale.x` | 2 | `combo/04`、`group/05` |
| `Group.scale.y` | 2 | `combo/04`、`group/06` |
| `Rectangle.size.width` | 2 | `geometry/01·05` |
| `Fill.alpha` | 4 | `fill/*`（4） |
| `Fill.color` | 5 | `fill/01·03`、`geometry/05`、`sharing/02·03` |
| `Stroke.width` | 2 | `stroke/03·06` |
| `TrimPath.end` | 2 | `geometry/04`、`stroke/06` |
| `SolidColor.color` | 4 | `sharing/01`、`solid_color/01`、`timing/04·06` |

`Layer.alpha` 是最典型的例子：`layer/01` 是通道基准，`interpolation/*` 变插值方式，`boundary/*` 变关键帧
边界，`timing/*` 变循环与 `evaluationOffset`，`combo/02·03` 变"与其他通道共享同一组 keyframe stop"。

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

**未实现但已固化**：表 1 中"导出 ✘ 且有用例"的行，共 110 条 `Expect: gap` 用例，逐通道覆盖 Layer
3D/支点、几何位置与尺寸、渐变色源、修改器、文本与文本修改器、图层样式、滤镜参数。

**豁免的通道族：`TextPath`（10 个通道）**。这些通道在 `PAGXNodeChannel.cpp` 里标为可动画，但
`src/renderer/LayerBuilder.cpp` 的 `convertTextPath()` 没有注册任何通道 writer（对比
`convertTextModifier()` 有 `setAccessor`），实测 `rotation` / `position.x` / `scale.x` / `alpha` 动画在
`pagx render --time` 下的首尾帧差值都是 0，写不出"参考确实在动"的用例。它们已列入
`tools/anim_coverage.py` 的 `CHANNEL_EXEMPTIONS`；渲染器补上 writer 后，删除豁免并补用例。

**已知映射问题**：`{SolidColor, "alpha"}` 与 `{DropShadowFilter, "alpha"}` 指向不存在的通道，`--check`
报 WARN。

## 5. 用例族与 fixture

| 族 | 用例数 | 强调色 | 主体 | 独有维度 |
|----|--------|--------|------|----------|
| `layer` | 18 | `#4F46E5` | 菱形；旋转/矩阵类用不等边三角形 | 单个 Layer 通道的映射基准 |
| `group` | 11 | `#EA580C` | 三角形 | Group 变换链与锚点 |
| `textbox` | 10 | `#7C3AED` | 文本 "AB" | TextBox 布局与变换通道 |
| `fill` | 6 | `#DB2777` | 胶囊 / 文本 / 渐变三角 | 绘制器颜色与透明度承载 |
| `stroke` | 6 | `#0E7490` | 椭圆描边环 / 五角星 | 描边参数 |
| `filter` | 21 | `#15803D` | 六边形 + 深色阴影 | 滤镜参数 |
| `geometry` | 20 | `#B45309` | 矩形 / 椭圆 / 五角星 | 几何尺寸与圆角 |
| `mask` | 4 | `#B91C1C` | 被遮罩内容 | 遮罩路径逐点变形 |
| `timing` | 8 | `#0284C7` | 圆形 | 循环、帧率、`evaluationOffset`、挂载 |
| `boundary` | 6 | `#0F766E` | 五角星 | 关键帧边界 |
| `interpolation` | 4 | `#C026D3` | 弧形 | 插值方式 |
| `combo` | 4 | `#475569` | 八边形 | 多通道共享 keyframe |
| `sharing` | 3 | `#9A3412` | 圆形 / 圆形与文本 | 资源与绘制器共享 |
| `solid_color` | 1 | `#65A30D` | 圆形 | 颜色源 |
| `anchor` | 9 | `#C2410C` | 长条 + 支点圆点 / 卡片 | 局部变换支点（含 `anchor.z` 深度）与 3D 耦合 |
| `gradient` | 16 | `#0891B2` | 椭圆 | 渐变色源参数 |
| `modifier` | 19 | `#9333EA` | 五边形 / 五角星描边 / 方块阵列 | 修改器参数 |
| `text` | 22 | `#BE123C` | 72px 粗体文本 | 文本与文本修改器参数 |
| `style` | 28 | `#4338CA` | 白色卡片（模糊类带背板） | 图层样式参数 |

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

`stroke/05_miter_limit`（`static-miter-join-rendering`）：通道本身已导出，但 `miterLimit > 1` 时**静态**导出的
尖角几何就与参考差约 1.4%（1/4/10/20 里只有 1 完全一致，其余结果相同），属于既有静态差异，**计划后续修**。

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
