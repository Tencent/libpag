# PAGX → PAG 实施计划（M0-9：一期按文件拆分的任务清单）

> 依据：`docs/pagx_to_pag_design.md` v1.0-rc（§12.1/§12.2/§13.4）、`spec/pag_binary_spec.md` v1.0-draft1、
> `docs/pagx_to_pag_interfaces.md`。
> 执行约定（§13.4）：一期开工前先输出一期接口与伪代码确认；每任务带测试；截图基准只由
> `/accept-baseline` 接受；全量 PAGFullTest 通过后提交；只实现一期范围但遵守 §12.2 预留。

## M0 剩余项（本清单之后一期才可开工）

| 任务 | 产出 | 依赖 |
|---|---|---|
| M0-6 PAGX 规范扩展定稿 | ✅ 已完成（2026-10-22）：§6 全部扩展、默认时间线、时间锁定（秒）、子文档路径规则 → spec/pagx_spec 中英文 + pagx.xsd 三处同步；strokeOverFill 用声明顺序、tracking 新增通道两项定稿已回写设计文档 §6.5 | 无 |
| M0-7 样本集 | ✅ 已完成（2026-10-22）：resources/pagx_to_pag/ 57 个样本、9 个子目录，pagx verify 全量 57/57 零诊断；同步修正 xsd composition 路径模式、Timelines evaluationOffset、规范 §8.3.1 示例、verify 的 Font file 豁免（见样本集 README） | 无 |
| ~~M0-8 基线~~ | ✅ 已完成（2026-10-22）：docs/pagx_to_pag_baseline.md——体积（语料 690.6KB）、加载（导入+布局 176ms，文本排版主导）、内存（重样本 51–61MB）、包体（旧 codec text 段 5.06MB Debug）、PAGXOptimizer 实测（XML -17%，**默认开启**）、embed 对照 4.17×、旧 pag 特性分布；量化验收指标（体积 ≤50% XML、加载 ≤70%、内存 +10%、包体 ≤500KB Release、确定性、渲染等价）已定 | 构建环境 |

## 一期任务（依赖顺序）

### T1 基础编解码设施（无依赖）✅ 已完成（2026-10-22）
- 新增 `src/pagx/pag/TagCode.h`：v4 全部 TagCode 枚举（规范 §1.3/§9，含 263/264/436/489–492 新增号）。
- 新增 `src/pagx/pag/StreamContext.h` 复用与 `EncodeStream`/`DecodeStream` 接入：确认流工具对
  独立 PAGX 构建的可编入性（设计文档 §2 编译依赖），必要时把 `src/codec/utils/` 移到共用位置。
- 新增 `src/pagx/pag/PAGAttributeIO.h/.cpp`：属性块读写（存在位 + 值区、BitFlag/Fixed/Custom，
  规范 §4），PAGX 类型新写（不绑 pag::Property）。
- 新增路径读写（规范 §7：4 位记录 + Quad + 步长指数）与关键帧读写骨架（规范 §8，一期只编译不写文件）。
- 格式识别与文件头（规范 §1.1/§3.6）：`PAGImporter::DetectFormat`、v4 头写出与校验。
- **验收**：varint/位打包/颜色/矩阵/路径/字符串表的单测（字节级断言，直接采用规范 §10 的示例字节）。

### T2 导出前处理：布局扁平化（依赖：无，与 T1 并行）✅ 已完成（2026-10-22，c344ba560）
- 新增 `src/pagx/pag/exporter/LayoutFlattener.cpp`：§4.2 逐类写回（Layer/形状/Path/Text/Group/TextBox），
  `clipToBounds → scrollRect`，布局输入删除；几何变换回调接口（§12.2 预留，一期空操作）。
- 布局专用通道的动画/DataBind 删除 + 告警（一期动效内容整体跳过，告警清单先建好给二期用）。
- **验收**：T10 场景用例（textAnchor×3、缩放 Text、TextBox 自动高度、逐点通道缩放 Path、
  clipToBounds、布局通道动画告警）；扁平化前后逐像素一致。

### T3 剥离与资源收集（依赖 T2）✅ 已完成（2026-10-22，70a835d1d）
- 新增 `src/pagx/pag/exporter/Stripper.cpp`：§4.6 清单（data-* 三处保留、id 按保留集、死节点、
  资源合并）；存活根 = 渲染树（§12.2 预留"被引用目标"根）。
- 新增 `src/pagx/pag/exporter/ResourceCollector.cpp`：字符串表/资源表收集、首次引用顺序、
  内联 vs 共享决策、两趟发射序号分配（规范 §5.1）、RetainedIdTable 生成。
- **验收**：id 保留/剥离清单用例；两次导出逐字节相同（确定性）；内联/共享用例（规范 §10.3 字节）。

### T4 图片处理（依赖 T2）✅ 已完成（2026-10-22，bfdf38043）
- 新增 `src/pagx/pag/exporter/ImageProcessor.cpp`：最大显示缩放（实例×变换统一计算，§12.2 预留关键帧）、
  factor、裁边、WebP q80、已是 WebP 不重编码、去重、动图取首帧、缺失告警。
- `pagx::Data` 切片构造（父缓冲 + 偏移 + 长度，§5.1 零拷贝）→ `include/pagx/types/Data.h` + `src/pagx/Data.cpp`。
- ImageTable 编码（规范 §6.6，mediaType 一期只产生 0/1）。
- **验收**：decal 裁边/repeat 不裁、缩小、原样写入、去重、缺失用例；内存峰值记录给 M0-8。

### T5 编码器主体（依赖 T1、T3、T4）✅ 已完成（2026-10-22，7c3bc5d8e）
- 新增 `src/pagx/pag/PAGExporter.cpp`：Body 组装（FileAttributes/StringTable/ResourceBlock/
  DocumentBlock/[SubDocumentTable]/ImageTable/End）、文件头、4GB 检查、报告数据。
- 新增 `src/pagx/pag/tags/`：全部一期节点 Tag 写出（Layer/Group/TextBox/Text/GlyphRun/Font/
  矢量元素/画笔/修改器/样式/滤镜/Composition/Document，规范 §9）。
- 动效内容告警（AnimatedContentSkipped，§12.3）。
- **验收**：317 个仓库样本全部可导出（动效样本按静态画面 + 告警）；往返导出字节稳定；
  体积对比 XML 记入 T7 基准。

### T6 v4 解码器（依赖 T1；与 T5 并行开发，T7 联调）✅ 已完成（2026-10-23，含编码侧内联子 Tag/布尔语义/渐变规范化修复）
- 新增 `src/pagx/pag/PAGImporter.cpp`：LoadFromPAG、Tag 读取、未知 Tag 跳过、tagLevel 统计、
  上限检查（规范 §11）、引用回填、RetainedIdTable、`VerifyAndMake` 式校验（环/深度/类型）。
- 新增 `src/pagx/pag/tags/` 读取侧。
- **验收**：T3 兼容性用例（未知 Tag、补充 Tag、截断、超限、fuzz）；导出→解码→再导出字节一致。

### T7 运行时接入与往返等价（依赖 T5+T6）✅ 已完成（2026-10-23，e8d5f6692/7f3be7a04/114c69982/93f53c471：路径校验、全局字体注册、图片零拷贝渲染、GlyphRun/Font 编解码与渲染等价验收）
- `PAGScene::Make` 路径验证：解码文档不标记已布局，加载期排版（§4.2）；静态文件
  `hasContentChanged()` 语义（T9 静态部分）。
- `PAGFont::RegisterFont`/`SetFallbackFontNames` 全局注册接 `FontConfig`（接口草案 §3）。
- Image 零拷贝渲染路径（LayerBuilder 按逻辑尺寸/scaleFactor/偏移适配，§4.5）。
- **验收**：T1 渲染等价（PAGX XML 直接渲染 vs v4 渲染，同机同字体，静态样本）；嵌入字形样本
  （矢量 + 位图）渲染一致。

### T8 外部子 PAGX（依赖 T5+T6）✅ 已完成（2026-10-23，f066db59c：PAGXExternalFiles 递归加载、SubDocumentTable 内容去重编解码、逐引用独立实例）
- 新增 `src/pagx/PAGXExternalFiles.h/.cpp`（接口草案 §5）；递归加载、按绝对路径去重、
  引用链环检测、展开上限（1024 实例 / 1M 节点，规范 §11）。
- SubDocumentTable 编解码（规范 §6.5）、逐引用独立解码、包装 Composition 挂接
  （复用 `LoadExternalComposition` 逻辑）。
- **验收**：T11 引用与作用域用例（多级嵌套、同文件多引用、跨目录相对路径、缺失、环、超限）。

### T9 CLI 与工具（依赖 T5+T6+T8）✅ 已完成（2026-10-23，e2f62d056：--format pag/--strict/--report/--fallback、全命令 v4 输入）
- `src/cli/CommandExport.cpp`：`--format pag`、`--strict`、`--report`、`--fallback`。
- `CommandRender`/`CommandVerify`/`CommandInfo` 支持 v4 输入（verify 走 v4 解码 + PAGX 规则）。
- **验收**：CLI 全命令冒烟；report.json 结构与接口草案 §4 一致。

### T10 对比页与样本（依赖 T9；M0-7 样本就绪后扩展）✅ 已完成（2026-10-23，1833267e7：三列对比页、目录标签、diff 过滤排序、体积/告警卡片、render 外部文件加载）
- 新增 `test/gen_pagx_pag_comparison.sh/.py`（§10.3：三列、目录标签页、差异过滤、体积对比、告警数）。
- 接入 317 仓库样本 + `resources/pagx_to_pag/`。
- **验收**：对比页生成并通过人工抽查；静态样本 diff 阈值与 Baseline::Compare 一致。

### T11 测试收尾与全量回归（依赖全部）✅ 已完成（2026-10-24，7d6a52eaf/b5c00a8df：全节点类型编解码、mask 编号按引用计数修复、全量 2708/2709 通过）
- T1/T3/T4/T5(健壮性)/T9(静态)/T10/T11 用例并入 `PAGFullTest`；一期动效样本不进对比（§12.3）。
- `PAGFullTest` 全量通过 → 提交（分支 `feature/henryjpxie_pagx_to_pag`）。
- 截图基准变更走 `/accept-baseline`（仅用户执行）。
- **验收记录**：PAGFullTest 2709 tests，2708 通过 / 0 失败 / 1 skip（性能基线加载）；渲染等价 298/330
  bit-exact（90.3%），32 个差异全部归类（27 个 u8 量化、4 个文本/边缘 AA 级、fillrule 路径 0.05
  量化网格设计限制、modifier_trim_continuous 已知渐变插值微偏）；9 个二期语料按设计跳过。渲染期间发现
  并修复 mask NodeRef 两类深层缺陷（styles/filters/stops/selectors 未计数导致索引偏移；共享资源节点
  未按引用计数导致 complete_example 串色），screenshot.js 固化 chrome-headless-shell 免疫完整版
  Chrome 在 macOS 上的启动挂死。

## 二期预告（一期完成后按 §13.4 出接口与伪代码再开工）

Animation/StateMachine/时间线 Tag 写出（规范 §9.6/§9.7 已定稿）→ ViewModel/DataBind（§9.1/§9.8）→
扁平化中的关键帧变换回调实装 → 图片缩放含关键帧 → 嵌入文本修改回退（TextHolder）→
默认时间线统一（PAGViewer/pagx render 对齐 getDefaultTimeline）→ T2 动效行为等价用例。

## 二期完成记录（2026-10-24）

| 任务 | Commit |
|---|---|
| P2-1 动画核心编解码（AnimationBlock 480/489、AnimationObjectBlock 481/492、Channel 482 六种值类型列式关键帧）+ P2-2 RetainedIdTable（Layer/Animation/StateMachine/ViewModel/DataConverter id + 动画目标 id，解码注册 findNode） | 2095488ee |
| P2-3 Layer 时间线挂载（AnimationTimeline 366：playing/signed evaluationOffset；StateMachineTimeline 367） | 664f889c3 |
| P2-4 状态机编解码（StateMachineBlock 483、Input 484、StateRegion 485+490、State 486、Transition 487+491、Condition 488） | af897a716 |
| P2-5 ViewModel/DataBind（263 扩展 hasViewModel NodeRef、ViewModelBlock 299、Property 300 含内联 defaultImage、DataConverter 301、DataBind 544；Stripper 保留 VM/DC id） | 31bad4cfc |
| P2-6a 扁平化关键帧回调（PointKeyframeScaler：point{i}.x/y 关键帧随烘焙缩放同步变换） | 807662f56 |
| P2-6b 图片缩放含关键帧（AnimationScaleLookup：matrix/scale 通道的精确谱范数上界并入最大显示缩放） | fd5d7fcc7 |
| P2-6c TextHolder 嵌入文本回退（flush 改注册字体重排，修改不再被静默忽略） | 9306e1029 |
| P2-6d 默认时间线统一（C23：pagx render 与 PAGViewer 走 getDefaultTimeline，StateMachine 从初始状态帧步进推进） | f0bcfd598 |
| P2-7 动效等价验收（test/verify_animation_equivalence.sh） | 54456700c |

- **验收记录**：27/27 动效样本（animation 13 + state_machine 4 + viewmodel 5 + static_animated 5）
  × 3 个时间点（0/0.5/1.0s）XML 与 v4 渲染 **全部 bit-exact**；PAGFullTest 2717 tests，
  2716 通过 / 0 失败 / 1 skip（性能基线加载）；PAGViewer 独立构建（Qt 6.9）编译通过。
- 调试中修复的实现缺陷：动画字符串值/StringRef 预收集时序、RetainedIdTable 的
  registerNode 注册、point 通道名前缀长度、图片 maxScale 的 imgW=0 未布局退化、
  嵌入字形子集无字符映射的 TextHolder 静默忽略。

## 三期任务清单（旧 PAG 迁移，C28/C34；按 §13.4 先 P3-0 详细设计再编码）

依赖主线：P3-0 设计 → P3-1 规范扩展编解码 → P3-2 时间锁定 → P3-4/5 导入器 → 各能力块 → P3-13/15 验收。
文本（P3-3）、样式（P3-6）、特效（P3-7）可与技术主线并行。

### P3-0 详细设计（全部后续任务的前置）
- §7.4 能力清单逐项展开为详细设计（接口 + 核心伪代码），重点：时间锁定语义边界
  （子时间越界/负值/timeScale≤0 与旧 PAG 预合成对齐）、视频解码栈与 PAGScene 帧调度
  对接、3D 逐帧矩阵采样精度、运动模糊"前一帧变换"的获取路径。
- 旧接口对照表（§8.5 按类逐方法：已有对应 / 需新增 / 形式）。

### P3-1 规范扩展编解码
- LayerTransform（354）：startTime/timeScale 存在位 + 值（字段已定稿，三期才写）。
- Layer 变换属性（§6.2）：anchor/rotation/scale/skew/skewAxis（语义同 Group，x/y 为位置；
  优先级 matrix3D > matrix > 属性），XML 导入导出 + 节点字段 + 二进制 + 编号扩展。
- Path 值类型（§6.1）：ChannelValueType path=8 的变体与列式编码；命令序列相同逐点插值
  （对齐旧 PathData::interpolate），不同按 Hold + verify 告警。
- 反相遮罩（§6.4）：MaskType 新增 alphaInverted/luminanceInverted/contourInverted 编码映射。

### P3-2 时间锁定挂载运行时（§6.3，C26）
- PAGComposition 锁定模式：advance 不推进锁定实例；apply 按父时钟计算
  子时间 =（父时钟 − startTime）× timeScale 并定位；setCurrentTime 后下次 apply 级联。
- 默认时间线非 Animation 时锁定退回自行推进 + verify 告警。

### P3-3 文本属性通道扩展（§6.5）
- Fill/Stroke visible 通道；Text.tracking（1/1000em，与 letterSpacing 并存）；
  TextHolder 排版参数（textAlign/textAnchor/lineHeight/boxText 位置宽高/writingMode）
  运行时可写；baselineShift/firstBaseLine 通道；文本背景（backgroundColor/Alpha）。

### P3-4 旧 PAG 导入器骨架（src/pagx/pag/importer/）
- pag::File → PAGXDocument 基础映射：合成/图层树、Transform2D（含运动路径与自动朝向
  逐帧采样）、纯色/图片/文本/形状图层、图层属性（入出点 → visible Hold 关键帧、
  混合模式 Add → plusLighter）、字体表 → Font 节点。
- 无关键帧时生成同时长同帧率根 Animation（成为默认时间线，§7.3）。

### P3-5 旧动画与时间迁移
- 关键帧（含贝塞尔、空间插值）→ 通道；文本动画器/范围选择器 → TextModifier；
  倍速与起始时间 → 时间锁定；图层重映射（图片填充规则那份）。

### P3-6 图层样式扩展 + tgfx
- 描边、渐变叠加、外发光三个新样式；投影扩展（spread、blendMode）。

### P3-7 特效滤镜 8 项 + tgfx
- 动态拼贴、色阶、边角定位、鼓起、辉光、置换图、径向模糊、马赛克；
  全部参数动画通道（含颜色矩阵动画）。旧实现 src/rendering/filters/ 作移植参考。

### P3-8 遮罩路径列表 + 路径动画
- 与旧遮罩语义一致的路径列表（模式/反相/羽化/不透明度/扩展）；
  §6.1 路径动画在此消费。

### P3-9 3D、相机、运动模糊
- Transform3D/相机 → matrix3D 关键帧逐帧计算（投影与景深对齐旧运行时）；
  运动模糊属性 + tgfx 实现（当前帧与前一帧变换，无跨帧状态）。

### P3-10 视频与位图序列（§6.6，C28）
- ImageTable 媒体类型扩展（视频流/位图序列帧，多分辨率全保留）；XML 外部文件引用
  （.mp4 旁置）；复用旧解码栈（硬解/软解/预读/静态段跳过）；PAGScene 帧调度对接。

### P3-11 时间伸缩与音频
- TimeStretchMode（缩放/重复/反向重复）+ setDuration 文档级接口；
  AudioBytes 原样携带 + 起始时间读取接口。

### P3-12 可编辑内容与导出补全
- 旧 editableText/editableImage → ViewModel（§7.2 映射）；PAGXExporter 保留
  无对象 Animation（时长语义）。

### P3-13 迁移 CLI 与报告
- pagx import/export 支持 .pag 输入（内存直转，不落 XML 文本）；
  保真度报告（特性清单/等级/Unsupported 明细/体积/ViewModel 属性对照）；--strict。

### P3-14 旧接口对照实现（§8.5）
- 按 P3-0 对照表补齐需新增的 C++ 接口（Marker 除外，C35）。

### P3-15 迁移等价验收（C34：整数帧渲染一致，不接受近似）
- 仓库 166 个可解码旧样本逐整数帧新旧渲染对比；业务素材复核；
  可编辑内容用旧 replaceText/replaceImage 与新 ViewModel 赋值同等操作后对比。

## 三期完成记录

| 任务 | Commit |
|---|---|
| P3-0 三期详细设计（docs/pagx_to_pag_migration_design.md：模块边界/时间锁定语义/视频栈复用/旧接口对照表/5 项决策确认） | 110679f99 |
| P3-1 规范扩展（354 LayerTransform 分解字段 anchor/scale/skew/skewAxis/rotation + 反相遮罩 MaskType 3-5；格式层先行） | f57ada90c |
| P3-2 时间锁定运行时（propagateTimeLock：子时间线跟随父时钟，advance 跳过锁定挂载） | e88f2c2a6 |
| P3-4 旧 PAG 导入器骨架（LegacyPAGImporter：合成/图层树/PreCompose 时间锁挂载/根 Animation 兜底） | 6b83541e8 |
| P3-5 旧关键帧与动画迁移（range→point 关键帧、空间插值逐帧采样、入出点→visible Hold 通道） | c8963d75b |
| P3-3 文本属性通道扩展（tracking 1/1000em、TextHolder 容器通道、painter visible alpha 门控） | 89f3d007d |
| P3-6 图层样式扩展（StrokeLayerStyle 576/GradientOverlayStyle 577/OuterGlowStyle 578 + DropShadow spread；二进制与 XML 双向编解码，tgfx 着色器随后） | 2fdb48057 |
| P3-7 特效滤镜 8 项（MotionTile 579/Levels 580/CornerPin 581/Bulge 582/Glow 583/DisplacementMap 584/RadialBlur 585/Mosaic 586；二进制与 XML 双向编解码 + DisplacementMap 图层引用（NodeRef + @id 双链路）+ 全参数动画通道注册 + ColorMatrixFilter 补齐 binding 与 matrix[0..19] 通道；tgfx 着色器随后） | 091810d2a |
| P3-8 遮罩路径列表 + 路径动画（MaskPath 587：mode/inverted/feather/opacity/expansion + 内联 PathData；Layer.maskPaths 列表；§6.1 Path 值类型全链路——variant 第 9 备选、verb 相同逐点插值/不同 Hold、列式每帧完整 PathData、XML @id 引用 + id 合成、Path.data/MaskPath.path 通道；tgfx MaskContext 合成随后） | db0fe1b4c |
| P3-9a matrix3D 关键帧支持（ChannelValueType::Matrix3D=9 第 10 备选；逐元素插值；二进制每帧 16 float 列式 + XML "matrix3D" 类型；Layer.matrix3D 通道注册 + LayerRuntimeTarget 运行时写 tgfx setMatrix3D。**Transform3D/相机采样迁移暂缓**：旧运行时无 3D 求值实现可对齐（CameraLayer.cpp 仅 verify/bounds、transform3D 图层旧渲染直接跳过），公式确认后落地；渲染走 PAGX setMatrix3D） | f0bf078c9 |
| P3-10a 序列条目格式层（§6.6.1 定稿：mediaType 2/3 容器——帧率/尺寸/编码头/帧列表原样；SequenceCodec 编解码 + ImageTable 扩展 + Image 序列元数据（mediaType/frameRate/frameCount）+ ImageProcessor 序列分支（不重编码）+ XML 结构保真（决策 5）。迁移器旧 VideoComposition/BitmapComposition 对接与 SequencePlayer 运行时帧调度随 P3-10b） | 5a8d6840a |
| P3-10b 序列合成迁移（LegacyPAGImporter：VideoComposition/BitmapComposition → 序列 Image 全量入表（多分辨率保留、主序列=sequences.back() 对齐 Sequence::Get、渲染层=全幅 Rectangle+ImagePattern Stretch）；ImageProcessor 序列 Image 无条件收集；ExportToPAG 序列 Image id 合成保 XML hop 元数据。SequencePlayer 运行时帧调度/软解对比随 P3-10c） | fa8d6a847 |
| P3-10c SequencePlayer 帧调度（src/pagx/runtime/SequencePlayer：容器重建旧内存结构复用 VideoSequenceDemuxer+VideoReader/BitmapSequenceReader，frameAt 惰性 SequenceFrameGenerator；LayerBuilder 序列 pattern 注册 RuntimeBinding.sequencePatterns；PAGScene::draw 前 flushSequenceFrames 递归整树按默认时间线刷帧。bitmap 与视频链路全验证（contentA/diff>0，双管线对照一致）；**视频修复**：frameAt 补 makeRGBAAA 双区重采样（根因：缺 RGBAAA 时 I420 双区帧无法采样，软/硬解均连锁失败，修复后两解码路径均正常）） | ec941f1a1 |
| P3-10c-fix 视频帧 RGBAAA 修复（SequencePlayer::frameAt 对视频帧做 makeRGBAAA(w,h,alphaStart) 双区重采样对齐 SequenceInfo::MakeSequenceImage；解码器回退平台默认工厂列表（硬解优先+软解兜底）对齐旧管线；VideoSequenceFrameSchedulingTest 新旧双管线对照回归） | 75b29ca33 |
| P3-11 时间伸缩与音频（TimeStretchMode + PAGScene::setTimeStretchMode/setDuration：Repeat/Inverted→运行时 Loop/PingPong 覆盖、None→Once、Scale→rate 注入；Composition.audioData/startTime：ImageTable mediaType=4 + CompositionAudio 588 + XML data URI + 迁移帧转秒） | eff85bca7 |
| P3-12 可编辑映射（TextLayer→TextBox+Text 运行时排版、ImageLayer→Rect+Fill+ImagePattern 静态迁移补齐；editableTexts/editableImages→ViewModel text<i>/image<i> + DataBind 按目标 composition 挂载（scope 规则）+ VM 挂文档级；**修复既有 bug**：StringTable 预收集漏 Composition::dataBinds 致 v4 往返 bind 字段全空；PAGXExporter 保留无对象 Animation；运行时编辑渲染写回链（value→flush→TextHolder reshape→像素）为尾项待收） | 75daa214e |
| P3-13 迁移 CLI 与报告（pagx import 接受 .pag 输入走 LegacyPAGImporter 内存直转，默认输出 v4 .pag（决策 5）、显式 .pagx 扩展走 XML；--report Markdown 保真度报告：迁移等级/特性清单/issue 明细/体积对比/ViewModel 属性对照；--strict 有 issue 退出码 2；新 src/cli/MigrationReport） | 5761e7bbe |
| P3-14 旧接口访问器（PAGLayer visible/alpha/matrix/matrix3D 运行时读写直写 tgfx 不改文档节点；PAGComposition setTimeLock 运行时改时间锁定 + duration + audioData/audioStartTime 读取；PAGScene::duration 对齐旧 PAGFile::duration；excludedTargets 因 PAGX mask 模型不同不适用已记录；PAGPlayer 外观类四期） | 5419ad698 |
| P3-15 对比框架（PAGMigrationEquivalenceTest：282 样本遍历、166 可解码（对上设计文档）、双管线逐帧严格像素对比；首轮采样门限 6 帧/样本；首轮基线 41/166 全帧通过、帧级 333/1131（29.4%），107 样本全帧空白（P3-5 遗留 shape 覆盖）+ 18 部分差异为收敛清单；全帧扫与可编辑同操作对比随后续收敛逐步收紧） | 1ee22ddd3 |
| P3-15 shape-coverage round (6 element types + gradient helper; scale 0-1 semantics fix; layer width/height seeds; 47/166 fully passed, blanks 108 to 90) | c1f7b331f |
| P3-15 blend-convergence round (z-order reversal AE-first-on-top to PAGX-first-on-bottom; ImageBytes V2/V3 anchor/scaleFactor mapped to the pattern matrix with ScaleMode None so stripped bitmaps land where the legacy renderer put them; layer blendMode migration incl. Add to PlusLighter; in-point leading false keyframe in the visibility channel; **composition animation attached AFTER the layer loop** — the pre-loop lookup always missed the lazily created animation so composition timelines never spawned and no channel ever applied; comparison framework fixes: PAGFile duration is microseconds not frames, and the migrated clock now drives animation advanceAndApply instead of a never-applied setCurrentTime; 70/166 fully passed, frames 49.2%) | f9c1908d9 |
| P3-15 timestretch round (sequence rect centered on the composition — the default (0,0) position only covered the bottom-right quadrant; diagnosis on repeat.pag: sequence decode/RGBAAA/demuxer chain verified correct end-to-end (936x832 dual-region decode, makeRGBAAA display 468x832, container alphaStart preserved), both pipelines' sequence frames match at ~88%; the residual 100% diff isolates to the static image layers sandwiched between the two video precomposes — needs per-layer solo rendering next) | f9c1908d9 |
| P3-15 group-transform round (ShapeGroup transforms were dropped entirely: static fields now convert onto the Group node with animated channels joining the composition animation, and static translation-only transforms bake directly into child geometry because the PAGX Group position is layout-driven rather than a content-space translate; gradient alpha-stop folding now interpolates the opacity ramp at each color stop instead of multiplying qualifying stops; track-matte source layers skipped with a reported issue instead of double-rendering; 71/166 fully passed, frames 50.6%) | da8954e33 |
| P3-15 keyframe-end round (the legacy keyframe is a SEGMENT [startTime, endTime] interpolating startValue to endValue; the converters copied only each segment start point, so the FINAL segment end value had no successor key and the evaluator clamped all later times to the last start value — the eventMyHero photo rendered at scale 2.13 instead of the in-segment 0.72; ConvertFloatChannel, ConvertOpacityChannel and ConvertPointChannel now append the final segment endpoint and the spatial per-frame sampling includes the last frame; 71/166 fully passed, frames 51.7%, repeat.pag 99.99 to 98.65) | eecb4aaba |
| P3-15 repeat-mode round (analysis: the legacy Sequence::toSequenceFrame mapping compFrame x seqRate/compRate is mathematically equivalent to the migrated compositionTime x seqRate, and the root timeline needs no repeat remapping when the file duration equals the composition duration; repeat.pag's residual 98.65% isolates to a DECODER-LEVEL color difference — the same H.264 frame decoded through the two pipelines' VideoReader instances differs on ~12% of pixels by an average (-12,-7.5,-5.6) RGB delta, likely hardware-vs-software decoder selection or decode-order timing; recorded as a known decoder difference requiring a dedicated decode-path investigation) | d8413fb32 |
| P3-15 decoder round (the decoder is exonerated: the same H.264 frame decoded through both pipelines' VideoReader instances is 100% byte-identical; repeat.pag's residual color shift isolates to the RESAMPLE CHAIN — the legacy ImageProxyPicture pre-rescales downsampled (>0.7x) draws through RescaleImage into a mipmapped surface at the target scale before drawing, while the migrated ImagePattern samples the full-resolution texture directly, producing a uniform small RGB delta on downscaled video/image content; recorded as a rendering-architecture difference needing a bit-exact resample replication if the strict gate must cover it) | ef3406355 |
| P3-15 track-matte round (matte sources convert as regular layers and the matted layer references them through the PAGX per-layer mask by id resolution, with TrackMatteType mapped to MaskType Alpha/AlphaInverted/Luminance/LuminanceInverted; the source stays in the layer list preserving the sibling coordinate space and the renderer suppresses its drawing via maskOwner; LumaTrackMatte 63.41 to 37.11, no regressions, full suite green; the residual AlphaTrackMatte 17.13 isolates to ONE pair — a 6-stop linear GradientFill masked by a solid-fill rectangle renders as a flat constant color under the mask, next step is a no-mask A/B to split mask compositing vs gradient rendering) | 5ad4c4c4d |
| P3-15 composition-id round (migrated compositions now carry deterministic pag-legacy-comp-N ids — previously the empty id made the XML writer drop the composition attribute AND the whole composition subtree from every CLI .pagx output and v4 binary hop, so round-tripped files lost all nested content; verified the composition with its layers survives the XML hop; a cross-child runtime mask resolution was attempted and REVERTED: BuildLayerInto runs per-layer contexts so sibling masks cannot resolve in the runtime path, and forcing composition-level resolution regressed chaoli and vt_tianmigaobai because an empty or out-of-window matte must hide the target entirely (legacy TrackMatteIsEmpty semantics) instead of applying an empty mask) | 7d144915e |
| P3-15 matte-emptiness round (TrackMatteIsEmpty semantics from legacy Modifier::MakeMask: a NON-inverted matte whose source left its in/out window skips the target entirely, so the target's visible channel is rebuilt over the intersection of both windows; an INVERTED matte with an empty source keeps the target fully visible, which the runtime already produces when the mask layer turns invisible and tgfx drops the mask, so inverted mattes keep their own window; the first cut intersected both directions and regressed the inverted-matte samples test.pag and editableTextLayer at frame 41, the asymmetry fix restored them; baseline held at 71/166 and 51.8 percent with the full suite green) | d662a18da |
| P3-15 gradient-bake round (root cause of the mask-flattened gradient: the ShapeGroup translation bake moved the geometry (rect/ellipse/polystar/path positions) but NOT the gradient anchor points, and a fitsToGeometry=false gradient lives in the same space as the geometry, so the gradient stayed behind while the geometry moved and degenerated to a flat first-stop color; Fill and Stroke color sources now bake alongside the geometry (LinearGradient start/end, RadialGradient center), AlphaTrackMatte 17.13 to 2.47 percent, no other sample changed, full suite green) | 178143195 |
| P3-15 luma-diagnosis round (LumaTrackMatte 37.11 diagnosis: mask wiring CONFIRMED functional - clearing the mask yields 100 diff; remaining 37 is in the luma VALUE computation - uniform RGB shifts with alpha identical, consistent with weaker mask strength; both paths nominally BT.709 so candidates are the dstColorSpace conversion before the luma filter, gamma vs linear luma, or premultiplication in the shader pipeline; mask source gradient fully opaque excluding alpha-interaction; needs tgfx-level luma A/B next round) | DIAGNOSTIC |
| P3-15 luma-ab round (tgfx A/B: legacy matrix vs tgfx Luma produce IDENTICAL alpha on 6 swatches, filter NOT the cause; mask source layer solo: legacy 27120 px vs migrated 0 px, mask content renders COMPLETELY EMPTY in the migrated pipeline even when forced visible, dropping the mask and letting the target render unmasked - THIS is the root cause of the 37.11; prime suspect is the LAYOUT interaction with the baked group whose rect spans negative origin) | DIAGNOSTIC |
| P3-15 layout-diag round (runtime layout A/B on the mask source: the tgfx layer bounds=(-270,-520,273,521) matrix tx=360 ty=640 visible=1 alpha=1 are ALL CORRECT, the content geometry is right; the 0-pixel solo render is EXPLAINED by maskOwner - tgfx drawChildren skips mask owner layers so they can NEVER render as regular content, the solo test methodology is INVALID for mask sources; the mask renders through the internal getMaskPicture path instead; combined with the earlier luma filter A/B (identical alpha) the luma MASK is WORKING CORRECTLY and the 37.11 diff is now attributed to the KNOWN resample chain difference on the PIC.JPG image content under the mask - uniform +23R +9G RGB shifts with identical alpha match the resample signature exactly; LumaTrackMatte reclassified from luma-mask issue to resample-chain known difference) | DIAGNOSTIC |
| P3-15 timestretch-final round (DEFINITIVE diagnosis: time mapping VERIFIED CORRECT - legacy currentFrame and migrated composition animation currentTime match exactly at all sampled frames (0 30 60 90), both pipelines self-diff f0-vs-f60 at ~23.5 percent showing identical animation evolution; the 98.65 percent diff is the KNOWN RESAMPLE CHAIN architecture difference - pixel analysis shows uniform +10 to +14 RGB shifts with identical alpha across the entire canvas (every pixel of the video and image content shifts slightly so the strict pixel-exact comparison counts ~99 percent as different), the same signature as LumaTrackMatte and repeat.pag; the timestretch samples repeat repeatInverted scale are RECLASSIFIED from time-mapping issues to resample-chain known differences; the test_ variants at 100 percent likely have additional content differences on top of the resample baseline; no code change needed - the time stretch migration from P3-11 is working correctly) | DIAGNOSTIC |
| P3-15 blur-filter round (ConvertLayerEffects implemented: legacy layer effects were silently dropped; FastBlurEffect now maps to BlurFilter with the legacy /2 radius division (GaussianBlurFilter.cpp divides blurriness by 2 before tgfx) and repeatEdgePixels mapping to Clamp tile mode (not Repeat); hooked into Image and Shape layer branches; GaussBlur_Static 99.97 to 51.09, GaussianBlur_NoRepeat_Clip 69.03 to 58.46, fastblur samples show the blur now actually applies; remaining ~51 percent likely from filterScale/sourceScale scaling in the legacy renderer plus the known resample chain; full suite green) | 282bb3367 |
| P3-15 blur-scale-diag round (filterScale theory ELIMINATED for GaussBlur_Static: the image is at native 720x1280 with layer scale 1.0 so filterScale and sourceScale are both 1, no scaling compensation is needed and the /2 radius conversion of 40 to 20 is correct; the residual 51.09 percent consists of 37334 differing pixels of which 99 percent are small (sum RGB diff <= 10) and ZERO are big (> 30), isolating to the tgfx BlurFilter Layer-filter path vs the legacy ImageFilter Blur image-filter path having slightly different blur kernels or edge handling - an implementation-level architecture difference in tgfx, same class as the resample chain difference; reclassified from parameter mismatch to tgfx blur implementation path difference) | DIAGNOSTIC |
| P3-15 glow-filter round (3fba260d4: GlowEffect now migrates to the GlowFilter node with threshold/radius/intensity carried 1:1 and animated properties joined to the composition animation with raw keyframe times - legacy effect keyframes are composition-absolute because FrameCache evaluates at contentFrame + startTime; the renderer side adds a GlowLayerFilter (tgfx::LayerFilter) that reuses the legacy GlowBlur/GlowMerge RuntimeFilter shaders pixel-identically, enabled by a cache-less fallback in pag::RuntimeFilter that self-owns its pipeline resources (the PAGX layer path has no RenderCache) and rebuilds them when the GPU changes; Glow.pag 100.00 to 0.49 percent - the residual is rounding-level, same class as fastblur; outerglow.pag is the OuterGlowStyle layer style, not this effect; no regressions, full suite green) | PENDING |
| P3-15 mosaic-filter round (b982e1d9f: MosaicEffect now migrates to the MosaicFilter node with block counts and sharpColors carried over and uint16 block channels converted value-by-value into float channels; the renderer reuses the legacy mosaic RuntimeFilter shader through a new MosaicLayerFilter, VERIFIED BIT-EXACT at the native 720x1280 surface (0.00 percent diff); the harness residual 99.67 isolates to a filter-EXTENT architecture difference: both test pipelines render 1:1-cropped onto the scaled surface (PAGPlayer setMatrix Identity sets scaleMode None, PAGScene draws 1:1), and the legacy useParentSizeInput feeds non-visibleAreaOnly effects a PARENT-COMPOSITION-SIZED input texture so the mosaic grid stays anchored to the composition (80px blocks), while the tgfx layer filter quantizes over the visible-clipped layer render (22.5px blocks) - the grid re-anchors, an inherent tgfx offscreen-clipping semantics difference that only manifests when the surface crops the composition; no regressions, full suite green) | PENDING |
| P3-15 radialblur-filter round (3e4cb6e98: RadialBlurEffect migrates to the RadialBlurFilter node with amount/center/mode/antialias carried over, the spatial center routing through ConvertPointChannel per-frame sampling, and the renderer reusing the legacy radial blur RuntimeFilter shader; PLUS a significant migrator bug fix: ConvertComposition assigned state->currentAnimation only AFTER ConvertLayer returned, so the FIRST layer of every composition silently dropped its filter and shape-group animated channels (null currentAnimation) - the assignment now runs before the layer loop; matched-frame A/B verifies the blur is essentially bit-exact (0.15 percent at frame 7 with both pipelines on the same frame); the harness residual 83.77 isolates to a FRAME-QUANTIZATION semantics difference: the legacy player floors sub-frame times (progress 8/48 renders frame 7, amount 7.447) while PAGX channels evaluate continuously (7.99999 frames, amount 8.510) - a proposed harness fix is centering the legacy progress at (frame+0.5)/totalFrames so both pipelines target the same integer frame; RadialBlur 1/6 to 2/6 frames, no regressions, full suite green) | PENDING |
| P3-15 frame-align round (4879cc99f: harness fix: the comparison loop now aims both pipelines at the SAME integer frame - the legacy player at the center of the frame period (progress derived from the true duration, not the rounded totalFrames) because it floors sub-frame times, and the migrated clock ceiled ABOVE the frame boundary because its sequence players floor while its channels evaluate continuously; the first attempt centered the legacy progress on the totalFrames fraction and regressed six video samples whose sequences aligned at frame-1 on both sides, the ceil companion recovered them; net effect zero new failures, 13 samples improved, displement_map_video_scale fully passes, yanzhixiu 99.62 to 2.07, RadialBlur 83.77 to 0.03, error_2 7.39 to 1.57, vt_haokan 25.88 to 0.47; 72/166 fully passed, frames 543/1032 = 52.6 percent, full suite green) | PENDING |
| P3-15 cornerpin-filter round (d8432e6b8: CornerPinEffect migrates to the CornerPinFilter node with the four spatial corners routed through ConvertPointChannel per-frame sampling; the renderer wraps the legacy perspective quad warp RuntimeFilter through a tgfx::LayerImageFilter so the filter bounds delegate to the RuntimeFilter quad bounding box exactly like the legacy transformBounds; frame-0 identity-warp and frame-30 half-converged A/Bs verify the geometry and animation are correct (0.26 / 0.30 percent), with the residual isolating to a FILTER INPUT SCALE semantics difference: the legacy renders the effect input at the canvas content scale WITHOUT the layer transform (useParentSizeInput 0, sourceScale 1.0 while the layer itself is scaled 1.02) and applies the layer matrix after the filter, while tgfx bakes the layer-total scale into the layer-filter input - the resample ordering differs so content edges land on slightly different AA (193 boundary pixels); same architecture class as the blur kernel and resample chain differences; cornerpin/cornerpin-bulge 0.20 to 0.26 - the warp now actually applies instead of being dropped; no regressions, full suite green) | PENDING |
| P3-15 bulge-motiontile round (89b03e87c: BulgeEffect and MotionTileEffect migrate to their filter nodes with all animated properties joining the composition animation; both renderers reuse the legacy RuntimeFilter shaders cache-less, with layer-level filterBounds replicating the legacy transformBounds - the MotionTile output-percent expansion applies to BOTH map directions (a tiling filter always needs its full source, which the expansion preserves) and the Bulge reverse pass returns the rect unchanged (the bulge samples inside its own region); motiontile.pag 1.52 to 1.75 with the tile now correctly applied, Bulge.pag holds at 0.49 (its residual is the image resample class - the bulgeHeight animation 0 to 1 to -0.57 to 0 is verified migrating); motiontile_blur 0.08 to 98.76 RECLASSIFIED into the useParentSizeInput extent class (the legacy feeds the tile a comp-sized unclipped 858x1250 texture while tgfx clips the layer-filter input to 400x557, shifting the tileCenter uv anchor 0.5 to 0.458) - the pre-conversion 0.08 was a coincidental match with the effect dropped (the tile is nearly invisible in the cropped view); no other sample changed, full suite green) | PENDING |
| P3-15 levelsindividual round (781ca0af1: LevelsIndividualEffect migrates to the LevelsIndividualFilter node with all twenty global/red/green/blue level values carried 1:1 in 0-1 space and animated properties joining the composition animation; the renderer reuses the legacy levels RuntimeFilter shader cache-less; FOUR samples collapse from 100 percent to 0.49 (RGB, test_repeat, test_repeatInverted, test_scale - they carry levels effects that were silently dropped before, and the residuals join the rounding class), LevelsIndividualFilter.pag holds at 8.77 with 4/6 frames (its residual is not the levels remap), no regressions, full suite green) | PENDING |
| P3-15 huesaturation-node round (58fbadd42: new HueSaturationFilter PAGX node lands the full stack: NodeType enum, node class (seven values - the legacy per-channel-control slots flatten to the selected values because the legacy renderer only consumes the selected slot), XML read/write, v4 binary tag #589 read/write, animation channels with the colorize trio animatable; the migrator maps HueSaturationEffect and the renderer wraps the legacy HSV-space RuntimeFilter shader cache-less; PLUS two migrator branch fixes discovered on the way: ConvertLayerEffects was only called from the Shape and Image branches, so effects on TEXT and PRECOMPOSE layers were silently dropped - both branches now convert effects (the runtime applies node->filters uniformly); HueSaturation.pag 6.28 to 3.45 with the effect now applying over its precompose layer, no regressions, full suite green) | PENDING |
| P3-15 brightnesscontrast-node round (df8bcdc8a: new BrightnessContrastFilter PAGX node #590 lands the full stack following the hue saturation pattern: NodeType enum, node class (brightness/contrast plus the data-only useOldVersion flag the legacy renderer ignores), XML read/write, v4 binary tag read/write, animation channels, migrator mapping and a cache-less renderer wrapping the legacy RuntimeFilter shader; BrightnessContrast.pag 6.28 to 2.39 with the effect now applying over its precompose layer, no regressions, full suite green) | PENDING |
| P3-15 displacement-map round (ea84caec0: DisplacementMap effects now migrate with offscreen map rendering - the renderer re-renders the map layer content into the shader extra input each frame and feeds the target FULL unclipped content (the legacy never clips the effect input), enabled by a new tgfx LayerFilter inputOrigin parameter threaded through OffscreenRenderer and applyFilters so geometry filters recover the layer-local anchor of their input image (tgfx side on branch feature/henryjpxie_layer_filter together with Layer::makeContentPicture); the migrator animates the enum selectors (behavior and both sources) as Int Hold channels with a trailing end key for the final segment, plus effectOpacity and an enabled Hold channel replicating the legacy visibleAt gating (map layer time window and const-source collapse); the enum properties cast through their uint8 storage type (the codec assigns AnimatableProperty<unsigned char> into typed fields) and the visible channel now respects isActive so inactive displacement sources stay hidden while track matte sources keep their window; displacement_map_shape 33.99 to 4/6 frames at 0.28 rounding residual, LevelsIndividualFilter fully passes from the visibility fix, 73/166 samples and 549/1032 frames (53.2 percent); DisplacementMap.pag verified uniform-identical between pipelines with a +-1-only pixel residual (resample chain class); ShaderToyFilterFull reclassified - its custom extension tags are invisible to both standard pipelines and the 2.22 residual is +-1 rounding, not a GLSL conversion gap; XML round-trip for the new int and bool channels verified bit-identical; no regressions, full suite green) | ea84caec0 |
| P3-15 gradient-overlay round (GradientOverlay layer styles now migrate: the migrator maps the legacy style to the GradientOverlayStyle node from every layer branch (the Solid branch also gains the ConvertLayerEffects call it was missing), the gradient rides a shared Gradient resource with neutral unit geometry, and the animated properties (opacity alpha, angle, scale, offsetX/offsetY, reverse hold) join the composition animation; GradientOverlayType extends to Reflected and Angle so the legacy fill types map exactly (XML attribute values reflected/angle, v4 binary unchanged - the enum already rides a uint8) with only Diamond approximated as linear; the renderer adds a GradientOverlayLayerFilter (tgfx LayerFilter) replicating the legacy GradientOverlayFilter shader chain verbatim - angle-dependent scale factor, gradient matrix, opacity DstIn, content-alpha mask and blend-mode composite - evaluated in the CONTENT coordinate space (contentBounds equals the legacy source raster, keeping the conic sweep aspect exact) and mapped onto the input pixels through per-axis ratios; gradient_overlay 80.30 to 0.99, GradientOverlay 100.00 to 4.12, GradientOverlayScale 2.33 to 2.29 (residuals are the legacy 720x1280 style raster downsampled to the surface vs the direct surface-resolution render - interpolation class); no regressions, full suite green) | PENDING |
| P3-15 drop-shadow round (DropShadow layer styles now migrate: the migrator maps the legacy style to the DropShadowStyle node from ConvertLayerStyles (the tgfx-native rendering path and channel binding already existed in LayerBuilder) - the polar angle/distance pair flattens into offsetX/offsetY through the legacy angle-180 convention, size maps to blurX/blurY through size*(1-spread)/2, opacity bakes into the color alpha, and the animated properties ride per-frame-sampled channels (polar offsets sampled per frame because linear interpolation between polar samples would bow the shadow path, size through the static spread factor, color plus opacity as one color channel); nine corpus samples carry drop shadow styles and the conversion is net-positive: an A/B kill-switch on video_composition_filter (17 styles) measures mean error 12.33 to 10.86 with 1475 pixels improved vs 109 worsened, guide_hand_drag 14.98 to 13.52, zero regressions; the strict per-pixel metric rises slightly on two samples (video_composition_filter 26.13 to 27.90, LogoMipmap 77.86 to 80.43) because the shadow now covers more area with small interpolation errors - the legacy renders the style at its source-scale raster through SolidStrokeFilter composition while tgfx grows a spread shape and blurs at contentScale, so spread>0 keeps the legacy blur but only approximates the solid core growth (spread=0 is the exact path); DropShadow.pag itself stays at 33.08 because its text layers render at wrong positions in the migrated pipeline regardless of styles - a text-class issue blocking that sample, not a shadow defect) | PENDING |
| P3-15 displacement-binary-ref round (v4 binary export kept stripping invisible displacement map sources: Stripper::RemoveDeadLayers only protected animation targets and mask references, so a visible=false map layer was erased from the composition before emission while the filter kept pointing at it - the exported binary lost the whole map subtree (its composition, children and animations), the map NodeRef degraded to null at decode and every displacement effect in the DEFAULT v4 .pag output rendered as a no-op (displacement_map_shape 1.02 percent pixel diff vs the XML render; the XML path was unaffected). Fix: the dead-layer predicate now also keeps layers referenced by any DisplacementMapFilter::displacementMapLayer (IsUsedAsDisplacementMapAnywhere, mirroring the mask check). Verified: the local PAGX-vs-PAG-v4 comparison page section ptp_migration_styles goes 11/12 to 12/12 identical, the default pagx import output renders bit-identical to the XML render (0/777600 pixels), the round-tripped document carries all 36 nodes with the map reference resolved, and the full suite stays green; discovered through the T10 comparison page - the tag-stream probe showed the map LayerBlock missing from the binary while the reader-side channel decode was flawless) | PENDING |
| P3-15 outer-glow round (OuterGlow layer styles now migrate: the migrator maps the legacy style to the OuterGlowStyle node from ConvertLayerStyles - spread/range scale from the legacy 0-1 percent into the node's 0-100 percent space, opacity to the separate alpha, and the animated properties (alpha, size, spread, range, color) join the composition animation with the percent channels preserving their interpolation and easing; the renderer adds an OuterGlowLayerFilter (tgfx LayerFilter) replicating the legacy OuterGlowFilter chain verbatim - the spread portion dilates through the cache-less SolidStroke RuntimeFilter (spreadSize = spread*size/range, Thick mode switching on the raw layer-unit size), the remainder blurs through DropShadowOnly (sigma = size*(1-spread)/range/2), composed stroke-then-blur - and composites the halo BEHIND the content like the legacy LayerStylesFilter blow-position order, with filterBounds delegating to the composed image filter for both map directions so the offscreen input grows the halo band; outerglow.pag glow verified by distance-from-text profile analysis: the intensity-vs-distance rings match legacy within a few units at every distance (d=0: 253.8 vs 253.5, d=30: 25.1 vs 25.1), so the glow rendering is equivalent and the sample's 66.44 residual is the TEXT class (its four text layers render misplaced and re-wrapped in the migrated pipeline, the same blocker as DropShadow.pag); the local comparison page section ptp_migration_styles goes 12/12 to 13/13 identical (outerglow added, the style survives the v4 binary roundtrip bit-exact); no regressions, full suite green) | 8d55f1e62 |
| P3-15 stroke round (Stroke layer styles now migrate: the migrator maps the legacy style to the StrokeLayerStyle node from ConvertLayerStyles - the position maps by NAME because the legacy enum orders Outside/Inside/Center while PAGX orders Outside/Center/Inside, and the animated properties (alpha, size, position as an Int Hold channel read through the codec's uint8 DiscreteProperty storage, color per-frame) join the composition animation; the renderer adds a StrokeLayerFilter (tgfx LayerFilter) replicating the legacy StrokeFilter chain verbatim - Outside dilates the full content alpha through the cache-less SolidStroke RuntimeFilter, Center/Inside shrink the spread by 0.4/0.8 and run an AlphaEdgeDetect pre-pass with the original image feeding the shader's content-preservation sampler - and the merged stroke+content output REPLACES the layer content drawn as one image at the style opacity (the legacy draws above-position styles instead of the source), with filterBounds computing the position-scaled outset for both map directions; stroke.pag moves 8.50 to 15.71 percent overall - the metric RISES because the strokes now actually render attached to the mis-laid text (the same covers-more-area phenomenon as the drop shadow round), while the residual isolates to the TEXT class: the three stroked TextBoxes re-wrap and shift about 130px down in the migrated pipeline (white glyph pixels nearly equal at 59741 vs 58917, so the same glyphs render), and the stroke implementation itself is verified three ways - the translation-invariant distance-ring profile of yellow stroke coverage around the white glyph cores matches legacy within 0.04 at every ring (both fall to zero past 7px for the size-5 stroke), solo renders of each stroked layer show the correct inside/center/outside semantics, and the bottom image band renders pixel-identical; the local comparison page section ptp_migration_styles goes 13/13 to 14/14 identical (stroke added, the style survives the v4 binary roundtrip bit-exact); no regressions, full suite green) | 916fb6e3e |
| P3-15 text-anchoring round (Root cause of the text-class failures: (1) legacy point text anchors the first line BASELINE at layer-local y=0 while the PAGX half-leading linebox model puts it at ascent+halfLeading below the origin - every text dropped ~ascent (stroke.pag measured 124px at fontSize 120); (2) justification was not migrated so CenterJustify point text rendered Start-aligned from the anchor; (3) leading was not migrated (legacy auto = 1.2x fontSize vs PAGX font metrics ~1.35em); (4) vertical direction was not mapped to writingMode. Fixes: the migration now sets text->baseline=Alphabetic for point text (the TextBaseline::Alphabetic mechanism already existed for SVG), maps justification to textAlign (NaN-width boxes turn Center/End into the legacy -lineWidth/2/-lineWidth drawX offsets; Full variants approximate as Justify), maps lineHeight=leading||1.2xfontSize, tracking (same 1/1000 em convention) and vertical writingMode; TextBox::MakeTextLayoutParams now forwards the first child Text's baseline mode into the box layout at all three call sites. PLUS the critical renderer bug found by printf-instrumenting the layout: TextBox::setLayoutSize SETTLES auto axes by writing the measured preferredWidth into layoutWidth, and the re-typeset then treats the point box as fixed-width - centering-within-origin silently became centering-within-measurement (the A/B of textAlign=center vs start rendered IDENTICAL); updateLayout now keeps the typeset box NaN on content-sized axes (no authored size, percent, or opposite-edge constraints), preserving origin-anchored alignment for auto boxes without touching constraint-sized ones. Results: stroke 15.71 to 3.10, DropShadow 33.08 to 21.82 (residual = the documented shadow spread-core approximation), outerglow 66.44 to 36.87 (7.84 percent at full-resolution threshold-8; the test-scale strict metric amplifies the glow gradient plus a ~2px ascent-rounding residual), test_VerticalText 27.25 to 23.99, 73/166 samples fully pass; remaining text-class residuals are per-subsystem (vertical layout geometry, text animators, text paths, shadow spread core); no regressions, full suite green 2740/2741) | 4319416c6 |
| P3-15 shadow-spread round (The drop shadow spread core now renders: a new DropShadowLayerFilter (tgfx LayerFilter) replicates the legacy DropShadowFilter chain - the spread converts the stored pre-spread blur into a solid core (core = 2*blur*spread) grown through the cache-less SolidStroke runtime shader at the shadow offset, composed with the remaining blur falloff (sigma = blur*(1-spread)) and composited BEHIND the content - while spread-less styles keep the proven tgfx native binding (the native DropShadowStyle derives spread from the vector contour, which text and group layers cannot provide, skipping the shadow entirely, and it also applies the style blend mode that the legacy ignores); the importer now stores the PRE-SPREAD blur (size/2, keeping the size recoverable at every spread - a baked size*(1-spread)/2 blur loses it at spread 1 where the shadow is a pure solid silhouette) and TWO migration bugs surfaced: the spread was stored without the x100 percent conversion (1 read as 1 percent - harmless while the renderer ignored the field), and Multiply-style blend modes would have erased the shadow against the empty recording composite, so the filter draws SrcOver exactly like the legacy; animated spread now rides a dedicated channel instead of the unsupported warning. Verified structurally: the shadow dark-pixel count goes 8090 to 177910 against the legacy 183420 (solid cores restored), full-resolution diff 38.25 to 35.19 percent; the strict per-pixel metric TEMPORARILY RISES (DropShadow 21.82 to 24.57, LogoMipmap 80.43 to 85.23) because the sharp-edged solid cores amplify the KNOWN +-3px text-position residual (uniform baseline shift, suspected font ascent metric-source difference - the same class blocking stroke/outerglow residuals); fixing that text rounding is the prerequisite for these samples to converge; no regressions, full suite green 2740/2741) | PENDING |

- **验收记录（P3-6）**：LayerStyleExtensionsRoundTrip 4 样式全字段往返一致（含
  GradientOverlay 内联渐变子 Tag）；PAGFullTest 2724 tests，2723 通过 / 0 失败 / 1 skip
  （性能基线加载）。
- tgfx 渲染（三个新样式着色器、反相遮罩 ColorMatrix、baselineShift/firstBaseLine/文本
  背景）与 P3-7 特效滤镜一起落地；P3-5 遗留的旧素材迁移渲染空白问题在 P3-15 验收时
  逐帧对比收敛。

## 四期预告（平台接入，最后一期）
平台接口设计（§8.2 一次覆盖一至三期全部能力）→ Android/iOS/macOS/OHOS/Web/C API 接入
（播放统一、对象模型分离、ViewModel、字体注册、三期新增接口）→ 发版前全量回归（C33）。
