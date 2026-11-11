# PAGX 动效 → HTML：通道覆盖与判定标准

本目录是导出器动画能力的**验收基线**：每条可动画通道都要有对应用例，用例通过逐帧比对 PAGX 参考渲染来判定。

当前状态：**187 条用例（supported 184 / gap 3），整体平均 SSIM 0.9788**。

## 1. 判定标准

- **一条可动画通道 ⇒ 至少一条用例**；用例不需要声明期望，**存在即代表导出器承诺实现它**。
- **单通道用例**必须 `min SSIM` 达标——非文本 **≥ 0.99**，文本 **≥ 0.95**（文本受字体度量基线影响，见下）。
- **`composite/` 用例**只验证多个通道能否共存（必须导出关键帧），分数仅记录、不设阈值。
- **每个通道至少出现在一条 composite 用例里**，否则视为覆盖缺口。
- **豁免**：CSS 这一期做不了的通道不进用例，改为登记在 `tools/anim_coverage.py`，并打四类标签之一——
  `js-pending`（CSS 无法表达，JS 逐帧可做）、`js-after-static`（静态还没画出该效果）、
  `renderer-gap`（渲染器不应用该通道，导出侧无解）、`no-op`（不改变画面）。
- 豁免通道**不需要用例**；但若导出器仍保留其绑定（两轴同值时依然准确），不视为违规。

文本类的阈值更低，是因为浏览器与运行时的字体度量、抗锯齿基线存在 1~2px 差异，与动画正确性无关；该差异
单独立项处理，不在本目录的用例里修。

## 2. 已支持的通道（128 个）

| 节点 | 通道 |
|------|------|
| `Layer` | `alpha`、`x`、`y`、`matrix`、`visible`、`blendMode`、`rotation.z`、`skew.x`、`skew.y`、`rotation.x`、`rotation.y`、`z`、`perspective`、`anchor.x`、`anchor.y`、`anchor.z`、`matrix3D`、`scale.x`、`scale.y` |
| `Group` | `alpha`、`position.x`、`position.y`、`rotation`、`scale.x`、`scale.y`、`anchor.x`、`anchor.y`、`skew`、`skewAxis` |
| `TextBox` | `alpha`、`position.x`、`position.y`、`rotation`、`scale.x`、`scale.y`、`anchor.x`、`anchor.y`、`skew`、`skewAxis` |
| `Rectangle` | `size.width`、`size.height`、`position.x`、`position.y`、`roundness` |
| `Ellipse` | `size.height`、`position.x`、`position.y`、`size.width` |
| `Polystar` | `pointCount`、`outerRadius`、`innerRadius`、`outerRoundness`、`innerRoundness`、`rotation`、`position.x`、`position.y` |
| `RoundCorner` | `radius` |
| `Fill` | `color`、`alpha` |
| `SolidColor` | `color`、`alpha` |
| `Stroke` | `color`、`alpha`、`width`、`dashOffset`、`miterLimit` |
| `Text` | `fontSize`、`letterSpacing`、`x`、`y`、`fauxBold`、`fauxItalic`、`fontFamily`、`fontStyle` |
| `TextModifier` | `position.x`、`position.y`、`rotation`、`scale.x`、`scale.y`、`skew`、`skewAxis`、`anchor.x`、`anchor.y`、`alpha`、`fillColor`、`strokeColor`、`strokeWidth` |
| `RangeSelector` | `start`、`end`、`offset`、`easeIn`、`easeOut`、`weight` |
| `Repeater` | `rotation`、`copies`、`startAlpha`、`position.x`、`position.y`、`scale.x`、`scale.y`、`anchor.x`、`anchor.y`、`endAlpha`、`offset` |
| `TrimPath` | `end`、`start`、`offset` |
| `LinearGradient` | `startPoint.x`、`startPoint.y`、`endPoint.x`、`endPoint.y` |
| `RadialGradient` | `center.x`、`center.y`、`radius` |
| `ColorStop` | `color` |
| `BlendFilter` | `color` |
| `DropShadowFilter` | `blurX`、`blurY`、`offsetX`、`offsetY`、`color`、`alpha` |
| `InnerShadowFilter` | `color` |
| `DropShadowStyle` | `offsetX`、`offsetY`、`color`、`spread` |
| `InnerShadowStyle` | `color` |

## 3. CSS 不支持、留待 JS

| 节点 | 通道 | 标签 | 原因 |
|------|------|------|------|
| `Layer` | `scale.z` | `no-op` | 合成顺序决定它不改变画面 |
| `BlurFilter` | `blurX`、`blurY` | `js-pending` | CSS 模糊各向同性，只能两轴同值 |
| `DropShadowFilter` | `shadowOnly` | `js-pending` | `dx`/`dy`/`stdDeviation` 是滤镜原语属性 |
| `InnerShadowFilter` | `blurX`、`blurY`、`offsetX`、`offsetY`、`shadowOnly` | `js-pending` | 同上 |
| `DropShadowStyle` | `blurX`、`blurY`、`showBehindLayer` | `js-pending` | 单轴模糊无 CSS 等价；`showBehindLayer` 需静态先输出阴影层 |
| `InnerShadowStyle` | `blurX`、`blurY`、`offsetX`、`offsetY`、`spread` | `js-pending` | 内阴影几何在滤镜原语上 |
| `BackgroundBlurStyle` | `blurX`、`blurY` | `js-pending` | backdrop 模糊无单轴写法 |
| `GlassStyle` | `depth`、`dispersion`、`frost`、`lightAngle`、`lightIntensity`、`refraction`、`splay` | `js-after-static` | 静态本身只是近似（t=0 ≈0.97） |
| `ColorStop` | `offset` | `js-pending` | `offset` 是 XML 属性 |
| `TextPath` | `alpha`、`anchor.x`、`anchor.y`、`position.x`、`position.y`、`rotation`、`scale.x`、`scale.y`、`skew`、`skewAxis` | `renderer-gap` | `convertTextPath()` 未注册通道写入 |
| `ConicGradient` | 整节点 | `js-pending` | 静态光栅化成 <image>，无 CSS 渐变可动 |
| `DiamondGradient` | 整节点 | `js-pending` | 同上 |
| `NoiseStyle` | 整节点 | `js-after-static` | 静态未实现，t=0 画面为空 |
| `NoiseFilter` | 整节点 | `js-after-static` | 静态只画底色，无噪点 |

### 组合限制（不只是通道，而是通道的组合）

- **`TrimPath` 作用于带填充的形状**：运行时会截断路径本身，填充区域随之收缩（实测某矩形填充宽度
  178px → 130px），而 CSS 的 `stroke-dasharray` 只作用于描边，填充始终完整。描边部分已经支持，
  填充部分留待 JS 逐帧重绘路径。

## 4. 运行

```bash
# 全量：逐帧比对 PAGX 参考，输出 report.csv / report.md / index.html
node tools/html-snapshot/eval-animation/run.js --source pagx -r \
     --corpus resources/pagx_to_html/animations -j 6 --out <输出目录>

# 单例
node tools/html-snapshot/eval-animation/run.js --source pagx -r \
     --corpus resources/pagx_to_html/animations --only text/07 --out <输出目录>

# 覆盖校验：通道是否都有用例、Expect 是否与导出一致、fixture 是否是合法 XML
python3 tools/anim_coverage.py --check
```

## 5. 用例头部约定

每条 `.pagx` 顶部的注释块是机器可读的元数据：

```xml
<!-- Tests: 这条用例验证什么（一句话）
     Target: Text  Channel: fauxBold  Type: bool
     Ref: - -->                   // 可选，关联的实现或测试
```

`Target` + `Channel` 决定它覆盖哪个通道；`Type` 是通道值类型（float/color/bool/int/string/matrix）。
写法细节（字距显式写 `letterSpacing="0"`、复合用例如何取景等）都已固化在用例本身，不在本文档重复。
