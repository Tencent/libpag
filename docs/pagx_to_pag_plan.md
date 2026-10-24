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
