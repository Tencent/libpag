# PAGX → PAG 二进制格式技术方案（PAG v4）

> 版本 v1.0-rc ｜ 日期 2026-10-22 ｜ 状态：终审修订完成，待确认后进入实施计划
>
> 依据：`spec/pagx_spec.zh_CN.md`、`include/pagx`、`src/pagx`、`src/renderer`、旧版 codec `src/codec`、`include/pag`、
> 旧版 AE 导出插件 `exporter/src`，以及前序原型 `libpag_pagx_to_pag/` 的经验。
>
> v1.0-rc 相对 v0.16 的变化（终审修订，§14）：
> - 补充：枚举扩展规则（§3.2）、资源内联/共享规则与引用不跨文档（§5.2）、关键帧缓动与通道名存储（§5.3）、图片零拷贝的 Data 切片（§5.1）、长度校验（§5.5）、发版前不提交二进制产物（§12.1）；
> - 附录 B 补 Document、Composition 与 Layer 时间线；§7.4 特效逐项列出、投影 spread、运动模糊逐帧确定性；§8.5 按旧接口类整理。
>
> v0.16 相对 v0.15 的变化：三期与四期对调，三期为旧 PAG 迁移，四期（最后一期）为平台接入；平台接口一次设计完整，包含迁移新增的接口（C29、C34、§12.1）。
>
> v0.15 相对 v0.14 的变化：
> - 旧 PAG 迁移必须覆盖旧 PAG 的全部能力，作为最后一期（四期）实施；平台接入调为三期（C34、§12.1）；
> - Marker 不支持，迁移时跳过并记录（C35）；新增旧 PAG 全量能力清单，取消“近似”保真度（§7.1、§7.4）。
>
> v0.14 相对 v0.13 的变化（第二轮编码就绪度评估）：
> - 视频与位图序列合成纳入发版范围，新增四期与设计纲要（C28、C33、§6.6、§12.1）；
> - 路径量化步长按显示缩放自适应（§4.7）；子文档展开数量上限（§4.8）；导出结果确定性（§4.1）；解码资源上限（§5.5）；
> - 新增旧接口对照要求（§8.5）、量化验收指标（§13.3）；PAGXOptimizer 进入流水线评估（§4.1）。
>
> v0.13 相对 v0.12 的变化：
> - 确认 ViewModel/DataBind 放二期、一期遇到动效内容只导出静态画面并告警、三期全部完成并完成平台接入后才对外发版（C31–C33）；
> - 发版前格式允许修订，发版即冻结（§12）；一期的静态画面定义与对比范围（§12.3）。
>
> v0.12 相对 v0.11 的变化：
> - 确认 Q1 时间锁定挂载、Q2 保留三处 `data-*`、Q3 视频合成后置（C26–C28）；
> - 改为三期实施：静态 PAGX → 动效 PAGX → 旧 PAG 迁移；规范一次定全、实现分期，新增分期范围与预留清单（§12，C29）；
> - 新增外部子 PAGX 的完整处理（§4.8，C30）；§6.3 细化时间锁定挂载。
>
> v0.11 相对 v0.10 的变化（编码就绪度评估）：
> - 修正 v0.8 改为“按需保留 id”后残留的矛盾表述（§3.5、§4.4、§5.2、§5.4、§9、附录 B）；
> - 用 `pag::File` 统计仓库 166 个可解码旧 pag 的特性分布，修正迁移优先级（§7.4）；
> - 新增 §13：编码就绪度评估、待决问题（嵌套合成时钟、data-* 剥离范围、视频合成）与 AI 编码执行约定。
>
> v0.10 相对 v0.9 的变化：渲染对比页脚本改名为 `test/gen_pagx_pag_comparison.sh/.py`，输出目录改为 `test/out/pagx_pag_comparison/`（§10.3）。
>
> v0.9 相对 v0.8 的变化：
> - v0.8 的 id、命名、默认时间线、CLI 四项已确认（C21–C24）；新增 `pagx import --input old.pag`；新测试样本集放在 M0（C25）；
> - 明确旧 PAG 迁移的内部流程与中间产物（§7.1）；旧图片的缩放与裁边改用 ImagePattern 矩阵表达，已是 WebP 的图片不重新编码（§4.5、§7.1）。
>
> v0.8 相对 v0.7 的变化：
> - id：不生成合成 id，改为按需保留原 id（§5.2）；
> - 目录与接口命名对齐现有 svg/html/ppt 风格：`src/pagx/pag/`、`PAGExporter/PAGImporter`（§2）；
> - 播放统一使用 `PAGScene::getDefaultTimeline()`，并统一各播放端（§5.4、§8.2、§8.3）；
> - 取消 `pagx upgrade`，改用现有的 `import/export` 命令（§8.4）；
> - 新增测试样本覆盖评估与样本集规划、PAGX/PAG 渲染对比页（§10.1–10.3）；§6 补充旧 PAG 参考样本。
>
> v0.7 相对 v0.6 的变化（对照 PAGX 代码全面回顾）：
> - 修正：旧 codec 只复用流工具、Tag 读写和编码布局，属性块/路径/关键帧的读写函数要为 PAGX 类型新写；路径记录扩为 4 位以容纳 Quad（§2、§4.7）；
> - 补充：扁平化细则（只写回布局分配的值、Text 位置反解、缩放烘焙同步变换几何关键帧、clipToBounds、指向布局专用通道的动画/DataBind）（§4.2）；
> - 补充：节点序号、外部合成、引用作用域、声明顺序（§5.2）；基础类型编码约定（§5.6）；
> - 修正剥离规则：Composition 作用域的动画会自动播放，不能删（§4.6）；
> - 新增附录 D 边界情况清单；C20 旧 PAG 迁移生成根 Animation。
>
> v0.6 相对 v0.5 的变化：
> - 确认导出期字体参数 `--fallback`、嵌入文本被修改后回退到注册字体（C17、C18）；
> - 明确新 PAG 同时支持静态矢量与矢量动画（C19、§5.4）。
>
> v0.5 相对 v0.4 的变化：
> - 所有文本默认运行时排版，不再区分静态/动态文本，与旧 PAG 一致；
> - 支持字体嵌入场景：PAGX 源文件中已嵌入的字形（Font + GlyphRun）原样保留；
> - 布局改为“导出期求解 + 扁平化成显式属性”，加载期照常调用 `applyLayout()`，只剩逐节点测量与文本排版（§4.2）；
> - 补充嵌入文本被修改时回退到注册字体排版（§4.3）、导出期字体注册；
> - 第 11 节由待确认问题改为决策依据。
>
> v0.4 相对 v0.3 的变化：可编辑内容改为 ViewModel + DataBind；旧 PAG 文本属性全部支持；CLI 去掉图片/字体参数；补充 Extra/V2 Tag、tagLevel 的代码依据。

---

## 0. 已确认的决策

| 编号 | 决策 |
|---|---|
| C1 | 扩展名沿用 `.pag`，文件头 version = 4 |
| C2 | 版本兼容机制沿用旧 PAG：文件头版本只用于不兼容的格式变更；同一格式内新特性只追加 Tag，旧 SDK 跳过不认识的 Tag；新增字段优先用补充 Tag，必要时用 V2 替代 Tag；tagLevel |
| C3 | 新 SDK 继续打开旧 pag，旧文件走旧运行时，行为不变；端上不做即时转换 |
| C4 | 平台接口“播放统一、对象模型分离”；格式按文件头精确识别 |
| C5 | PAGViewer 同时支持旧 pag、新 pag、pagx |
| C6 | 只做 PAGX → PAG 单向转换和旧 PAG → 新 PAG 迁移，均通过 pagx CLI 离线完成 |
| C7 | 转换时求解全部自动布局并扁平化成显式属性，剥离渲染无关数据；保留图层名，`data-*` 按 C27 处理 |
| C8 | 可编辑能力由 ViewModel + DataBind 提供：文档带 ViewModel 才可编辑。对外接口是获取 ViewModel 并修改属性值，不再提供“可编辑文本/图片图层”接口；宿主不按 id 查找节点 |
| C9 | 旧 PAG 支持的文本属性（文本、字体、字号、颜色、粗/斜体、描边、描边宽度、对齐、行距等）在新格式中全部继续支持；旧 PAG 迁移时把可编辑文本/图片映射为 ViewModel |
| C10 | 所有文本默认运行时排版，不区分静态/动态文本，不存预排版字形，与旧 PAG 一致；PAGX 源文件中已嵌入的字形（`pagx embed` 生成的 Font + GlyphRun）原样保留 |
| C11 | 字体文件不嵌入，由宿主通过全局字体注册接口提供；图片嵌入沿用旧 PAG 逻辑，使用默认参数 |
| C12 | 数值精度沿用旧 PAG |
| C13 | 不启用 LZ4 等通用压缩（实测收益约 1.5%），compression 恒为 `'U'` |
| C14 | 旧 PAG 迁移所需的全部能力在 PAGX 中完整支持（§7.4）；迁移以语义转换为主线 |
| C15 | 新格式 TagCode 从 256 开始 |
| C16 | CLI 首发不提供 `--image-quality`、`--image-pixel-ratio`、`--export-fonts`，使用默认值 |
| C17 | `pagx export --format pag` 自动注册文档中带 `file` 的 Font，并支持 `--fallback <path\|name>`，用于导出期布局测量；缺字体写入 warnings |
| C18 | 嵌入字形的文本被 DataBind 或动画修改文本类属性后，改用注册字体排版；首帧仍显示嵌入字形 |
| C19 | 新 PAG 与 PAGX 一样同时支持静态矢量和矢量动画（关键帧动画、状态机、数据驱动），二者可在同一文件中混合；纯静态文件不带任何时间线数据（§5.4） |
| C20 | 旧 PAG 迁移时为根合成生成同时长、同帧率的根 Animation，即使原文件没有关键帧，也保留旧播放接口的时长与进度语义 |
| C21 | id 按需保留：Layer、被运行时按字符串查找的节点、Animation 与 StateMachine 保留原 id，其余剥离；不生成合成 id（§5.2） |
| C22 | 命名对齐现有格式目录：`include/pagx/PAGExporter.h`、`PAGImporter.h`，代码放在 `src/pagx/pag/`（`tags/`、`exporter/`、`importer/`）（§2） |
| C23 | 所有播放端统一用 `PAGScene::getDefaultTimeline()`；规范写明默认时间线规则（§5.4） |
| C24 | 不新增 `upgrade` 命令：`pagx export --format pag` 接受 PAGX 与旧 pag 输入；`pagx import` 接受旧 pag 输入，输出 PAGX 用于审查迁移结果；新 pag 输入一律报错（§8.4） |
| C25 | 新测试样本集 `resources/pagx_to_pag/` 在 M0 建立（§10.2） |
| C26 | 嵌套合成采用“时间锁定挂载”：子实例时间由父时钟计算，父时钟定位时子实例同步定位；旧 PAG 迁移的预合成全部使用（§6.3） |
| C27 | `data-*` 只保留三处：ViewModelProperty（`PropertyData::customData()` 公开接口）、文档根（宿主读取背景色）、ImagePattern（宿主读取 `image-scale-mode`）；其余剥离 |
| C28 | 视频合成、位图序列合成的迁移纳入发版范围，随旧 PAG 迁移在三期实现（§6.6） |
| C29 | 分期实施：一期静态 PAGX → 新 PAG，二期动效 PAGX → 新 PAG，三期旧 PAG → 新 PAG，四期平台接入（最后一期）。规范和格式一次定全，实现分期，避免返工（§12） |
| C30 | 支持 PAGX 引用外部子 PAGX：导出时递归打包进同一个文件，加载时不再访问外部文件（§4.8） |
| C31 | ViewModel、DataBind、DataConverter 放在二期实现 |
| C32 | 一期导出器遇到动效内容（Animation、StateMachine、时间线挂载、ViewModel/DataBind）时，只导出静态画面并告警，不报错（§12.3） |
| C33 | 各期只在内部交付，不单独对外发版；一期至四期全部完成后才对外发版。发版前文件格式允许修订，发版即冻结，之后按 C2 只追加 Tag |
| C34 | 旧 PAG 迁移覆盖旧 PAG 的全部能力，一项都不能漏，在三期实施（平台接入之前）。“能力”以旧运行时实际的渲染结果和对外接口为准；验收标准是迁移结果与旧运行时在每个整数帧渲染一致，不接受“近似”（§7.1、§7.4） |
| C35 | Marker（`markers`、`audioMarkers`）暂不支持，迁移时跳过并在报告中记录；后续视需求决定是否支持 |

---

## 1. 目标与非目标

**目标**

1. 承载 PAGX 全部渲染相关特性，同时支持静态矢量和矢量动画：矢量、图片、文本、样式/滤镜/遮罩、Composition、动画/状态机/时间线、ViewModel/DataBind。
2. 压缩率最高、加载最快：只存渲染与运行时行为所需的数据；加载期零 XML 解析、无约束/弹性布局求解；文本与旧 PAG 一样在加载时排版。
3. 沿用旧 PAG 经过多年验证的设计：容器、版本兼容、图片嵌入、字体注册、数值精度。
4. 支持旧 PAG → 新 PAG 的离线迁移，并输出保真度报告。

**非目标**

- PAG → PAGX 逆向转换。
- 二进制文件加载后的“结构编辑 + 重新布局”。
- 旧 SDK 播放新文件。
- 保留旧 PAG 的“可编辑文本/图片图层”接口。

---

## 2. 总体架构

```
.pagx ──PAGXImporter──> PAGXDocument ──PAGExporter（发布流水线 §4）──> .pag v4
旧 .pag ──PAGImporter（Codec::Decode → pag::File → 转换）──> PAGXDocument ──同上──> .pag v4

.pag v4 ──PAGImporter──> PAGXDocument（渲染子集，布局已扁平化）──> PAGScene（PAGX 运行时，加载时排版文本）
旧 .pag ──Codec::Decode──> pag::File ──> 旧渲染器（不变）
```

- **解码产物仍是 `PAGXDocument`，交给 `PAGScene`**：动画、DataBind、状态机都通过通道写入器作用在 Node 上（`src/pagx/PAGXChannelTable.h`），
  复用节点模型才能保证行为一致，不需要第二套渲染器。
- **模块划分**：

| 位置 | 内容 |
|---|---|
| `include/pagx/PAGExporter.h` | `PAGExporter`：PAGXDocument → 新 .pag（与 `SVGExporter`、`HTMLExporter`、`PPTExporter` 同一命名方式） |
| `include/pagx/PAGImporter.h` | `PAGImporter`：.pag → PAGXDocument。v4 直接解码；旧版本经转换，只在工具链中编入（与 `SVGImporter`、`HTMLImporter` 同一命名方式） |
| `src/pagx/pag/` | 格式目录，与 `svg/`、`html/`、`ppt/` 并列：`PAGExporter.cpp`、`PAGImporter.cpp`、TagCode 定义、编解码上下文 |
| `src/pagx/pag/tags/` | 各类节点的 Tag 读写（对应旧 codec 的 `src/codec/tags/`） |
| `src/pagx/pag/exporter/` | 导出前处理：布局扁平化、剥离、图片处理（对应 `html/importer/` 的拆分方式） |
| `src/pagx/pag/importer/` | 旧版 PAG 导入：`pag::File` → PAGXDocument |

- **复用旧 codec 的范围**：
  - 直接复用：`EncodeStream/DecodeStream`（varint、位读写、按位宽打包的整型/浮点列表）、`TagHeader`、`ReadTags/WriteTag`、精度常量。
  - 复用编码布局、新写读写函数：属性块（存在位 + 非默认值，对应 `AttributeHelper`）、路径（对应 `WritePath`）、关键帧（对应 `WriteKeyframes`）。
    原因是旧函数绑定 `pag::Property`、`pag::PathData`、`pag::Keyframe` 等旧类型，PAGX 节点是普通字段、`pagx::PathData` 与 `pagx::Keyframe` 也是另一套类型。
- **编译依赖**：
  - 运行时只编入 `PAGImporter` 的 v4 解码与 `tags/` 的读取部分；`PAGExporter`、`exporter/`、`importer/` 只编入 CLI 与测试（`PAG_BUILD_CLI`、`PAG_BUILD_TESTS`），`importer/` 依赖旧 `pag::File`。
  - 主工程按子目录 GLOB 收集 PAGX 源码（`CMakeLists.txt:305-314`），新增子目录要加入；独立的 PAGX 构建（`pagx/wechat/CMakeLists.txt`）不编译 `src/codec`，需要把解码器依赖的流工具一并编入（一期梳理依赖）。

---

## 3. 版本与兼容（沿用旧 PAG 设计）

### 3.1 旧 PAG 的兼容机制（代码核实）

**① 同一格式内的新特性：只追加 Tag，旧 SDK 跳过不认识的 Tag**

- `ReadTags`（`src/codec/TagHeader.h:31-48`）逐个读取“编号 + 长度”，把 Tag 内容交给处理函数；处理函数遇到不认识的编号走 `default` 分支直接忽略，
  例如 `ReadTagsOfLayer`（`src/codec/tags/LayerTag.cpp:193`）、文件级处理表 `ReadTagsOfFile`（`src/codec/tags/FileTags.cpp`）。
- 编码器一直写 `Version = 1`（`src/codec/Version.h`）。多年来新增的特性（3D、相机、描边样式、外发光等）都没有修改版本号，
  旧 SDK 能打开新文件，只是缺少不支持的效果。

**② 不兼容的格式变更：文件头版本号硬拒绝**

- `ReadBodyBytes` 拒绝 `version > KnownVersion(3)`（开源首版为 `CompatibleVersion = 2`），3 为加密格式。

**③ 新增字段：补充 Tag 与替代 Tag 两种做法**

| 旧 Tag | 内容 |
|---|---|
| `LayerAttributes`（V1） | 基础字段 |
| `LayerAttributesV2` | V1 全部字段 + `name` |
| `LayerAttributesV3` | V2 全部字段 + `motionBlur` |
| `LayerAttributesExtra` | 只有 `name` 和 `motionBlur` |

  - **V2/V3 是“替代 Tag”**：重新编码全部字段，写了 V2 就不再写 V1。不认识 V2 的 SDK 会连同 V1 的字段一起丢失。
  - **Extra 是“补充 Tag”**：只装新增字段，与 V1 并存。不认识 Extra 的 SDK 仍能读到 V1，只丢新字段。
  - 编码器的选择（`src/codec/tags/LayerTag.cpp:258-265`）：有 `motionBlur` 时写 V1 + Extra，而不是 V3，就是为了让更旧的 SDK 尽量多读到内容。
  - “Extra”只是旧代码里的命名习惯，本质就是一个新的 TagCode。

**④ tagLevel**

- **tagLevel** 不存在文件里，是解码时统计出来的：每读到一个 Tag 头就记录最大编号（`src/codec/TagHeader.cpp:31-33`），解码结束写入 `File::_tagLevel`（`src/codec/Codec.cpp:406`）。
- **MaxSupportedTagLevel** 是当前 SDK 认识的最大编号，等于 `TagCode::Count - 1`（`src/codec/Codec.cpp:47`）。
- 两者的关系：`tagLevel > MaxSupportedTagLevel` 说明文件里有当前 SDK 不认识的 Tag，它们已被跳过；文件能打开，但部分效果缺失。宿主可据此提示升级或换用降级素材。

### 3.2 新格式的处理

| 场景 | 处理 |
|---|---|
| 旧 SDK 打开新格式 | 文件头 version = 4，`ReadBodyBytes` 立即拒绝。原因：新格式的内容全部是 256 以后的新 Tag，没有旧格式的合成数据。即使沿用 version 1，旧 SDK 也会跳过所有 Tag 后因“没有任何合成”在 `VerifyAndMake` 失败（`src/codec/Codec.cpp` 中的 `!compositions.empty()`）。用 version 4 让它在文件头就失败，正是旧设计处理不兼容变更的方式 |
| 新格式自身的演进 | 完全按旧机制：版本号保持 4；新特性追加 Tag，较早的新 SDK 跳过不认识的 Tag 并照常播放；新增字段优先用补充 Tag（按内容命名，如 `LayerTransform`），只有已有字段的编码必须改变时才用 V2 替代 Tag |
| 枚举新增取值 | 发版后给已有枚举字段（混合模式、插值类型等）新增取值时：原字段写一个旧 SDK 认识的最接近取值，新取值写进补充 Tag。旧 SDK 读到的是可降级的效果，不会出现未知值；解码器遇到未知值（损坏文件）时取默认值并告警 |
| tagLevel | 新运行时同样在解码时统计 `tagLevel`，并提供 `MaxSupportedTagLevel` |
| 导出器目标 tagLevel | 旧 AE 导出器的“导出版本控制”（Stable/Beta/Custom）首发不需要，等新格式出现第二批 Tag 后再加 |

### 3.3 文件头与版本分流

分流点在加载入口：读前 16 字节识别格式（§3.6）。version ≤ 3 仍交给现有 `Codec::ReadBodyBytes`（`src/codec/Codec.cpp:209`，其中 `version == 3` 的加密判断保持在前），`KnownVersion` 不改；version 4 交给新解码器。

文件头与旧格式逐字节一致：`'P' 'A' 'G'` + version(u8) + bodyLength(u32 LE) + compression(i8 = `'U'`)。

| version | 含义 | 新 SDK 处理 |
|---|---|---|
| 0 / 1 / 2 | 旧格式（仓库样本分别 102 / 166 / 14 个） | 旧 codec + 旧运行时 |
| 3 | 旧加密格式 | 保持现有逻辑 |
| 4 | 新格式 | 新解码器 + PAGX 运行时 |
| ≥ 5 | 未来不兼容版本 | 拒绝，返回“需要更高版本 SDK” |

### 3.4 TagHeader 与编号空间

- **TagHeader 与旧格式相同**：uint16，高 10 位为编号，低 6 位为长度；长度 ≥ 63 时写 63 并追加 u32。10 位决定编号上限为 1023。
- **编号分配**：

| 区间 | 用途 |
|---|---|
| 0 | End，两种格式共用 |
| 1–255 | 留给旧格式。已用到 94，2023-06 以后没有新增 |
| 256–1022 | 新格式 |
| 1023 | 保留 |

### 3.5 Tag 结构

沿用旧格式：

- **容器节点用 Block**：如 Layer、Group、Composition。内部是子 Tag 流，以 End 结束；节点属性放在属性子 Tag 中。
- **叶子节点用属性 Tag**：如 Fill、Rectangle。
- **属性编码沿用 `AttributeHelper` 的布局**：先写存在位（BitFlag 布尔值直接存在位里），再写非默认值；读写函数为 PAGX 类型新写（§2）。

### 3.6 格式识别

| 文件头 | 判定 |
|---|---|
| 前 3 字节为 `'PAG'` | 按第 4 字节分流（§3.3） |
| 跳过可选的 UTF-8 BOM 与空白后首字符为 `'<'` | PAGX |
| 其它 | 未知格式 |

- 二进制首字节为 `'P'`，XML 只能以 `'<'`、BOM 或空白开头，不会混淆。仓库 317 个 pagx 样本全部以 `<` 开头（1 个无效文件除外）。
- 按路径加载时只读前 16 字节。

### 3.7 `PAGFile.Load` 为什么不加载新格式

- **旧 SDK**：文件头版本拒绝（§3.2）。
- **新 SDK**：`PAGFile` 是旧对象模型（`PAGFile/PAGLayer` 图层树，底层是 `pag::File`），新格式解码出的是 PAGX 节点模型，由 `PAGScene` 播放，两者无法一一映射。
  因此按“对象模型分离”，`PAGFile.Load` 遇到新格式返回明确错误码，提示改用新接口；播放视图则自动识别格式并选择内核，只做播放的业务不受影响。

---

## 4. 发布流水线

### 4.1 步骤

1. 解析导入指令；递归加载外部子 PAGX（§4.8）与外部图片
2. 注册导出期字体（§4.3），对主文档和每个子文档执行 `applyLayout()`，求解全部约束与容器布局
3. 布局扁平化：把布局结果写回显式属性，删除布局输入（§4.2）
4. 剥离渲染无关数据，删除死节点，资源去重（§4.6）
5. 图片处理（§4.5）
6. 编码（精度见 §4.7）

缺图片、缺字体、缺子文件、不支持的特性等写入 warnings，CLI 可选输出报告；`--strict` 时任何 warning 都导致失败。

- **结构优化**：在第 3 步之后可调用现有的 `PAGXOptimizer`（合并冗余图层和 Group、把描述矩形/椭圆的 Path 改写为图元，保证渲染不变，`include/pagx/PAGXOptimizer.h`）。扁平化后文档不再有布局节点，它能优化更多结构。M0 用样本实测体积和加载收益后决定是否默认开启。
- **结果确定性**：同一输入、同一工具版本必须输出逐字节相同的文件。字符串表、资源表、图片表、子文档表的顺序都按文档遍历顺序生成，不依赖哈希表的迭代顺序；文件中不写时间戳。T3 增加“两次导出逐字节比较”。

### 4.2 布局：导出期求解并扁平化，加载期只测量

**已核实的运行时行为**

- 文本排版发生在 `applyLayout()` 内部：独立 Text 在 `Text::onMeasure` 中排版（`src/pagx/nodes/Text.cpp:34`），TextBox 的子文本在 `TextBox::onMeasure/updateLayout` 中排版（`src/pagx/nodes/TextBox.cpp`），结果写入 `glyphData->layoutRuns`，构建渲染树时生成 TextBlob（`src/renderer/LayerBuilder.cpp:1051`）。
- `PAGScene::Make` 只在 `!isLayoutApplied()` 时调用 `applyLayout()`（`src/pagx/PAGScene.cpp:75`）。
- 动画、DataBind、状态机运行时都不会重新布局；文本类属性被修改时由 `TextHolder` 重排单个 Text 或 TextBox，位置保持不变（`src/renderer/TextHolder.cpp:168`）。

v0.4 的做法是“存布局输出、标记已布局、跳过 `applyLayout()`”。改为运行时排版后这条路走不通：跳过 `applyLayout()` 就不会排版，没有嵌入字形的文本不显示；另写一套“只排版”的流程，又会和 `Text/TextBox` 的测量逻辑形成两套语义。

**新做法：扁平化**

- **导出期**：用导出机字体执行 `applyLayout()`，把每个节点的布局结果写回它自身的显式属性，再删除全部布局输入（约束、百分比尺寸、容器布局属性等，见 §4.6）。扁平化后的文档仍是合法的 PAGX 文档，可以导出 XML 检查。
- **加载期**：解码器不标记“已布局”，`PAGScene::Make` 照常调用 `applyLayout()`。文档里已经没有约束和布局容器，`applyLayout()` 退化为逐节点测量，主要开销就是文本排版，与旧 PAG 加载时排版文本相同。
- **好处**：二进制与 XML 走同一条运行时代码路径，不需要给 `LayoutNode/PAGXDocument` 增加 friend 入口。
- **与动画的关系**：规范规定布局属性在运行时不存在，动画不能修改布局属性，只作用于布局结果之上（`spec/pagx_spec.zh_CN.md` §4.4、§9.7）。扁平化做的正是规范描述的“把布局结果写入绝对坐标、丢弃布局属性”；被动画驱动的属性只改写其静态初值，关键帧值的语义不变。

**扁平化规则（一期对照各节点 `render*()` 访问器逐类定稿）**

通用原则：

- 只写回“由布局分配”的值；由内容决定的轴（如固定宽度 TextBox 的自动高度）保持自动，运行时按注册字体重新测量，字体不一致时更自然。
- 规范规定的取整（Layer 位置取整、内容尺寸向上取整，规范 §4.4）按求解结果原样写回。
- 不是 LayoutNode 的元素（Fill、Stroke、各修改器、Repeater、TextModifier、RangeSelector、样式、滤镜）不受影响，原样保留。

| 节点 | 布局结果写回 |
|---|---|
| Layer | 位置写回 `x/y`；尺寸只在渲染需要时写回 `width/height`；`clipToBounds` 按 `Layer::updateScrollRect`（`src/pagx/nodes/Layer.cpp:289`）写成 `scrollRect` 后清除 |
| Rectangle / Ellipse | `position`、`size` |
| Polystar | 布局缩放乘进半径，位置写回 `position`。运行时半径写入器写原始值（`src/renderer/LayerBuilder.cpp:1429`），与 XML 路径一致，关键帧不用变换 |
| Path / TextPath | 布局缩放乘进路径坐标（TextPath 同时缩放基线原点与边距），位置写回 `position`。Path 已有逐点通道 `point{i}.x/y`，写入时会乘 `renderScale`（`src/renderer/LayerBuilder.cpp:248-262`），烘焙后缩放变为 1，所以要把指向该节点几何的关键帧值同步乘上缩放（逐点通道与 §6.1 的 path 通道） |
| Group / TextBox | `position`、布局分配的 `width/height`；TextBox 的 `padding` 是排版输入，保留 |
| Text | 写回的 `position` 按 `Text::renderPosition`（`src/pagx/nodes/Text.cpp:61`）的公式反解，保证加载时重新测量后位置一致；布局造成的适配缩放写成 Text 自身的 `width/height`；`textAnchor/baseline` 不变，字体不一致时仍按原锚点对齐 |

**指向布局属性的动画与 DataBind**

- 布局专用通道（`width/height`、百分比、约束、`gap/flex/padding`、`clipToBounds`、`includeInLayout`）在通道表里只有 `Layout` 标记、不可动画（`src/pagx/PAGXNodeChannel.cpp:383-420`），运行时也没有写入器，XML 路径下指向它们的动画和 DataBind 同样不生效。导出时删除并警告。
- 可动画又影响布局的通道（Layer `x/y`、Rectangle `size`、Polystar 半径、Text 的 `text/x/y` 等，标记 `AnimLayout`）保留。运行时写入器把关键帧值直接写在布局结果之上，与 XML 一致（规范 §4.4）。

**解码后文档的编辑接口**：`notifyChange` 等编辑接口仍可用，对扁平化文档重新布局是幂等的，只是不再有约束行为；被剥离 id 的节点用 `findNode` 找不到，宿主不按 id 查找节点。

验收标准：同一台机器、同一套字体下，扁平化前后逐像素一致（§10 T1）。

**字体不一致时的行为**：自动布局按导出机字体的度量求解。运行时字体不同时，文本在自身的位置、文本框或适配宽高内重新排版，周围已扁平化的布局不会跟着调整，可能溢出或错位。旧 PAG 的文本有同样的依赖。

### 4.3 文本与字体

**统一规则：所有文本运行时排版（与旧 PAG 一致）**

- 不再区分静态文本和动态文本。每个 Text、TextBox 都保留完整的排版属性（文本内容、字体族/样式、字号、字间距、粗/斜体、`textAnchor`、TextBox 的排版参数及 §6.5 新增字段），不存预排版字形。
- 加载时在 `applyLayout()` 中用宿主注册的字体逐个排版一次；之后被 DataBind 或动画修改时由 `TextHolder` 重排。
- 字体文件不嵌入。宿主在使用前通过全局字体注册接口注入字体（对应旧 `PAGFont::RegisterFont`、`SetFallbackFontNames`），新运行时底层接入 PAGX 的 `FontConfig`。
- 宿主可通过 `PAGXDocument::getRequiredFonts()` 查询文档需要的字体，`pagx info` 列出同样的清单。

**字体嵌入场景：原样保留作者嵌入的字形**

作者用 `pagx embed` 处理过的 PAGX 文件里，有 Font（子集字形：矢量轮廓或位图）和 Text 下的 GlyphRun。

- **存储**：Font 与 GlyphRun 原样写入。矢量字形用 `WritePath`；位图字形（Emoji 等）原样存入 ImageTable，不做缩放、裁边和重新编码，因为字形的 `offset/advance` 依赖原图尺寸。该 Text 的排版属性照常保留。
- **混合情况**：同一个 TextBox 里只要有一个子 Text 没有 GlyphRun，整组文本都走运行时排版（`AllHaveEmbeddedGlyphRuns`，`src/pagx/TextLayout.cpp:1700`），导出时警告。
- **渲染**：一组文本全部带 GlyphRun 时，现有运行时直接使用嵌入字形，不排版、不依赖字体（`TextLayout::Layout` 的 `useEmbeddedGlyphRuns`，`src/pagx/TextLayout.cpp:1737`），各平台显示一致。
- **Font 的 `file` 属性**（外部字体文件路径）只在导出期用于注册字体，不写入二进制。
- **嵌入文本被修改时回退到注册字体（已确认）**：嵌入的只是字形子集，没有字符映射，无法显示新文字。
  - 现状（代码核实）：`TextHolder::flush` 调用 `TextLayout::Layout` 时沿用默认的 `useEmbeddedGlyphRuns = true`，嵌入文本仍走嵌入分支、不产生排版结果，修改被静默忽略（`src/renderer/TextHolder.cpp:168`）。XML 路径同样有这个问题。
  - 修复：某个 TextHolder 的文本类通道被写入后，改用注册字体排版（传 `useEmbeddedGlyphRuns = false`）。首帧仍显示嵌入字形，修改后显示注册字体的排版结果。
  - 导出时，对“带嵌入字形、且文本类属性被 DataBind 或动画驱动”的 Text 给出警告：修改后依赖宿主注册的字体。

**导出期字体（已确认）**

自动布局在导出时求解，文本测量必须用与运行时相同的字体，否则布局按错误的度量求解。

- `pagx export --format pag` 按 `pagx embed` 的方式注册字体：自动注册文档中带 `file` 的 Font，并接受 `--fallback <path|name>`（与 `pagx embed`、`pagx render` 同名同义）。
- 找不到字体的文本写入 warnings，列出字体名和所在节点。

### 4.4 可编辑内容：ViewModel + DataBind

- **规则**：文档带 ViewModel 才可编辑，没有 ViewModel 就不可编辑。可编辑的范围就是 ViewModel 属性经 DataBind 连接到的通道。
- **对外接口**：`PAGScene::viewModel()`（`include/pagx/PAGScene.h:128`）获取 ViewModel，修改属性值即驱动渲染。
  - 文本：ViewModel 字符串/数值/颜色/布尔属性绑定到 Text、TextBox、Fill、Stroke 的通道；
  - 图片：ViewModel 图片属性绑定到 `ImagePattern.image`，运行时写入器已存在（`src/renderer/LayerBuilder.cpp:2015`）；
  - 嵌套 ViewModel 已支持（`src/pagx/DataContext.cpp:75`），可以用 `$vm.text0.fontSize` 这样的路径组织一组文本属性。
- **二进制保留的内容**：ViewModel、ViewModelProperty（名称、类型、默认值、约束、转换器、嵌套引用）、DataBind、DataConverter 全部保留。
  ViewModel 属性名是对外契约，必须保留；DataBind 的目标按字符串原样存储（§5.2）。
- 旧 PAG 支持的全部文本属性都要能被绑定，其中一部分需要扩展 PAGX 通道（§6.5）。

### 4.5 图片处理（沿用旧 PAG，默认参数）

旧导出器流程（`exporter/src/export/PAGExport.cpp:458-480`、`exporter/src/export/data/ImageBytes.cpp:124-200`）：

1. 计算最大显示缩放（含动画）；
2. `factor = min(1, 最大显示缩放 × 2.0)`，只缩小不放大；
3. 裁掉透明边并记录偏移；
4. 以质量 80 编码为 WebP；
5. 记录逻辑尺寸与 `scaleFactor`。

新格式照此执行，参数固定为默认值，CLI 不提供调节。PAGX 特有的适配：

- **最大显示缩放**：累计 Image 每一处 ImagePattern 引用的祖先变换、`ImagePattern.matrix`、`scaleMode` 适配缩放，取所有引用与关键帧的最大值。
- **透明边裁剪**：只在所有引用都是 `decal` 平铺时执行。
- **运行时**：Image 节点增加逻辑尺寸、`scaleFactor`、内容偏移（编码细节，不进 PAGX 规范）；LayerBuilder 按逻辑尺寸适配，以 `1/scaleFactor` 缩放并平移绘制。
- **被 ViewModel 绑定的图片**：默认图片同样处理；宿主替换的新图片按自身尺寸，由 `scaleMode` 适配。
- **不重复压缩**：图片已是 WebP、且不需要缩小和裁边时，字节原样写入，不重新编码（避免有损叠加，旧 PAG 迁移的图片走这条路径）。
- **去重**：内容哈希 + 字节比较，相同的图片只存一份；ImageTable 序号按首次引用顺序分配。
- **异常**：外部图片找不到或解码失败时写入 warnings，该 Image 不写数据，引用它的填充按运行时现有逻辑不绘制；动图只取首帧（PAGX 的 Image 是静态图片）。
- **外部文件全部内嵌**：解码后的文档 `getExternalFilePaths()` 为空，宿主不需要再调用 `loadFileData`。

### 4.6 剥离清单

| 类别 | 删除 | 保留 |
|---|---|---|
| 元数据 | 其余节点的 `data-*`、`sourceLine`、导入指令、外部文件路径（子文档的原始路径只作为诊断名保留，§4.8） | 图层名；ViewModelProperty、文档根、ImagePattern 的 `data-*`（C27） |
| id | 其余节点的 id（资源、矢量元素等，它们的 `@id` 引用在导入时已解析为指针，编码为序号） | Layer 的 id（`PAGLayer::id()` 是公开接口）；被运行时按字符串查找的节点 id（动画目标、DataBind 目标）；Animation、StateMachine 的 id |
| 对外名称 | — | ViewModel 属性名、动画 id、状态机 id、状态机输入名/状态名 |
| 布局 | 约束、百分比尺寸、容器的 padding（TextBox 除外）、layout/gap/flex/alignment/arrangement/includeInLayout；`clipToBounds` 改写为 `scrollRect` | 扁平化后的显式位置与尺寸；TextBox 的宽高与 padding（排版输入） |
| 文本 | Font 的 `file` 外部路径 | 全部 Text/TextBox 的排版属性；作者已嵌入的 Font 与 GlyphRun |
| 节点 | 不被动画/DataBind 指向、且不作为遮罩的不可见图层；不被指向的空 Group；按渲染器作用域规则（含外层绘制器）确定不会被任何绘制器使用的几何元素 | 其余全部 |
| 资源 | 未被引用的资源；内容相同的图片、路径、颜色源合并 | — |
| 时间线 | 随未被使用的 Composition 一起删除的 Animation | 文档与 Composition 作用域内的全部 Animation、StateMachine（Composition 作用域的动画会自动播放） |

### 4.7 数值精度（沿用旧 PAG）

| 数据 | 编码 |
|---|---|
| 路径点（Path、遮罩路径、TextPath 路径、字形轮廓） | 沿用旧 `WritePath` 的编码思路（退化记录 + 点列表按精度打包）；记录位宽由 3 位扩为 4 位以加入 Quad（旧记录只有 8 种，`src/codec/DataTypes.cpp:27`）。不把 Quad 升阶为 Cubic，因为逐点通道按点序号寻址。量化步长见下文 |
| 点列表（GlyphRun 位置/偏移/锚点） | `writeFloatList(..., SPATIAL_PRECISION)` |
| 关键帧贝塞尔控制点 | `BEZIER_PRECISION = 0.005` |
| 渐变色标位置 | `GRADIENT_PRECISION = 0.00002` |
| 其它标量 | float32 |
| 透明度（Layer/Fill 等的 alpha 字段） | u8 |
| 颜色 | sRGB 写 RGBA 各 8 位；广色域（P3）或超出 [0,1] 时写颜色空间 + float（PAGX 特有能力，见 §5.6） |
| 帧时间 | `WriteTime` |

**量化步长按显示尺寸计算**：旧 PAG 的 0.05 是 AE 合成坐标，基本等于显示像素。PAGX 的几何可能写在很小的局部坐标里再整体放大（例如 SVG 导入时用归一化的 viewBox），固定 0.05 会在放大后产生肉眼可见的误差。规则：

- 路径与点列表的步长 = 0.05 ÷ 该节点的最大显示缩放（与 §4.5 图片的最大显示缩放用同一套计算，含所有引用实例与关键帧），取不大于结果的 2 的负整数次幂。
- 步长指数写在路径记录中，解码器按记录还原。
- 显示缩放不大于 1 时步长就是 0.05，与旧 PAG 相同；字形轮廓按字体单位，固定 0.05。
- 仓库 317 个样本中的 305 条路径都不需要更细的步长，这条规则主要防御导入内容。

---

### 4.8 外部子 PAGX

PAGX 的 Layer 可以用 `composition="sub.pagx"` 引用外部 PAGX 文件（规范 §8.3）。

**现状（代码核实）**

- XML 运行时由宿主驱动加载：`getExternalFilePaths()` 返回待加载路径，宿主读取后调用 `loadFileData(path, data)`（`src/pagx/PAGXDocument.cpp:115-148`）。
- 每个引用该路径的 Layer 都各自解析出一个独立的 `PAGXDocument`（`layer->externalDoc`），并在父文档里生成一个包装 Composition，复制子文档的宽高、图层和根动画。子文档的根动画因此成为 Composition 作用域的动画，自动播放。
- 子文档用自己的宽高独立布局，排版使用宿主的字体配置（`src/pagx/PAGXDocument.cpp:255-266`）；环形引用会被检测并报错。
- 目前只有微信端实现了外部文件加载；`pagx` CLI 和 PAGViewer 都不加载外部子 PAGX，引用它的图层显示为空。

**导出**

- **路径解析**：相对路径基于“写出该引用的文件”所在目录，与 `import`、Font `file` 的规则一致（M0 写入规范）。URL（含 `://`）不支持，给出 warning。
- **递归加载**：子文档里再引用的子文档同样加载；按“引用链”检测环，发现环即报错。
- **去重**：按规范化后的绝对路径去重，同一文件只存一份 SubDocument。
- **逐个处理**：每个子文档单独执行布局求解、扁平化、剥离；图片进入全文件共用的 ImageTable 并跨文档去重；字符串表全文件共用。
- **图片缩放**：子文档中图片的最大显示缩放要乘上每一处引用实例的父级变换，取最大值。
- **字体**：子文档中带 `file` 的 Font 同样注册，用于导出期测量。
- **缺失**：子文件不存在或解析失败时给出 warning，该图层按 XML 运行时“未加载”的状态输出（显示为空）。
- **分期**：一期支持静态子文档；子文档中的动画、状态机、ViewModel 随二期支持。
- **展开上限**：每个引用独立解码，多级共享会成倍展开（例如 10 层、每层引用 10 次，就是 10¹⁰ 个实例）。导出时统计展开后的实例总数和节点总数，超过上限（初定 1024 个实例、100 万个节点，M0 用样本校准）就报错；解码器同样检查，超限拒绝加载。

**编码与加载**

- SubDocumentTable 中每个 SubDocument 是一个完整的文档块（自己的节点序号空间，§5.2），并带一个诊断名（原始相对路径），不用于加载。
- Layer 记录 SubDocument 序号。
- 解码时，每个引用 Layer 各自解码一份 PAGXDocument，与 XML 运行时“每个 Layer 一个独立子文档”的语义一致（编辑和 ViewModel 实例互不影响）。
- 挂接复用 `LoadExternalComposition` 的逻辑（改为接受已解码的文档），生成同样的包装 Composition，保证结构与 XML 路径一致。
- 文件自包含：解码后 `getExternalFilePaths()` 为空，宿主不需要再加载任何外部文件。

**工具链配套**

新增一个共用的“本地外部文件加载”函数：按上面的路径规则递归读取子 PAGX 和图片，再调用 `loadFileData`。`pagx export/render/verify` 和 PAGViewer 都用它，保证对比页第 1 列（PAGX 直接渲染）也能显示子文档内容。

## 5. 文件格式

### 5.1 主体结构

```
Body := FileAttributes  StringTable  ResourceBlock  DocumentBlock  [SubDocumentTable]  ImageTable  End
```

- **StringTable**：去重后的字符串，包括图层名、ViewModel 属性名、动画 id、通道名、文本内容与字体名、保留的 `data-*` 键值等；主文档与子文档共用。
- **SubDocumentTable**：外部子 PAGX 打包后的文档块（§4.8）。
- **ImageTable**：图片字节（WebP；位图字形保持原格式）、逻辑尺寸、`scaleFactor`、内容偏移；放在末尾，主文档与子文档共用。每个条目带“媒体类型”字段，一期只有静态图片，为后续视频、位图序列预留（C28）。
- **图片零拷贝**：`pagx::Data` 目前只有 `MakeWithCopy` 和 `MakeAdopt`（`include/pagx/types/Data.h`），需要新增“持有父 Data + 偏移 + 长度”的切片构造，Image 节点引用文件缓冲区的切片，渲染时才解码。代价是文档存活期间整个文件缓冲常驻内存，与旧 PAG 的做法一致；一期实测内存后确认。

### 5.2 引用与校验

**节点序号**

- 每个文档（根文档和每个 SubDocument）各有独立的序号空间，与运行时按文档查找目标一致。
- 序号在剥离之后、按编码时的先序遍历顺序分配，解码时按读取顺序重建“序号 → 节点”表，最后统一回填。

| 引用类型 | 编码 |
|---|---|
| 资源引用（颜色源、路径、图片、字体、内部 Composition） | varint，0 表示内联子 Tag，k 表示资源表第 k−1 项 |
| 外部 Composition（子 PAGX） | SubDocument 序号；同一外部文件只存一份，每个引用 Layer 各自解码一份（§4.8） |
| 图层引用（遮罩） | 同一文档内的节点序号 + 1 |
| 动画目标、DataBind 目标、State 引用的动画、Layer 挂载的时间线 | 字符串原样存储（字符串表序号），见下文 |
| 关键帧中的图片值（`ImageRef`） | 图片资源序号 |

**按需保留原 id，不生成合成 id**

- 运行时有两类引用：
  - 导入时已解析成指针的（填充的颜色源、遮罩、Composition、图片等）：编码为序号或资源序号，不需要 id；
  - 运行时按字符串查找的：`AnimationObject.target`（`src/pagx/PAGAnimation.cpp:72`）、`DataBind.target`（`src/pagx/DataBindRuntime.cpp:113`）、`State.animationId`（`src/pagx/PAGStateMachine.cpp:319`）、Layer 上的 `AnimationTimeline/StateMachineTimeline`（`src/pagx/runtime/PAGComposition.cpp:143`），以及公开接口 `getAnimation(id)`、`getStateMachineTimeline(id)`、`playTimeline(id)`。
- 第二类引用按字符串原样存储（字符串表序号），被引用节点保留原 id；Layer 的 id 也保留，因为 `PAGLayer::id()` 是公开接口（`include/pagx/PAGLayer.h:72`）。其余 id 剥离。
- 不选“生成合成 id”的原因：要额外实现改写与防冲突（合成 id 不能和必须保留的动画、状态机 id 撞名），日志和 `pagx info` 里看到的是无意义的名字，而节省的体积可以忽略。
- 不选“全部保留”的原因：资源和矢量元素的 id 在运行时完全用不到。
- 体积参考：仓库 317 个 PAGX 样本共 297 个 id、平均 7.5 字节，其中 Layer 占一半；全部 id 只占 XML 结构体积的约 0.6%。
- 作用域检查（目标必须在同一绑定作用域内）和重复 id 的处理都由运行时完成，与 XML 行为一致。

**内联与共享**：被两处及以上引用的颜色源、路径、图片、字体、Composition 写进资源表，只被一处引用的作为内联子 Tag。规则只看引用次数，与原文件是否写了 id 无关，结果确定。

**引用不跨文档**：资源引用、遮罩、动画目标、DataBind 目标都只在所在文档内解析；跨文档只有 Layer → SubDocument 一种引用（§4.8），与 XML 运行时按文档绑定的作用域一致。

**保持声明顺序**：同一通道上的多个 DataBind、多个 Animation、各列表里的节点都按原声明顺序编码，运行时语义依赖这个顺序。

**校验**：沿用旧 `Codec::InstallReferences`、`VerifyCompositionGraph` 的思路，用 `nodeType()` 检查引用类型；检查引用环、遮罩环、外部合成环；嵌套深度（图层、Group、Composition、StateRegion）上限 128。

### 5.3 关键帧

沿用旧格式的列式编码（`AttributeHelper.h` 的 `WriteKeyframes/WriteTimeAndValue/WriteTimeEase`）：

- 关键帧数
- 插值类型（每个 2 位）
- 时间列表
- 值列表（按值类型）
- 贝塞尔控制点（`BEZIER_PRECISION`）

读写函数按 PAGX 的点式关键帧实现，编码布局与旧格式一致：

- 插值类型 None/Linear/Bezier/Hold 与旧格式相同，2 位即可（`include/pagx/nodes/Keyframe.h`）；PAGX 每个关键帧只有一组一维贝塞尔控制点，只在 Bezier 关键帧上写。
- 值类型（`ChannelValueType`）：Float、Int、Bool、String、Color、Matrix、ImageRef 按类型编码；PAGImage 只在运行时出现，不会写入文件；新增 `path`（§6.1）。
- 贝塞尔控制点描述的是时间缓动曲线（时间 → 进度），与值类型无关；颜色、矩阵等多分量值按同一进度逐分量插值。
- 通道名（`Channel.name`）按字符串原样存入字符串表，包括 Path 的逐点通道 `point{i}.x/y` 这类动态名称，不做特殊编码。
- 关键帧时间必须非递减，否则解码时丢弃该通道并告警。

### 5.4 静态矢量与矢量动画

新 PAG 与 PAGX 一样，同一种格式既能表达静态矢量，也能表达矢量动画，二者可以在同一个文件里混合。

**内容类型**

| 类型 | PAGX 表达 | 二进制 | 运行时 |
|---|---|---|---|
| 静态矢量 | 没有 Animation / StateMachine | 只有节点 Tag，不含任何时间线 Tag | 构建一次渲染树；没有默认时间线；首帧之后 `hasContentChanged()` 为 false，宿主不需要逐帧渲染 |
| 关键帧动画 | `<Animation>`（`duration`、`frameRate`、`loop`）→ Object → Channel → Key | AnimationBlock / AnimationObject / Channel，关键帧按 §5.3 编码 | 顶层动画通过 `getAnimation(id)`、`getDefaultTimeline()` 播放；嵌套合成内自动播放的动画由 `PAGScene::advanceAndApply` 推进 |
| 状态机 | `<StateMachine>`：输入、状态、转换、条件 | StateMachine 系列 Tag | 宿主通过 `getStateMachineTimeline(id)` 设置输入驱动 |
| 嵌套合成 | Layer 引用 Composition，`<Timelines>` 挂载动画或状态机 | AnimationTimeline / StateMachineTimeline；时间偏移与倍速见 §6.3 | 各实例独立推进 |
| 数据驱动 | ViewModel + DataBind，静态文件也可以带 | ViewModel 系列 Tag | 修改属性即刷新，静态与动画文件都适用 |

**设计要点**

- **动画与节点分离存储**：PAGX 的节点只有静态值，动画放在独立的 Animation 里按目标节点引用。二进制照此存储（动画目标按字符串存储），静态内容不为“可能被动画”付出任何字节。旧 PAG 则在属性块里为每个可动画属性记录“是否动画”标志位。
- **静态文件零时间线开销**：不写 Animation、StateMachine、Timeline 相关 Tag；加载后不创建时间线，不需要逐帧推进。
- **动画不受布局扁平化影响**：动画只作用于布局结果之上（§4.2）。
- **发布流水线兼顾动画**：剥离时保留被动画指向的节点（包括当前不可见的图层）；图片最大显示缩放包含全部关键帧（§4.5）；保留动画 id、状态机 id、输入名、状态名。
- **默认时间线**：所有播放端统一使用 `PAGScene::getDefaultTimeline()`，即文档根作用域中按声明顺序的第一个 Animation 或 StateMachine（`src/pagx/PAGScene.cpp:444`）。
  - 不只取“第一个 Animation”：HTML 导入有多段 CSS 动画时，会生成一个多区域并行的 StateMachine 并放在最前面，专门让它成为默认时间线（`src/pagx/html/importer/HTMLParserContext.cpp:426-463`）。只取第一个 Animation 会只播其中一段。
  - 现状不一致，需要统一：`pagx/wechat` 用默认时间线；PAGViewer 只在第一个是 Animation 时才播放（`viewer/src/rendering/pagx/PAGXViewModel.cpp:546-555`），遇到上面的 StateMachine 什么都不播；`pagx render` 把所有根动画都定位到同一时间（`src/cli/CommandRender.cpp:285`），忽略 StateMachine。
  - 规范 §8.1.1 说根作用域的动画“自动作用”，但运行时不会自动推进根作用域的动画（`include/pagx/PAGScene.h:171`）。M0 在规范里写明“默认时间线 = 根作用域第一个 Animation 或 StateMachine”，不新增文件字段。
- **播放语义**：
  - 默认时间线是 Animation：时长、帧率、循环模式取自它，进度可读写（`setCurrentTime`）。
  - 默认时间线是 StateMachine：按真实时间推进；没有固定时长，时长报 0，进度接口不生效。需要定位到某一时刻时（测试、对比页），从初始状态推进到该时刻。
  - 没有时间线：作为静态画面显示，播放、进度接口不报错，也不重复渲染。
- **无效时长**：`duration ≤ 0` 或 `frameRate ≤ 0` 的 Animation 在运行时时长为 0（`src/pagx/PAGAnimation.cpp:34`），导出时告警并原样保留。
- **没有对象的 Animation**：二进制保留（C20 需要）。现有 `PAGXExporter` 导出 XML 时会省略它（`src/pagx/PAGXExporter.cpp:1927`），需改为“有时长就保留”，否则迁移结果导出成 XML 检查时会丢失时长。
- **旧 PAG 迁移**：旧文件都有时长。迁移时为根合成生成同时长、同帧率的根 Animation 并放在最前面，成为默认时间线，即使原文件没有任何关键帧，也保留旧播放接口的时长与进度语义（三期核实没有关键帧的 Animation 在运行时的行为）。嵌套合成通过时间锁定挂载跟随这个时间（§6.3）。

Animation、AnimationObject、Channel、StateMachine 系列、AnimationTimeline、StateMachineTimeline、ViewModel、ViewModelProperty、DataBind、DataConverter 各自对应属性 Tag 或 Block（附录 A）。

### 5.5 健壮性

- 沿用旧格式：计数上限检查（如 `MAX_KEYFRAMES`）、引用校验、未知 Tag 跳过。
- 未知枚举值取默认值并告警；通道值类型与通道定义不符时丢弃该通道并告警。
- 空文档、零尺寸画布都是合法文件。
- 资源上限：单个字符串、单个路径的点数、关键帧数、图片像素尺寸、节点总数、子文档展开实例数都有上限，超限时拒绝加载并给出错误；具体数值在 M0 规范中定义。
- 文件主体长度沿用 u32，单个文件上限 4 GB：编码器在写文件头前算出主体长度，超出即报错；解码器读文件头后检查主体长度不超过实际数据长度，否则按截断文件报错。
- 新增 fuzz 测试。

### 5.6 基础类型编码约定（字节级细节在 M0 规范中定义）

| 类型 | 约定 |
|---|---|
| 可选值（NaN 表示未设置） | 用存在位表达，判断“是否默认值”时把 NaN 视为相等；Inf 等非有限值导出时报错 |
| 颜色 | 1 字节类型：sRGB 时写 RGBA 各 8 位（沿用旧精度）；P3 或超出 [0,1] 时写颜色空间 + 4 个 float32 |
| 2D 矩阵 / 3D 矩阵 | 6 / 16 个 float32；单位矩阵靠存在位省略；`matrix3D/preserve3D` 原样保留，不参与扁平化 |
| 字符串 | UTF-8，写 StringTable 序号 |
| GlyphRun | 字形 ID、位置、偏移、锚点、缩放、旋转、斜切各自成列，列顺序在 M0 规范中固定 |
| 帧时间 | 沿用 `WriteTime` |


---

## 6. PAGX 规范扩展（在 PAGX 中完整支持）

每项都要落到：中英文规范、`pagx.xsd`、XML 导入导出、运行时、通道表、二进制 Tag、`pagx verify`；静态导出器（SVG/HTML/PPT）按当前值处理。

**旧 PAG 参考样本**（用 `pag::File` 扫描仓库 282 个旧 pag 得到，可在 PAGViewer 中查看效果，也作为三期的迁移对比样本）：

| 扩展 | 样本 |
|---|---|
| 6.1 Path 数据动画 | `resources/compare/guide_hand_drag.pag`（图层 shape Outlines）、`resources/apitest/shape_transform_round_corner.pag`；遮罩路径动画：`resources/apitest/ZC2.pag`（clip_1、clip_2）、`resources/compare/vt_chaokulama.pag` |
| 6.2 Layer 变换动画 | `resources/apitest/ImageFillRule.pag`（wenzi.png 旋转+缩放）、`resources/compare/hb_rixiyixia.pag`（缩放）、`resources/compare/feature_all.pag`（预合成图层缩放）；仓库中没有锚点动画样本 |
| 6.3 预合成时间偏移/倍速 | 偏移：`resources/apitest/TrackmatteImageLayer.pag`（合成 1、合成 3）、`resources/compare/hb_xiaoqingxin.pag`；倍速：`resources/compare/hb_lianaishouji.pag`（1.3 倍）、`resources/compare/chaoli.pag`（1.2 倍并带偏移） |
| 6.4 反相遮罩 | 反相 Alpha 轨道蒙版：`resources/apitest/complex_test.pag`、`resources/compare/data_vector.pag`；反相遮罩：`resources/filter/FeatherMask.pag`；仓库中没有反相亮度蒙版样本 |
| 6.5 文本属性 | `resources/apitest/test_VerticalText.pag`（描边、填充在描边之上、两端对齐、框文本、竖排、基线偏移、背景色）、`resources/apitest/test_TextDirection.pag`（横竖排、文本动画）、`resources/apitest/TEXT04.pag`（居中、框文本、描边）、`resources/apitest/test_font.pag`（基线偏移、粗/斜体） |

### 6.1 Path 数据动画

- **规范**：`ChannelValueType` 新增 `path`，`<Path>` 的 `data` 成为可动画通道。
- **现状**：Path 已有逐点浮点通道 `point{i}.x/y`（HTML 导入的 clip-path 变形在用，`src/renderer/LayerBuilder.cpp:248`），二进制必须支持这种动态通道名。新增的 `path` 值类型是更紧凑、通用的形式，二者并存。
- **插值**：命令序列相同时逐点插值，与旧 PAG `PathData::interpolate`（`src/base/PathData.cpp:135`）一致；不同时按 Hold 处理，并由 verify 告警。

### 6.2 Layer 变换动画

- **规范**：Layer 新增 `anchor/rotation/scale/skew/skewAxis`，语义与 Group 相同，`x/y` 作为位置。优先级：`matrix3D` > `matrix` > 上述属性。
- **通道**：新增对应通道，均可动画。

### 6.3 Composition 实例的时间锁定挂载（C26）

**问题**：嵌套合成的时间线现在按时间增量各自推进（`src/pagx/runtime/PAGComposition.cpp:69`），父时间线定位时子合成不跟随。旧 PAG 是单一时间轴，任意帧都能定位。

**规范**：

- 引用 Composition 的 Layer 新增时间锁定属性 `startTime`、`timeScale`；声明任一属性即进入“时间锁定”模式。
- 锁定模式下，该实例内全部时间线（Composition 作用域自动播放的动画，以及 `<Timelines>` 挂载的动画）不再自行推进，时间由父时钟计算：子时间 =（父时钟 − startTime）× timeScale。
- 父时钟的定义：
  - 在主文档中：文档的默认时间线（必须是 Animation）的当前时间；
  - 在另一个锁定实例中：该实例的子时间（可以逐级嵌套）。
- 默认时间线不是 Animation（是 StateMachine 或没有）时，锁定不生效，退回自行推进，并由 verify 告警。
- 子时间超出子合成时长、为负数、`timeScale` 为 0 或负数时的行为，与旧 PAG 的预合成对齐，在 M0 规范中逐项定义。
- 时间单位已定稿为**秒**（M0，避免依赖父子各自的帧率）；规范条文见 PAGX 规范 §8.3.3。

**运行时**：`PAGComposition` 增加锁定模式：`advance` 不推进锁定实例；`apply` 时按父时钟计算子时间并定位其时间线。父时钟被定位（`setCurrentTime`）后，下一次 `apply` 全部锁定的后代同步定位。

**分期**：规范、二进制字段在 M0 一次定全；运行时实现放在三期（旧 PAG 迁移依赖它），如果二期的动效 PAGX 需要，可以提前到二期。

### 6.4 反相遮罩

- **规范**：`MaskType` 新增 `alphaInverted/luminanceInverted/contourInverted`。
- **运行时**：tgfx 已支持（`LayerMaskType.h`），主要是规范与映射。

### 6.5 旧 PAG 文本属性的通道支持

旧 `TextDocument`（`include/pag/types.h:1448`）的全部字段都要能通过 ViewModel + DataBind 修改：

| 旧字段 | PAGX 通道 | 现状 |
|---|---|---|
| text、fontFamily、fontStyle、fontSize、fauxBold、fauxItalic | Text 同名通道（`TextHolder`） | 已有 |
| fillColor | Fill.color | 已有 |
| strokeColor、strokeWidth | Stroke.color、Stroke.width | 已有 |
| applyFill、applyStroke | Fill/Stroke 新增 `visible` 通道 | 新增 |
| strokeOverFill | 已定稿（M0）：不新增通道，用 Fill/Stroke 声明顺序表达（Fill 在 Stroke 之前 = stroke 在 fill 之上），迁移时按此排序 | 既有 |
| tracking | 已定稿（M0）：新增 Text.tracking 通道（1/1000 em，默认未设置），与 letterSpacing 独立并存，有效字距 = letterSpacing + tracking × fontSize / 1000；保证 tracking 与 fontSize 同时动画时精确 | 新增 |
| justification | TextBox.textAlign、Text.textAnchor 改为运行时可写 | 扩展 |
| leading | TextBox.lineHeight 运行时可写 | 扩展 |
| boxText、boxTextPos、boxTextSize | TextBox 位置/宽/高运行时可写 | 扩展 |
| direction | TextBox.writingMode 运行时可写 | 扩展 |
| baselineShift、firstBaseLine | 新增 Text 通道 | 新增 |
| backgroundColor、backgroundAlpha | 新增文本背景能力 | 新增 |

“运行时可写”的实现：`TextHolder` 目前在构建时快照一次 TextBox 排版参数（`TextLayoutParams`），需改为可由通道写入，修改后重排。

图片替换：`ImagePattern.image` 通道已有；旧 `ImageScaleModes` 映射为 `ImagePattern.scaleMode`。

### 6.6 视频与位图序列（三期，C28）

**旧格式现状**：

- **视频合成**：可以有多份 VideoSequence（不同分辨率、帧率），每份包含 H.264 帧数据（含关键帧标记）、Mp4 头、静态时间段，以及把 Alpha 通道并排编码时的位置 `alphaStartX/Y`（`include/pag/file.h:2275-2330`）。
- **位图序列合成**：每帧由若干带位置的 WebP 小图组成，关键帧为整图，其余帧只存变化区域（`include/pag/file.h:2145-2250`）。
- **运行时**：旧运行时用 `src/rendering/sequences/`（解复用、预读队列）和 `src/rendering/video/`（平台硬解与软解兜底）解码。

**设计纲要**（三期开工前展开为详细设计，M0 先在规范中占位）：

- **PAGX 规范**：新增序列资源（视频序列、位图序列），可以作为图片填充的来源。显示哪一帧由所在合成实例的时间决定，与时间锁定挂载（§6.3）共用同一套时间计算。
  - XML 中序列数据以外部文件引用，例如 `pagx import` 时把视频写成旁边的 .mp4 文件。
  - 二进制中原样存储帧数据，不重新编码。
- **存储**：ImageTable 条目的媒体类型扩展为视频流、位图序列帧（§5.1 已预留）；多分辨率序列都保留，运行时按显示缩放选择，与旧运行时一致。
- **运行时**：在 PAGX 运行时中复用旧的解码栈（平台硬解、软解兜底、预读队列、静态时间段跳过），不重写解码器。
- **难点**（三期设计中逐项解决）：与 `PAGScene` 的帧调度和预读的对接；渲染对比测试中视频解码结果的平台差异；Web 端的解码方式。

---

## 7. 旧 PAG → 新 PAG

### 7.1 路线

**内部流程**：旧 pag 先在内存中转换成 PAGXDocument，再走与 PAGX 完全相同的发布流水线。中间不会生成、也不会重新解析 PAGX XML 文本。

```
旧 .pag ─Codec::Decode─> pag::File ─旧版导入（src/pagx/pag/importer/）─> PAGXDocument
                                                                     ├─ PAGExporter（发布流水线 §4）─> 新 .pag   ← pagx export
                                                                     └─ PAGXExporter ─> .pagx               ← pagx import
```

- **两条命令共用同一个中间文档**：`pagx import` 输出的 XML 就是 `pagx export` 的输入，审查看到的就是实际转换结果。
- **为什么不经过 XML 文本**：PAGX 导出浮点数用 `%g`（6 位有效数字，`src/pagx/utils/StringParser.cpp:650`），经过文本会损失精度；直接在内存中传递，导出的数值与旧文件一致。
  因此“直接 export”与“先 import 再 export”的结果可能有极小的数值差异，T6 用渲染对比校验二者等价，不要求字节一致。
- **中间文档必须能完整写成 XML**：旧版导入只产生 PAGX 规范内的节点和属性，不使用二进制专用字段；规范还不支持的能力（§6、§7.4）先扩展规范再迁移。这样 `pagx import` 的输出总是完整、可渲染的 PAGX。
- **图片**：旧图片已是处理过的 WebP（带 `scaleFactor` 和透明边偏移 `anchorX/anchorY`，`include/pag/file.h:2119-2127`）。导入时字节原样放进 Image，缩放和偏移写进 ImagePattern 的矩阵，XML 中可见。导出时按 §4.5 计算出不需要再缩小，字节原样写入，不重新编码。
- **报告与严格模式**：两条命令都支持 `--report`（保真度报告）和 `--strict`（存在 Unsupported 即失败）。
- **保真度分级**：

| 等级 | 含义 | 是否可以发版 |
|---|---|---|
| Exact | 语义直映 | 可以 |
| Equivalent | 改写或逐帧采样，每个整数帧与旧运行时渲染一致（旧运行时按整数帧渲染） | 可以 |
| Unsupported | 无法转换 | 只允许 Marker（C35）和加密文件 |

不再设“近似”等级（C34）：原先评估为近似的能力（遮罩的变亮/变暗/差值模式与扩展、部分文本动画器等），都要通过 §7.4 的规范扩展做到 Exact 或 Equivalent。

### 7.2 可编辑内容 → ViewModel

可编辑范围按旧语义确定：`editableTexts/editableImages` 为空时全部文本图层、图片图层可编辑，否则按列表。

- **文本**：生成一个标准 ViewModel 结构 `PAGTextDocument`，属性与旧 `TextDocument` 字段同名（text、fontFamily、fontStyle、fontSize、fillColor、applyFill、
  strokeColor、strokeWidth、applyStroke、strokeOverFill、fauxBold、fauxItalic、tracking、justification、leading、baselineShift、firstBaseLine、
  boxTextPos、boxTextSize、direction、backgroundColor、backgroundAlpha）。根 ViewModel 为每个可编辑文本生成 `text<编号>` 属性（类型为上述嵌套 ViewModel），
  默认值取原文件的值，并为每个字段生成 DataBind（如 `$vm.text0.fontSize` → Text.fontSize）。
- **图片**：每个可编辑图片组生成 `image<编号>` 图片属性，默认值为原图，绑定到该组所有图层的 `ImagePattern.image`。
- **编号**：与旧 `editableTextIndex/editableImageIndex` 一致。业务从 `replaceText(i, …)` 迁移到“修改 `text<i>` 属性”时可一一对应。
- **文本动画**：原文本内容带关键帧动画时，ViewModel 默认值不能覆盖动画；需要“宿主赋值后才生效”的绑定语义，M0 核实 DataBind 初始化行为后定稿。

### 7.3 其它映射要点

| 旧特性 | 处理 | 等级 |
|---|---|---|
| 形状组、矩形、椭圆、多边形、路径、填充、描边、合并、裁剪、复制器、圆角 | 一一对应（MergePaths：Merge→append、Add→union、Subtract→difference、Intersect→intersect、ExcludeIntersections→xor；TrimPaths：Simultaneously→separate、Individually→continuous；`composite` 转为元素顺序） | Exact |
| 路径动画、遮罩路径动画 | §6.1 | Exact |
| 渐变 | alpha 色标与 color 色标合并重采样 | Equivalent |
| 一维缓动 / 多维属性 | 分段边界变为关键帧；多维拆 x/y | Exact |
| 运动路径 | 逐帧采样 + 关键帧精简 | Equivalent |
| 图层变换、父子链 | §6.2；父子链用嵌套容器表达，保持绘制顺序 | Exact |
| 入点/出点 | `visible` 的 Hold 关键帧 | Exact |
| 预合成 startTime/stretch | §6.3 | Exact |
| 纯色/图片/空图层 | Rectangle + Fill / ImagePattern / 消除 | Exact |
| 遮罩列表 | 全部模式、反相、羽化、不透明度、扩展及其动画：新增与旧遮罩语义一致的遮罩路径列表（§7.4） | Exact |
| 轨道蒙版（含反相） | §6.4 | Exact |
| 文本、文本框、路径文本 | Text / TextBox / TextPath；可编辑文本按 §7.2 | Exact |
| 文本动画器 + 范围选择器 + 抖动选择器 | TextModifier + RangeSelector，缺少的能力按 §7.4 扩展 | Exact |
| 投影样式 | DropShadowStyle，换算复用旧 `DropShadowFilter.cpp` | Exact |
| 快速模糊 | BlurFilter | Exact |
| 亮度对比度、色相饱和度（静态） | ColorMatrixFilter | Equivalent |

### 7.4 旧 PAG 全量能力清单（C34）

**判定规则**：

- 以旧运行时实际的渲染结果和对外接口为准。旧格式里有、但旧运行时不使用的数据不迁移，并在本表记录原因。例如图层上的时间重映射：旧运行时只使用图片填充规则里的那一份（`src/rendering/layers/PAGImageLayer.cpp:140`）。
- 每项能力都要落到 PAGX 规范、XML 导入导出、运行时（必要时 tgfx）、二进制、迁移转换和测试样本。
- 三期开工前，把本表展开为逐项的详细设计；旧运行时的效果实现（`src/rendering/filters/` 等）作为移植参考。

**旧样本特性分布**（用 `pag::File` 扫描仓库可解码的 166 个文件；仓库样本以功能测试为主，三期开工前用业务素材复核）：视频合成 35、位图序列合成 3、图层混合模式 34、运动路径 24、Marker 24、投影样式 13、运动模糊 13、音频 7、渐变叠加/描边/外发光样式 5/1/1、各类特效各 1–7、3D 与相机 0、图层时间重映射（旧运行时不使用）164。

| 类别 | 旧能力（TagCode） | 新格式中的处理 | 需要新增的能力 |
|---|---|---|---|
| 合成与图层 | 矢量合成、预合成（2、3、12）；纯色、空、图片、文本、形状图层；图层属性 V1–V3/Extra/ExtraV2（6、52、62、63、88）；入出点、父子链、倍速与起始时间、混合模式、激活状态 | Composition、Layer、嵌套容器；入出点为 visible 的 Hold 关键帧；时间锁定挂载 | §6.2 图层变换动画、§6.3 时间锁定挂载；旧混合模式 Add 映射为 plusLighter |
| 变换 | Transform2D（13）、运动路径、自动朝向 | Layer/Group 变换；运动路径与自动朝向逐帧采样 | §6.2 |
| 3D | Transform3D（90）、相机图层（91） | 逐帧计算出 matrix3D 关键帧 | matrix3D 动画通道；相机投影与景深按旧运行时计算方式对齐 |
| 运动模糊 | 图层 motionBlur（63、88） | 图层属性 | PAGX 运动模糊属性与 tgfx 实现。旧实现按“当前帧与前一帧的图层变换”计算（`src/rendering/filters/MotionBlurFilter.cpp:90`），没有跨帧状态，可以逐帧对比；新实现需要能取到前一帧的变换 |
| 遮罩与蒙版 | MaskBlock/V2（14、84）：全部模式、反相、羽化、不透明度、扩展、路径动画；轨道蒙版四种（含反相） | 遮罩路径列表；蒙版为 mask + maskType | 与旧遮罩语义一致的遮罩路径列表；§6.1 路径动画；§6.4 反相蒙版 |
| 形状 | 15–27 全部 | 一一对应 | 渐变的 alpha 色标与颜色色标分离：先验证合并重采样能否做到整数帧一致，做不到则扩展渐变 |
| 文本 | TextSource V1–V3（8、64、68）、TextMoreOption（10）、TextPathOption（69）、文本动画器与全部属性（70、72–79）、范围选择器（71）、抖动选择器（80）、字体表（1） | Text、TextBox、TextPath、TextModifier、RangeSelector；字体名存于 Text | §6.5 文本属性；抖动选择器、追踪类型、锚点分组等 PAGX 尚无的文本动画能力 |
| 图层样式 | 投影 V1/V2（29、65）、描边（92）、渐变叠加（85）、外发光（93） | 投影：角度 + 距离换算为偏移，大小换算为模糊；其余为新增样式 | 描边、渐变叠加、外发光样式；投影的扩展（spread）与混合模式：旧投影有 `spread`、`blendMode`（`include/pag/file.h:966`），PAGX 的 DropShadowStyle 没有（`include/pagx/nodes/DropShadowStyle.h`） |
| 特效 | 动态拼贴（56）、色阶（57）、边角定位（58）、鼓起（59）、快速模糊（60）、辉光（61）、置换图（66）、径向模糊（81）、马赛克（82）、亮度对比度（86）、色相饱和度（87），以及它们的参数动画 | 快速模糊 → BlurFilter；亮度对比度、色相饱和度 → ColorMatrixFilter | 已有对应：快速模糊；亮度对比度、色相饱和度的静态值。需新增 PAGX 滤镜与 tgfx 实现：动态拼贴、色阶、边角定位、鼓起、辉光、置换图、径向模糊、马赛克。需新增动画通道：各滤镜参数，含颜色矩阵。旧实现在 `src/rendering/filters/`，可作移植参考 |
| 图片 | 图片表与 ImageBytes V1–V3（4、47–49）、图片引用（11）、图片填充规则 V1/V2（54、67）、缩放模式（94）、可编辑索引（83） | Image + ImagePattern；可编辑 → ViewModel（§7.2） | 图片填充规则中的时间重映射：图片图层被替换为非静态内容（视频等）时，用它把图层时间映射到内容时间（`PAGImageLayer::getContentTimeRemap`），需要在 ViewModel 图片属性被替换为动态内容时提供等价能力 |
| 序列 | 位图序列合成（45、46）、视频合成（50、51）、Mp4 头（33） | §6.6 | §6.6 |
| 时间 | TimeStretchMode（32）与 `setDuration` | 文档级时间伸缩 | 时间伸缩模式（缩放、重复、反向重复）及接口 |
| 音频 | AudioBytes（55） | 原样携带 | 音频数据与起始时间的存储和读取接口 |
| 运行时提示 | CachePolicy（30）、Performance（28） | 不影响渲染结果；三期评估新运行时是否需要对应的缓存开关 | 视评估结果 |
| 元数据 | MarkerList（53） | 跳过并记录（C35） | — |
| 文件属性 | FileAttributes（31） | 保留生成工具等信息 | — |
| 加密 | EncryptedData（89） | 开源旧 SDK 同样无法解码（`version == 3` 被拒绝），不在迁移范围 | — |

### 7.5 报告与校验

- 输出 `report.json`：特性清单、等级统计、Unsupported 明细、体积对比、ViewModel 属性清单（旧编号 → 新属性名）。
- 校验：旧 `PAGPlayer` 与新 `PAGScene` 渲染对比；对可编辑内容，用旧 `replaceText/replaceImage` 与新 ViewModel 赋值做同等操作后再对比。
- `--strict`：存在 Unsupported 即失败。

---

## 8. SDK 与工具

### 8.1 SDK 核心

- 格式识别、v4 解码、tagLevel 与 `MaxSupportedTagLevel`、明确错误码。
- 全局字体注册接口，对应旧 `PAGFont`，底层接入 `FontConfig`。

### 8.2 平台接口

- **播放统一**：播放视图（PAGView、PAGImageView 等）的“按路径/数据播放、进度、循环、暂停”接口按文件头自动选择内核。新 pag 与 pagx 一律按 `getDefaultTimeline()` 播放，纯静态文件作为静态画面显示（§5.4）。
- **对象模型分离**：
  - 旧文件：使用 `PAGFile/PAGLayer`；`PAGFile.Load` 遇到新格式返回明确错误码；
  - 新文件：使用新运行时对象，包括 ViewModel（属性读写与观察）、动画/状态机控制。
- **逐平台接入**：Android、iOS/macOS、OHOS、Web、C API。

### 8.3 PAGViewer

- 改造：
  - 由按扩展名分流（`viewer/src/PAGViewer.cpp:42`）改为按文件头分流：旧 pag → `rendering/pag`；新 pag 与 pagx → `rendering/pagx`；
  - 新 pag 显示文件信息面板（版本、tagLevel、各区块体积、ViewModel 属性列表、需要的字体），可在面板中修改 ViewModel 属性预览效果；
  - 改用 `getDefaultTimeline()` 选择播放内容（§5.4），默认时间线是 StateMachine 时进度条不可拖动。

### 8.4 CLI

沿用现有命令的含义：`import` 把其它格式转成 PAGX，`export` 把 PAGX 转成其它格式。旧 PAG 作为“其它格式”的一种，不新增命令。

| 命令 | 说明 |
|---|---|
| `pagx export --input a.pagx --format pag [--output a.pag] [--fallback <path\|name>] [--report r.json]` | PAGX → 新 pag。图片参数固定；`--fallback` 用于导出期布局测量（§4.3） |
| `pagx export --input old.pag --format pag [--output new.pag] [--strict] [--report r.json]` | 旧 pag → 新 pag，一步完成：内存中先转成 PAGXDocument，再走发布流水线（§7.1）。`--strict` 遇到不支持的特性即失败 |
| `pagx import --input old.pag [--output old.pagx] [--images external\|embed] [--strict] [--report r.json]` | 旧 pag → PAGX，用于审查和排查迁移结果；图片存储方式沿用现有 `--images` 选项 |
| `pagx info --input x.pag` | 版本、tagLevel、Tag 统计、各区块体积、ViewModel 属性、需要的字体 |
| `pagx render/verify` | 支持新 pag 输入 |

- 输入为新 pag（v4）时，`import` 和 `export` 都报错：不做新 pag 的逆向转换。
- 格式按文件头识别，不看扩展名（§3.6）。

### 8.5 旧接口对照（三期交付）

迁移后的文件由新运行时播放，业务需要把旧接口调用改成新接口。按 C34，旧接口的能力都要有等价实现（Marker 除外，C35）；三期开工前逐个方法产出对照表，标明“已有对应 / 需新增”及新接口形式。按旧接口类（`include/pag/pag.h`）整理的范围：

| 旧接口类 | 主要能力 | 新运行时 | 状态 |
|---|---|---|---|
| `PAGFile` | `Load`、`tagLevel`、`MaxSupportedTagLevel`；`numTexts/numImages/numVideos`、`getTextData`、`replaceText`、`replaceImage`、`replaceImageByName`、`getEditableIndices`、`getLayersByEditableIndex`；`timeStretchMode`、`setTimeStretchMode`、`setDuration`；`copyOriginal` | 加载与 tagLevel（§3）；可编辑内容 → ViewModel `text<i>/image<i>`（§7.2）；时间伸缩（§7.4）；`copyOriginal` 对应重新加载文档 | 部分已设计，其余需新增 |
| `PAGComposition` | 宽高与 `setContentSize`；子图层增删改查与换序；`getLayersByName`、`getLayersUnderPoint`；`audioBytes`、`audioStartTime`、`audioMarkers` | 结构编辑走文档编辑接口（`notifyChange`）；命中测试已有；音频数据接口（§7.4）；`audioMarkers` 不支持（C35） | 需新增 |
| `PAGLayer` | 矩阵、透明度、可见性、起始时间、当前时间与进度、时间换算、包围盒、轨道蒙版图层、`excludedFromTimeline`、`markers` | 文档编辑接口或新增运行时接口；`markers` 不支持（C35） | 需新增 |
| `PAGTextLayer` | 文本、字体、字号、填充色、描边色、`reset` | ViewModel 文本属性（§7.2） | 已设计 |
| `PAGSolidLayer` | `setSolidColor` | 对应 Fill 颜色的修改接口 | 需新增 |
| `PAGImageLayer` | `replaceImage/setImage`、`contentDuration`、`getVideoRanges`、图层时间与内容时间换算 | ViewModel 图片属性；动态内容的时间映射（§7.4 图片行） | 需新增 |
| `PAGImage` | 从路径、字节、像素、纹理创建；缩放模式、矩阵；非静态内容 | 新运行时已有 `PAGImage`；缩放模式与矩阵、非静态内容需核对 | 待核对 |
| `PAGFont` | `RegisterFont`、`UnregisterFont`、回退字体名与路径 | 全局字体注册接口（§8.1） | 已设计 |
| `PAGPlayer`、`PAGSurface` | 播放控制、缓存开关与缩放、最大帧率、缩放模式与矩阵、视频开关、命中测试、性能统计 | 播放视图与 `PAGScene`/`PAGDisplayOptions` | 需逐项核对 |
| `PAGDecoder` | 按帧解码为位图、帧变化检测、静态时间段（`PAGImageView` 使用） | 新运行时的逐帧渲染接口 | 需新增 |
| `PAGDiskCache`、`PAGVideoDecoder` | 磁盘缓存、硬件解码器数量、软解工厂 | 随四期平台接入与 §6.6 视频解码设计 | 需新增 |

---

## 9. 性能

| 环节 | XML 路径 | 新二进制 |
|---|---|---|
| 解析 | XML DOM + 字符串解析 + base64 | 线性 Tag 扫描 |
| 引用 | 字符串 id 查表 | 导入时解析的引用改为序号回填；运行时按字符串查找的引用不变 |
| 布局 | 完整布局（约束、弹性容器） | 只剩逐节点测量（已扁平化） |
| 文本 | 运行时排版 | 运行时排版，与旧 PAG 相同（评估见 §11）；嵌入字形的文本直接绘制 |
| 图片 | base64 解码 | 零拷贝切片，渲染时解码 |
| 静态内容 | — | 不含时间线 Tag，加载后不逐帧推进 |
| 压缩 | — | 无 |

LZ4 实测（`resources/` 下 282 个旧 PAG 样本）：

| 分组 | LZ4 | zstd -3 |
|---|---|---|
| 纯矢量 | 98.6% | 92.4% |
| 含图片/视频 | 98.3% | 97.2% |

不启用通用压缩。M0 建立体积、解码耗时、内存、包体基线。

---

## 10. 测试

| 编号 | 测试 | 判定 |
|---|---|---|
| T1 | 渲染等价：PAGX 样本 XML 直接渲染 vs 新 pag 渲染（同一套字体），覆盖静态样本、动画样本的多个时间点、布局扁平化、字体嵌入样本 | 与 baseline 对比通过 |
| T2 | 运行时行为等价：动画、状态机输入序列、ViewModel 赋值序列；嵌入文本被修改后按注册字体重排；缺字体时回退且不崩溃 | 一致 |
| T3 | 兼容性与确定性：未知 Tag、补充 Tag、V2 Tag；tagLevel 统计；同一输入两次导出逐字节相同 | 正确跳过；tagLevel 正确；字节一致 |
| T4 | 格式识别：旧 pag v0/v1/v2、加密、新 pag、pagx、BOM、损坏文件；SDK 与 PAGViewer 的路由和错误码 | 正确 |
| T5 | 健壮性：截断、破坏、引用环、超深嵌套、子文档成倍展开、各项资源超限、fuzz | 不崩溃、不死循环、超限拒绝 |
| T6 | 旧 PAG 迁移：282 个样本旧运行时 vs 新 pag 渲染对比；可编辑内容的 `replaceText/replaceImage` vs ViewModel 赋值对比；“直接 export”与“import 后再 export”渲染等价；`pagx import` 输出的 XML 能通过 `pagx verify` | 报告 + 对比通过 |
| T7 | 基准：体积、解码耗时、内存、包体 | 趋势报告 |
| T8 | §6 规范扩展：XML 导入导出、运行时、verify | 通过 |
| T9 | 静态与动画：静态文件不含时间线 Tag，首帧后 `hasContentChanged()` 为 false；动画文件的时长、帧率、循环模式与 XML 一致；默认时间线选择；播放视图对两类文件的播放与进度接口行为 | 正确 |
| T10 | 扁平化边界：被约束定位的 Text（textAnchor 为 start/center/end）、被缩放的 Text、TextBox 自动高度、带逐点通道且被缩放的 Path、clipToBounds、指向布局专用通道的动画/DataBind | 扁平化前后逐像素一致；告警正确 |
| T11 | 引用与作用域：多个 SubDocument、子文档多级嵌套、同一外部文件多次引用、子文档内图片与字体、子文件缺失、合成环、遮罩环、跨作用域目标、同一通道多个 DataBind 的顺序、保留与剥离的 id | 与 XML 行为一致 |

全部纳入 `PAGFullTest`。

### 10.1 现有样本覆盖评估

按 `spec/pagx.xsd` 的元素、属性和枚举值，扫描仓库全部 317 个 PAGX 样本（`resources/cli` 134、`resources/pagx_to_html` 100、`spec/samples` 50、`resources/layout` 20、`resources/text` 10 等）：

| 方面 | 现状 | 结论 |
|---|---|---|
| 静态矢量、布局、文本 | 63 种元素中大部分有样本；约束 216 个文件、布局容器 142 个、TextBox 43 个、竖排 8 个 | 基本满足 |
| 关键帧动画 | 只有 2 个文件；没有 pingPong 循环、贝塞尔/none 插值，通道类型只用过 float | 严重不足 |
| 状态机 | 0 个文件（StateMachine、StateRegion、State、Transition、Condition、Input 全部未出现） | 缺失 |
| ViewModel / DataBind | 2 个文件；属性类型缺 string、boolean、color、image、enum、trigger、嵌套 ViewModel；DataBind 的 direction 未用过 | 严重不足 |
| 样式与滤镜 | NoiseFilter、NoiseStyle 未出现；样式、滤镜、填充、描边上的混合模式大多未覆盖；tileMode、excludeChildEffects、shadowOnly 部分取值未覆盖 | 不足 |
| 资源与特殊值 | 外部 Composition 文件 0 个；P3 颜色 2 个；matrix3D 3 个；位图字形 2 个；Font `file` 1 个 | 不足 |
| 其它属性 | TextBox 的变换属性、Path 的 reversed/position、DiamondGradient 的 fitsToGeometry、Composition 的 viewModel、Property 的 options/viewModelRef 未出现 | 不足 |

动画、状态机、ViewModel 目前主要靠 `test/src` 里的内联 XML 用例覆盖（如 `PAGXRuntimeTest`、`PAGXViewModelTest`），这些用例也要接入“先编码再解码”的路径（T2）。

### 10.2 新增样本集 `resources/pagx_to_pag/`

与现有的 `resources/pagx_to_html/` 命名一致，按主题分子目录，每个文件聚焦一个特性，便于在对比页中定位问题：

| 子目录 | 内容 |
|---|---|
| `animation/` | 每种通道值类型（float/int/bool/string/color/matrix/image）、每种插值、三种循环模式、Path 逐点通道、Composition 作用域自动播放、Layer 挂载的时间线、多个根动画 |
| `state_machine/` | 三种输入、全部条件运算符、过渡参数（时长、插值、earlyExit、pauseOnExit）、多区域并行、any 状态 |
| `viewmodel/` | 全部属性类型、嵌套 ViewModel、转换器、四种 DataBind 方向、Composition 级 ViewModel、绑定到文本属性 |
| `style_filter/` | NoiseFilter/NoiseStyle、各混合模式、tileMode、excludeChildEffects、shadowOnly |
| `layout_flatten/` | §4.2 的扁平化边界（T10 的各场景） |
| `text_font/` | 嵌入矢量字形、嵌入位图字形、嵌入与未嵌入混合、Font `file`、缺字体回退、竖排、双向文本、emoji、富文本 |
| `resource/` | data URI 与外部路径图片、需缩小的大图、带透明边的图片（decal/repeat）、同图多处引用、缺失图片、JPEG/WebP/GIF、P3 颜色、matrix3D |
| `sub_document/` | 外部子 PAGX：单次与多次引用、多级嵌套、子文档内的图片与 Font \`file\`、不同目录的相对路径、环形引用、缺失文件、子文档带布局约束 |
| `static_animated/` | 纯静态、静态 + ViewModel、静态与动画混合、无对象的 Animation、时长为 0 |
| `spec_extension/` | §6 规范扩展与 §7.4 能力清单各项的样本（三期随功能补充） |

每个新增特性都要先在样本集里有对应文件，再进入对比页与 T1/T2。

### 10.3 PAGX / PAG 渲染对比页

参考现有 `test/gen_html_comparison.sh/.py` 的生成方式，新增 `test/gen_pagx_pag_comparison.sh/.py`，输出 `test/out/pagx_pag_comparison/index.html`：

- **三列**：第 1 列 PAGX 直接渲染，第 2 列新 pag 渲染，第 3 列两者的像素级 diff（差异像素高亮，并显示最大差值、平均差值、差异像素占比）。
- **顶部控制栏**：显示列数切换（2 列：PAGX + PAG；3 列：加上 diff）；按样本目录分标签页；“只看有差异”的过滤与按差异排序。
- **每个样本卡片**：文件名、PAGX 与 pag 的体积及压缩比、告警数量。
- **动画样本**：按默认时间线在多个时刻渲染（开始、1/4、1/2、3/4、结束），卡片内可切换时刻；StateMachine 按“从初始状态推进到该时刻”渲染。
- **渲染方式**：两列都用 `pagx render` 在同一台机器、同一套字体下生成；diff 的通过阈值与截图测试 `Baseline::Compare` 一致。
- **范围**：现有 317 个样本 + `resources/pagx_to_pag/`；旧 PAG 迁移样本另开一个标签页，第 1 列换成旧运行时的渲染。

## 11. 文本方案评估（决策依据）

**结论（已确认）**：所有文本默认运行时排版，与旧 PAG 一致；只保留作者已嵌入的字形（§4.3）。以下是做出该决策的实测数据。

**体积实测**：对仓库 120 个含文本的 PAGX 样本执行 `pagx embed`，按旧 PAG 精度（0.05）估算二进制体积。

| 口径 | 字形轮廓 + 预排版位置 | 只存文本信息（运行时排版） |
|---|---|---|
| 120 个样本合计 | 约 575 KB（轮廓 523 KB + 位置 52 KB） | 约 22 KB |
| 文本较多的单个文件 | 约 10–13 KB | 约 0.2–0.7 KB |
| 每个不重复字形 | 拉丁字母约 105 B，中文约 284 B（苹方） | 每字符 1 B（拉丁）/ 3 B（中文） |
| 50 个不同汉字 | 14.1 KB | 0.17 KB |

字形轮廓体积约为文本信息的 25–80 倍，并随不重复字符数线性增长；作为对比，旧 PAG 纯矢量样本平均约 46 KB。

**性能评估**：

- **每帧绘制**：两者最终都生成 TextBlob，字形光栅化后进入缓存，后续帧开销相同。
- **加载阶段**：
  - 运行时排版：HarfBuzz 实测（`hb-shape`，Apple Silicon）约 2 µs/行短文本；完整排版还包括断行、双向文本、回退字体查找，预计仍在亚毫秒级，M0 实测确认。首次按名字加载字体文件为毫秒级，之后全局缓存，旧 PAG 同样有这部分开销。
  - 字形轮廓：需要解码轮廓并构造路径字体（`GlyphRunRenderer.cpp:138` 的 `PathTypefaceBuilder`），开销与字形数成正比，同为亚毫秒级。
- 结论：两种方式的渲染性能没有明显差异。

**真正的差别在于显示一致性与字体依赖**：

- **字形轮廓**：任何设备显示一致，不需要字体。
- **运行时排版**：依赖宿主注册与导出时相同的字体。缺字体时会使用回退字体，字宽变化后文本在自身文本框内重新换行和对齐，但周围已解算的自动布局不会随之变化，可能出现溢出或错位。旧 PAG 有同样的依赖。

## 12. 分期实施与预留（C29）

**原则**：

- 规范和格式一次定全，实现分期。M0 产出覆盖各期全部节点和字段的字节级规范（三期新增的能力在 M0 先确定 Tag 编号区间与结构框架，字段在三期详细设计时补全，发版前都可修订）；每一期只实现编码器、解码器、运行时中属于本期的部分。
- 各期之间不对外发版（C33）。后续各期如果发现格式设计有问题，可以直接修订规范和实现，不需要兼容旧的中间格式，也不加兼容层；对外发版后格式冻结，之后只能按 C2 追加。
- 发版前不向仓库提交 `.pag`（v4）二进制产物：测试和对比页都从 `.pagx` 现场导出，避免格式修订后残留过期文件。

### 12.1 各期范围

| 期 | 目标 | 范围 | 不在本期 |
|---|---|---|---|
| M0 | 规范与基础 | 字节级规范（各期全部 Tag）；规范扩展定稿（§6、默认时间线、时间锁定挂载、子文档路径规则）；公开接口草案；`resources/pagx_to_pag/` 样本集；基线测量；按文件拆分的任务清单 | 编码实现 |
| 一期 | 静态 PAGX → 新 PAG | 容器与基础编码、格式识别；全部静态节点（图层、矢量元素、样式、滤镜、遮罩、内部与外部 Composition、图片、文本）；布局扁平化、剥离、图片处理；文本运行时排版与嵌入字形；外部子 PAGX（§4.8）；`PAGImporter` 解码与 `PAGScene` 静态显示；全局字体注册接口；CLI `export/info/render/verify`；PAGViewer；渲染对比页 | 动画、状态机、时间线、ViewModel/DataBind |
| 二期 | 动效 PAGX → 新 PAG | Animation、关键帧、StateMachine、Layer 挂载的时间线；ViewModel、DataBind、DataConverter；嵌入文本被修改后的回退（C18）；扁平化中的关键帧变换、剥离与图片缩放对动画的处理；各播放端统一默认时间线（C23） | 旧 PAG 迁移、§6 扩展 |
| 三期 | 旧 PAG → 新 PAG | §6 全部规范扩展与时间锁定挂载的运行时实现；§7.4 全量能力清单（含视频与位图序列 §6.6、特效、样式、3D、运动模糊、时间伸缩、音频等）；旧版 PAG 导入、`pagx import/export` 旧 pag 输入、保真度报告；可编辑文本/图片 → ViewModel；PAGXExporter 保留无对象的 Animation；旧接口对照与新增接口的 C++ 实现（§8.5） | Marker（C35）；平台封装（四期） |
| 四期 | 平台接入（最后一期） | 平台接口设计；Android、iOS/macOS、OHOS、Web 接入：播放统一、新运行时对象模型、ViewModel、字体注册，以及三期迁移新增的接口（时间伸缩、音频、结构编辑等，§8.5）；发版前全量回归 | — |

### 12.2 一期必须预留、避免返工的点

| 预留项 | 一期怎么做 | 后续怎么用 |
|---|---|---|
| 字节级规范 | M0 写全一期、二期的 Tag 和字段，并为三期的能力划定编号区间与结构框架 | 后续各期按规范实现；发版前允许修订 |
| 引用模型 | 一期实现节点序号、字符串表、按需保留 id 的完整机制（§5.2），一期只有 Layer id 用到 | 二期的动画目标、DataBind 目标、时间线引用直接复用 |
| 扁平化 | 模块接口预留“几何变换回调”：烘焙缩放时调用，一期没有关键帧时为空操作 | 二期在回调中变换路径相关的关键帧值 |
| 剥离 | 节点存活分析按“渲染树 + 被引用目标”两类根计算，一期只有渲染树 | 二期把动画、DataBind 目标加入根集合 |
| 图片缩放 | 最大显示缩放按“实例 × 变换”统一计算 | 二期把关键帧中的变换加入计算 |
| ImageTable | 条目带媒体类型字段 | 后续的视频、位图序列 |
| 时间锁定挂载 | 字段在规范中定义，一期不写 | 三期实现运行时（二期需要时可提前） |
| `LayerTransform` 等规范扩展字段 | 编号和字段在规范中定义，一期不写 | 三期实现 |
| 解码器 | 遇到本期未实现的 Tag 按长度跳过（`tagLevel` 记录），与发版后的兼容规则一致 | 后续各期逐步实现这些 Tag |

### 12.3 一期遇到动效内容（C32）

- **处理**：只导出静态画面并告警，不报错。告警列出被忽略的元素（Animation、StateMachine、时间线挂载、ViewModel、DataBind、DataConverter）及所在文档（含子文档）。
- **静态画面的定义**：文档中写明的静态属性值，不应用任何动画和数据绑定。
  它可能与 XML 运行时的第一帧不同：例如关键帧在 0 时刻的值不同于属性的静态值，或者文字绑定了 ViewModel 的默认值。告警中会注明这一点。
- **为什么不烘焙第一帧**：动画在运行时直接写在布局结果之上，不触发重新布局。把它们写回文档再布局，会改变语义（§4.2），而且这部分工作二期本来就要按正确方式实现。一期产物只在内部使用（C33），保持简单。
- **测试范围**：一期的 T1 和对比页只覆盖不含动效内容的样本；含动效的样本从二期开始对比。

## 13. 编码就绪度与执行约定

### 13.1 评估结论（第二轮）

| 阶段 | 能否直接交给 AI 编码 | 依据 |
|---|---|---|
| M0 | 可以 | 决策、边界清单、分期范围齐全；M0 本身就是把本文落成可执行的规范、接口、样本和指标 |
| 一期 | M0 完成后可以 | 需要 M0 的字节级规范、接口草案、量化指标和任务清单 |
| 二期 | 开工前可以 | 设计已全盘考虑并在格式中预留；开工前按 §13.4 输出该期接口与伪代码 |
| 三期（旧 PAG 迁移） | 需先完成详细设计 | §7.4 能力清单中大量新增能力（特效、样式、3D、运动模糊、遮罩路径列表、文本动画器、时间伸缩、音频、视频与位图序列）只有方向，需要逐项设计 |
| 四期（平台接入） | 需先完成平台接口设计 | 只有原则（§8.2）；放在最后，接口可以一次覆盖一至三期的全部能力 |

第二轮新发现并已补入的问题：视频合成纳入发版范围（§6.6）；固定量化步长在局部坐标被放大时失真（§4.7）；子文档逐引用解码会成倍展开（§4.8）；导出结果需要确定性（§4.1）；解码资源上限（§5.5）；迁移后业务需要旧接口对照（§8.5）；缺少量化验收指标（§13.3）。

### 13.2 问题清单

| 编号 | 问题 | 状态 |
|---|---|---|
| Q1 | 嵌套合成时钟 | 已关闭：时间锁定挂载（C26，§6.3） |
| Q2 | `data-*` 剥离范围 | 已关闭：保留三处（C27） |
| Q3 | 视频合成范围 | 已关闭：随旧 PAG 迁移在三期实现，发版前完成（C28） |
| Q4 | ViewModel/DataBind 分期 | 已关闭：二期（C31） |
| Q5 | 一期遇到动效内容 | 已关闭：只导出静态画面并告警（C32，§12.3） |
| Q6 | 平台 SDK 交付时机 | 已关闭：发版前完成（C33） |
| Q7 | 旧 PAG 迁移的范围 | 已关闭：覆盖全部能力，在三期实施（C34，§7.4） |
| Q8 | 旧接口对齐程度 | 已关闭：Marker 不支持（C35）；其余旧接口按 C34 提供等价能力，形式在三期设计中确定（§8.5） |

### 13.3 M0 需要补齐的产出（一期开工前提）

1. `spec/pag_binary_spec.md`：每个 Tag 的编号、字段顺序、类型、默认值、存在位，以及基础类型的字节布局；附录 A/B 以它为准。
2. 公开接口草案：`include/pagx/PAGExporter.h`、`PAGImporter.h`（选项、警告与错误的表达方式）、全局字体注册接口、报告 `report.json` 的结构。
3. 规范扩展定稿：§6 五项、默认时间线、时间锁定挂载（§6.3）、外部子 PAGX 的路径规则（§4.8）。
4. `resources/pagx_to_pag/` 第一批样本（§10.2）。
5. 基线：体积、解码耗时（含手机上的文本排版）、包体；业务素材的旧 pag 特性分布。
7. 共用的“本地外部文件加载”函数的接口（§4.8），供 CLI、PAGViewer 使用。
8. 量化验收指标：以基线为参照，给出文件体积（相对 XML）、加载耗时（解码 + 构建 + 首帧）、内存峰值、包体增量的目标值；每期验收时对照。
9. §5.5 的资源上限、§4.8 的展开上限的具体数值。
10. `PAGXOptimizer` 是否默认开启的实测结论（§4.1）。
6. 按文件拆分的任务清单：每个任务写明改动文件、依赖、验收用例。

### 13.4 AI 编码执行约定

- 每一期开工前，先输出该期的接口与伪代码，确认后再编码；遇到与本文不符的情况，停下说明，不自行降级。
- 每个任务必须带测试：新增节点或 Tag 时，同步增加样本（`resources/pagx_to_pag/`）与往返用例；渲染相关改动进入对比页（§10.3）。
- 截图基准只能由用户通过 `/accept-baseline` 接受；全量 `PAGFullTest` 通过后才能提交。
- 运行时只读不写：解码器不得修改 PAGX 运行时的行为语义；需要改运行时的点（§4.3 嵌入文本回退、§5.4 空 Animation 导出、§6 扩展与时间锁定挂载、§4.8 子文档挂接）单独成任务并配测试。
- 只实现当期范围（§12.1），但不得违反 §12.2 的预留约定；发现预留不足时停下来说明。

---

## 14. 终审结论

**评审方式**：从五个方面并行评审，评审意见逐条回到代码核实后再采纳：内部一致性、代码引用核实、一二期就绪度、三期迁移完整性、二进制格式。

**结论：有条件通过，可以进入实施计划。**

- 内部一致：C1–C35 与正文一致，36 处文件引用、21 处函数引用都已核实。
- 本轮修订已补入正文：枚举扩展规则（§3.2）、资源内联与共享规则、引用不跨文档（§5.2）、关键帧缓动与通道名（§5.3）、图片零拷贝需要的 Data 切片（§5.1）、长度校验（§5.5）、不提交二进制产物（§12.1）、附录 B 的 Document/Composition/时间线、§7.4 特效与样式逐项、§8.5 按接口类整理。
- 核实后不采纳的意见：
  - “空 Animation 无法与无 Animation 区分”：Animation 块本身存在即可区分。
  - “Text.baseline 遗漏”：附录 B 已列。
  - “字符串表、属性块缺少逐字节定义”属实，但这是 M0 字节级规范的工作内容，已列入 §13.3。

**条件（M0 必须完成，否则不开始一期编码）**：

1. 字节级规范中给出逐字节示例：属性块（含字符串、可选值）、字符串表、资源表与内联、关键帧块、路径记录（含 Quad 与步长指数）、ImageTable 条目、SubDocument。
2. 锁定 §4.8、§5.5 的各项上限数值。
3. 一期、二期涉及的 Tag 字段全部定稿；三期能力划定编号区间。
4. §13.3 其余产出（接口草案、量化指标、样本集、任务清单）。

**延后到对应期开工前的详细设计**：三期的 §6.6 视频与位图序列、§7.4 新增能力、§8.5 逐方法对照；四期的平台接口。

---

## 附录 A. TagCode 分配（新格式，初版）

| 区间 | 编号 |
|---|---|
| 256–287 文件级 | 256 FileAttributes、257 StringTable、258 ResourceBlock、259 DocumentBlock、260 SubDocumentTable、261 SubDocument、262 ImageTable |
| 288–351 资源 | 288 Image、289 PathData、290 SolidColor、291 LinearGradient、292 RadialGradient、293 ConicGradient、294 DiamondGradient、295 ImagePattern、296 Font、297 CompositionBlock、298 CompositionAttributes、299 ViewModelBlock、300 ViewModelProperty、301 DataConverter |
| 352–415 图层 | 352 LayerBlock、353 LayerAttributes、354 LayerTransform、355 DropShadowStyle、356 InnerShadowStyle、357 BackgroundBlurStyle、358 NoiseStyle、359 GlassStyle、360 BlurFilter、361 DropShadowFilter、362 InnerShadowFilter、363 BlendFilter、364 ColorMatrixFilter、365 NoiseFilter、366 AnimationTimeline、367 StateMachineTimeline |
| 416–479 矢量元素 | 416 Rectangle、417 Ellipse、418 Polystar、419 Path、420 TextBlock、421 TextAttributes、422 GlyphRun、423 Fill、424 Stroke、425 TrimPath、426 RoundCorner、427 MergePath、428 TextModifierBlock、429 RangeSelector、430 TextPath、431 TextBoxBlock、432 TextBoxAttributes、433 GroupBlock、434 GroupAttributes、435 Repeater |
| 480–543 动画 | 480 AnimationBlock、481 AnimationObject、482 Channel、483 StateMachineBlock、484 StateMachineInput、485 StateRegionBlock、486 State、487 StateTransition、488 TransitionCondition |
| 544–575 数据绑定 | 544 DataBind |
| 576–767 | 三期迁移能力（§7.4：特效、样式、遮罩路径列表、3D、序列、时间伸缩、音频等）、补充 Tag、V2 Tag |
| 768–1022 | 未来预留 |

## 附录 B. 节点属性（渲染子集，初版）

“渲染值”指扁平化后写回的显式属性（§4.2），精确集合在一期对照 `render*()` 访问器确定。

| 节点 | 属性 |
|---|---|
| Document | 宽、高、文档根的 `data-*`（C27）；子：根图层、根作用域的 Animation/StateMachine（二期）、ViewModel 与 DataBind（二期） |
| Composition | 宽、高；子：图层、Composition 作用域的 Animation/StateMachine（二期）、ViewModel 与 DataBind（二期） |
| Layer | LayerAttributes：name、x、y（渲染值）、visible、alpha、blendMode、matrix、matrix3D、preserve3D、antiAlias、groupOpacity、passThroughBackground、scrollRect、mask、maskType、composition、vmContext；LayerTransform：anchor、rotation、scale、skew、skewAxis、startTime、timeScale；子：AnimationTimeline/StateMachineTimeline（二期，时间锁定字段三期） |
| Rectangle / Ellipse | position、size（渲染值）、roundness（仅 Rectangle）、reversed |
| Polystar / Path / TextPath | 渲染值 position、scale + 各自 PAGX 字段 |
| Text | TextAttributes：position、width/height（渲染值，可选）、text、fontFamily、fontStyle、fontSize、letterSpacing、fauxBold、fauxItalic、textAnchor、baseline 及 §6.5 新增字段；子：GlyphRun（仅源文件已嵌入字形时） |
| TextBox | Group 属性；TextBoxAttributes：width、height（渲染值）、padding、textAlign、paragraphAlign、writingMode、lineHeight、wordWrap、overflow |
| Group | anchor、position（渲染值）、rotation、scale、skew、skewAxis、alpha |
| Fill / Stroke / 修改器 / 颜色源 / 样式 / 滤镜 | 与 PAGX 字段一致 |
| Image | ImageTable 索引、逻辑尺寸、scaleFactor、内容偏移 |
| Font | 仅源文件已嵌入字形时存在：unitsPerEm、字形列（advance、offset、路径或 ImageTable 引用）；不存 `file` |
| 动画 / 状态机 / ViewModel / DataBind | 与 PAGX 字段一致；运行时按字符串查找的引用保持字符串（§5.2） |

## 附录 C. 旧 TagCode 在迁移中的处理

全部旧 TagCode（0–94）的处理见 §7.4 能力清单；其中只有两类不迁移：

| 旧 Tag | 处理 |
|---|---|
| MarkerList（53） | 跳过并在报告中记录（C35） |
| EncryptedData（89） | 开源旧 SDK 同样无法解码，不在迁移范围 |
| Performance（28）、CachePolicy（30） | 不影响渲染结果，三期评估是否需要对应的运行时开关（§7.4） |

## 附录 D. 边界情况清单

| 类别 | 场景 | 处理 | 位置 |
|---|---|---|---|
| 格式识别 | 旧 v0/1/2、加密 v3、新 v4、≥5、PAGX（含 BOM/空白/注释开头）、空文件、截断文件 | 按文件头分流；≥5 返回“需要更高版本 SDK”；其它返回明确错误码 | §3.3、§3.6 |
| 兼容 | 较早的新 SDK 遇到后续追加的 Tag | 按长度跳过；tagLevel > MaxSupportedTagLevel 时宿主可提示升级 | §3.1、§3.2 |
| 布局 | 约束、弹性容器、百分比、自动尺寸、clipToBounds、Layer 取整 | 扁平化写回；内容决定的轴保持自动 | §4.2 |
| 布局 | 动画/DataBind 指向布局专用通道 | XML 下也不生效，导出时删除并告警 | §4.2 |
| 布局 | 被缩放的 Path 带几何关键帧 | 烘焙缩放时同步变换关键帧值 | §4.2 |
| 文本 | 运行时缺字体 | 走回退字体；文本在自身位置/文本框内重排，周围布局不动 | §4.2、§4.3 |
| 文本 | 导出机缺字体 | 写入 warnings，列出字体名与节点 | §4.3 |
| 文本 | 嵌入字形的文本被修改 | 改用注册字体排版 | §4.3 |
| 文本 | TextBox 内嵌入/未嵌入混合 | 整组运行时排版并告警 | §4.3 |
| 文本 | 空文本、空字体名、emoji、双向文本、竖排 | 与 XML 运行时一致（UTF-8 原样存储，运行时排版处理） | §4.3 |
| 图片 | 找不到、解码失败、动图、超大图 | 告警并跳过 / 取首帧 / 按最大显示缩放缩小 | §4.5 |
| 图片 | ViewModel 图片属性的默认图、宿主替换图 | 默认图走旧流程；替换图按自身尺寸由 scaleMode 适配 | §4.5 |
| 引用 | 前向引用、同一外部文件多次引用、跨作用域目标 | 序号回填；外部文件存一份；作用域由运行时检查 | §5.2 |
| 引用 | 引用环、遮罩环、合成环、超深嵌套 | 解码时拒绝该引用或子树，深度上限 128 | §5.2 |
| 子文档 | 多级嵌套、同一文件多处引用、相对路径跨目录、URL、缺失、环形引用 | 递归打包、按绝对路径去重、每个引用独立解码；URL 与缺失告警；环报错 | §4.8 |
| 元数据 | 宿主读取的 `data-*` | 只保留 ViewModelProperty、文档根、ImagePattern 三处 | C27 |
| 分期 | 一期导出器遇到动效内容（含子文档中的） | 只导出静态画面并告警 | §12.3 |
| 迁移 | 旧格式有、旧运行时不使用的数据（如图层时间重映射）；Marker；加密文件 | 不迁移并在报告中说明原因 | §7.4、C35 |
| 动画 | 时长 ≤ 0、帧率 ≤ 0 | 告警并原样保留，运行时时长为 0 | §5.4 |
| 动画 | 没有对象的 Animation | 保留；PAGXExporter 同步保留 | §5.4 |
| 动画 | 关键帧时间乱序、值类型不符 | 丢弃该通道并告警 | §5.3、§5.5 |
| 动画 | 同一通道多个 DataBind / Animation | 保持声明顺序 | §5.2 |
| 静态 | 纯静态文件 | 不写时间线 Tag，播放接口作为静态画面 | §5.4 |
| 数值 | NaN（未设置）、Inf、P3 颜色、超出 [0,1] 的颜色 | 存在位 / 导出报错 / 浮点颜色 | §5.6 |
| 数值 | 几何写在很小的局部坐标并被放大 | 量化步长按显示缩放缩小 | §4.7 |
| 子文档 | 多级共享导致实例成倍展开 | 导出与解码检查上限，超限报错 | §4.8 |
| 导出 | 同一输入多次导出 | 逐字节相同 | §4.1 |
| 视频 | 多分辨率序列、Alpha 并排编码、静态时间段、平台解码差异 | 原样存储、按显示缩放选择、复用旧解码栈 | §6.6 |
| 健壮性 | 未知枚举值、计数超限、Tag 长度越界、fuzz 输入 | 取默认值 / 拒绝 / 跳过该 Tag，不崩溃 | §5.5 |
| 文档 | 空文档、零尺寸画布 | 合法 | §5.5 |
| 编辑 | 解码后调用 notifyChange、按被剥离的 id 调 findNode | 编辑可用、重新布局幂等；被剥离的 id 找不到 | §4.2、§5.2 |
| 播放 | 默认时间线是 StateMachine、根作用域有多个 Animation | 按 getDefaultTimeline；StateMachine 无进度 | §5.4 |
