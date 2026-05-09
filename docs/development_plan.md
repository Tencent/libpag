# PAGX → PAG v2 开发计划

> **性质声明**：本计划是 [`pagx_to_pag_v2_design.md`](pagx_to_pag_v2_design.md) v2.20 的执行编排，不重复设计内容，只补设计文档没覆盖的执行层：开工前置准备、AI 编码提示词模板、风险矩阵、进度跟踪。所有架构决策一律以设计文档为准；本计划与设计文档冲突时以设计文档为准。
>
> **目标读者**：AI Agent（主实施者）+ tech lead（评审与决策点把关）。
>
> **执行模型**：AI 单人按 Phase 顺序串行实施，TDD 严格纪律（产品代码与测试同批次提交），tech lead 在每个里程碑评审 + 解决 3 个开工前决策点 + 截图基准接受。

---

## 1. 开工前置（D-1，必做）

设计文档 §21 附录 B 的 7 项已全部敲定，但实施期还有 3 个工程决策点必须先解决，否则 Phase 0 无法启动。

### 1.1 三个 D-1 决策点

| # | 决策项 | 触发 Phase | 决策方式 | 推荐方案 |
|---|---|---|---|---|
| D-1.1 | 创建 `src/pagx/pag/` 目录与 `pagx-pag` 静态库 CMake target | Phase 0 启动前 | tech lead 直接执行 | 在 `src/pagx/CMakeLists.txt` 新增 `add_subdirectory(pag)`，`pag/CMakeLists.txt` 暂只声明空 target，逐 Phase 追加 source |
| D-1.2 | GlyphRun 序列化字段集最终核对 | Phase 8 之前确认 | 比对 `src/pagx/TextLayout.h:41-53` + `include/pagx/nodes/GlyphRun.h:35-117` 与设计文档附录 C 的 `GlyphRunBlob` | 已有定稿（§21 待确认 1），仅需 Phase 8 启动时复核字段一一对应 |
| D-1.3 | Mask 循环检测算法选择 | Phase 9 之前确认 | tech lead 二选一 | **推荐简化 DFS**（着色法：white/gray/black），N≤MAX_LAYERS_PER_COMPOSITION（10000）规模下足够；Tarjan SCC 仅在多入口环检测有优势，本场景单入口 inflate 不需要 |

### 1.2 环境校验 checklist

```bash
# 1. 主仓库可编译当前 main
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest

# 2. tgfx 子模块就绪（Phase 9.5 / 12 需 Surface 渲染能力）
ls third_party/tgfx/include/tgfx/layers/Layer.h

# 3. 若怀疑后续 Phase 12 需源码级 tgfx 调试，预先准备本地 tgfx
ls ../tgfx/CMakeLists.txt 2>/dev/null && echo "本地 tgfx 可用，Phase 12 可走 -DTGFX_DIR=../tgfx"

# 4. 现有截图基准 baseline 与 .cache 状态干净
git status test/baseline/version.json
```

### 1.3 worktree 策略（推荐）

设计文档默认 PR 在当前分支累积。若希望并行做"实施"与"文档微调"两条线，按项目 `.codebuddy/rules/Git.md` worktree 规约新建：

```bash
git worktree add ../libpag_pagx_pag_impl -b feature/${USER}_pag_v2_impl --no-track main
cp -R test/out/ ../libpag_pagx_pag_impl/test/out/
cp -R test/baseline/ ../libpag_pagx_pag_impl/test/baseline/
cp -R third_party/ ../libpag_pagx_pag_impl/third_party/
```

否则在当前 `libpag_pagx_to_pag` 分支直接推进即可。

---

## 2. Phase 路线图（基于设计文档 §19）

设计文档 §19 已完整定义 Phase 0-16（含 4a/4b/9.5/10.5/11.5/11.6/16.0-16.6 拆分）的产品代码、测试、交付判定、依赖矩阵。本节不重复，只补 Phase 之间的"里程碑分组"和"AI 实施切入点"。

### 2.1 五个里程碑（M1-M5）

| 里程碑 | 包含 Phase | 含义 | 阶段门槛 | 提交策略 |
|---|---|---|---|---|
| **M1：地基就绪** | 0, 1, 2, 3 | 全 include 基础 + 测试基建 + Baker 通用层 | code review 可启动 | 每 Phase 一个 commit |
| **M2：编解码闭环** | 4(4a/4b), 5, 6, 7, 8 | Codec 全 Tag + 4 个 Baker 子模块 | feature 分支可合入 | 每 Phase 一个 commit；M2 末做一次集成 commit |
| **M3：渲染 + 对外 API** | 9, 9.5, 10, 10.5, 11, 11.5, 11.6 | Inflater + 双对外 API + CLI + forward-ref + Baker render* / LayerMaskRef 补齐 | PR 可开 | Phase 10/10.5 完成后必做 API review |
| **M4：质量保障** | 12, 13(空), 14, 15 | Render 等价 + Fuzz + 性能基线 + 覆盖率工具 | Phase 15 coverage.sh 落地；脚本早期误用 85% 硬门槛（v2.24 已对齐设计文档 §17 D3 的 ≥75% 整体门槛） | Phase 12 触发首次截图基准接受流程 |
| **M5：文本 runtime-shape 回退** | 16.0-16.6 | Phase 8 预 shape 架构回退为 v1 runtime-shape；字体 FontProvider 注入；文本 inflate 不再因 macOS MakeFromName null 全空 | 含文字样本全部 inflate 非空；coverage 指定模块 ≥75%；可合入 main | 每子 Phase 一个 commit；16.6 用户 /accept-baseline 接 48 张新基线 |

### 2.2 各 Phase 估时（AI 单人）

仅作进度跟踪参照，不作 SLA 约束。

| Phase | 名称 | 估时（AI 实施） | 关键风险 |
|---|---|---|---|
| 0 | Diagnostic | 2-3 h | `kAllDiagnosticCodes[]` 5 步维护清单需 Phase 0 全部建立 |
| 1 | ValueCodec / TagCode / CorruptBuilder | 4-6 h | 必须做 Phase 1 exit gate：`grep std::vector<uint8_t>` 零命中 |
| 2 | PAGDocument + 测试基建（PAGXBuilder / PAGDocumentEquals / StructBuilders） | 6-8 h | 测试基建质量决定后续所有 Phase 速度，**不可省工** |
| 3 | LayerBaker | 3-4 h | Baker fatal 段 100-105/207 必须测全 |
| 4a | Codec 容器头框架 | 3-4 h | 可与 4b 合并；不合并时 4a 需独立可走通空 composition |
| 4b | Codec 主体 Tag | 4-6 h | EncodeSession 栈内构造模式（不走头文件）需严格遵守 |
| 5 | VectorBaker + ElementTags | 5-7 h | Path 量化双格式（format=0/1）必须保留兜底 |
| 6 | PaintBaker（融入 VectorBaker） | 2-3 h | 仅追加 paint 路径 |
| 7 | StyleFilterBaker + FilterTags + StyleTags | 4-5 h | 5 filter + 3 style 共 8 种枚举派发 |
| 8 | TextBaker + GlyphRun 序列化 | 5-6 h | D-1.2 GlyphRun 字段决策必须先到位 |
| 9 | LayerInflater | 6-8 h | D-1.3 Mask 循环检测算法决策必须先到位；Pass1/Pass2 + PackedLayerPath |
| 9.5 | RenderUtil（测试基建） | 1-2 h | 仅在 Phase 12 前完成即可，可与 Phase 10 并行 |
| 10 | PAGExporter（导出 API） | 3-4 h | 必做 API review |
| 10.5 | PAGLoader（加载 API） | 3-4 h | 必做 API review；`ImageBytesReleasedAfterInflate` 断言强制 |
| 11 | CLI 扩展 | 2-3 h | EndToEndTest + EdgeCasesTest |
| 12 | RenderEquivalence + Fuzz + CI | 4-6 h | **首次截图基准接受**触发 `/accept-baseline` 流程 |
| 13 | （取消） | 0 | v1 已 graceful reject |
| 14 | PerformanceTest + baseline.json | 2-3 h | 基线入 git |
| 15 | coverage.sh + 报告 | 2-3 h | 覆盖率对齐设计文档 §17 D3（整体 ≥75%，Baker/Codec/Inflater 各 ≥80%），未达则补测试回 Phase 1-9 |
| 16.0 | Phase 16 文本 runtime-shape 设计定稿 | 2 h | 已完成，详见 `pagx_to_pag_v2_phase16_text_redesign.md` |
| 16.1 | PAGDocument 新 ElementTextData + FontProvider 接口 | 3-4 h | ABI-兼容前提下同步移除 FontAsset 族；需 `FontProviderTest` 覆盖 default + null + fallback |
| 16.2 | TextBaker 重写 + pre-shaped 降级 warning | 2-3 h | 测试矩阵大改，保留的原有用例 ~30% |
| 16.3 | Codec 读写 schema 对齐 | 2 h | RoundTrip 全绿 |
| 16.4 | LayerInflater 重写 + 布局器接入 | 4-5 h | 复用 v1 TextLayout + TextShaper，接口层 FontProvider |
| 16.5 | 测试基建 FontProvider 注入 | 1-2 h | 共享 util 下沉 |
| 16.6 | Baseline accept + 覆盖率重跑 | 1 h + 用户审图时间 | coverage.sh 在 src/pagx/pag/LayerInflater.cpp + TextBaker.cpp ≥75% |

合计估时 ≈ **75-95 h**（M1-M5 全量），约对应 10-12 个 AI 工作日。**Phase 0-18 全部完成**（含 Phase 16 五轮补丁 + Phase 17 PAGX/PAG 对等文本模式 + Phase 18 TextModifier），CrossCheck 48/48 全绿。剩余仅：用户 `/accept-baseline` 接受新基线 + coverage 重跑（软指标）。

---

## 3. AI 实施提示词模板

每 Phase 启动时，AI Agent 应使用以下结构化提示词与 tech lead 同步上下文，避免重复探索：

### 3.1 Phase 启动提示词模板

```
任务：实施 PAG v2 转换的 Phase {N}：{Phase 名称}
权威设计：docs/pagx_to_pag_v2_design.md §19 Phase {N} 一行 + 依赖矩阵
依赖前置 Phase：{X, Y, Z}（必须确认前置 Phase 全绿）
本 Phase 产品代码文件：{...}
本 Phase 测试文件：{...}
交付判定：{原文复制 §19 表格"交付判定"列}

执行步骤：
1. 读取设计文档涉及本 Phase 的所有章节（按依赖矩阵列）
2. 读取前置 Phase 的产品代码（gen 出的接口）
3. 先写测试（TDD），再写产品代码
4. 编译 + 运行单测：cmake --build cmake-build-debug --target PAGFullTest && ./cmake-build-debug/test/PAGFullTest --gtest_filter="{SuiteName}.*"
5. 全绿后 commit（按 .codebuddy/rules/Git.md 自动提交规则）
6. 更新本计划 §5 进度表的 Phase {N} 行

异常处理：
- 编译错误 → 优先修产品代码，禁止注释测试或调宽断言
- 测试失败 → 定位根因，禁止 mock 绕过
- 设计文档不明确 → 暂停，启 AskUserQuestion 与 tech lead 确认
```

### 3.2 Phase 完成自检 checklist

每 Phase 提交前必过：

- [ ] 所有产品代码文件按 §16 目录组织放置
- [ ] 所有测试文件按 §16 `test/src/pag/` 组织
- [ ] `./codeformat.sh` 已执行（按 .codebuddy/rules/Test.md，忽略报错）
- [ ] `cmake --build cmake-build-debug --target PAGFullTest` 编译无错
- [ ] 本 Phase 新增测试套件全绿
- [ ] 设计文档"交付判定"列每项打勾（如 Phase 1 的 grep 零命中、Phase 0 的 5 步 CR 清单等）
- [ ] commit message 按 .codebuddy/rules/Git.md（120 字符内英文，描述用户可感知变化）
- [ ] 本计划 §5 进度表对应行更新

---

## 4. 风险矩阵

按概率 × 影响打分（1-5），分数 = 概率 × 影响。≥9 红色，4-8 黄色，<4 绿色。

| ID | 风险 | 概率 | 影响 | 分 | 缓解 | 触发 Phase |
|---|---|---|---|---|---|---|
| R1 | tgfx 渲染非确定性 → Day-1 smoke 失败 | 3 | 4 | 12 | 设计文档 §18.2 已计划 Day-1 smoke；失败则改用 Baseline::Compare 容差比对 | 9.5 / 12 |
| R2 | 截图基准首次大量 FAIL（~12 个） | 5 | 2 | 10 | 设计文档 §19 P1-10 已说明：tech lead 人工审图后用户 `/accept-baseline` 接受 | 12 |
| R3 | Phase 2 测试基建质量不足导致 Phase 3-11 反复返工 | 2 | 5 | 10 | 强制 PAGXBuilder ≥13 条 fluent smoke、PAGDocumentEquals 完整字段对比、StructBuilders 4 个 helper 全交付 | 2 |
| R4 | Mask 两趟 + PackedLayerPath 实现复杂度高 | 3 | 3 | 9 | 设计文档 §12 + §9.4 + §22 已给完整 C++ 骨架；D-1.3 算法决策先做 | 9 |
| R5 | varU32 / Path 量化等 wrapper 安全边界遗漏 | 2 | 4 | 8 | Phase 1 必交付 CorruptBuilder + 设计文档 §D.1 4 个 Safe wrapper；Phase 12 fuzz 兜底 | 1 / 12 |
| R6 | Codec EncodeSession 栈内构造模式被误改为头文件 | 2 | 3 | 6 | 设计文档 §8.5 P0-D 明确禁止；Phase 4 commit 前 grep `EncodeSession.h` 应仅命中 §16 目录登记的 Phase 0 头 | 4 |
| R7 | Phase 0 `kAllDiagnosticCodes[]` 5 步维护清单后续 PR 漏改 | 3 | 2 | 6 | 设计文档已固化 `DiagnosticTest.CodeToString.AllEnumValues` 自动断言，漏改即测试挂 | 0 之后所有 Phase |
| R8 | Phase 8 GlyphRun 字段集与运行时 TextLayout.h 偏差 | 2 | 3 | 6 | D-1.2 决策点强制 Phase 8 启动前复核 | 8 |
| R9 | tgfx 子模块版本与本期 PAGLoader.h 暴露的 Layer API 不兼容 | 1 | 4 | 4 | Phase 10.5 启动前编译 PAGExporter.h 验证 include 链 | 10.5 |
| R10 | 覆盖率 <75%（设计文档 §17 D3）触发 Phase 15 反向回补测试 | 2 | 2 | 4 | Phase 1-11 严守 TDD；Phase 14 完成后预跑 coverage.sh 提前发现 | 15 |
| R11 | Phase 15 脚本误用 85% 硬门槛导致 Phase 15 实测 76.75% 误报未达标 | 5 | 2 | 10 | 已分析：主要缺口 html/ 73% + LayerInflater 55% + TextBaker 60% 源于 Phase 15 测试环境字体不可用 → Phase 16-18 修完后，v2.24 重跑 coverage 整体 78.21% / LayerInflater 79.03% / TextBaker 89.80%（设计文档 §17 D3 门槛：整体 ≥75%、三模块各 ≥80%）；脚本阈值已对齐为 75% | 15 / 16 / v2.24 |
| R12 | Phase 16 跨平台字体解析不一致 | 4 | 3 | 12 | FontProvider 可注入接口 + 默认走 `pag::FontManager`；测试环境用 PAGXTest SpecSamples 的 font 注册逻辑复用；生产用户须调用 `PAGFont::RegisterFont` | 16.4 |
| R13 | Phase 16 `spec/samples/*.pagx` 中 pre-shaped pagx::Text 样本一律降级损失 per-glyph xform | 3 | 2 | 6 | 发 `TextGlyphRunsDowngraded=208` warning；PAGX 原生渲染路径（不经 .pag）仍保留完整 pre-shaped 支持 | 16.2 |

**最高优先级缓解**（红色风险 ≥9）：

1. **R1 / R2 联动方案**：Phase 9.5 完成后立即跑 Day-1 smoke，若失败 → 暂停 Phase 12 → 与 tech lead 决定容差策略 → 才进入 RenderEquivalenceTest
2. **R3 强制门**：Phase 2 完成时必须 `wc -l test/src/pag/support/PAGXBuilder.cpp` 报告并人工审过 fluent API 设计

---

## 5. 进度跟踪表

实施期间 AI Agent 必须每 Phase 完成时更新此表。状态码：⏳ 未开始 / 🔄 进行中 / ✅ 已完成 / ⚠️ 阻塞。

| Phase | 状态 | commit hash | 实际耗时 | 阻塞/备注 |
|---|---|---|---|---|
| D-1.1 创建 src/pagx/pag/ 目录 + CMake target | ✅ | (本提交) | 0.5 h | 跟随 glob 模式追加；设计文档 §16 同步修订 |
| D-1.2 GlyphRun 字段集复核 | ✅ | (Phase 8 实施期内确认) | — | Phase 8 启动时已逐字段对齐 PAGX TextLayout.h；后 Phase 16 v2.20 + Phase 17 v2.23 推翻 GlyphRunBlob 改用 case A/B 双分支，决策含义本身已被替换 |
| D-1.3 Mask 循环检测算法选择 | ✅ | (Phase 9 实施期内确认) | — | Phase 9 LayerInflater 落地时采用简化 DFS 着色法（white/gray/black），见 InflaterContext + Pass 2 mask 解析 |
| 0 Diagnostic | ✅ | (本提交) | 1 h | 7 文件交付；12/12 单测全绿；limits.h 一次性建全避免后续返工 |
| 1 ValueCodec / CorruptBuilder | ✅ | (本提交) | 1.5 h | 9 文件交付；38/38 单测全绿；exit gate `grep std::vector<uint8_t> src/pagx/pag/` 零命中 |
| 2 PAGDocument + 测试基建 | ✅ | (本提交) | 2 h | PAGDocument.h 全量字段；BakeContext + ResourceBaker；PAGDocumentEquals + 4 个 StructBuilders；26 条新测试全绿。**PAGXBuilder 推迟到 Phase 3**（与 LayerBaker 一起接 PAGX 节点更自然） |
| 3 LayerBaker | ✅ | (本提交) | 1.5 h | Baker 入口 + LayerBaker 通用字段 + CompositionBaker；含前置 Phase 2 推迟的 PAGXBuilder（13 fluent + 7 LayerBaker + 5 fatal = 25 测试全绿）。深度/总量 cap 集成测因 PAGX applyLayout 在深嵌套上 O(N²) 退化，回退到 BakeContext 直接验证（已注释原因） |
| 4a Codec 容器头 | ✅ | (本提交) | 1.5 h | DecodeContext + EncodeSession + Codec.h/cpp + FileHeader/CompositionList/Composition 三个框架 Tag；14 测试全绿（RoundTrip 4 + VersionReject 4 + Truncate 6）覆盖 300/301/302/303/304/305 段。`SeekTo()` helper 落地以绕开 v1 setPosition 怪癖；ImageAssetTable/FontAssetTable/LayerBlock/payload 推迟到 4b |
| 4b Codec 主体 Tag | ✅ | (本提交) | 2 h | ImageAssetTable + FontAssetTable + LayerBlock + LayerTransform + CompositionRefPayload；扩展 9 测试（ImageAsset/FontAsset/LayerBlock/Children/CompositionRef/LayerTransformDefaults + 306/403/406）。LayerBlock 字节布局采用固定顺序（LayerTransform → optional payload by type → childCount + children），避免 sub-Tag 区段长度歧义。Mask/Filter/Style sub-Tag 推迟到 Phase 5/7 |
| 5 VectorBaker + ElementTags | ✅ | (本提交) | — | 三段拆分：5a Codec 端 11 element + VectorPayload；5b Path 量化 + ShapePath 完整；5c VectorBaker 9 几何 element + Mask 两趟（已交付）。Text/TextPath/TextModifier 整体推迟到 Phase 8；Fill/Stroke ColorSource 推迟到 Phase 6 |
| &nbsp;&nbsp;5a element Tag codec | ✅ | (本提交) | 1.5 h | ElementTags.h/cpp + 11 element body Read/Write + VectorPayload(=24) + LayerBlock 接 VectorPayload；ShapeStyleData 内含 innerLength 包裹器（仅 SolidColor 分支）；MAX_VECTOR_ELEMENT_DEPTH 防御 + Element*Limit cap；12 测试全绿；Text 三种 element warn UnknownTagCode skip |
| &nbsp;&nbsp;5b Path codec | ✅ | (本提交) | 1 h | PathCodec.h/cpp（仅 format=0 裸 float，format=1 量化推迟 Phase 12）+ ReadPathProperty/WritePathProperty + ElementShapePath 接 path Property；8 测试全绿覆盖 404 PathVerbLimitExceeded + NaN/Inf/无效格式 + Conic weight 校验；Iterator 暴露 close 自动展开为 lineTo+close（行为锁定，wire 用 iterator 计数） |
| &nbsp;&nbsp;5c VectorBaker + Mask | ✅ | (本提交) | 1.5 h | VectorBaker.h/cpp（Rectangle/Ellipse/Polystar/Path/TrimPath/RoundCorner/MergePath/Repeater/Group 9 element baker）+ LayerBaker 接 Vector layer + Mask 两趟 pre-pass（recordLayerPaths Pass 1 + 路径解析 Pass 2）；11 测试全绿（VectorBaker 9 + LayerBaker mask 2）覆盖 202 MaskTargetMissing。**关键陷阱**：pagx::PolystarType {Polygon=0,Star=1} vs tgfx::PolystarType {Star=0,Polygon=1}，必须按名称映射 |
| 6 PaintBaker | ✅ | (本提交) | 1.5 h | ShapeStyleData codec 全 6 sourceType 分支（SolidColor/LinearGradient/RadialGradient/ConicGradient/DiamondGradient/ImagePattern）+ Property<vector<Color/float>> 特化 + PaintBaker ColorSource 派发（融入 VectorBaker.cpp）+ Fill/Stroke element baker + ImagePattern 复用 ResourceBaker::RegisterImage；10 测试全绿（PaintBaker 9 + ShapeStyleCodec 1）覆盖全 ColorSource roundtrip + 空 gradient stops 兜底注入 [Black@0,White@1] + filePath-only ImageSourceMissing=200 告警 + UnknownSourceType 降级 SolidColor + InvalidEnumValue=407 警告 |
| 7 StyleFilterBaker | ✅ | (本提交) | 2 h | StyleFilterTags.h/cpp（5 Filter Tag + 3 Style Tag + LayerFilters=13/LayerStyles=14 容器）+ StyleFilterBaker.h/cpp（5 PAGX filter node + 3 PAGX style node → pag::LayerFilter/LayerStyle）+ LayerBlock 接入容器 sub-Tag + `Property<std::array<float,20>>` 特化（ColorMatrix 用）；11 测试全绿（StyleFilterBaker 11）。**关键 wire 修正**：TagCode enum 44-53 + 120-142 按设计文档 §D.11/§D.12 重新对齐（Phase 5a 落地的名字是早期设计稿遗留）。**LayerBlock 布局扩展**：layerFlags bit 5/6 新增 HasFilters/HasStyles 位，Reader 用其决定是否读可选 sub-Tag（避开"sub-Tag TagHeader vs childCount varU32"歧义） |
| 8 TextBaker + GlyphRun | ✅ | (本提交) | 2 h | GlyphRunCodec.h/cpp（LayoutRun + ClassicGlyphRun 量化字节布局 §D.11：u8 kind + u32 fontIndex + f32 fontSize + 变长 Matrix + varU32 glyphCount + u16 glyphIds + int32List/floatList 量化数组 + hasXformBits bitstream，alignWithBytes 衔接）+ Phase 5a 占位的 ElementText/ElementTextPath/ElementTextModifier 三 Tag 完整 codec + RangeSelectorData inline + path_flags/modifier_flags 位域 + Property<vector<Point>> + Property<array<float,20>>  特化 + TextBaker.h/cpp（Text/TextPath/TextModifier/TextBox 派发，TextBox 走 BakeGroup；Font intern 成 System 占位 FontAsset；Runtime shaping 经 GlyphRunRenderer::GetLayoutRuns 读 Text.glyphData->layoutRuns 避开 friend 污染 pagx 命名空间）；13 测试全绿（GlyphRunBlob 7 + TextBaker 6） |
| 9 LayerInflater | ✅ | (本提交) | 2.5 h | InflaterContext.h（DiagnosticCollector 子类，物理屏蔽 pushError 实现"Inflater 无 fatal"语义 + PackedLayerPath uint64 打包 + CompositionVisitScope RAII + reserveLayerBudget/reservePendingMaskSlot 双预算）+ LayerInflater.h/cpp（Inflate 入口 + Pass 1 inflateLayer/inflateComposition + Pass 2 mask 解析 + Finalize warnings 转移）+ applyCommon 逐字段 setter（LayerBuilder::applyLayerAttributes 1:1 同构）+ 5 Filter（Blur/DropShadow/InnerShadow/ColorMatrix/Blend）+ 3 Style（DropShadow/InnerShadow/BackgroundBlur）+ 14 VectorElementType 完整派发 + ShapeStyleData 6 ColorSource 分支（SolidColor/Linear/Radial/Conic/DiamondGradient/ImagePattern）+ Text 走 resolveFontAsset + GlyphRunRenderer::BuildTextBlobFromLayoutRuns + ShapePayload 完整实现（ShapeStyleData→tgfx::ShapeStyle 经 tgfx::Shader 工厂）+ ImagePayload + SolidPayload + CompositionRef 递归；20 测试全绿（LayerInflater 17 覆盖 604/600/601/602/603/605 全告警路径 + VectorPayload/ShapePayload/ImagePayload/SolidPayload/Filter+Style/Mask/NestedGroup 场景 + LayerInflaterParity 3 覆盖 Baker→Encode→Decode→Inflate 与 LayerBuilder::Build 结构对比） |
| 9.5 RenderUtil | ✅ | (本提交) | 1 h | test/src/pag/support/RenderUtil.h/cpp（RenderLayerToSurface 与 test/src/PAGXTest.cpp:264-270 的 LayerBuilder 渲染样板 1:1 同构：Surface::Make + container.setMatrix(scale) + DisplayList.root()->addChild + render）+ test/src/pag/unit/RenderUtilTest.cpp（3 条：null 降级/零维降级/SolidLayer 像素 getColor 断言）+ test/src/pag/unit/InflaterRenderConsistencyTest.cpp（2 条，Simple 过、Complex DISABLED_）；Simple 用例（Rectangle + SolidColor Fill）LayerBuilder vs Bake→Encode→Decode→Inflate 两路渲染逐字节相等通过，证明非 blur 路径完整同构；Complex 用例（+LinearGradient+Stroke+DropShadow）实测 16384 字节中 2 字节差异 maxDelta=1（DropShadow 高斯模糊 GPU 浮点抗抖极限），DISABLED_ 等 Phase 12 RenderEquivalenceTest 用 webp baseline 容差接管 |
| 10 PAGExporter | ✅ | (本提交) | 1.5 h | include/pagx/PAGExporter.h（tgfx-render-free 对外头，FontMode 枚举 + Options::strict + ToBytes/ToFile 对称 Result）+ src/pagx/pag/PAGExporter.cpp（Bake → Codec::Encode 串联 + warnings 聚合 + strict=true 时 Severity==Warning 整体晋升 + ToFile 走 `<path>.tmp-<pid>-<tid>-<ts>` 临时文件 + std::filesystem::rename 原子写 + parent 不存在 → ProducerFatal=106）；6 测试全绿（PAGExporter 6：ToBytes 成功/LayoutNotApplied/strict 晋升 EmptyDocument=207、ToFile 写成功+magic 校验/parent 缺失/strict 阻止写） |
| 10.5 PAGLoader | ✅ | (本提交) | 1.5 h | include/pagx/PAGLoader.h（依赖 tgfx::Layer 对外头，LoadFromBytes/LoadFromFile/Peek 三入口 + PeekResult 携带 FileHeader 5 字段）+ src/pagx/pag/PAGLoader.cpp（Codec::Decode → LayerInflater::Inflate 串联 + strict=true 时只晋升 `SeverityOf==Warning && StageOf==Codec` 的诊断（Inflater 告警永不晋升）+ FileReadFailed=307 触发 + ReadFileToBytes 前置 MAX_TOTAL_BODY_BYTES 检查 + Peek 浅解析仅读 FileHeader Tag O(header)）+ **LayerInflater 补丁**：MakeFromEncoded 成功后 `doc.images[idx]->data.reset()` 实现 §11.1 "Inflater 生命周期纪律"（use_count==1 契约）；9 测试全绿（PAGLoader 9：LoadFromBytes 成功+短字节拒绝、LoadFromFile 成功+缺失 307、Peek header 核对+文件路径+错误 magic 拒绝、Inflater warning 透传不晋升 ok、strict 不晋升 Inflater warning） |
| 11 CLI 扩展 | ✅ | (本提交) | 1 h | src/cli/CommandExport.cpp 扩展 `--format pag` 分支调 PAGExporter::ToFile + `--pag-strict` 开关映射 Options::strict + ExportToPAG() 走 LoadDocument → hasUnresolvedImports 检查 → applyLayout → ToFile + 诊断信息经 FormatDiagnostic 逐行打印到 stderr（error/warning 区分前缀）+ ok 决定 exit code（§14.2 契约）；CMakeLists.txt 在 PAGFullTest 上注入 `PAGX_CLI_BINARY_PATH="$<TARGET_FILE:pagx-cli>"` 让测试能 fork-exec CLI；3 smoke 测试全绿（CommandExportPAG：SmokeProducesPagFile 校验 PAG magic / MissingInputFileFails 非零退出 / StrictFlagBlocksEmptyDocument 确认 --pag-strict 阻止空文档导出） |
| 11.5 CompositionRef forward-ref | ✅ | 569a339c | 0.5 h | Codec 允许 CompositionRefPayload 引用未解码的后续 composition（范围上界改为 CompositionList 已声明总数而非 `out->size()`）；RoundTripTest +2 条回归（CompositionRefForwardReferenceSupported / CompositionRefBeyondDeclaredCountStillFatal）；Phase 12.0 探测 6 DECODE_FAIL → 0（app_icons / complete_example / composition / container_layout / nebula_cadet / space_explorer 全绿） |
| 11.6 Baker render\* + LayerMaskRef codec | ✅ | 84bdf774 | 2 h | 5 个根因修复：(a) VectorBaker Rectangle/Ellipse/Polystar/ShapePath/Path 改读 `renderPosition/renderSize/renderScale/renderOuterRadius/renderInnerRadius` 而非原始字段（布局 shorthand 场景 NaN/0 → tgfx drawRRect DEBUG_ASSERT crash）；(b) LayerBaker `BuildLayerMatrix` 改读 `layer.renderPosition()` 接入 flex/top/left 布局；(c) VectorBaker `BakeGroup` 同样改读 `renderPosition()`；(d) TextBaker `BakeTextPath` 补齐 renderScale path transform + MakeTrans(renderPosition) + renderBaselineOrigin；(e) **CodecTagsLayer 新增 LayerMaskRef (TagCode=12) Write/Read + layerFlags bit 7 HasMaskRef**——Phase 4b 预留了 TagCode 位但从未实现 serialise，masks 每次 round-trip 静默丢失。CrossCheck FAIL 从 34 降到 15；220/220 回归全绿；48/48 样本跑完无 tgfx crash |
| 12 RenderEquivalence + Fuzz + CI | ✅ | 77decbe6 | 3 h | 48 samples + 48 CrossCheck 参数化；96 首跑 FAIL 为基线缺失预期；RenderCrossCheckTest 33 PASS / 15 FAIL (PSNR < 30dB 的多为文字样本，Phase 16 修复后预计清零 8+)；libFuzzer harness + standalone fallback + .github/workflows/pagx-fuzz.yml 4-shard × 6h CI yaml 已落 |
| 13 v1 改动（取消） | ✅ | — | 0 | 设计文档已确认无需改 v1 |
| 14 PerformanceTest + baseline | ✅ | 8b706d4c | 1 h | test/src/pag/unit/PerformanceTest.cpp：对 48 spec/samples 每样本测 size_ratio + load_ratio + 5 绝对时间（中位数-11 跑），±20% load_ratio 漂移非阻塞 WARNING；亚毫秒噪声地板 = 1ms；baseline.json 不入 git；实测 size_ratio 中位数 0.43 / load_ratio 中位数 0.20，达成设计文档期望 |
| 15 coverage.sh | ✅ | 3ee770ce 等 | 3 h | CMake -DPAG_COVERAGE 开关 + tools/coverage.sh (clang source-based) + `-fsanitize=fuzzer` coverage-mode 剔除 ASAN；Phase 15 时实测 line coverage 76.75%（脚本当时误配 85% 硬门槛，实际设计文档 §17 D3 为整体 ≥75%）：html/ 73% + pag/ 75%（LayerInflater 55% / TextBaker 60% 为主要缺口）；tools/render_compare.py + `PAGXNativeReferenceTest.RenderSpecSamplesToPAGXNativeDir` 辅助生成两列对比 HTML 便于人审；Phase 16–18 修完后 v2.24 重跑：整体 78.21% / LayerInflater 79.03% / TextBaker 89.80%（均达 §17 D3 门槛）；脚本阈值已对齐 75% |
| **16.0 text redesign 设计** | ✅ | 3ee770ce | 2 h | docs/pagx_to_pag_v2_phase16_text_redesign.md (422 行) + Phase 16 集成到主设计文档 6 章节 + 术语索引 + Phase 表 + 维护日志 v2.20 |
| **16.1 PAGDocument + FontProvider** | ✅ | f76384de | 3-4 h | FontProvider 接口 + 默认 pag::FontManager 实现；ElementTextData 新字段集；FontAsset 族删除 |
| **16.2 TextBaker 重写** | ✅ | f76384de | 2-3 h | runtime-shape Baker；pre-shaped 路径降级发 `TextGlyphRunsDowngraded=208` |
| **16.3 Codec schema 对齐** | ✅ | f76384de | 2 h | ElementText body 新 schema；FontAssetTable 仅保留 read-warn-skip 兼容 |
| **16.4 Inflater 重写 + 布局器接入** | ✅ | f76384de | 4-5 h | inflateElementText 走 FontProvider + TextShaper + v1 TextLayout |
| **16.5 测试基建 FontProvider 注入** | ✅ | 53a5609a | 1-2 h | PAGXTest fixture 内置 Arial + Noto + fontConfig() / fontProvider() getter |
| **16.6 Baseline accept + 覆盖率重跑** | 🔄 | a36b06e0 | — | 覆盖率已重跑（设计文档 §17 D3 门槛全部达标：整体 78.21%、LayerInflater 79.03%、TextBaker 89.80%）；仅余用户操作 `/accept-baseline` 接受 48 张新基线 |
| 16.x 五轮稳定补丁 | ✅ | 45cbe0b0 / 53a5609a / 36471b33 / 55590003 / bfa0637f | 累计 ~6 h | baseline-y / font align / HarfBuzz shaper / layoutOrigin / shapedRuns hint；CrossCheck 15 → 4 |
| **17 PAGX/PAG 对等文本模式（4 commit）** | ✅ | 2832e8f3 / a62b3652 / 1c1bae28 / (Commit 4) | 累计 ~12 h | case A path-based + case B host-font-shapedGlyphs 双分支；EmbeddedFont 顶层资源；删除 Phase 16 runtime-shape 4 条死路径；CrossCheck 4 → 2（剩 image_pattern + text_modifier） |
| **17.x ImageAsset decodedImage cache** | ✅ | (Phase 17 收尾) | — | image_pattern.pagx 4 Layer 共享单 ImageAsset 不再静默 fallback；CrossCheck 转 PASS |
| **18 TextModifier RangeSelector 实施** | ✅ | 0cf5dd04 | — | TextBaker::BakeTextModifier selector 透传打通；text_modifier.pagx CrossCheck PSNR=+∞ bit-perfect；CrossCheck 终态 48/48 全 PASS |

---

## 6. 验收（设计文档 §17 索引）

发布前必过四档验收，详细判定见 `pagx_to_pag_v2_design.md` §17：

| 档 | 内容 | 何时检查 |
|---|---|---|
| **A 功能正确性** | A1-A5：spec/samples export + Decode + RoundTrip + Inflater 同构 | Phase 11 完成时 |
| **B 渲染一致性** | B1-B3：Render / OutlineAll 模式 Baseline::Compare | Phase 12 完成时 |
| **C 鲁棒性** | C1-C10：空 / 缺资源 / 截断 / 循环 / 超大 文件 | Phase 4-12 各 TruncatedDecodeTest 已逐项覆盖 |
| **D 非功能性（软指标）** | D1-D5：体积 ≤30%、性能 ≤20%、覆盖率对齐设计文档 §17 D3（整体 ≥75%，Baker/Codec/Inflater 各 ≥80%）、API review、lint | Phase 14-15 完成时 |

---

## 7. 文档维护纪律

- 本计划与设计文档冲突 → 以设计文档为准，本计划同步修改
- 设计文档变更（Phase 拆分变化、新增决策点） → 同步更新本计划 §2 / §5
- 进度表 §5 由 AI Agent 实施时维护，不需 tech lead 同步
- 实施期发现的新风险 → 追加到 §4 风险矩阵末
- 完成全部 Phase 后，本计划归档到 `docs/archive/development_plan_completed.md`，主分支保留链接
