# PAGX → SVG 开发方案与覆盖核查

> 初稿：2026-11-09；完整续作方案：2026-11-12。分支：`feature/lifengdy_svg_export`。
> 续作核查基点：`aeb346c05`。正文的旧问题定位对应发现时源码；续作使用第6节的文件与函数定位，不凭旧行号替换代码。
> 本轮仅整理文档和执行规则，未修改源码、构建或运行渲染测试；过去“已收口”记录不能替代本轮全量验收。
> D1—D7按已选契约执行，D8保持MB禁用。第0/1.4/6节取代旧R1/R2分批结束点；第2.8节固定数学语义，附录保存完整范围。

## 0. 新终端启动指令（可直接使用）

```text
阅读 docs/pagx-svg-export-development-plan.md 的第0、1.4、2.8和6节，
然后按T00→T12完成本分支全部剩余静态与动效及统一验收，不只做首轮子集。
先确认当前源码/工作区状态，保留用户已有提交和未提交文件；从已实现且符合契约的代码继续。
批次只是依赖顺序，不是收尾点。不要中途汇报“做了什么”，不要问“继续吗”，
不要以提交成功、完成某个S/M阶段或局部测试通过结束本轮。
连续实现，测试随功能补，最后统一格式化、构建、全量测试并集中修复。
D1—D7、接口迁移和数学规则已经固定，不反复请求相同决策。MB保持禁用。
N/G承诺项必须实现；不能把未写算法降为B/U或拒绝所有输入后说完成。
只有需要覆盖用户工作、发布依赖/推送等未授权动作、受保护基准接受，
或有证据表明无法遵守契约且必须改变方向时才暂停，并一次给出准确阻塞和剩余清单。
已存在的待发布依赖不能算完成；普通编译/测试失败应定位修复，不是阶段汇报理由。
完成标准是第6.10节的全部门禁，不是“代码写完”或“91个Supported”。
只本地提交本轮实际改动，不自动推送，不自动接受基准，不跳过hooks。
```

执行者必须在同一工作区只有一个实现进程时工作；本说明不授权丢弃另一个终端的未提交内容。若新终端启动时代码已前进，核实差异，保留正确实现并更新剩余状态，不重放旧补丁。

## 1. 总览、范围与已定决策

### 1.1 结论与阅读顺序

**先完成静态，再编译动效；共用实例、绘制、资源、状态和诊断契约，不另造第二套解释器。**

静态声明已登记全集：55 个 NodeType / 335 个自有字段、31 个基类或附属结构 / 111 个字段、40 类 enum / 155 个值。完整声明、逐字段运行语义、组合和证据保留在附录 A—F。动效反射的 30 类 / 175 个通道全部分类，另登记反射表外动态路径和图片 payload；**全集已分类不等于全部已支持，更不等于修复或像素已通过。**

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

### 1.3 已定D决策的语义索引（保留原ID，静态先列）

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

按第1.5节已经采用的D1—D7继续执行，不重复等待确认；门禁数值是目标而不是已通过结果。D8明确保持禁用，MB不是本轮交付项；不能据此遗漏B/U拒绝契约。

### 1.4 全量连续执行规则（唯一交付终点）

本次授权范围是全部剩余静态S0—S6、动效M0—M6及其验收；旧H0/R1/R2只是历史开发次序，**不再是停止或重新排期的依据**。T00—T12是第6节的连续续作批次，任一批次完成立即进入下一依赖，没有“这批先到这里”。

- 任务、测试和实现状态持续记录在本文与任务列表，用户可查询；不逐步发送进度总结、不问是否继续。
- 正常复杂度、补测、编译错误、测试失败、已提交一个commit，不构成暂停条件。编写对应测试后继续实现，最终统一验证；必要的编译接口探针只用于消除真正的后续编译阻塞。
- 只在未授权的外部动作/覆盖用户工作/受保护基准，或需要改变已定语义与技术路线时暂停。给证据、无法继续的具体步骤和唯一所需决定，不能把阻塞隐藏成简化方案。
- 值得修复的N/G映射、Painter、Mask、资源、文字和组合算法必须实现，不因工作量转为“只告警”“暂不支持”。本来就属于B且D8禁用的有效场景必须有明确拒绝测试，不要求偷偷开启MB。
- 接口/白名单/ledger/生成能力文档与实现同批改，只有统一验证通过才登记Verified；ImplementedUnverified不是最终完成。
- 最终只有两种诚实结果：第6.10节全部满足后的完成；或有具体外部阻塞的Blocked。不能把Blocked标为任务完成，也不能在代码局部成功时给完成率冒充整体交付。

本轮不推送、不创建远程MR/PR，不跳过hooks，不接受截图基准。tgfx依赖发布若确有必要，属于真实审批前置而非重新讨论功能方案；第6.4节规定本地执行和正式交付的区别。

### 1.5 已采纳的 D 决策

按 1.3 的推荐值执行，不再逐项等待确认；若后续要改，必须重跑对应门禁并把差异写回本节。

| ID | 采纳值 | 当前落地状态 |
|---|---|---|
| D1 | Default首个top-level timeline；Named指定Animation；All全部top-level Animation；nested独立按挂载驱动 | 选择入口已存在，实例/contextDoc/pause/未选择StateMachine过滤仍未完成；Default不等于只扫Layer.timelines |
| D2 | 几何0.25文档像素、颜色/alpha分量1/255 | 固定8段和32/512点上限不是证明；T08补连续界、序列化误差与预算拒绝 |
| D3 | 默认Strict，BestEffort显式选择 | Policy入口已存在，Writer丢轨道和局部采样/资源失败必须传播到ExportResult，T01/T08/T11收口 |
| D4 | 默认PreserveAppearance，原生优先且必要bake失败拒绝 | 不等于bakeUnsupported=true；pattern旁路和失败回矢量尚未闭合，T01/T05/T06修复 |
| D5 | 默认Fidelity，实际glyph为准 | 不等于旧convertTextToPath=false；移除冲突映射，保持WOFF2/轮廓/原bitmap的有效表示，T01/T03 |
| D6 | 固定仓库锁定的Playwright浏览器、画布、背景、色彩、缩放和按类别验收 | 单文字样本0.05/10/0.4仅历史宽容值，不能覆盖几何和动效门禁；T12固定实际版本并扩大矩阵 |
| D7 | TemplateStatic，不初始化VM/StateMachine历史 | 模板静态入口已有；Oracle必须同源、局部求值不改共享模板，T02/T07 |
| D8 | 不启用MB | 有效B场景Strict确定拒绝，不新增位图帧序列；N/G未写算法不能借B分类跳过 |

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
- 任务：附录 A—C 声明集合、D.1 组合、E 的 ST1—ST46 建门禁；落实已定D4—D7；no-op / inactive / 有效空 / 未初始化宿主分类；共用 XML、同源静态一帧与预算 / 失败入口。
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

本表固定四类展开的执行次序，不需要实施时再询问。当前已存在 `src/pagx/svg/SVGCoverageLedger.h:32` 的Supported/Planned/NotExpressible实现状态和 `src/pagx/svg/SVGAnimationPlan.cpp:67` 的白名单；它们不等同正文N/G/B/U能力分类。**当前白名单为18类/104项（另含 NoiseFilter/NoiseStyle 的 size/seed 与 color/firstColor/secondColor、shadow style spread）**，Path point 族按表外通道单列，其余 Text shaping 与动态 modifier 尚未放行；不按旧7类/39项覆盖用户已有滤镜增量。未发现已实现的ModifierDependencyGraph，本方案只扩现有resolver产物/DrawRecord来源，不能引用不存在的图模块当作前置。

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
2. 某子集实现typed XML→Channel求值→ordered apply→最终glyph/paint/geometry→SVG重建及对应测试后，同步Plan白名单和ledger条目；统一T12通过后才能登记Verified（不要求每子批次立即跑browser并停下汇报）；验证一个节点类型的一条通道不自动放行其全部通道。
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

以下是续作接口契约，按T01/T11落实，不重复询问已定默认值；当前已有字段和缺失字段以源码为准。静态enum语义见2.3，N/G/B/U是能力分类，不直接替代policy。

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

实现顺序静态先行：按第6节T00—T06完成共用/静态代码与测试，再连续T07—T11动效，T12统一验证并集中修复；不在S/M阶段边界停止或强制逐步构建。D8禁用，MB不实施；必要的真正编译阻塞探针是例外。同一基础只实现一处。

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
| 本轮 D6 门禁稳定性（2026-11-12） | 浏览器门禁改为 `domcontentloaded` 后立刻 `pauseAnimations()+setCurrentTime(0)`，再等字体后 seek 目标时间：避免 filled transform 动画先跑完再回退时 Chrome 绘制层保持终态（`getScreenCTM` 已正确但截图仍是终值）；8 个动效用例复跑全过 | Chrome 对 text 内 transform 轨道的合成刷新仍有边界，TextBox 动效暂不入 gate |
| 本轮 M2/F8（2026-11-12） | 同一 painter 的 color alpha 与 painter alpha 同时动画时，fill/stroke-opacity 改为在并集网格上按 8 段加密采样两者的乘积（0.75×0.75=0.5625 可验证），不再只发 painter alpha 而丢掉 color alpha；新增双因子回归 | 三个及以上 alpha 因子（如 TextModifier alpha）与 color alpha 的组合未覆盖 |
| 本轮 M5.4 TrimPath（2026-11-12） | start/end/offset 进入白名单与 Supported；`BuildBakedShapeDataTrack` 把 scope 内可烘焙的 TrimPath 通道并入采样帧网格，逐帧暂存/恢复 trim 参数后重跑 resolver，静态图形也能随 trim 变化发射 `d` 轨道；采样轨道记录自己的 Animation/Timeline，不再依赖 shape 的绑定；新增 trim 动画单测与 0.5s 浏览器门禁 | Merge/Repeater 链、trim type 切换与多 trim 叠加未单独验证 |
| 本轮 M5.4 RoundCorner（2026-11-12） | `RoundCorner.radius` 进入白名单与 Supported；采样器与 TrimPath 共用 modifier 扫描，逐帧暂存/恢复 radius 后重跑 resolver，静态图形发射半径变化的 `d` 轨道；新增单测与 0.5s 浏览器门禁（0 / 0.09 / 0.999） | 多个 RoundCorner/Trim 叠加与 Merge 链未覆盖 |
| 本轮 M2 Layer 分量（2026-11-12） | Layer 的 x/y/rotation.z/scale.x/scale.y/skew.x/skew.y/anchor.x/anchor.y 进入白名单与 Supported；`SampleLayerTransform` 在每帧暂存/恢复这些字段后重建 layer matrix，matrix 通道仍保持最高优先级；`IsLayerTransformChannel` 同步放行分量，新增位移/旋转单测与 Layer.x 浏览器门禁（0 / 0 / 1） | 超过 180° 的纯 rotation.z 仍经矩阵分解丢圈数（F6 残留）；z/perspective/matrix3D 仍未投影 |
| 本轮 M3/F6（2026-11-12） | Layer `rotation.z` 与 Group `rotation` 在只有旋转通道、scale/skew 为恒等时改用连续角度轨道（channel 值减 authored），不再经矩阵分解回绕；`WriteTransformTracks` 的 rotate 变化检测也改用连续值，修复 0→360 分解回 0 导致整条轨道被丢弃；新增 Layer/Group 370° 单测与 0→360 浏览器门禁（0.25s，0 / 0 / 1） | 多通道混合、非均匀 scale 下的旋转仍未走连续轨道；Matrix 通道的 170→190 由矩阵插值覆盖，未单独像素对照 |
| 本轮 M3/F7（2026-11-12） | 多通道 Layer 分量 / Group 变换 / Rect/Ellipse 几何轨道在任一通道含 Bezier 时按 8 段加密采样，共用线性轨道不再只走端点；单通道仍用 keySplines 精确表达；Hold 段因求值恒定不受加密影响；新增 ease-in 中点非 50 的回归 | 多通道混合 Hold+Bezier 的误差上界与采样预算未量化；F7 的 overshoot 仍未单独证明 |
| 本轮 M4.E shadow color（2026-11-12） | DropShadowFilter / InnerShadowFilter / DropShadowStyle / InnerShadowStyle 的 `color` 进入白名单与 Supported；`feColorMatrix` 的 20 项常量行由静态值改为可动画的 `values` 轨道，逐帧编码 RGBA（保留 source alpha 语义）；新增 filter/style 两个单测与 0.5s 浏览器门禁（0 / 0.24 / 0.999） | spread/showBehindLayer、NoiseStyle color 与 P3 shadow 未覆盖 |
| 本轮 M4.E NoiseStyle seed（2026-11-12） | `NoiseStyle.seed` 进入白名单与 Supported；`feTurbulence` 的 owner 由 NoiseFilter 泛化为 Node，style 的 dark/bright/multi 三路 turbulence 共用同一 seed 轨道；新增 style seed 回归 | NoiseStyle 的 color/firstColor/secondColor/opacity 与 filter spread 未覆盖 |
| 本轮 M4.E noise color（2026-11-12） | NoiseFilter / NoiseStyle 的 color、firstColor、secondColor 进入白名单与 Supported；`applyFloodColor` 带 owner/channel 并在动画时改发 `flood-color`/`flood-opacity` 轨道，BlendFilter 与 Mono/Duo 两种噪声共用；P3 静态声明在动画时跳过；新增 Mono color 与 Duo firstColor 回归 | NoiseStyle/Filter 的 opacity（Multi）与 spread 未覆盖 |
| 本轮 M4.E spread（2026-11-12） | DropShadowStyle / InnerShadowStyle 的 `spread` 进入白名单与 Supported；`feMorphology` 的 radius 变为绝对值轨道，operator 取 keyframe 共同符号；跨零的 spread 在 Strict 下拒绝并告警（operator 离散不可动画），BestEffort 保留静态；filter 侧 regions 已按全时段 |spread| 预留；新增正向轨道与跨零拒绝回归 | 无浏览器门禁；filter 无 spread 通道 |
| 本轮 M4.E noise size（2026-11-12） | NoiseFilter / NoiseStyle 的 `size` 进入白名单与 Supported；`feTurbulence` 的 `baseFrequency` 按 1/size 逐帧发射轨道，size=0 退化为 0.25；同元素可同时携带 baseFrequency 与 seed 两条轨道；新增 size 回归 | density（离散 tableValues）与 opacity 未覆盖 |
| 本轮 M6 能力文档（2026-11-12） | 新增 `docs/pagx-svg-animation-capabilities.md`，由 `SVGChannelLedger()` 生成（175 通道：77 Supported / 90 Planned / 8 NotExpressible），并新增 `SVGAnimationCapabilitiesDocIsCurrent` 门禁：ledger 改动后文档不更新即失败；表外 Path point 族与动效门禁用例一并记录 | 文档仅覆盖通道级状态，F1—F18/A12—A20 的场景矩阵仍待逐条落表 |
| 本轮 M6 动效门禁（2026-11-12） | 浏览器门禁支持 `case.pagx@seconds`：暂停 SMIL、`setCurrentTime(seconds)` 后截图，并与 `pagx render --time` 参考比较；新增 `SVGBrowserAnimTest` 与十三个 0.5s 用例：SolidColor 红→蓝（pixelDiff 0 / RGB 0 / SSIM 1）、ColorStop 红→绿（0 / 0.14 / 0.99999）、linear endPoint.x 160→80（0 / 0.06 / 1）、Text.x 平移（0.00007 / 0.63 / 0.983）、Polystar outerRoundness 0→1（0 / 0.14 / 0.9994）、Rect size+Trim 的 d 轨道（0 / 0 / 1）、Rect size+RoundCorner（0 / 0.07 / 0.999）、Ellipse size+Trim（0 / 0.24 / 0.994）、TrimPath end 0.3→0.9（0.024 / 4.22 / 0.795）、RoundCorner radius 4→24（0 / 0.09 / 0.999）、Layer.x 0→100（0 / 0 / 1）、Layer rotation.z 0→360（0.25s，0 / 0 / 1）、DropShadowFilter color 红→蓝（0 / 0.24 / 0.999）；门禁发现并修复 Text 包装 `<g>` 未先闭合 start tag 导致 `<g  <animateTransform` 的非法结构，单元测试补结构断言 | d 轨道的浏览器 seek 对照（运行时无普通 Path point writer）与更多语料阈值未做 |
| 本轮 M4.1（2026-11-11） | SolidColor.color 进入白名单；Fill/Stroke 的 painter 动画会向上解析共享 SolidColor 的绑定并各自发射 fill/stroke 轨道；`HasPainterAnimation` 同步识别源绑定；ledger 的 SolidColor 升级为 Supported；新增 SolidColor 驱动 Fill 回归 | 多消费者去重像素与源/Painter 同有绑定的真实顺序未做 |
| 本轮 M4.2（2026-11-11） | ColorStop.offset/color 进入白名单；linear/radial 的 `<stop>` 直接承载 offset、stop-color、stop-opacity 三条 SMIL；共享 stop 在每个 def 实例各发一份；conic/diamond 的 stop 通道在 Plan 阶段按实例拒绝（Strict 拒绝、BestEffort 保留静态）；P3 动画 stop 报 `color-space`/Approximated 并去掉会覆盖轨道的 CSS 声明；新增 stop 双通道 / 跨 def / conic 拒绝 / P3 四个回归 | 重复/非单调 offset 与 stop 颜色混色的浏览器像素对照、stop 数量重排未做 |
| 本轮 M4.3（2026-11-11） | linear 四端点与 radial 中心/半径进入白名单；def 内直接发射 x1/y1/x2/y2 与 cx/cy/r；r 的采样值下限钳到 0（SVG 拒绝负半径）；静态已退化的 gradient 不承载几何轨道，Plan 阶段按实例拒绝并告警；conic/diamond 的几何通道在 ledger 标为 NotExpressible；新增端点共享 painter / radial 钳制 / 静态退化拒绝三回归 | 动态 bounds 与 stop 联动的浏览器像素门禁留到 M5.4；conic/diamond 仍为退化近似 |
| 本轮 M5.0/M5.1 部分（2026-11-12） | 新增 Path `point{N}.x/y` 表外通道族（不进入 175 条账本）：Plan 按实例校验 float 类型与点索引，导出在 `<path>` 上发射 `d` 轨道，逐帧求值点数组后按原 verb 结构重建，静态 renderPosition/renderScale 烘入每帧；Hold → discrete，单一或一致 Bezier → keySplines，混合缓动按 8 段降采样并告警；被 path modifier 烘焙、stroke align、adaptive dash 改写的 path 在 Strict 下拒绝并告警；新增 d 轨道 / Hold / modifier 拒绝三回归 | Polystar/Trim/Round 的逐帧重烘焙、point 原始空间与动态 bounds、Mask+modifier 的 d 轨道与浏览器像素门禁未做 |
| 本轮 M5.2（2026-11-12） | Text.x/y 进入白名单；导出在 Text 外层插入无静态 transform 的 `<g>`，用 animateTransform type="translate" 发射相对 authored position 的位移，原生 `<text>`、WOFF2、glyph path 三个出口共用；TextPath 重排与 TextBox 容器内的 Text 不假成功，Strict 拒绝并告警；新增 standalone translate 回归 | TextPath normal 沿曲线重求值未实现；TextBox 内逐 run 位移留待 M5.4 |
| 本轮 M5.3（2026-11-12） | TextHolder flush 改为 `TextLayout::Layout(..., false)` 强制从有效 TextGlyphParams 塑形，all-embedded 不再走 bounds fast path；有效空（runs 为空）调用新增的 tgfx `Text::clearTextBlob()` 清空 blob/anchors 并保留对象与 position；塑形失败清空并 LOGE，不再保留旧 glyph；显式 anchors 按 glyph 序截断/补零后随 setTextBlob 保留；新增空→非空、anchor 重排、embedded→新文字三回归 | tgfx `clearTextBlob()` 只在本地 `third_party/tgfx` 实现并重编验证，尚未回填上游依赖版本；ST21/ST22 新 glyph 像素门禁未做 |
| 本轮 M5.4 部分（2026-11-12） | 同一 node+channel 被多个 Animation 驱动时按同属性多 Clock 分类：BestEffort 保留后写轨道并告警，Strict 拒绝；Path point `d` 轨道增加 512 采样上限并按 budget 降采样告警；Text.x/y ledger 升级 Supported；新增 Plan 白名单↔ledger 双向门禁（Path point 族表外豁免）与 multi-clock 回归；任何被 path modifier / reversed 烘焙替换的动画元素不再静默冻结，Strict 拒绝且 BestEffort 告警保留静态；新增 A14 rect+Trim 回归 |
| 本轮 M5.4 Polystar（2026-11-12） | Polystar→Path 转换抽到共享的 `PolystarPath`（resolver 与导出共用），resolver 记录 bake→source 映射；rotation / 内外 radius / position.x/y / 内外 roundness 进入白名单并在烘焙后的 `<path>` 上发射 `d` 轨道；每帧用临时 Polystar 快照重算 content bounds、renderScale 与 renderPosition，逐帧强制 cubic verb 列表保持结构稳定；pointCount / reversed 仍按拓扑变化拒绝；新增 rotation d 轨道、roundness d 轨道与 pointCount 拒绝回归 | Repeater 拓扑、TextModifier/RangeSelector、动画 Text 的 flow/region 未做 |
| 本轮 T00 部分（2026-11-12） | tgfx 分支 `feature/lifengdy_svg_export`：`clearTextBlob`（`a9672fcd3`）与 `VectorLayer::prepareDraws` / `VectorDrawSink`（`75efe5d75`）；同步本地 vendor 后编译并通过 PAGX 相关测试 | DEPS 未回填精确 SHA、tgfx 单元测试未在独立构建中运行、未推送，正式依赖交付仍 Blocked |
| 本轮 T01 部分（2026-11-12） | 删除 `bakeUnsupported` / `convertTextToPath` / `resolveModifiers` 及 optional 映射；`staticPolicy` 默认 PreserveAppearance、`textMode` 默认 Fidelity；`SVGBudget` 增加实例、点、glyph、采样、细分、编码、边长与输出上限并在动画 writer 接入每轨采样预算；`SVGDiagnostic` 增加 outputClass / stateSource / consumedRangeOrFreezeTime 并在告警与拒绝站点填充；Native 模式跳过 WOFF2 预扫并在逐 glyph 场景回退布局字形并告警；`PAGXSVGTest` 280 / 281（唯一失败为既有未接受基线 `GroupAlphaRoundTrip`） | typed schema 区分作者类型/runtime payload、全会话计数与其余预算站点、Native TextBox 全出口未完成；`PAGXTest.SVGToPAGXAll` 为既有基线失败 |
| 本轮 T02 部分（2026-11-12） | tgfx 把 `Painter::draw` 抽成 `PainterSink` 遍历，`VectorLayer::onUpdateContent` 与 `prepareDraws` 共用同一准备流程；静态/导出构建新增会话内 TextBlob 与 Typeface 缓存，不再写回 `Text.glyphData` / `Font.renderCache`，runtime 构建保持原节点缓存契约；新增 tgfx `PrepareDraws` 与 `PAGXTest.StaticBuildDoesNotWriteTextProductsToTemplate`；`PAGXTest` 367 / 368、SVG/Utils 909 中仅两处既有基线失败 | 逐实例 actors、SVG 导出消费 prepared draws、Group 上行 alpha 与背景消费、Merge/Repeater 完整生命周期仍未完成 |

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
- [ ] 静态 55 / 335、31 / 111、40 / 155 声明集合与完整逐字段语义不遗漏；C01—C24、ST1—ST46 有直接验证；分类与真正实现 / pixel 状态分别登记。
- [ ] staticPolicy / textMode / D6 / D7 正交验收；背景消费、Painter lifecycle、资源链、Font 不可变 / 串行化、singular 与 atomic file 行为闭合。
- [ ] 动效 175 / 30、表外 Path / image / 无 writer 全分类；F1—F18 与动态 A 矩阵直接验证；每个真实支持 N/G 项有 typed apply、初态、连续误差 / 区间、边界及 browser 证据。
- [ ] Clock / pause scope、Layer 三层 priority / ordered mix、full turns / shortestArc、Hold / overshoot / empty / key boundary 与 topology 正确；Strict 无默认 PNG freeze，损失 / 拒绝可定位。
- [ ] 同输入重复导出与 ID / 资源稳定，无模板污染 / 无界 nodes；失败和全部写失败保旧文件按承诺测试，warnings sink 不改变成功。
- [ ] 共用工具、PAGX runtime、静态 SVG、PPT / HTML、全量与目标浏览器门禁通过；能力文档区分 classified、supported、approximate、B/MB 和 U。

### 4.6 接口收口的唯一执行决定

以项目“不保留向后兼容层”规则和第2.3节为准：本分支一次删除旧 `bakeUnsupported / convertTextToPath / resolveModifiers` 及optional映射；staticPolicy明确默认PreserveAppearance，textMode明确默认Fidelity，必要modifier总是解析。不增加V2开关、双轨或deprecated兼容字段，旧迁移A/C不再是续作选择。

同步SDK全部调用方、CLI映射、测试和生成能力文档；其他格式的text-to-path选项按原格式保留，SVG入口转换为明确textMode，不改变PPT/HTML契约。公开结构变化记录为需调用方重编的接口变更，不擅自修改产品版本号/发布包/推送。实现在本分支完整通过编译；是否发布新版本是独立外部动作，不能成为保留两套语义的理由。T01/T11执行此决定，不在阶段末重问迁移方式。

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

#### A.3 Node inventory（55 NodeType / 335 自有字段；保留原表格式）

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
| Layer | Layer | `include/pagx/nodes/Layer.h` | Node, LayoutNode | `name, visible, alpha, blendMode, x, y, z, rotationX, rotationY, rotationZ, scaleX, scaleY, scaleZ, skewX, skewY, perspective, matrix, matrix3D, anchor, anchorZ, preserve3D, antiAlias, groupOpacity, passThroughBackground, scrollRect, hasScrollRect, clipToBounds, mask, maskType, composition, compositionFilePath, externalDoc, timelines, vmContext, contents, styles, filters, children, layout, gap, flex, padding, alignment, arrangement, includeInLayout, importDirective, scrollRectFromLayout` | 47个自有字段（含派生scroll标志）按B.1分组；S1 / S2 / S5 / S6 |
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
| ST8 | `src/pagx/svg/SVGExporter.cpp:3335`TextBox富文本旁路；`src/pagx/utils/TextUtils.h:74`只非空text且丢内Group状态 | glyph-only/Rect/多Painter/transform-alpha/foreground双遍；S3 |
| ST9 | `src/pagx/svg/SVGExporter.cpp:2388`WOFF2多Font/advance/anchor不全；`src/pagx/utils/TextUtils.cpp:84`matrix不同、`src/pagx/utils/TextUtils.cpp:192`漏offset | 双Font、无position/xOffset、非零anchor/rotate-scale-skew/vector offset；S3 |
| ST10 | `src/pagx/svg/SVGExporter.cpp:2293`fill-opacity；`src/pagx/svg/SVGExporter.cpp:2335`image漏因子 | 真实PNG/WebP glyph、scope/Painter alpha、缺resource诊断；S3/S5 |
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
| ST32 | `include/pagx/nodes/Node.h:30`、`src/pagx/PAGXChannelTable.h:28`反射非全集 | 55/335+31/111+40/155集合与结构/None/derived输入，漏分类阻断；S0/S6 |
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
| F10 | `src/pagx/svg/SVGExporter.cpp:1794`Mask writer漏Plan、`src/pagx/svg/SVGExporter.cpp:3335`Native TextBox路径不全 | Mask内部、TextBox/祖先轨道到实际宿主且空间正确；M2 |
| F11 | `src/pagx/svg/SVGExporter.cpp:3406`只自身依赖、resolver产物失来源 | 内Group/Rect/Fill动态不单PNG freeze，替换geometry保依赖；M2/M5 |
| F12 | `src/cli/CommandExport.cpp:184`不收warnings | stderr损失可见、Strict非零、不成功提示掩盖；M6 |
| F13 | `src/pagx/PAGXImporter.cpp:1906`、`src/pagx/PAGXImporter.cpp:1933`typed parsing；`src/renderer/LayerBuilder.cpp:1239`、`src/renderer/LayerBuilder.cpp:1939`writer mismatch | 全typed XML→evaluate→actual apply；int/float/enum/imageRef≠runtimeImage、Path point长度与溢出，源修或U；M1/M5 |
| F14 | `src/pagx/PAGAnimation.cpp:59`、`src/pagx/PAGAnimation.cpp:111`merge/filter；`src/renderer/LayerBuilder.cpp:192`、`src/renderer/LayerBuilder.cpp:267`shared pose | 静态masked/同anim跨Object/跨anim ordered mix三层，matrix不屏z/anchor、后component decompose；M2 |
| F15 | `src/pagx/runtime/KeyframeEvaluatorImpl.h:106`T{}、`src/pagx/svg/SVGAnimationWriter.cpp:129`clamp key、`src/pagx/svg/SVGAnimationWriter.cpp:191`Hold近似；`src/pagx/runtime/BezierEasing.cpp:104`、`src/pagx/utils/ColorSpaceUtils.cpp:78`overshoot | 跨duration t0=50、空≠无、单/1-frame/D1/D0/nonfinite/duplicate ordered keys、float不clamp/Color分量clamp、No epsilon；M3 |
| F16 | `src/pagx/runtime/PAGComposition.cpp:57`、`src/pagx/runtime/PAGComposition.cpp:130`paused IDs共享 | 同slot同ID任paused禁全advance但apply全执行，兄弟slot独立、ordinal≠pause scope；M1 |
| F17 | `src/pagx/PAGXNodeChannel.cpp:486`175/30 registry；本次 `src/pagx/svg/SVGAnimationPlan.cpp:67`104/18白名单与 `src/pagx/svg/SVGCoverageLedger.cpp:30`实现状态 | M4.0—M5.4逐子集同步Plan、ledger和输出/拒绝门禁；颜色、stop、d、Text完整依赖不漏，Planned/Supported≠N/G证明；M0/M4/M5/M6 |
| F18 | `src/pagx/PAGAnimation.cpp:274`Clock、`src/pagx/PAGScene.cpp:514`宿主flush、`src/pagx/utils/RasterUtils.cpp:351`背景合成契约 | 多Clock无公周期、finite事件/3D/noise/complex fallback；D8禁用拒绝、明确MB初态/window/resources/resolution/sampling/tail/repeat/预算，无界U；M3/MB/M6 |

### 5.1 只读 coverage 校验与维护门禁

| 核查集合 | 已登记范围 / 只读校验要求 | 不代表 |
|---|---|---|
| Node inventory A.3 | 55/55，335 自有字段逐名 / 直接基类一致；private输入/缓存不漏，NodeType恰好一次 | 55类视觉或335字段等价输出已实现 |
| 辅助 A.1 | 31类 / 111自有字段，按表递归继承，非Node附属 / 派生完整 | 111项都是作者输入、非法Font热修改受支持 |
| enum C | NodeType之外40类 / 155值，与nodes/types/ChannelFlags集合一致 | 无限组合穷举或所有值pixel通过 |
| dynamic ledger 3.2 | 30类 / 175全名，与反射Anim集合一致；styles/filters51、Group/TextBox各10；表外另账 | 175全部支持、39白名单保真或B已经实现 |
| 组合 / 缺口 | C01—C24、A01—A22、唯一ST1—ST46 / F1—F18，无漏号 / 重号，阶段闭合 | 缺陷总数、修复数量或新测试通过数 |

重组后重新对照当前声明，55节点/335字段及继承、31附属结构/111字段、40枚举/155值、30类/175动效通道全部一致；ST/F/C/A编号唯一完整，D决策、代码块与章节引用已核对。文中file:line引用逐项检查了文件存在与行号范围，并经独立语义复审；这些检查不执行渲染，也不证明所有引用行为已运行复现。源码新增声明使旧计数失效，必须重新枚举；未分类项先阻断，不能静默default。

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

## 6. 新终端全量续作方案（T00—T12，执行权威）

### 6.1 基点、现状和必须完成的边界

本节依据 `aeb346c05` 整体核查；编写期间新增 `c60e9e2e2`（四种阴影color）、`6a0d91368`（NoiseStyle.seed）与 `c53424cad`（Noise颜色轨道），已接入的子集续作保留，但Noise轨道存在不等于像素算法已等价。335字段计数已按此阶段声明含Layer.scrollRectFromLayout重新核对，不沿用历史334。启动时重新读取HEAD和相关diff；进度日记与ledger是线索，不是验收结果。本次只整理文档，未执行源码修复或渲染。

当前需要保留的有效进展：Default/Named/All入口、全反射ledger与白名单双向检查、CLI warnings、三静态policy与文字mode、ST28最终尺寸/空子盒分配、TextHolder强制塑形/清空/保anchor、颜色源/stop/渐变参数、固定拓扑Polystar/Path/Rectangle/Ellipse+Trim/Round重烘焙、纯旋转整圈、Bezier加密采样、Mask上下文、Native TextBox变换、同目录文件替换和少量浏览器门禁。

**这些子项均仍须经下面的整体契约复核。** 需要修的主线是实例与独立驱动、最终Painter捕获/alpha、资源扇出、文字/沿线单位、Mask根/Contour、effects输入/背景合成、图案资源/采样、连续编译/拓扑、策略与全会话预算和正式依赖交付。

| 范围 | 本轮完成的定义 | 不允许的替代 |
|---|---|---|
| 静态 | 附录A/B的全部有效绘制成员有正确原生/最终几何输出，或按既定复杂效果分类完整bake；三policy和有效空/失败均可判定 | 有原生可修缺陷时永久用截图掩盖；PreserveAppearance失败后矢量成功 |
| 动效N/G | 第3.2/3.3节的有效2D变换、shape、source/stop、可等价effects、d、Text、Selector/Modifier、有限Repeater/pointCount实现并联合验证 | Planned转NotExpressible、只加白名单、固定8段/32点即声称有界、以拒绝全部正例交差 |
| 动效B/U | D8禁用；真正一般透视/深度、动态Glass/backdrop、非等价Noise等B场景及开放宿主U具备确定拒绝/显式BestEffort损失 | 开启未授权MB；把静态PNG冻结说成保留动效；把算法尚未写归为B |
| 正式交付 | 源码、tgfx修订、DEPS锁定、全套构建和测试同一版本可复现 | 仅本地vendor补丁编译；只报告旧测试结果或屏蔽新增失败 |

一般3D轨道整段可证降为2D/常量w时仍实现N/G，不因字段名含3D一律拒绝。没有writer的静态字段、不可逆恢复的源SVG参数不新增隐藏驱动；它们的拒绝/主动排除属于完整契约。

### 6.2 连续执行、记录与暂停规则

1. 开始就建立T00—T12全部任务及依赖；每完成一步更新状态，**继续下一步而不是发送进度回复**。源码修改、函数/接口和新增测试记录在任务/本文第6节，不持续追加每日流水账。
2. 连续实现，测试随功能编写。统一验证在T12；只在确实阻塞后续编译的接口/依赖探针时提前构建。正常失败先定位根因再修，不通过强转、删代码、跳hooks/基准绕过。
3. 每项状态使用 `Pending → ImplementedUnverified → Verified`，另有 `Blocked`。不得用Supported计数替代Verified；某通道基本实现但组合未通过时不能完成对应任务。
4. T01/T08/T11会暂时同步改schema/ledger/Writer/生成文档，直到T12通过前不对外宣称新增支持已验收。最终一次集中汇报完整改动、门禁和真实限制。
5. 只有外部权限、覆盖用户工作、依赖发布、受保护基准审核，或证据表明原定算法无法满足且必须改变契约时暂停。先列可独立继续的未阻塞工作并完成；不能在普通“某部分已做好”时让用户决定继续。
6. 启动时查主库和所有要修改的依赖仓库工作区；不自动stash/reset/checkout/覆盖用户未提交内容。不同终端同时修改同一工作区是实际阻塞，先由用户停止其他实现进程，不抢写、不杀进程。
7. 不推送、创建MR/PR、安装系统包、改共享服务、接受基准。需要这些外部动作时明确审批范围。自动本地提交只含本执行者实际修改，按仓库各自规则，不含他人未提交状态。

### 6.3 唯一内部接口和数据责任

以下是续作需要实现的接口草案，不是当前已有接口；按现有模块落地，只在实际需要分离源码时新增编译单元。禁止创建空helper、重复解析器或第二套通用场景框架。

```text
ExportSession:
  immutableSourceIdentity, exportOwnedState, instanceGraph
  effectiveFontConfig, consumerResourceResults, driverGraph
  preparedDraws, diagnostics, oneSharedBudget, outputResult
ValidateReachableInput(document, options, budget) -> Valid / Failed
BuildInstanceGraph(document, resources, budget) -> InstanceGraph
BuildExportActors(instanceGraph, stateSource) -> isolated runtime actors
PrepareDraws(actor, mode, sink, budget) -> Prepared / ValidEmpty / Failed
SampleEffectiveFrame(driverGraph, timeUs, budget) -> FrameSnapshot / Failed
CompileTrack(expression, clock, budget) -> Native / CertifiedG / Rejected
EmitPreparedDraw(draw, options, budget) -> output / ValidEmpty / Failed
BakeScope(sourceActors, consumptionPlan, budget) -> raster / Failed
WriteAtomicFile(bytes, target) -> Success / Failed
```

- InstanceKey固定 `(ownerDocument, stableInstancePath, sourceNode)`，SourceId是模板身份，RenderId是实例/副本身份；cache不跨实际消费方或实例错误共享。
- PreparedDraw持有shape/blob/图片等产物，含源/副本身份、captured inner和final矩阵、Painter来源/实际paint、base/overlay角色、fit矩阵/bounds、placement/order、有效覆盖及依赖。**owned typed描述必须在源prepare时生成：** primitive/path/glyph几何参数/verb与其空间快照、SolidColor/Gradient/Pattern参数及颜色空间、实际endpoint身份与不可变参数值、base/overlay分别的alpha表达式、创建→替换→复制provenance。endpoint只作身份，不能延后读其已被下一探测帧改写的值；不能逆解LayerRecord/Shader补这些信息。overlay按第2.8.1节不再乘原ColorSource alpha。临时Geometry指针不能在VectorContext销毁后悬挂。
- 实际SVGWriter只序列化PreparedDraw/CompiledTrack，不在close tag时补优先级、透明度或插值。有效空是成功的无贡献，不是资源/采样失败。
- 同一预算被全部子Writer、实例、Mask、defs、glyph、采样与bake引用；子对象不能重新获得完整额度。所有失败传ExportResult，不能只有warnings文本。
- 资源保留消费者类型：ImagePattern允许runtimeImage优先；bitmap glyph和默认VM沿data→dataURI→file；不统一改变公开来源顺序。runtime可写状态按实例独立，源模板和Font缓存不被采样改写。

### 6.4 T00：清理执行前置和闭合正式tgfx依赖

**文件/入口：** `DEPS`；实际tgfx仓库的 `include/tgfx/layers/vectors/Text.h`、`src/layers/vectors/Text.cpp`；后续T02需改的VectorLayer/Painter共享准备接口。默认独立源码调试目录按项目规则选择，不覆盖常规构建。

1. 当前 `third_party/tgfx` 是独立Git依赖，锁定 `24bcc1109601c3dc849f9b0bba3bda2ee21e439c`，Text.h/Text.cpp的clearTextBlob仍为用户本地修改；先读取diff核实归属，**不自动把别人未提交文件合入本执行者提交**。
2. 如同级tgfx开发仓库存在，先读它的规则和工作区，再对照依赖补丁；实施clearTextBlob及T02必需窄接口的正式源码修订，不把改ignored vendor当正式交付。需要外部工作区权限时走正常授权。
3. 本地源码编译可用 `-DTGFX_DIR` 指向正在开发的实际tgfx源码，独立构建目录；记录tgfx SHA与dirty状态。这个结果可用于后续编码，**不能认证默认DEPS构建**。
4. tgfx修订在正确仓库形成独立本地commit后，libpag DEPS回填精确SHA，不能猜future SHA或只更新头文件缓存。若标准依赖下载需要远端可取commit，发布/推送需用户一次明确授权；没有授权不发布、不标正式Verified。
5. 若依赖发布是唯一外部前置，继续完成T01—T12的其余本地源码与源码链接验证，最终一次报告“正式依赖交付Blocked”及确切tgfx SHA、发布动作和未通过的默认构建。不在每个批次反复报告同一个阻塞。

**完成条件：** clearTextBlob及共享准备接口在正式源码中有调用和单元测试；源码链接与DEPS常规构建都使用相同tgfx修订；不能靠setTextBlob(nullptr)、缺省参数、移除调用绕过。

### 6.5 静态续作 T01—T06

#### T01：typed入口、policy、诊断和全会话预算先落地

**文件：** `include/pagx/SVGExporter.h`、`src/pagx/PAGXImporter.cpp`、`src/pagx/PAGXNodeChannel.cpp`、`src/renderer/LayerBuilder.h/.cpp`、`src/pagx/svg/SVGAnimationPlan.h/.cpp`、`src/cli/CommandExport.cpp`、`SVGExporter.cpp`及全部调用方。

- 新schema区分作者编辑类型与runtime payload：float/bool/int/string/Color/Matrix/Matrix3D/ImageRef/宿主PAGImage；enum作者名在合法边界转换为确定runtime枚举，不在Writer强取错误variant。forward reference在节点/ID解析完成后按所属实例文档校验。
- 无type的关键帧按目标channel schema推断，合法整数文本可进入float通道；显式type与目标不匹配时拒绝并带node/channel/sourceLine。ImageRef须解析成对应消费者需要的PAGImage再apply；保留runtimeImage来源与VM默认图片的差异。
- Path `point{N}.x/y`检查数字累乘溢出、N范围、数据类型、verb所需点数；schema不存在且有效writer也不存在时明确U，inactive/masked/no-op不要错误触发拒绝。
- 按4.6唯一决定删除旧三字段和optional兼容映射；不要因迁移便创建V2/双轨。默认staticPolicy=PreserveAppearance、textMode=Fidelity、animationPolicy=Strict；SDK/CLI相同有效选项，不忽略冲突参数。
- 一个诊断状态保存Failed，不依赖sink：所有失败带reasonCode、实例路径、sourceId、channel/输出、动作、具体预算或时段；warnings只是文本投影。未知input/errors/unresolved imports在递归/布局前处理；合法空画布按既定API保持有效SVG或明确无效，不能混淆。

**本轮固定预算（可显式设更小值测试，不借无限值绕过）：** 最大引用深度128、实例10000、总几何点/最终glyph各1000000、生成绘制元素200000、每轨采样4096、整图采样65536、证明细分深度24、联合周期展开最大10000段、总bake像素67108864、单边8192、编码及输出各67108864字节。沿用已存在budget字段，其余只暴露调用确需的上限，其它内部集中默认；计数用checked add/multiply，ceil(copies)计实际分配，不用float转floor。任一耗尽是Failed，不截断/清scope后成功；解码、glyph构建、复制、Surface和XML扩容**之前**消费。有意提升预算不等于放宽误差。

**测试：** typed XML→evaluate→实际writer正/反例，forward/external引用、合法NaN unset、Unknown target、mask/defs/tile共预算、每种policy的资源/GPU/编码/输出失败、warnings=nullptr成败一致。

#### T02：逐实例运行树和最终Painter准备出口（关键层次修复）

**文件：** `LayerBuilder.h/.cpp`、`SVGExporter.cpp`、`ModifierResolver.h/.cpp`；tgfx `VectorLayer.cpp`、`vectors/VectorContext.h/.cpp`、`vectors/Painter.h/.cpp`、`FillStyle.cpp`、`StrokeStyle.cpp`及窄prepared-draw接口。

固定路线：将tgfx现有 `VectorLayer::onUpdateContent` 和 `Painter::draw` 的“全部element apply后→ surviving Painter prepare→Recorder”准备流程抽成一个共用出口，渲染与导出消费同一准备结果。**不从Shader/Picture反解，不另模拟一套VectorContext。** 使用受控sink/visitor或持有的PreparedDraw列表，预算参数只负责分配拒绝，不能改变结果；该接口对所有返回值有真实调用，不留空API。

```text
context = apply all enabled elements in author order
for surviving painter in actual draw order:
  for each captured geometry and contiguous styled glyph run:
    prepare with the existing shape/glyph hooks exactly once
    forward the owned result and base/overlay metadata to the sink
record surviving geometries no painter captured as own-geometry, not stage paint
```

- provenance从源VectorElement/Geometry创建、modifier替换及Repeater复制传递，建立endpoint→源节点/实例映射；没有stage paint的own geometry仍供Glass/contour等消费，不默认画黑。
- runtime原Recorder接同一prepared出口，保持原矩阵/style/paint和合法空规则；不要为了SVG改变正常非导出渲染分支。
- 局部BuildForRuntime/BuildCompositionSubtree逐slot建独立bindings，保存instance层父链；第二遍连接mask引用。**这些函数当前仍可写共享Text.glyphData/Font.renderCache，不能仅独立binding就声称输入不变：** 固定给LayerBuilder/GlyphRunRenderer构建context增加session-local glyph/layout产物与Font构建cache的内部入口；export路径用独立cache和状态，已有不可变Typeface/encoded资源可安全共享，普通非export路径保持既有cache契约。private布局状态独立持有，源document/Font/Text作者字段和共享cache不写回。TemplateStatic不初始化VM/SM/timeline，采样后续T07加入。
- Group只给内部Painter乘alpha，上行geometry不携带alpha给外Painter；captured inner矩阵与最终矩阵都有来源，不从singular最终矩阵求inverse恢复。Layer整体/逐项alpha按实际needsOffscreen条件，不用“有style必group”推断。
- Merge真正清旧geometry/Painter，Append保持PathOp::Append；Repeater使用原始捕获关系、实际ceil副本和顺序，后续Trim/Round能继续修改存活geometry。删除SVG“遇Painter立即emit”路径和局部克隆Fill回插，避免ST21后置modifier漏旧draw。

**测试：** `Group(Rect,alpha=1/2)`后外部Fill应保外Painteralpha；同Group内Fill为1/2；Text→Fill→Modifier影响该存活Painter；Merge后无Painter不绘制；Rect→Fill→Rect→Merge→Stroke只剩后画笔；复杂capture、连续Repeater、singular前/后Fill/Stroke/Mask直接对照源renderer，不用导出的roundtrip当原PAGX基准。

#### T03：完整静态glyph、Selector、TextPath和Native TextBox

**文件：** `TextUtils.h/.cpp`、`TextPaintUtils.h/.cpp`、`TextPathUtils.h/.cpp`、`TextSelectorUtils.h/.cpp`、`TextLayout.h/.cpp`、`GlyphRunRenderer.h/.cpp`、`TextHolder.h/.cpp`、`SVGExporter.cpp`与相关tgfx共用最终glyph准备。

- 复用T02最终styled glyph/run，不再按每Text独立force或统计字串长度。索引跨scope存活geometry/run连续；geometry矩阵M、glyph矩阵G、font designScale、实际user-space anchor分清，不能让已缩放anchor又被fontScale乘一遍。
- Selector直接复用tgfx factor/shuffle算法或抽共用纯函数；Index先start/end除glyph总数再加offset，首selector/mode与最终clamp、uint16 seed及mt19937排序完全同源。删除LCG的“更可复现”替代语义，不只对齐seed0首字符。
- 第2.8.1的0/部分/1权重分支在每个Painter/run准确执行，base原shader/原blend，overlay SrcOver，alpha各自施加；styled Stroke实际width/cap/join/align/pathEffect两分支一致，无Painter不新增上色。保持bitmap与outline顺序、原glyph.offset/faux、alpha含Painter因子。
- TextPath只运行一次整个当前scope的glyph列表，严格按2.8.2的TP01—TP16及M可逆规则；force使用所有advance统一extraGap；normal投影真实anchor，perpendicular/反向法向分离。曲线PathMeasure直接用tgfx，不凭采样点另写弧长。
- ST28已有最终resolved方法和空子盒分配保留，逐轴0/NaN测试复用；Native TextBox不能另走单一Fill/Stroke简化收集，非Text子geometry、多Painter和placement均走同一context。有效空布局/Hidden全丢直接无绘制，不fallback画原文。
- 有效FontConfig固定 `options.fontConfig != nullptr ? options.fontConfig : &doc.fontConfig()`，布局、TextHolder/Native再塑形、external字体和bake共用。源Font.glyphs不可热改；如需private构建state复制cache，不与其他scene抢写；外部调用串行约定清楚。
- Native保原Unicode，但只有实际字形/布局可证一致才走Native；否则Fidelity正确路径/按policy拒绝，不凭styled family字符串保证字体选中。字体缺失、有效空和塑形失败分开，不把missing fallback当成功。

**测试：** TP01—TP16全部数字及多Text/multiFont force、unitsPerEm≠fontSize、nonidentityM；ST21颜色分数/非Normal/gradient/pattern/后modifier；7数组空/短/全、bitmap/vector交错、vector offset/faux、Hidden横/竖空、native run transform/foreground只一次、doc-only字体与external同配置；TextHolder故障传session，clearBlob空→非空保identity/position/anchor。

#### T04：各形状描边、dash、modifier与数值精度

**文件：** `StrokeGeometryUtils.h/.cpp`、`ModifierResolver.h/.cpp`、`PolystarPath`实际模块、`SVGPathParser.cpp`、`StringParser.cpp`、`SVGExporter.cpp`及PPT/HTML相关消费者。

- T02准备结果直接作为正确几何，Rect/Ellipse不能继续inset近似。Center能保持原始几何属性时原生；Inside/Outside采用原轮廓倍宽stroke+clip或等价stroke轮廓交/差，style/dash/方向/coverage与tgfx一致。fit仍是原stroke前shape bounds，不拿扩展stroke bounds拟合。
- 普通circle/ellipse/rect方向、顶部起点、adaptive和cap简化全部使用原Path/PathEffect；零尺寸primitive微轮廓与不可逆ApplyMatrix无geometry分开。不把不透明Fill/半透明Stroke合成成一条路径省掉真实draw次数。
- Trim Separate/Continuous跨Group、Text→Shape、Merge、Repeater全部作用实际存活geometries，保留modifier order和来源；PPT的Append→Union补偿移回PPT端，不改共享语义。
- 不在源doc.nodes临时追加后依赖全局末尾删节点；arena自己持有，生命周期records→actors/holders→resource→GPU，有失败也释放。源Fill/PathData/Font/nodes/索引不污染。
- 浮点序列化用能roundtrip的通用格式，允许科学计数法；不将科学数统一固定9位抹掉小值。conic→quad转换用几何误差/控制多边形界和预算细分，阈值引用源常量，不另设1e-4近似谓词。

**测试：** 3align×3cap×3join×primitive/path/glyph典型闭/开/孔轮廓、adaptive/奇数dash/Trim顺序；微轮廓及singular；Append重叠stroke、不均匀scale+外Painter原fit、Q/C控制点bounds与coverage、小差值/大数/scientific数、SVG→PPT→SVG与重复释放。

#### T05：静态Mask、effect图、背景与3D完整保外观

**文件：** `SVGExporter.cpp`、`SVGFeatureProbe.h/.cpp`、`RasterUtils.h/.cpp`、`LayerBuilder`与tgfxLayer/LayerRecorder对应源模式。

- 普通stage owner visible=false就是隐藏；Mask引用根遵守tgfx专用绘制模式，不因stage hidden省覆盖，root alpha/effects/children/composition/scroll/其自身mask保留。Contour按源模式排除effects并抽有效paint覆盖，不假定一律忽略全部alpha；stroke-only提取真实stroke环，afterVisible判定与源一致。
- 相对Matrix由**实际实例**父链和getMatrixWithScrollRect等源规则计算，cache含mask/owner实例、mode、space和generation。不可逆相对矩阵按源有效空/失败处理，不悄悄returnidentity；不能先锁第一模板parent。
- effect构图按源Layer真实阶段/输入bucket和作者顺序，Below→default/children→Above→foreground→ordered filters及外层合成；每filter从前一步输出提alpha、改outputOffset和bounds，不固定SourceAlpha。excludeChildEffects的真实独立输入不能从最终合成图事后删除。
- 普通zero-spread Shadow/ColorMatrix/Blur和可等价Blend保原生；spread/behind先用源SpreadUtils/contour/bounds算法准备，非等价morphology不得标Exact。Noise/tile/Glass等像素算法在static PreserveAppearance进入正确完整bake，VectorOnly拒绝、BestEffort才显式损失；原生简单场景有正例。
- 背景依赖固定“原合成空间内的闭合范围替换”：保存Layer阶段事件及合成栈，patch在其所属空间替换原draw范围。**不能切开尚未闭合的groupOpacity/filter/mask合成**，否则整体alpha被变成前缀/后段各一次；发现此情况必须扩大到该完整闭合作用域并合并重叠消费范围，包含其本来同槽输出的后续内部事件，不能再另外发射它们。外部背景依赖上移到能封闭依赖的祖先，最远根；只有合成栈允许独立切分时才渲染prefix+目标并继续其后事件。prefix按实际阶段而非siblings，范围外祖先Above/foreground不提前纳入；不直接render整棵root丢掉后续顺序、不把patch叠回原背景。后续背景目标始终从source actors重建有效范围，不从patch反推。
- samplingHalo与替换coverage独立；已有prefix范围不能因只烘目标bounds而丢远处内容。最远完整root前缀可涵盖整画布；优化局部区域须证等价，不能先错用局部patch。连续背景效果和3D island按原depth/flatten规则成块，alpha/mask/scroll/effect各只应用一次。
- 静态bake资源/GPU/预算/编码/无法满足外观时Failed，PreserveAppearance不继续vector。动态B按D8拒绝；这个静态兜底不授权冻结动画。

**测试：** C03—C11，root mask alpha0/0.5和Composition/scroll/effect、Contourstroke/hole/hidden后代；两级groupOpacity及18blend真实背景；Blur→Shadow/Shadow→Blur/Only非交换链、20项Matrix、±spread塌陷/behind/exclude；小bbox大halo/offset累计；两个背景效果、透明背景/祖先foreground、一般投影与mask/scroll/3D交叉，failure无输出。

#### T06：渐变、图案及消费者实际资源快照

**文件：** `ImageFormatUtils.h/.cpp`、`RasterUtils.h/.cpp`、`LayerBuilder.h/.cpp`、`SVGExporter.cpp`、`Woff2FontGenerator.h/.cpp`。

- ResourceResult固定 `Ready / ValidEmpty / Missing / Invalid / Failed`，含decoded image实际width/height、encoded bytes/mime、colorSpace、采样输入和来源generation；href与dimensions必须来自同一实际资源。GPU-only runtimeImage读回编码使用正确设备上下文与预算，不回模板图片假成功。
- 保持每消费者链；可复用decode/encode但不共享错误優先级。所有supplier已经供应后再布局/bake/Native，缺失外部资源/字体/import preflight拒绝，不自动联网猜图。
- Gradient空stops采用builder黑白；单stop、重复/边界/无序按源规范化、linear/radial/conic/diamond的退化常量/无效规则用源谓词。shape用实际Path.getBounds、glyph用准备时的tightBounds，fit/outer空间与静态geometry一致。RGBA、mixed/P3空间转换及premultiply遵守实际shader，不能把时间LerpColor当空间gradient算法。
- ImagePattern先matrix变换imageRect，再用源getFitMatrix；完整旋转/skew/负scale和4scaleMode适用全部16tile组合。能原生等价的双轴Repeat/Decal输出局部pattern+coverage；需要mirror/clamp/无法原生sampling的场景复用原shader生成有限有效pattern patch，受staticPolicy/budget共同约束。VectorOnly不可从writeImagePatternDef旁路新增效果位图。
- sampling保存FilterMode/MipmapMode并透传native/bake；image-rendering提示只在验证等价的目标环境用。非等价sampling进入static bake，不把全部nearest/linear/mipmap统一忽略。

**测试：** C17/C18，四gradient类型及空/单/重复/退化阈值两侧、透明内部/P3混色；16tile×4fit×6sampling的数据驱动真实纹理/透明边缘/放缩/旋转；四真实编码解码或明确codec不支持、dataURI、runtimeImage蓝覆盖data红但glyph消费仍红、GPU-only、external同名，VectorOnly/PreserveFailure。

### 6.6 动效续作 T07—T10

#### T07：真实实例DriverGraph、独立帧oracle与资源扇出

**文件：** `SVGAnimationPlan.h/.cpp`、`PAGAnimation.cpp`、`PAGComposition`runtime、`LayerBuilder.h/.cpp`、`TextHolder.h/.cpp`、`SVGExporter.cpp`。

1. 先选根集合：Default文档第一timeline是StateMachine时拒绝；Named只指定顶层Animation且未知ID失败；All只顶层Animation声明序，不把未选SM全局拒绝。**无根动画仍扫描实际Composition挂载，绝不按document.animations.empty提前返回。** root选择不覆盖nested driver的offset/playing。
2. 每个实际Composition实例独立binding/contextDoc；普通Layer无composition不spawn。DriverKey含slot实例+ordinal，PausedKey含slot实例+animationId；同ID任一个paused禁该slot全部同IDadvance，但全部apply，兄弟slot独立。
3. resolve先contextDoc查类型再contains。对同Animation跨Object的同target合并channels后做原FilterLayerChannels；matrix3D屏matrix/x/y/z/components，matrix屏x/y/components，anchor/z例外保留；跨Animation按writer顺序更新共享pose，不能按channel去重。静态matrix masked与混合后需要decompose的runtime规则不另重写。
4. 保留当前每Painter独立ColorSource endpoint，资源writer扇出到该实例的实际对象去重集合；一次apply每endpoint mix一次，不重复混合同对象。stop映射实际gradient/index，资源值读回只有全部一致才返回；分歧reader返回明确Disagreed，不任取last。局部Painter.color仍只写自身，资源channel更新所有endpoint，解绑/重建reverse索引同步。
5. FrameOracle每探测时刻从同一private初态独立创建或完整重置actor状态（全部pose、endpoint、holder有效属性/dirty/anchors、clock/playing与glyph/prepare产物，不仅reset时间），使用T02局部cache入口；根set/apply→nested按目标elapsed/paused求content/apply→TextHolder真实flush→T02 final prepares。不累计上一probe的mix，不额外先apply(t=0)再apply(t)改变mix初值，不修改作者Layer/Trim/Path字段临时还原。
6. TemplateStatic与动画编译从已选模板初态驱动；默认VM/SM历史不初始化。若以后显式host-init则先全部referenceInstance链接→bind→首次flush；本轮不扩快照/MB。FrameSnapshot观察真实运行对象与最终PreparedDraw，不能scene.apply后ToSVG原模板。
7. content clock复用PAGAnimation.apply的整数微秒fold/subtract，不用playbackPosition报告值。初始SVG属性为完整t=0求值；Once正offset之前保持正确首值，Loop正offset原点模回绕；paused导出该驱动的固定求值而不是beginindefinite。Loop/PingPong轨道从原点相位完整编排，唯一周期D/2D只算一次。

**测试：** A01—A05/A07/A11，多slot各相位、external同名、同IDpaused、ordinary误挂、根缺省/Named/All与SM、同target跨Objectpriority、资源源/局部Painter交错写、多endpointmix=1/2与readback分歧、probe时刻乱序结果一致、t=0/delay/offset/2nd period、source snapshot不变。

#### T08：统一精确/N与CertifiedG编译器，禁止固定采样冒充证明

**文件：** `SVGAnimationWriter.h/.cpp`及必要内部表达式/区间编译单元、`PAGAnimation.cpp`的纯time函数、`KeyframeEvaluatorImpl.h`、`BezierEasing`、`MatrixDecompose`、测试。复用实际求值器；新增内部证明算子不另建输入解析器。

- CompiledTrack固定值/共享Clock/有序segments/initialValue/证书/representation；支持空Channel的T{}、单key、duplicate ordered time、duration=1和跨duration真实端值。无效时间/类型在T01拒绝，不抹掉合法边界。
- 原生标量操作能保留则保链，不先matrix化标量rotation；纯和多分量rotation均保持full turns。固定Matrix每段decompose并按MixDecomposed最短弧展开，不把170→-170交给SMIL长弧；cross-writerpose以T07观测为准，singular区间不借inverse静态差量。
- Hold/None及复合属性跳变在每事件处有左/右完整值。将连续区间与离散事件分段，使用互斥半开gate承载该段连续SMIL或定值；不插“临界点之前很近”的假渐变，不把混合连续段整体discrete。Once终点和Loop接缝严格按源内容时钟；空透明状态有实际定值。
- Bezier裁剪按X(u)=区间边界根做参数隔离，de Casteljau截取并归一化handle，不能给新边界硬设Linear。float保overshoot；Color在真实空间转换后按分量clamp，clamp/绝对值/min/max事件分区。SMIL不接受的handle不夹控制点，转G。

**CertifiedG的具体算法：** 输入表达式节点支持常量、time映射、标量keyframe、加/乘、除法、clamp/abs/min/max、sin/cos、合法正底幂和固定topology控制点/矩阵组合。每节点返回向外舍入interval及可用的一/二阶界；float求值/格式化误差保留证书q。

```text
queue = intervals split at all keys, jumps, clock folds and structural events
for interval I:
  use Linear interpolation between serialized endpoint values as the CertifiedG candidate
  bound residual F(t) - SVG(t) over I
  if the candidate is Linear and the smooth derivative bound is valid:
    upperError = sup(abs(F_second_derivative))*width(I)^2/8 + q
    q includes both serialized endpoint residuals
  otherwise:
    bound residual by parameter subdivision / interval arithmetic / Bernstein hull
  if upperError <= tolerance:
    accept I with certificate
  else:
    split I, consume shared budget, continue
  if invalid input or budget exhausted:
    return Rejected with operator, interval, error bound and budget
```

上述h²/8只认证Linear候选；如果另保留非线性SVG候选，必须对R=F-SVG本身及两端残差求区间界，不能用F''=0接受错误非线性曲线。Bezier X'下界为0不能除法求导，也不能直接判“无法支持”：先用参数根隔离/残差interval细分；fixedpath对应控制点误差用Bezier凸包界，传播完整matrix到文档坐标。量化q包含时间微秒、runtime求解误差、float round及输出数字重建。**有限探点只作验证，不作全区间证明。** 候选缺证明实现是Pending/Blocked，不把整类转B/U并标完成。

- 独立属性保独立Clock；同表达式多个周期优先结构分解，可有界联合时按checked有理frame/fps时间统一并限制段数；没有有限可展开/受控表达的真正B场景D8禁用拒绝，不无限LCM。
- 不删除中间key或跳变以满足32/512上限，删除固定DensifyFrames当唯一正确性依据；已有8段可作初始候选网格，最终由证书决定增点/接受/预算拒绝。
- 不能声称同一参数0.25px证明所有像素相等；颜色/alpha按1/255，几何及targetscale按文档坐标证书，最终图像误差由T12另验。

**必须有的正例与反例：** 多easing+anchor/skew/非均匀scale+720rotation、matrix170→190、中途scale0恢复、三alpha因子零→1、Hold+Linear+Bezier、overshoot、keys[-10,0]/[10,100]原点50、空/单/重复keys、X'零端点、exact/CertifiedG证书重建；人为变坏算子/序列化应击穿测试，budget耗尽确定拒绝，不只数采样分隔符。

#### T09：颜色/stop/可等价effect与最终动态依赖统一

**文件：** `SVGAnimationPlan`、`SVGCoverageLedger`、`SVGAnimationWriter`、`SVGExporter`、`LayerBuilder`、`RasterUtils`。

- 依赖固定T02/T07资源endpoint图→SolidColor→ColorStop→linear/radial参数→可等价effects；现有shadow.color正确子集保留，但复查前input/空间/alpha/region与T05一致，不重新做重复功能。
- 对每个PreparedDraw编译完整paint表达式：所属scope/painter/color/glyph alpha的全部有效因子、源与Painter写入顺序、ST21分支，零起点不退出，不因Painter已写fill-opacity跳Group，Layer groupOpacity=false仍保持真实合成。
- stop按实际消费者gradient/def实例，不按一个source节点全局共享；动态geometry bounds与gradient fit、退化常量/非退化转换共同分区，不凭静态首态“退化”拒绝后续有效正例。offset有效排序和空间颜色保持源算法，不自己换stop顺序吞掉不同定义。
- effects依据T05有序图，Blur/Offset/ShadowColor、Only/behind离散属性与可等价spread/ColorMatrix/Blend充分表达；full-time region用证书bounds而非只max key值，overshoot不能裁。inactiveNoise/no-op分开；真正Noise算法/动态背景B保持拒绝，不能feTurbulence.seed有轨道即声称整Noise保真。
- dynamic closure遍历到最终shape、所有源/consumer、modifier、Mask、Composition、effects/foreground；任何bake单元存在有效dynamic时必须走N/G编译或B/U处理，不能只HasLayerAnimates决定PNG。StaticConic与其层alpha/transform动态如能“静态patch+真实可证外动画”可分解，不能冻结实际会变化的patch内部。

**测试：** source/LocalPainter同时写、3+alpha并且都有不同key/easing、双实例defs、stop+endpoint+d bounds联合、由退化到非退化、透明/P3内部、Only/behind与shadow链、effectovershoot全时段、静态复杂patch外alpha与内部dynamic判定，输出/损失/拒绝状态均直接验证。

#### T10：完整d、动态glyph、Selector/Modifier和有限结构变化

**文件：** `ModifierResolver`、`TextLayout`、`TextHolder`、`TextPathUtils`、`TextPaintUtils`、`TextSelectorUtils`、`SVGAnimationWriter`、T02 prepared出口及相关测试。

- 一次FrameOracle同时采所有有效source/依赖，再跑同一最终prepare；不对每个Path只暂存一个source+Trim/Round，Merge/Repeater/多shape/多Trim/Round所有绑定共同作用，Polystar先过真实modifier链再输出，不能直接d优先绕链。
- Fixedtopology Path point/形状/Polystar/Trim/Round控制点、原空间/renderScale和paint fit一并编译。Trim证书对象是源tgfx PathMeasure/getSegment实际输出：复用其测长/截段/阈值分支并纳入近似与舍入误差。de Casteljau弦长下界/控制多边形上界只辅助定位interval，不能用数学真弧长替换源算法再称一致；最终控制点残差对源截段求界。RoundCorner同样复用源退化/截断阈值，对角度/半径/分母和几何分支返回保守区间。boolean曲线仅凭同构key样本不证明连续topology，须隔离交点/接触事件并验证interval稳定；缺这种算法不能把常规链永远排除。
- Text.x/y先完成无沿线普通消费者，沿线normal重新投影曲线为G，force对该消费者位置无贡献，但其它consumer独立保留。TextBox各实际glyph出口必须通，不一律拒绝容器。
- 七shaping通过实际TextHolder完成一轮所有writer后只flush一次，保空/失败区分、anchor/identity/position；字符串/fontStyle/fontFamily/faux等离散状态可预构造有效字形并用互斥矢量gate，无需raster。连续fontSize/letterSpacing固定font/GID时按设计轮廓/advance组成表达式并隔离换行/替换事件；字形hint/后端不确定区间须证资源/模式稳定，不能有限探点认证。
- TextModifier13项/RangeSelector6项使用T03同源factor/最终paint，glyph数量变化时重算index域；TextPath所有scope glyph共同算，顺序仍真实。静态TextPath参数虽非Anim，其path数据/目标glyph动态依赖仍需重求，不强行给无writer字段新增动画。
- Repeater连续copies按n=ceil(copies)分区：每区已有副本alpha ramp分母n，末副本再乘copies-i，整数边界连已存在副本也可能跳，不是只加一个新副本淡入。预算内预建实际副本并分区，copyorder/Painter重绑定与原逻辑相同；正底幂有interval界，负底非整数幂非法U，不用SignedPow造新语义。
- Polygon floor与Star分数pointCount按真实分支隔离区间；每区连续几何正确，改变topology时互斥矢量状态scope和真实event值，不用一串静态key图替代连续区间。glyph/path状态副本是**已准备的绘制叶子**，不重新给祖先Painter捕获，不双重上色。
- 结构gate、空间fit和事件都受T08证书/预算；不能证明的实际复杂场景按原分类拒绝，但全部承诺的稳定d、常规字体、Selector、有限copies/点数正例必须实现并通过。ImageRef换图先T01源端resolved payload/资源，有效有限字典G离散image，无资源/开放futureU；不启MB。

**测试：** A12—A20，多个source+Trim+Round+Merge/Repeater、动态Mask、fixed/changedtopology、连续copies整数两侧/分数alpha、pointCount各种区间、7shaping与13modifier/6selector、empty→nonempty、短anchors、font/GID/行/字符切换、沿线normal/force和ST21源Painter前后，文字/shape/stop/full alpha/region联合。

### 6.7 T11：正式能力台账、文件事务和SDK/CLI收口

**文件：** `SVGCoverageLedger.h/.cpp`、`SVGAnimationPlan.h/.cpp`、`SVGExporter.h/.cpp`、`CommandExport.cpp`、`CommandRender.cpp`、`docs/pagx-svg-animation-capabilities.md`及现有生成/测试工具。

1. 一套实际capability descriptor同时服务Plan与ledger/文档：含目标schema、staticconsumer前置、N/G/B/U、已实现算子/representation、reject条件；不按字段名等于Writer全覆盖。reflective175逐项有分类，表外point/image另外维护。
2. T01—T10所有ImplementedUnverified条目写完整有效正例、复杂组合限制与拒绝测试；统一T12通过才Verified。没有真实输出的Planned不提Supported；已Supported但回丢轨道/失败或误差未知的场景在实例级拒绝并修算法，不隐藏在note。
3. SDK/CLI选相同root/nested/状态、调用方fontconfig同源、static/animationpolicy完整正交；所有子Writer/Mask/tile/bake拒绝上抛Failed，diagnostics实例/node/时间/预算真实填，不只有“animation rejected”总句。
4. ToFile使用同目录独占、不可预测且本执行者持有的临时文件；checkedwrite+实际flush/close结果都检查，成功后atomicrename；失败/replace失败清理自己的temp，不覆盖/删除别人的`.tmp`。不承诺崩溃durability，跨filesystem/权限等外部失败确定保旧文件。
5. 禁止在完成函数里print逐步进度给用户；工具测试日志自然保留。所有本地commit按项目规则和 `/commit` 流程，只含会话改动，不为显示进度而每修一行提交并结束。`commit --only`按文件不按hunk隔离：若源文件本来就有用户未提交改动且本批要重构，不在混合文件上自动提交；T00选择隔离worktree保留已授权输入，或取得该hunk范围专项授权。不能仅凭文件名符合本轮就把用户修改夹入commit。

**测试：** Plan/ledger/生成文档双向完整、所有Writer有效输出或确定拒绝、enum/类型/unsupportedreason、全选项CLI冲突/Named未知、无sink结果一致、两同时ToFile不同temp归属、write/flush/close/replace各失败旧bytes不变；空输出/部分reject不落成功文件。

### 6.8 T12：统一构建、完整矩阵和新工作区复现

先检查本轮源码diff及注释/规范，运行项目code-style、修改语言对应comment guard和change-report。**格式化前Read codeformat.sh**：核实是否调用brew/pipx等安装、是否全仓改用户文件；需要系统安装走授权，不能预先无沙箱执行。有用户未提交/非本轮源码且脚本可能重写时，先按项目worktree规则隔离本轮版本与所需依赖/测试资源，再在隔离目录运行；不在格式化后恢复用户文件或将其hunk混入提交。普通本地工具已具备且工作区干净时按如下项目流程，格式化报错按规则忽略但不跳安装权限：

```bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest_OpenGL
./cmake-build-debug/PAGFullTest_OpenGL
cmake --build cmake-build-debug --target SVGBrowserTest
cmake --build cmake-build-debug --target SVGBrowserAnimTest
cmake --build cmake-build-debug --target HTMLTest
```

- tgfx源码链接另用独立build目录，加 `-DTGFX_DIR` 指向实际修订；不能把同名旧PAGFullTest二进制当本次构建。最后再从DEPS锁定的干净依赖做一次相同版本默认构建；不用stash/reset清脏vendor，建立独立验证目录按项目worktree规则或使用明确新构建/依赖位置。
- 浏览器从仓库lockfile精确版本安装/使用其匹配binary，记录实际Playwright版本、browser.version/revision/hash、libpag/tgfx SHA、GPU backend、font资源指纹、画布/background/scale/color和阈值。**不存在browser就安装匹配包或报真实环境阻塞，不回宿主Chrome。** 不猜训练知识里的浏览器版本号。
- Staticgate显式 `--svg-no-animation`，oracle使用TemplateStatic导出局部renderer，不传time初始化VM；Animatedgate选择相同Default/Named/All+nested，先pause，再seek，等待font/image decode与提交；不能仅rootdriver。
- 当前一个文字样本的0.05/10/0.4不作为全集门禁。每个case登记必要roi、预期非空/透明、身份/矩阵/轮廓/关键像素；人工构造向量font及简单几何关键点tol固定1e-4/alpha1/255。browser几何参考采用证书允许的0.25px边缘带，band之外须在1/255颜色误差内，band内检测轮廓位移/面积不超过证书而非全图均值。Native字体差异不得用低SSIM自动接受，可选择正确Fidelity表示；复杂static raster与renderer同分辨率/color，关键区颜色/覆盖逐项比较。
- 每种声明N/G有直接typed→effective apply→PreparedDraw→SVG重建正例，每个B/U/inactive/masked/no-op有有意义分类例；对每个真实动画正例验证t=0、keys左右、连续内部、周期接缝、第二周期、offset±/paused，C01—C24/A01—A22和TP01—TP16不只表格数完整。
- 默认新增目标/runner扩展现有入口，不只把12个fixture各测0.5s重复跑；生成一套有时刻/选项/字体/ROI元数据的case矩阵，范围在代码中能自动核对所有descriptor。属性证书与browser不同种类都直接检测，roundtrip只补充。
- 共享glyph/layout/resource/modifier/prepare改动同时跑tgfx相关测试、PAGX runtime、SVG/PPT/HTML；ci仅compile不能替代运行。PPT额外视觉目标环境存在时执行 `PPTTest`，缺LibreOffice等未授权系统工具时给具体环境Blocked，不静默跳过列为全通过。
- 正常测试非零定位修复后重跑受影响项和最终全量；相同失败命令不无修改重复。基准差异要查输出与源语义，不能改断言/版本/图片让门禁绿。接受截图只用户 `/accept-baseline`，HTML protected均值按既定独立授权流程；基准需用户动作时一次报告key/输出diff/原因，不写Verified。

### 6.9 覆盖关系与可复用的续作状态表

每项T状态在后续执行时直接更新下表，不再新增R阶段或把已完成日记继续长篇追加。状态词必须准确，有实际函数/测试/结果入口。

| 执行项 | 覆盖原方案 | 启动时状态 / 不可跳过的余项 |
|---|---|---|
| T00 | tgfx交付、ST44、M5.3 | ImplementedUnverified：`clearTextBlob` 与 `prepareDraws` 已在 tgfx 分支本地提交（`a9672fcd3`、`75efe5d75`），vendor 同步验证编译；DEPS 回填与正式发布仍 Pending |
| T01 | S0/S6、ST32/37/41/43/46、F13 | ImplementedUnverified：旧三字段与 optional 映射已删除，policy/textMode 默认值与预算/诊断结构已落地；typed schema、全会话计数与其余预算站点 Pending |
| T02 | S1/S4、ST1/5/8/12/16/21/23/24/33/34/35/45、F5/F9 | ImplementedUnverified：tgfx prepared 出口与静态构建会话缓存已完成；逐实例 actors、SVG 导出消费 prepared draws、背景消费 Pending |
| T03 | S3、ST9/10/18/20/22/28/29/30/31/36/44 | Pending：最终glyphscope/units/Selector/Native全出口，ST28已有正修复保留 |
| T04 | S4、ST11/12/16/23/24/27/42/45 | Pending：所有primitive/glyph描边、append/跨scope、精度及临时归属 |
| T05 | S1/S2、ST2/3/4/5/6/7/17/38/39/40 | Pending：完整Mask/effect/背景prefix/3D与采样；不以已有标签证明 |
| T06 | S5、ST13/14/15/19/25/26/36/37/42 | Pending：实资源/sampling/fit/空间色彩完整性 |
| T07 | M1/M2/M4.0、F1/2/3/4/13/14/16/17 | Pending：逐实例graph、oracle/clock/资源fanout/mix |
| T08 | M2/M3、F6/7/8/14/15/18 | Pending：精确操作链/事件、连续证书/global sample budget |
| T09 | M4/M5.4、F8/10/11/17 | Pending：所有consumer颜色/effect/full bounds及dynamic closure |
| T10 | M5.0—M5.4、F10/11/13/17/18 | Pending：全部最终d/shaping/Modifier/Selector/有限结构正例 |
| T11 | S6/M6、ST32/41/43/46、F12/17/18 | Pending：实现与descriptor/文档/真实输出和文件一致性 |
| T12 | 全部ST1—ST46、F1—F18、C/A/TP全集 | Pending：统一实际运行验证；没有新通过结果 |

每项Pending包含已有代码需保留的部分；不能机械删除重写，也不能把Pending理解为所有函数从零开始。启动时代码变化使状态表过时，核实对应diff后更新，但只在Verified证据足够时完成。

**原组合编号到续作批次的完整映射（每项都需T12统一实测）：**

| 原验收范围 | 实现责任 | 验收状态规则 |
|---|---|---|
| C01/C02/C03 | T01/T02/T05/T06 | 布局/组件/实例/资源，含ST35现有生成scroll状态；正确部分保留并补组合 |
| C04/C05/C06 | T02/T03/T04/T05 | Mask/捕获/生命周期，不能用静态布局图替代真正source动作 |
| C07/C08/C09/C10/C11 | T02/T05/T09 | 离屏/effect/bucket/背景，不切开未闭合合成栈 |
| C12/C13/C14/C15/C19 | T03/T06/T10 | 字形/容器/沿线/选择/资源，TP01—TP16全部由T03负责 |
| C16/C17/C18/C21 | T04/T06/T09 | 描边/颜色/图片/合法缓存重复导出，warm不可变资源与局部cache同源 |
| C20 | T01/T02/T07/T11 | TemplateStatic为本轮正例；默认host/已有快照/开放事件只验证主动排除或确定拒绝，不要求新增快照。保留已有runtime VM链/身份回归，不改变默认业务语义 |
| C22/C23/C24 | T01/T06/T08/T11 | policy/预算/源导入边界，MB禁用、未保存参数不逆推 |
| A01/A02/A03/A04/A05 | T01/T07/T08 | root/nested/typed/pause/offset/priority；含F3全部镜像正例，已修也补接缝 |
| A06/A07/A08/A09/A10/A11 | T02/T07/T08/T09 | fullturns/alpha/关键帧/误差/独立clock/endpoint，含F5/F9现有正确子集不回归 |
| A12/A13/A14/A15/A16/A17/A18/A19/A20 | T03/T04/T06/T09/T10 | source/stop/最终geometry/glyph/动态结构联合，不局部单轨替代 |
| A21/A22 | T01/T08/T11 | D8禁用/无界宿主、证明与资源预算失败确定拒绝，不执行MB |
| ST35/F3/F5/F9的已修子项 | T02/T07/T09 | 不是删掉重写；verify原正确行为、补方案残余及最终矩阵，没有“日记说已修所以不测试” |

### 6.10 唯一完成标准、不可声称完成的情况与最终交付

- [ ] T00—T12全部Verified，任务表、完整ST/F/C/A/TP及descriptor覆盖关系一致。存在任何外部Blocked时本轮只能交付准确阻塞报告，不能勾选此项或声称全部完成。
- [ ] 所有承诺N/G都有实际可运行正例与联合场景，没有新增无理由NotExpressible/B化；B/U和policy拒绝是原定有效限制，不是代替实现。
- [ ] 正规3静态policy、2文字模式、3选择集合、offset/paused、正式typed/resource/mix/priority及dependencyclosure全部对应同一真实sourceactor。
- [ ] Prepared出口与renderer使用同一最终shape/glyph/Painter准备，mutable source/临时Fill/即遇即画旧路径不残留；所有副本、alpha、矩阵、fit、coverage与order正确定义。
- [ ] TextPath TP、ST21 paint分支、ST28轴状态及TextHolder新glyph/空/失败均通过；没有sourceFont热改或未提交dependency掩盖。
- [ ] 连续G证书、边界/结构事件和序列化误差可核验，8段/32/512点限制不冒充证明；budget耗尽确定失败，所有角度/alpha/payload有限。
- [ ] static背景patch不双合成、不提前消费foreground，VectorOnly无新增effect bitmap，PreserveAppearance bake失败非成功矢量；Strict dynamic不被patch冻结。
- [ ] DEPS锁定的干净新依赖与源码链接同版本均构建/全量测试，所有backend/font/browser环境和新通过结果可复现；历史通过数不计本轮结果。
- [ ] SDK/CLI诊断与状态相同、所有实际file失败保旧内容/不侵犯别人temp、无sink不改success；所有会话源码改动经规范与comment检查。
- [ ] 构建、tgfx/runtime/SVG/PPT/HTML及browser完整矩阵全通过，没有未解释/自动接受/静默跳过的失败；需用户基准动作则仍Blocked。

最终只输出一次：实现范围与尚未支持的**原定**限制、完整改动清单、源码/tgfx SHA、实际测试命令/退出码/报告目录、基准/发布真实阻塞。不能只说“写完”“功能已有”“账本全分类”或把未验收项推下一轮。D8禁用下MB不用实现，但其拒绝测试要通过。

本轮交付是这份可继续实施的完整文档，不是T项源码实现。后续新终端按第0节启动，遵守唯一交付终点，避免逐批停下来汇报。
