# PAGX → SVG 开发方案与代码评审

> - 日期：2026-11-09
> - 评审分支：`feature/lifengdy_svg_export`
> - 评审源码：`8684fbee3`；动画主体提交：`1e3b114b2`
> - 状态：待评审，尚未开始实现本方案。
> - 仓库根目录：`/Users/lifengdy/Desktop/project/libpag`。下文源码定位均相对于该目录，行号对应评审提交。

## 1. 阅读结论

**保留现有静态 SVG 导出和 Plan / Writer 分工，先建立与 PAGX 运行时一致的实例、时钟、绘制作用域，再扩展动画通道。**

本分支已经有较完整的静态 SVG 导出。当前新增的 SMIL 实现可以覆盖部分单通道样例，但尚不能作为完整动画支持交付。最重要的缺口不是再增加几种 SVG 属性，而是顶层动画没有进入导出、Composition 实例共享时序、组合插值不等价、透明度和栅格化改变动画语义。

本方案的执行顺序为：

1. 建立能检测实际错误的语义测试和浏览器采帧入口。
2. 修复实例绑定、目标作用域和时间映射。
3. 修复变换、透明度及各输出分支的承载语义。
4. 建立原生轨道与受控采样轨道，覆盖组合属性。
5. 补齐 Layer 位移、颜色源、渐变及动态修饰器。
6. 收口 API、CLI、诊断和完整回归。

浏览器验证从第一阶段开始，不在扩展完通道后再补。各阶段同步完成对应测试，不以 XML 中出现 `<animate>` 作为功能完成标准。

本次保留目录中已有的 `docs/pagx-svg-export-plan.md` 原稿，不覆盖其内容。本文件是基于本次源码评审的独立方案；不沿用原稿中“动画基线已完成”或“先扩滤镜再处理实例”的判断。

## 2. 目标、交付边界与待确认决策

### 2.1 核心目标

- 对承诺支持的动画，SVG 与 PAGX 运行时在相同时刻具有一致的属性、绘制顺序和合成语义。
- 优先保留矢量、文字、资源引用和原生 SMIL，不默认用整段位图动画替代。
- 无法等价表达的组合属性可以进入受控采样，但必须满足明确的误差和资源预算。
- 所有无法支持、近似、冻结和资源失败都能定位到实例、节点和属性，不静默丢弃。
- 静态文档以及显式关闭动画的导出继续保持现有行为，不因动画改造回归。
- 不把修改模板文档后重新布局当作运行时动画求值，不破坏多场景共享模板的边界。

### 2.2 本次完整计划包含的能力

- 顶层 Animation 选择与导出。
- 已挂载 Composition 时间轴，包括多实例、不同 offset / playing 和已解析外部文档。
- 当前白名单通道的语义修正，以及 Layer x / y、共享颜色源、线性 / 径向渐变和色标动画。
- 动态修饰器和路径结果的来源追踪、能力判断与可表达路径的动画输出。
- 库接口、CLI 选项、错误行为和浏览器验证。

### 2.3 后续独立确认的能力

- StateMachine 输入、交互事件、DataBind / ViewModel 宿主行为的完整移植。
- 某个时刻的运行态矢量快照接口。
- Android / iOS / Web 等平台导出绑定。
- 动画位图序列、无法等价投影的 3D / 透视动画、背景模糊和玻璃效果。
- 需要动态重排版的文字内容、字体与复杂逐字形效果。

这些内容不是被当作已支持，而是有独立的输入和输出契约。当前模式遇到它们时仍必须明确诊断，不能默认为可保真导出。

### 2.4 编码前需要确认

| 编号 | 决策 | 推荐方案 | 为什么需要确认 |
|---|---|---|---|
| D1 | 多个顶层动画如何选择 | 默认按 `PAGScene::getDefaultTimeline()` 选择首个；另提供指定动画和显式全部并播 | 运行时默认选择首个，而当前 render CLI 会应用全部顶层 Animation。替代动画不应被无意叠加 |
| D2 | 是否接受受控采样 | 接受原生表达之外的有界近似；几何误差目标 0.25 文档像素，alpha / 颜色误差目标 1/255 | 若要求完全解析等价，则部分组合属性只能明确拒绝 |
| D3 | 不支持时默认行为 | 默认 Strict；显式 BestEffort 才允许告警后冻结或降级 | 成功写出文件不能等同于完整保留动画 |
| D4 | 是否要求纯矢量 | 允许沿用现有静态 PNG 补丁，但动态依赖不得被单张 PNG 冻结 | 如果输出必须纯矢量，需要同时约束现有 `bakeUnsupported`，不能只改动画 |

上述推荐均未视为用户已经确认。D1 首项如果是 StateMachine，Strict 应报告不支持，不偷偷跳到下一条 Animation。顶层选择作用于根动画；Composition 是否激活仍由其挂载的 driver 决定。`exportAnimation=false` 则关闭全部动画导出，包括嵌套实例。

## 3. 已核实的代码与验证证据

### 3.1 已执行验证

本次使用工作区已有 Debug 二进制，未重新编译。结果证明的是这些二进制的现有测试状态，不是对全部源码路径的重新构建认证。

| 验证 | 结果 | 能说明什么 |
|---|---|---|
| `PAGFullTest --gtest_filter='PAGXSVGTest.*'` | 163 / 163 通过 | 现有 SVG 测试未失败，不代表组合动画已保真 |
| 当前动画结构测试与两项 Composition 运行时测试 | 19 / 19 通过，其中 SVG 17 项、运行时 2 项 | SVG 17 项包括 16 个 `SVGExport_Anim*` 和 1 个静态结构测试 |
| `spec/samples/animation.pagx` 实际导出并解析 SVG | 动画元素数为 0，stderr 为空 | 合法顶层 Animation 没进入当前导出计划，而且没有丢弃诊断 |
| 工作区状态检查 | 未改动受版本控制的源码 | 本轮评审没有执行修复、格式化或基准接受 |

完整 SVG 测试的 stderr 有三次 PNG 解码错误，测试仍通过。本次未定位其原因，不将“断言通过”扩写为“日志没有异常”。

未执行：重新构建、全仓测试、浏览器逐帧像素对比、HTML / PPT 全量回归、跨浏览器兼容验证。其余缺陷为源码路径与运行时规则对照确认，实施时必须先加入对应复现测试。

### 3.2 现有测试的主要盲区

`test/src/PAGXSVGTest.cpp:4455` 起的动画相关测试主要检查字符串，不检测区间中间值、初始姿态、实例独立性和合成结果。

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
| SVG 导出 | 把选定场景行为编译为 SVG 结构与轨道 | 另造一套与运行时不同的插值和透明度规则 | `src/pagx/svg/SVGExporter.cpp:3662` |

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
| F5 | `src/pagx/utils/ExporterUtils.cpp:142` 的恒等判断未考虑 position override | 默认 Group 单独 `position.x: 0→100` 时中点必须平移 50，不能提前返回恒等矩阵 | M2 |
| F6 | `src/pagx/svg/SVGAnimationWriter.cpp:537` 对合成矩阵重新分解，没有保留标量圈数和展开最短弧 | 标量 `0→360°` 保留整圈；Matrix `170→190°` 中点应为 180°而非 0° | M2 |
| F7 | `src/pagx/svg/SVGAnimationWriter.cpp:432` 不接受 Hold；`src/pagx/svg/SVGAnimationWriter.cpp:940` 的派生属性轨道仍连续输出且端点不统一 | 宽度 100 Hold→200 的中点保持 100；15/45 帧关键点、60 帧时长需补齐首尾；多个 easing 不能靠关键点求值保证中间状态 | M2 / M3 |
| F8 | `src/pagx/svg/SVGAnimationWriter.cpp:772` 与 `src/pagx/svg/SVGAnimationWriter.cpp:816` 零 alpha 直接退出，`src/pagx/svg/SVGAnimationWriter.cpp:834` 使用静态比率；`src/pagx/svg/SVGExporter.cpp:2051` 未传 Painter / color 因子 | 零起点淡入正确；Group=0.5、Fill=0.5、colorAlpha=0.5 时结果为 0.125；各因子同时动画应组合而非覆盖 | M2 / M3 |
| F9 | `src/pagx/svg/SVGExporter.cpp:3419` 因任意 Layer 动画改变 perChildAlpha；`src/pagx/svg/SVGExporter.cpp:3573` 的组透明度承载不等价 | `groupOpacity=false`、静态 alpha=0.5、仅 matrix 动画仍保留逐项透明度；重叠内容不得改成整体合成 | M2 |
| F10 | `src/pagx/svg/SVGExporter.cpp:1784` mask nestedWriter 未传 Plan；`src/pagx/svg/SVGExporter.cpp:3324` native TextBox 未传完整路径 | mask 内形状动画、TextBox 自身和相关祖先动画完整到达实际输出节点 | M2 |
| F11 | `src/pagx/svg/SVGExporter.cpp:3393` 只保护 Layer 自身绑定；ModifierResolver 生成节点后缺少动画来源 | Layer 内 Group / Rectangle / Fill 动画不得被一张 PNG 冻结；被 modifier 替换的几何仍保留动态依赖 | M2 / M5 |
| F12 | `src/cli/CommandExport.cpp:184` 不收集 SVG warnings | 不支持或丢弃的动画在 stderr 可见；Strict 失败返回非零，不能仍打印正常成功结果 | M6 |

补充依据：

- 运行时目标范围检查：`src/pagx/PAGAnimation.cpp:76`。
- 每 Composition 独立 binding 与驱动：`src/pagx/runtime/PAGComposition.cpp:105`、`:130`。
- offset 求值规则：`src/pagx/PAGAnimation.cpp:221`。
- Matrix 最短弧：`src/pagx/runtime/MatrixDecompose.h:98`，测试 `test/src/PAGXTest.cpp:10264`。
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

保留当前 `SVGExportOptions::exportAnimation`、`SVGExporter::ToSVG / ToFile` 和静态选项。下面字段是待确认的拟议接口，不代表已经存在：

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
- 不在这一轮顺带增加快照、平台接口或与动画无关的 CLI 配置。

## 8. 能力矩阵与完成标准

“现有接线”只表示有代码路径，不表示组合语义已经正确。

| 能力 | 当前情况 | 本方案目标 | 阶段 |
|---|---|---|---|
| 静态 SVG 几何 / 文字 / 图片 / 渐变 / Mask / filter | 已有实现和测试，存在既有降级选项 | 保留并回归；不新增“全特性无损”承诺 | 全阶段 |
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
2. 修复纯位置 override 的提前返回；补 Layer x / y，不引入当前模型不存在的 rotation / anchor 等 Layer 字段。
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

1. 落实确认后的 D1—D4，完善公开参数、结果、诊断和选择语义说明。
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

- [ ] D1—D4 已确认，文档与接口使用相同解释。
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
| `src/pagx/utils/ModifierResolver.cpp` | 来源追踪、动态几何解析与临时产物生命周期 |
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
