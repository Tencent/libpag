# PAGX → SVG 开发方案与覆盖核查

> 初稿：2026-11-09；本轮重组与语义核查：2026-11-10。评审分支：`feature/lifengdy_svg_export`。
> 源码基点：`847116b92`。源码定位相对于 `/Users/lifengdy/Desktop/project/libpag`，均使用 `file:line`。
> 本轮仅修改本文档，没有源码实现、构建、测试或像素验证；所有推荐决策仍待确认。
> **2026-11-10 补**：新增 1.4 执行分层、4.6 接口迁移、4.7 可用性基线；H0 热修批次（F3 / F5 / F9 / A08）已落地并各带一个回归用例。
> 本次四项契约补充以 `a7c6c9f23` 重新核对：第2.8节固定ST21/ST22/ST28，第3.8节固定M4/M5依赖及账本门禁。仅补文档，不修改或认证用户已有实现。

## 1. 总览、范围与待确认决策

### 1.1 结论与阅读顺序

**先完成静态，再编译动效；共用实例、绘制、资源、状态和诊断契约，不另造第二套解释器。**

静态声明已登记全集：55 个 NodeType / 334 个自有字段、31 个基类或附属结构 / 111 个字段、40 类 enum / 155 个值。完整声明、逐字段运行语义、组合和证据保留在附录 A—F。动效反射的 30 类 / 175 个通道全部分类，另登记反射表外动态路径和图片 payload；**全集已分类不等于全部已支持，更不等于修复或像素已通过。**

正文阅读顺序：第 2 节静态契约与 S0—S6 → 第 3 节动效分类与 M0—M6 → 第 4 节共用工程与验收。附录 E 是唯一 ST 清单，附录 F 是唯一 F 清单；阶段只引用编号，不重复展开缺陷。

### 1.2 目标与责任边界

- 合法文件、PAGX 绘制外观、可编辑矢量、可读原文、源 SVG 结构还原分别验收，不能互相代替。
- 本计划覆盖当前 PAGX 已建模的静态输入，以及有效运行时动画的原生、有界展开、显式有限场景烘焙或拒绝分类；保留 `SVGExporter::ToSVG / ToFile`、Plan / Writer 分工及已有能力。
- 未保存的源 SVG DOM / CSS / 脚本、响应式行为、foreignObject、链接和独立渐变参数不能由 Exporter 凭空恢复；任意 SVG 往返需另确认导入与模型范围。
- StateMachine / VM 的开放宿主输入和历史、任意时刻已有 scene 快照、平台绑定不被当作当前已移植能力；有限事件脚本仅在 D8 明确同意后进入 MB。

| 层 | 责任与数据流 | 边界 / 依据 |
|---|---|---|
| 模板 / 布局 | 节点、资源、结构定义 → 有效资源与布局产物 | 不保存实例实时播放状态；`include/pagx/PAGXDocument.h:69` |
| runtime | 实例 binding → 初始化 / 时间 / ordered apply → 实际绘制 | 不靠修改共享模板模拟播放；`src/pagx/PAGAnimation.cpp:44` |
| SVG | 共用上下文 → DrawRecord / 资源 → 静态表示或 compiled tracks → XML | Writer 不补运行语义；`src/pagx/svg/SVGExporter.cpp:3673` |

引用安全检查先于可能递归的布局、Mask、Composition 和资源遍历；祖先路径检测允许合法兄弟复用。修正共享源头时同步 runtime / SVG / PPT / HTML，不保留错误输出兼容层。

### 1.3 待确认 D 决策（保留原 ID，静态先列）

| ID | 决策 | 推荐契约与确认原因 |
|---|---|---|
| D4 | 静态表示策略 | 默认 PreserveAppearance：原生优先，必要时完整上下文 bake，失败拒绝；VectorOnly 禁止新增效果 PNG，但允许原 bitmap；BestEffort 显式报告损失 |
| D5 | 文字模式 | 默认 Fidelity 使用实际已布局字形 / WOFF2 / 轮廓；Native 保留原文但可能重新塑形；PUA 不承诺原文复制搜索 |
| D6 | 保真目标环境 | 固定画布、背景、色彩、Chromium 版本、缩放 / 分辨率与容差；PNG 不承诺任意缩放无损；再确认其他浏览器 |
| D7 | 静态状态来源 | 默认 TemplateStatic：模板 + 明确供应资源 + 布局，不执行动画 / SM / 默认 DataBind；DefaultHostInitialization、ExistingSceneSnapshot 独立定义 |
| D1 | 根动画选择 | Default取getDefaultTimeline首项，Named只选指定顶层Animation，All按声明顺序选全部顶层Animation（不含顶层StateMachine）；Default首项为StateMachine时Strict拒绝，不偷偷跳过。根先apply，再实际挂载nested driver；两者均不自行激活未挂载定义 |
| D2 | 动效有界近似 | 建议几何 0.25 文档像素、alpha / 颜色 1/255；N/G 必须证明连续时刻误差，不能证明或预算不足则 Strict 拒绝 |
| D3 | 动效失败策略 | 默认 Strict；显式 BestEffort 才允许指定时刻冻结 / 降级且逐项诊断；不以静态 PNG 冒充保留动画 |
| D8 | 可选 MB 输出模式 | 是否允许显式有限场景烘焙：时间窗、初态、事件、资源、分辨率、采样与尾帧 / repeat 必须确定；未同意前 B 类 Strict 拒绝，不是已保真；开放宿主历史仍为 U |

D1—D8 均未擅自确认。推荐数值不是已通过门禁；MB 不是默认开启、已实现或原生矢量能力。

### 1.4 执行分层与首轮交付子集

正文的 S0—S6 / M0—M6 是全量契约，不是一次性交付范围。按「可独立验收 + 可独立回滚」拆三层，避免全量门禁把已交付能力一起卡住。

| 层 | 范围 | 退出条件 |
|---|---|---|
| **H0 热修** | 已定位、改动局部、当前就在污染输出的缺陷：F3（PingPong 时长翻倍）、F5（Group 位置 override 丢失）、F9（任意动画改变 per-child alpha）、A08（跨 duration 关键帧被 clamp） | 每条一个一对一回归用例；`PAGXSVGTest` 全绿；不引入新决策 |
| **R1 首轮** | 静态：ST1、ST2、ST4、ST5、ST6、ST7、ST33、ST34、ST35、ST39、ST40；动效：F1、F2、F4、F6、F7、F8；外加 P1（滤镜参数）、P2（渐变与 stop）两个增量能力 | 只对上面列出的编号建门禁；其余编号保持现状告警，不阻断合入 |
| **R2 后续** | 其余 ST / F；M3 的连续误差证明；M5 的 `d` / 文本 / Selector；MB | R1 收口后重新排期并重估 |

R1 的取舍依据：

- **只做会改变像素或结构的**。ST8—ST31 多为文字与几何的精细语义，缺陷存在但不影响主流文档；先记录，不先修。
- **动效只修契约层的**。F1/F2 是「根入口选错 / 实例作用域错」，F4 是「初态错」，F6/F7/F8 是「插值与 alpha 乘积错」——都会让输出明显不对；M3 的误差证明与 M5 的拓扑展开留到 R2。
- **P1/P2 与任何待定决策无关**，不需要等 D1—D8。

推迟的编号在文档里保留分类，不进入本轮门禁；`classified` 与 `supported` 的区分照旧成立。

### 1.5 已采纳的 D 决策

按 1.3 的推荐值执行，不再逐项等待确认；若后续要改，必须重跑对应门禁并把差异写回本节。

| ID | 采纳值 | 当前落地状态 |
|---|---|---|
| D1 | Default：`getDefaultTimeline` 首项 | `SVGAnimationPlan` 只遍历 `Layer::timelines`，等价于「实际挂载的驱动」；Named / All 尚未实现（M1） |
| D2 | 几何 0.25 文档像素 / alpha 1/255 | 误差计算未实现，作为 M3 的验收阈值 |
| D3 | 默认 Strict | 现为「告警 + 降级」，属 BestEffort；切换 Strict 需先补拒绝路径（M6） |
| D4 | PreserveAppearance | 等价于现有 `bakeUnsupported = true` |
| D5 | Fidelity | 等价于 `convertTextToPath = false` + `embedFontsAsWoff2 = true` |
| D6 | 固定 Chromium + 固定画布 / 背景 / 缩放 / 容差 | 已落地（2026-11-11）：`test/svg_browser_gate.js` + `SVGBrowserTest` 使用 Playwright 自带 Chromium、viewport 1400×900、白底、deviceScaleFactor=1、pixelmatch threshold 0.1；首批 `constraint_textbox` 标定阈值为 pixelDiff 0.05 / meanRgbDelta 10 / SSIM 0.4（文字 AA 差异） |
| D7 | TemplateStatic | 已落地：`ToSVG` 只读模板字段，不执行动画、StateMachine 或默认 DataBind |
| D8 | 不启用 MB | B 类保持拒绝与告警，不引入有限场景烘焙 |

## 2. 静态导出：契约、补齐与阶段
### 2.1 范围与能力摘要

静态指无动画文档，或 `exportAnimation=false` 的 TemplateStatic；它既不是动画 t=0，也不是 scene 首次 draw。line / polyline / polygon 可用等价 Path，use / symbol / marker 可已展开为 Layer；依据 `src/pagx/svg/SVGImporter.cpp:446`、`src/pagx/svg/SVGImporter.cpp:490`。RadialGradient 没有独立 focus，Gradient 没有 spreadMethod，不能还原未保留参数（`include/pagx/nodes/RadialGradient.h:31`、`include/pagx/nodes/Gradient.h:33`）。

“已有”仅表示代码路径存在。完整字段见附录 A/B，以下只汇总能力，不再次逐节点列成员。

| 范畴 | 现状 / 补齐边界 | 阶段 |
|---|---|---|
| 根画布、约束与容器布局 | 已有固定 viewBox、约束、flow、renderScale；补布局失效、NaN unset、真实字体与有效尺寸 | S0 / S1 / S3 |
| Layer / Group / Composition | 已有包装、几何累积、Painter 分次发射与内联；补实例 ID、外部归属、有效内容优先级与捕获矩阵 | S1 |
| Mask / Contour / scroll | 已有简单覆盖；补跨父相对矩阵、根 alpha / effects / composition、stroke 覆盖、隐藏根特殊语义及一次裁剪 | S1 / S2 / S4 |
| alpha / blend / 背景隔离 | 已有 opacity / mix-blend-mode；必须按实际离屏条件保留整体或逐项合成，不能只套 `<g>` | S1 / S2 |
| primitive / Path / Stroke / modifier | 常规几何和解析已有；补方向起点、退化微轮廓、内外描边、adaptive dash、Append、Merge 清 Painter、Repeater 顺序 / 幂 | S4 |
| Text / TextBox / GlyphRun | Native / WOFF2 / 路径 / bitmap 已有；补 layoutRuns、多 Font、短数组、faux、最终字形样式、有效空布局和绘制次序 | S3 |
| TextPath / Selector / TextModifier | 现有 bake 不能代替完整语义；优先展开实际最终 glyph 与 base / overlay paint | S3 |
| 渐变 / 色彩 | linear / radial 与 stops 已有；补空 / 单 / 退化规则、P3 / alpha / 空间插值和 shape / glyph 各自 fit bounds | S5 |
| Image / ImagePattern | 已有图片与部分 repeat / bake；补消费方资源链、真实多格式、16 tile 组合、4 fit、6 sampling 与完整变换 | S5 |
| styles / filters / backdrop | primitive 与部分 bake 已有；补有序输入、Below / Above bucket、spread / tile / noise / Glass 和背景消费边界 | S2 |
| 2D / 3D / antiAlias | 当前 Layer 已有组件与 anchor，仿射 / 常量 w 可原生；一般透视与有效 preserve3D 已有 bake，补 flatten / Mask / AA | S1 / S2 |
| 资源、元数据与生命周期 | 补统一诊断、有效依赖、局部产物、不污染共享数据、合法缓存测试和文件失败行为 | S0 / S6 |

### 2.2 唯一共用导出上下文与 DrawRecord

不新增通用场景框架；扩展现有 `AccumulatedGeometry` 与 exporter-owned 局部产物，供第 3 节复用。

```text
ExportContext:
  selectedStateSource, effectiveOptions, effectiveFontConfig
  instanceMap, globalIdAllocator, resourceResults, diagnostics, budgets
  ancestorReferenceStack, localGeometryAndGlyphProducts
InstanceKey = (ownerDocument, stableInstancePath, sourceNode)
DrawRecord:
  instanceKey, sourceGeometry, capturedGeometryIdentities
  capturedInnerMatrices, finalGeometryMatrices, geometryTransformChain
  geometryToPainter, painterToLayer, painterScope, painterOpacityChain
  painter, painterLifetimeAfterModifiers, paintBounds
  placement, drawOrder, copyDrawOrder, perGlyphStyle
  compositionBoundary, modifierProvenance, dependencies, referencedResources
BakeConsumption:
  consumedDrawRange, replacementRegion, samplingHalo, coordinateSpace
  appliedAlphaMaskScrollEffects, remainingForeground
```

- sourceNode 是模板身份，instancePath 是引用实例；ID 与 defs 缓存必须包含影响输出的实例、空间与依赖，不能全局按 Node 指针复用。
- geometry transform 与 Painter opacity 分离：外部 Painter 可消费 Group 上行几何而不继承该 Group alpha；保存捕获 innerMatrix 与最终矩阵，不能靠最终 inverse 恢复。
- `paintAlpha(t)=painterScopeAlpha(t)*painterAlpha(t)*colorAlpha(t)`；静态使用同一表达式。多个因子不得丢失、相互覆盖或除以静态零 alpha。
- Painter 捕获引用后仍受 modifier 生命周期影响；Merge 清旧几何及 Painter，Repeater 复制并重绑定，Text 可转 Shape。不能先 resolve 全部 modifier 后遇 Painter 立即输出就宣称等价。
- bounds 分四类：layout、paint-fit、ink / coverage、bake 采样裁剪。shape fit 使用包含全部控制点的 Path.getBounds，glyph fit 使用 textBlob tight bounds，不推广一种规则到全部消费者。
- MatrixShape 奇异矩阵返回 null，不等于零尺寸 primitive 的微轮廓；Painter 捕获前 / 后零缩放分别验证，恢复不强行求 inverse（`third_party/tgfx/src/core/shapes/MatrixShape.cpp:30`）。

数据流依据：`third_party/tgfx/src/layers/VectorLayer.cpp:69`、`third_party/tgfx/src/layers/vectors/Painter.cpp:25`、`third_party/tgfx/src/layers/vectors/VectorGroup.cpp:162`、`third_party/tgfx/src/layers/vectors/MergePath.cpp:70`、`third_party/tgfx/src/layers/vectors/Repeater.cpp:139`。fit 依据保留在附录 B.2。

### 2.3 静态 policy、文字模式与 API 收口

| 拟议类型 / 值 | 权威契约 |
|---|---|
| SVGStaticExportPolicy.VectorOnly | 禁止为效果新增 PNG；原 Image / bitmap glyph 可保留；无法原生或最终几何等价表达则拒绝，非“绝对无位图” |
| SVGStaticExportPolicy.PreserveAppearance | 原生优先；已建模但不等价内容仅在满足固定 D6、完整状态 / 资源 / 预算时 bake；失败拒绝 |
| SVGStaticExportPolicy.BestEffort | 近似 / 丢弃必须显式诊断；无效输入、执行失败或无界消耗不能当作正常损失 |
| SVGTextExportMode.Fidelity | 实际布局字形权威；使用 WOFF2 / outline / 原 bitmap；layoutRuns 与 authored glyphRuns 都覆盖 |
| SVGTextExportMode.Native | 保留可读字符串，目标字体可能重新塑形；超容差不得绕过 policy，可正确回退或拒绝 |

保留 `ToSVG / ToFile` 与 `exportAnimation`，公开接口、CLI 和统一预算只在第 4.1 节定义。以下收口同步全部调用方，不留兼容层：

- 用 staticPolicy 替换 `bakeUnsupported`，用 textMode 替换 `convertTextToPath`；其他格式 text-to-path 含义另按自身契约处理，不生硬替换。
- **移除 `resolveModifiers` 作为“关闭就丢效果”的开关**；静态与动效默认解析所有必要 modifier。若保留编辑偏好，只能选择表示，不能绕过所选 policy、静默丢 Polystar / TextPath / modifier。
- `embedFontsAsWoff2` 可保留为字形编码偏好，不允许 TextBox 绕过权威字形，不承诺 PUA 原文搜索或复制。
- staticPolicy 与 animationPolicy 正交：静态 bake 不授权动画冻结，动画 BestEffort 也不授权 VectorOnly 新增效果 PNG。

### 2.4 状态、资源与缓存生命周期

| 状态来源 | 本方案含义 |
|---|---|
| TemplateStatic（D7 推荐） | 模板字段 + 供应的资源 + 布局；不执行动画、SM、VM 默认绑定；有效绑定登记未初始化宿主 |
| DefaultHostInitialization | 新scene先构造全部VM/DataContext/referenceInstance链接，再bind和首次flush；父slot与Composition复用同一VM实例，open slot回填；随后apply/advance/draw顺序独立确认，不是直接ToSVG原模板 |
| ExistingSceneSnapshot | 业务值、播放位置、trigger / Once 历史与实际资源；独立快照入口，不能从模板逆推 |

PAGScene.draw先flushDataBinds/flushTextHolders（`src/pagx/PAGScene.cpp:514`）；TemplateStatic参考不初始化行为，原生与bake同源。若选择默认宿主或有限场景，必须复用全部VM链接→统一bind→首次flush的两阶段流程（`src/pagx/PAGScene.cpp:300`、`src/pagx/PAGScene.cpp:349`），以及父slot与Composition的共享VM身份；提前bind可永久漏跨层source，逐实例新建VM也可能改变共享值。静态绘制modifier始终有效，不因名字而被当宿主行为排除。

资源消费链不能因共用工具而统一改语义：

- ImagePattern：runtimeImage → data → dataURI → filePath（`include/pagx/PAGXDocument.h:208`）。
- 默认 VM 图片：data → dataURI → filePath，不读 runtimeImage（`src/pagx/PAGScene.cpp:53`）。
- bitmap Glyph：codec data → dataURI → filePath，不读 runtimeImage（`src/renderer/GlyphRunRenderer.cpp:157`）。
- WOFF2 bitmap 编码内部只读 Image.data；当前 SVG 不启用 bitmap Font 的 WOFF2 预构建，走逐 glyph image（`src/pagx/utils/Woff2FontGenerator.cpp:1263`）。

每个消费者原生与 bake 同源；真实解码 / 尺寸 / GPU readback 与 MIME 分开验收。本计划不自动下载未供应网络资源；源链若需改变，先确认源头行为变化。

Font.glyphs 首次 render 后修改为 undefined（`include/pagx/nodes/Font.h:103`）。`built` 即使 typeface 构建失败也置位并缓存失败（`src/renderer/GlyphRunRenderer.cpp:109`）；不能承诺热 Font 修改 glyphs 自动重建。cold / warm 仅用合法同输入、新对象或既定清理流程比较。

共享文档布局、Font/PathData/索引缓存与外部调用须串行化；局部树不等于整个API线程安全。解析/采样产物不得持续追加document.nodes，不修改源Fill/glyph/PathData；合法失效与notifyChange归属保持既有契约。导出内部可能获取GPU设备锁，调用方不得已持有同一设备context锁（`include/pagx/SVGExporter.h:124`）；原生与bake分支均按该调用前置条件验证，不能绕锁或并发缓存读写。

### 2.5 完整背景合成与 bake 消费契约

现有 probe 已把 TextPath / TextModifier、conic / diamond、一般透视与有效 preserve3D 送入 bake（`src/pagx/svg/SVGFeatureProbe.cpp:86`）；已有实现不是完整保真证明。普通 Layer bake 与带背景能力分别见 `src/pagx/utils/RasterUtils.cpp:130`、`src/pagx/utils/RasterUtils.cpp:351`。

1. 先判原生等价，再选最小但**完整可隔离合成边界**；所有 Mask / 引用路径都经过能力判断，不能只检查普通 Layer。
2. 使用实际目标之前的 draw 前缀与背景；sampling halo 与 replacementRegion 分开，不能用透明背景上的目标 PNG 冒充 backdrop。
3. 背景 PNG 已含背景 + 目标，必须消费对应 draw 前缀，或裁掉旧前缀在 replacementRegion 内的输出；不能叠回原背景造成二次 alpha。记录 consumedDrawRange 和剩余 foreground。
4. alpha、Mask、scroll、clip、effect、3D flatten 各只作用一次；后续 sibling / 祖先 foreground 不得提前消费，深度交叉保留完整上下文。
5. 无法证明局部替换安全就扩大到完整边界；不能悄悄丢背景。affine / 常量 w 可证等价时保持矢量，不把所有 3D 一律转图。
6. shader / sampling / antiAlias / colorSpace 不等价、资源 / GPU / 编码 / 预算失败按 policy 处理；PreserveAppearance 仅在固定 D6 下成功，不声称 PNG 任意缩放矢量无损。
7. 有效动态依赖不得默认单 PNG 冻结；静态缺陷能修就从源头修，不用截图长期掩盖。可选动效 bake 只按第 3.7 节 MB 契约执行。

### 2.6 静态实施 S0—S6

阶段中的文件均是未来实现范围，不代表本轮改动；共用工程前置，S6 只收口。

#### S0：声明、状态与验证入口

- 文件：`test/src/PAGXSVGTest.cpp`、`test/src/PAGXCliTest.cpp`、`tools/html-snapshot`、`CMakeLists.txt`、`include/pagx/SVGExporter.h`。
- 任务：附录 A—C 声明集合、D.1 组合、E 的 ST1—ST46 建门禁；确认 D4—D7；no-op / inactive / 有效空 / 未初始化宿主分类；共用 XML、同源静态一帧与预算 / 失败入口。
- 验收：显式 `exportAnimation=false` 无 SMIL；可见控制样例非空；声明漏项阻断；历史通过数不冒充本轮结果，不用墙钟碰巧采帧。

#### S1：实例、坐标、Mask 与合成

- 文件：`src/pagx/svg/SVGExporter.cpp`、`src/pagx/utils/ExporterUtils.{h,cpp}`、`src/pagx/utils/LayerTransformUtils.{h,cpp}`、`src/pagx/nodes/Layer.cpp`、`src/pagx/PAGXDocument.cpp`、`test/src/PAGXSVGTest.cpp`。
- 任务：唯一 ID / 引用 / defs；共用 DrawRecord；跨父 Mask 相对矩阵与完整像素 Mask；Contour stroke / clip-rule；owner / Mask 根可见性；composition 与 contents 实际优先级；生成 scrollRect 重布局失效；真实离屏 alpha / blend / 背景隔离。
- 验收：ST1—ST5、ST17、ST33—ST35、ST44—ST45 与 C01—C07；零 / 负 / 奇异矩阵、捕获前后 scale、父空间和状态各只作用一次，布局源头变化同步 runtime。

#### S2：effects、背景、3D 与采样回退

- 文件：`src/pagx/svg/SVGExporter.cpp`、`src/pagx/svg/SVGFeatureProbe.{h,cpp}`、`src/pagx/utils/RasterUtils.{h,cpp}`、`test/src/PAGXSVGTest.cpp`。
- 任务：11 种 effect 全字段；style Below / Above 顺序与输入 bucket、excludeChildEffects；逐步 filter output、user-space region / halo；spread / tile / noise / antiAlias 能力判断；Glass own geometry / SDF-UDF；背景消费前缀；透视 / preserve3D 与 flatten 完整上下文。
- 验收：ST6—ST7、ST17、ST38—ST40、C07—C11 / C22；小 bbox 大 blur 不裁、shadowOnly 链有序、背景不二次叠加、不提前画 foreground；GPU / 编码失败正确拒绝。

#### S3：文字、最终 glyph 与布局

- 文件：`src/pagx/svg/SVGExporter.cpp`、`src/pagx/utils/TextUtils.{h,cpp}`、`src/pagx/TextLayout.{h,cpp}`、`src/pagx/TextLayoutParams.h`、`include/pagx/nodes/LayoutNode.h`、`src/pagx/LayoutNode.cpp`、`src/pagx/nodes/TextBox.cpp`、`src/renderer/GlyphRunRenderer.{h,cpp}`、`src/pagx/utils/Woff2FontGenerator.{h,cpp}`、`test/src/PAGXSVGTest.cpp`、`test/src/PAGXTest.cpp`。
- 任务：TextBox 两模式共用 Painter；layoutRuns / glyphRuns、多 Font / size、7 数组、offset / advance / anchor / faux、bitmap alpha 和 run 顺序；resolved box / valid empty；TextPath 八字段、Selector 全分支、TextModifier base / overlay；有效 fontConfig、Font 不可变与串行缓存。
- 验收：ST8—ST10、ST18、ST20—ST22、ST28—ST31、ST36/ST44、C12—C15/C19；ST21按第2.8.1节分支/数字/排他capture，ST22按TP01—TP16，ST28按第2.8.3节继承入口/逐轴分配/0与NaN。RTL/竖排/rich text、Native损失与PUA原文边界独立验证，PPT/HTML共用字形不回归。

#### S4：几何、Stroke 与 modifier

- 文件：`src/pagx/svg/SVGExporter.cpp`、`src/pagx/svg/SVGPathParser.cpp`、`src/pagx/PathData.cpp`、`src/pagx/utils/StrokeGeometryUtils.{h,cpp}`、`src/pagx/utils/ModifierResolver.{h,cpp}`、`src/pagx/ppt/PPTExporter.cpp` 及实际 PPT 输出模块、`test/src/PAGXSVGTest.cpp`。
- 任务：原轮廓 / 方向 / 起点 / 微轮廓 / 数值精度；内外 Stroke 倍宽 clip 或等价展开、adaptive dash / cap / join；完整 VectorContext；Text→Shape、Merge 清 Painter、Append 非 Union；Repeater 次序、整数幂、fractional alpha；局部产物与来源；移除丢效果开关。
- 验收：ST11—ST12、ST16、ST23—ST24、ST27、ST42—ST45、C05—C06 / C16 / C21；负底数分数幂按真实边界处理，不自造 SignedPow；PPT 补偿只留 PPT 端，模板 Fill / nodes 不污染。

#### S5：paint、资源与消费者 fit

- 文件：`src/pagx/svg/SVGExporter.cpp`、`src/pagx/utils/ImageFormatUtils.{h,cpp}`、`src/pagx/utils/RasterUtils.{h,cpp}`、`src/renderer/LayerBuilder.{h,cpp}`、`test/src/PAGXSVGTest.cpp`。
- 任务：渐变空 / 单 / 重复 stop、退化 / 无效与空间插值；实际 shape / glyph fit 与独立 coverage；资源链闭合、external Font / fontConfig、真实 PNG / JPEG / WebP / GIF；16 tile × 4 fit × 6 sampling、旋转 / skew / 负 scale 同源 bake。
- 验收：ST13—ST15、ST19、ST25—ST26、ST36—ST37 / ST42、C17—C19；MIME 不替代解码，runtimeImage 红蓝优先链、data URI、P3 / alpha 与高频缩放直接验证，失败不静默改 Repeat / scaleMode。

#### S6：API、文件与静态交付

- 文件：`include/pagx/SVGExporter.h`、`src/pagx/svg/SVGExporter.cpp`、`src/pagx/svg/SVGFeatureProbe.{h,cpp}`、`src/cli/CommandExport.cpp`、所有 SVGExporter 调用方、`test/src/PAGXSVGTest.cpp`、`test/src/PAGXCliTest.cpp`、共用评测入口、`CMakeLists.txt`。
- 任务：落实已确认静态决策与替代选项；全集分类 / 漏项阻断；SDK / CLI 前置一致、诊断不依赖 Plan 或 warnings sink；数量 / 像素 / 字节预算、合法缓存串行化、文件事务；共享工具全量回归。
- 验收：ST32、ST37、ST41、ST43—ST46、C20—C24 与全部有效静态路径；分别记录原生 / 展开 / 完整 bake / 拒绝 / 损失，所有写失败保旧文件承诺按第 4.1 节验收，不声称 crash durability。

### 2.7 静态验收索引

附录 D.1 的 C01—C24 覆盖不可替代的交互，附录 E 的 ST1—ST46 保留缺陷与补核查依据；闭合 enum 逐值、bool 双值、optional unset / 0 / 非 0、数组空 / 短 / 完整 / 超预算。无动画文档开关字节相等只证明无动态不回归，不证明静态像素正确。

原生 / bake 比较同一 D6 / D7 输入，固定资源后采一帧；关键区域、轮廓和字形位置直接检测，SSIM 辅助。源 SVG→PAGX 与 PAGX→SVG 两段分别判损失，不让双方同错的 round-trip 抵消缺陷。

### 2.8 ST21 / ST22 / ST28 的固定实现与验收契约

本节以当前 `a7c6c9f23` 的 PAGX / tgfx 实际实现为依据，补清语义、数字参考与实现入口，不改用另一份 AE 导出，也不新增 TextBox 字段。以下期望值是源码推导的测试标准，尚未在本轮运行验证；旧进度记录中的“已收口”不代替这些组合验收。

#### 2.8.1 ST21：override 是有权重的绘制覆盖，不是无条件换 Fill

**唯一语义：遵守 tgfx::TextModifier + FillPainter / StrokePainter。** `fillColor` / `strokeColor` 的 alpha 是覆盖权重，selector factor 的绝对值参与权重；不直接替换源 Painter，不对原 shader 颜色先做一个简单 lerp。依据 `third_party/tgfx/src/layers/vectors/TextModifier.cpp:44`、`third_party/tgfx/src/layers/vectors/FillStyle.cpp:66`、`third_party/tgfx/src/layers/vectors/StrokeStyle.cpp:63`。

定义 `f` 为组合 selector factor，`u=abs(f)`。GlyphStyle 初始 override 权重 `w=0`，不含原 Fill / Stroke 的颜色。逐个 modifier 按作者顺序累积，旧 override RGB 为 `C`，新 override RGB 为 `U`、alpha 为 `a`：

```text
t = a * u
if t <= 0: keep the previous override
wNext = w + (1 - w) * t
if t >= 1: CNext = U
else: CNext = (C * w * (1 - t) + U * t) / wNext
```

不在 `wNext=0` 时做除法；transparent override 不清除已有覆盖，optional unset 只是不施加这一项。glyph 自身 alpha 独立累积：`gNext = g * max(0, 1 + (modifier.alpha - 1) * u)`。变换仍使用有符号 `f`，不能把所有通道统一改为 `u`。

**最终绘制分支写死如下。** 令 `r = effectivePainterAlpha * glyphStyle.alpha`；effectivePainterAlpha 按实际 Painter / 作用域求值，Layer 的整体合成 alpha 保留在其原位置。原 ColorSource 自带的 alpha 仍在 base shader 内，不再重复计入 `r`。

| 最终覆盖权重 | base 分支 | overlay 分支 |
|---|---|---|
| `w=0` | 原 shader，原 Painter blendMode，paint alpha=`r`，绘制一次 | 不绘制 |
| `0<w<1` | **完整原 shader**，paint alpha=`r`，原 blendMode；不乘 `1-w` | 紧接着绘制 RGB=`C`、shader alpha=`w` 的纯色覆盖，paint alpha=`r`，固定 SrcOver |
| `w=1` | 不绘制 | 仅纯色覆盖，shader alpha=`1`、paint alpha=`r`，固定 SrcOver |

这是每个有效 Painter、每个连续 glyph-style run 的分支，不是整段文字先画一次 base 再画全部 overlay。overlay 沿用原 Painter 的 placement；Stroke 两分支使用相同最终 runStroke、cap / join、alignment 和原 pathEffect。最终宽度由 `BlendStrokeWidth(baseWidth, GlyphStyle)` 求值，不以颜色权重代替 strokeWidthFactor。没有有效源 Fill / Stroke 时，颜色 modifier **不会凭空创建 Painter**；原 Stroke width<=0 不产生 Painter，不能靠 override 把它复活。

**拆组和累计几何的处理必须具有排他性。** 推荐不把 styled glyph 的局部 Fill 回插到共享 PAGX 元素列表；在导出局部 DrawRecord 中发射最终绘制叶子：

```text
for each effective painter in draw order:
  for each captured geometry in that painter:
    for each contiguous glyph-style run in source order:
      emit the base branch if w < 1
      emit the overlay branch if w > 0
      consume the original draw identity (painter, geometry, run, placement)
```

同一原 Painter 的旧绘制记录被上述最终分支**替换**，不能保留旧整段 Fill 再追加局部 Fill。发射的叶子不重新进入任何祖先 Painter 的累计 geometry；若确需临时 PAGX Group，必须重建对应 Painter capture 列表及同下标 innerMatrices，并移除它对这些输出克隆的再次消费。**普通嵌套 Group 不是隔离证明**。其他本来就消费同一源 geometry 的合法 Painter 各自保留一次完整分支，不能全局删除所有外部 Fill，也不能把真实多 Painter 合并成一个颜色。

`Painter::captureGeometries` 保存 Geometry 引用而非冻结 glyph style，后置 modifier 会改变已捕获 Painter 最终看到的 glyph；依据 `third_party/tgfx/src/layers/vectors/Painter.cpp:25`、`third_party/tgfx/src/layers/vectors/Painter.cpp:34`。源 Geometry / Font / GlyphRun 不增加 per-character paint 字段，最终 GlyphStyle 与分支由导出局部产物承载。部分覆盖的 base+overlay 不能套统一 `<g opacity=r>`，必须各分支施加 paint alpha，避免改变重叠合成。

**整数几何与可手算的颜色验收：** 采用边长20、采样点位于满覆盖内部的单 glyph，工作色空间SRGB、透明背景、蓝色不透明 base、红色 override。alpha/权重的分数是语义输入，不是模糊几何坐标。

| 输入 | 预期有效绘制及像素（预乘RGBA） |
|---|---|
| `w=0, r=1` | 一次蓝色：`(0,0,1,1)` |
| `w=1/2, r=1` | 蓝base一次+红overlay一次：`(1/2,0,1/2,1)` |
| `w=1/2, r=1/2` | 两分支分别半透明：`(1/4,0,3/8,5/8)`；非预乘RGB=`(2/5,0,3/5)`，不是alpha=1/2的紫色 |
| `w=1, r=1/2` | 只有半透明红：`(1/2,0,0,1/2)`；base必须不存在 |
| 红、绿modifier依次 `a=1, u=1/2` | 累积 `w=3/4, C=(1/3,2/3,0)`；不是最后直接变绿 |
| 在已有override后加 `a=0` 或 optional unset | 已有 `w,C` 不变；不清空、也不多画 |

还必须验证：gradient / pattern base、非Normal base blend、带描边及strokeWidthFactor、多Painter、后置modifier、bitmap/vector混排、AA边缘。结构断言固定每个来源draw的分支数量和顺序，像素与tgfx对照；AA边缘和复杂blend不能用满覆盖内部公式替代完整渲染。ST21通过这些门禁前不能仅凭“拆成局部Fill”声明已支持。

#### 2.8.2 ST22：以 tgfx 为唯一标准的沿线坐标验收

**以本仓库 tgfx::TextPath 的实际语句为准，不以 AE 样例为基准；AE只可作为附加输入，不能覆盖当前PAGX模型契约。** 当前八个作者字段是 `path, baselineOrigin, baselineAngle, firstMargin, lastMargin, perpendicular, reversed, forceAlignment`，声明见 `include/pagx/nodes/TextPath.h:35`。用户关心的“闭合”“超出路径”“字距”“垂直对齐”分别是路径结构/沿线规则/force间距/normal法向投影，不新增同名字段。

设路径长度`L>0`，`M`为geometry到TextPath共同空间的矩阵，`G`为旧glyph矩阵，`A`为去除G平移后的线性部分，`a`为实际glyph anchor，`p=M*G*a`，baseline origin=`O`，baseline angle=`beta`：

```text
Normal:
  d = dot(p - O, (cos(beta), sin(beta)))
  h = dot(p - O, (-sin(beta), cos(beta)))
  s = firstMargin + d
  lastMargin does not participate
ForceAlignment:
  extraGap = 0
  if N > 1 and sum(fontAdvance) > 0:
    extraGap = (L + lastMargin - firstMargin - sum(fontAdvance)) / (N - 1)
  s[i] = firstMargin + fontAdvance[i] / 2 + sum(fontAdvance before i) + i * extraGap
  h = 0
```

force使用字体水平advance，不额外乘glyph.scale，不拉伸字形；忽略旧position、baselineOrigin/Angle和旧法向偏移。各glyph advance不同时，anchor间距不是等距。`lastMargin`为**终点方向的有符号偏移**：负值留白，正值扩出路径；不能把两端正margin都当内缩。余量为负时保留负extraGap，不自行拒绝或夹零。

然后按固定次序：`reversed`令`s=L-s`；闭合路径仅在`s<0或s>L`时正模回绕，恰好`s=L`保留终点与末段切线；开放路径超端沿起点/终点切线延伸，不裁字也不clamp。normal的新anchor为`q=P(s)+h*(-tangent.y,tangent.x)`；force为`q=P(s)`。reversed改变沿线距离和perpendicular的旋转方向，**不把这个法向再取负**。

perpendicular=true时`R=Rotate(arg(reversed ? -tangent : tangent)-beta)`，force旋转使用beta=0；false时R=I，保留已有glyph线性变换而非强制字形直立。新局部矩阵为 `GNext = inverse(M) * Translate(q - R*A*a) * R*A`，保证`M*GNext*a=q`。使用不可逆M时不强行求逆恢复，按第2.2节有效几何/失败契约处理。多contour当前只测量第一contour，不自行走到下一contour。依据 `third_party/tgfx/src/layers/vectors/TextPath.cpp:95`、`:117`、`:127`、`:155`、`:170`、`:217`。

**统一参考资源，避免依赖系统字体：** 程序构造向量Font，unitsPerEm=100、fontSize=100、三个glyph各advance=20、anchor=(10,0)，旧G分别平移(0,10)、(20,10)、(40,10)，所以旧anchor为(10,10)、(30,10)、(50,10)。可用glyph轮廓矩形(0,-8)到(20,2)，M=I、O=(0,0)、beta=0。定义H100为`M0 0 L100 0`，H200为`M0 0 L200 0`；V100/V200为对应竖直线。闭合Q为`M0 0 L50 0 L50 50 L0 50 Z`，L=200；闭合测试取h=0并用firstMargin设置目标距离。

| 验收ID / 语义 | 输入变化 | 必须相等的参考anchor / 旋转 |
|---|---|---|
| TP01 起点偏移 / normal | H100、first=10或-20 | x分别20/40/60或-10/10/30，y均10；H100换H200不自动铺满 |
| TP02 normal首尾留白 | 固定first=10，改变last为-10/0/20 | 始终(20,10)/(40,10)/(60,10)，证明normal不消费last |
| TP03 normal法向 / perpendicular | V100、true/false | anchor均(-10,10)/(-10,30)/(-10,50)；true追加+90°，false不加旋转，glyph已有A保留 |
| TP04 反向距离与法向 | reversed=true，H100/V100 | H为(90,10)/(70,10)/(50,10)、true追加180°；V为(-10,90)/(-10,70)/(-10,50)、true追加-90°；false无追加 |
| TP05 真反向path≠reversed标记 | H100路径改为`M100 0 L0 0`，reversed=false | anchor=(90,-10)/(70,-10)/(50,-10)，追加180°；不能照TP04保留y=10 |
| TP06 对齐基准 | 单glyph旧anchor=(15,30)、O=(5,0)、beta=90°、H100 | d=30、h=-10，q=(30,-10)、perpendicular=true追加-90°；normal使用anchor不是glyph左上角 |
| TP07 force字距与法向 | force=true，H100/H200或V100 | H100=(10,0)/(50,0)/(90,0)，H200=(10,0)/(100,0)/(190,0)；V100=(0,10)/(0,50)/(0,90)；旧y=10消失 |
| TP08 force首尾符号 | H100，first=10,last=-10 | extraGap=10，x=20/50/80，y=0，正first留起始空白、负last留终止空白 |
| TP09 开放超端与负间距 | force H100：first=-20,last=20；或first=80,last=-80 | 前者x=-10/50/110沿线延伸；后者extraGap=-60，x=90/50/10，不clamp或拒绝负间距 |
| TP10 非等advance | force H200，advance=20/40/60、anchor=10/20/30 | extraGap=40，x=10/80/170，不等距anchor也不拉伸glyph |
| TP11 单字 / 空路径 | force单glyph advance20，first=10，任意last/O/beta | s=20；N=0、path空或L<=0时TextPath不修改glyph，不伪造字形 |
| TP12 闭合与终点 | Q，s=-10/210/0/200、h=0 | -10→190→(0,10)，210→10→(10,0)；0/200都为(0,0)，0切线向右、200末切线向上，不混同回绕端点 |
| TP13 先reverse再闭合回绕 | Q，reversed=true，s原=-10/210 | L-s得到210/-10，再模为10/190；q=(10,0)/(0,10)，旋转再用反向切线 |
| TP14 多contour | H100后追加远处V200，normal距离s=110、h=0 | 仍沿第一H延伸到(110,0)，不进入后续V |
| TP15 布局与字距单位 | H100设置width=200,left=30,top=40，force三glyph | path变成(30,40)→(230,40)，O变(30,40)；q=(40,40)/(130,40)/(220,40)，glyph advance仍20，不把字形放大两倍 |
| TP16 force忽略基准、保留A | TP07上改变旧position、O、beta，或给glyph已有90°线性旋转 | q仍与TP07相同；force不消费旧基准，已有A不被清掉，最终R按beta=0求 |

TP03首glyph的true矩阵为`{a=0,b=1,c=-1,d=0,tx=-10,ty=0}`，映射(10,0)得到(-10,10)；false矩阵为Translate(-20,10)。加入非恒等可逆M时直接断言`M*GNext*a=q`，不要只比较一张看起来位置差不多的截图。

布局入口复用 `src/pagx/nodes/TextPath.cpp:35`、`src/pagx/nodes/TextPath.cpp:67` 和 `src/renderer/LayerBuilder.cpp:2344`：先求uniform scale k及平移T，再得到`path'=k*path+T`、`O'=k*O+T`；first/last、font advance、glyph线性矩阵和angle不跟随k再缩放。glyph字距输入若来自Text.letterSpacing，应先塑形生成旧G；normal保留其沿线投影，force按自己的extraGap重排，不另设TextPath letterSpacing。

**通过标准：** TP01—TP16在独立向量字形的anchor/矩阵测试中位置误差<=1e-4文档像素、矩阵分量误差<=1e-4；数值均有限。再用同一fixture对照tgfx和导出SVG的轮廓、裁剪、Painter/override及可见区域，按D6固定环境门禁验收。曲线路径采样复用tgfx PathMeasure，不用AE或自写弧长近似替换；曲线、尖角、TextModifier前后次序与layout另补组合。源码推导坐标不能在本轮标成已通过测试。

#### 2.8.3 ST28：保留，修的是继承尺寸的读取优先级

**不删除ST28，不新增TextBox.width/height/Native字段。** `TextBox : Group : Element, LayoutNode`，width/height实际声明在LayoutNode并被继承：`include/pagx/nodes/TextBox.h:43`、`include/pagx/nodes/Group.h:36`、`include/pagx/nodes/LayoutNode.h:94`、`include/pagx/nodes/LayoutNode.h:100`。Native是SVG导出路径/拟议输出模式，不是TextBox数据字段。

明确入口：XML `width/height`在 `src/pagx/PAGXImporter.cpp:1307` 调用ReadDimension写继承字段；约束分配在 `src/pagx/LayoutNode.cpp:150`；TextBox实际排版在 `src/pagx/nodes/TextBox.cpp:129` 使用layoutWidth/layoutHeight减padding。SVG先在 `src/pagx/svg/SVGExporter.cpp:4048` 完成layout，Native分支 `src/pagx/svg/SVGExporter.cpp:3678` → `writeTextBoxGroup` → `MakeTextBoxParams`又重新选尺寸。当前 `src/pagx/utils/TextUtils.cpp:515`、`:525` 错误先取author值，`:533`再减padding，覆盖了已解析的更高优先级双边/百分比尺寸。

固定修复规则：

- **布局完成的容器TextBox（elements非空）：** EffectiveTextBoxWidth/Height优先读取该次layout的有效外框尺寸，含合法0，不再先返回author值；MakeTextBoxParams只减自身padding一次。实际优先级双边约束→百分比→author→内容测量，布局阶段已完成，导出不得重新计算一套低优先级尺寸。
- **modifier-only TextBox（elements空）：** 明确author轴可作测量输入，百分比/双边轴必须有真正完成的分配才读取resolved，含合法0；没有任何尺寸来源的轴保持NaN/unbounded。不能把空盒测量的0误作百分比/约束已生效。
- **实际布局入口必须一起修：** `src/pagx/nodes/TextBox.cpp:134` 调用 `CollectLayoutNodes(elements,true)`，而 `src/pagx/LayoutNode.cpp:267` 当前连空的子TextBox也跳过。限定调整这条收集路径：普通Text仍由typesetting负责，空modifier-only子TextBox加入布局列表，先updateSize(context)完成author/首选测量，再PerformConstraintLayout分配对应轴，随后读取参数；不只在helper里换优先级，也不把未测量的NaN首选尺寸传给分配。
- **有效性读取固定为逐轴状态：** 复用LayoutNode现有layoutWidth/layoutHeight产物，增加只读 `resolvedLayoutSize()` 访问方法返回原始Size并保留未分配NaN（不是新增作者字段）。helper只在该轴确有分配时读取，合法0不回退；unbounded modifier轴按输入来源保持NaN。`layoutBounds()`会把未知轴伪装成0，不能单靠它或doc.layoutApplied判子轴已分配。
- helper用于最终输出/运行树的调用前置是对应节点和轴完成有效layout；SVG/PPT入口及LayerBuilder/TextHolder路径共同保证。需要未布局测量的代码留在原测量路径，不调用最终尺寸helper；百分比/双边尚未分配就诊断/先完成布局，不能回退旧author或0。
- 不覆写author width/height，不通过加默认值/强转绕过未知状态；共用 `TextUtils.h/.cpp` 的helper与参数构造一次修正，覆盖 `src/renderer/LayerBuilder.cpp:3634` 的TextHolder及 `src/pagx/ppt/PPTTextWriter.cpp:1102` 等调用方。

整数验收：父框300×200、无parent padding，TextBox author100×60，left=20/right=30/top=10/bottom=20，自身padding四边10：author保持100×60；resolved外框250×170；有效text params为230×150，**不是当前80×40**。删除双边约束、改percentWidth=80/percentHeight=50时，resolved外框240×100、params220×80；只author时外框100×60、params80×40。Horizontal wrap看有效width，Vertical wrap看有效height。再用300×200外层容器TextBox、无outer padding，内部空modifier-only TextBox percentWidth=80/percentHeight=50，断言它确实参与分配且为240×100而不是测量0；双边left=170/right=130分配width=0应保持合法0。空modifier-only且无尺寸来源保持NaN。另测layout重跑与all-lines Hidden空结果，SDK/Native/PPT/TextHolder不得各选一套宽高。

## 3. 动效导出：全通道分类与编译

### 3.1 范围与分类规则

复用第 2 节全部静态契约；根选择、实际 Composition 挂载、Clock、typed apply、最终 DrawRecord 依赖是动画准入边界，不把模板修改后重布局当运行时求值。

| 类别 | 定义 / 准入 |
|---|---|
| N | 原生等价属性 / SMIL，包括合法离散属性；插值、操作链、颜色与宿主结构可证等价 |
| G | 有界属性表达式、几何或 glyph 展开；连续时刻误差可证明，拓扑切换显式离散且满足预算 |
| B | 需要明确 finite-scene 合成 / 位图计划，只能按 D8 + MB；未授权或不满足契约则 Strict 拒绝 |
| U | 无有效 runtime payload / writer、未知 / 非法目标、开放宿主历史或无法界定状态 / 时间 / 资源；明确拒绝 |

下表是**计划准入分类，不是当前支持表**。每通道有 primary 或明确组合条件；N/G 只在等价 / 误差证明、资源和目标状态均成立时准入，复杂效果 / text / geometry fallback 转 B；无法闭合则 U。已证明 inactive、no-op 或 priority-masked 记录为无有效贡献，不伪造动画，也不能漏报其他有效依赖。

### 3.2 全部 175 个反射 Animatable 通道（30 类）

通道名逐项展开；Group / TextBox 分两行，各 10 项。计数范围只含反射 Animatable，不混入资源引用或无 writer 字段。依据均为 `src/pagx/PAGXNodeChannel.cpp` 的对应注册位置。

| NodeType | 数 | 完整 channel 名 | primary / 组合条件与阶段 | 源码依据 |
|---|---:|---|---|---|
| Layer | 20 | `visible, alpha, blendMode, x, y, z, rotation.x, rotation.y, rotation.z, scale.x, scale.y, scale.z, skew.x, skew.y, perspective, matrix, matrix3D, anchor.x, anchor.y, anchor.z` | N/G：visible、alpha、x/y、rotation.z、scale.x/y、skew.x/y、matrix、anchor.x/y；B：z、rotation.x/y、scale.z、perspective、matrix3D、anchor.z，一旦可证整体 affine 可降 N/G；blendMode 在 int payload 匹配且离散合成等价时 N/G，否则合成 B，string / mismatch 为 U，不能强转掩盖；M1—M3 | `src/pagx/PAGXNodeChannel.cpp:486` |
| Group | 10 | `anchor.x, anchor.y, position.x, position.y, rotation, scale.x, scale.y, skew, skewAxis, alpha` | N 操作链；多 easing / alpha / 捕获矩阵组合 G；复杂合成 B；M2/M3 | `src/pagx/PAGXNodeChannel.cpp:684` |
| TextBox | 10 | `anchor.x, anchor.y, position.x, position.y, rotation, scale.x, scale.y, skew, skewAxis, alpha` | 同 Group 的 N/G 条件，但两布局 / 发射模式均须有效；复杂动态文本 B；M2/M3/M5 | `src/pagx/PAGXNodeChannel.cpp:713` |
| Rectangle | 5 | `position.x, position.y, size.width, size.height, roundness` | N 参数；原轮廓 / align / dash / modifier 组合 G，拓扑无法闭合 B；M2/M3/M5 | `src/pagx/PAGXNodeChannel.cpp:530` |
| Ellipse | 4 | `position.x, position.y, size.width, size.height` | N 参数，不按静态正圆锁元素；退化 / 描边组合 G，否则 B；M2/M3/M5 | `src/pagx/PAGXNodeChannel.cpp:543` |
| Polystar | 8 | `position.x, position.y, pointCount, outerRadius, innerRadius, rotation, outerRoundness, innerRoundness` | G 最终 geometry；pointCount 拓扑切换按离散与预算，不能闭合 B；M5 | `src/pagx/PAGXNodeChannel.cpp:555` |
| Text | 9 | `text, x, y, fontFamily, fontStyle, fontSize, letterSpacing, fauxBold, fauxItalic` | G TextHolder + flush / reshape 后最终 glyph；x/y 可证单纯位移时 N；string/bool 离散，尺寸等连续需区间证明；复杂布局 / 字体切换 B，未供应资源 U；M5 | `src/pagx/PAGXNodeChannel.cpp:582` |
| Fill | 2 | `color, alpha` | N 单属性；scope / color alpha 乘积、P3 / 多 paint G，否则 B；M2—M4 | `src/pagx/PAGXNodeChannel.cpp:604` |
| Stroke | 5 | `color, width, alpha, miterLimit, dashOffset` | N 属性；align / dash / fit / glyph overlay 组合 G，不能等价 B；M2—M5 | `src/pagx/PAGXNodeChannel.cpp:614` |
| TrimPath | 3 | `start, end, offset` | G 实际 Separate / Continuous geometry，跳变 / topology 显式预算，否则 B；M5 | `src/pagx/PAGXNodeChannel.cpp:630` |
| RoundCorner | 1 | `radius` | G 最终 corner geometry，含 Text→Shape / no-op；无法闭合 B；M5 | `src/pagx/PAGXNodeChannel.cpp:639` |
| Repeater | 11 | `copies, offset, anchor.x, anchor.y, position.x, position.y, rotation, scale.x, scale.y, startAlpha, endAlpha` | G 有界副本 / transform / fractional alpha；copies 变化显式离散拓扑；无效幂 U、复杂合成 B；M5 | `src/pagx/PAGXNodeChannel.cpp:726` |
| TextModifier | 13 | `anchor.x, anchor.y, position.x, position.y, rotation, scale.x, scale.y, skew, skewAxis, alpha, fillColor, strokeColor, strokeWidth` | G 最终 glyph 矩阵 / signed factor / base+overlay，复杂 paint / effect B；M5 | `src/pagx/PAGXNodeChannel.cpp:651` |
| RangeSelector | 6 | `start, end, offset, easeIn, easeOut, weight` | G 实际 glyph factor 与活跃 shape / mode，inactive 登记无贡献；无法闭合 B；M5 | `src/pagx/PAGXNodeChannel.cpp:743` |
| SolidColor | 1 | `color` | N 颜色映射；P3 / alpha / 共享引用表达式 G，否则 B；M4 | `src/pagx/PAGXNodeChannel.cpp:759` |
| LinearGradient | 4 | `startPoint.x, startPoint.y, endPoint.x, endPoint.y` | N 端点；matrix / fit / 动态 bounds 与退化规则 G，否则 B；M4 | `src/pagx/PAGXNodeChannel.cpp:765` |
| RadialGradient | 3 | `center.x, center.y, radius` | N 参数；fit / 退化 / bounds G，负 radius 无效 U；不能闭合 B；M4 | `src/pagx/PAGXNodeChannel.cpp:775` |
| ConicGradient | 4 | `center.x, center.y, startAngle, endAngle` | B sweep 合成；全区间可证退化常量才 N/G；M4 分类、MB 可选 | `src/pagx/PAGXNodeChannel.cpp:784` |
| DiamondGradient | 3 | `center.x, center.y, radius` | B diamond 合成；全区间退化常量 N/G，负 radius U；M4 分类、MB 可选 | `src/pagx/PAGXNodeChannel.cpp:794` |
| ColorStop | 2 | `offset, color` | N stop；重排 / 退化 / mixed space / alpha G，否则 B；M4 | `src/pagx/PAGXNodeChannel.cpp:803` |
| DropShadowStyle | 7 | `offsetX, offsetY, blurX, blurY, spread, color, showBehindLayer` | N/G 原生等价 Below 链、离散 behind 与有界 contour / spread；完整输入不能等价 B；M4 | `src/pagx/PAGXNodeChannel.cpp:820` |
| InnerShadowStyle | 6 | `offsetX, offsetY, blurX, blurY, spread, color` | N/G Above 链与有界 spread / contour；复杂来源 B；M4 | `src/pagx/PAGXNodeChannel.cpp:834` |
| BackgroundBlurStyle | 2 | `blurX, blurY` | B 动态 backdrop / draw 前缀；全区间双零 no-op 登记；M4 分类、MB 可选 | `src/pagx/PAGXNodeChannel.cpp:847` |
| GlassStyle | 7 | `refraction, depth, frost, dispersion, splay, lightAngle, lightIntensity` | B 背景、own geometry、SDF/UDF 与光照完整合成；M4 分类、MB 可选 | `src/pagx/PAGXNodeChannel.cpp:857` |
| NoiseStyle | 7 | `size, density, seed, color, firstColor, secondColor, opacity` | B 活跃 noise 算法 / mode / blend；Mono color、Duo 两色、Multi opacity 之外为无 writer 的 U/无贡献；零 density no-op；M4 分类、MB 可选 | `src/pagx/PAGXNodeChannel.cpp:876` |
| BlurFilter | 2 | `blurX, blurY` | N/G 双轴 / tile / region 可证等价；不同采样或复杂链 B；M4 | `src/pagx/PAGXNodeChannel.cpp:891` |
| DropShadowFilter | 6 | `offsetX, offsetY, blurX, blurY, color, shadowOnly` | N/G 上一步完整输入与离散 Only；非等价链 / 色彩 B；M4 | `src/pagx/PAGXNodeChannel.cpp:899` |
| InnerShadowFilter | 6 | `offsetX, offsetY, blurX, blurY, color, shadowOnly` | N/G 内侧遮罩、完整 output 与离散 Only；复杂合成 B；M4 | `src/pagx/PAGXNodeChannel.cpp:910` |
| BlendFilter | 1 | `color` | N/G 真正 ColorFilter source/dest/alpha 与固定 blendMode；Plus / 非等价合成 B；M4 | `src/pagx/PAGXNodeChannel.cpp:921` |
| NoiseFilter | 7 | `size, density, seed, color, firstColor, secondColor, opacity` | B 活跃三 mode / input offset / phase；inactive U/无贡献与 density0 input no-op 分开；M4 分类、MB 可选 | `src/pagx/PAGXNodeChannel.cpp:934` |

合计175，其中styles/filters完整51项。原重组基点白名单为7类/39项；本次 `a7c6c9f23` 已增加BlurFilter、四种阴影filter/style的offset/blur及BlendFilter.color，`IsSupportedChannel`共14类/59项（2026-11-11 增加 SolidColor.color）（`src/pagx/svg/SVGAnimationPlan.cpp:67`）。这是当前接线集合，不是175项全支持或组合保真证明；颜色源、stop、d及文字展开按本节M4/M5子步骤同步白名单和 `SVGCoverageLedger`。

### 3.3 表外通道、类型与目标准入

| 表外 / 特殊路径 | 实际事实 | 处理与直接验证 |
|---|---|---|
| Path `point{N}.x / point{N}.y` | PathRuntimeTarget 动态 writer，unscaled 点 + renderScale；nonnull data 才安装 | G 原始点求值 → layout / fit / modifier 后 geometry；校验 N<size、数字溢出、verb/data 长度；`src/renderer/LayerBuilder.cpp:505`、`src/renderer/LayerBuilder.cpp:1728` |
| ImagePattern.image | writer 要 `shared_ptr<PAGImage>`，XML key 是 TypedChannel<ImageRef>，apply 直传不 resolve | 当前 U，不能列为已支持换图；有限 host 换图须确认 payload / 资源 / 时序并从源头闭合，才可 G 离散 image 或 B；`src/renderer/LayerBuilder.cpp:1939`、`src/pagx/PAGXImporter.cpp:1933`、`src/pagx/PAGAnimation.cpp:44` |
| 无 writer / 非 Anim 字段 | Path.position 只有 Layout；Text anchor/baseline、gradient matrix、tile / mode、20 项 ColorMatrix 等是静态语义，不因存在字段就可动画 | 保留附录静态语义；effective channel 无 writer 为 U，已证 inactive/no-op 单独记录；`src/pagx/PAGXNodeChannel.cpp:572`、`src/pagx/PAGXNodeChannel.cpp:582` |
| Layer.blendMode / 数字类型 | runtime writer get_if<int>；反射 FIELD_ENUM 是 string，parser enum 可为 int；未定类型 integer XML 优先 int，期望 float 的 writer 不写 | XML→evaluate→实际 apply 三段准入；源头 typed schema 修复或拒绝，不靠强转 / 默认值掩盖；`src/renderer/LayerBuilder.cpp:1239`、`src/pagx/PAGXNodeChannel.cpp:402`、`src/pagx/PAGXImporter.cpp:1906`、`src/pagx/PAGXImporter.cpp:1925` |

TypedChannel 实际有 float / bool / int / string / ImageRef / Color / Matrix / Matrix3D；KeyValue 另含 PAGImage 宿主值，二者不可混称。未知 channel / target、跨 binding 作用域、非法非有限 payload 按 U 拒绝；不把运行时无写入后的模板静止画面标为动画成功。

查找必须使用实例所属 contextDoc，再校验 binding.contains（`src/pagx/PAGAnimation.cpp:101`、`src/pagx/PAGAnimation.cpp:108`）。普通 Layer 没有 Composition 不 spawn timeline；兄弟实例独立，externalDoc 归属见 `src/pagx/runtime/PAGComposition.cpp:113`、`src/pagx/runtime/PAGComposition.cpp:130`。

### 3.4 Clock、Layer 三层 priority 与 ordered apply

```text
DriverKey = (instancePath, timelineOrdinal)
PausedScopeKey = (compositionInstance, animationId)
Binding = (targetInstance, typedChannel, driver, applicationOrder, mix)
Clock = (durationUs, frameRate, loopMode, evaluationOffsetUs, initialElapsedUs)
EvaluateAtExportTime:
  initialize resources and the confirmed state source
  if host initialization is selected, link all VM instances before binding
  apply selected root animations in declaration order
  advance and apply mounted nested drivers with the runtime pause scope
  map content time with PAGAnimation.apply and preserve typed mix / shared pose
  flush effective bindings and text only when the state contract requires it
  draw through the shared context
```

- DriverKey 区分 driver，不代表 playing 独立：同 Composition 以 animationId 共享 pausedTimelineIds，任一同 ID driver 初始 paused 会暂停同 slot 的全部同 ID advance；apply 仍全部执行。兄弟 slot 独立（`src/pagx/runtime/PAGComposition.cpp:57`、`src/pagx/runtime/PAGComposition.cpp:69`、`src/pagx/runtime/PAGComposition.cpp:78`、`src/pagx/runtime/PAGComposition.cpp:130`）。
- content 求值与 playbackPosition 报告分离：Loop 在 t=D 内容回 0，但 progress 可报告 D；Once clamp，PingPong 唯一周期 2D。offset 使用实际 apply 的 fold / subtract 顺序，不能把 progress 或 begin 当内容时间（`src/pagx/PAGAnimation.cpp:182`、`src/pagx/PAGAnimation.cpp:199`、`src/pagx/PAGAnimation.cpp:274`）。
- 暂停只禁 advance，不禁 apply；初始值包含 offset / paused / delay 前的真实求值，不能用永不开始的 SMIL 或模板值代替。
- 无效 duration / frameRate、period 乘法 / offset 溢出、非有限时间须在边界判定；概念伪代码不冒充数值安全实现。

| Layer priority 层 | 真实规则与依据 |
|---|---|
| 静态初 pose | 非恒等 matrix3D 覆 matrix / components；非恒等 matrix 覆 x/y 与组件，但保留 z；anchor 在选择后作用，layout 位移在外部；`src/pagx/utils/LayerTransformUtils.cpp:93` |
| 同一 Animation | 先 merge 同 target 全部 Object 的 channels，再按“通道存在”过滤：matrix3D 屏 matrix、x/y/z 与 components；matrix 屏 x/y 与 components，不屏 anchor 或 z；`src/pagx/PAGAnimation.cpp:59`、`src/pagx/PAGAnimation.cpp:111` |
| 跨 Animation / writer | 静态 masked 状态与共享 Layer pose，后续 component write 可能重新 decompose 当前 animated matrix；保留 writer / mix / apply 顺序，不只 last-wins 清 channel；`src/renderer/LayerBuilder.cpp:192`、`src/renderer/LayerBuilder.cpp:267` |

未选择根定义和未挂载动画不激活。根集合以D1为准：Default首timeline、Named指定顶层Animation、All仅顶层Animation声明序；根先apply，再nested递归advance/apply，最后按已选宿主状态flush绑定/文字。当前render CLI已遍历顶层Animation并驱动nested（`src/cli/CommandRender.cpp:286`、`src/cli/CommandRender.cpp:297`）；它不是Default/Named或有限事件重放的直接参考。getDefaultTimeline见 `src/pagx/PAGScene.cpp:444`，Plan入口/去重问题仍需修复。

### 3.5 原生、组合采样与关键帧边界

```text
CompiledTrack:
  attributeOrTransformOperation, initialValue, sharedClock
  orderedSegments, interpolationMode, approximationStatus
Compile:
  evaluate the full dependency expression
  choose exact native, bounded G, explicit B, or reject U
  validate serialized reconstruction and budgets
  let Writer serialize only validated tracks
```

1. 单属性插值与 SVG 等价才 N；Group 尽量保持操作链、anchor、skewAxis 与标量 rotation，不先合成矩阵丢圈数。标量 0→360 / 720 保 full turns，Matrix 170→190 复用分解 shortestArc，逐段保持角度连续。
2. 多 easing、alpha 乘积、动态 fit / bounds、非线性 effect 参数求完整表达式，不把端点值线性连接。复用 Channel.evaluateAt、Matrix 分解、颜色空间与 modifier，不复制求值器。
3. G 种子含首尾、全部依赖 key、跳变两侧、相位边界；比较序列化后轨道重建，包含数值格式化误差。连续区间需区间 / 导数误差界等证明；**单中点或有限探点不证明全区间**。无法证明或超预算则 Strict 拒绝。
4. Hold / None 的真实离散行为直接复现；No Hold epsilon 假渐变。连续与离散段明确拆开，端点 / 镜像 / 单 key 共用编译入口。
5. 独立属性保持独立 Clock；跨 Clock 表达式优先结构分解，公周期必须有界。无有界公周期的完整合成只在明确 finite window 的 B / MB 下计划，不无界 LCM 展开。
6. 首尾截取必须真求值：keys `[-10,0]`、`[10,100]` 在 t=0 为 50，不能先 clamp keytime 成 0 改值；现 Writer 问题见 `src/pagx/svg/SVGAnimationWriter.cpp:129`。
7. 空 Channel.evaluate 得 T{} 与“无 channel、保模板”不同；空 / 单 key、1-frame、duration=1、duration=0、跨 duration key、duplicate ordered key、非有限值全部直接验证（`src/pagx/runtime/KeyframeEvaluatorImpl.h:106`）。
8. Bezier 可 overshoot：float 不 clamp easing factor，Color 按分量 clamp；不能统一 clamp 得到看似安全但不同结果（`src/pagx/runtime/BezierEasing.cpp:104`、`src/pagx/utils/ColorSpaceUtils.cpp:78`）。

### 3.6 几何、文字、拓扑与 fallback

- dependency closure 覆盖 geometry、Group、Painter、color source、modifier、Mask、Composition、effect 与引用资源；没有动态依赖的复杂效果才继续静态 bake。
- ModifierResolver 产物保留源 geometry、作用顺序、Painter lifetime、copy 序号和依赖；静态 Trim / Round 作用动态 Path 后仍是动态，局部采样不增长共享 nodes。
- `d` 命令类型 / 数量稳定只是必要条件；还需连续形状与误差证明。Trim 断点、pointCount、copies、glyph 数 / 字体切换显式离散拓扑与预算，不一律声称“不可能”，也不偷偷连续 morph。
- Text 使用实际 TextHolder payload、flush / reshape、Selector glyph index 与最终 base / overlay paint；动态 path points 使用未缩放坐标再走 renderScale。复杂重排版 / effect / 拓扑无 G 证明则 B，未供应字体、无有效 payload 或开放历史 U。
- N/G 需包含整个 effect 链的输入 / outputOffset / region / 全时段 bounds；spread 不默认等于 morphology，非线性倒数不能只换端点。M4 实际扩可原生 effect 参数，不用“后续支持滤镜”代替 51 项分类。
- Strict 不靠单 PNG freeze；BestEffort 冻结必须说明实例、依赖范围、原点真实求值时刻和动画损失，且仍满足 staticPolicy。类型 / 状态 / 预算未满足不允许静默降级。

### 3.7 可选 MB：显式 finite-scene bake（M5 后、M6 前）

B 只提供可审查的编译计划，不默认运行；D8 未同意时 Strict 拒绝。它是有限分辨率 / 时间的位图输出模式，不是原生 vector，不是当前已实现保真。

| 必须明确的输入 | MB 契约 |
|---|---|
| 时间与初态 | 有限 [start,end]、各实例初始 elapsed / playing / offset、D7 或明确初始化快照；跨无公周期只对给定窗口编译 |
| 事件与宿主 | 有限、排序、可重放的 SM / VM / trigger / Once 事件和初始化顺序；任意未来输入 / 开放历史仍 U |
| 资源与环境 | 所属文档、图片 / 字体供应、画布 / 背景 / 色彩、固定分辨率与 D6，资源集合和回调执行边界闭合 |
| 采样与误差 | 采样时刻、离散显示约定、运动 / 像素误差检查、帧数 / 像素 / 字节预算；不以帧端点吻合承诺任意连续时刻 |
| tail / repeat | 尾帧保持或结束、窗口 repeat 是否允许及接缝语义；窗口重复不等同原无限多 Clock 场景 |
| 合成与输出 | 完整 3D / backdrop / Glass / noise、复杂 shape / text / effects fallback；复用 BakeConsumption，不重复消费背景或状态 |
| 失败 | 缺初态 / 事件 / 资源、未知 payload、无法满足约定或预算不足 → 拒绝；报告 B/MB、raster 与时间限制 |

**拟议帧序列表示：** 对已编译的完整合成区域，在同一固定逻辑范围内为每个采样帧输出嵌入PNG的image；初始各帧hidden，使用SMIL离散visibility轨道在相邻采样时刻之间恰好显示一个帧。采用[sample[i],sample[i+1])区间，透明帧保持透明且不让前一帧透出；有限窗口之前/之后分别按明确初态与tail契约显示。repeat只复现已确认窗口，不等同源场景的无限多时钟；全部image共用BakeConsumption且不重新施加已烘焙alpha/mask/transform。

该表示的阶梯时间误差按D8另行接受或证明，不以采样帧像素吻合承诺任意连续时刻相等；不满足则拒绝。验证seek到每个采样点、区间内部、透明帧、尾帧和repeat接缝，确认不会两帧同时可见或留下旧内容。D8授权仍不能突破VectorOnly禁新增效果位图；该策略下B/MB拒绝，不能因有限场景开关自动改静态策略。

MB范围包含3D、conic/diamond、noise、backdrop/Glass、复杂shape/text/effects fallback及无有界公周期的多Clock；资源/状态不能有限化则U。有限host换图须先闭合第3.3节runtime payload，不能由Exporter独自假设成功。该帧序列表示仍为待确认设计，不是当前单image写出已具备的能力。

### 3.8 动效实施 M0—M6

#### M0：动态 ledger 与同源复现

- 文件：`test/src/PAGXSVGTest.cpp`、`test/src/PAGXTest.cpp`、`tools/html-snapshot`、`CMakeLists.txt`。
- 任务：全 175 / 表外通道分类、typed XML→apply 与 F1—F18、附录 D.2 动态组合；复用静态 XML / 浏览器入口，建立真实实例 runtime oracle。
- 验收：初态 / 中间 / 跳变 / 第二周期可击穿旧问题；不靠 animate 字符串、空图或只 top render CLI 判 nested 正确。

#### M1：实例、选择、pause scope 与 Clock

- 文件：`src/pagx/svg/SVGAnimationPlan.{h,cpp}`、`src/pagx/svg/SVGAnimationWriter.{h,cpp}`、`src/pagx/PAGAnimation.cpp`、必要内部共享时间函数、`test/src/PAGXSVGTest.cpp`、`test/src/PAGXTest.cpp`。
- 任务：根入口 / 去重、Composition 挂载、contextDoc / contains、DriverKey 与 animationId pause scope、ordered bindings；复用 content time / offset / 初态与唯一 PingPong 周期；类型准入先行。
- 验收：F1—F4 / F13—F16；官方样例、仅 Composition、双 slot / externalDoc、同 ID paused、不同 fps / 正负 offset / Loop t=D 与 progress 分离；ordinary Layer 不错误激活。

#### M2：绘制作用域与变换

- 文件：`src/pagx/svg/SVGExporter.cpp`、`src/pagx/svg/SVGAnimationWriter.{h,cpp}`、`src/pagx/utils/ExporterUtils.{h,cpp}`、`test/src/PAGXSVGTest.cpp`。
- 任务：复用 DrawRecord / LayerTransformUtils；三层 priority、shared pose / decompose / mix 顺序；位置 override、full turns / shortestArc、alpha 因子 / groupOpacity；Mask / Contour / TextBox / 各 glyph 分支与动态 ellipse。
- 验收：F5—F6 / F8—F11 / F14；零起 alpha、捕获前后 singular、重叠内外 Painter、matrix 与 component 跨动画顺序、所有文字 / Mask 宿主；共享静态不回归。

#### M3：统一轨道与证明预算

- 文件：`src/pagx/svg/SVGAnimationWriter.{h,cpp}`、必要时才拆的内部编译单元、`test/src/PAGXSVGTest.cpp`、共用 SVG 评测入口、`CMakeLists.txt`。
- 任务：统一初态 / 首尾真求值、空 / 单 / duplicate key、Hold / None / Bezier overshoot；完整表达式、序列化重建与连续误差证明；多 Clock 有界分解、采样 / 递归 / 输出预算。
- 验收：F7 / F15 / F18；跨 duration t=0=50、float 与 Color clamp 分歧、混合 easing / 跳变、预算和无法证明时确定拒绝，无 epsilon 假 Hold。

#### M4：颜色源、渐变与可原生 effects

- 文件：`src/pagx/svg/SVGAnimationPlan.cpp`、`src/pagx/svg/SVGCoverageLedger.{h,cpp}`、`src/pagx/svg/SVGAnimationWriter.{h,cpp}`、`src/pagx/svg/SVGExporter.cpp`、`src/renderer/LayerBuilder.{h,cpp}`、`test/src/PAGXSVGTest.cpp`、`test/src/PAGXTest.cpp`。
- 任务：按下面M4.0—M4.3顺序落实源/消费者与stop/defs实例映射，再做SolidColor、stop、linear/radial参数及联合fit；LerpColor/P3/alpha；可等价effects作为M4.E支路，逐步输入、spread/behind/Only/region全时段验证；其余effect全部B/U判定。每次放行同步Plan、ledger和真实输出门禁，不将Planned当Supported。
- 验收：175 表中本阶段每项直接求值 / 像素，完整 51 effect 项不漏；Noise inactive / no-op 与 B 分开，conic / diamond 退化条件覆盖，不用“后续”冒充分类。

#### M5：动态 geometry、文字与 Selector

- 文件：`src/pagx/utils/ModifierResolver.{h,cpp}`、`src/pagx/svg/SVGAnimationPlan.{h,cpp}`、`src/pagx/svg/SVGCoverageLedger.{h,cpp}`、`src/pagx/svg/SVGAnimationWriter.{h,cpp}`、`src/pagx/svg/SVGExporter.cpp`、`src/renderer/LayerBuilder.cpp`、`src/renderer/TextHolder.cpp`、`src/pagx/TextLayout.cpp`、`third_party/tgfx/include/tgfx/layers/vectors/Text.h`、`third_party/tgfx/src/layers/vectors/Text.cpp`、`test/src/PAGXSVGTest.cpp`、`test/src/PAGXTest.cpp`、共用评测入口。
- 任务：按M5.0—M5.4，先共用局部最终产物，稳定d、Text.x/y、七shaping通道，再联合动态bounds/颜色/stop/modifier/拓扑；Path point、Polystar/Trim/Round/Repeater来源与求值，ST21最终paint及ST22坐标标准按第2.8节；有限host换图先源头准入或拒绝，不跳过后排组合。
- 验收：F11 / F13 / F17；静态 modifier 不冻动态 geometry，变化 copy / glyph 数可审计，Text / fonts / faux / overlay 与路径 renderScale 正确；不污染 Font / nodes。

#### M4/M5：固定展开顺序、前置依赖与台账准入

本表固定四类展开的执行次序，不需要实施时再询问。当前已存在 `src/pagx/svg/SVGCoverageLedger.h:32` 的Supported/Planned/NotExpressible实现状态和 `src/pagx/svg/SVGAnimationPlan.cpp:67` 的白名单；它们不等同正文N/G/B/U能力分类。**当前白名单为17类/76项（含 SolidColor.color、ColorStop.offset/color、linear 四端点、radial 中心/半径、Text.x/y、Polystar 固定拓扑六通道、NoiseFilter.seed）**，Path point 族按表外通道单列，其余 Text shaping 与动态 modifier 尚未放行；不按旧7类/39项覆盖用户已有滤镜增量。未发现已实现的ModifierDependencyGraph，本方案只扩现有resolver产物/DrawRecord来源，不能引用不存在的图模块当作前置。

共同前置Q：S1的实例/引用/Painter capture与宿主，S3的最终glyph及第2.8节ST21/ST22/ST28，S4的modifier顺序/来源，S5的静态ColorSource/defs/fit；M1的typed/contextDoc/Clock和M2的ordered pose/alpha；M3至少具备本子批次所需的关键帧、Hold、区间求值、序列化和预算门禁。**不要求先做完所有M3高级组合再做独立N轨道，但某条G轨道缺其误差/预算能力时必须保持Planned或明确拒绝，不能先放行再补证明。**

| 固定子步骤 / 优先级 | 内容与硬依赖 | 可并行/可后排边界 | 本步骤退出门禁 |
|---|---|---|---|
| M4.0，必须最先 | 建立source/stop→全部实际runtime消费者→各defs实例的映射；先修下述runtime资源绑定扇出，只最后一个consumer被写入的现状不能作为正确oracle；Planned依赖也不能单PNG冻结 | 映射供source/stop/gradient共用，不等待d或动态文字；基于现有colorSourceUsers与局部bindings扩展，跨Composition实例不共享写入 | 同源多Fill/Stroke、共享stop跨gradient/index、多defs和双实例；实际runtime全部consumer更新且mix一次，无仅SVG正确而oracle仍漏写 |
| M4.1，第一类先做 | `SolidColor.color`：源binding传播到各实际消费点，Painter/color/scope alpha按第2.8.1完整求值；依赖M4.0 | 和M4.2实现逻辑独立，可并行；连续实施采用先M4.1再M4.2。M5组合不作为静态几何/固定glyph颜色的硬前置 | 共享SolidColor多消费者、源与Painter同有绑定的真实顺序、透明/P3/外部实例、颜色与alpha同时变化 |
| M4.2，与第一类同级必须做 | `ColorStop.offset/color`两通道：在正确stop宿主写offset、stop-color、stop-opacity；依赖M4.0、静态Gradient规范化和Typed颜色求值 | **不依赖SolidColor输出，也不依赖linear/radial端点动画**，现有静态gradient足以承载。不能把conic/diamond的radial降级宿主算作原生stop保真 | linear/radial静态几何+动画stop，重复/边界/重排及混色/alpha、同stop多个defs实例；conic/diamond消费者正确B/拒绝分类 |
| M4.3，随后完成颜色类 | linear四端点分量、radial中心/半径三分量与fit/退化；依赖M4.0和实际paint-fit表达式 | 可与M4.1/M4.2开发并行，但合并后必须联合测试；动态bounds组合在M5后补最终门禁，不能一开始宣称所有组合Supported | 端点/中心/radius与stop一起求值、零/负/单stop退化、P3/alpha、共享defs/实例。完成颜色/stop独立子集后才进入后续展开 |
| M5.0，d与文字共用基础必须先做 | 导出局部按时刻求值→真实modifier/最终glyph-style→有效Painter几何；保留source/copy/capture/局部matrix/paint分支；依赖Q，不修改共享模板 | 可在颜色子批次同时建设；**不新建一套文字几何求值器**，d和文字复用同一最终产物。ST21覆盖不要求给GlyphRun新增paint字段 | 局部产物稳定、模板无污染、静态modifier作用动态输入、被modifier替换后的绑定仍可定位 |
| M5.1，第三类先完成稳定d | Path `point{N}.x/y`、固定拓扑Polystar/Trim/Round最终路径及d序列化；依赖M5.0及M3本轨道插值/误差/预算 | 固定拓扑d并不在算法上依赖动画stop，但按本次连续顺序在M4.3后交付。动态glyph或变副本/点数可以后排，**不能省掉** | point索引/原始空间/renderScale、verb结构与中间帧、Mask+modifier、原生SVG d浏览器支持；表外point族独立登记而不伪造反射175条 |
| M5.2，第四类先做位置 | Text.x/y的runtime写入不触发reshape，但最终发射需继续经过TextPath/modifier；无沿线依赖且可证整体平移才N，normal沿曲线重新求值为G，force忽略旧位置的该消费者为无贡献 | 可早于M5.1/并行，不等待七shaping；不能因writer直接改position就把所有最终输出判N。同Text其他有效消费者仍分别判断 | 固定glyph各出口、层/Group/TextBox/Mask，另对normal曲线和force无贡献测试；不重复layout/padding，不全局丢该Text的x/y轨道 |
| M5.3，第四类随后做塑形 | Text七shaping通道经TextHolder的强制实际塑形、有效空清除和anchor保留入口（下文固定）；依赖M5.0、ST28及字体，不能以当前null blob continue视为成功 | 和d序列化可并行；string/bool可预编译有限glyph状态。Text→Shape参与Trim/Round/Merge的联合输出等待M5.1；未闭合的有效塑形分支拒绝而非显示旧glyph | typed/字体/尺寸/字距/faux、all-embedded转新文字、文本变空/Hidden全丢、custom anchors、bounds/glyph切换；ST21/ST22随新glyph求值 |
| M5.4，最后完成跨类与结构变化 | 颜色/stop+动态bounds+d+重塑形、TextModifier/RangeSelector、Repeater变副本/点数与拓扑；依赖M4.1—M4.3、M5.1—M5.3及所需M3 G能力 | 可后排到基本子集之后，**全量完成不可跳过**；无法G等价的组合落B且D8禁用时拒绝，不靠静态freeze | 动态fit/region、同属性多Clock、逐glyphbase/overlay、离散拓扑、连续误差和预算，A12—A20直接验证 |

**M4.0 的源端入口写定：** `src/renderer/LayerBuilder.cpp:1983` 每次convertColorSource产生独立对象，`:2103`重复建立stop/index绑定，而 `src/renderer/LayerBuilder.h:133` 的set只保留同node的最后对象。沿用每个Painter自己的runtime对象，不通过无条件共享对象改变Fill.color的局部写入语义；对SolidColor/Gradient/ColorStop资源channel在同一RuntimeBinding实例内建立实际消费者扇出，复用colorSourceUsers索引并记录stop对应各gradient/index。一次资源apply按writer顺序更新每个去重后的实际对象，mix不得对同一对象重复应用；Painter.color仍只写自身对象，两个slot互不写串。源码、读回/解绑/重建和SVG消费者映射一并修，不能只让SVG全部变色而reference只变最后一个。读回只有消费者值一致时才能代表一个资源值，若已因局部Painter写入分歧，不能任取last供TwoWay；该分歧按所选绑定契约明确诊断/拒绝。新增runtime与SVG联合测试后才放行M4.1—M4.3。

**M5.3 的源端入口写定：** `src/renderer/TextHolder.cpp:157` 当前flush仍用默认embedded fast path，`src/pagx/TextLayout.cpp:1745` 在全部带GlyphRun时只给bounds；null blob continue又保留旧内容，单参数setTextBlob会清掉原anchors。实际七shaping覆盖时，调用已有 `TextLayout::Layout(elements,params,&context,false)` 强制从有效TextGlyphParams塑形，不修改作者glyphRuns。分类非空成功、有效空结果、资源/塑形失败；有效空（text为空或Hidden全丢）新增明确的tgfx::Text::clearTextBlob()并由TextHolder调用，保持Text对象和position、清空blob/anchors且invalidate，不调用当前会no-op的setTextBlob(nullptr)；失败分支按策略拒绝，不能用旧glyph假成功。非空成功以当前显式anchors按glyph序保留，按新glyph数量截断/补零，再调用setTextBlob(blob,anchors)，默认anchor由新font advance/2求值；不要盲目单参数清空。同步 `third_party/tgfx/include/tgfx/layers/vectors/Text.h`、其Text.cpp、TextHolder与相关测试；该方法是实际调用的空状态入口，不新增PAGX glyph paint字段。验证all-embedded旧字→新字、非空→空→非空、Hidden有效空、不同字体/数量及anchor结合ST21/ST22。clearTextBlob须在对应tgfx源码中实现并重编/更新实际链接依赖，不能只改缓存头文件或靠默认空blob绕过链接/行为错误；源码级验证沿用项目TGFX_DIR路径。入口未完成时对应shaping条目保持Planned/拒绝。

**tgfx 交付方式（2026-11-11 定）：** `third_party/tgfx` 是本仓库忽略的独立依赖。本批在本地源码实现 `clearTextBlob()` 并用 `-DTGFX_DIR=third_party/tgfx` 编译验证；libpag 仓库只提交 TextHolder / 测试侧改动，tgfx 改动另由 tgfx 仓库提交并回填依赖版本。若上游未同步，M5.3 的对应验收保持未完成，不能用 setTextBlob(nullptr) 冒充。

现有滤镜参数增量作为M4.E独立支路保留并回归；新增效果参数依赖S2的有序effect图和M3轨道，不能阻塞无effect的颜色/stop/d/文字基本子集，也不能用“已有P1”证明全部51项效果通道已支持。D8已选禁用，B类保留明确拒绝；MB不会因上述四类展开而自动启用。

连续执行顺序固定为：`Q → M4.0 → M4.1 → M4.2 → M4.3 → M5.0 → M5.1 → M5.2 → M5.3 → M5.4 → M6`。可并行列只说明真实依赖，不要求实施期间反复征求选择；它不是减少范围的理由。

**白名单与SVGCoverageLedger的同步规则（每个子步骤都执行）：**

1. 先完成Planned条目的binding、真实输出宿主和该支持子集的测试，不提前把Planned改Supported。`SVGCoverageLedger`的note准确说明支持组合/限制；当前“text/font是bake only”“stop的参数可动”不能代替已经存在的输出。
2. 某子集通过typed XML→Channel求值→ordered apply→最终glyph/paint/geometry→SVG重建、确定性browser seek后，在同一次变更中加入Plan白名单并升级相应ledger条目；验证一个节点类型的一条通道不自动放行其全部通道。
3. 增加Plan支持集合与ledger Supported集合的双向检查，再验证真实输出和条件拒绝：无误放行、无Supported而Writer丢弃、无Plan支持却ledger漏项。当前 `test/src/PAGXSVGTest.cpp:5084` 仅验证反射表与175/30账本双向完整，**不验证白名单/输出/像素**，必须补这层门禁。
4. SourceColor/stop直接目标、共享消费者和defs实例都纳入依赖判定；Gradient参数动态时不可继续用静态bounds。类型合法但某种复杂组合未实现，应在该实例能力判断中拒绝/报告，不靠类型级Supported掩盖。
5. 保持反射台账30类/175条；Path `point{N}.x/y`是表外可变族，单列模式、索引和输出门禁，不硬塞固定条目改变计数；ImageRef/PAGImage未闭合仍不得宣传换图成功。
6. 每个升级的测试含原点、关键点左右、区间内部、终态、Loop/PingPong第二周期及暂停/offset；每个拒绝项有确定诊断/非成功冻结的断言。现有已支持通道不能因为新类型加入白名单而改变其Clock/alpha/静态输出。

这些是执行契约，不是本轮实现。当前映射/输出入口依据为 `src/pagx/svg/SVGAnimationWriter.cpp:1500`、`src/pagx/svg/SVGExporter.cpp:704`、`src/renderer/LayerBuilder.cpp:617`、`src/renderer/TextHolder.cpp:157`；ModifierResolver当前只烘焙几何通道TextModifier，不能视为ST21或动态d/重塑形已经完成。

#### MB：仅 D8 同意后的可选阶段

- 文件：`src/pagx/svg/SVGAnimationPlan.{h,cpp}`、`src/pagx/svg/SVGAnimationWriter.{h,cpp}`、`src/pagx/utils/RasterUtils.{h,cpp}`、`src/pagx/svg/SVGExporter.cpp`、共用评测入口与相关测试。
- 任务 / 验收：严格按第 3.7 节有限编译计划；F18 和 D.2 的有限 / 无界对照、首尾 / repeat / 事件 / 完整背景 / 预算直接验证。未授权不执行，也不阻止 N/G 阶段独立交付。

#### M6：公开契约与动效交付

- 文件：`include/pagx/SVGExporter.h`、`src/pagx/svg/SVGAnimationPlan.{h,cpp}`、`src/pagx/svg/SVGAnimationWriter.{h,cpp}`、`src/pagx/svg/SVGExporter.cpp`、`src/cli/CommandExport.cpp`、所有调用方、`test/src/PAGXCliTest.cpp`、`test/src/PAGXSVGTest.cpp`、`CMakeLists.txt`。
- 任务：落实已确认 D1—D8、选择 / Strict / BestEffort / 可选 MB、diagnostics / 文件事务；完整 classification 与真实支持状态分别公布；重复导出、共享工具与浏览器回归。
- 验收：F1—F18 各有直接回归；每个声明支持项含初态 / 连续区间 / 边界，不静态 PNG freeze，不依赖 warnings sink，B 未授权与 U 正确拒绝。

## 4. 共用工程、证据与最终验收

### 4.1 API、CLI、诊断和预算（唯一工程定义）

以下是待确认接口草案，不是已新增代码。静态 enum 契约见第 2.3 节，动画 N/G/B/U 是能力分类，不直接替代 policy。

```text
SVGExportOptions:
  exportAnimation
  staticPolicy: SVGStaticExportPolicy = VectorOnly | PreserveAppearance | BestEffort
  textMode: SVGTextExportMode = Fidelity | Native
  embedFontsAsWoff2, fontConfig
  animation: SVGAnimationOptions
    selection: SVGAnimationSelection = Default | All | Named
    animationId
    policy: SVGAnimationPolicy = Strict | BestEffort
    maxPositionError = 0.25 document pixels
    maxAlphaError = 1/255
    maxSamplesPerTrack = 4096
  optionalFiniteScenePlan: disabled unless D8 explicitly authorizes MB
Diagnostics:
  reasonCode, instancePath, nodeId, channelOrAffectedOutput
  reason, action: exact | approximated | frozen | dropped | rejected
  outputClass, stateSource, consumedRangeOrFreezeTime
Budgets:
  referenceDepth, totalInstances, geometryPoints, glyphs, copies
  samplesPerTrack, subdivisionDepth, commonPeriodExpansion
  rasterPixels, rasterFrames, encodedBytes, totalOutputBytes
```

颜色误差、总预算先由内部统一管理，仅有已确认调用需求再扩公开字段；合法 Layout NaN sentinel 不被全局 finite 校验误拒绝。未知 / 越界 / typed 不匹配、资源失败、无支持映射、误差不达标、预算耗尽和开放宿主分别给原因码；未选择不可达定义不自动使导出失败。

CLI：`--svg-no-animation`、`--svg-animation <id>`、`--svg-all-animations`、`--svg-animation-policy strict|best-effort`、`--svg-static-policy vector-only|preserve-appearance|best-effort`、`--svg-text-mode fidelity|native`。Named / All 互斥，关闭动画与选择互斥，未知 ID / 冲突失败不 fallback；所有 warnings 到 stderr，成功提示不掩盖损失。MB 不增加默认偷偷开启的 CLI 路径。

ToSVG严格失败返回空结果、ToFile返回false、CLI非零；warnings=nullptr只影响收集，不影响成败。转换/能力/资源/编译失败不打开目标。**本方案推荐ToFile原子写出契约：** 同目录临时文件写入、flush/close全部成功后原子替换；写入或替换失败旧文件保持不变，临时产物清理有界。无法满足替换前置条件就失败，不改为直接截断目标；该契约不承诺断电/崩溃durability，失败注入须区分转换、write、flush/close和replace。

### 4.2 实施顺序与三层测试

严格静态先行：S0—S6 静态契约验收 → M0—M5 动效 → D8 同意才可 MB → M6。同一实例、资源、Mask、字形、预算、文件和评测基础只实现一处。

1. 结构层：XML / 唯一 ID / 完整引用、defs / 宿主与轨道数量、时间有限有序；静态无 SMIL、关闭动画不执行任何嵌套 driver。
2. 语义层：XML typed payload → Channel 真求值 → runtime ordered apply → effective text flush → draw；同初态 / 初始化 / 实例驱动 / mix 顺序，与序列化轨道重建对照。
3. 浏览器层：固定 Chromium、资源与绘制提交完成后，SMIL `svg.pauseAnimations()` / `svg.setCurrentTime(timeSeconds)` 确定性采帧，不用 WAAPI `document.getAnimations()` 或墙钟 seek。

动画参考从新场景按选择契约初始化：已选根Animation声明序apply→nested递归advance/apply→按宿主契约flush绑定和文字→draw。当前render CLI已驱动全部顶层Animation及挂载nested driver，可用于同集合/同初始化的场景，但不能直接充当Default/Named/有限事件重放参考。跨层VM先全部链接再bind，开放历史不从模板逆推；有限MB使用明确事件与同一共享VM身份。

简单 transform / alpha 直接属性；复杂图像比较关键区域、轮廓 / glyph 位置和像素差，SSIM 辅助；预期可见帧检查非空，透明帧断言透明。跨浏览器、d / 字体 / filter 支持以固定目标实测，不凭旧兼容性结论。

### 4.3 历史证据与当前核查状态（唯一证据汇总）

| 证据 | 已知结果 | 不能推出 |
|---|---|---|
| 旧工作区 Debug binary 的 PAGXSVGTest | 163 / 163 passed，未重新编译 | 当前源码全部通过、组合保真或日志无异常 |
| 历史动画 / Composition subset | 19 / 19 passed：SVG 17（16 Anim + 1 static）+ runtime 2 | 所有实例 / 初态 / 中间时刻 / pixel 正确 |
| 历史官方 `spec/samples/animation.pagx` 导出 | 0 animate，stderr 空 | 动画已支持；该结果正暴露根入口与丢弃诊断缺口 |
| 历史 SVG stderr | 三次 PNG 解码错误但断言仍通过，尚未定位 | 无日志异常或图片资源正确 |
| 当前 source 声明核查 | 166 个 SVG 用例、16 个 SVGExport_Anim、0 处显式 exportAnimation=false 选项 | “166 all passed”；声明数与历史 binary 结果必须分开 |
| 本轮文档重组 | 仅只读源码 / 声明语义核查与文档编辑 | 任何新构建、修复、测试、像素或跨浏览器认证 |
| 本轮 H0 热修（2026-11-10） | F3 / F5 / F9 / A08 已修，各带一个回归用例；`PAGXSVGTest` 170 / 170 通过（本机 SwiftShader） | 其余 F / ST 编号、像素门禁与跨浏览器结果 |
| 本轮 P1 增量（2026-11-10） | `BlurFilter` blurX/blurY、`DropShadowFilter` / `InnerShadowFilter` 的 offset + blur、`BlendFilter.color` 已可动画；滤镜区域改为按通道最大绝对值扩大；`PAGXSVGTest` 173 / 173 | 阴影 `color`（烘焙在 feColorMatrix 常量列）、Noise 参数、像素门禁 |
| 本轮 S0 部分（2026-11-10） | 通道账本（30 类 / 175 条）与双向门禁；退化动画改为显式告警；`ToFile` 改为同目录临时文件 + 原子替换；D1—D8 按推荐值采纳并记录；`PAGXSVGTest` 176 / 176 | S0 剩余项（D6 门禁、预算入口）、S1—S6、M0—M6 |
| 本轮 S1 前两项（2026-11-10） | ST33 隐藏层不再因持有 Mask 而漏画；ST34 composition 优先、不再与 contents 双画；各带一个回归用例；`PAGXSVGTest` 178 / 178 | S1 其余项（唯一 ID / 跨父 Mask 矩阵 / Contour clip-rule / scrollRect 失效 / 离屏 alpha） |
| 本轮 ST1（2026-11-10） | 生成 def id 避让作者 id（预扫描 `doc.nodes` 的 id 作保留集）；Composition 多实例的内联 id 自动加 `_1` 后缀；`PAGXSVGTest` 180 / 180 | ST2 跨父 Mask 矩阵、ST4 Contour clip-rule、ST5 离屏 alpha/blend、ST35 scrollRect |
| 本轮 ST2（2026-11-10） | 跨父 Mask / ClipPath 改为按 `inverse(W(owner.parent)) * W(mask)` 计算相对矩阵，def 缓存 key 加入 owner 父级；原来的跨父告警被真实矩阵取代；`PAGXSVGTest` 180 / 180 | ST4 Contour clip-rule、ST5 离屏 alpha/blend、ST35 scrollRect、S1 其余项 |
| 本轮 ST4 部分（2026-11-10） | `<clipPath>` 内容改用 `clip-rule` 承载 even-odd，不再写对 clipPath 无效的 `fill-rule`；`PAGXSVGTest` 181 / 181 | Contour 的 stroke 环（需要把描边展开成轮廓）未做；ST5 离屏 alpha/blend、ST35 scrollRect |
| 本轮 ST5（2026-11-10） | 需要离屏合成的层（filter / style / blend / 像素 mask）改为把 alpha 折到 `<g opacity>`，不再逐子下发；仅变换动画的层仍保持 per-child；`PAGXSVGTest` 182 / 182 | ST35 scrollRect 生成失效、Contour stroke 环 |
| 本轮 ST35（2026-11-10） | `updateScrollRect` 记录「由 layout 生成」，重新布局时刷新尺寸；作者显式 scrollRect 仍优先且不被覆盖；`PAGXSVGTest` 182 / 182、`PAGX*:PPT*:HTML*` 2436 / 2437（唯一失败是既有的 `PAGXTest.SVGToPAGXAll` 基线差异，与本次改动无关） | S1 遗留：Contour stroke 环、passThroughBackground 的完整离屏隔离；S2 未开始 |
| 本轮 ST6 部分（2026-11-10） | 滤镜区域把像素边距按 owner 的实际 bounds 换算成百分比，小元素大模糊不再被裁；`DropShadowStyle` 也改用通道最大值；`PAGXSVGTest` 183 / 183 | ST6 剩余：shadow 链不推进 source、filter 与 style 的顺序；ST7 spread / Glass / backdrop；ST40 BlendFilter 的额外 alpha clip |
| 本轮 ST7 部分（2026-11-10） | 阴影 `spread` 用 `feMorphology`（正 dilate / 负 erode）实现，不再丢弃并告警；区域边距把 spread 计入；`PAGXSVGTest` 183 / 183 | Glass / backdrop 仍是 B 类；ST40 BlendFilter 的额外 alpha clip 未动 |
| 本轮 ST40 部分（2026-11-10） | 没有 feBlend 对应物的 blend mode（plus-lighter / plus-darker）改为显式告警，不再静默按 normal 渲染；`PAGXSVGTest` 184 / 184 | BlendFilter 的 source/dest alpha 组合与透明洞仍未逐 mode 验证；Glass / backdrop 仍是 B 类 |
| 本轮 ST17（2026-11-10） | `antiAlias=false` 的层输出 `shape-rendering="crispEdges"` 并强制成组，默认值不改变输出；`PAGXSVGTest` 185 / 185 | 斜边 / 旋转 / clip 下的 AA 仍无像素级验证 |
| 本轮 ST43（2026-11-10） | `resolveModifiers` 不再作为「关闭就丢效果」的开关，修饰器一律解析，false 只产生告警；`PAGXSVGTest` 186 / 186 | — |
| 本轮 ST6 链 + ST7 backdrop（2026-11-10） | 阴影滤镜内联 `feMerge` 并推进 `currentSource`，后续滤镜读到阴影输出；`BackgroundBlurStyle` / `GlassStyle` 在预扫描阶段显式告警；顺带修好嵌套 mask writer 丢失 warnings 的问题；`PAGXSVGTest` 189 / 189 | S3 文字、S4 几何、S5 资源、S6 API 未开始 |
| 本轮 ST28（2026-11-11） | `LayoutNode::resolvedLayoutSize()` 逐轴读取布局产物；`EffectiveTextBoxWidth/Height` 对容器盒优先 resolved（含合法 0），对 modifier-only 盒保留 author 测量输入但百分比/双边必须先分配；`TextBox::updateLayout` 让空 modifier-only 子盒参与约束分配。新增 8 个单测与 2 个 SVG 回归；`PAGXUtilsTest` 233 / 233、`PAGXSVGTest` 202 / 203、全量 2866 / 2868 | 唯一 SVG 失败是既有 `SVGExport_GroupAlphaRoundTripPixels` 尚未接受截图基线（本分支新增用例，非 ST28 改动）；另一失败为既有 `PAGXTest.SVGToPAGXAll` 基线差异；未做像素门禁，RTL / 竖排 / rich text 排版未覆盖 |
| 本轮 ST21（2026-11-11） | 几何通道与 paint 通道分离：几何仍烘进 Text 克隆，paint 通道生成 identity 几何的 paint-only modifier；SVG 导出按 scope glyph index 累积 `GlyphPaintStyle`，按连续样式发射 base/overlay（overlay 纯色 SrcOver、alpha=weight），stroke 走 `BlendStrokeWidth`；`CanBakeTextModifiers` 放行可表达的 paint modifier，不再整层栅格化。新增 TextPaintUtils 单测 4 个、SVG 结构/像素回归 7 个；`PAGXUtilsTest` 237 / 237、`PAGXSVGTest` 208 / 209、`PAGX*:PPT*` 860 / 861 | 仅既有未接受基线与 `PAGXTest.SVGToPAGXAll` 失败；容器 TextBox 内的 paint modifier 仍走 bake；bitmap/vector 极端交错与 AA 边缘未做像素门禁 |
| 本轮 ST22（2026-11-11） | 新增 `TextPathUtils`，按 tgfx::TextPath 逐语句实现 normal/force、signed margin、reverse、closed wrap、open tangent、perpendicular 与 layout scale；SVG 导出把最终 glyph 矩阵存进局部 `glyphTransforms` 并发射轮廓，`CanBakeTextPaths` 放行可画的 glyphRuns。TP01—TP16 共 16 个数值测试；另加 SVG 结构回归；`PAGXUtilsTest` 253 / 253、`PAGXSVGTest` 209 / 210、四套件 1085 / 1087 | 两处失败均为既有基线（GroupAlphaRoundTrip / SVGToPAGXAll）；curve / 尖角 / TextModifier 前后组合的像素对照未做；native Text / TextBox 内 TextPath 仍走 bake |
| 本轮 ST23/ST24（2026-11-11） | MergePath 先清空当前 scope 的 geometry 与 painters 再放入合并 path；Repeater 改用 `powf`，非有限副本跳过，BelowOriginal 反序、AboveOriginal 正序，与 tgfx 的 copy 顺序一致；新增 resolver 级结构测试并修正负 fractional scale 的旧 SignedPow 期望；`PAGXUtilsTest` 255 / 255、SVG+Utils+PPT 720 / 721 | 仅既有 `SVGExport_GroupAlphaRoundTripPixels` 基线失败；Group/Text 参与 Merge 的递归合并与 Repeater 预算未覆盖 |
| 本轮 ST27（2026-11-11） | Rectangle / Ellipse 的零或负轴按 tgfx 的 5e-3 微轮廓处理：Fill 直接跳过，Stroke 输出微元素以保留 cap/dash；正常轴仍走原 stroke inset 路径；新增 3 个 SVG 结构回归 | 微轮廓的 inside/outside align 与 Trim/dash 的像素对照未做；Path 的退化微轮廓不在本条 |
| 本轮 ST11 部分 + ST42 部分（2026-11-11） | reversed 的 Rectangle / Ellipse / Path 在 resolver 中烘成带方向的 Path，SVG/PPT 的 dash 起点不再丢方向；`stroke.width <= 0` 显式输出 `stroke="none"`；SVG path 序列化从 `%g` 改为 `FloatToString` 的 9 位 round-trip 精度；新增方向/零宽/大数值精度回归；`PAGXSVGTest` 214 / 215 | 仅既有 GroupAlpha 基线失败；adaptive dash、Path 的 inside/outside align、奇数 dash × cap、layout-fit-coverage 分离与曲线远控制点 bounds 未做 |
| 本轮 ST16 部分（2026-11-11） | `ModifierResolver` 析构时按构造前 node 数移除本次创建的临时节点；Merge 产生的 even-odd Fill 改为克隆，不再修改作者 Fill；重复 SVG 导出的 node 数与输出字节稳定；四套件 1095 / 1097 | 两处均为既有基线；Repeater 乘性预算、shared PathData 污染与并发导出未覆盖 |
| 本轮 ST45 部分（2026-11-11） | 零 scale Group 保留退化 matrix 并输出结构有效的 SVG，不强行 inverse 恢复；新增结构回归 | 仅验证 Group + Rectangle + Fill；Painter 捕获前后 singular、Fill/Stroke/Mask 组合与像素未验证 |
| 本轮 S6 API 部分（2026-11-11） | 新增 `SVGStaticExportPolicy` / `SVGTextExportMode`，设置时覆盖 deprecated `bakeUnsupported` / `convertTextToPath`；CLI 增加 `--svg-static-policy` / `--svg-text-mode`；VectorOnly 对需要新增效果位图的图层直接拒绝（ToSVG 返回空并告警），BestEffort 保留降级告警；两项映射 + 拒绝/降级回归通过 | Named/All、diagnostics / reasonCode、预算、D6 浏览器门禁、策略可用性基线、ST32/37/41/44/46 仍未做 |
| 本轮 ST36 部分（2026-11-11） | `ToSVG` 每次都用 `options.fontConfig` 重跑 `applyLayout`，约束修改后导出不再复用旧 resolved 尺寸；新增编辑后导出回归 | 外部 Font / fontConfig 冷热与 ST37 externalDoc 预扫未做 |
| 本轮 ST37 部分（2026-11-11） | WOFF2 预扫递归 externalDoc 的 Font 节点并去重，external composition 的内嵌字体不再漏嵌；新增 externalDoc 字体回归 | external Image 资源 / 嵌套外部文档加载失败路径未覆盖 |
| 本轮 M1 部分（2026-11-11） | `SVGAnimationOptions.selection` 实现 Default（首个声明 Animation）/ Named / All；未被 timeline 驱动的动画用默认 timing；CLI 增加 `--svg-no-animation` / `--svg-animation` / `--svg-all-animations` 与互斥校验；新增三模式回归 | F2 双实例 / external 同名去重、F4 offset/paused 初态、F16 pause scope 仍未做；同 target 同 channel 的多动画 ordered mix 仍 last-wins |
| 本轮 ST30 部分（2026-11-11） | TextBox Native 路径加 `xml:space="preserve"`，多空格不再被 SVG 折叠；tab 按 layout glyph x 拆 tspan 定位；CJK 无空格行 justify 改用 `textLength` + `lengthAdjust="spacing"`；RTL 段落输出 `direction="rtl"` 并按物理边翻转 text-anchor；新增空白/tab/CJK/RTL 四回归 | RTL 数字与混合标点的像素对照、justify 与 tgfx 的逐行宽度对照未做 |
| 本轮 ST30 standalone（2026-11-11） | 独立 Text 的 native `<text>` 同样输出 `xml:space="preserve"` 与 `direction="rtl"`，anchor 按 RTL 物理边翻转；新增 standalone 空白/RTL 回归；D6 文本样本把 meanRgbDelta 阈值标定为 10 | RTL 数字 / 混合标点的像素对照仍未做 |
| 本轮 ST12/ST23 部分（2026-11-11） | MergePath 递归收集前置 Group 内的 shape 并按 group matrix 合并，随后清空整个 scope；空合并仍保留一个空 Path 以维持后续 modifier 的 shape slot；新增 group 合并回归 | Text 参与 Merge、跨 Group painter 语义与 Append overlap 像素未覆盖 |
| 本轮 ST11 部分（2026-11-11） | Path 的 `StrokeAlign::Inside` 用 2× stroke + 同路径 clipPath 实现，只保留内侧一半；新增结构回归 | Outside / adaptive dash / 奇数 dash×cap / fit 原 bounds 未做 |
| 本轮 ST42/ST45 部分（2026-11-11） | PathData bounds 控制点保守性测试；singular Group 的 Fill+Stroke 组合结构回归 | layout-fit-coverage 分离与 Painter 前后 singular 像素未覆盖 |
| 本轮 ST8 部分（2026-11-11） | `CollectRichTextRuns` 携带 Group 累积 alpha，TextBox 的 line tspan / tab tspan / embedded glyphRun 都乘上 run alpha；新增嵌套 Group alpha 回归 | per-run transform、foreground 双遍、混合 vector/bitmap 的完整顺序仍未做 |
| 本轮 ST21 部分（2026-11-11） | 容器 TextBox 的 paint-only modifier 对带 GlyphRun 的 Text 保持矢量：resolver 放行、writeTextBoxGroup 累积逐 glyph 样式并走局部 base/overlay；新增结构回归 | native Text 的 paint modifier 仍走 bake；未做像素对照 |
| 本轮 ST11 部分（2026-11-11） | Path 的 Inside/Outside stroke align 分别用 clipPath / mask 实现；adaptive dash 通过 tgfx PathEffect 烘进中心线后输出普通 stroke；非 adaptive 的 Square cap dash 也先按 tgfx 规则合并短 gap；新增三类回归 | Rectangle/Ellipse 的 adaptive dash、Text stroke align、dash×Trim 组合未做 |
| 本轮 ST8 部分（2026-11-11） | `CollectRichTextRuns` 同时携带 Group 累积 transform；TextBox embedded GlyphRun 的发射矩阵乘上 run transform；新增平移 Group 回归 | foreground 双遍、native tspan 的 per-run transform、bitmap/vector 完整顺序未做 |
| 本轮 ST16/ST24 部分（2026-11-11） | Resolver 增加跨嵌套 Repeater 的 200k 生成元素总预算，耗尽后停止复制，避免乘性展开耗尽内存 | 预算是硬截断而非 diagnostics 拒绝；200k 阈值与逐项预算未公开配置 |
| 本轮 S5 部分（2026-11-11） | ImagePattern 优先使用 runtimeImage 的 data URI / 可读路径；零长度 linear 取首色、近零 radial 取末色；fitsToGeometry 的零轴 bbox 按 1/16 clamp；Nearest filter 输出 `image-rendering="pixelated"`；新增 runtime image / 退化渐变 / nearest 三回归 | ST14 的真实四格式与 16 tile×4 fit、ST19 的 mipmap、ST25 空间插值/P3、ST13 shape 零轴 objectBoundingBox、ST42 layout-fit-coverage 仍未做 |
| 本轮 S6 部分（2026-11-11） | `SVGAnimationOptions.policy` 实现 Strict / BestEffort：Strict 对 StateMachine、未知 target、不支持通道、无有效通道直接拒绝（ToSVG 返回空），BestEffort 保留可表达子集并告警；CLI 增加 `--svg-animation-policy`；新增 Strict 拒绝回归 | Diagnostics reasonCode / 预算 / D6 浏览器门禁 / 策略基线 / ST32/41/44/46 / C20—C24 仍未做 |
| 本轮 S6 诊断与文件（2026-11-11） | 新增 `SVGDiagnostic` sink（reasonCode / action / message），Strict 与 VectorOnly 拒绝写 `Rejected`；ToFile 增加 `PAGX_SVG_FAIL_STAGE` 失败注入，覆盖 write/flush/close/replace 并验证旧文件保持不变；新增回归 | reasonCode 尚未覆盖全部站点；budgets、D6 浏览器门禁、策略基线、ST32/41/44 仍未做 |
| 本轮 S6 预算（2026-11-11） | `SVGBudget.maxGeneratedElements` 接入 Resolver 的乘性展开上限；`maxOutputBytes` 超限时 ToSVG 返回空并写 `budget-exceeded` 诊断；`maxRasterPixels` 在层烘焙前检查，超限回退矢量并写诊断；新增预算回归 | samples / instances / encodedBytes 等其余预算字段未实现；D6、策略基线、ST32/41/44、C20—C24 未完成 |
| 本轮 D6（2026-11-11） | 新增 `test/svg_browser_gate.js` 与 `SVGBrowserTest`：`pagx export` → 固定 Chromium 截图 → `pagx render` 参考 → pixel/SSIM 指标，固定 1400×900 / 白底 / scale 1 / pixelmatch 0.1；支持 `SVG_BROWSER_CASES` 与单例阈值覆盖；`constraint_textbox` 实测 pixelDiff 0.031、RGB 5.29、SSIM 0.416 | 默认阈值按首个文本样本标定为 0.05 / 10 / 0.4，仍需更多语料标定；Playwright 固定 Chromium 未在本机安装 |
| 本轮 4.7 策略基线（2026-11-11） | 新增 `test/svg_policy_baseline.js` 与 `SVGPolicyBaseline` target，扫描 271 个 resources PAGX：vector-only 241 native / 4 bake / 26 reject（17 unsupported-feature、9 import/resource）、preserve 241 / 21 / 9、best-effort 256 / 6 / 9；生成 `docs/svg-policy-baseline.md` | bake 桶把作者图片的 PNG 也计入，是效果烘焙上界；真实 reject 比例仍需分离资源失败 |
| 本轮 M4.E seed（2026-11-12） | `NoiseFilter.seed` 进入白名单与 Supported；`feTurbulence` 的 seed 作为静态基值保留，元素内发射 `seed` 轨道；NoiseStyle 仍走烘焙路径，不伪造 seed 动画；新增 seed 回归 | 其余 50 项 effect 通道与 NoiseStyle 参数未动 |
| 本轮 M5.4 修饰器覆盖补全（2026-11-12） | 同一 `ShapeGeometryOverride` 通路覆盖 RoundCorner 与 Continuous Trim：Rect+RoundCorner、双 Path+Continuous Trim 均按帧重烘焙并各自发 `d` 轨道；新增两条单测与 RoundCorner 浏览器门禁用例 | Merge/Repeater 链、椭圆 size 采样、非均匀 scale 的 bounds 仍未覆盖 |
| 本轮 M5.4 修饰器重烘焙（2026-11-12） | resolver 将 bake→authored source 映射沿 Trim/RoundCorner/连续 Trim 传播；Path point 通道在 modifier 链后按帧替换 PathData 并重跑 resolver，`<path>` 发射重烘焙后的 d 轨道（每帧上限 32 采样，失败在 Strict 拒绝）；Path point 与 Rect/Ellipse 的 position/size/roundness 通过 `ShapeGeometryOverride` 无副作用逐帧采样（不触发 layout），统一重烘焙为 d 轨道；新增 Rect+Trim 与 Path+Trim 两条 d 轨道回归 | Merge/Repeater 链、椭圆/圆角独立像素门禁、浏览器 d 轨道 seek 对照未做 |
| 本轮 M6/F12（2026-11-12） | `pagx export` 的 SVG 路径把 `ToFile` 的 warnings 逐条输出到 stderr；Strict 拒绝时先打印原因再报错，保持非零退出且不落文件；成功导出也把 silent drop 告警打到 stderr；新增 strict 拒绝 CLI 回归 | PPT/HTML 的 warning sink 未接 |
| 本轮 M6/F3（2026-11-12） | PingPong 的 visibility 轨道补齐镜像：discrete 值序列同样往返并双倍 dur，与 transform/color 的镜像规则一致；新增 visible→hidden 的 `visible;hidden;visible` 回归 | 无 |
| 本轮 M2/F10（2026-11-12） | mask/clipPath 的嵌套 writer 现在继承 animation plan、Strict/BestEffort 策略、diagnostics 与预算，mask 内部的轨道发射到实际 defs 宿主且 Strict 拒绝会传播到整份导出；写 def 前对 mask 子树补 `updateSize`，修复 mask 层不在 doc.layers 导致正文恒空的问题；新增 mask 内 width 轨道回归 | mask 内 TextBox/TextPath 的宿主映射、跨父级 mask 轨道空间未单独验证 |
| 本轮 M2/F10 TextBox（2026-11-12） | native TextBox 的变换动画用外层 `<g>` 承载 `animated * inverse(static)` 增量矩阵，再分解为 translate/rotate/scale/skew 轨道，覆盖 path 之外的原生 `<text>/<tspan>` 出口；新增 position.x 回归；顺带确认 TextBox 的 `position` 属性不参与布局、需用 left/top 等约束 | TextBox 基线与运行时的像素差（浏览器偏上约 5px）未解决，未加入浏览器门禁 |
| 本轮 M6 能力文档（2026-11-12） | 新增 `docs/pagx-svg-animation-capabilities.md`，由 `SVGChannelLedger()` 生成（175 通道：77 Supported / 90 Planned / 8 NotExpressible），并新增 `SVGAnimationCapabilitiesDocIsCurrent` 门禁：ledger 改动后文档不更新即失败；表外 Path point 族与动效门禁用例一并记录 | 文档仅覆盖通道级状态，F1—F18/A12—A20 的场景矩阵仍待逐条落表 |
| 本轮 M6 动效门禁（2026-11-12） | 浏览器门禁支持 `case.pagx@seconds`：暂停 SMIL、`setCurrentTime(seconds)` 后截图，并与 `pagx render --time` 参考比较；新增 `SVGBrowserAnimTest` 与八个 0.5s 用例：SolidColor 红→蓝（pixelDiff 0 / RGB 0 / SSIM 1）、ColorStop 红→绿（0 / 0.14 / 0.99999）、linear endPoint.x 160→80（0 / 0.06 / 1）、Text.x 平移（0.00007 / 0.63 / 0.983）、Polystar outerRoundness 0→1（0 / 0.14 / 0.9994）、Rect size+Trim 的 d 轨道（0 / 0 / 1）、Rect size+RoundCorner（0 / 0.07 / 0.999）、Ellipse size+Trim（0 / 0.24 / 0.994）；门禁发现并修复 Text 包装 `<g>` 未先闭合 start tag 导致 `<g  <animateTransform` 的非法结构，单元测试补结构断言 | d 轨道的浏览器 seek 对照（运行时无普通 Path point writer）与更多语料阈值未做 |
| 本轮 M4.1（2026-11-11） | SolidColor.color 进入白名单；Fill/Stroke 的 painter 动画会向上解析共享 SolidColor 的绑定并各自发射 fill/stroke 轨道；`HasPainterAnimation` 同步识别源绑定；ledger 的 SolidColor 升级为 Supported；新增 SolidColor 驱动 Fill 回归 | 多消费者去重像素与源/Painter 同有绑定的真实顺序未做 |
| 本轮 M4.2（2026-11-11） | ColorStop.offset/color 进入白名单；linear/radial 的 `<stop>` 直接承载 offset、stop-color、stop-opacity 三条 SMIL；共享 stop 在每个 def 实例各发一份；conic/diamond 的 stop 通道在 Plan 阶段按实例拒绝（Strict 拒绝、BestEffort 保留静态）；P3 动画 stop 报 `color-space`/Approximated 并去掉会覆盖轨道的 CSS 声明；新增 stop 双通道 / 跨 def / conic 拒绝 / P3 四个回归 | 重复/非单调 offset 与 stop 颜色混色的浏览器像素对照、stop 数量重排未做 |
| 本轮 M4.3（2026-11-11） | linear 四端点与 radial 中心/半径进入白名单；def 内直接发射 x1/y1/x2/y2 与 cx/cy/r；r 的采样值下限钳到 0（SVG 拒绝负半径）；静态已退化的 gradient 不承载几何轨道，Plan 阶段按实例拒绝并告警；conic/diamond 的几何通道在 ledger 标为 NotExpressible；新增端点共享 painter / radial 钳制 / 静态退化拒绝三回归 | 动态 bounds 与 stop 联动的浏览器像素门禁留到 M5.4；conic/diamond 仍为退化近似 |
| 本轮 M5.0/M5.1 部分（2026-11-12） | 新增 Path `point{N}.x/y` 表外通道族（不进入 175 条账本）：Plan 按实例校验 float 类型与点索引，导出在 `<path>` 上发射 `d` 轨道，逐帧求值点数组后按原 verb 结构重建，静态 renderPosition/renderScale 烘入每帧；Hold → discrete，单一或一致 Bezier → keySplines，混合缓动按 8 段降采样并告警；被 path modifier 烘焙、stroke align、adaptive dash 改写的 path 在 Strict 下拒绝并告警；新增 d 轨道 / Hold / modifier 拒绝三回归 | Polystar/Trim/Round 的逐帧重烘焙、point 原始空间与动态 bounds、Mask+modifier 的 d 轨道与浏览器像素门禁未做 |
| 本轮 M5.2（2026-11-12） | Text.x/y 进入白名单；导出在 Text 外层插入无静态 transform 的 `<g>`，用 animateTransform type="translate" 发射相对 authored position 的位移，原生 `<text>`、WOFF2、glyph path 三个出口共用；TextPath 重排与 TextBox 容器内的 Text 不假成功，Strict 拒绝并告警；新增 standalone translate 回归 | TextPath normal 沿曲线重求值未实现；TextBox 内逐 run 位移留待 M5.4 |
| 本轮 M5.3（2026-11-12） | TextHolder flush 改为 `TextLayout::Layout(..., false)` 强制从有效 TextGlyphParams 塑形，all-embedded 不再走 bounds fast path；有效空（runs 为空）调用新增的 tgfx `Text::clearTextBlob()` 清空 blob/anchors 并保留对象与 position；塑形失败清空并 LOGE，不再保留旧 glyph；显式 anchors 按 glyph 序截断/补零后随 setTextBlob 保留；新增空→非空、anchor 重排、embedded→新文字三回归 | tgfx `clearTextBlob()` 只在本地 `third_party/tgfx` 实现并重编验证，尚未回填上游依赖版本；ST21/ST22 新 glyph 像素门禁未做 |
| 本轮 M5.4 部分（2026-11-12） | 同一 node+channel 被多个 Animation 驱动时按同属性多 Clock 分类：BestEffort 保留后写轨道并告警，Strict 拒绝；Path point `d` 轨道增加 512 采样上限并按 budget 降采样告警；Text.x/y ledger 升级 Supported；新增 Plan 白名单↔ledger 双向门禁（Path point 族表外豁免）与 multi-clock 回归；任何被 path modifier / reversed 烘焙替换的动画元素不再静默冻结，Strict 拒绝且 BestEffort 告警保留静态；新增 A14 rect+Trim 回归 |
| 本轮 M5.4 Polystar（2026-11-12） | Polystar→Path 转换抽到共享的 `PolystarPath`（resolver 与导出共用），resolver 记录 bake→source 映射；rotation / 内外 radius / position.x/y / 内外 roundness 进入白名单并在烘焙后的 `<path>` 上发射 `d` 轨道；每帧用临时 Polystar 快照重算 content bounds、renderScale 与 renderPosition，逐帧强制 cubic verb 列表保持结构稳定；pointCount / reversed 仍按拓扑变化拒绝；新增 rotation d 轨道、roundness d 轨道与 pointCount 拒绝回归 | Repeater 拓扑、TextModifier/RangeSelector、动画 Text 的 flow/region 未做 |

### 4.8 S3 文字待办清单（下一批入口）

按文档 S3 拆成可独立验收的条目，每条的源码入口已定位：

| 编号 | 问题 | 入口 |
|---|---|---|
| ST8 | `writeTextBoxGroup` 只重排 `run.text->text`，丢弃 `GlyphRun`、多 Painter、transform / alpha、foreground 双遍 | `SVGExporter::writeTextBoxGroup` |
| ST9 | WOFF2 路径多 Font / 多字号 / advance / anchor 不完整 | `writeTextAsFont`、`WriteSharedTextAttrs` |
| ST10 | glyph `fill-opacity` 与位图字形缺 alpha 因子 | `writeTextAsPath` |
| ST18 | `fauxBold` / `fauxItalic` 在 WOFF2 与 outline 路径上丢失 | `BuildStyledTypeface` |
| ST20 | `RangeSelector` 的 index offset / shape / 首 mode | `TextUtils::CollectRichTextRuns` 一带 |
| ST21 | 累积override与base/overlay分支、排他draw归属 | `ModifierResolver::bakeTextModifier`及导出局部glyph/paint发射；第2.8.1节 |
| ST22 | TextPath八字段/布局到最终glyph矩阵 | `LayerBuilder::convertTextPath`、tgfx::TextPath::apply、局部glyph发射；第2.8.2节TP01—TP16 |
| ST28 | Native重排版错取继承author尺寸，覆盖resolved双边/百分比 | `EffectiveTextBoxWidth/Height`→`MakeTextBoxParams`→`writeTextBoxGroup`；第2.8.3节，不新增字段 |
| ST29 | 空布局 fallback 重新画出被 Hidden 的行 | `writeTextWithLayout` |
| ST31 | 同一 run 内 vector / bitmap 字形先 path 后 image，破坏遮挡序 | `ComputeGlyphPathsAndImages` + `writeTextAsPath` |

其中 ST31 需要把两个 `glyphPaths` / `glyphImages` 容器合并成带序号的单一列表，改动会波及 `TextUtils`；其余多为局部修正。

ST8 进度：带 `GlyphRun` 的 run 在两条路径上都改为输出轮廓 `<g>`（行内 `<text>` 关闭后按行输出；无行信息时按 run 顺序输出），只有在没有任何可用轮廓时才告警。`TextLayout` 对所有 Text 都带内嵌 `GlyphRun` 的 TextBox 只算 bounds、不产出行，因此行布局分支对内嵌 run 实际不可达，已由兜底分支直接覆盖（`SVGExport_TextBoxGlyphRunEmitsOutlines`）；混合 run 的 TextBox 里未被行消费的内嵌 run 在行循环之后补发，不再静默丢弃。多 Painter 由 `CollectRichTextRuns` 配对，per-run transform / alpha 与 foreground 双遍仍未表达。

ST9 / ST10 / ST18 / ST29 / ST31 已收口：`writeTextAsFont` 走字形 advance（无 positions / xOffsets 时按字宽推进）、要求单一 Font 且无旋转（旋转轴心与 anchor 无法在 `<text>` 表达），否则回落到轮廓；`<text>` 的 x 列表因此与 `WalkGlyphs` 的笔位一致。字形 `<g>` 的 fill / stroke opacity 携带累计 alpha，位图字形单独补 `opacity`。fauxBold / fauxItalic 在 WOFF2 与原生 `<text>` 两条路径都输出 `font-weight` / `font-style`。`overflow="hidden"` 丢光所有行时不再由兜底分支重画。字形改由 `ComputeGlyphDrawItems` 按 glyph 顺序返回，vector / bitmap 混排不再先画全部 path 再画 image。

ST20 已收口：新增 `TextSelectorEvaluator`（`src/pagx/utils/TextSelectorUtils.h`）实现 RangeSelector 的单位换算（Percentage / Index）、offset 先作用于两端、start > end 交换、六种 shape（Square / RampUp / RampDown / Triangle / Round / Smooth，Triangle 吃 easeIn / easeOut）、randomOrder 的确定性洗牌，以及多选择器按序合并（首个选择器直接取值，Subtract 从 1 起算，逐次 clamp 到 [-1, 1]）。`ModifierResolver` 遇到仅含几何通道的 TextModifier 时，把 position / rotation / scale / skew / anchor 按字符权重烘进前置 Text 的 GlyphRun 通道并丢弃该 modifier；含 alpha / fillColor / strokeColor / strokeWidth 或 skewAxis 的 modifier 仍保留给烘焙路径。SVG 侧用 `ModifierResolver::CanBakeTextModifiers` 让探针跳过这类图层，不再整层栅格化；原生 TextBox 的元素列表也会先经过 resolver。克隆 Text 会清空 id，避免文档里出现两个同 id 节点；`randomSeed == 0` 时额外套用 AE 的首字符固定映射表，保证第一个字符的落点稳定。若 modifier 之后还有 Text 嵌在更早的 Group / TextBox 里，resolver 保留 modifier 并让探针不跳过，避免外层 modifier 打不到嵌套字形而静默丢效果。

ST21 已收口（2026-11-11）：几何通道仍由 `ModifierResolver` 烘进 Text 克隆，paint 通道改由 identity 几何的 paint-only modifier 承载；SVG 导出在 scope 内按 glyph index 累积 `GlyphPaintStyle`，同一连续样式 run 先发 base（weight<1，原 shader）再发 overlay（weight>0，纯色 SrcOver，fill/stroke-opacity=weight×painterAlpha×styleAlpha），stroke 宽度用 `BlendStrokeWidth`。像素 roundtrip 已覆盖 weight 0.5×r1、0.5×r0.5、1×r0.5、红绿累积与逐 glyph range；结构覆盖 full override 去 base、stroke overlay、无栅格化。仍待补：容器 TextBox 内 paint modifier、bitmap/vector 极端交错、AA 边缘像素门禁。

ST22 已收口（2026-11-11）：新增 `TextPathUtils`，按 tgfx::TextPath 的 normal / force 分支实现 baseline 投影、signed first/last margin、reversed、闭合正模回绕、开放端沿切线延伸、perpendicular 旋转与 layout k/T；SVG 导出把最终矩阵存入 `AccumulatedGeometry::glyphTransforms` 并输出轮廓，`CanBakeTextPaths` 仅在目标 Text 有可画 GlyphRun 时跳过栅格化。TP01—TP16 的 anchor/矩阵数值测试全部通过，另加一个 SVG 结构用例。仍待补：曲线/尖角/TextModifier 前后次序与 tgfx 的像素对照，以及 native Text / TextBox 内 TextPath（当前仍走 bake）。

ST28必须保留：TextBox确实继承Group→LayoutNode的width/height，Native是Exporter分支不是节点字段。入口和双边/百分比/author/内容测量的优先级已在第2.8.3节写定，包含容器有效0与modifier-only unbounded的区别；不删除此条、不新增width/Native字段。

ST28 已收口（2026-11-11）：`LayoutNode` 新增只读 `resolvedLayoutSize()`，保留未分配轴的 NaN；`EffectiveTextBoxWidth/Height` 对容器盒只读该次布局的外框（合法 0 保留），对 modifier-only 盒保留 author 作为测量输入、但百分比/双边必须先真正分配；`MakeTextBoxParams` 仍只扣一次 padding。`CollectLayoutNodes(skipTextLayout=true)` 放行空 TextBox 并排除容器 TextBox，`TextBox::updateLayout` 在约束分配前先 `updateSize`。整数验收覆盖 250×170→230×150、240×100→220×80、100×60→80×40、left=170/right=130 的合法 0、无来源 NaN、嵌套 modifier-only 与布局重跑；SVG 侧另测锚点取 250 而非 author 100，以及高度分配为 0 时 overflow="hidden" 仍丢弃全部行。像素门禁、RTL / 竖排 / rich text 仍不在本条覆盖内。

现有盲区保留依据：cross-parent Mask 只查 warning / defs 且父恒等（`test/src/PAGXSVGTest.cpp:4433`）；伪 JPEG / WebP 头仅 MIME（`test/src/PAGXSVGTest.cpp:3183`、`test/src/PAGXSVGTest.cpp:3211`）；bitmap fallback 无真实数据（`test/src/PAGXSVGTest.cpp:3506`）；动画字符串测试从 `test/src/PAGXSVGTest.cpp:4556` 起。开关用例未做字节相等、普通Layer timeline不证Composition scope、PingPong未全属性、paused/offset仅begin均需直接复现；render CLI当前已有nested驱动，不沿用此前“仅顶层”的旧描述。

### 4.4 后续实现时的构建与基准规则

以下仅保留项目规定的未来验证命令，**本轮不执行格式化、构建、测试或基准操作**。

```bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest_OpenGL
./cmake-build-debug/PAGFullTest_OpenGL --gtest_filter='PAGXSVGTest.*:PAGXTest.*:PAGXCliTest.*'
./cmake-build-debug/PAGFullTest_OpenGL
cmake --build cmake-build-debug --target HTMLTest
cmake --build cmake-build-debug --target SVGBrowserTest
```

共享 tools / TextLayout / ModifierResolver / 资源变化覆盖 PAGX runtime、静态 SVG、PPT / HTML 全量及浏览器门禁，不只编译。测试非零就是失败，不原样重跑碰运气；同工作区不并行不同后端截图。不得跳过基线、手改 version.json 或运行 baseline 更新脚本 / target；截图仅用户主动 `/accept-baseline`。HTML 均值不手改，有意提升按项目规定独立流程处理。

### 4.5 最终 accept check

- [ ] D1—D8 逐项明确；未同意 D8 时 MB 禁用、B Strict 拒绝，开放宿主 U 不假装有限化。
- [ ] 静态 55 / 334、31 / 111、40 / 155 声明集合与完整逐字段语义不遗漏；C01—C24、ST1—ST46 有直接验证；分类与真正实现 / pixel 状态分别登记。
- [ ] staticPolicy / textMode / D6 / D7 正交验收；背景消费、Painter lifecycle、资源链、Font 不可变 / 串行化、singular 与 atomic file 行为闭合。
- [ ] 动效 175 / 30、表外 Path / image / 无 writer 全分类；F1—F18 与动态 A 矩阵直接验证；每个真实支持 N/G 项有 typed apply、初态、连续误差 / 区间、边界及 browser 证据。
- [ ] Clock / pause scope、Layer 三层 priority / ordered mix、full turns / shortestArc、Hold / overshoot / empty / key boundary 与 topology 正确；Strict 无默认 PNG freeze，损失 / 拒绝可定位。
- [ ] 同输入重复导出与 ID / 资源稳定，无模板污染 / 无界 nodes；失败和全部写失败保旧文件按承诺测试，warnings sink 不改变成功。
- [ ] 共用工具、PAGX runtime、静态 SVG、PPT / HTML、全量与目标浏览器门禁通过；能力文档区分 classified、supported、approximate、B/MB 和 U。

### 4.6 对外接口迁移（新增）

`SVGExporter.h` 位于 `include/pagx/`，是公开 SDK 头文件。2.3 节的收口——`bakeUnsupported → staticPolicy`、`convertTextToPath → textMode`、移除 `resolveModifiers`——属于破坏性变更，需要先定迁移方式再动代码。

| 方案 | 做法 | 代价 |
|---|---|---|
| A. 保留旧字段并标记 deprecated | 旧字段映射到新策略：`bakeUnsupported=true → PreserveAppearance`、`false → VectorOnly`；`convertTextToPath=true → Fidelity`、`false → Native`；`resolveModifiers` 恒视为 true 并告警 | 字段冗余，但零迁移成本 |
| B. 一次性替换 + 升主版本 | 删除旧字段，同步全部调用方与对外文档 | 需要 libpag 主版本号变更 |
| C. 双轨 + 编译期开关（`PAGX_SVG_EXPORT_API_V2`） | 同时维护两套入口 | 两套分支长期共存，不推荐 |

**已采纳 A（2026-11-11）**：新增 `staticPolicy` / `textMode` 作为权威字段，旧字段保留并标 deprecated 做映射；`resolveModifiers=false` 立即改为忽略并告警。等下一个主版本再删除旧字段。

### 4.7 默认策略可用性基线（新增）

D3 推荐 Strict、D4 推荐 PreserveAppearance/VectorOnly；这组默认值会让一部分真实文档直接拒绝导出。上线前需要一条基线，避免「保真度拉满但没有文档导得出来」。

在 `resources/**/*.pagx` 上按三套静态策略各跑一遍导出，统计：

| 指标 | 含义 |
|---|---|
| native 比例 | 全部内容走原生矢量，无烘焙 |
| bake 比例 | 至少触发一次完整上下文烘焙 |
| reject 比例 | 按策略拒绝导出 |
| 首因分布 | 拒绝的首要原因：conic / diamond / TextPath / TextModifier / 透视 / 预算 |

基线结果写入独立文件（例如 `docs/svg-policy-baseline.md`），不进入像素门禁；策略调整时重跑并更新。若 Strict 的 reject 比例过高，先回看 D4 的默认值，而不是放宽误差阈值。

## 5. 完整附录与唯一核查目录
### A. 完整模型、辅助结构与值数据

#### A.1 继承基类、附属结构与派生字段（31 类 / 111 项）

字段均为自有成员；Node inventory 按直接基类递归继承。`—` 是确实没有成员，不是未核查。方法 / 静态常量 / 前置声明不算字段；反射 flags 不是模型全集（`src/pagx/PAGXChannelTable.h:28`）。

| 模型类 / 结构 | 声明位置 | 直接基类 | 自有字段（完整） | 分类与处理 |
|---|---|---|---|---|
| Node | `include/pagx/nodes/Node.h:277` | — | `id, customData, sourceLine` | 全部节点继承：统一身份、转义、确定性及元数据保留策略；sourceLine 用于诊断，不参与绘制 |
| Element | `include/pagx/nodes/Element.h:31` | Node | — | 自身没有 visible / alpha / enabled；不能把 tgfx VectorElement 的运行开关误当作者字段 |
| LayoutNode | `include/pagx/nodes/LayoutNode.h:38` | — | `left, right, top, bottom, centerX, centerY, percentWidth, percentHeight, width, height, preferredX, preferredY, preferredWidth, preferredHeight, layoutX, layoutY, layoutWidth, layoutHeight` | 前 10 个是作者输入；后 8 个是测量 / 布局产物。未设置用 NaN，不得被统一 finite 校验误拒绝 |
| ColorSource | `include/pagx/nodes/ColorSource.h:30` | Node | — | inline / resource 引用身份，不等于 SolidColor |
| Gradient | `include/pagx/nodes/Gradient.h:33` | ColorSource | `matrix, colorStops, fitsToGeometry` | 四种渐变全部继承；matrix 是空间映射，stop 是有序结构而非只有一个颜色值 |
| LayerFilter | `include/pagx/nodes/LayerFilter.h:29` | Node | — | 不继承样式的 blendMode / excludeChildEffects |
| LayerStyle | `include/pagx/nodes/LayerStyle.h:30` | Node | `blendMode, excludeChildEffects` | 五种样式全部继承；决定样式绘制与输入来源，必须进入 S2 |
| TextSelector | `include/pagx/nodes/TextSelector.h:28` | Node | — | RangeSelector 的基础身份；选择作用于实际 glyph，不凭空引入字符 / 单词 / 行选择字段 |
| ImportDirective | `include/pagx/nodes/Layer.h:355` | — | `source, format, content, resolvedFrom` | Layer.importDirective 的四字段；前三个需 resolve / 明确前置条件，resolvedFrom 是来源元数据 |
| Timeline | `include/pagx/nodes/Timeline.h:40` | — | — | 非 Node；按 TimelineType 分类，不要求生成 DOM |
| AnimationTimeline | `include/pagx/nodes/AnimationTimeline.h:32` | Timeline | `animationId, playing, evaluationOffset` | 实例驱动；模板静态主动排除时间求值，paused 不等于没有初态 |
| StateMachineTimeline | `include/pagx/nodes/StateMachineTimeline.h:32` | Timeline | `stateMachineId` | 实例宿主行为，模板静态不启动 |
| AnimationState | `include/pagx/nodes/State.h:67` | State | `animationId` | NodeType 仍是 State；空引用表示逻辑空状态，不是另一个 NodeType |
| TypedChannel | `include/pagx/nodes/Channel.h:94` | Channel | `keyframes` | 各实际类型实例化有序 Keyframe；类型分派不是额外 NodeType |
| Keyframe | `include/pagx/nodes/Keyframe.h:86` | — | `time, value, interpolation, bezierOut, bezierIn` | 帧索引和值、左右段控制点；静态不应用，求值初态不可省略 |
| ImageRef | `include/pagx/nodes/Keyframe.h:66` | — | `id` | 引用所属文档 Image，不能当宿主 PAGImage |
| FontRenderCache | `src/pagx/nodes/FontRenderCache.h:32` | — | `typeface, built` | 派生构建状态，含失败缓存；仅按合法生命周期测试，不承诺热修改重建 |
| TextLayoutGlyphRun | `src/pagx/TextLayout.h:42` | — | `font, glyphs, positions, xforms` | shaping 产物，GID 对应实际 font，不是直接可复制的 Font.glyphs 子集索引 |
| Text::GlyphData | `src/pagx/TextLayout.h:49` | — | `textBlob, anchors, layoutRuns` | Text.glyphData 派生结果；Fidelity 兼顾 layoutRuns 与作者 glyphRuns |
| TextLayoutLineInfo | `src/pagx/TextLayout.h:96` | — | `baselineY, startX, lineWidth, byteStart, byteEnd` | 真实分行、位置和字节范围，不能只重算文字字符串 |
| TextElement | `src/pagx/TextLayout.h:65` | — | `node, glyph` | Text 身份与实际 shaping 属性，后者可来自宿主覆盖，不能直接取原模板 |
| PositionedGlyph | `src/pagx/TextLayout.h:77` | — | `glyphID, font, x, y` | 水平布局派生字形结果 |
| VerticalPositionedGlyph | `src/pagx/TextLayout.h:84` | — | `glyphID, font, useRSXform, position, xform` | 竖排布局派生字形与变换模式 |
| TextGlyphParams | `src/pagx/TextLayoutParams.h:36` | — | `text, fontFamily, fontStyle, fontSize, letterSpacing, fauxBold, fauxItalic` | 每 Text 塑形输入，来自模板或明确宿主状态 |
| TextLayoutParams | `src/pagx/TextLayoutParams.h:52` | — | `boxWidth, boxHeight, textAlign, paragraphAlign, writingMode, lineHeight, wordWrap, overflow, baseline, textScale` | 容器级有效布局投影，不被低优先级 author 尺寸替代 |
| tgfx::Geometry | `third_party/tgfx/src/layers/vectors/Geometry.h:71` | — | `matrix, shape, textBlob, textAnchors, glyphs, glyphRuns` | 运行时实际几何 / 字形与缓存，不是 PAGX 新节点 |
| tgfx::Glyph | `third_party/tgfx/src/layers/vectors/Geometry.h:50` | — | `glyphID, font, anchor, matrix, style` | 运行展开 glyph 与最终样式，与 PAGX Font 内 Glyph 资源分开 |
| tgfx::GlyphStyle | `third_party/tgfx/src/layers/vectors/Geometry.h:36` | — | `fillColor, strokeColor, strokeWidth, strokeWidthFactor, alpha` | TextModifier 最终 paint 覆盖，不只是 RGBA |
| tgfx::StyledGlyphRun | `third_party/tgfx/src/layers/vectors/Geometry.h:61` | — | `textBlob, matrix, style` | 连续同样式绘制结果，保留顺序 |
| tgfx::Geometry::TextBlobResult | `third_party/tgfx/src/layers/vectors/Geometry.h:73` | — | `blob, commonMatrix` | geometry 转换后的公共变换与 TextBlob，不叠加两次 |
| TextLayoutResult | `src/pagx/TextLayout.h:104` | — | `bounds, horizontalGlyphs, verticalGlyphs, perTextBounds, layoutGlyphRuns, textLines` | 整体及每 Text 派生布局；有效空不等于缺 metadata |

#### A.2 分量、开放输入与缓存投影

| 类型 | 完整分量 / 存储 | 验证要求 |
|---|---|---|
| Point | `x, y` | 位置、anchor、scale 各按消费者单位与默认 |
| Size | `width, height` | primitive size 不等于 LayoutNode width / height |
| Rect | `x, y, width, height` | layout、ink、scroll viewport 与采样区域分开 |
| Padding | `top, right, bottom, left` | XML 1 / 2 / 4 值按 parser，容器与 TextBox 两路径 |
| Matrix | `a, b, c, d, tx, ty` | identity / finite / invertible 按消费方，不统一套 3D 规则 |
| Matrix3D | `values[16]` | column-major，全部分量、affine、constant w、投影 / 深度 |
| Color | `red, green, blue, alpha, colorSpace` | 默认不透明黑；alpha、色域 / 空间插值单列 |
| ColorMatrixFilter.matrix | `matrix[20]` | 四行五项 RGBA + bias；20 项 one-hot / bias，不只 identity |
| DataConverter.params | 开放 `string → string` map | 四内建参数与宿主注册边界；未知参数不能当闭合 enum |
| Data / PAGImage / FontConfig | 编码数据、宿主图片、字体供应接口 | 实际来源、尺寸、像素可读回、字体选择与生命周期；非 NodeType，不枚举运行库所有内部成员 |

Document._fontConfig 是输入，layoutApplied 是流程状态；nodeMap / nodeSet 是索引，liveScenes 是生命周期，layersByImageFilePath / Built 是缓存。Font.renderCache、Text.glyphData / textBounds / textScale、PathData._cachedBounds / _boundsDirty 是派生状态；Image.runtimeImage、PathData._verbs / _points 虽 private 却是可观察输入，不排除。

PropertyData 是 runtime 投影而非模型清单（`include/pagx/PropertyData.h:36`）：基类 `propertyType, propertyName, customDataMap`；Number `defaultValue, minValue, maxValue`；String / Boolean / Color `defaultValue`；ViewModel `viewModelId`；Enum `options, defaultValue`；Image / Trigger 无附加字段。ImagePropertyData 未暴露 defaultImage 不代表真实图片默认依赖不存在。

#### A.3 Node inventory（55 NodeType / 334 自有字段；保留原表格式）

每 NodeType 恰好一行，继承按 A.1 递归；TextBox 继承 Group，State / Channel 的具体附属类型不另造 NodeType。42 类静态图形 / 资源 / 布局，13 类动画 / SM / 绑定定义；后者仍登记宿主影响。

| NodeType | 模型类 | 声明文件 | 直接基类 | 自有字段（完整） | SVG 路径 / 计划归属 |
|---|---|---|---|---|---|
| Document | PAGXDocument | `include/pagx/PAGXDocument.h` | Node | `width, height, layers, animations, viewModel, dataBinds, nodes, errors, _fontConfig, layoutApplied, nodeMap, nodeSet, liveScenes, layersByImageFilePath, layersByImageFilePathBuilt` | 画布、结构、字体及依赖；宿主与缓存分类，S0 / S1 / S5 / S6 |
| PathData | PathData | `include/pagx/nodes/PathData.h` | Node | `_verbs, _points, _cachedBounds, _boundsDirty` | M/L/Q/C/Z 几何序列及精度；S4 |
| Image | Image | `include/pagx/nodes/Image.h` | Node | `data, filePath, runtimeImage` | 实际图片资源解析与嵌入；S5 |
| Composition | Composition | `include/pagx/nodes/Composition.h` | Node | `width, height, layers, animations, viewModel, dataBinds` | 模板实例展开、布局及宿主作用域；S1 / S5 / S6 |
| SolidColor | SolidColor | `include/pagx/nodes/SolidColor.h` | ColorSource | `color` | 单色与 alpha / 色彩空间；S5 |
| LinearGradient | LinearGradient | `include/pagx/nodes/LinearGradient.h` | Gradient | `startPoint, endPoint` | 渐变原生或退化常量；S5 |
| RadialGradient | RadialGradient | `include/pagx/nodes/RadialGradient.h` | Gradient | `center, radius` | 径向渐变与退化规则；S5 |
| ConicGradient | ConicGradient | `include/pagx/nodes/ConicGradient.h` | Gradient | `center, startAngle, endAngle` | 一般完整 paint bake，退化可常量；S2 / S5 |
| DiamondGradient | DiamondGradient | `include/pagx/nodes/DiamondGradient.h` | Gradient | `center, radius` | diamond bake / 常量，不用 radial 冒充；S2 / S5 |
| ImagePattern | ImagePattern | `include/pagx/nodes/ImagePattern.h` | ColorSource | `image, tileModeX, tileModeY, filterMode, mipmapMode, matrix, scaleMode` | 资源、独立轴、fit 与 sampling；S5 |
| ColorStop | ColorStop | `include/pagx/nodes/ColorStop.h` | Node | `offset, color` | 有序 stops、空间颜色与 alpha；S5 |
| Font | Font | `include/pagx/nodes/Font.h` | Node | `unitsPerEm, file, fileOriginal, glyphs, renderCache` | 字体来源、glyph 与 WOFF2 / 轮廓；S3 / S5 |
| Glyph | Glyph | `include/pagx/nodes/Font.h` | Node | `path, image, offset, advance` | design-space 字形与资源；S3 / S5 |
| Animation | Animation | `include/pagx/nodes/Animation.h` | Node | `duration, frameRate, loop, objects` | TemplateStatic 排除驱动；M 阶段 / S0 / S6 |
| AnimationObject | AnimationObject | `include/pagx/nodes/AnimationObject.h` | Node | `target, channels` | 目标作用域与定义；S0 / S1 / S6 / M1 |
| Channel | Channel | `include/pagx/nodes/Channel.h` | Node | `name` | TypedChannel 附属登记；静态不求值；S0 / S6 / M3 |
| StateMachine | StateMachine | `include/pagx/nodes/StateMachine.h` | Node | `inputs, regions` | 宿主排除 / 明确状态来源；S0 / S6 |
| StateRegion | StateRegion | `include/pagx/nodes/StateRegion.h` | Node | `name, initialState, states, transitions` | 初态 / 并行 region；S0 / S1 / S6 |
| State | State | `include/pagx/nodes/State.h` | Node | `name` | AnimationState 附属登记；S0 / S6 |
| StateTransition | StateTransition | `include/pagx/nodes/StateTransition.h` | Node | `from, to, duration, frameRate, interpolation, bezierOut, bezierIn, enableEarlyExit, pauseOnExit, conditions` | 事件 / 转场排除，不伪造 SMIL；S0 / S6 |
| TransitionCondition | TransitionCondition | `include/pagx/nodes/TransitionCondition.h` | Node | `inputName, op, valueBool, valueNumber` | 条件类型与默认值；S0 / S6 |
| StateMachineInput | StateMachineInput | `include/pagx/nodes/StateMachineInput.h` | Node | `name, type, defaultBool, defaultNumber` | 宿主 persistent 输入 / trigger；S0 / S6 |
| Layer | Layer | `include/pagx/nodes/Layer.h` | Node, LayoutNode | `name, visible, alpha, blendMode, x, y, z, rotationX, rotationY, rotationZ, scaleX, scaleY, scaleZ, skewX, skewY, perspective, matrix, matrix3D, anchor, anchorZ, preserve3D, antiAlias, groupOpacity, passThroughBackground, scrollRect, hasScrollRect, clipToBounds, mask, maskType, composition, compositionFilePath, externalDoc, timelines, vmContext, contents, styles, filters, children, layout, gap, flex, padding, alignment, arrangement, includeInLayout, importDirective` | 46 个自有字段按 B.1 分组；S1 / S2 / S5 / S6 |
| DropShadowStyle | DropShadowStyle | `include/pagx/nodes/DropShadowStyle.h` | LayerStyle | `offsetX, offsetY, blurX, blurY, color, showBehindLayer, spread` | Below 与真实来源 / blend；S2 |
| InnerShadowStyle | InnerShadowStyle | `include/pagx/nodes/InnerShadowStyle.h` | LayerStyle | `offsetX, offsetY, blurX, blurY, color, spread` | Above 与顺序；S2 |
| BackgroundBlurStyle | BackgroundBlurStyle | `include/pagx/nodes/BackgroundBlurStyle.h` | LayerStyle | `blurX, blurY, tileMode` | backdrop 与 no-op；S2 |
| NoiseStyle | NoiseStyle | `include/pagx/nodes/NoiseStyle.h` | LayerStyle | `mode, size, density, seed, color, firstColor, secondColor, opacity` | Above 三模式与来源；S2 |
| GlassStyle | GlassStyle | `include/pagx/nodes/GlassStyle.h` | LayerStyle | `refraction, depth, frost, dispersion, splay, lightAngle, lightIntensity` | own geometry、背景与 SDF / UDF；S2 |
| BlurFilter | BlurFilter | `include/pagx/nodes/BlurFilter.h` | LayerFilter | `blurX, blurY, tileMode` | 双轴与边缘采样；S2 |
| DropShadowFilter | DropShadowFilter | `include/pagx/nodes/DropShadowFilter.h` | LayerFilter | `offsetX, offsetY, blurX, blurY, color, shadowOnly` | 当前输入 shadow，有序 output；S2 |
| InnerShadowFilter | InnerShadowFilter | `include/pagx/nodes/InnerShadowFilter.h` | LayerFilter | `offsetX, offsetY, blurX, blurY, color, shadowOnly` | 内侧覆盖与 output；S2 |
| BlendFilter | BlendFilter | `include/pagx/nodes/BlendFilter.h` | LayerFilter | `color, blendMode` | ColorFilter source / dest / alpha；S2 / S5 |
| ColorMatrixFilter | ColorMatrixFilter | `include/pagx/nodes/ColorMatrixFilter.h` | LayerFilter | `matrix` | 20 项与有序输入；S2 |
| NoiseFilter | NoiseFilter | `include/pagx/nodes/NoiseFilter.h` | LayerFilter | `mode, size, density, seed, blendMode, color, firstColor, secondColor, opacity` | 当前输入三 mode；S2 |
| Rectangle | Rectangle | `include/pagx/nodes/Rectangle.h` | Element, LayoutNode | `position, size, roundness, reversed` | 原生 / path、退化与方向；S4 |
| Ellipse | Ellipse | `include/pagx/nodes/Ellipse.h` | Element, LayoutNode | `position, size, reversed` | oval / path、起点与描边；S4 |
| Polystar | Polystar | `include/pagx/nodes/Polystar.h` | Element, LayoutNode | `position, type, pointCount, outerRadius, innerRadius, rotation, outerRoundness, innerRoundness, reversed` | 最终 path、分数点与布局；S4 |
| Path | Path | `include/pagx/nodes/Path.h` | Element, LayoutNode | `data, position, reversed` | 数据引用、统一 scale 与方向；S4 |
| Text | Text | `include/pagx/nodes/Text.h` | Element, LayoutNode | `text, position, fontFamily, fontStyle, fontSize, letterSpacing, fauxBold, fauxItalic, textAnchor, glyphRuns, baseline, glyphData, textBounds, textScale` | authored / layout glyph；S3 |
| Fill | Fill | `include/pagx/nodes/Fill.h` | Element | `color, alpha, blendMode, fillRule, placement` | 捕获 geometry 的 Painter；S1 / S4 / S5 |
| Stroke | Stroke | `include/pagx/nodes/Stroke.h` | Element | `color, width, alpha, blendMode, cap, join, miterLimit, dashes, dashOffset, dashAdaptive, align, placement` | 原轮廓、dash / clip / fit；S4 / S5 |
| TrimPath | TrimPath | `include/pagx/nodes/TrimPath.h` | Element | `start, end, offset, type` | Separate / Continuous 与完整 scope；S4 |
| RoundCorner | RoundCorner | `include/pagx/nodes/RoundCorner.h` | Element | `radius` | 累积 geometry 圆角；S4 |
| MergePath | MergePath | `include/pagx/nodes/MergePath.h` | Element | `mode` | 合并 final geometry、清旧 Painter；S4 |
| TextModifier | TextModifier | `include/pagx/nodes/TextModifier.h` | Element | `anchor, position, rotation, scale, skew, skewAxis, alpha, fillColor, strokeColor, strokeWidth, selectors` | 最终 glyph 矩阵 / 样式 / 多 paint；S3 |
| TextPath | TextPath | `include/pagx/nodes/TextPath.h` | Element, LayoutNode | `path, baselineOrigin, baselineAngle, firstMargin, lastMargin, perpendicular, reversed, forceAlignment` | 沿 path 最终 glyph；S3 |
| TextBox | TextBox | `include/pagx/nodes/TextBox.h` | Group | `textAlign, paragraphAlign, writingMode, lineHeight, wordWrap, overflow` | 容器 / modifier 两模式；S3 |
| Group | Group | `include/pagx/nodes/Group.h` | Element, LayoutNode | `anchor, position, rotation, scale, skew, skewAxis, alpha, elements, padding` | geometry / Painter 分离；S1 / S4 |
| Repeater | Repeater | `include/pagx/nodes/Repeater.h` | Element | `copies, offset, order, anchor, position, rotation, scale, startAlpha, endAlpha` | geometry / Painter 复制与顺序；S4 |
| RangeSelector | RangeSelector | `include/pagx/nodes/RangeSelector.h` | TextSelector | `start, end, offset, unit, shape, easeIn, easeOut, mode, weight, randomOrder, randomSeed` | glyph factor 与稳定随机；S3 |
| GlyphRun | GlyphRun | `include/pagx/nodes/GlyphRun.h` | Node | `font, fontSize, glyphs, x, y, xOffsets, positions, anchors, scales, rotations, skews, bounds` | 12 项、7 数组、真实 glyph；S3 |
| ViewModel | ViewModel | `include/pagx/nodes/ViewModel.h` | Node | `properties` | schema 与默认宿主；S0 / S6 |
| ViewModelProperty | ViewModelProperty | `include/pagx/nodes/ViewModelProperty.h` | Node | `name, propertyType, dataConverter, viewModelRef, defaultNumber, minValue, maxValue, defaultString, defaultBoolean, defaultColor, defaultImage, enumOptions, defaultEnum` | 八类型默认 / 嵌套 / 约束元数据；S0 / S5 / S6 |
| DataBind | DataBind | `include/pagx/nodes/DataBind.h` | Node | `source, target, channel, direction` | Once / 持续 / 回写与 scope；S0 / S6 |
| DataConverter | DataConverter | `include/pagx/nodes/DataConverter.h` | Node | `converterType, params` | 内建 / 开放注册；S0 / S6 |

### B. 按字段运行语义与直接验收

分组仅合并同一消费路径，不删字段语义；以下验收均是未来任务。状态 / policy 权威定义只在正文，宿主表只登记字段事实。

#### B.1 文档、布局、Layer 与资源

| 节点 / 字段组 | PAGX 实际语义与 SVG 现状 | 表达 / 修复方案与直接验收 |
|---|---|---|
| Node.id | 文档身份；当前主要 Layer 保留，generated 不避碰 | S1：作者 grad0、实例 / 外部同名 / Mask 后缀碰撞，全引用改写 |
| Node.customData / sourceLine；Layer.name | 非视觉元数据 / 来源 / 名称，根 parser 特例，bake 可丢标签 | S6：全节点保留或显式排除，根 / Layer / resource / bake、转义、customData 排序、sourceLine=-1 |
| Document.width / height / layers | 固定画布与顶层顺序，已有根尺寸 / viewBox | S1：正尺寸、有效空、负 / 非有限契约；重叠换序、顶层百分比 |
| Document.nodes / errors | nodes 是所有权不是舞台；errors 非空仍可返回文档 | S0/S6：有效结构 / 不可达资源分类，严格错误不因非空 SVG 成功 |
| Document._fontConfig / layoutApplied | 字体输入与布局状态；applyLayout(null) 用文档 font，但 SVG 可另用系统 | S3：有效配置同源，doc-only 注册、合法冷热 / 外部字体；`src/pagx/svg/SVGExporter.cpp:3678` |
| Document.nodeMap / nodeSet / liveScenes / layersByImageFilePath / layersByImageFilePathBuilt | 索引、归属、生命周期与查找缓存 | S6：合法冷热 / 多 scene、资源变化 notifyChange 所属 doc；不输出 cache，外部串行化 |
| LayoutNode.left / right / top / bottom / centerX / centerY | 普通位置 center→双边→起边→末边，尺寸双边优先；flow 另算法 | S1：各派生类两轴全分支、冲突、非零 origin / padding，不重造约束 |
| LayoutNode.percentWidth / percentHeight / width / height | XML 百分比解析到 percent；双边→百分比→preferred，Path / Text 统一 scale | S1/S3/S4：10 项默认 / 单设 / 冲突；primitive size 区分、NaN unset |
| LayoutNode.preferredX / preferredY / preferredWidth / preferredHeight / layoutX / layoutY / layoutWidth / layoutHeight | 测量 / 布局产物，0 与 NaN 不同；尺寸 ceil、位置 round | S1：render accessor 只消费一次，重布局、先 bounds 再改、负 origin / 统一 scale |
| Layer.visible | 普通 hidden 省略；hidden 且自身有 mask 可泄漏 | S1：hidden owner 与 Mask 根特殊覆盖区分；外父舞台 Mask 不额外绘制 |
| Layer.alpha / groupOpacity / blendMode / passThroughBackground | 整体 / 逐项、离屏受 filter / pixel Mask / blend / 背景隔离影响 | S1/S2：18 blend 各上下文、两级 alpha、半透明重叠、强制离屏与隔离；不等价则完整 bake / 拒绝 |
| Layer.x / y / z / rotationX / rotationY / rotationZ / scaleX / scaleY / scaleZ / skewX / skewY / perspective / matrix / matrix3D / anchor / anchorZ | 非恒等 3D 优先；非恒等 2D 覆 x/y 与组件但 Lift 叠 z；anchor 后作用、layout 外部；Group anchor 不同 | S1/S2：每分量、identity / 非 identity、零 / 负 scale、masked x/y、anchor depth、constant w / 透视；`src/pagx/utils/LayerTransformUtils.cpp:93` |
| Layer.preserve3D / antiAlias | effect / mask / scroll flatten；已有一般透视 / 有效 preserve3D bake，原生 AA 未对齐 | S2：交叉深度逐项加 flatten、AA=false 斜边 / clip；仅可证 affine 保矢量 |
| Layer.scrollRect / hasScrollRect / clipToBounds | 视口与负 offset；layout 生成 rect，当前 authored / generated 未分导致旧尺寸 | S1：explicit 优先、flag / rect 组合、首次 / 二次尺寸变化；源头失效；`src/pagx/nodes/Layer.cpp:289` |
| Layer.mask / maskType | 三覆盖、实例相对坐标；stage hidden Mask 仍覆盖 | S1/S4：三 type、根 alpha、composition / scroll / effects、描边环 / 洞、共同祖先 / 子 Mask / 双实例 |
| Layer.composition / contents / children | runtime 选 composition 后不同时画 contents；SVG 当前双画；children / placement 有序 | S1：两内容冲突、子层 / foreground 顺序，按实际有效优先级；`src/renderer/LayerBuilder.cpp:1177` |
| Layer.compositionFilePath / externalDoc | 未供应与已加载所属 doc；externalDoc 非普通 cache | S1/S5：外部同名 / 递归、加载与未加载、Font / Image / VM 所属归属，WOFF2 不只根 nodes |
| Layer.styles / filters | Below / Above 作者顺序、真实输入 bucket、foreground 后有序 filter | S2：B.3 全字段与 C07—C11，不只标签 |
| Layer.layout / gap / flex / padding / alignment / arrangement / includeInLayout | flow 消解，padding 改框，exclude flow 不 hidden；hidden include 可占位 | S1：3×4×6、0/1/多 child、explicit / percent、flex 1:2、exclude 仍画 |
| Layer.importDirective.source / format / content / resolvedFrom | source / inline 先 resolve，resolvedFrom 不重导入；CLI 拒 unresolved、SDK 不统一 | S0/S5/S6：SDK / CLI 同前置、source+content 冲突、无扩展名+format、metadata 转义；不 raw passthrough |
| Document / Composition.animations / viewModel / dataBinds；Layer.timelines / vmContext | 定义、挂载、宿主；默认 VM 能改首次 draw | S0/S6：模板≠首 key≠默认绑定；两 vmContext、未知 driver / target、nested 分类，状态按 D7 |
| Composition.width / height / layers | intrinsic / 内部约束，instance 尺寸不自动两倍拉伸或裁剪 | S1：模板100 / instance200、right / bottom、双父空间、Mask 与 bake lookup |
| Image.data / filePath / runtimeImage | bytes、路径 / URL / data URI、宿主 decoded image，消费者链不同 | S5：真实4格式、损坏 / 缺失、GPU-only readback、红 data / 蓝 runtime；不擅自联网 |
| Font.unitsPerEm / file / fileOriginal / glyphs / renderCache | design scale、源字体 / 原路径 / 实际子集，cache 非已嵌入 | S3/S5：注册 / embed、empty / blank / null / mixed、6400 PUA、external Font；合法同输入冷热 / 新对象，不热改 glyphs |
| Glyph.path / image / offset / advance | outline / bitmap + design offset / advance；blank 无墨迹可有 advance | S3：vector / bitmap offset、blank 与 GID0 / 越界、path+image、Font 类型一致、WOFF2 量化 |
| PathData._verbs / _points / _cachedBounds / _boundsDirty | M/L/Q/C/Z，实际消费 bounds 与 coverage 分开；getBounds 包控制点，%g 精度有限 | S4：每 verb 点数、多 contour、远 Q/C 控制点、大坐标小差、共享 / 先 bounds 后 transform；`src/pagx/svg/SVGPathParser.cpp:31` |

根 ParseDocument 只取尺寸 / customData，普通 makeNodeFromXML 身份路径不同；不能由 SVG 补出从未保存的根 id / sourceLine（`src/pagx/PAGXImporter.cpp:3115`）。

#### B.2 几何、Painter、文字与颜色

| 节点 / 字段组 | renderer 实际语义与现状 | SVG方案、阶段和直接测试 |
|---|---|---|
| Group.anchor / position / rotation / scale / skew / skewAxis / alpha / elements / padding | geometry 上行带 final matrix，内 Painter 才带 alpha；padding / layout 不自动 clip | S1/S4：内外 Painter、非均匀 scale / skewAxis、多 paint、后 modifier、capture / final 区分 |
| Rectangle.position / size / roundness / reversed | 中心、resolved size、圆角 / 方向；零 / 负轴微轮廓可 stroke | S4：rect 可等价才原生；起点 / 薄轴 / 厚 stroke / Trim / dash 转 path，NaN 默认位置单列 |
| Ellipse.position / size / reversed | oval 顶部起点，方向影响 dash / measure，退化微轮廓 | S4：圆 / 非圆、两方向 / 零轴、内外 stroke / round cap / 约束，非 SVG 右侧起点 |
| Path.data / position / reversed | layout 统一 scale，stroke 在 Painter 空间；reversed 改 geometry | S4/S5：data origin、renderScale、Q/C fit / coverage、dash、空 / 共享，不能仅 transform 改 paint 空间 |
| Polystar.position / type / pointCount / outerRadius / innerRadius / rotation / outerRoundness / innerRoundness / reversed | Polygon floor 与 Star 分数点不同，radius 受 layout scale | S4：两 type、整 / 分数 / 少点、全部半径 / 圆角 / 旋转 / 方向；不能关闭解析丢形 |
| Fill.color / alpha / blendMode / fillRule / placement | ColorSource 引用、null 黑；捕获 geometry，不是最终标签 | S1/S4/S5：多 fill、两 placement、18 blend、两 rule、前后 modifier、透明 / null / missing pattern 不黑占位 |
| Stroke.color / width / alpha / blendMode / cap / join / miterLimit / dashes / dashOffset / dashAdaptive / align / placement | width 非布局宽，≤0 无 stroke；奇数 dash 复制并按 cap / width 简化，align 倍宽 clip，fit 原 bounds | S4/S5：全 enum、奇 / 零 / adaptive dash、miter 临界、open / 洞 / glyph、多 Painter、捕获前后 scale |
| TrimPath.start / end / offset / type | start/end 归一、offset 度；Separate 逐 geometry、Continuous 累计；Text 可转 Shape | S4：多 / 空 / 零长 / reversed range / ±offset、Text / Group / Repeater、已捕获 Painter 后 Trim |
| RoundCorner.radius | 累积 geometry corner effect 含 Text→Shape，≤0 no-op | S4：open 端、已圆角、glyph / Group、Trim / Merge 顺序，no-op 保 geometry |
| MergePath.mode | final matrix 上 Append / boolean，清旧 geometry / Painter 后放 merged；resolver 未清 Painter | S4：五 mode / contour 方向，Rect→Fill→Rect→Merge→Stroke 只后 paint；Merge 后无 Painter 不画、Text / Group |
| Repeater.copies / offset / order / anchor / position / rotation / scale / startAlpha / endAlpha | ceil、progress=i+offset、scale 幂、末 copy fractional alpha；复制 / 重绑定 Painter | S4：两 order 真重叠 / alpha、0 / 负 / 分数 copies、整数负幂、anchor / 连续 Repeater / 外 Painter；负底数分数幂单独边界，不自造 SignedPow |
| Text.text / position / fontFamily / fontStyle / fontSize / letterSpacing / textAnchor / baseline | readable 输入、font shaping、baseline / origin；有效 layoutRuns / glyphRuns 优先 | S3：同源 fontConfig、两 baseline / 三 anchor、tracking、space / tab / 空行 / CJK justify / BiDi；Native 损失独立 |
| Text.fauxBold / fauxItalic | 合成样式不等于选真实 face，Native CSS 不证 path 等价 | S3：真实 style × 四 faux，outline / advance / bounds / Mask，Native / WOFF2 / path 分验 |
| Text.glyphRuns / glyphData / textBounds / textScale | authored embed、glyphData.layoutRuns shaping、派生 bounds / scale | S3：无 embed Fidelity 仍用 layoutRuns；不 Native 假保真；合法冷热 / 重布局 / 约束字号 / valid empty，scale 一次 |
| GlyphRun.font / fontSize / glyphs / x / y / bounds | 各 run font / size，GID0 missing 与越界 / blank 区分；bounds 通常首 run 整 Text linebox | S3：多 Font / size、GID0 / 越界 / 空白、非首 bounds、constraint scale，不首 font 覆全部 |
| GlyphRun.xOffsets / positions | 每索引 positions 优先再加 xOffset；两者缺用 default cursor，三出口推进不同 | S3：7 数组 empty / short / full / extra；首 glyph 有 explicit position 后第二 glyph 对齐 runtime，不强制等长 |
| GlyphRun.anchors / scales / rotations / skews | advance 中心+pivot，绕 glyph 原点组合，不是 SVG 字符 rotate | S3：非零 run origin / anchor / rotate / skew / 负 scale / vector offset，bitmap / vector 交错保持次序 |
| TextBox.textAlign / paragraphAlign / writingMode / lineHeight / wordWrap / overflow | 有 elements 容器 / 空 elements modifier；logical axes，Hidden 丢整行 / 列，resolved 优先 | S3：6 项全分支、constraint 覆 author、all/some/none overflow、glyph-only / 非 Text / foreground；valid empty 不画原文 |
| TextPath.path / baselineOrigin / baselineAngle / firstMargin / lastMargin / perpendicular / reversed / forceAlignment | 以tgfx实际求值为准，normal不消费last；force长度L+last-first，先reverse再closed回绕，open切线延伸，perpendicular只改旋转，首contour | S3：第2.8.2节TP01—TP16给定向量glyph、水平/竖直/闭合path、anchor/矩阵、margin/法向/基准/layout数字期望，不使用AE作替代基准 |
| TextModifier.anchor / position / rotation / scale / skew / skewAxis / alpha | scope 全局 glyph index；signed factor transform、abs factor alpha/paint；scale=1+(scale-1)*factor | S3：跨 Text/run、factor -1/0/部分/1、modifier 顺序、已有 Painter 后改、共用 final matrix |
| TextModifier.fillColor / strokeColor / strokeWidth / selectors | optional unset≠0；按absFactor累积override权重；0只base、部分完整base再SrcOver overlay、1只overlay，strokeWidthFactor独立 | S3/S1：第2.8.1节公式/数值/排他Painter capture为固定契约；同Painter旧draw被替换，局部Fill不再次进入祖先累计几何，多合法Painter各保一次 |
| RangeSelector.start / end / offset / unit / shape / easeIn / easeOut / mode / weight / randomOrder / randomSeed | glyph 数非原文长；Index 仅 start/end 归一后 offset 直接加；Triangle ease、6 shape / 6 mode / 首 selector、seed uint16 | S3：2×6×6、顺序 / 首 Subtract、连字 / combining / RTL、0/大 seed、swap start/end、factor 与像素 |
| SolidColor.color | RGBA / ColorSpace，P3 分量不直接当 sRGB fallback | S5：两空间、三 alpha、转换 / 量化 / blend 底色、固定目标 |
| Gradient.matrix / colorStops / fitsToGeometry | Painter 空间 / 每 geometry fit；default / 单色 / 重复 / 单调收口；空间 alpha / premultiply 非时间 LerpColor | S5：empty / one / repeat / missing ends / nonmonotonic、退化轴 / mixed space / 透明红蓝；实际 fit 非随意 tight |
| LinearGradient.startPoint / endPoint | linear，同 / 近端点 shader 首色，非注释 end color | S5：正常 / 相等 / 阈值两侧、默认 stop 再退化、matrix / fit |
| RadialGradient.center / radius | 非圆 fit 生成 ellipse gradient；近0末色、单 stop 自身色、负无效 | S5：center 非独立 focus、非圆 bbox、0 / 临界 / 负 radius / matrix / fit，不补造 fx/fy |
| ConicGradient.center / startAngle / endAngle | sweep、角区间 / 近等首色，现 bake / radial fallback | S2/S5：正 / 反 / 部分角、近等 / 单 stop / matrix / fit / Mask；退化常量不强制整层 bake |
| DiamondGradient.center / radius | diamond 空间、近0末色、负无效，现 bake / radial fallback | S2/S5：四角、退化 / 临界 / matrix / fit / Mask，仅 BestEffort 明示 radial |
| ImagePattern.image / matrix / scaleMode | fit 根据 matrix 后 rect 非仅 natural size；None 连续，其他 fit geometry | S5：4 fit、旋转 / skew / 负 scale、非零 bbox、多格式 / runtimeImage，各 tile 同 fit |
| ImagePattern.tileModeX / tileModeY / filterMode / mipmapMode | 独立 4×4 / sampling2×3；runtime 已传，原生 / RenderTiledPattern 漏采样 | S5/S2：16×6、放大 / 缩小 / 旋转高频 / 透明边缘；image-rendering 非 mipmap 证明 |
| ColorStop.offset / color | builder 空列表默认，规范化 / 各位置 colorSpace / alpha | S5：逐分量、缺边界 / 重复 / 乱序契约、透明渐变内部，不只端点 |

复用依据：TextPath `third_party/tgfx/src/layers/vectors/TextPath.cpp:170`；Selector `third_party/tgfx/src/layers/vectors/TextSelector.cpp:320`、`third_party/tgfx/src/layers/vectors/TextSelector.cpp:436`；TextModifier `third_party/tgfx/src/layers/vectors/TextModifier.cpp:177`。最终 glyph paint 不能只读 Text 字段还原。

fit 依据：shape 的 `third_party/tgfx/src/layers/vectors/FillStyle.cpp:51`、`third_party/tgfx/src/layers/vectors/StrokeStyle.cpp:55` 用 Path.getBounds；`third_party/tgfx/src/core/PathRef.cpp:42` 包括全部 Bezier 控制点，不是 tight bounds。glyph 的 `third_party/tgfx/src/layers/vectors/FillStyle.cpp:73`、`third_party/tgfx/src/layers/vectors/StrokeStyle.cpp:72` 用 textBlob.getTightBounds。Q/C 远控制点与 glyph 保守 / 紧边界分别验证，layout / coverage / bake 另算。

#### B.3 全部 effects 字段

Style 继承 Node 与 LayerStyle；filter 不继承 style 的 blend / exclude。excludeChildEffects 选择真实源，不能从最终 SourceGraphic 事后删除；inactive / no-op / invalid / unsupported 分开。

| 节点 / 字段组 | 实际语义 / 当前覆盖 | SVG方案与直接测试（S2，颜色协同S5） |
|---|---|---|
| LayerStyle.blendMode / excludeChildEffects | 各位置 / 背景，选择含 / 不含 child effects 输入；多处未消费 | 18 blend × exclude，child-only / 子 shadow / own；content / contour / own geometry 三来源，非统一 SourceAlpha |
| BlurFilter.blurX / blurY / tileMode | 双轴 / contentScale，default Decal；仅 stdDeviation 漏 tile | 单 / 双 / 零、4 tile、边缘、巨大 halo / 连续 blur；原生验证后用，不全透明延伸 |
| DropShadowFilter.offsetX / offsetY / blurX / blurY / color / shadowOnly | 上一步输入 shadow，Only / 合成为新 output；RGBA / space；现固定 SourceAlpha 不推进 | ±offset、各向异性、两 Only、半透明/P3、Blur→Shadow、Shadow→Blur/Matrix/Noise、连续 Only，非末尾 merge |
| InnerShadowFilter.offsetX / offsetY / blurX / blurY / color / shadowOnly | 当前输入内侧及 Only；无 spread / style blend | 同上加洞 / alpha 边、Inner / Drop 换序、后矩阵处理完整 output |
| ColorMatrixFilter.matrix | 20 项；bias 改透明区 alpha，values 已有但链仍须修 | 20 one-hot / bias、RGBA 换、alpha bias / 灰 / 透明黑 / nonfinite / shadow-noise 前后、region |
| BlendFilter.color / blendMode | 常量 source / 当前 pixel dest 可改 alpha；现额外 in clip，Plus 无 mode | 18 mode × source/dest 0/0.5/1、非交换色/P3/洞；验证 extra clip，Plus 不 silent Normal |
| NoiseFilter.mode / size / density / seed / blendMode | 三 mode、1/(size*scale)、中心 phase / 上游 offset，density0 返回 input，现 bucket 残留 | 三 mode、0/阈值 density、无效size/no-op、float seed、前 Blur 后不漂、18 blend；不能等价 Perlin 则完整 bake |
| NoiseFilter.color / firstColor / secondColor / opacity | Mono color、Duo 两色、Multi opacity；inactive 不影响，Duo SrcOver 非 SVG screen | 活跃逐项 / inactive 扰动、半透明双色 / alpha0、三 mode+Layer alpha / Mask |
| DropShadowStyle.offsetX / offsetY / blurX / blurY / color / showBehindLayer / spread | Below；behind=false 剔 contour 内 shadow；SpreadUtils shape/bounds 非自动 morphology | 两 behind、±/塌陷 spread、规则 / 不规则 / Text / child-only、align/P3/18blend/exclude，非等价 bake |
| InnerShadowStyle.offsetX / offsetY / blurX / blurY / color / spread | Above，spread 反向 / 塌陷可整块；无 Only / behind | ±spread / 洞 / 塌陷、Inner↔Noise 顺序、foreground、18blend/exclude、后 filters 整体 |
| BackgroundBlurStyle.blurX / blurY / tileMode | actual backdrop、default Mirror、双零 no-op；onDraw 用 Src 非普通 style blend，exclude 影响限定输入 | 4 tile / 两轴 / 半透明背景 / Mask-scroll，inactive blend 如实登记，no-op 不空 filter，消费前缀 |
| NoiseStyle.mode / size / density / seed / color / firstColor / secondColor / opacity | Above，density0 不画，Duo dark 后 bright 各 style blend，非预合成一次；default center phase | 全 mode 活跃/inactive、0 density、同位置双 Style、Inner↔Noise、exclude/foreground/18blend/P3/后 filters |
| GlassStyle.refraction / depth / frost / dispersion / splay / lightAngle / lightIntensity | 七 shader 参数 default80/20/5/50/0/45/80；frost0 有 sigma 下限，own geometry / SDF-UDF / 背景 / 光照 | 每项/boundary/default 非空、SDF-UDF、alpha0 geometry、bitmap 孔、child-only fallback、scale/blend/exclude；不普通 blur 冒充 |

构建 / 样式依据：`src/renderer/LayerBuilder.cpp:2809`、`src/renderer/LayerBuilder.cpp:3497`、`third_party/tgfx/src/layers/Layer.cpp:1805`、`third_party/tgfx/src/layers/Layer.cpp:1952`；Noise density / phase `third_party/tgfx/src/layers/filters/NoiseFilter.cpp:139`；Glass `include/pagx/nodes/GlassStyle.h:25`。Blur0 / density0 / single stop / 退化常量不扩大 bake，invalid Noise 不生成空 filter；effect P3 与 SolidColor 同源。

#### B.4 时间、宿主定义参数与 converter

以下保留定义全部字段和实际消费；正文状态契约不在此重复。开放宿主历史 U，有限事件只能按 D8 / MB。

| 节点 / 全字段组 | 运行时事实 | 分类验收（S0/S1/S6，目标协同S2/S3/S5及M） |
|---|---|---|
| Animation.duration / frameRate / loop / objects | 时间定义；空 channel T{}，首 / 末保持；模板不首 key apply | 模板 alpha≠首 key、三 loop、空/单/paused/offset、静态无 SMIL |
| AnimationObject.target / channels；Channel.name / TypedChannel.keyframes | binding 作用域，八实际 TypedChannel；PAGImage 非 key 实例 | unknown/跨域/external target、各类型 XML→evaluate→apply，imageRef≠PAGImage |
| Keyframe.time / value / interpolation / bezierOut / bezierIn；AnimationTimeline.animationId / playing / evaluationOffset；StateMachineTimeline.stateMachineId | 左 interpolation、当前 out/下一 in；playing 只 advance、offset phase | 四 interp、fps / ±offset、同 ID pause scope、no driver 不激活、空/单/duplicate/key 边界 |
| StateMachine.inputs / regions | 并行 region 与共享输入 | 根/Composition/external 全定义分类，非只 top warning |
| StateRegion.name / initialState / states / transitions；State.name / AnimationState.animationId | name 初态、未命中可首 state，空 animation 空状态 | 空/unknown initial、多 region/顺序；模板不 initial apply |
| StateTransition.from / to / duration / frameRate / interpolation / bezierOut / bezierIn / enableEarlyExit / pauseOnExit / conditions | any 优先、条件 AND、zero 可即时，new-over-old mix；无 exitTime | 全 bool/4interp/empty 条件，zero delta 可连跳，不假设 advance(0) no-op、不重写注释 |
| TransitionCondition.inputName / op / valueBool / valueNumber；StateMachineInput.name / type / defaultBool / defaultNumber | 按类型有效值，persistent Bool/Number≠Trigger | Bool Equal/NotEqual、Number 六 op、Trigger；inactive 默认扰动、模板不 transition |
| ViewModel.properties | schema 非 values，不被有效消费定义非视觉 | empty/multi/nested cycle、metadata 与宿主影响分开 |
| ViewModelProperty.name / propertyType / dataConverter / viewModelRef | 八类型、nested scope/converter；parent slot referenceInstance与Composition共享，open slot回填，全VM链接后才bind，非PropertyData替代 | source leaf/type、跨层source、open slot回填、父子共享值、多实例身份和unknown/open converter；`src/pagx/PAGScene.cpp:318`、`src/pagx/PAGScene.cpp:366` |
| ViewModelProperty.defaultNumber / defaultString / defaultBoolean / defaultColor / defaultImage / defaultEnum | type 活跃 default 可改 alpha/text/visible/color/image/enum；null image 可清空 | 八 default 异于模板、inactive、Once、默认 text reshape；模板主动排除 |
| ViewModelProperty.minValue / maxValue / enumOptions | number 范围 metadata，setter 不自动 clamp；enum index 非 string | 越 min/max、enum 越界、converter 后 type，不擅 clamp |
| DataBind.source / target / channel / direction | ToTarget/TwoWay dirty、Once、ToSource 回写；matrix/3D 绑定有拒绝 | 四方向、一源多目标、先 syncBack 后 update、nested/unknown/shared channel；`src/pagx/DataBindRuntime.cpp:127` |
| DataConverter.converterType / params | 开放注册、四内建；unknown passthrough 非已可重现默认宿主 | 全 params / mismatch / unknown/custom / nonfinite / 输出预算，不擅执行无限回调/循环 |

| 名称 | 全部内建参数与默认 | 正 / 反向运行规则及验收 |
|---|---|---|
| secondsToFrames | `frameRate=30` | 秒×fps / 帧÷fps，解析0回退30；float mismatch passthrough；缺省/0/非法/非有限 |
| priceFormat | `prefix="", suffix="", decimals=0` | float/int→string / 去前后缀解析float；特殊字符/负数/precision预算/mismatch |
| rangeMapper | `inputMin=0, inputMax=1, outputMin=0, outputMax=100` | 正反线性不 clamp；zero 区间取 output/input min，范围外 / 反向 |
| degsToRads | 无内建参数 | float 度 / 弧互转；mismatch passthrough |

Converter 依据 `src/pagx/DataConverterRegistry.cpp:37`；AnyStateName="any" 是保留字符串，params / converterType 是开放集而非 enum。

### C. 完整 enum（40 类 / 155 值，NodeType 另见 A.3）

每值是真实模型名，存在映射字符串不代表视觉支持；bool / optional / 数组边界遵循正文验收。

| 枚举 | 完整值 | 直接测试 / 阶段 |
|---|---|---|
| LayoutMode | `None, Horizontal, Vertical` | 0/1/多 child、intrinsic/explicit/flex；S1 |
| Alignment | `Start, Center, End, Stretch` | 交叉轴 explicit/percent/unset；S1 |
| Arrangement | `Start, Center, End, SpaceBetween, SpaceEvenly, SpaceAround` | 剩余空间/gap/单 child；S1 |
| MaskType | `Alpha, Luminance, Contour` | root alpha/stroke/洞/cross-parent；S1/S4 |
| BlendMode | `Normal, Multiply, Screen, Overlay, Darken, Lighten, ColorDodge, ColorBurn, HardLight, SoftLight, Difference, Exclusion, Hue, Saturation, Color, Luminosity, PlusLighter, PlusDarker` | 18值分Layer/Fill/Stroke/BlendFilter/Noise/Style真实透明背景，Plus不silent Normal；S1/S2/S5 |
| FillRule | `Winding, EvenOdd` | contour方向/重叠/boolean/clip-rule；S4 |
| LayerPlacement | `Background, Foreground` | children/composition/Style/TextBox多遍；S1/S3 |
| LineCap | `Butt, Round, Square` | open/zero segment/dash；S4 |
| LineJoin | `Miter, Round, Bevel` | miter临界/厚stroke；S4 |
| StrokeAlign | `Center, Inside, Outside` | primitive/path/glyph/洞/dash；S4 |
| PolystarType | `Polygon, Star` | floor/分数pointCount/radius/round；S4 |
| TrimType | `Separate, Continuous` | 长度/多contour/外scope；S4 |
| MergePathMode | `Append, Union, Intersect, Xor, Difference` | append保子路径/清Painter/非交换顺序；S4 |
| RepeaterOrder | `BelowOriginal, AboveOriginal` | 重叠/不同copy alpha；S4 |
| PathVerb | `Move, Line, Quad, Cubic, Close` | 点数1/1/2/3/0、多contour/精度；S4 |
| TextAlign | `Start, Center, End, Justify` | CJK/tab/space/末行/logical axis；S3 |
| ParagraphAlign | `Near, Middle, Far` | 双writing axes/padding/resolvedbox；S3 |
| WritingMode | `Horizontal, Vertical` | CJK/Latin/标点/BiDi；S3 |
| Overflow | `Visible, Hidden` | none/some/all line丢弃；S3 |
| TextAnchor | `Start, Center, End` | standalone/TextBox覆盖/RTL；S3 |
| TextBaseline | `LineBox, Alphabetic` | 同position两baseline；S3 |
| SelectorUnit | `Index, Percentage` | Index归一/offset；S3 |
| SelectorShape | `Square, RampUp, RampDown, Triangle, Round, Smooth` | 每shape/ease活跃；S3 |
| SelectorMode | `Add, Subtract, Intersect, Min, Max, Difference` | 首selector/双selector顺序；S3 |
| ColorSpace | `SRGB, DisplayP3` | single/mixed/alpha/空间采样；S5 |
| TileMode | `Clamp, Repeat, Mirror, Decal` | image16axis、blur/backdrop4；S2/S5 |
| FilterMode | `Nearest, Linear` | 放大/旋转/alpha边；S5 |
| MipmapMode | `None, Nearest, Linear` | 缩小高频/6sampling；S5 |
| ScaleMode | `None, Stretch, LetterBox, Zoom` | transform后fit/各tile同fit；S5 |
| NoiseMode | `Mono, Duo, Multi` | color/两color/opacity活跃/inactive；S2 |
| LoopMode | `Once, Loop, PingPong` | 静态主动排除、M content time |
| TimelineType | `Animation, StateMachine` | 挂载/paused/offset/nested；S0/S6/M1 |
| KeyframeInterpolationType | `None, Linear, Bezier, Hold` | 静态不apply、连续/离散；M3 |
| ChannelValueType | `Float, Bool, Int, String, ImageRef, Color, Matrix, PAGImage, Matrix3D` | 九alternative、PAGImage宿主非TypedChannel；S0/S6/M1 |
| StateType | `Animation` | AnimationState/空state，不造BlendState；S0/S6 |
| StateMachineInputType | `Bool, Number, Trigger` | persistent default/event；S0/S6 |
| TransitionConditionOp | `Equal, NotEqual, LessThan, LessThanOrEqual, GreaterThan, GreaterThanOrEqual, Trigger` | 按input合法子集/AND/any/default；S0/S6 |
| ViewModelPropertyType | `Number, String, Boolean, Color, Image, ViewModel, Enum, Trigger` | 八type/default/资源；S0/S5/S6 |
| DataBindDirection | `ToTarget, ToSource, TwoWay, Once` | 初始化/回写/Once，非简单time；S0/S6 |
| ChannelFlags | `None, Animatable, RequiresLayout` | bitmask正交、None/未反射仍静态有效；S0/S6 |

### D. 完整组合矩阵

闭合 enum 逐值；连续参数按边界 / 性质 / 误差界，不承诺无限笛卡尔积。静态 C 与动态 A 分开，组合不能被逐字段孤立测试替代。

#### D.1 静态 C01—C24

| 组合编号 | 必测交互 | 检测标准 / 阶段 |
|---|---|---|
| C01 | 每LayoutNode派生×约束/percent/explicit/intrinsic×padding×合法cold/warm/re-layout | position/size/scale一次，NaN unset≠0产物；S1/S3/S4 |
| C02 | Layer matrix3D/matrix/components×anchor×layout×scroll | 全分量/pixel、matrix保z、3D覆z、flatten；S1/S2 |
| C03 | 同Composition双实例×cross-parentMask×external同名×ID | 实例/父空间/no重复ID/引用正确；S1/S5 |
| C04 | stage owner/Mask根/后代visible×alpha×三Mask×scroll/effect/composition | hidden owner不漏、根特例、Contour stroke/洞；S1/S2/S4 |
| C05 | Group上行geometry×内外Painter×前后scale/skew×gradient/pattern/Stroke | capture inner/final分离、paint/width/bounds；S1/S4/S5 |
| C06 | geometry→Painter→modifier→Painter×Group/Text/Repeater/Merge | Painter lifetime/reference更新/Merge清/复制序与alpha；S1/S4 |
| C07 | 两级groupOpacity×18blend上下文×filter/pixelMask/背景隔离 | direct/offscreen一致、半透明重叠直接pixel；S1/S2 |
| C08 | Below顺序×Above顺序×children×foreground×后filter | 不按Style类型重排、真实bucket；S1/S2 |
| C09 | Blur/Drop/Inner/Matrix/Blend/Noise非交换链×Only×outputOffset | 前output→后input、bounds累计、不末尾merge；S2 |
| C10 | excludeChildEffects×own/child-only×child effects×spread/behind | content/contour/geometry三源、spread实际规则；S2/S4 |
| C11 | Glass/backdrop×alpha0 own/bitmap孔/SDF-UDF×祖先foreground×bake | backdrop/coverage/halo、无二次背景、consume前缀；S2/S5 |
| C12 | layoutRuns/authored runs×多Font/size×短数组×faux/anchor | placement/paint、Font/GID/advance/bounds；S3 |
| C13 | TextBox两模式×nestedGroup/Painter/foreground×resolvedsize×overflow | glyph-only/非text/valid empty不画原文；S3 |
| C14 | TextPath normal/force×open/closed×margin×Selector×Modifier序 | final glyph/normal/顺序/base-overlay；S3 |
| C15 | 2unit×6shape×6mode×连字/RTL/multiText×seed/weight | factor/首selector/random稳定/活跃；S3 |
| C16 | primitive/path/glyph×cap/join/align×dash×reversed×退化×fit | 原轮廓/退化Stroke/dash起点/bounds；S4 |
| C17 | 四gradient×stop空/单/重复/边界×matrix/fit×space/alpha | 默认/退化、空间premultiply、runtime fit；S5 |
| C18 | data/dataURI/file/runtimeImage×4format×16tile×4fit×6sampling | 各消费者优先链/dimensions/纹理/预算；S5 |
| C19 | vector/bitmap交错×overlap×scope/painter alpha×failure | run遮挡序/image opacity/missing≠blank；S3/S5 |
| C20 | 模板/defaultHost/已有scene×VM8type×4direction×converter×open/shared slot | 全referenceInstance链接→统一bind→首次flush，父slot/Composition共享VM、open slot回填与跨层source；主动排除、同状态参考；D7/S0/S6 |
| C21 | SVG→PPT→SVG/同doc重复×sharedFill/PathData/Font×合法冷热 | 模板/nodes/metadata/ID稳定，Font不热改，无Fill污染；S4/S6 |
| C22 | 3staticPolicy×2textMode×animationStrict/BestEffort×bake失败 | 正交、不冻Strict、不VectorOnly新增补丁、不silent成功；S0/S6 |
| C23 | deep/cycle×乘性Repeater×glyph/points/image/output预算 | 有限/数量安全、预算确定失败、NaN sentinel不误拒；S0/S6 |
| C24 | sourceSVG→PAGX/PAGX→SVG两段×未保存源参数 | 分离import损失/export新增损失，非roundtrip掩同错；S0/S6 |

#### D.2 动态 A01—A22

| 组合编号 | 必测动态交互 | 检测 / 分类 |
|---|---|---|
| A01 | Default/Named/All×首SM/unknown×无根仅Composition | 选择不暗跳、定义≠激活；N/G或明确U；M1 |
| A02 | 同template双slot×external同名×contains×普通Layer timeline | 实例binding/所属doc、无越界/误激活；M1 |
| A03 | 同slot同animationId多driver×任一paused×兄弟slot | shared pause scope禁advance但全apply，兄弟独立；M1 |
| A04 | 三Loop×±offset/paused×fps×D/2D/第二周期 | content与progress分离、初态/唯一周期；M1 |
| A05 | 静态masked pose×同anim跨Object priority×跨anim matrix/component/mix | merge后filter、anchor/z例外、ordered decompose；M2 |
| A06 | scalar full turns×Matrix shortestArc×anchor/skewAxis/负/零scale | 全圈/连续角/捕获前后singular，不inverse恢复；M2/M3 |
| A07 | 多easing×scope/Painter/color alpha×零起点×两groupOpacity | 完整乘积与重叠合成；N或有证明G；M2/M3 |
| A08 | 跨duration/空/单/duplicate ordered key×1-frame/D1/D0 | t0真值50、T{}≠无channel、重复边界按runtime；M3 |
| A09 | None/Hold/Linear/Bezier×overshoot float/Color×镜像 | No epsilon、float不clamp因子/Color分量clamp；M3 |
| A10 | XML整数/浮点/string/enum/ImageRef×实际writer | parse→evaluate→apply，blendMode/image mismatch源修或U；M1/M5 |
| A11 | shared color/gradient/stop×实例/Clock×dynamic fit/space/alpha | defs依赖、连续bounds/退化与颜色；N/G或B；M4 |
| A12 | 动态blur/shadow/behind/Only/spread×有序effect链×region/offset | 全时段来源/halo/非线性参数；N/G可证，否则B；M4 |
| A13 | Noise活跃/inactive/density0×backdrop/Glass×动态背景 | 无writer/no贡献≠B；B只D8/MB；M4/MB |
| A14 | 动态Rect/Ellipse/Polystar×静态Trim/Round/Merge×Painter | 来源不丢、形变/拓扑/painter lifetime；G或B；M5 |
| A15 | Path pointN×renderScale×verb长度/overflow×fit/dash | 原始点求值后完整geometry；G，非法U；M5 |
| A16 | Repeater copies/offset/幂/alpha×copy order×外Painter | bounded副本/离散拓扑，非法幂U、复杂B；M5 |
| A17 | Text九channel×字体供应×TextBox/overflow×glyph count切换 | TextHolder/flush/reshape/有效空；G证明或B/U；M5 |
| A18 | Selector六channel×Modifier十三×TextPath×base-overlay paint | glyph index/factor/矩阵/顺序、拓扑与预算；G或B；M5 |
| A19 | Mask/Contour/TextBox/WOFF2/path/bitmap分支×祖先/实例动画 | 实际宿主带共用上下文、覆盖与alpha；M2/M5 |
| A20 | 多Clock无有界公周期×组合依赖×有限window/repeat | 分解可证N/G；明确窗口B；无界/未知U；M3/MB |
| A21 | finite事件/Once/初态/资源×3D/conic/diamond/noise/backdrop/complex fallback×staticPolicy | D8未同意或VectorOnly拒绝；有限MB离散image每区间恰好一帧、透明/尾帧/repeat/seek/消费范围，无隐式改policy；MB |
| A22 | 175全集分类×实际输出×staticPolicy×预算/写失败 | classified≠supported、Strict不PNG冻、B/U确定拒绝、旧file不变；M6 |

### E. 唯一静态 ST1—ST46 目录

ST1—ST42 合为一表，不再旧 / 新两篇；ST43—ST46 是本轮契约补核查。下列是开发输入与待验证场景，条数不是修复数量。

| 编号 | 源码依据与根因 / 契约 | 直接复现、修复与阶段 |
|---|---|---|
| ST1 | `src/pagx/svg/SVGExporter.cpp:319` ID不避作者；`src/pagx/svg/SVGExporter.cpp:3537`重复内联，Mask按template cache | grad0、双实例内部同ID、Mask后缀/external碰撞；S1 |
| ST2 | `src/pagx/svg/SVGExporter.cpp:1858`跨父只告警；`src/pagx/svg/SVGExporter.cpp:1814`只自身matrix | 祖先平移/旋转/scroll、子Mask、双实例；完整相对空间；S1 |
| ST3 | `src/pagx/svg/SVGExporter.cpp:1828`根alpha=1，绕effects/composition/scroll/probe | alpha0/0.5、仅composition、filters/scroll/TextPath、Luminance前景白背景黑；S1/S2 |
| ST4 | `src/pagx/svg/SVGExporter.cpp:1842`Contour普通paint、未抽覆盖；`src/pagx/svg/SVGExporter.cpp:3557`只跳直接Mask child | stroke环/evenodd洞/hidden后代，外父stage Mask不额外画；S1/S4 |
| ST5 | `src/pagx/svg/SVGExporter.cpp:3432`、`src/pagx/svg/SVGExporter.cpp:3539`简化alpha包装 | nested groupOpacity=false重叠、filter/blend/pixelMask/passThrough=false；真实离屏；S1 |
| ST6 | `src/pagx/svg/SVGExporter.cpp:1711`px margin当%；`src/pagx/svg/SVGExporter.cpp:1453`shadow不推进；`src/pagx/svg/SVGExporter.cpp:1729`filter先style | 小bbox大blur/offset、连续非交换链、style后filter、Mask缩放尺度；S2 |
| ST7 | `src/pagx/svg/SVGExporter.cpp:1521`忽略spread；`src/pagx/svg/SVGExporter.cpp:1576`、`src/pagx/svg/SVGExporter.cpp:1652`跳Glass/backdrop | ±spread/tile/noise、渐变背景半透明Glass/祖先foreground，不能孤立目标PNG；S2 |
| ST8 | `src/pagx/svg/SVGExporter.cpp:3334`TextBox富文本旁路；`src/pagx/utils/TextUtils.h:74`只非空text且丢内Group状态 | glyph-only/Rect/多Painter/transform-alpha/foreground双遍；S3 |
| ST9 | `src/pagx/svg/SVGExporter.cpp:2388`WOFF2多Font/advance/anchor不全；`src/pagx/utils/TextUtils.cpp:84`matrix不同、`src/pagx/utils/TextUtils.cpp:192`漏offset | 双Font、无position/xOffset、非零anchor/rotate-scale-skew/vector offset；S3 |
| ST10 | `src/pagx/svg/SVGExporter.cpp:2293`fill-opacity；`src/pagx/svg/SVGExporter.cpp:2334`image漏因子 | 真实PNG/WebP glyph、scope/Painter alpha、缺resource诊断；S3/S5 |
| ST11 | `src/pagx/svg/SVGExporter.cpp:2118`inset；`src/pagx/svg/SVGExporter.cpp:1990`简dash；`src/pagx/svg/SVGExporter.cpp:2217`漏reversed | 厚非圆ellipse、open/closed Path/Text内外stroke、adaptive/roundcap/reverse/原fit；S4 |
| ST12 | `src/pagx/utils/ModifierResolver.cpp:499`PPT Append→Union；`src/pagx/utils/ModifierResolver.cpp:596`Group不进slots；`src/pagx/utils/ModifierResolver.cpp:720`Repeater清槽 | Append overlap stroke、外Trim/Merge、负scale首copy幂、shared Fill rule；S4 |
| ST13 | `src/pagx/svg/SVGExporter.cpp:665`empty stop；`src/pagx/svg/SVGExporter.cpp:639`退化fit；`src/pagx/svg/SVGExporter.cpp:3293`未携Painter空间 | 默认黑白、零轴gradient、Group geometry由外user-space paint；S5 |
| ST14 | `src/pagx/svg/SVGExporter.cpp:872`仅PNG尺寸；`src/pagx/svg/SVGExporter.cpp:816`任Repeat双轴；`src/pagx/svg/SVGExporter.cpp:915`旋转丢fit | 真实JPEG/WebP/GIF、Repeat/Decal、Mirror+LetterBox、rotate/负scale+Stretch/Zoom；S5 |
| ST15 | `src/pagx/utils/ImageFormatUtils.cpp:270`、`src/pagx/utils/RasterUtils.cpp:393`只data/file；`src/renderer/LayerBuilder.cpp:2295`runtimeImage优先 | dataURI/供应runtime、红data蓝runtime、missing拒绝不泄本机路径；按消费者闭合；S5/S6 |
| ST16 | `src/pagx/utils/ModifierResolver.cpp:736`改shared Fill、makeNode临时增长 | 重复SVG→PPT→SVG、Fill/nodes稳定、nonfinite/deep/乘性copy/像素预算；S4/S6 |
| ST17 | `src/renderer/LayerBuilder.cpp:2756`消费AA；`src/pagx/svg/SVGExporter.cpp:3575`未承载 | AA=false斜边/rotate/clip固定scale，不以hint未经验证视等价；S1/S2 |
| ST18 | `src/renderer/GlyphRunRenderer.cpp:200`faux；`src/pagx/svg/SVGExporter.cpp:2480`WOFF2/outline漏样式 | 真实style+faux、outline/advance/bounds/Mask、Native/WOFF2/path/bitmap分别；S3 |
| ST19 | `src/renderer/LayerBuilder.cpp:2261`sampling；`src/pagx/utils/RasterUtils.cpp:401`bake漏 | 6sampling×16tile、放缩旋转真实纹理，native/bake同源；S5 |
| ST20 | `third_party/tgfx/src/layers/vectors/TextSelector.cpp:436`Index offset/shape/首mode | 完整factor/glyph/mode/seed，非字符串长；S3 |
| ST21 | `third_party/tgfx/src/layers/vectors/TextModifier.cpp:44`累积权重；`third_party/tgfx/src/layers/vectors/FillStyle.cpp:66`、`third_party/tgfx/src/layers/vectors/StrokeStyle.cpp:63`分支 | 第2.8.1节：部分base完整再overlay，权重1跳base；每源Painter排他替换，不让局部Fill再次被祖先消费；数值/gradient/blend/stroke验收；S3/S1 |
| ST22 | `third_party/tgfx/src/layers/vectors/TextPath.cpp:95`、`:117`、`:127`、`:170`、`:217`实际坐标与分支 | 第2.8.2节tgfx唯一标准及TP01—TP16；normal不消费last、force有符号终偏移、法向/旋转分离、闭合/open/第一contour/layout明确；S3 |
| ST23 | `third_party/tgfx/src/layers/vectors/MergePath.cpp:70`清旧Painter，resolver只换slots | Rect→Fill→Rect→Merge→Stroke、Merge后无Painter、Text→Shape；S4 |
| ST24 | `third_party/tgfx/src/layers/vectors/Repeater.cpp:139`Below末copy先；`src/pagx/utils/ModifierResolver.cpp:711`反序 | 相交不同alpha、真次序/整数幂，负底分数幂独立边界；S4 |
| ST25 | `third_party/tgfx/src/core/shaders/GradientShader.cpp:140`空间插值/premultiply | 透明红蓝内部、P3/mixed，非时间LerpColor；S5 |
| ST26 | `third_party/tgfx/src/core/shaders/GradientShader.cpp:337`退化/单stop | linear/conic近等首色、radial/diamond近0末色、正常/阈值/无效；S5 |
| ST27 | `third_party/tgfx/src/layers/vectors/Rectangle.cpp:80`零轴微轮廓 | 零/负轴+Stroke/cap/dash/Trim，Fill独立，与singular区分；S4 |
| ST28 | `include/pagx/nodes/TextBox.h:43`→`include/pagx/nodes/Group.h:36`→`include/pagx/nodes/LayoutNode.h:94`的继承尺寸；`src/pagx/utils/TextUtils.cpp:515`、`:525`误先author | 保留、不新增字段；第2.8.3节父300×200、author100×60、resolved250×170、inset230×150；Native/PPT/TextHolder同源，0与unbounded分开；S3/S1 |
| ST29 | `src/pagx/svg/SVGExporter.cpp:2839`empty fallback重画Hidden文字 | all/some/none有效行，valid empty≠missing metadata；S3 |
| ST30 | `src/pagx/svg/SVGExporter.cpp:2504`、`src/pagx/TextLayout.cpp:1082`Native空白/BiDi/justify差 | 连space/tab/CJK无space/RTL数字rich text、两text模式；S3 |
| ST31 | `src/pagx/svg/SVGExporter.cpp:2316`先path后bitmap | 合法多Font先bitmap后vector重叠，保run序；S3 |
| ST32 | `include/pagx/nodes/Node.h:30`、`src/pagx/PAGXChannelTable.h:28`反射非全集 | 55/334+31/111+40/155集合与结构/None/derived输入，漏分类阻断；S0/S6 |
| ST33 | `src/pagx/svg/SVGExporter.cpp:3391`hidden+mask漏stage过滤 | hidden owner≠hidden Mask根；S1 |
| ST34 | `src/renderer/LayerBuilder.cpp:1177`composition优先、SVG双画 | 双内容相交颜色，实际优先级或明确拒冲突；S1 |
| ST35 | `src/pagx/nodes/Layer.cpp:289`生成scroll被hasScroll阻断失效 | width100→200、explicit scroll优先，源layout修；S1 |
| ST36 | `src/pagx/svg/SVGExporter.cpp:3689`有效fontConfig不同 | 仅doc注册字体、首次layout后export、同源配置；S3/S5 |
| ST37 | `src/pagx/svg/SVGExporter.cpp:3709`根nodes预扫external Font遗漏 | external embed/bitmap/nested未供应、SDK/CLI同前置；S5/S6 |
| ST38 | `third_party/tgfx/src/layers/filters/NoiseFilter.cpp:144`density0 input no-op | 三mode零density、无效size不空filter、phase/offset/Duo合成；S2 |
| ST39 | `include/pagx/nodes/LayerStyle.h:30`、`include/pagx/nodes/DropShadowStyle.h:34`继承/特有漏消费 | behind/exclude/blend/Below-Above顺序/spread contour塌陷；S2 |
| ST40 | `src/pagx/svg/SVGExporter.cpp:1076`Blend额外alpha clip/Plus Normal | 18×source/dest alpha与透明洞、真正ColorFilter；S2/S5 |
| ST41 | `src/pagx/PAGScene.cpp:514`draw先flush绑定 | false关SMIL≠defaultHost、Once/默认VM≠模板、同源bake/reference；S0/S6 |
| ST42 | `src/pagx/svg/SVGPathParser.cpp:43`%g；`third_party/tgfx/src/core/PathRef.cpp:42`全控制点bounds | 大坐标小差/Q-C远点/LetterBox fit、layout-fit-coverage分开，不擅tight化；S4/S5 |
| ST43 | `src/pagx/utils/ModifierResolver.cpp:596`、`src/pagx/svg/SVGFeatureProbe.cpp:86`解析与probe有效路径 | 移除resolveModifiers丢效果开关；Polystar/Group/Text/Mask静动默认完整解析，偏好不绕policy；S0/S4/S6 |
| ST44 | `include/pagx/nodes/Font.h:103`glyphs首render后undefined；`src/renderer/GlyphRunRenderer.cpp:109`失败也built | 合法同输入cold/warm、新Font/既定清理、失败缓存、共享layout/cache外部串行；不承诺热改自动重建；S1/S3/S6 |
| ST45 | `third_party/tgfx/src/core/shapes/MatrixShape.cpp:30`singular→null；`third_party/tgfx/src/layers/vectors/Rectangle.cpp:80`微轮廓不同 | Painter捕获前后零scale/恢复、Fill/Stroke/Mask，保runtime空几何不inverse强恢复；S1/S4 |
| ST46 | `src/cli/CommandExport.cpp:184`、`src/pagx/svg/SVGExporter.cpp:3673`入口输出契约需闭合 | 推荐转换成功后同目录tmp→write/flush/close→atomic replace；逐步失败注入，旧file不变、清理有界、不承诺crash durability；S0/S6 |

复用静态证据：倍宽 alignment clip `third_party/tgfx/src/layers/contents/ShapeContent.cpp:129`；adaptive dash `third_party/tgfx/src/layers/DashEffect.cpp:24`；Contour stroke `third_party/tgfx/src/layers/contents/DrawContent.cpp:70`；离屏与Mask相对matrix `third_party/tgfx/src/layers/Layer.cpp:1185`、`third_party/tgfx/src/layers/Layer.cpp:1277`；glyph offset / advance / anchor `src/renderer/GlyphRunRenderer.cpp:137`、`src/renderer/GlyphRunRenderer.cpp:218`；空stop `src/renderer/LayerBuilder.cpp:2097`、退化fit `third_party/tgfx/src/layers/vectors/Gradient.cpp:93`、image fit `third_party/tgfx/src/layers/vectors/ImagePattern.cpp:95`。

### F. 唯一动效 F1—F18 目录

F1—F12 原 ID 全保留；F13—F18 是动态补审。除历史官方样例证据外，本轮没有新增运行复现；可修错误从源头修，不以 unsupported 告警掩盖。

| 编号 | 根因与源码定位 | 复现 / 预期与阶段 |
|---|---|---|
| F1 | `src/pagx/svg/SVGAnimationPlan.cpp:124`提前返，只扫Layer driver | 根Animation无Layer driver、仅Composition且根定义空仍按实际激活导出；M1 |
| F2 | `src/pagx/svg/SVGAnimationPlan.cpp:131`pointer去重、`src/pagx/svg/SVGAnimationPlan.cpp:153`根doc解析/Node binding | 双slot不同phase/playing、external同名不根误绑、scope contains；M1 |
| F3 | `src/pagx/svg/SVGAnimationWriter.cpp:599`共享duration重复翻倍、`src/pagx/svg/SVGAnimationWriter.cpp:1283`color同、`src/pagx/svg/SVGAnimationWriter.cpp:1212`visibility未镜像 | PingPong 2D统一，现transform16D/color4D，visibility往返；M1 |
| F4 | `src/pagx/svg/SVGAnimationWriter.cpp:253`offset/paused仅begin | delay前/paused真实初态、Loop正offset原点模回绕，不等待begin；M1 |
| F5 | `src/pagx/utils/ExporterUtils.cpp:160`identity漏position override | 默认Group position.x 0→100中点50；M2 |
| F6 | `src/pagx/svg/SVGAnimationWriter.cpp:537`重decompose丢标量圈数/shortestArc | scalar0→360整圈，Matrix170→190中点180非0；M2 |
| F7 | `src/pagx/svg/SVGAnimationWriter.cpp:432`不Hold、`src/pagx/svg/SVGAnimationWriter.cpp:940`派生连续/首尾不一 | width100 Hold→200中点100、15/45key duration60真补端、不同easing内部；M2/M3 |
| F8 | `src/pagx/svg/SVGAnimationWriter.cpp:772`、`src/pagx/svg/SVGAnimationWriter.cpp:816`零alpha退出、`src/pagx/svg/SVGAnimationWriter.cpp:834`静态比率、`src/pagx/svg/SVGExporter.cpp:2060`缺Painter/color因子 | 零起淡入、0.5³=0.125、多因子同时animate组合；M2/M3 |
| F9 | `src/pagx/svg/SVGExporter.cpp:3432`任Layer animation改perChildAlpha、`src/pagx/svg/SVGExporter.cpp:3584`组opacity不等价 | groupOpacity=false静alpha0.5仅matrix动仍逐项重叠合成；M2 |
| F10 | `src/pagx/svg/SVGExporter.cpp:1794`Mask writer漏Plan、`src/pagx/svg/SVGExporter.cpp:3334`Native TextBox路径不全 | Mask内部、TextBox/祖先轨道到实际宿主且空间正确；M2 |
| F11 | `src/pagx/svg/SVGExporter.cpp:3406`只自身依赖、resolver产物失来源 | 内Group/Rect/Fill动态不单PNG freeze，替换geometry保依赖；M2/M5 |
| F12 | `src/cli/CommandExport.cpp:184`不收warnings | stderr损失可见、Strict非零、不成功提示掩盖；M6 |
| F13 | `src/pagx/PAGXImporter.cpp:1906`、`src/pagx/PAGXImporter.cpp:1933`typed parsing；`src/renderer/LayerBuilder.cpp:1239`、`src/renderer/LayerBuilder.cpp:1939`writer mismatch | 全typed XML→evaluate→actual apply；int/float/enum/imageRef≠runtimeImage、Path point长度与溢出，源修或U；M1/M5 |
| F14 | `src/pagx/PAGAnimation.cpp:59`、`src/pagx/PAGAnimation.cpp:111`merge/filter；`src/renderer/LayerBuilder.cpp:192`、`src/renderer/LayerBuilder.cpp:267`shared pose | 静态masked/同anim跨Object/跨anim ordered mix三层，matrix不屏z/anchor、后component decompose；M2 |
| F15 | `src/pagx/runtime/KeyframeEvaluatorImpl.h:106`T{}、`src/pagx/svg/SVGAnimationWriter.cpp:129`clamp key、`src/pagx/svg/SVGAnimationWriter.cpp:191`Hold近似；`src/pagx/runtime/BezierEasing.cpp:104`、`src/pagx/utils/ColorSpaceUtils.cpp:78`overshoot | 跨duration t0=50、空≠无、单/1-frame/D1/D0/nonfinite/duplicate ordered keys、float不clamp/Color分量clamp、No epsilon；M3 |
| F16 | `src/pagx/runtime/PAGComposition.cpp:57`、`src/pagx/runtime/PAGComposition.cpp:130`paused IDs共享 | 同slot同ID任paused禁全advance但apply全执行，兄弟slot独立、ordinal≠pause scope；M1 |
| F17 | `src/pagx/PAGXNodeChannel.cpp:486`175/30 registry；本次 `src/pagx/svg/SVGAnimationPlan.cpp:67`76/17白名单与 `src/pagx/svg/SVGCoverageLedger.cpp:30`实现状态 | M4.0—M5.4逐子集同步Plan、ledger和输出/拒绝门禁；颜色、stop、d、Text完整依赖不漏，Planned/Supported≠N/G证明；M0/M4/M5/M6 |
| F18 | `src/pagx/PAGAnimation.cpp:274`Clock、`src/pagx/PAGScene.cpp:514`宿主flush、`src/pagx/utils/RasterUtils.cpp:351`背景合成契约 | 多Clock无公周期、finite事件/3D/noise/complex fallback；D8禁用拒绝、明确MB初态/window/resources/resolution/sampling/tail/repeat/预算，无界U；M3/MB/M6 |

### 5.1 只读 coverage 校验与维护门禁

| 核查集合 | 已登记范围 / 只读校验要求 | 不代表 |
|---|---|---|
| Node inventory A.3 | 55/55，334 自有字段逐名 / 直接基类一致；private输入/缓存不漏，NodeType恰好一次 | 55类视觉或334字段等价输出已实现 |
| 辅助 A.1 | 31类 / 111自有字段，按表递归继承，非Node附属 / 派生完整 | 111项都是作者输入、非法Font热修改受支持 |
| enum C | NodeType之外40类 / 155值，与nodes/types/ChannelFlags集合一致 | 无限组合穷举或所有值pixel通过 |
| dynamic ledger 3.2 | 30类 / 175全名，与反射Anim集合一致；styles/filters51、Group/TextBox各10；表外另账 | 175全部支持、39白名单保真或B已经实现 |
| 组合 / 缺口 | C01—C24、A01—A22、唯一ST1—ST46 / F1—F18，无漏号 / 重号，阶段闭合 | 缺陷总数、修复数量或新测试通过数 |

重组后重新对照当前声明，55节点/334字段及继承、31附属结构/111字段、40枚举/155值、30类/175动效通道全部一致；ST/F/C/A编号唯一完整，D决策、代码块与章节引用已核对。文中file:line引用逐项检查了文件存在与行号范围，并经独立语义复审；这些检查不执行渲染，也不证明所有引用行为已运行复现。源码新增声明使旧计数失效，必须重新枚举；未分类项先阻断，不能静默default。

后续每有效输入至少默认 / 非默认 / 合法边界 / 关键依赖；cache按合法生命周期、metadata按转义 / 确定性或排除、宿主按主动排除 / 状态来源；所有有效 draw 与 channel 有输出 / 确定拒绝记录。开放 converter / URL / 业务历史不能标“穷举”。连续 N/G 按误差证明，B按明确有限契约；视觉与集合门禁分别报告，共享源变化回归按第4节。

### 5.2 压缩文件索引（未来责任范围）

| 文件 / 模块 | 责任 |
|---|---|
| `include/pagx/SVGExporter.h` | policy/text/selection/诊断公开契约，替代冲突选项 |
| `src/pagx/svg/SVGAnimationPlan.{h,cpp}` | instance/driver/pause scope、typed绑定/ordered apply、依赖/分类计划 |
| `src/pagx/svg/SVGAnimationWriter.{h,cpp}` | compiled track / native-G-discrete、序列化；职责挤占时才拆内部单元 |
| `src/pagx/svg/SVGExporter.cpp` | 共用Context/DrawRecord、Painter/defs/Mask/TextBox/bake消费 |
| `src/pagx/utils/ExporterUtils.{h,cpp}`、`src/pagx/utils/LayerTransformUtils.{h,cpp}` | position override、组件/anchor/layout/3D与共用矩阵 |
| `src/pagx/utils/ModifierResolver.{h,cpp}` | VectorContext、局部结果/provenance/lifetime、Append/Merge/Repeater |
| `src/pagx/svg/SVGFeatureProbe.{h,cpp}`、`src/pagx/utils/RasterUtils.{h,cpp}` | 完整能力/引用/背景/3D、消费范围与像素预算 |
| `src/pagx/utils/TextUtils.{h,cpp}`、`src/pagx/utils/Woff2FontGenerator.{h,cpp}` | glyph放置/alpha/faux、Font分段与WOFF2 |
| `src/pagx/utils/ImageFormatUtils.{h,cpp}`、`src/pagx/utils/StrokeGeometryUtils.{h,cpp}` | 消费者资源链/解码尺寸、描边/clip/dash/fit |
| `src/renderer/LayerBuilder.{h,cpp}`、`src/renderer/GlyphRunRenderer.{h,cpp}` | runtime writer/type/pose、资源/effect、glyph与cache源头依据 |
| `src/pagx/TextLayout.{h,cpp}`、`src/pagx/TextLayoutParams.h` | 字体/布局/有效TextHolder参数、reshape与最终glyph |
| `src/pagx/nodes/Layer.cpp`、`src/pagx/PAGXDocument.cpp`、`src/pagx/PathData.cpp` | layout失效/归属/生命周期、路径与缓存 |
| `src/pagx/ppt/PPTExporter.cpp`及实际PPT模块 | PPT专用补偿，只在自身输出端处理，共用回归 |
| `src/pagx/svg/SVGImporter.cpp`、`include/pagx/nodes/Gradient.h`、`include/pagx/nodes/RadialGradient.h` | 源SVG展开和模型未保留边界 |
| `src/pagx/PAGAnimation.cpp`、`src/pagx/runtime/KeyframeEvaluatorImpl.h`、`src/pagx/runtime/MatrixDecompose.h` | 复用content Clock/evaluate/shortestArc，不另造求值器 |
| `src/pagx/PAGXNodeChannel.cpp`、`src/pagx/PAGXImporter.cpp`、`src/pagx/runtime/PAGComposition.cpp` | channel声明/typed解析、有效实例和pause scope |
| `src/cli/CommandExport.cpp`、`src/cli/CommandRender.cpp` | CLI选项/warnings/失败；render已有top和nested，选择集合/初始化/事件契约需与参考一致 |
| `test/src/PAGXSVGTest.cpp`、`test/src/PAGXTest.cpp`、`test/src/PAGXCliTest.cpp` | XML/语义/资源/pixel、runtime Clock与CLI事务 |
| `tools/html-snapshot/eval-animation/baseline-frames.js`、`tools/html-snapshot`共用入口、`CMakeLists.txt` | 借采帧基础而非WAAPI时钟，接入SMIL与构建/评测目标 |

本轮交付仅为重组后的完整方案。D决策确认、源码实现、修复验证、浏览器像素、共享格式回归和基准审核仍未完成。
