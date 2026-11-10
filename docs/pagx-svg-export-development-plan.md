# PAGX → SVG 开发方案与代码评审

> - 初稿日期：2026-11-09；静态补充评审日期：2026-11-10。
> - 评审分支：`feature/lifengdy_svg_export`。
> - 本次源码基点：`ccae49ab6`；当前历史中的动画主体提交：`02a0a528d`。
> - 状态：静态与动画的完整方案均待评审，尚未开始源码实现。
> - 仓库根目录：`/Users/lifengdy/Desktop/project/libpag`。源码定位均相对于该目录；关键位置已按本次源码核对。
> - 第 3 节的运行结果为此前已有二进制的历史记录，不代表本次静态补充评审重新构建或重跑了测试。

## 1. 阅读结论

**现有静态 PAGX → SVG 也没有覆盖全部 PAGX 绘制语义，不能据此承诺“覆盖所有 SVG 情况”。保留已有导出能力与 Plan / Writer 分工，先修正静态和动画共用的实例、绘制、文字、资源与合成边界，再扩展动画。**

静态已有矩形、椭圆、路径、文字、图片、渐变、滤镜、遮罩和部分栅格化实现，但内外描边、跨父遮罩、TextBox / 字形、混合轴图案、滤镜顺序、背景依赖和资源身份仍有实际缺口。SMIL 同时存在顶层入口、实例时序和组合插值问题。两条链路共用的错误应从源头修复，不把静态视为无需检查的正确基线。

总体顺序为：

1. S0 / M0 共用验证基础：建立显式静态导出和动画求值的 XML、属性及浏览器对照；策略、诊断和预算契约前置。
2. S1 / S2 优先处理高风险的实例坐标、Mask、透明度、滤镜和背景合成；其中实例与绘制上下文和 M1 / M2 共用，不重复建设。
3. S3 / S4 / S5 补齐文字字形、描边 / 修饰器、渐变 / 图案与资源解析，使对应静态路径达到承诺的能力标准。
4. 在正确的共用基础上完成 M1—M3 的时钟、变换和组合轨道，再推进 M4 / M5 的动态颜色源与动态几何。
5. S6 / M6 收口接口、CLI、失败行为和全量回归。不是到最后才加入预算或错误处理，各阶段均须使用它们。

S0—S6 详见第 9.1 节，动画 M0—M6 详见第 9.2 节。浏览器验证从第一阶段开始；静态不以“有对应标签”，动画不以“出现 `<animate>`”作为完成标准。视觉保真、原生可编辑性和源 SVG 结构还原分别验收，不能相互替代。

本次保留目录中已有的 `docs/pagx-svg-export-plan.md` 原稿，不覆盖其内容。本文件是基于本次源码评审的独立方案；不沿用原稿中“动画基线已完成”或“先扩滤镜再处理实例”的判断。

## 2. 目标、交付边界与待确认决策

### 2.1 核心目标

- 对承诺支持的动画，SVG 与 PAGX 运行时在相同时刻具有一致的属性、绘制顺序和合成语义。
- 优先保留矢量、文字、资源引用和原生 SMIL，不默认用整段位图动画替代。
- 无法等价表达的组合属性可以进入受控采样，但必须满足明确的误差和资源预算。
- 所有无法支持、近似、冻结和资源失败都能定位到实例、节点和属性，不静默丢弃。
- 静态常规能力不因动画改造回归；已确认的静态错误按本方案修正，不为保持旧输出而保留错误语义。需要保留原结构与需要有意修正的输出分别验收。
- 不把修改模板文档后重新布局当作运行时动画求值，不破坏多场景共享模板的边界。

### 2.2 本次完整计划包含的能力

- 静态 PAGX 模型可表达内容的覆盖盘点与补齐：实例 ID、坐标空间、Mask / Contour、合成、字形、描边、修饰器、渐变、图案、资源和滤镜。
- 静态原生输出优先；无法等价原生表达的已建模效果，以完整上下文栅格化保留外观，或按显式策略拒绝 / 降级。
- 顶层 Animation 选择与导出。
- 已挂载 Composition 时间轴，包括多实例、不同 offset / playing 和已解析外部文档。
- 当前白名单通道的语义修正，以及 Layer x / y、共享颜色源、线性 / 径向渐变和色标动画。
- 动态修饰器和路径结果的来源追踪、能力判断与可表达路径的动画输出。
- 库接口、CLI 选项、错误行为和浏览器验证。

### 2.3 后续独立确认的能力

- StateMachine 输入、交互事件、DataBind / ViewModel 宿主行为的完整移植。
- 某个时刻的运行态矢量快照接口。
- Android / iOS / Web 等平台导出绑定。
- 动画位图序列、无法等价投影的 3D / 透视动画，以及随时间变化的背景模糊和玻璃效果；这些效果的静态保外观输出纳入 S2，不再笼统推迟。
- 需要动态重排版的文字内容、字体与复杂逐字形效果；已给定 GlyphRun 的静态字形和静态 TextModifier 纳入 S3。
- 任意源 SVG 的 DOM / CSS / 脚本、响应式行为、foreignObject 和未被 PAGX 模型保留的参数还原。若目标扩展到此范围，需先设计导入与模型保留，不仅修改 Exporter。

这些内容不是被当作已支持，而是有独立的输入和输出契约。当前模式遇到它们时仍必须明确诊断，不能默认为可保真导出。

### 2.4 编码前需要确认

| 编号 | 决策 | 推荐方案 | 为什么需要确认 |
|---|---|---|---|
| D1 | 多个顶层动画如何选择 | 默认按 `PAGScene::getDefaultTimeline()` 选择首个；另提供指定动画和显式全部并播 | 运行时默认选择首个，而当前 render CLI 会应用全部顶层 Animation。替代动画不应被无意叠加 |
| D2 | 是否接受受控采样 | 接受原生表达之外的有界近似；几何误差目标 0.25 文档像素，alpha / 颜色误差目标 1/255 | 若要求完全解析等价，则部分组合属性只能明确拒绝 |
| D3 | 动画不支持时默认行为 | 动画默认 Strict；显式 BestEffort 才允许告警后冻结或降级 | 成功写出文件不能等同于完整保留动画；动画与静态策略相互独立但共同约束最终输出 |
| D4 | 静态表示与保真策略 | 默认 PreserveAppearance：原生优先，必要时完整上下文栅格化，失败则拒绝；另提供 VectorOnly 与显式 BestEffort | VectorOnly 禁止新增效果 PNG 补丁但允许原始图片 / 位图字形；若要求文件完全不含位图，这是更严格的独立要求 |
| D5 | 静态文字优先级 | 默认 Fidelity：以已布局字形为准，可用 WOFF2 / 轮廓；Native 文字作为显式选择并诊断排版差异 | 视觉保真与可读原文、复制搜索不是同一能力；PUA 字符不能承诺原文搜索和复制 |
| D6 | 静态保真的目标环境 | 先固定画布、色彩、缩放及 Chromium 主门禁，再确认其他目标浏览器和导出缩放范围 | 不能承诺任意浏览器、字体环境和任意缩放都像素相等；PNG 补丁的保真与采样分辨率相关 |

上述推荐均未视为用户已经确认。D1 首项如果是 StateMachine，Strict 应报告不支持，不偷偷跳到下一条 Animation。顶层选择作用于根动画；Composition 是否激活仍由其挂载的 driver 决定。`exportAnimation=false` 则关闭全部动画导出，包括嵌套实例。

## 3. 已核实的代码与验证证据

### 3.1 此前评审已执行的验证

此前评审使用当时工作区已有 Debug 二进制，未重新编译。结果证明的是这些二进制在当时的测试状态，不是本次静态评审的新结果，也不是对全部源码路径的重新构建认证。

| 验证 | 结果 | 能说明什么 |
|---|---|---|
| `PAGFullTest --gtest_filter='PAGXSVGTest.*'` | 163 / 163 通过 | 现有 SVG 测试未失败，不代表组合动画已保真 |
| 当前动画结构测试与两项 Composition 运行时测试 | 19 / 19 通过，其中 SVG 17 项、运行时 2 项 | SVG 17 项包括 16 个 `SVGExport_Anim*` 和 1 个静态结构测试 |
| `spec/samples/animation.pagx` 实际导出并解析 SVG | 动画元素数为 0，stderr 为空 | 合法顶层 Animation 没进入当前导出计划，而且没有丢弃诊断 |
| 工作区状态检查 | 未改动受版本控制的源码 | 本轮评审没有执行修复、格式化或基准接受 |

此前完整 SVG 测试的 stderr 有三次 PNG 解码错误，测试仍通过；当时未定位原因，本次也未重跑或定位，不将“断言通过”扩写为“日志没有异常”。

未执行：重新构建、全仓测试、浏览器逐帧像素对比、HTML / PPT 全量回归、跨浏览器兼容验证。其余缺陷为源码路径与运行时规则对照确认，实施时必须先加入对应复现测试。

### 3.2 现有测试的主要盲区

本次静态补充评审仅核对源码，没有重新构建或重跑测试。当前 `PAGXSVGTest.cpp` 声明 166 个 SVG 用例，其中 16 个 `SVGExport_Anim*`；源码没有显式 `exportAnimation=false` 的测试设置。此数字是用例声明数，不能写成“166 项通过”。第 3.1 节的 163 项通过属于此前二进制结果。

静态测试也存在盲区：`test/src/PAGXSVGTest.cpp:4433` 的跨父 Mask 用例只验证告警和 defs，父变换仍为恒等；`test/src/PAGXSVGTest.cpp:3183` 和 `test/src/PAGXSVGTest.cpp:3211` 用伪 JPEG / WebP 头只验证 MIME；`test/src/PAGXSVGTest.cpp:3506` 的位图字体 fallback 没有真实位图数据。这些不能证明坐标、图案适配和字形 alpha 正确。

`test/src/PAGXSVGTest.cpp:4556` 起的动画相关测试主要检查字符串，不检测区间中间值、初始姿态、实例独立性和合成结果。

- `SVGExport_NoAnimationOutputUnchanged` 没有真正比较开关前后的 SVG 字节。
- 大部分动画样例把 timeline 挂在普通 Layer 上，不能作为 Composition 驱动作用域正确性的证据。
- 已有 PingPong 测试覆盖 Layer alpha，但没有覆盖 transform、color、visibility 的所有输出路径。
- 暂停和 offset 测试仅检查 `begin` 字符串，没有检查对应显示状态。

## 4. 职责边界与数据流

### 4.1 模板、运行时、导出三层

| 层 | 责任 | 不应承担的责任 | 主要定位 |
|---|---|---|---|
| PAGX 模板与布局 | 保存节点、资源、动画定义，解析静态布局 | 保存某个播放实例的实时状态 | `include/pagx/PAGXDocument.h:69` |
| PAGScene / PAGAnimation | 每实例绑定、时间推进、通道求值、写入运行对象 | 通过修改共享模板实现播放 | `src/pagx/PAGAnimation.cpp:63`、`:202` |
| SVG 导出 | 把选定场景行为编译为 SVG 结构与轨道 | 另造一套与运行时不同的插值和透明度规则 | `src/pagx/svg/SVGExporter.cpp:3673` |

当前静态流向：

```text
PAGXDocument
  → applyLayout
  → ModifierResolver / geometry and painter traversal
  → SVG elements / defs
  → optional static raster patches
```

目标动画流向：

```text
PAGXDocument + confirmed export options
  → reference validation and instance enumeration
  → static layout and resource resolution
  → SVGAnimationPlan: selected drivers and instance-scoped bindings
  → draw records: geometry / painter / reference dependencies
  → attribute track compilation: exact native or controlled sampling
  → SVGAnimationWriter: XML serialization
  → diagnostics and export result
```

引用安全检查必须在可能递归的布局和导出遍历之前生效。只给动画收集增加循环检测，而其他入口仍先递归，不构成完整修复。

### 4.2 继续保留的设计

- 保留 `SVGExporter::ToSVG / ToFile` 和现有静态导出选项。
- 保留 `SVGAnimationPlan` 与 `SVGAnimationWriter`，不重写成熟的静态主体。
- 复用 `Channel::evaluateAt(microseconds, frameRate)`，不复制 float / color / Matrix 插值。
- 复用矩阵分解、颜色空间转换、描边几何和修饰器解析能力。
- 继续遵守 PAGX 几何累积与 Painter 作用域，不把所有 Group 一一映射为全局 `<g>`。

## 5. 必须修复的问题清单

编号用于后续任务和测试对应。除 F1 官方样例外，本轮未通过新增用例或浏览器实测演示缺陷，不能把下表描述当作已完成修复。

| 编号 | 根因与源码定位 | 复现场景及预期 | 计划归属 |
|---|---|---|---|
| F1 | `src/pagx/svg/SVGAnimationPlan.cpp:124` 提前返回且只扫描 Layer timelines | 顶层 Animation 无 Layer driver 仍应按选择策略导出；仅 Composition 定义动画、slot 正常挂载也不能被根列表为空拦截 | M1 |
| F2 | `src/pagx/svg/SVGAnimationPlan.cpp:131` 按 Animation 指针去重；`src/pagx/svg/SVGAnimationPlan.cpp:153` 在根文档解析目标，绑定按 Node 指针存储 | 同模板两个 slot、不同 offset / playing 应独立；外部文档中的同名 ID 不得误绑定根文档 | M1 |
| F3 | `src/pagx/svg/SVGAnimationWriter.cpp:599` 多条 transform 共用会被重复翻倍的 duration；`src/pagx/svg/SVGAnimationWriter.cpp:1283` color 路径同理；`src/pagx/svg/SVGAnimationWriter.cpp:1212` visibility 未镜像 | D 时长的 PingPong 统一为 2D；transform 当前生成 16D、color 生成 4D，visibility 必须往返 | M1 |
| F4 | `src/pagx/svg/SVGAnimationWriter.cpp:253` 只把 offset / paused 映射为 begin | 延迟前和暂停状态应显示初始求值结果；Loop 正 offset 在原点按运行时模回绕，而不是等待首次开始 | M1 |
| F5 | `src/pagx/utils/ExporterUtils.cpp:160` 的恒等判断未考虑 position override | 默认 Group 单独 `position.x: 0→100` 时中点必须平移 50，不能提前返回恒等矩阵 | M2 |
| F6 | `src/pagx/svg/SVGAnimationWriter.cpp:537` 对合成矩阵重新分解，没有保留标量圈数和展开最短弧 | 标量 `0→360°` 保留整圈；Matrix `170→190°` 中点应为 180°而非 0° | M2 |
| F7 | `src/pagx/svg/SVGAnimationWriter.cpp:432` 不接受 Hold；`src/pagx/svg/SVGAnimationWriter.cpp:940` 的派生属性轨道仍连续输出且端点不统一 | 宽度 100 Hold→200 的中点保持 100；15/45 帧关键点、60 帧时长需补齐首尾；多个 easing 不能靠关键点求值保证中间状态 | M2 / M3 |
| F8 | `src/pagx/svg/SVGAnimationWriter.cpp:772` 与 `src/pagx/svg/SVGAnimationWriter.cpp:816` 零 alpha 直接退出，`src/pagx/svg/SVGAnimationWriter.cpp:834` 使用静态比率；`src/pagx/svg/SVGExporter.cpp:2060` 未传 Painter / color 因子 | 零起点淡入正确；Group=0.5、Fill=0.5、colorAlpha=0.5 时结果为 0.125；各因子同时动画应组合而非覆盖 | M2 / M3 |
| F9 | `src/pagx/svg/SVGExporter.cpp:3432` 因任意 Layer 动画改变 perChildAlpha；`src/pagx/svg/SVGExporter.cpp:3584` 的组透明度承载不等价 | `groupOpacity=false`、静态 alpha=0.5、仅 matrix 动画仍保留逐项透明度；重叠内容不得改成整体合成 | M2 |
| F10 | `src/pagx/svg/SVGExporter.cpp:1794` mask nestedWriter 未传 Plan；`src/pagx/svg/SVGExporter.cpp:3334` native TextBox 未传完整路径 | mask 内形状动画、TextBox 自身和相关祖先动画完整到达实际输出节点 | M2 |
| F11 | `src/pagx/svg/SVGExporter.cpp:3406` 只保护 Layer 自身绑定；ModifierResolver 生成节点后缺少动画来源 | Layer 内 Group / Rectangle / Fill 动画不得被一张 PNG 冻结；被 modifier 替换的几何仍保留动态依赖 | M2 / M5 |
| F12 | `src/cli/CommandExport.cpp:184` 不收集 SVG warnings | 不支持或丢弃的动画在 stderr 可见；Strict 失败返回非零，不能仍打印正常成功结果 | M6 |

补充依据：

- 运行时目标范围检查：`src/pagx/PAGAnimation.cpp:76`。
- 每 Composition 独立 binding 与驱动：`src/pagx/runtime/PAGComposition.cpp:105`、`:130`。
- offset 求值规则：`src/pagx/PAGAnimation.cpp:221`。
- Matrix 最短弧：`src/pagx/runtime/MatrixDecompose.h:98`，测试 `test/src/PAGXTest.cpp:11107`。
- 顶层默认选择：`src/pagx/PAGScene.cpp:444`；CLI 全部顶层求值：`src/cli/CommandRender.cpp:284`。

## 6. 关键内部设计

### 6.1 实例键与驱动键

以下是设计模型，不是已经新增的接口：

```text
InstanceKey = (ownerDocument, stableInstancePath, sourceNode)
DriverKey   = (instancePath, timelineOrdinal)
Binding     = (targetInstance, channel, animation, driver, applicationOrder)
```

- sourceNode 表示模板身份，instancePath 表示本次引用实例身份，两者不可互相替代。
- 同一个 Animation 在不同实例中可以有不同驱动，不能全局去重。
- 进入外部 Composition 时使用该实例的 externalDoc 做查找。
- 文档内 findNode 之后，再校验目标是否属于当前实例作用域，禁止跨 Composition 边界写入。
- 同属性多个已选择驱动按确认后的运行时顺序应用，不把“保留最后一个模板通道”当作任意混合语义的替代品。
- 未选择的顶层动画和未挂载的定义不自动激活；嵌套激活规则以实际运行时和最终 D1 契约为准。
- SVG ID 按稳定遍历与实例路径生成；定义缓存同时包含实例和影响其结果的依赖上下文。
- 循环保护使用当前祖先路径集合，允许合法兄弟复用。布局、mask、composition 和资源遍历保持一致的保护边界。

### 6.2 绘制记录：变换链与透明度链分离

扩展现有 `AccumulatedGeometry`，只增加导出必需的数据，不引入独立通用场景框架。

```text
DrawRecord:
  instance
  sourceGeometry
  geometryTransformChain
  painterScope
  painterOpacityChain
  painter
  modifierProvenance
  referencedResources
```

原因：外部 Painter 可以绘制 Group 内累积几何，但该绘制不一定继承几何来源 Group 的 alpha。因此不能从 geometryTransformChain 推导 painterOpacityChain。

每个 SVG 属性由完整依赖表达式产生：

```text
paintAlpha(t) = painterScopeAlpha(t) * painterAlpha(t) * colorAlpha(t)
```

Layer `groupOpacity=true` 时，透明度作用在合成后的组；false 时保持逐项作用。Mask / filter / transform 的包装顺序继续保留现有坐标空间边界，不为动画把它们随意合并。

多个透明度因子落到同一属性时先统一求值，再编译一条正确轨道。不得跳过 Group alpha、忽略 color alpha，或者通过除以静态 alpha 恢复动态值。

### 6.3 统一时钟与初始求值

将运行时的纯时间映射提取为内部共享能力，保持现有行为与整数溢出保护；SVG 不复制一份略有区别的规则。

```text
Clock:
  durationUs
  frameRate
  loopMode
  evaluationOffsetUs
  playing

EvaluateBinding(binding, exportTimeUs):
  elapsed = binding.playing ? exportTimeUs : 0
  contentTime = MapEvaluationTime(elapsed, binding.clock)
  return binding.channel.evaluateAt(contentTime, binding.frameRate)
```

实际实现需对齐运行时 Once / Loop / PingPong 的完整规则，不用上面的概念伪代码代替数值安全处理。

- 帧到微秒使用项目现有转换约定，支持不同 frameRate。
- 初始 SVG 属性由同一求值入口生成，包括延迟前、负 offset、暂停和单关键帧。
- PingPong 先构建唯一的完整周期，再给属性序列共享；镜像序列不能修改共享周期。
- 一次播放完成后保持终态，循环边界按运行时求值约定处理。
- paused 不需要依靠一条从未开始的动画表达固定状态。

### 6.4 轨道编译，不在 XML 写出时补语义

建议在现有模块内部建立统一的 compiled-track 数据结构。仅在编译职责继续挤占 Writer 时才拆独立编译文件，不先创建空抽象。

```text
CompiledTrack:
  attribute / transformOperation
  initialValue
  sharedClock
  segments
  interpolationMode
  approximationStatus
```

Plan 负责绑定、来源和能力诊断；轨道编译负责求值、插值映射和误差检查；Writer 只写出经过验证的轨道和宿主结构。

#### 原生等价路径

在属性映射与插值等价时使用原生 SMIL：

- float 直接属性、离散 visibility、可表达的颜色和几何参数。
- Group 标量变换尽量保留操作链、锚点、skewAxis 和原始 rotation，不先合成矩阵丢掉圈数。
- 可在每次 Painter 发射的局部包装结构中保留变换链；不把源 Group 错误提升为包含所有绘制的全局 `<g>`。
- Matrix 通道使用运行时的分解与最短弧约定，逐段处理角度连续性。
- 单关键帧可以产生固定初态，不要求伪造两个不同值。
- 首尾补齐、Hold / None、混合离散和连续段都走同一编译入口。

#### 受控采样路径

多通道 easing、透明度乘积、复杂派生几何无法直接映射时，采样实际表达式，而不是只把关键帧结果再线性连接。

采样种子包含首尾、所有依赖关键点、跳变两侧、时钟相位边界；在平滑区间继续细分。每次细分比较的是运行时求值与最终序列化轨道的重建结果，包含数值格式化带来的误差。

- 默认建议：1×文档坐标几何误差目标 0.25px；alpha / 颜色分量误差目标 1/255，均待 D2 确认。
- 仅测中点或有限检查点不能证明整个区间达标。能给出区间或导数误差界的表达式使用该界；不能证明时必须如实标记，并按 D2 / Strict 契约处理。
- Hold 是跳变，不使用短渐变来伪造保持段；连续段和离散段明确分开。
- 限制每轨道采样点、递归深度和整体输出预算。达到预算仍不满足要求时返回明确诊断，不无限细分。
- 不同周期的独立属性保持独立时钟。表达式跨多个时钟时，优先用等价结构分解；确需共同周期时进行有界计算，不能无界求最小公倍数并展开。
- 负缩放、奇异矩阵、零缩放恢复不依赖不可逆的静态矩阵求逆；不能等价表达时进入受控采样或明确失败。

### 6.5 动态依赖与修饰器来源

将当前 Layer 自身动画判断扩展为绘制单元的依赖闭包：几何、Group、Painter、颜色源、修饰器、Mask、Composition 和引用资源。

- 没有动态依赖的复杂效果，继续允许现有静态栅格化。
- 有动态依赖时禁止默认用单张 PNG 把动画冻结。
- BestEffort 选择冻结必须明确指出范围、时刻与丢失的动画，不能仅写“某效果不支持”。
- ModifierResolver 输出附带源几何、modifier 顺序、Repeater 副本序号和依赖来源，不只返回失去身份的 Path。
- 动态几何经过静态 TrimPath / RoundCorner 后，结果仍然动态。
- 按时刻求值使用导出局部状态与临时产物，不持续把每次采样结果追加到共享 document.nodes。
- 路径命令类型和数量稳定是 `d` 插值的必要条件，不是充分条件；还需验证区间形状和误差。
- 固定数量 Repeater 可按副本编译；副本数量变化、路径拓扑变化需单独做能力判断，不能统一声明“SVG 绝对不能表达”。
- 不依赖其他分支存在的 helper 或导入能力；如需引入，先核实当前接口和全局影响，再确认范围。

## 7. API、诊断与 CLI 草案

### 7.1 保留入口，补充明确策略

保留当前 `SVGExportOptions::exportAnimation` 与 `SVGExporter::ToSVG / ToFile` 入口。静态表示与文字选项按第 8.6 节收口，现有静态能力不删减；下面动画字段是待确认的拟议接口，不代表已经存在：

```cpp
enum class SVGAnimationSelection {
  Default,
  All,
  Named,
};

enum class SVGAnimationPolicy {
  Strict,
  BestEffort,
};

struct SVGAnimationOptions {
  SVGAnimationSelection selection = SVGAnimationSelection::Default;
  std::string animationId = {};
  SVGAnimationPolicy policy = SVGAnimationPolicy::Strict;
  float maxPositionError = 0.25f;
  float maxAlphaError = 1.0f / 255.0f;
  size_t maxSamplesPerTrack = 4096;
};
```

建议作为现有 options 的动画子选项。颜色误差、总输出上限和递归限制先由内部编译预算统一管理，不为尚未确认的需求扩张公开接口；确有调用方需要时再暴露。

入口行为：

- `exportAnimation=false`：模板静态导出，不等于 t=0 的动画初态，更不等于任意时刻快照。
- Default / All / Named 的解释完全对齐 D1；Named ID 不存在时失败，不回退为默认。
- Strict：选定动画存在不可表达的有效依赖时失败。`ToSVG` 返回空结果，`ToFile` 返回 false，生成成功之前不打开目标文件。
- BestEffort：允许约定的初始态冻结或视觉降级，但输出诊断。冻结值来自该驱动在导出原点的求值，不任意取模板值或最后关键帧。
- 未传 warnings 只影响诊断收集，不影响 Strict 成败。
- 保留当前 warnings 字符串入口，内部使用含原因码的诊断记录，不为文档方案直接改变现有公开返回类型。
- 类型、数值有限性和预算在公开输入边界校验；合法内部运行对象继续复用项目保证。

### 7.2 诊断内容

每项至少包含：

```text
reasonCode
instancePath
nodeId
channel / affectedOutput
reason
action: exact / approximated / frozen / dropped / rejected
```

要区分：未实现映射、输入无效、目标越界、资源失败、预算耗尽、近似误差不达标、输出能力不足。不要把“实现尚未接线”写成“SVG 无法表达”。

未被选择的定义不自动作为导出失败；实际选定的 StateMachine / DataBind 行为则必须显式诊断。

### 7.3 CLI 拟议选项

```text
--svg-no-animation
--svg-animation <id>
--svg-all-animations
--svg-animation-policy strict|best-effort
```

- 指定 ID 与 all 互斥；关闭动画与选择动画互斥，冲突输入报错。
- 所有导出 warnings 输出到 stderr；成功信息不掩盖降级信息。
- Strict 失败返回非零，不创建、截断或覆盖输出文件。
- 静态策略与文字模式按第 8.6 节补齐；不在这一轮顺带增加快照、平台接口或其他无关 CLI 配置。

## 8. 静态与动画能力矩阵及补齐设计

### 8.1 静态范围：不承诺任意源 SVG 可逆还原

这里的“静态导出”指无动画文档，或显式 `exportAnimation=false`，导出 PAGX 模板布局后的绘制结果；不采样正在播放的某个时刻。

必须区分三种要求：

1. **合法 SVG 文件：** XML、ID、引用和资源有效，目标浏览器能够打开。
2. **PAGX 静态语义保真：** 在确认的画布、色彩、缩放和环境下，外观、裁剪、绘制顺序、合成与 PAGX renderer 一致。这是本方案的完整性目标。
3. **还原所有源 SVG 结构和功能：** 保留原来的标签、CSS、DOM、响应式 viewBox、脚本、foreignObject、链接及所有 SVG 参数。这不是现有 PAGX 模型和 Exporter 的能力，不能由“补齐静态标签”完成。

例如 line / polyline / polygon 可以输出为等价 Path；use / symbol 与 marker 在导入时可能被展开为 Layer，导出不必重建原标签才能保留外观。已有导入展开依据为 `src/pagx/svg/SVGImporter.cpp:446` 与 `src/pagx/svg/SVGImporter.cpp:490`。反之，模型没保存的独立径向焦点和 spreadMethod 无法从 center / radius / colorStops 凭空恢复，见 `include/pagx/nodes/RadialGradient.h:31` 与 `include/pagx/nodes/Gradient.h:33`。

因此目标不是“发射 SVG 全部元素”，而是“覆盖 PAGX 已建模的全部有效绘制路径，每处无法原生表达都有保外观或拒绝策略”。如果以后要求任意 SVG 往返，先增加必要的导入、模型与资源语义，再设计导出，另行确认范围。

### 8.2 当前静态能力矩阵

表中“已有”表示存在实现，不表示所有边界与组合已验证；“缺口”包含可修复错误和明确的近似，不能统称 SVG 不支持。

| 范畴 | 当前输出与实际边界 | 补齐目标 / 阶段 |
|---|---|---|
| 根画布与布局 | 输出 width / height / `viewBox=0 0 W H`；已有 applyLayout、约束、Path renderScale；不保留原 SVG 响应式结构 | 验证布局与绘制空间，不误承诺还原源结构；S0 / S1 |
| Layer / Group / Composition | Layer 包装、Group 几何累积与 Painter 分次输出、Composition 内联已有；实例 ID 和父空间不能只按模板指针 | 实例身份、唯一 ID、作用域和稳定引用；S1 |
| Rectangle / Ellipse / Path | 常规几何已有；反向轮廓、起点、退化轴、解析后的 conic 近似仍有缺口 | 保留原生轮廓语义和受控几何转换；S4 |
| Fill / Stroke | 多 Painter、fill-rule、普通 cap / join / width 已有 | 每次绘制的坐标、透明度和先后顺序正确；S1 / S4 |
| 内外描边 | Rectangle / Ellipse 用 inset 近似；Path / Text 未统一处理；非圆椭圆与虚线会偏离 | 原轮廓上倍宽描边加裁剪或等价展开，保持原始 paint bounds；S4 |
| 虚线 / reversed | 写 dasharray / dashoffset，但不消费完整 adaptive、轮廓方向和原生起点规则 | 原生 dash 简化与自适应；必要时输出解析后的虚线几何；S4 |
| Polystar / TrimPath / RoundCorner / MergePath / Repeater | 常规静态解析已有；外层 modifier 作用于 Group / Repeater、Append、幂和共享 Fill 状态不完整 | 共用解析语义，不把 PPT 特定补偿施加给 SVG；S4 |
| 线性 / 径向渐变 | 普通 stops、matrix、fit 模式已有；空 stops、退化 bounds、外部 Painter 空间不一致 | 复用原生 fit 与默认 stop；区分几何空间和 Painter 空间；S5 |
| 色彩 / alpha | sRGB、P3 CSS 和 stop alpha 有输出；色域、插值空间、量化及浏览器差异没有完整门禁 | 同一色彩约定下验证；非等价时保外观或明确诊断；S5 / S6 |
| Image / ImagePattern | 内嵌图片、双轴 Repeat 和部分 Decal fit 已有 | 所有实际供应的图片、编码尺寸、独立 X/Y tile、采样与 scaleMode 一致；S5 |
| Mirror / Clamp 图案 | 已尝试局部 PNG 烘焙；fit、资源顺序和失败 fallback 不等价；并非完全没实现 | 使用完整 shader / fit / tile 状态渲染；禁止静默改变轴模式；S5 |
| Text / TextBox | 原生 text、行分割、竖排、富文本有路径；默认 TextBox 可绕过 GlyphRun 和内层 Painter 状态 | 布局与实际字形统一；非文字内容和前景顺序不丢；S3 |
| GlyphRun / WOFF2 / 轮廓 | 已有 WOFF2 与逐字形路径；同 Text 多 Font、缺省 advance、anchor、offset、变换和 faux 样式存在差异 | 复用 renderer 字形放置与合成样式，按 Font 分段，正确 fallback；S3 |
| fauxBold / fauxItalic | Native 文字已有 CSS 属性；WOFF2 / 轮廓路径未完整应用 renderer 的合成加粗 / 倾斜 | 各字形模式与 bounds 一致，不以 CSS 声明替代保真证明；S3 |
| 位图字形 | 已有逐字形 image 输出；字形 alpha、资源失败与数据来源未完整接通 | 真实位图、scope / painter alpha、编码与位移验证；S3 / S5 |
| TextPath / TextModifier / RangeSelector | 正常可见 Layer 默认触发整层烘焙；关掉或失败时丢效果；Mask 旁路 probe | 静态结果可优先展开为最终字形，无法表达则完整栅格化；S3 |
| Alpha / Luminance Mask | 简单同父路径有输出；跨父仅告警，自身 alpha / effects / composition / scroll 不完整 | 实例相对矩阵、完整像素合成与绘制顺序；S1 |
| Contour clipPath | 已有基本形状输出；clip-rule、描边轮廓、隐藏后代和有效覆盖判定不完整 | 按 renderer 提取裁剪覆盖，描边先展开，正确排除不可见内容；S1 / S4 |
| scrollRect / clipToBounds | 常规 clipPath 与滚动位移已有；与跨父 Mask、effects、3D 组合仍需验证 | 区分视口空间与内容空间，一次裁剪和位移；S1 / S2 |
| groupOpacity / blend / passThroughBackground | alpha 与 mix-blend-mode 有输出；对子树的 alpha 包装不等于原生离屏规则；背景隔离未完整映射 | 根据真实离屏条件决定整体 / 逐项合成，不能仅套 `<g>`；S1 |
| Blur / Shadow / ColorMatrix / Blend / Noise | 许多 filter primitive 已有，但链路顺序、区域单位、spread、tileMode、noise 算法有差异 | 原生等价链路与 user-space 区域，非等价效果显式栅格化；S2 |
| BackgroundBlur / Glass | 当前即使 bake 开启也跳过背景效果；普通目标 Layer PNG 不包含所需背景 | 完整背景上下文烘焙与绘制替换，或明确拒绝；S2 |
| 静态 2D / 3D 变换 | 当前已使用 LayerTransformUtils，含 anchor、组件变换和常量 w 归一化；不能沿用“Layer 无这些字段”旧结论 | 仿射保持矢量；一般透视与有效 preserve3D 已有 bake，补旁路和组合；S1 / S2 |
| antiAlias 与栅格采样 | renderer 消费 Layer.antiAlias；原生 SVG 未对齐逐层边缘抗锯齿状态，不能默认认为浏览器一致 | 在目标缩放下验证，原生提示不能等价时按策略 bake / 拒绝；S1 / S2 |
| 资源与失败诊断 | 部分警告已有；空 bitmap、字体 fallback、probe 旁路和 CLI 不收集导致损失不完整可见 | 所有有效依赖有结果或诊断；fail 不截断文件；S6 |

### 8.3 静态缺口、证据与复现场景

下表为本次源码核实的开发输入，不是已完成修复，也不是本轮浏览器实测结果。前次“SVG 测试全通过”不消除这些未覆盖的路径。

| 编号 | 源码依据与根因 | 必须加入的复现场景 | 阶段 |
|---|---|---|---|
| ST1 | `src/pagx/svg/SVGExporter.cpp:319` 生成 ID 不检查作者 ID；`src/pagx/svg/SVGExporter.cpp:3537` 重复内联；Mask cache 按模板节点和类型 | Layer 叫 grad0 再生成渐变；两实例包含相同内部 Layer ID；Mask 后缀撞作者 ID；外部同名资源 | S1 |
| ST2 | `src/pagx/svg/SVGExporter.cpp:1858` 跨父 Mask 只有告警，`src/pagx/svg/SVGExporter.cpp:1814` 只用 Mask 自身矩阵 | Mask 与 owner 不同父级，祖先平移 / 旋转 / scroll；owner 子 Mask；同模板多个实例 | S1 |
| ST3 | `src/pagx/svg/SVGExporter.cpp:1828` Mask 固定 alpha=1，直接 contents / children，绕过自身 effects / composition / scroll 和完整 probe | 根 Mask alpha=0 或 0.5、仅 Composition 内容、带 filters / scrollRect / TextPath；Luminance 的前景白和背景黑跨 children 合成 | S1 / S2 |
| ST4 | `src/pagx/svg/SVGExporter.cpp:1842` Contour 使用普通 paint 输出，未按可见性与描边覆盖提取；`src/pagx/svg/SVGExporter.cpp:3557` 只跳 owner 的直接 Mask 子项 | 只有 Stroke 的环形裁剪、evenodd 洞、隐藏子层；放在其他父级的舞台可见 Mask 不应额外上屏 | S1 / S4 |
| ST5 | `src/pagx/svg/SVGExporter.cpp:3432` 与 `src/pagx/svg/SVGExporter.cpp:3539` 用单层 alpha 或子树 `<g opacity>` 代替离屏条件 | 嵌套 groupOpacity=false 的半透明重叠，叠加 filter / blend / Alpha Mask，passThroughBackground=false | S1 |
| ST6 | `src/pagx/svg/SVGExporter.cpp:1711` 把像素 margin 直接写百分比；`src/pagx/svg/SVGExporter.cpp:1453` shadow 不推进 currentSource；`src/pagx/svg/SVGExporter.cpp:1729` 先 filter 后 style | 很小 bbox 加大 blur / offset；连续 Blur / Shadow / ColorMatrix；style 后 filter；Mask 加缩放后的滤镜尺度 | S2 |
| ST7 | `src/pagx/svg/SVGExporter.cpp:1521` spread 只告警忽略；`src/pagx/svg/SVGExporter.cpp:1576` 和 `src/pagx/svg/SVGExporter.cpp:1652` 跳 Glass / backdrop；普通 Raster 只画目标 | 正负 spread、不同 blur tile、noise 保真；背景渐变上的半透明玻璃、混合与祖先前景，不能把孤立 Layer bake 当兜底 | S2 |
| ST8 | `src/pagx/svg/SVGExporter.cpp:3334` 默认 TextBox 另走富文本收集；`src/pagx/utils/TextUtils.h:74` 只收 text 非空且不保存内层 Group transform / alpha | glyph-only TextBox、TextBox 中 Rectangle、多个 Painter、内层变换与 alpha、foreground 两次遍历 | S3 |
| ST9 | `src/pagx/svg/SVGExporter.cpp:2388` WOFF2 选择与位置不覆盖多 Font / 缺省 advance / glyph anchor；`src/pagx/utils/TextUtils.cpp:84` 矩阵不同；`src/pagx/utils/TextUtils.cpp:192` 轮廓漏 glyph.offset | 同 Text 两 Font、无 positions / xOffsets、非零位置和 anchor 加旋转 / 缩放 / skew、非零 vector offset | S3 |
| ST10 | `src/pagx/svg/SVGExporter.cpp:2293` 把透明度写 fill-opacity，`src/pagx/svg/SVGExporter.cpp:2334` 的 image 不应用对应因子 | 带真实 PNG / WebP 数据的位图字形，Group / Painter 半透明，资源不存在时必须诊断 | S3 / S5 |
| ST11 | `src/pagx/svg/SVGExporter.cpp:2118` 用 inset 改原轮廓；`src/pagx/svg/SVGExporter.cpp:1990` 直接输出 dash 属性；`src/pagx/svg/SVGExporter.cpp:2217` 未消费 Path reversed | 厚描边非圆椭圆、闭合 / 开放 Path 和 Text 内外描边、虚线 round cap、自适应与反向路径、渐变描边原始 bounds | S4 |
| ST12 | `src/pagx/utils/ModifierResolver.cpp:499` 为 PPT 把 Append 改 Union；`src/pagx/utils/ModifierResolver.cpp:596` Group 不进 shapeSlots；`src/pagx/utils/ModifierResolver.cpp:720` Repeater 后清槽 | Append 重叠轮廓的描边、外部 Trim / Merge 作用 Group 和 Repeater；`SignedPow(-1,0)` 的首副本应正确；不同源 fill-rule 不得互相修改 | S4 |
| ST13 | `src/pagx/svg/SVGExporter.cpp:665` 空渐变不写 stop；`src/pagx/svg/SVGExporter.cpp:639` 退化 fit 不同；`src/pagx/svg/SVGExporter.cpp:3293` 几何 transform 未携带 Painter 空间 | 空 stops 应按 renderer 默认黑白；零轴路径渐变；Group 平移 / 缩放几何由外部 user-space 渐变和 Stroke 绘制 | S5 |
| ST14 | `src/pagx/svg/SVGExporter.cpp:872` 只取 PNG 尺寸；`src/pagx/svg/SVGExporter.cpp:816` 任一 Repeat 进入双轴路径；`src/pagx/svg/SVGExporter.cpp:915` 旋转 fallback 丢 scaleMode | 真实 JPEG / WebP / GIF 适配、Repeat/Decal 混合轴、Mirror+LetterBox、旋转或负缩放图片+Stretch/Zoom | S5 |
| ST15 | `src/pagx/utils/ImageFormatUtils.cpp:270` 只 data / 文件；`src/pagx/utils/RasterUtils.cpp:393` 同样解析；renderer 在 `src/renderer/LayerBuilder.cpp:2295` 优先 runtimeImage | data URI 与宿主已供应 runtimeImage、字节和 runtimeImage 同时存在时一致；未供应资源严格失败而非泄漏本机路径 | S5 / S6 |
| ST16 | `src/pagx/utils/ModifierResolver.cpp:736` 修改共享 Fill；解析通过 document.makeNode 持有临时节点；公开数值与烘焙预算需统一检查 | 重复导出后模板不被改、nodes 不无界增长；SVG→PPT→SVG 不串状态；非有限数值、深引用、乘性复制和烘焙预算耗尽 | S4 / S6 |
| ST17 | `src/renderer/LayerBuilder.cpp:2756` 消费 Layer.antiAlias，原生 `src/pagx/svg/SVGExporter.cpp:3575` 等 Layer 发射没有相应状态 | 关闭边缘抗锯齿的斜边、旋转矩形与裁剪，比较固定缩放下覆盖；不能用 shape-rendering 提示未经验证地视为等价 | S1 / S2 |
| ST18 | `src/renderer/GlyphRunRenderer.cpp:200` 设置 fauxBold / fauxItalic；WOFF2 的 `src/pagx/svg/SVGExporter.cpp:2480` 与轮廓工具未消费完整合成样式 | 同 GlyphRun 使用真实 Bold / Italic 与额外 faux 组合，检查轮廓、advance、bounds、Mask 覆盖；Native、WOFF2、path 和位图模式分别验证 | S3 |

渲染依据与复用位置：

- 原生内外描边使用倍宽加 alignment clip：`third_party/tgfx/src/layers/contents/ShapeContent.cpp:129`；虚线简化和 adaptive：`third_party/tgfx/src/layers/DashEffect.cpp:24`。
- Contour 包含有效描边覆盖：`third_party/tgfx/src/layers/contents/DrawContent.cpp:70`。Mask 根舞台 `visible=false` 不等于关闭 Mask，其语义必须沿用 renderer，不能一律过滤所有隐藏根。
- 离屏条件包含 blend、背景隔离、groupOpacity、filter 和像素 Mask：`third_party/tgfx/src/layers/Layer.cpp:1185`；完整相对 Mask 矩阵：`third_party/tgfx/src/layers/Layer.cpp:1277`。
- 字形 offset、advance 与 anchor 变换：`src/renderer/GlyphRunRenderer.cpp:137`、`src/renderer/GlyphRunRenderer.cpp:218`。
- 空 stops 默认黑白：`src/renderer/LayerBuilder.cpp:2097`；退化渐变 fit：`third_party/tgfx/src/layers/vectors/Gradient.cpp:93`；图片适配矩阵：`third_party/tgfx/src/layers/vectors/ImagePattern.cpp:95`。
- 普通目标 bake：`src/pagx/utils/RasterUtils.cpp:130`；已有带背景合成能力：`src/pagx/utils/RasterUtils.cpp:351`。后者不是自动可替换的 SVG 补丁，仍需绘制消费范围契约。

### 8.4 静态正确性的共用模型

不另造一套静态场景树。扩展第 6 节的导出局部实例和 DrawRecord，让动画计划使用同一基础：

```text
DrawRecord:
  instancePath
  sourceGeometry
  geometryToPainter
  painterToLayer
  paintBounds
  alphaChain
  placement / drawOrder
  compositionBoundary
  dependencies / provenance
```

- **几何与画笔空间：** 先明确 geometryToPainter，再决定把几何烘焙到 Painter 空间还是保留局部包装；不能同时把外部 gradient / stroke 错误变成随几何 Group 变换。
- **Mask：** 按实例计算到 owner 输出空间的相对变换，包含共同祖先和滚动；Alpha / Luminance 使用完整层像素合成，Contour 使用有效几何提取。不同空间或实例不能误用缓存。
- **合成：** 以 renderer 离屏条件选择整体 alpha 或逐项 alpha；filter、clip、mask、blend 的包装顺序保证各自所在空间与效果尺度。
- **字形：** TextBox 不另走丢 Painter 状态的简化分支。已布局 GlyphRun 优先，统一字形 offset / advance / anchor / transform，WOFF2 按实际 Font 分段，位图字形显式承载 alpha。
- **修饰器：** 共用算法保留 Append 与布尔运算的真实差别，PPT 特定补偿放在 PPT 输出端。返回结果包含填充规则与来源，不能更改原 Fill。
- **资源：** 复用 renderer 的实际资源优先级和通用解码 / 尺寸获取；宿主供应状态是输入的一部分。不存在的网络资源不会被本方案擅自下载，必须有明确前置条件或失败诊断。
- **生命周期：** 静态解析和采样产物由导出局部上下文持有；不在共享模板节点池长期追加，不污染后续 renderer / PPT / HTML 导出。

### 8.5 完整上下文栅格化，不把截图当成万能兜底

已确认正常 Layer 的 probe 会把 TextPath / TextModifier、conic / diamond、一般透视和有效 preserve3D 路径送入 bake，见 `src/pagx/svg/SVGFeatureProbe.cpp:86`。这不是“完全未支持”，而是以编辑性换取特定分辨率下外观的路径；Mask 旁路、背景效果和失败 fallback 仍不完整。

方案：

1. 对每个有效输出单元先判断原生等价能力，再选最小的**完整合成边界**，不是简单取含有不支持节点的 Layer。
2. 复用 exporter-owned renderer 树及 RasterUtils；Alpha、Mask、scrollRect、3D 和 effect 状态在 bake 中只应用一次。
3. 背景效果使用实际在目标之前绘制的内容，采样需要的 halo 与输出覆盖区域分开；不能把透明背景上孤立绘制的目标图当成背景模糊结果。
4. 带背景 PNG 已包含“背景+目标”。必须替换对应绘制前缀或裁去原前缀在补丁区域的输出，不能直接叠在原背景上造成半透明二次合成。后续 foreground / sibling 继续按原次序输出。
5. 记录 bake 已消费的 DrawRecord 范围、坐标空间、有效覆盖与已应用状态。不能证明局部替换安全时扩大到完整可隔离合成边界，不静默丢掉背景或只保留目标。
6. 一般投影与深度交叉必须保留完整 3D 上下文；可精确降为 affine 或常量 w 的场景继续原生输出。效果 / Mask 造成的扁平化规则与 renderer 一致。
7. GPU、资源、像素预算或编码失败时，PreserveAppearance 失败；只有显式 BestEffort 才允许诊断后降级。原生路径存在可修复错误时，应修复，不永久用截图掩盖。
8. 视觉保真按 D6 的固定缩放和色彩容差验收，不声称 PNG 任意缩放仍矢量无损。带有效动态依赖的单元继续受动画策略约束，Strict 动画不允许单帧冻结。

### 8.6 静态策略、文字模式与选项收口

以下为拟议接口，需 D4—D6 确认后实施：

```cpp
enum class SVGStaticExportPolicy {
  VectorOnly,
  PreserveAppearance,
  BestEffort,
};

enum class SVGTextExportMode {
  Fidelity,
  Native,
};
```

| 模式 | 输出契约 | 失败行为 |
|---|---|---|
| VectorOnly | 仅原生 / 最终几何表达，不为效果新增 PNG；已有 Image、位图 glyph 可以作为原资源保留 | 不能等价表达就拒绝，不自动删效果 |
| PreserveAppearance（推荐默认） | 原生优先；不能等价表达的已建模内容以完整上下文 bake 保外观 | 无法满足确认的容差、资源或预算失败即拒绝，不退成“成功但不同” |
| BestEffort | 允许明确近似或丢弃，逐项报告原因、范围与动作 | 仍拒绝无效输入与无界资源消耗，不把执行错误当正常近似 |
| Fidelity 文字（推荐默认） | 使用实际已布局字形；可用 WOFF2、轮廓或原位图，不重新猜字体排版 | 字形数据或外观无法满足要求时按静态策略处理 |
| Native 文字 | 保留可读字符串，允许目标字体重新塑形 | 比较布局和形状差异；超过容差时不得绕过 PreserveAppearance 假装保真，可按策略回退或拒绝 |

收口原则：

- 用明确静态策略替代语义重叠的 bakeUnsupported，用文字模式替代 convertTextToPath 的简单布尔二分；同步所有公开调用方、CLI 和测试，不保留冲突的旧选项兼容层。
- embedFontsAsWoff2 作为字形编码偏好保留，不使默认 TextBox 绕过权威字形，也不承诺 PUA 对应原文可搜索 / 复制。
- 静态策略与第 7 节的动画 Strict / BestEffort 正交：静态允许 bake 不代表允许冻结动画，动画非严格也不能突破 VectorOnly 的表示约束。
- 导出成功只在所有有效依赖满足对应契约后成立，不依赖 warnings 指针是否为空。静态诊断复用第 7.2 节的原因码、实例路径和动作。
- CLI 在原计划基础上增加 `--svg-static-policy vector-only|preserve-appearance|best-effort` 和 `--svg-text-mode fidelity|native`；整理原 text-to-path 选项及其对其他格式的影响，不对所有格式生硬替换同一个含义。
- 任意文件写出前完成能力、资源与编译检查；只验证可写并不代表场景转换正确。

### 8.7 静态验收与新增测试矩阵

所有新增静态用例显式设置 `exportAnimation=false`，独立验证没有 SMIL。无动画文档开启 / 关闭动画的结果应相等，但不能以此证明静态视觉正确。

| 测试组 | 必须检测的行为 |
|---|---|
| 实例与文件结构 | 作者 ID 与 generated ID 碰撞、多实例 / externalDoc 同名、defs 单一有效引用、重复导出确定性 |
| Mask / clip | 跨父矩阵与滚动、根 alpha、Mask composition / effects、可见 Mask 不上屏、Luminance 前景顺序、Contour stroke / evenodd / 隐藏后代 |
| 合成与背景 | 两种 groupOpacity 的嵌套重叠，filter / blend 强制离屏，背景隔离；半透明背景补丁无二次绘制，祖先 foreground 不被提前烘焙 |
| 字形与布局 | glyph-only TextBox、多字体、多 Painter、缺省 advance、vector offset、anchor / rotate / skew / scale、fauxBold / fauxItalic、真实位图 alpha、RTL / 竖排 / rich text |
| 几何与描边 | 各形状内外 / 居中描边、厚描边、开放轮廓、adaptive dash、反向 / 起点、Append 重叠描边、外层 modifier、Repeater 负缩放首副本 |
| paint 与图片 | 空 stop 和退化 bbox、几何 / Painter 分离、真实 PNG / JPEG / WebP / GIF、data URI / runtimeImage、独立 X/Y tile 和全部 scaleMode |
| bake | conic / diamond、TextPath / 静态 modifier、一般透视 / preserve3D、antiAlias=false；成功外观、失败结果、覆盖边界、缩放 / 色彩与预算 |
| 模板与错误 | 原 Fill / glyph / nodes 不被污染；不同格式顺序导出一致；深引用、非有限数值、预算耗尽、资源未供应、Strict 不写文件 |

使用与动画共用的浏览器启动和图像评测设施，静态只等待资源完成后采一帧，不复用 WAAPI 时钟。比较 PAGX renderer 与 SVG 的同一画布、背景、缩放和颜色空间。关键区域误差、轮廓、文字位置与像素差直接检测，SSIM 作为补充；不能让大背景掩盖小主体错误。

源 SVG 往返样例另设两段判据：“源 SVG→PAGX”检查导入是否已损失，“PAGX→SVG”检查此次导出是否新增损失。只有后一段正确不能证明前一段覆盖所有 SVG。

### 8.8 与动画计划的关系

S0—S6 的实例、DrawRecord、Mask、字形、资源解析、probe、诊断与预算由动画复用；M1—M6 不再复制另一套。静态阶段先修属性和作用域，动态阶段再对这些表达式按时刻编译。

当前源码已经具备静态 Layer anchor / rotation / scale 和 LayerTransformUtils。动画白名单仍不等于所有这些字段已可动画。后续 M2 应对照当前字段和 runtime writer，不能按旧初稿“Layer 无组件字段”新增重复模型；第 5 节记录的矩阵 / Group 动画问题仍需处理。

### 8.9 动画能力矩阵

“现有接线”只表示有代码路径，不表示组合语义已经正确。

| 能力 | 当前情况 | 本方案目标 | 阶段 |
|---|---|---|---|
| 静态 SVG 几何 / 文字 / 图片 / 渐变 / Mask / filter | 已有实现但并不完备，详见第 8.2 / 8.3 节 | S0—S6 补齐并与动画共用，不承诺还原任意源 SVG | S0—S6 / 全阶段 |
| 顶层 Animation | 未正常进入 Plan | 按选择策略导出 | M1 |
| Composition 多实例 / 外部文档 | 未按实例和文档正确绑定 | 独立驱动和稳定引用 | M1 |
| Once / Loop / PingPong / offset / paused | 有实现，但多个路径不等价 | 共用运行时规则与初态 | M1 |
| Layer alpha / visible / matrix | 已接线，有透明度与矩阵问题 | 合成及区间插值正确 | M2 / M3 |
| Layer x / y | 未接线，运行时已有通道 | 按运行时位置叠加规则导出 | M2 |
| Group / TextBox 变换与 alpha | 已接线，有链路和作用域问题 | 所有实际发射分支正确 | M2 / M3 |
| Rectangle / Ellipse 几何 | 已接线，有 Hold 和派生属性问题 | 中心、尺寸、圆角、描边依赖正确；动态非正圆使用 ellipse | M2 / M3 |
| Fill / Stroke color、alpha、width、dashOffset、miterLimit | 已接线，有因子与时钟问题 | 组合轨道不丢因子 | M2 / M3 |
| SolidColor、渐变几何、ColorStop | 未接线 | 引用与实例语义正确，颜色和几何一起验证 | M4 |
| Path / Polystar / TrimPath / RoundCorner / Repeater | 动态输出未完整支持 | 保留来源；对可表达结果编译，否则明确失败或降级 | M5 |
| 可映射滤镜 / 样式参数 | 当前未支持动画 | M4 / M5 核心完成后独立扩展，复用统一轨道编译 | 后续扩展 |
| StateMachine、交互、复杂文本重排版、3D | 不具备本方案的完整契约 | 诊断明确，不假装支持 | 后续独立方案 |

滤镜扩展时，blur 输出区域必须覆盖动画全过程，非线性倒数参数需要编译表达式，不能只换算端点后线性插值。浏览器对 `d`、颜色、字体和滤镜动画的支持以固定版本实测为依据，不沿用未经核验的兼容性断言。

## 9. 分阶段实施任务

### 9.1 静态补齐阶段 S0—S6

静态阶段按影响与风险安排，不先扩充容易发射的标签而推迟合成、遮罩和字形错误。策略、诊断、预算从 S0 起确定并在各阶段生效；S6 是收口而非第一次实现错误处理。

#### S0：静态能力门禁与共用验证基础

**涉及文件：** `test/src/PAGXSVGTest.cpp`、`test/src/PAGXCliTest.cpp`、`tools/html-snapshot` 内的共用 SVG 评测入口、`CMakeLists.txt`、`include/pagx/SVGExporter.h` 的拟议契约。

**实施内容：**

1. 将 ST1—ST18 的关键场景构造为显式关闭动画的输入，先建立能击穿现有错误的结构和视觉断言。
2. XML 检查唯一 ID、引用、defs、有效尺寸和没有 SMIL；视觉入口使用固定资源、画布、缩放和色彩，静态和 M0 复用启动 / 比较设施而不是两套 runner。
3. 确认 D4—D6 的表示、文字和环境标准，以及失败、不写文件和资源预算契约。
4. 测试区分当前实现缺陷、既有有损策略和源 SVG 模型缺失，不把所有“不相等”都归给 Exporter。

**验收：** 静态失败场景可稳定检测；原生控制样例非空且正确；历史 163 passed 不作为新门禁结果；用例不依赖动态时钟和墙钟碰巧采对状态。

#### S1：实例身份、坐标、Mask 与透明度合成

**涉及文件：** `src/pagx/svg/SVGExporter.cpp`、`src/pagx/utils/ExporterUtils.h`、`src/pagx/utils/ExporterUtils.cpp`、`src/pagx/utils/LayerTransformUtils.h`、`src/pagx/utils/LayerTransformUtils.cpp`、`test/src/PAGXSVGTest.cpp`；与 M1 / M2 共用实例和绘制记录接口。

**实施内容：**

1. 在共享 Context 中建立全局 ID 分配与实例映射，作者 ID、generated ID、Mask 后缀和外部资源统一保留 / 重命名；所有引用和缓存使用同一映射。
2. 记录 geometryToPainter、painterToLayer、placement 和合成边界；保留现有 Layer / Group 布局与变换工具，不另造组件模型。
3. Mask 使用实例到 owner 输出空间的完整相对矩阵，包含祖先和 scrollRect；Alpha / Luminance 经过完整 Mask 层求值，nestedWriter 携带实际选项与诊断上下文。
4. Contour 按 renderer 有效覆盖抽取，接入 stroke 展开和 clip-rule，忽略无效后代但不错误忽略舞台不可见的 Mask 根；Mask 引用定义不能重复作为舞台内容绘制。
5. 按 renderer 离屏规则计算 alpha、blend 和 passThroughBackground；父 alpha 的传播不能用每 child 子树一个 opacity 包装替代逐项绘制。

**验收：** ST1—ST5 的不同父级 / 同模板复用、半透明根 Mask、Luminance 顺序、Contour stroke / 洞、嵌套 alpha 和背景隔离通过结构及像素验证；没有重复 ID、错引用或多次应用 transform / alpha。

#### S2：静态滤镜、背景效果与 3D 保外观回退

**涉及文件：** `src/pagx/svg/SVGExporter.cpp`、`src/pagx/svg/SVGFeatureProbe.h`、`src/pagx/svg/SVGFeatureProbe.cpp`、`src/pagx/utils/RasterUtils.h`、`src/pagx/utils/RasterUtils.cpp`、`test/src/PAGXSVGTest.cpp`；复用 renderer 层与效果能力。

**实施内容：**

1. 维护每步滤镜真实输入 / 输出，按运行时 style→filter 的顺序构图；shadowOnly、多 shadow 和上游变更不能聚合到最后就视为同义。
2. 从实际内容与效果 bounds 计算 filterUnits / primitive 空间，优先显式 user-space 区域；不能把像素 margin 当百分比。Mask 分层后仍保持正确效果尺度。
3. spread、blur tile、noise、style blend / excludeChildEffects、Layer.antiAlias 等已建模状态必须进入能力判断；有原生等价算法则实现，否则在 PreserveAppearance 下完整烘焙，BestEffort 才允许明确近似。shape-rendering / text-rendering 只作为环境相关提示，未经验证不能代替原生抗锯齿规则。
4. Probe 覆盖实际引用与 Mask 路径及背景依赖。BackgroundBlur / Glass 不再默认丢弃；使用已有带背景合成能力并实现第 8.5 节的绘制前缀替换，不重复绘制已消费内容。
5. 复用现有一般透视 / preserve3D bake，对上下文、共同深度、Mask、布局和滚动组合补测；仿射与常量 w 场景继续矢量，不笼统把全部 3D 转位图。
6. GPU / 编码 / 像素预算失败按静态策略返回；只画目标的 PNG 不得作为背景依赖场景的“成功兜底”。

**验收：** ST6 / ST7 及静态 3D 组合通过；小 bbox 大 blur 不裁、效果链顺序正确、背景补丁无二次合成、祖先前景不被提前消费，所有回退具备完整诊断和受控范围。

#### S3：TextBox、真实 GlyphRun 与文字输出模式

**涉及文件：** `src/pagx/svg/SVGExporter.cpp`、`src/pagx/utils/TextUtils.h`、`src/pagx/utils/TextUtils.cpp`、`src/pagx/TextLayout.cpp`、`src/renderer/GlyphRunRenderer.h`、`src/renderer/GlyphRunRenderer.cpp`、`src/pagx/utils/Woff2FontGenerator.h`、`src/pagx/utils/Woff2FontGenerator.cpp`、`test/src/PAGXSVGTest.cpp`；复用现有 WOFF2 生成和字形工具，不复制 renderer 的另一套放置公式。

**实施内容：**

1. TextBox 复用 Painter 作用域，不让 glyph-only Text、非文字子项、多 Painter 或内层 Group 状态被富文本收集遗漏；foreground 发射不重复整个 TextBox。
2. 从 renderer 的字形放置与合成样式提取或复用共用逻辑，覆盖 Font 分段、unitsPerEm、glyph offset、advance fallback、anchors、rotation / skew / scale 和 fauxBold / fauxItalic，含实际轮廓、advance 与 bounds；不把现有 TextUtils 注释或 Native CSS 字重声明当作其他字形模式等价的证据。
3. WOFF2 / 路径 / 位图 glyph 使用相同位置与 scope alpha。不能表达的 WOFF2 run 分段或正确转轮廓，不能给整个 Text 使用首个 Font。
4. 复用既有 TextLayout 与 FontConfig，Fidelity 不随意再猜系统字体；Native 以原文为输出但明确排版差异，保留缺字和资源失败诊断。
5. 静态 TextPath / TextModifier / RangeSelector 优先展开最终字形；不要求恢复原 modifier 的编辑结构，无法原生表达才按策略 bake。
6. PUA 可选择不等于原文可搜索。将可见字形、Unicode / 可访问原文和导出表示分别验证，不承诺不存在的逆映射。

**验收：** ST8—ST10 的 glyph-only、多 Font、缺省位置、offset / anchor 变换、真实半透明位图、RTL / 竖排 / 富文本与前景通过；所有文字模式使用同一有效布局与资源契约，PPT / HTML 共用字形路径不回归。

#### S4：几何、描边与静态修饰器完整语义

**涉及文件：** `src/pagx/svg/SVGExporter.cpp`、`src/pagx/utils/StrokeGeometryUtils.h`、`src/pagx/utils/StrokeGeometryUtils.cpp`、`src/pagx/utils/ModifierResolver.h`、`src/pagx/utils/ModifierResolver.cpp`、`src/pagx/ppt/PPTExporter.cpp` 及实际使用解析结果的 PPT 输出模块、`test/src/PAGXSVGTest.cpp`；复用 tgfx path / dash / stroke 算法。

**实施内容：**

1. 保留 primitive / Path 的原始起点、方向、renderPosition / scale 和原始 paint bounds；conic 转换采用确认的几何误差而非固定细分就宣称无损。
2. 各形状和字形的 Inside / Outside 使用原轮廓倍宽加裁剪，或等价展开路径；虚线和非圆椭圆不能继续靠 inset 近似。Fill 和 Stroke 各自 bounds 与状态正确。
3. 复用原生 dash 简化、自适应、cap / join 与 reversed 行为，必要时解析为显式几何；圆 / 椭圆的原生起点与 SVG 默认起点不同要处理。
4. 修饰器作用于累积有效几何，涵盖 Group 与 Repeater 展开后的来源；Append 保留子路径而非 Union，PPT 的 evenodd 补偿移至 PPT 端。
5. 修正 Repeater 幂、顺序、fractional copy alpha 和组合语义；源 Fill 的规则不被统一修改。局部产物返回对应 fill-rule / 来源，由输出端消费。
6. 临时几何局部持有，维护稳定顺序和复制 / 点数预算，不污染共享模板；动态 M5 在这一正确静态解析基础上扩展。

**验收：** ST11 / ST12 / ST16 的路径方向、虚线、内外描边、Append 描边、跨作用域 modifier、负缩放首副本和重复跨格式导出通过；几何细分有误差依据，模板 Fill / nodes 不被持久修改。

#### S5：渐变、图案、图片与实际资源解析

**涉及文件：** `src/pagx/svg/SVGExporter.cpp`、`src/pagx/utils/ImageFormatUtils.h`、`src/pagx/utils/ImageFormatUtils.cpp`、`src/pagx/utils/RasterUtils.h`、`src/pagx/utils/RasterUtils.cpp`、`src/renderer/LayerBuilder.h`、`src/renderer/LayerBuilder.cpp`、`test/src/PAGXSVGTest.cpp`；复用 tgfx ImageCodec / Gradient / ImagePattern fit。

**实施内容：**

1. 空渐变 stop 使用 renderer 默认黑白，退化 fit 与原生一致；在 Painter 空间解析 gradient matrix 与 geometry bounds，不让外部 Painter 随几何的 Group 错位。
2. 共享颜色和 P3 / alpha / 插值空间保持一致；原生能等价时用原生，固定环境下仍不同的算法按策略保外观，不笼统用 CSS 声明当保真证明。
3. 按 renderer 的资源优先级复用 runtimeImage、编码 data、data URI 与磁盘解析；解码和尺寸用通用能力，不再以 MIME 识别代替格式能解码或正确放置。
4. X/Y tile 独立处理 Repeat / Mirror / Clamp / Decal，全部 scaleMode 与旋转 / skew / 负缩放 fit 一起进入 shader / pattern 计算。
5. pattern bake 使用同一解析资源、fit matrix 和采样参数；不会因为无效 bounds / 烘焙失败就静默双轴 Repeat 或丢 scaleMode。
6. 本方案不自动联网取未供应资源；调用方资源前置条件、缺失、解码失败、输出嵌入和 GPU 像素编码失败都有诊断。

**验收：** ST13—ST15 的默认 stop / 退化轴、外部 Painter、真实多格式资源、runtimeImage 优先级、data URI、混合轴 tile、完整 fit / sampling 通过；M4 的动态资源在相同模型上扩展。

#### S6：公开选项、诊断、预算与静态交付回归

**涉及文件：** `include/pagx/SVGExporter.h`、`src/pagx/svg/SVGExporter.cpp`、`src/pagx/svg/SVGFeatureProbe.h`、`src/pagx/svg/SVGFeatureProbe.cpp`、`src/cli/CommandExport.cpp`、所有当前 SVGExporter 调用方、`test/src/PAGXSVGTest.cpp`、`test/src/PAGXCliTest.cpp`、共用 SVG 评测入口、`CMakeLists.txt`。

**实施内容：**

1. 完成第 8.6 节静态策略与文字模式，替换语义冲突选项，更新完整调用范围、CLI 帮助及 API 参数说明，不留两套优先级。
2. 静态和动画复用同一诊断基础；对每个有效节点 / 参数做能力台账：原生、显式解析、栅格化、拒绝或 BestEffort 损失，禁止沉默的 default 分支漏功能。
3. 数值有限性、深引用、几何 / glyph / 复制数量、像素、输出大小和总预算在公共边界与编译计划中校验；预算耗尽不是放弃检查的理由。
4. Strict 或 PreserveAppearance 失败不写 / 截断文件；warnings=nullptr 不改变成功判断；资源失败不会泄漏本机路径。
5. 完成显式静态全部路径、每种策略、字体环境、比例 / 色彩和重复导出确定性门禁，以及共享工具影响的 PPT / HTML 全量回归。

**验收：** ST1—ST18 全部可定位且有直接回归；能表达的内容不以告警代替修复；保外观和无新增 raster 模式有明确标准，未经浏览器验证的能力不列为完全等价。源码修改与截图变化按项目既定基准流程处理。

### 9.2 动画阶段 M0—M6

下面保留原动画阶段，复用 S0—S6 中已完成的共用能力；不得创建重复的静态 / 动态实例、资源、Mask 或字形解释器。

### M0：建立失败复现与验证入口

**目的：** 在开始重构前，把 F1—F12 的关键语义变成可检测行为。

工作：

1. 在 `test/src/PAGXSVGTest.cpp` 增加顶层与 Composition、PingPong 各属性、整圈 / 最短弧、零 alpha、groupOpacity、Mask / native TextBox、栅格化冻结等回归输入。
2. 将相关结构断言改为 XML 解析后的宿主、引用和轨道验证。
3. 在 `tools/html-snapshot` 的现有浏览器与图像比较基础上增加 SVG SMIL 采帧入口；先跑无需新增功能的静态控制样例和明确的失败复现。
4. 测试参考直接使用 PAGScene / 实例时间轴。当前 render CLI 只驱动顶层动画，不能直接拿来给嵌套动画判正确。
5. 在 `CMakeLists.txt` 接入需要的评测目标，使后续每阶段可调用同一验证入口。

验收：测试能够击穿原实现中的周期、初态、旋转、alpha 和实例错误；不因两张空白图或仅出现动画标签而通过。

### M1：实例、目标作用域与时钟

涉及：`SVGAnimationPlan.h`、`SVGAnimationPlan.cpp`、`SVGAnimationWriter.h`、`SVGAnimationWriter.cpp`、`src/pagx/PAGAnimation.cpp`、必要的内部共享时间函数、`PAGXSVGTest.cpp`、`PAGXTest.cpp`。

工作：

1. 移除错误的根 animations 空列表退出和 Animation 全局去重。
2. 按实际挂载实例枚举驱动，加入根动画选择、externalDoc 和目标成员范围校验。
3. 明确同属性驱动应用顺序，保留每实例不同 offset / playing。
4. 提取并复用纯时间映射；计算唯一 PingPong 周期；生成正确初始属性。
5. 完整覆盖循环引用和可复用兄弟实例的区别。

验收：官方 animation 样例能导出动画；仅 Composition 动画正常；双实例不同相位独立；transform / color / alpha / visibility 往返周期均为 2D；暂停与正负 offset 在原点及第二周期正确。

### M2：绘制作用域、变换和输出分支

涉及：`SVGExporter.cpp`、`SVGAnimationWriter.h`、`SVGAnimationWriter.cpp`、`ExporterUtils.h`、`ExporterUtils.cpp`、`PAGXSVGTest.cpp`。

工作：

1. DrawRecord 分离几何 transform 链与 Painter opacity 链。
2. 修复纯位置 override 的提前返回；Layer 动画复用当前已有的 LayerTransformUtils、组件字段及 runtime writer，按 matrix3D / matrix / 组件与布局位置的真实优先级接入，不新增重复模型，不沿用旧初稿“字段不存在”的假设。
3. 保留标量旋转圈数、变换操作顺序与锚点；Matrix 对齐最短弧规则。
4. 保持两种 groupOpacity 的绘制合成语义，完整计算各 alpha 因子。
5. 接通 Mask / clipPath、native TextBox、文字轮廓 / WOFF2 / 位图字形等实际输出分支；不能只给 nestedWriter 传一个指针就认定所有遮罩变换已正确。
6. 动态椭圆按 ellipse 输出，避免元素类型由静态正圆状态锁死。
7. 用动态依赖闭包限制整层静态栅格化，保留变换与遮罩坐标空间边界。

验收：纯位移、360 / 720 度旋转、跨 ±180 度 Matrix、Group 内外 Painter、透明度零起点、重叠子项、不同文字模式和遮罩动画通过属性及浏览器对照；共享工具的静态 SVG / PPT / HTML 不回归。

### M3：统一轨道编译与误差预算

涉及：`SVGAnimationWriter.h`、`SVGAnimationWriter.cpp`、必要时拆出的内部编译单元、`PAGXSVGTest.cpp`、M0 的浏览器评测入口、`CMakeLists.txt`。

工作：

1. 所有属性使用统一首尾、单关键帧、离散段和 spline 编译入口。
2. 原生等价轨道保留各自 easing；复合属性由完整表达式求值。
3. 对采样输出进行序列化后重建和误差检查；加入跳变、区间内部与相位边界。
4. 处理多时钟依赖，限制公共周期、样本量、递归和输出大小。
5. 对奇异变换、无法证明误差和预算耗尽产生确定诊断，不退成未经说明的线性近似。

验收：混合 Hold / Bezier、不同 easing 的 transform 和 alpha 乘积在区间内部正确；采样输出满足确认后的 D2；预算耗尽失败可重复、资源有界；同一输入导出稳定。

### M4：颜色源与渐变动画

涉及：`SVGAnimationPlan.cpp`、`SVGAnimationWriter.h`、`SVGAnimationWriter.cpp`、`SVGExporter.cpp`、`PAGXSVGTest.cpp`。

工作：

1. 接入 SolidColor、LinearGradient、RadialGradient 和 ColorStop 的实际运行时可动画字段，以通道表为依据。
2. 共享颜色源传播到各实际绘制引用；渐变自身参数和 stop 放在正确的 defs 实例中。
3. fitsToGeometry 渐变与动态几何尺寸一起计算，不仅对静态 bounds 换算关键点。
4. 颜色和 alpha 对齐 LerpColor / 色域转换；P3 样式不能把动画 presentation attribute 覆盖成静态值。
5. 同一颜色源经多个实例或不同时钟消费时，定义缓存不能误共享。

验收：共享 SolidColor 的多引用、渐变端点、径向参数、stop offset / color / alpha、动态 bounds、P3 与 alpha 同时变化，以及多实例资源引用全部有直接验证。

### M5：动态修饰器与路径输出

涉及：`ModifierResolver.h`、`ModifierResolver.cpp`、`SVGAnimationPlan.h`、`SVGAnimationPlan.cpp`、`SVGAnimationWriter.h`、`SVGAnimationWriter.cpp`、`SVGExporter.cpp`、`PAGXSVGTest.cpp`、浏览器评测入口。

工作：

1. 给解析产物增加完整来源映射，涵盖源几何替换、组合和复制。
2. 复用现有 modifier 算法，对局部求值状态生成动态几何，不持续扩张共享模板节点池。
3. 稳定拓扑路径编译 `d`，验证插值中间形状和误差。
4. 固定副本 Repeater 保留副本独立 transform / alpha；变化副本数和拓扑变化单独评估，不用几条字符串断言宣称支持。
5. 不支持策略同时覆盖 modifier 失去来源和整层烘焙的情况。

验收：静态 modifier 作用于动态 Rectangle / Path 不丢动画；TrimPath / RoundCorner / Polystar 可表达路径通过对照；复制来源、透明度、拓扑变化、预算耗尽与资源释放均有测试。

### M6：接口、CLI、诊断与交付回归

涉及：`include/pagx/SVGExporter.h`、`SVGAnimationPlan.h`、`SVGAnimationPlan.cpp`、`SVGAnimationWriter.h`、`SVGAnimationWriter.cpp`、`SVGExporter.cpp`、`src/cli/CommandExport.cpp`、`test/src/PAGXCliTest.cpp`、`PAGXSVGTest.cpp`、`CMakeLists.txt`。

工作：

1. 落实确认后的 D1—D6，完善公开参数、结果、诊断和选择语义说明。
2. 严格结果不依赖 warnings sink；生成成功前不打开文件，避免失败时截断已有输出。
3. CLI 选项校验、stderr 诊断、退出码和不写文件行为逐项测试。
4. 强化重复导出确定性、静态开关字节相等和无动态输入不回归。
5. 运行全量编译、测试和逐帧评测；共享工具改动覆盖 HTML / PPT。

验收：Strict 与 BestEffort 契约一致；官方样例真实播放；每个白名单支持项有区间语义验证；所有有效动态依赖得到输出或明确诊断；无未解释的回归。

## 10. 测试矩阵与质量门禁

### 10.1 三层验证

1. **结构层：** XML 合法、ID 唯一、引用完整、宿主正确、values / keyTimes / keySplines 数量匹配，时间有限有序。
2. **语义层：** PAGScene / Channel 连续求值与导出轨道在相同时间一致，检测初态、边界、跳变和中间值。
3. **浏览器层：** 实际 SVG 渲染与 PAGX 渲染对照，检测 transform 组合、opacity 合成、资源、字体和浏览器实现差异。

Round-trip 可以补充，但不是唯一判据：导入和导出共享同一个错误时也可能互相抵消。当前没有验证其他分支的 SMIL 导入实现，不作为本方案的前置依赖。

### 10.2 最低场景覆盖

| 维度 | 必测场景 |
|---|---|
| 动画选择 | 无动画、首个顶层、指定 ID、全部并播、首个 StateMachine、未知 ID |
| 实例作用域 | 仅 Composition、两个相同模板不同相位、外部同名 ID、越界 target、重复资源、引用环 |
| 时钟 | Once / Loop / PingPong，正负 offset、暂停、不同 fps、周期边界及第二周期 |
| 插值 | 单关键帧、关键点不覆盖首尾、Hold / None / Linear / Bezier 混合、不同通道 easing |
| 变换 | 原点纯位移、整圈 / 多圈、最短弧、锚点、skewAxis、负缩放、零缩放恢复 |
| 透明度 | 零起点、多个因子同时动画、内外 Painter、多 Painter、两种 groupOpacity 的重叠内容 |
| 几何 / 资源 | 圆变椭圆、roundness、描边对齐与 width、共享颜色、动态渐变 bounds |
| 输出分支 | Mask / clipPath、native TextBox、文字路径、WOFF2、位图字形、动态内容含不支持效果 |
| 安全与确定性 | 非有限数值、类型不匹配、预算耗尽、重复导出、唯一 ID、Strict 不截断文件 |

不固定“每能力只写一个测试”的数量指标。每项是否完成由上述语义分支和异常边界覆盖决定。

### 10.3 浏览器采帧规则

```javascript
svg.pauseAnimations();
svg.setCurrentTime(timeSeconds);
```

- 使用 SMIL 的时钟接口，不直接照搬 `document.getAnimations()` 的 WAAPI seek。
- 等待字体、图片和渲染提交完成，再采帧；采样不依赖墙钟推进。
- 参考 PAGScene 与 SVG 使用相同画布、时间、背景、缩放和色彩假设。
- 嵌套 timeline 从新场景按对应实例驱动到目标时刻，避免只设置根动画。
- 初期固定 Chromium 版本建立主门禁；目标浏览器范围确认后加入对应兼容验证，不凭旧文档下结论。
- 简单几何 / alpha 直接属性断言；复杂视觉结合像素差、关键区域与 SSIM。不要只看整图均值，防止小主体的大误差被背景掩盖。
- 对预期有可见内容的样例检查非空区域，对纯透明时刻使用预期透明断言，不统一要求每帧非透明。

### 10.4 编译与回归命令

本次评审运行的是已有 `PAGFullTest` 文件。当前 CMake 源码使用后端后缀，新构建按项目规范使用 `PAGFullTest_OpenGL`，不把旧文件名当作新构建目标。

```bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest_OpenGL
./cmake-build-debug/PAGFullTest_OpenGL --gtest_filter='PAGXSVGTest.*:PAGXTest.*:PAGXCliTest.*'
./cmake-build-debug/PAGFullTest_OpenGL
```

共享 ExporterUtils / ModifierResolver 的行为变化，需要实际覆盖现有 PPT / HTML 用例，不能仅编译通过。涉及 HTML 评测链路时追加：

```bash
cmake --build cmake-build-debug --target HTMLTest
```

- 测试返回非零表示失败，先定位根因，不原样重复命令直到碰巧通过。
- 同一工作区不并行跑不同后端截图测试。
- 不自动跳过缺失基线，不手改 version.json，不运行基准更新 target。
- 截图基准接受仅由用户主动执行 `/accept-baseline`。
- HTML 语料基准均值不手改；涉及有意保真提升时按项目规定的独立流程处理。

## 11. 最终交付条件

- [ ] D1—D6 已确认，文档与接口使用相同解释。
- [ ] 静态 ST1—ST18 各有直接回归与浏览器证据；第 8.2 节的有效绘制路径均有原生、几何解析、栅格化或明确拒绝策略。
- [ ] 静态 S0—S6 与动画 M0—M6 使用同一实例、绘制、资源、诊断和预算基础，没有重复解释器。
- [ ] 无新增效果 raster 与保外观模式分别验收，背景补丁不会重复消费背景或提前绘制 foreground。
- [ ] 静态 Fidelity / Native 文字、源 SVG 还原边界和 D6 环境限制明确，不把 PUA / PNG 当作原文或矢量无损的承诺。
- [ ] F1—F12 各有直接回归验证，已修复而非只补告警掩盖可修复错误。
- [ ] 每个已声明支持的通道同时覆盖初态、区间中间值和边界。
- [ ] 所有实际绘制分支都携带正确实例和依赖上下文。
- [ ] 动态依赖不会被默认静态栅格化冻结。
- [ ] 采样误差和资源预算满足确认后的契约，不能证明时明确处理。
- [ ] Strict 失败不写文件，BestEffort 的每处损失可定位。
- [ ] 重复导出、ID 与资源引用稳定，无未解释的共享模板增长。
- [ ] 静态 SVG、共享工具影响的 PPT / HTML、全量测试和浏览器门禁通过。
- [ ] 当前支持、近似支持和不支持能力在接口与文档中一致。

## 12. 文件索引

| 文件 | 方案中的责任 |
|---|---|
| `include/pagx/SVGExporter.h` | 导出选项、动画选择、策略与公开契约 |
| `src/pagx/svg/SVGAnimationPlan.h` | 实例绑定、驱动和依赖计划的数据结构 |
| `src/pagx/svg/SVGAnimationPlan.cpp` | 选择、作用域、目标解析、动态依赖与诊断 |
| `src/pagx/svg/SVGAnimationWriter.h` | 轨道与输出上下文接口 |
| `src/pagx/svg/SVGAnimationWriter.cpp` | 原生 / 采样轨道编译与 SMIL 写出，必要时拆分内部职责 |
| `src/pagx/svg/SVGExporter.cpp` | DrawRecord、Painter 作用域、宿主、defs、Mask、TextBox 与栅格化 |
| `src/pagx/utils/ExporterUtils.h` | 现有共享矩阵工具的覆盖接口 |
| `src/pagx/utils/ExporterUtils.cpp` | 位置 override 与共享变换语义 |
| `src/pagx/utils/ModifierResolver.h` | 修饰器局部求值与来源接口 |
| `src/pagx/utils/ModifierResolver.cpp` | 静态 / 动态修饰器语义、Append、来源与临时产物生命周期 |
| `src/pagx/utils/LayerTransformUtils.h` / `src/pagx/utils/LayerTransformUtils.cpp` | 复用当前静态组件、anchor、布局与 3D 变换语义 |
| `src/pagx/svg/SVGFeatureProbe.h` / `src/pagx/svg/SVGFeatureProbe.cpp` | 完整静态能力探测、Mask 旁路与背景依赖判断 |
| `src/pagx/utils/RasterUtils.h` / `src/pagx/utils/RasterUtils.cpp` | 完整合成上下文、背景补丁与像素预算 |
| `src/pagx/utils/TextUtils.h` / `src/pagx/utils/TextUtils.cpp` | 共用字形放置、offset / advance / alpha 与 Painter 作用域 |
| `src/pagx/utils/ImageFormatUtils.h` / `src/pagx/utils/ImageFormatUtils.cpp` | 实际图片供应、编码与通用尺寸解析 |
| `src/pagx/utils/StrokeGeometryUtils.h` / `src/pagx/utils/StrokeGeometryUtils.cpp` | 描边展开、裁剪、原始 paint bounds 与形状依赖 |
| `src/pagx/utils/Woff2FontGenerator.h` / `src/pagx/utils/Woff2FontGenerator.cpp` | WOFF2 编码与多字体字形数据 |
| `src/renderer/LayerBuilder.h` / `src/renderer/LayerBuilder.cpp` | 可复用的资源优先级、原生层与效果构建 |
| `src/renderer/GlyphRunRenderer.h` / `src/renderer/GlyphRunRenderer.cpp` | 字形位置、advance、anchor 和变换的共用依据 |
| `src/pagx/TextLayout.cpp` | 实际字体和静态布局，避免重复错误塑形 |
| `src/pagx/ppt/PPTExporter.cpp` 与实际 PPT 输出模块 | 把 PPT 专用几何 / fill-rule 补偿留在 PPT 端，并回归共享工具变化 |
| `src/pagx/svg/SVGImporter.cpp` 与 `include/pagx/nodes/Gradient.h` / `include/pagx/nodes/RadialGradient.h` | 核实源 SVG 展开与模型保留边界，不由导出端凭空补参数 |
| `src/pagx/PAGAnimation.cpp` | 提取纯时间映射，运行时既有行为保持不变 |
| `src/pagx/runtime/KeyframeEvaluatorImpl.h` | 复用的边界、插值与 Matrix 求值依据，不计划另造插值 |
| `src/pagx/runtime/MatrixDecompose.h` | 复用的矩阵分解与最短弧依据 |
| `src/pagx/PAGXNodeChannel.cpp` | 核实当前模型和可动画字段，不凭其他分支新增字段 |
| `src/cli/CommandExport.cpp` | CLI 选项、warnings、退出码和不写文件行为 |
| `src/cli/CommandRender.cpp` | 当前参考路径的限制；若未来复用它验收嵌套动画，需单独同步修正 |
| `test/src/PAGXSVGTest.cpp` | 结构、属性语义、资源与导出回归 |
| `test/src/PAGXTest.cpp` | 时间映射提取的运行时回归 |
| `test/src/PAGXCliTest.cpp` | CLI 契约与文件行为回归 |
| `tools/html-snapshot/eval-animation/baseline-frames.js` | 可参考的采帧基础，不直接复用其中的 WAAPI 驱动方式 |
| `tools/html-snapshot` 内拟新增 SVG SMIL 评测入口 | 浏览器与运行时的确定性逐帧验证，文件名在 M0 落实 |
| `CMakeLists.txt` | 新内部编译单元和评测目标接入 |

本轮交付仅为此方案文档。实现、源码格式化、编译和基线变更都应在方案确认后按上述阶段执行。
