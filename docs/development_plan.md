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

设计文档 §19 已完整定义 16 个 Phase（0-15，含 4a/4b/9.5/10.5 拆分）的产品代码、测试、交付判定、依赖矩阵。本节不重复，只补 Phase 之间的"里程碑分组"和"AI 实施切入点"。

### 2.1 四个里程碑（M1-M4）

| 里程碑 | 包含 Phase | 含义 | 阶段门槛 | 提交策略 |
|---|---|---|---|---|
| **M1：地基就绪** | 0, 1, 2, 3 | 全 include 基础 + 测试基建 + Baker 通用层 | code review 可启动 | 每 Phase 一个 commit |
| **M2：编解码闭环** | 4(4a/4b), 5, 6, 7, 8 | Codec 全 Tag + 4 个 Baker 子模块 | feature 分支可合入 | 每 Phase 一个 commit；M2 末做一次集成 commit |
| **M3：渲染 + 对外 API** | 9, 9.5, 10, 10.5, 11 | Inflater + 双对外 API + CLI | PR 可开 | Phase 10/10.5 完成后必做 API review |
| **M4：质量保障** | 12, 13(空), 14, 15 | Render 等价 + Fuzz + 性能基线 + 覆盖率 | 可合入 main | Phase 12 触发首次截图基准接受流程 |

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
| 15 | coverage.sh + 报告 | 2-3 h | 覆盖率 ≥85%，未达则补测试回 Phase 1-9 |

合计估时 ≈ **60-80 h**，约对应 8-10 个 AI 工作日（按每天 8h 有效编码）。这与设计文档 §19 评估的"单人 16 工作日或 3 人 10-12 天"一致。

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
| R10 | 覆盖率 <85% 触发 Phase 15 反向回补测试 | 2 | 2 | 4 | Phase 1-11 严守 TDD；Phase 14 完成后预跑 coverage.sh 提前发现 | 15 |

**最高优先级缓解**（红色风险 ≥9）：

1. **R1 / R2 联动方案**：Phase 9.5 完成后立即跑 Day-1 smoke，若失败 → 暂停 Phase 12 → 与 tech lead 决定容差策略 → 才进入 RenderEquivalenceTest
2. **R3 强制门**：Phase 2 完成时必须 `wc -l test/src/pag/support/PAGXBuilder.cpp` 报告并人工审过 fluent API 设计

---

## 5. 进度跟踪表

实施期间 AI Agent 必须每 Phase 完成时更新此表。状态码：⏳ 未开始 / 🔄 进行中 / ✅ 已完成 / ⚠️ 阻塞。

| Phase | 状态 | commit hash | 实际耗时 | 阻塞/备注 |
|---|---|---|---|---|
| D-1.1 创建 src/pagx/pag/ 目录 + CMake target | ✅ | (本提交) | 0.5 h | 跟随 glob 模式追加；设计文档 §16 同步修订 |
| D-1.2 GlyphRun 字段集复核 | ⏳ | — | — | Phase 8 前 |
| D-1.3 Mask 循环检测算法选择 | ⏳ | — | — | Phase 9 前 |
| 0 Diagnostic | ✅ | (本提交) | 1 h | 7 文件交付；12/12 单测全绿；limits.h 一次性建全避免后续返工 |
| 1 ValueCodec / CorruptBuilder | ✅ | (本提交) | 1.5 h | 9 文件交付；38/38 单测全绿；exit gate `grep std::vector<uint8_t> src/pagx/pag/` 零命中 |
| 2 PAGDocument + 测试基建 | ✅ | (本提交) | 2 h | PAGDocument.h 全量字段；BakeContext + ResourceBaker；PAGDocumentEquals + 4 个 StructBuilders；26 条新测试全绿。**PAGXBuilder 推迟到 Phase 3**（与 LayerBaker 一起接 PAGX 节点更自然） |
| 3 LayerBaker | ✅ | (本提交) | 1.5 h | Baker 入口 + LayerBaker 通用字段 + CompositionBaker；含前置 Phase 2 推迟的 PAGXBuilder（13 fluent + 7 LayerBaker + 5 fatal = 25 测试全绿）。深度/总量 cap 集成测因 PAGX applyLayout 在深嵌套上 O(N²) 退化，回退到 BakeContext 直接验证（已注释原因） |
| 4a Codec 容器头 | ✅ | (本提交) | 1.5 h | DecodeContext + EncodeSession + Codec.h/cpp + FileHeader/CompositionList/Composition 三个框架 Tag；14 测试全绿（RoundTrip 4 + VersionReject 4 + Truncate 6）覆盖 300/301/302/303/304/305 段。`SeekTo()` helper 落地以绕开 v1 setPosition 怪癖；ImageAssetTable/FontAssetTable/LayerBlock/payload 推迟到 4b |
| 4b Codec 主体 Tag | ✅ | (本提交) | 2 h | ImageAssetTable + FontAssetTable + LayerBlock + LayerTransform + CompositionRefPayload；扩展 9 测试（ImageAsset/FontAsset/LayerBlock/Children/CompositionRef/LayerTransformDefaults + 306/403/406）。LayerBlock 字节布局采用固定顺序（LayerTransform → optional payload by type → childCount + children），避免 sub-Tag 区段长度歧义。Mask/Filter/Style sub-Tag 推迟到 Phase 5/7 |
| 5 VectorBaker + ElementTags | 🔄 | — | — | 三段拆分：5a Codec 端 11 element + VectorPayload（已交付）；5b Path 量化 + ShapePath 完整（已交付）；5c VectorBaker + Mask 两趟（待）。Text/TextPath/TextModifier 整体推迟到 Phase 8 |
| &nbsp;&nbsp;5a element Tag codec | ✅ | (本提交) | 1.5 h | ElementTags.h/cpp + 11 element body Read/Write + VectorPayload(=24) + LayerBlock 接 VectorPayload；ShapeStyleData 内含 innerLength 包裹器（仅 SolidColor 分支）；MAX_VECTOR_ELEMENT_DEPTH 防御 + Element*Limit cap；12 测试全绿；Text 三种 element warn UnknownTagCode skip |
| &nbsp;&nbsp;5b Path codec | ✅ | (本提交) | 1 h | PathCodec.h/cpp（仅 format=0 裸 float，format=1 量化推迟 Phase 12）+ ReadPathProperty/WritePathProperty + ElementShapePath 接 path Property；8 测试全绿覆盖 404 PathVerbLimitExceeded + NaN/Inf/无效格式 + Conic weight 校验；Iterator 暴露 close 自动展开为 lineTo+close（行为锁定，wire 用 iterator 计数） |
| 6 PaintBaker | ⏳ | — | — | — |
| 7 StyleFilterBaker | ⏳ | — | — | — |
| 8 TextBaker + GlyphRun | ⏳ | — | — | — |
| 9 LayerInflater | ⏳ | — | — | — |
| 9.5 RenderUtil | ⏳ | — | — | 可与 Phase 10 并行 |
| 10 PAGExporter | ⏳ | — | — | API review |
| 10.5 PAGLoader | ⏳ | — | — | API review |
| 11 CLI 扩展 | ⏳ | — | — | — |
| 12 RenderEquivalence + Fuzz + CI | ⏳ | — | — | 截图基准接受 |
| 13 v1 改动（取消） | ✅ | — | 0 | 设计文档已确认无需改 v1 |
| 14 PerformanceTest + baseline | ⏳ | — | — | — |
| 15 coverage.sh | ⏳ | — | — | ≥85% |

---

## 6. 验收（设计文档 §17 索引）

发布前必过四档验收，详细判定见 `pagx_to_pag_v2_design.md` §17：

| 档 | 内容 | 何时检查 |
|---|---|---|
| **A 功能正确性** | A1-A5：spec/samples export + Decode + RoundTrip + Inflater 同构 | Phase 11 完成时 |
| **B 渲染一致性** | B1-B3：Render / OutlineAll 模式 Baseline::Compare | Phase 12 完成时 |
| **C 鲁棒性** | C1-C10：空 / 缺资源 / 截断 / 循环 / 超大 文件 | Phase 4-12 各 TruncatedDecodeTest 已逐项覆盖 |
| **D 非功能性（软指标）** | D1-D5：体积 ≤30%、性能 ≤20%、覆盖率 ≥85%、API review、lint | Phase 14-15 完成时 |

---

## 7. 文档维护纪律

- 本计划与设计文档冲突 → 以设计文档为准，本计划同步修改
- 设计文档变更（Phase 拆分变化、新增决策点） → 同步更新本计划 §2 / §5
- 进度表 §5 由 AI Agent 实施时维护，不需 tech lead 同步
- 实施期发现的新风险 → 追加到 §4 风险矩阵末
- 完成全部 Phase 后，本计划归档到 `docs/archive/development_plan_completed.md`，主分支保留链接
