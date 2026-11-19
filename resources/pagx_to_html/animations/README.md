# PAGX 动效 → HTML：通道覆盖与判定标准

本目录是导出器动画能力的**验收基线**：每条可动画通道都要有对应用例，用例通过逐帧比对 PAGX 参考渲染来判定。

当前状态：**189 条用例（supported 126 / exempt 59，通道全集 185），整体平均 SSIM 0.9915**。

## 1. 判定标准

- **一条可动画通道 ⇒ 至少一条用例**；用例不需要声明期望，**存在即代表导出器承诺实现它**。
- **判定分档**：`≥ 0.999` 完全还原、`≥ 0.99` 略有差异、其余差异较大（以 `min` 与 `mean` 中更严的一档为准）。
- **含文本节点的用例按另一条曲线**：`≥ 0.99` 完全还原、`≥ 0.96` 略有差异、其余差异较大。原因是两套渲染器
  的字形轮廓本就不同——PAGX 填充带 hint 的轮廓、fauxBold 时再并上一个描边，浏览器用自己的字面栅格化、
  fauxBold 时用 `-webkit-text-stroke` 描边——文本用例的静态天花板通常落在 0.95–0.99，用非文本的 0.999
  曲线判定会把渲染策略差异记成导出缺陷。同理，含文本用例若"动画路线与静态路线在同一状态的得分差
- **`composite/` 用例**只验证多个通道能否共存（必须导出关键帧），分数仅记录、不设阈值。
- **每个通道至少出现在一条 composite 用例里**，否则视为覆盖缺口。
- **豁免**：CSS 这一期做不了的通道不进用例，改为登记在 `tools/anim_coverage.py`，并打四类标签之一——
  `js-pending`（CSS 无法表达，JS 逐帧可做）、`js-after-static`（静态还没画出该效果）、
  `renderer-gap`（渲染器不应用该通道，导出侧无解）、`no-op`（不改变画面）。
- 豁免通道**不需要用例**；但若导出器仍保留其绑定（两轴同值时依然准确），不视为违规。

文本类的阈值更低，是因为浏览器与运行时的字体度量、抗锯齿基线存在 1~2px 差异，与动画正确性无关；该差异
单独立项处理，不在本目录的用例里修。

## 2. 已支持的通道（128 个）

> 本表是**导出器绑定表口径**（128 条）；其中 `SolidColor.alpha`、`DropShadowFilter.alpha` 未在注册表 `PAGXNodeChannel.cpp` 声明为可动画，故 §8 的 `supported` 是 126，差异说明见 §8 末。

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

- **整段与逐字混合文本动画**：独立、单行、起点锚定、纯色填充的 Text，可将 `fontSize` / `letterSpacing`
  与单个 `TextModifier` / `RangeSelector` 合成逐字 CSS 关键帧；同一时刻整行只重排一次。
  字体与字形映射需稳定，所有通道需属于同一动画挂载。TextBox 换行、嵌入字形、字体切换、复杂整形、
  多选择器及超出采样预算的混合输入目前返回明确错误，不静默丢弃通道；纯整段、纯逐字沿用原路线。

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

## 6. 通道全集的来源

「本目录覆盖了全部可动画通道」这句话里的**全集**不是手写清单，而是从源码解析出来的：

1. **宏表**：`src/pagx/PAGXNodeChannel.cpp` 的每个 `Build*Fields()` 用 `FIELD_*` 宏登记一个通道，第四个参数为
   `Anim` / `AnimLayout` 即表示可动画（`FIELD_ENUM` 还会多带一个枚举类型参数，所以行的结尾可能是 `, Anim)`，也可能是
   `, Anim, BlendMode)`）。`AppendGroupCommonFields<T>` 用模板参数 `T` 写行，它由 `Group` / `TextBox` / `TextPath`
   三个节点实例化；`Fill` / `Stroke` 的 `color` 不走宏，是 `{"color", Anim, &AccessFillStrokeColor}` 这样的表格行。
2. **解析**：`tools/anim_coverage.py` 按行扫描，记录「源码行号 → (节点, 通道)」（公共行展开到三个节点），
   所以每条通道都能指回它来自哪一行。
3. **行数对账**：脚本先独立数出源码里所有把 `Anim` / `AnimLayout` 放进第四个参数位置的行，再和解析结果比对：
   `宏表 165 行 + 10 条公共行 ×(3−1) = 185 条通道`。只要有一行没被任何规则消费，`--check` 就报
   `registry row not consumed by any rule` 并带上行号与源码原文；两个数字对不上则报
   `registry line count does not match the parse`。

所以**改宏表必须过 `--check`**：新增或删除通道、调整宏参数位置、把 `FIELD_*` 改成别的形状，都会让行数对账、
「用例驱动了注册表没声明的通道」这条反向断言、或节点解析为空这些检查变红，而不是悄悄少算一条通道。

唯一不来自宏表的通道族是 `Path.point{N}.x` / `.y`：导出器在查 writer 表之前就把它解析掉了，注册表里没有对应行，
脚本按 `point{N}.x|y` 的形状单独识别。

## 7. 判定矩阵：supported 与四类豁免

每条通道最终只落在一个判定里。这里要说清一件事：`supported` 在脚本里是**减法定义**的，`supported = 全集 − exempt`，
所以 `全集 = supported ∪ exempt` 这个集合恒等式是**同义反复，本身不构成任何证明**。真正起约束作用的是 case 侧的两条
反向检查，它们才是「没有第三条路」的来源：

- **不在豁免表里的通道必须有用例**，否则 `--check` 报 `animatable channel without a case`；用例还必须真的导出
  `@keyframes`，否则报 `case does not export keyframes`。
- **导出器绑定了的通道必须有用例，且必须被 composite 覆盖**：单通道用例证明映射（缺则 `exported channel without a
  case`），composite 证明多个通道能共存——出现在单通道用例里的每个通道名都必须也出现在某条 composite 里，否则报
  `supported channel missing from a composite case`。

换句话说，一条通道不可能「既没进豁免表、又没人测」而仍然安静地留在全集里：它会变成上述两类红灯之一。

| 判定 | 含义 | 举证要求 |
|------|------|----------|
| `supported` | 导出器实现了该通道，CSS 关键帧能驱动它 | 至少一条单通道用例（`min SSIM` 达标）+ 至少一条 composite 用例；导出器侧有绑定且不是死绑定；用例必须真的导出 `@keyframes` |
| `exempt` / `js-pending` | CSS 这一期无法表达，JS 逐帧重绘可以做 | 写清「CSS 为什么做不到」，并指出 JS 路线具体重绘什么（如重写 `<filter>` 原语、重建 `<stop>` 的 `offset`） |
| `exempt` / `js-after-static` | 静态导出还没画出该效果，动画没有可移动的对象 | 给出静态缺失的证据：writer 零引用、t=0 画面为空，或只是近似（附实测 SSIM） |
| `exempt` / `renderer-gap` | 运行时根本不应用该通道，导出侧（含 JS）无解 | 指出渲染侧缺失的注册点，例如 `convertTextPath()` 不注册通道 writer |
| `exempt` / `no-op` | 通道值不改变任何像素 | 说明为什么不可能影响画面（合成顺序、参数被运行时忽略等），`Layer.scale.z` 的推导见本节末 |

`exempt` 只登记「CSS 这一期做不了」，**不登记「还没做」**：「没实现」不会因为不写用例就消失，它要么变成上面那条红灯，
要么被显式豁免、带着标签和原因落进 §3 的表里。豁免条目越少、理由越具体，这一节的可信度越高。

豁免条目本身也被检查：理由不能为空，且必须以四类标签之一开头（`js-pending` / `js-after-static` /
`renderer-gap` / `no-op`），否则 `--check` 报 `exemption has no reason` 或
`exemption reason must start with one of ...`；豁免指向注册表未声明的通道则报
`channel exemption for a channel the registry does not declare`。

`no-op` 的典型是 `Layer.scale.z`：合成顺序是 `P·T·R·(K·S)`，缩放发生在旋转之前的 z=0 平面上，
所以 z 方向的缩放取任何值都不会改变最终投影，它不是「待办」而是「无对象可做」，因此不进用例、只登记豁免。

## 8. 完整性论证

要证明「本目录覆盖了全部可动画通道」，需要三个前提，缺一条结论就不成立：

1. **全集来自源码，不是人写的清单。** 见 §6：宏表行数与解析结果逐行对账，未被消费的行会带行号报错，
   节点解析为空、总数低于阈值也会报错。
2. **全集里每条通道都归入 supported 或 exempt，没有第三条路。** 这一条**不能**用 `全集 = supported ∪ exempt`
   来论证：`supported` 就是 `全集 − exempt`，那个等式是减法定义的同义反复，写成断言也不会增加任何约束力。
   真正让它成立的是几条反向检查——不在豁免表里的通道必须有用例（`animatable channel without a case`，且用例要
   真实导出关键帧），导出器绑定了的通道必须有用例（`exported channel without a case`）且其通道名还要出现在某条
   composite 里（`supported channel missing from a composite case`），豁免只能指向真实存在的通道
   （`channel exemption for a channel the registry does not declare`），理由必须带四类标签之一。于是「既没被豁免、
   也没被测到」的通道会先变红灯，而不是被并集恒等式吞掉。
3. **每个判定都有反向证据。** `supported` 有单通道用例、composite 用例与真实导出的关键帧；反过来，用例驱动的通道
   必须在注册表里（注册表某行被删而用例还在，会报 `case drives a channel the registry does not declare`）；
   豁免有标签与具体原因。

三者合起来：全集不漏（1），每条通道要么被测到要么被显式豁免（2，靠 case 侧红灯而不是并集恒等式），判定都有证据（3）。

下面的计数由脚本产出，**不要手写**（`python3 tools/anim_coverage.py --emit-readme` 的输出直接粘在下面）：

<!-- generated by `python3 tools/anim_coverage.py --emit-readme`; do not edit by hand -->

| 量 | 值 |
|----|----|
| 宏表可动画行（`, Anim` / `, AnimLayout`） | 165 |
| 其中 `AppendGroupCommonFields` 公共行（×3 节点） | 10 |
| 行数对账（165 + 10×2） | 185 |
| 通道全集 | 185 |
| supported（全集 − 豁免） | 126 |
| exempt（登记在 `tools/anim_coverage.py`） | 59 |
| ├ `js-pending` | 27 |
| ├ `js-after-static` | 21 |
| ├ `renderer-gap` | 10 |
| └ `no-op` | 1 |
| 用例数 | 189 |

185 = 126 (supported) + 59 (exempt)

§2 的 128 条是导出器绑定表口径（见 §2 标题下的说明），比 `supported` 多 2 条 `stale mapping`，不影响上面的等式。

这套证明的边界（别把它读得比实际强）：

- **行数对账是文件内自洽的**：它比较的是「源码里有多少可动画行」与「解析出多少条通道」，两边取自同一个文件。整行删除时
  两边同时变小，对账本身不会红，能红的是用例侧的反向断言（用例还在、注册表里那行没了）。如果被删的通道当时还没有用例，
  两条证据链会一起消失——这种情况只有拿一份全集基线快照去比对才能发现，本目录目前没有快照，所以这条边界真实存在。
- **用例侧是记账口径，不是逐行绑定**：`Target:` / `Channel:` 被读成整份 header 的笛卡尔积，且 `Channel: a, b` 只取
  第一个 token。于是 `combo/07` 的 `Channel: size.width, RoundCorner.radius` 只记到 `Rectangle.size.width`；
  `RoundCorner.radius` 的单通道覆盖来自 `modifier/01_round_corner_radius`，它的 composite 覆盖则按**通道名**匹配，
  由 `composite/06_painter`、`composite/07_gradient` 里的 `radius`（来自 `RadialGradient`）顺带满足；`geometry/05`、
  `stroke/06` 这类两节点用例还会多出 4 个没人声明的配对，已登记在脚本的 `CROSS_NODE_PAIRS` 里。这些都是「声明过、
  名字对上了」级别的证据，不是 (节点, 通道) 级的精确绑定，把它们当成后者会高估这套论证的力量。

## review/ · 结构性回归用例

`review/` 目录存放代码评审提出的结构性问题用例，**暂不并入正式覆盖统计**，用于固定行为基线：

| 用例 | 固定什么 |
| --- | --- |
| `01_filter_inner_then_blur` | 残差阶段（内阴影）在可动画阶段（模糊）**之前**时的链序 |
| `02_filter_blur_then_inner` | 残影阶段在**之后**时的链序 |
