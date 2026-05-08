# PAGX → PAG v2 技术方案修订记录（CHANGELOG）

本文档归档了 `docs/pagx_to_pag_v2_design.md` 的完整修订记录（v1.0 → v2.23）。

**实现阶段不需通读历史**；主文档 `### 上次开工必读` 段已列出开工前必知的 17 条硬约束（12 基线 + 3 v2.19 新增 + 1 v2.20 文本回退 + 1 v2.23 Phase 17 文本对等）。
历史版本条目仅作为"为什么这样设计"的背景参考。

---

### 历史修订记录

- **v2.23 Phase 17 PAGX/PAG 对等文本设计**（推翻 Phase 16，主文档 §10 整章替换，实施按 4 次 commit 拆分待启动）：

  **背景**：Phase 16 runtime-shape 方案在 `glyph_run.pagx` 上暴露根本冲突——作者 `<GlyphRun>` 引用的嵌入 `<Font>` path 数据在 Bake 时被完整丢弃，PAG 端用 host font 重新 shape，glyph 形状完全不同于 PAGX-native 渲染。Phase 16.6 shapedRuns hint / Phase 16.7 author-GlyphRun origin salvage 都是对"数据源丢失"打补丁。用户重新校准原则：**PAG 是 PAGX 的二进制版本，PAGX 怎么处理 PAG 就怎么存，两边加载后渲染行为完全对等**。

  **核心决策**：按 PAGX Text 节点原始数据类型二分路径：
  - **情况 A**：PAGX 有作者 `<GlyphRun>` → PAG 存 path 数据。新增顶层资源 `EmbeddedFont{ unitsPerEm, glyphs[{advance, path}] }`（对应 PAGX `<Font id="fontN"><Glyph path="..."/></Font>`，**不是 ttf 子集**）；`ElementTextData.glyphRuns` 存 `GlyphRunData{embeddedFontIndex, fontSize, glyphs, x, y, xOffsets, positions}`；Inflater 合成 `tgfx::Path` → `ShapePath` 渲染。
  - **情况 B**：PAGX 只有纯 `<Text text="...">` → PAG 存已 shape 的 glyph 序列。`ElementTextData.shapedGlyphs` 存 `ShapedGlyphRun{typefaceFamily/Style/Key, fontSize, glyphs, positions, xforms}`，直接搬自 PAGX `Text.glyphData->layoutRuns`（Bake 时 PAGX `applyLayout()` 已 shape）；Inflater 用 FontProvider 解析 typeface 组装 `tgfx::TextBlob`。
  - **ttf/otf 字体文件永不嵌入 PAG**。情况 A 的 path 数据是 PAGX 文件内 SVG-style 资源；情况 B 继续靠外部 host font。
  - **约束布局（FlexBox / percentWidth / centerX / left-right-top-bottom / TextBox 尺寸传播）完全 Bake 时固化**。PAG 里 Layer / Group / Text 的 position/size 字段全是绝对坐标。Inflater 不再跑约束解析。
  - **Text 排版（paragraph / line-box / justify / writing mode）完全 Bake 时固化**。Inflater 不再调 `pagx::TextLayout` 或 `TextShaper`。

  **数据模型变更**：
  - `PAGDocument` 新增 `vector<unique_ptr<EmbeddedFont>> embeddedFonts`。
  - `ElementTextData` 重写：删除 Phase 16.6 `shapedRuns` 字段（语义升级为 `shapedGlyphs`，从 opt-in hint 变为情况 B 唯一数据源），新增 `glyphRuns` / `shapedGlyphs` 双分支字段。保留 `text / fontFamily / fontStyle / fauxBold/italic / paint / paragraph` 源属性元数据（渲染不用，工具反向用）。
  - 新增 struct `EmbeddedGlyph` / `EmbeddedFont` / `GlyphRunData` / `ShapedGlyphRun`。

  **Tag 表变更**：
  - 顶层段新增 `EmbeddedFontTable=8` / `EmbeddedFontItem=9`（占尽顶层预留空位）。
  - ElementText 内新字段 `glyphRuns` / `shapedGlyphs` **不新增 TagCode**——与 Phase 16.6 `shapedRuns` 同构，在 ElementText body 的 `boxFlags` 字段里用位开关条件写入（与 ElementText 现有扁平字段风格一致；sub-Tag 方案评估后放弃，避免在 leaf payload 里引入容器结构）。
  - `boxFlags` 从 `uint8` 扩展为 `uint16`（§6.5 字段追加规则 ①）；Phase 16.6 `0x40 = hasShapedHint` 保留，新增 `0x80 = hasGlyphRuns` / `0x100 = hasShapedGlyphs`。
  - body 顶层写入顺序：`FileHeader → ImageAssetTable → EmbeddedFontTable → CompositionList → End`。
  - Phase 16.6 `shapedRuns` 编解码逻辑（最终）删除在 Commit 4。
  - PAG v2 尚未对外发布，不保留向后兼容；FORMAT_VERSION 保持 `0x02`。

  **Baker/Inflater 变更**：
  - `TextBaker::BakeText`：按 `src.glyphRuns` 是否非空分两分支。情况 A 走 `InternEmbeddedFont` 去重写资源表；情况 B 搬 `src.glyphData->layoutRuns` 到 `shapedGlyphs`（positions 减 layoutOrigin+positionY 转相对存储）。删 Phase 16.6 `kMaxHintGlyphs` 上限和 `hasMultipleLines || hasXforms` gate（无条件写 shapedGlyphs）；删 Phase 16.7 author-GlyphRun origin salvage；删 `TextGlyphRunsDowngraded=208` warning（不再降级）。
  - `PAGExporter::ToBytes` 自动调 `applyLayout`——外部调用方无需手动调。
  - `LayerInflater::inflateElementText`：按 `glyphRuns` / `shapedGlyphs` 分两分支。情况 A 合成 tgfx::Path 走 ShapePath；情况 B 组装 TextBlob 走 Text。删除 Phase 16 runtime-shape 所有分支（HarfBuzz 直连 / primitive shaper / resolveFont 主路径）。
  - CrossCheck `PathA_vs_PathB` 从"辅助诊断"升级为"强相等保证"——两边走完全相同的 path 数据（情况 A）或同一 host font + glyph IDs（情况 B），像素级一致应为常态。

  **实施拆分（4 commit）**：
  1. 数据模型 + Codec 骨架（新 struct + 新 tag 编解码 + Codec roundtrip 单测；Baker/Inflater 暂保留 Phase 16 逻辑作为 bridge）
  2. Baker + Inflater 情况 A 分支（glyph_run 样本视觉对齐 PAGX-native）
  3. Baker + Inflater 情况 B 分支（删 Phase 16 runtime-shape 路径；48 个样本稳定）
  4. 清理 + 文档同步（删 `shapedRuns` 字段 + Phase 16 过渡代码 + memory 更新）

  **Phase 16 文档处置**：Phase 16 原独立设计文档 `pagx_to_pag_v2_phase16_text_redesign.md` 随 v2.23 删除；其历史叙述（v1 runtime-shape 决策、FontProvider 接口、TextShaper 依赖、FontAsset 废弃等）已由下方 v2.20 / v2.21 / v2.22 条目完整覆盖。主文档 §10 章首简要指向这几条 changelog 作为历史溯源。

- **v2.22 Phase 16.6 shapedRuns hint**（TextBox multi-line bypass，1 个 commit，主文档 §10.5 / §10.6 修订）：

  背景：v2.21 四补丁落地后 CrossCheck 还剩 7 个 FAIL，其中 `text_box` / `constraint_textbox_and_group` / `pagx_features` 三个样本卡在 15-30 dB 之间。根因 §10.6 #1 已经描述：PAGX `TextLayout` 把单个 `<Text>` 拆成多行 / 拉成 justify / 按列竖排后，`layoutRuns[]` 里每个 glyph 的 (x, y) + 可选 RSXform 全部独立，**单一 `ElementTextData::position` 架构上表达不出多行**。

  **核心决策**：选 §10.6 候选方案 A（hint 字段）而非 B（拆多 ElementTextData，修不全 justify/vertical）或 C（port 段落布局器，~2-4 天工作量）。tgfx 早就暴露 `TextBlob::MakeFrom(glyphIDs[], positions[], count, font)` + `TextBlobBuilder::allocRunMatrix/RSXform` API，且 `pagx::GlyphRunRenderer::BuildTextBlobFromLayoutRuns` 已经把 layoutRuns 落成 TextBlob 的逻辑封装好——Inflater 直接复用，不需要重写 shape 路径。

  **改动（7 文件 / +294）**：
  - `ElementTextData` 加 inner struct `ShapedRun { glyphs, positions (relative to position), xforms, fontSize, typefaceFamily, typefaceStyle, typefaceKey }` + `vector<ShapedRun> shapedRuns`。字段追加式，不升 FORMAT_VERSION。
  - `ElementTags.cpp` boxFlags 加 `0x40 = hasShapedHint` 位，条件写入 hint 块（位于 boxText 块之后、paint 之前；旧 decoder 仍能通过 boxFlags 跳过）。read 端 guard by `MAX_RUNS_PER_TEXT=256` + `MAX_GLYPHS_PER_RUN=100000`。
  - `TypefaceKey.h` 新 helper `MakeTypefaceKey` 返回四元组签名 `family|style|unitsPerEm|glyphsCount`，Baker 写入、Inflater 比对。
  - `TextBaker::BakeText` 加**语义判定**的 hint 生成：`layoutRuns` 出现多行 baseline y 或非空 xforms，且 glyph 总数 ≤ 500，则 snapshot。positions 相对 `position` 坐标（Baker 减 layoutOrigin.x / positionY），xforms.tx/ty 同理。
  - `LayerInflater::inflateElementText` 前置 hint 优先路径：逐 run 用 FontProvider 重新 resolve typeface，比对 `MakeTypefaceKey` 字符串；全匹配则用 `BuildTextBlobFromLayoutRuns(runs, Matrix::I())` 重放，任一不匹配 emit `TextShapingHintMiss=608` info 并降级到现有 HarfBuzz / primitive 双路径。
  - 新增 Diagnostic code `TextShapingHintMiss=608`（Inflater info 段）。
  - Baker 不生成 hint 时（例如 standalone single-line Text），Inflater 完全走现有 runtime-shape 路径，零行为变化——hint 是**严格增补**，不影响已过的 10 个样本。

  **语义判定 vs 结构判定（非显然设计决策）**：最初实现用 `layoutOrigin().x/y != 0`（结构判定，"Text 在不在 TextBox 里"）触发 hint，实测 `text_box.pagx` Box 1 的 Text 起点恰好在 TextBox (0,0)，被误判为 standalone、hint 不生成、justify 多行降级成 runtime shape 单行。改成"layoutRuns 有多行 baseline 或 xforms"的语义判定后，Box 1 命中。教训沉淀到 memory。

  **累积效果**：
  - CrossCheck FAIL 7 → **4**（`text_box` + `constraint_textbox_and_group` + `pagx_features` 三个过 30 dB）。
  - 总 PAGFullTest FAIL 56 → **53**（48 Render_Baseline 等 `/accept-baseline` 不变）。
  - 剩 4 个 CrossCheck 全部属 §10.6 已知限制（`text_modifier` 未实现 / `glyph_run` 嵌入字体 / `complete_example` 综合 / `image_pattern` 非文字）。

  **非显然成本权衡**（写入设计文档）：
  - `.pag` 体积：每 hint-eligible Text 增 ~10-20 B/glyph。text_box 四 Box 总计 ~110 glyph = +2 KB 增量；对比 ElementTextData 原始 ~200 B，增大约 10×，但仍是 KB 级；大段落（>500 glyph）强制跳过 hint 避免撑爆文件。
  - 字体精确匹配依赖 `typefaceKey` 签名。字体版本/子集变化时 signature 会 miss → emit info → 降级 runtime shape（与 substitution 自然行为一致），不会错渲染。
  - `shapedRuns` 作为**可选优化**而非 canonical storage：Inflater 拿不到字段、拿到空字段、拿到但 key 不匹配——三种情况全部 graceful fallback 到 runtime shape。

  **Pre-shaped GlyphRun / 嵌入字体决策（方案 β 确认，§10.6 #2）**：经可行性验证，方案 γ（扩展 shapedRuns hint 覆盖 pre-shaped Text）不可行：(1) `TextLayout::Layout()` 对 pre-shaped Text 走快速路径、**不生成 `layoutRuns`**，Baker 端 hint 语义判定永远为 false；(2) 嵌入字体 typeface 是 `PathTypefaceBuilder` 产生的运行时临时对象，无 family/style/unitsPerEm 可做跨进程 key 签名；(3) glyph ID 体系完全独立于系统字体。**决定：不支持嵌入字体序列化（方案 α 工程量大且违背 Phase 16 "不存 glyph 数据"原则）**，`glyph_run.pagx` PSNR ~16 dB 接受为已知限制。§10.6 同步由"设计未定"改为明确不支持。

---

- **v2.21 Phase 16 实施期补丁**（实测期文档同步，5 个 commit，主文档 §10 修订）：

  背景：Phase 16.1-16.4 主体（commit `f76384de`）落地后，跑 `RenderCrossCheck.PathA_vs_PathB` 发现 7 个文字相关样本仍 FAIL（PSNR 7-29 dB）。逐一排查暴露 4 个 Phase 16 设计稿未预见的实施细节，外加 1 个架构层面已知限制。本条目同步主文档 §10 章节结构、不改 PAG 文件格式（FORMAT_VERSION 仍 0x02）、不改 ABI。

  - **§10.4 重写**："TextBox 的 inverseMatrix 处理" → "TextBox 的坐标处理（实施修订）"。设计稿要求 Baker 把 `inverseMatrix` 展平成 position 后乘修正、对齐 `LayerBuilder::prepareTextBoxTextBlobs`——实施期发现该路径在 runtime-shape 下不可行（Inflater 没有 LayoutContext）。改为 Baker 直接读 `pagx::Text::layoutOrigin()`（新 public API = `textBounds.x/y`）作为绝对 origin 序列化，Inflater 端不需要 TextBox 上下文。`rich_text.pagx` 多 span 拼接样本验证 PSNR ≥ 30 dB。
  - **§10.5 新增**："实施期补丁要点（v2.21）"——四个补丁的根因 + 修复表格：
    - `45cbe0b0` baseline-y：`tgfx::TextBlob::MakeFrom(text, font)` glyph 以 `y=baseline=0` 写入，首行被裁。新增 public `Text::firstBaselineY()` API；Baker 累加到 `position.y`。Inflater 不做 metrics 补偿（猜的 ascent 会差 ~16 px 且字体替换后 metrics 还会变）。
    - `53a5609a` font align：PathA 用 `pagx::FontConfig`、PathB 用 `pag::FontManager` 两套割裂。`FontConfig` 加 public `findTypeface()` / `fallbackTypefaces()`；新增 `MakeFontProviderFromConfig()` adapter；`PAGXTest` fixture 内置 Arial+Noto 注册 + `fontConfig()` / `fontProvider()` getter；CrossCheck 两路径走同一 FontConfig。
    - `36471b33` HarfBuzz shaper：`tgfx::TextBlob::MakeFrom(text, font)` 内部 primitive shaper 与 PAGX 的 HarfBuzz 对**同一 typeface 同一 glyph** 给出不同 advance（macOS Arial Bold 84pt 'P' HarfBuzz 49.79 vs CoreText 56，差 ~11%——**不是 kerning 是平台 shaper 本身的差异**）。`FontProvider` 加可选 `getFontConfig()` 虚方法；Inflater 检测 non-null 时改走 `pagx::TextShaper::Shape` + `TextBlob::MakeFrom(glyphIDs[], positions[], count, font)` 重载；nullptr 降级 primitive。
    - `55590003` layoutOrigin：`Text::renderPosition()` 公式 `bounds + offset - textBounds` 对 TextBox child 自抵消为 `(0,0)`（PathA 的 inverseMatrix 路径需要这个减法，但 runtime-shape 没有 inverseMatrix）。新增 public `Text::layoutOrigin()` = `(textBounds.x, textBounds.y)`；Baker 按 `layoutOrigin().x/y == 0` 切换 standalone vs TextBox child 两条路径，y 计算公式也不同（standalone `position.y = renderPos.y + firstBaselineY`，TextBox child `position.y = firstBaselineY`——后者 baseline 已是 TextBox 绝对 y）。**两个 API 不能统一**。
  - **§10.6 新增**："已知限制（Phase 16.7+ 待决策）"——三个无小代价修复的场景：
    - **单 Text 多行换行 + 每行独立对齐**（`text_box.pagx` PSNR ~16 dB）：`ElementTextData::position` 单 Point 表达不出多行不同 x 起点。候选方案 A/B/C 待决策（详见主文档 §10.6）。
    - **TextModifier + RangeSelector 字符级动画**（`text_modifier.pagx` PSNR ~15 dB）：MVP 完全未实现，Phase 16.7 规划项。
    - **嵌入字体 `@font1`**（`glyph_run.pagx` PSNR ~16 dB）：Baker `TextGlyphRunsDowngraded=208` 警告 + 丢弃 pre-shaped glyphRuns；设计未定（资产化序列化 vs 接受 trade-off）。
  - **§10.1 设计原则修正**：原"Inflater 重建：TextShaper::Shape 动态 shape + 复用 v1 TextLayout 做段落布局（justification / box wrap / tracking）"——实施期 Inflater 完全没用 v1 TextLayout、段落布局根本没做（这正是 §10.6 #1 限制的根本表现）。改为"Baker 把 PAGX `applyLayout()` 算出的派生几何（`firstBaselineY` / `layoutOrigin` / `renderFontSize`）显式序列化进 `ElementTextData`；Inflater 端只做 shape，不做段落布局"，并明确 shape 双路径（HarfBuzz / primitive）。

  累积效果：CrossCheck FAIL 15 → 7（`text` / `text_path` / `trim_path` / `container_layout_include_in_layout` / `nebula_cadet` / `game_hud` / `rich_text` 全部 ≥ 30 dB）；总 PAGFullTest FAIL 66 → 56；剩余 7 个 CrossCheck 全部已识别根因（见 §10.6 + memory `project_pagx_to_pag_v2.md`）。

  设计层教训沉淀为 memory `feedback_runtime_shape_baseline.md`（renderPosition / layoutOrigin 双 API 分流 + 派生几何必须 Baker 序列化）+ `feedback_shaper_advance.md`（HarfBuzz vs CoreText 平台 advance 差异，primitive shaper 不可替代）。

---

- **v2.20 Review 收敛轮次**（P0+P1 文档修订，9 项，非编码阶段交付）：

  背景：Phase 16 文本回退设计定稿后，用户要求对整份设计文档做一次完备性 Review。Agent 对架构合理性 / 可扩展性 / 可维护性 / 可读性 / 性能 / 边界情况 6 个维度分派 4 个子代理深度评审，去重汇总得到 15 项建议，按"不超前设计"原则收敛并落地 P0+P1 共 9 项：

  - **P0-18**（§7.1）：Baker 输入 PAGX 生命周期约束写入共同约束——`BakeContext::layerPathByPagxLayer` / `imageIndexByNode` 以 `const void*`（PAGX 节点指针）为 key，依赖 PAGX 结构稳定。补强制条款"Bake 期间禁止并发修改 doc、禁止节点 GC"，杜绝悬空指针与 UAF。
  - **P0-19**（§15.3 + §G.3）：`PAGLoader::Result` 新增 `uint32_t compositionCount` 字段 + §G.3 加 "消费侧 range-check 纪律"段。`InvalidCompositionIndex=306` 等码的 contextIndex 是 Decoder 从字节流读出的未校验原始值，消费方必须用 compositionCount 做范围检查才能安全分派。
  - **P0-20**（§I.5）：Phase 1 开工前加 "varU32 安全预检"清单。3 种 v1 现状 → 3 种 Phase 1 动作（零改 / 补最短形式分支 / 落完整 wrapper），判定结论写入 Phase 1 PR 描述；预检未完成禁止进入 Phase 1 exit gate。
  - **P0-21**（§18.3bis + §18.3ter）：Fuzz corpus 初始种子源规范——CorruptBuilder 为唯一来源；开 fuzz 前 `test/fuzz_corpus/seeds/` ≥ 30 条覆盖 §G.2 Codec 400 段 + Inflater 600 段；本地 corpus 上限 100 MB；禁止在 fuzz 侧复制独立生成器导致两份漂移。
  - **P1-22**（§17 D3 + §19 Phase 15）：覆盖率目标改为分模块——Baker/Codec/Inflater 三大模块各 ≥80%、整体 ≥75%。取代原 "≥85%" 全局口径（与 Phase 15 实测 76.75% 的冲突消除）；工具链辅助头不单列指标。
  - **P1-23**（§8.5 + §3.2 + §16 + 开工必读 #13）：`EncodeSession` 裸指针聚合体升级为独立 `EncodeContext`（继承 DiagnosticCollector，持 StreamContext + debugLayout）。4 Context 形态完全对称——都继承基类、都有 3 参 public `warn`、调用点永远 `ctx->warn(...)`；消除 "`session->diag->pushWarning(...)`" vs "`ctx->warn(...)`" 的双轨困扰。
  - **P1-24**（§18.4quater / §18.4quinta）：微型专项测试合并——`PackLayerPath` 从 3 条合并为 1 条参数化（RoundTripMatrix）；`ZeroCopyScope` OwnershipMatrix 从 2 条合并为 1 条；`ZeroCopyScope.LeakDetection` 采用 ASAN/weak_ptr 双策略，非 ASAN 环境不再 GTEST_SKIP。
  - **精简收敛（非编号）**：(a) §6.1 TagCode 分段表中 `240-299 资源扩展段` 预留删除（无 roadmap 的 SVG/Lottie/HDR 占位，遵循 §2 精简原则）。(b) `PAGExporter::Options::FontMode` 枚举 + `--pag-font-mode` CLI 选项 + `PAGRenderEquivalenceTest::OutlineAll_Baseline` 参数化 test **彻底移除**（避免长期 GTEST_SKIP 腐化；枚举值 206 `TextGlyphDataEmpty` 保留仅为 ABI append-only，新代码禁引用）。

  **未采纳的 Review 建议**（列入"可选"留待实施期再议）：B3 样本特性矩阵表、B6 ElementText D.11 字段序对齐声明、B7 FontProvider const 正确性、B9 UTF-8/图像魔数入口清单、D.14 AnimationData 占位段——均为细粒度完备性补强，不阻塞开工。

---

- **v2.19 → v2.20 修订要点**（Phase 15 实测暴露的架构级回退，非评审团轮次）：

  背景：Phase 15 覆盖率工具落地后实测 76.75%（未达 85%），同时发现 PAGX→PAG 转换后所有含文字样本在 macOS 测试环境下完全不渲染。根因诊断（`Phase12ProbeFixture.DISABLED_TextInflateDiag` 定位）：`tgfx::SystemFont::MakeFromName` 在非 Windows 平台硬 `return nullptr`（见 `third_party/tgfx/src/core/vectors/freetype/SystemFont.cpp:267,276`），Phase 8 "glyph ID 预存序列化" 架构不可行——Inflater 拿不到原始 typeface，TextBlob 必然 null，ElementText 整体丢弃。

  **方向决策**（用户确认）：回退到 v1 风格 runtime-shape。`ElementTextData` 存原始 text + fontFamily/fontStyle 字符串（对齐 v1 `pag::TextDocument`），Inflater 端调 v1 `TextShaper::Shape` + 复用 v1 TextLayout 做段落布局；字体通过新引入的 `FontProvider` 注入接口由调用方提供。

  **主文档变更面**（31 处）：
  - **§2 核心原则 #7** "字体双模式" → "文本 runtime-shape"
  - **§3.3 术语索引** 新增 `FontProvider` / `TextShaper`；标 `GlyphRunBlob` / `FontAsset` 废弃
  - **§6.1 TagCode 分段** `FontAssetTable=3` / `FontAsset=7` 保留编号但标 deprecated
  - **§6.2 body 顶层写入顺序** 移除 `FontAssetTable`（legacy 对照保留）
  - **§6.5 必选 top-level Tag 清单** 从 6 个缩减为 5 个
  - **§7 Baker 职责表** TextBaker 描述更新
  - **§8.2 Encode 流程伪码** `WriteTag(body, FontAssetTable, doc.fonts)` 注释掉
  - **§8.3 档 (i) fatal 语义** 顶层序列描述更新
  - **§8.5 BakeContext** `fontIndexByNode` 等 3 字段移除
  - **§9.4 InflaterContext 错误码表** 601/602 触发路径更新
  - **§10 整章重写**（字体双模式 → runtime-shape 模式）
  - **§11.0 资源生命周期总表** FontAsset 行移除
  - **§11.2 整节废弃**（FontAsset 结构）
  - **§11.3 索引化时机** 去掉 Font pre-pass
  - **§15.1 错误码 ABI 表** `FontSourceMissing=201 / InflateFontCreateFailed=601 / InflateGlyphRunBuildFailed=602 / FontResourceSizeExceeded=408` 的 contextIndex 语义从 fontIndex 改为 layerIndex 或 UINT32_MAX；`GlyphRunKindInferred=208` 重命名为 `TextGlyphRunsDowngraded=208`（新语义，不加码）
  - **§15.2 PAGExporter::Options::FontMode** OutlineAll 注释更新为 "reserved, Phase 17+"
  - **§15.3 PAGLoader::Options** 新增 `fontProvider` 字段
  - **§16 目录结构** 增加 `include/pagx/pag/FontProvider.h`
  - **§17 验收 B3** OutlineAll 模式 SKIP
  - **§18.4bis** GlyphRunBlobCodecTest 3 用例标 DISABLED
  - **§18.7** OutlineAll_Baseline GTEST_SKIP 包装
  - **§19 Phase 表** 新增 Phase 16.0-16.6 共 7 行；阶段门槛 + 依赖矩阵对应扩充；M4 → M5 新增
  - **§20 附录 A** 资源索引化行去掉 `buildFonts`
  - **§21 附录 B** 待确认 #1 GlyphRun 字段集标废弃；#3 OutlineAll 策略标废弃
  - **§C.4** 整节重写为 "ImageAsset（Phase 16 后唯一保留的资源）"；`FontAsset` / `FontAxis` / `FontSourceKind` / `GlyphRunBlob` / `LayoutGlyph` / `ClassicGlyph` / `GlyphRunKind` 结构全部移除
  - **§C.5-pre PAGDocument** 删 `std::vector<std::unique_ptr<FontAsset>> fonts` 字段
  - **§C.7 ElementTextData** 字段集重写为 v1 TextDocument 风格
  - **§D.6** FontAssetTable 字节布局整节标 Phase 16 废弃
  - **§D.11 VectorElement Tags 表** ElementText (50) body 字段重写；`text_flags` 位域新增（BoxText/FauxBold/FauxItalic/ApplyFill/ApplyStroke/StrokeOverFill）
  - **§D.11 GlyphRunBlob inline** 整段标废弃
  - **§D.13 函数签名模板** `WriteGlyphRunBlob` / `ReadGlyphRunBlob` 标 Phase 16 废弃
  - **§G.2 错误码枚举** `FontSourceMissing=201` 新语义（layerIndex）；`TextGlyphDataEmpty=206` 备注"Phase 16 起不再触发"；`GlyphRunKindInferred=208` 重命名 `TextGlyphRunsDowngraded=208`（Phase 16 新语义，ABI 值不变）；`FontResourceSizeExceeded=408` 备注废弃；`InflateFontCreateFailed=601` / `InflateGlyphRunBuildFailed=602` 新语义
  - **§H.5 字体魔数校验** 整节标 Phase 16 废弃（PAGX 自身 HTML/SVG 路径仍可复用参考）
  - **§I.1 公共头** 增加 `pag/FontProvider.h`（tgfx-dependent）
  - **§22 上次开工必读** 条目 5 `FontAsset::data` 移除；新增条目 16 描述 Phase 16 回退；引言 v2.17 → v2.20
  - **§文档维护** PAG v2 版本备注加 Phase 16 ElementText schema 不兼容变更说明

  **Phase 16 子 Phase 拆分**（§19 表格扩充）：
  - 16.0 设计定稿（本次提交，详见 `pagx_to_pag_v2_phase16_text_redesign.md`）
  - 16.1 PAGDocument 结构 + FontProvider 接口
  - 16.2 TextBaker 重写（runtime-shape，pre-shaped 降级发 `TextGlyphRunsDowngraded=208`）
  - 16.3 Codec 读写 schema 对齐；FontAssetTable 写路径移除（读路径保留 `UnknownTagCode=400` warn skip 以兼容老版本开发分支产物）
  - 16.4 LayerInflater `inflateElementText` 重写，接入 `TextShaper::Shape` + v1 `TextLayout` 段落布局 + `FontProvider.getTypeface`
  - 16.5 测试基础设施 `FontProvider` 注入（RenderEquivalenceTest / CrossCheck）
  - 16.6 用户 `/accept-baseline` 接受新 48 张基线 + `coverage.sh` 重跑验证 `LayerInflater.cpp` / `TextBaker.cpp` ≥75%

  **不升 FORMAT_VERSION**：PAG v2 尚未对外发布（仅在本仓内使用），ElementText schema 不兼容变更不构成任何用户 .pag 破坏，继续 0x02。

  **配套交付物**（已提交；文档于 v2.23 整合）：
  - `tools/coverage.sh` + CMake `-DPAG_COVERAGE` 开关 + fuzz 分支 ASAN 剔除（Phase 15）
  - `tools/render_compare.py` + `test/src/pag/unit/GeneratePAGXNativeReferencesTest.cpp` 的 PAGX 原生 48 张参考图生成（Phase 15 辅助）
  - `test/src/pag/unit/Phase12ProbeTest.cpp` 新增 `DISABLED_TextInflateDiag` 文字诊断 probe
  - 本文档 v2.20 修订条目
  - ~~`docs/pagx_to_pag_v2_phase16_text_redesign.md` 独立设计文档（422 行）~~ —— v2.23 起该文档删除，其内容已在本条及 v2.21 / v2.22 条目中完整覆盖

---

- v2.18 → v2.19 修订要点（5 位专家评审团第 10 轮综合：11 P0 含 2 项字节布局重构 + 20 P1 + 10 P2 = 41 项，**用户要求全部 41 项清零含 P0-R1 + P0-R2 字节重构**）：
  - **字节布局重构**（v2 未发布，第二次也是最后一次不兼容重构，v2 后续仅字段追加）：
    - **P0-R1** ImageAssetTable / FontAssetTable 改为**每 asset 独立 sub-Tag**（新增 TagCode `ImageAsset=6` / `FontAsset=7`）——原 repeat 裸结构中字段级追加从根不成立（没有单 asset TagHeader.length 作边界，追加字段第 2 个 asset 起全部错位）。现改为 body 只有 `varU32 assetCount + repeat [ImageAsset/FontAsset sub-Tag]`，每 sub-Tag 自带 TagHeader，字段级追加走 sub-Tag 内 length skip 合法。§6.1 / §D.6 / §C.4 / §11.1 / §11.2 全链路同步。
    - **P0-R2** `LayerTransform=15` sub-Tag **本期就落**——LayerBlock body 顶层移除 `visible/alpha/blendMode/matrix/matrix3D/scrollRect` 6 个 Property 字段，挪入独立 `LayerTransform` sub-Tag。Baker 本期恒写；Reader 缺失推 304 fatal。§4.4 规则 1 "未知 encoding 收窄 skip 到 transform" 立即可用，动画期无需改字节布局避免打脸 §13 "不升版本号" 承诺。§D.8 / §D.9 / §4.4 同步。
  - **文档级 P0 硬矛盾清零**（9 项）：
    - **P0-A** §D.12 Filter/Style TagCode 数值迁移 80-84/100-102 → 120-124/140-142（对齐 §6.1 P1-3 v2.18 迁移声明）；§24 附录 E 复核。
    - **P0-C** `DiagnosticCollector` 基类契约钉死：只暴露 protected `pushError/pushWarning` helper，3 Context 各自 3 参 public `error/warn` wrapper（无 name hiding 歧义）；InflaterContext 物理屏蔽 `errors` / `hasError()`。
    - **P0-D** EncodeContext 合并收尾：删 §8.5 `struct EncodeContext` 片段；引入 `struct EncodeSession { DiagnosticCollector*; StreamContext*; };` 轻量 2 指针聚合；§D.13 Write 签名改 `EncodeSession*`；§3.1 L105 数据流图、§8.1 L514、§18.3bis TagWriterScope 全链路同步。
    - **P0-E** `kAllDiagnosticCodes` size 41 → **43**，补 3 码（`BlendModeUnmapped=204` / `PrematureEndTag=409` / `InflateMaskCycle=607`）。
    - **P0-F** `CodeToString` switch 补 `PrematureEndTag` / `InflateMaskCycle` case。
    - **P0-G** ImageAsset.kind Decoder 强制校验 `kind == 0`（Raster）——kind != 0 但 <= 3 warn 104 + 回退 Raster；**严禁**基于字节流 kind 分派解码器（防未来 Video/Hdr 解码器 UAF 扩面）。
    - **P0-H** FontAsset.axes[] 加 `MAX_FONT_AXES = 64` 上限（对齐 OpenType fvar 表惯例），Decoder 超限推 105 fatal 防 4GB OOM。
    - **P0-I** §C.4 `struct ImageAsset` / `struct FontAsset` 补 `kind` / `axes` 字段 + `FontAxis` struct 定义，与 §11.1 / §D.6 同步。
  - **一致性 / 测试脆弱 / 边界 P1**（20 项）：
    - **P1-01** §2.1 伪码 lambda 清零（§D.2.1 checkCoord → static `PathCoordIsFinite`；§18.2 renderLocal → static `RenderLayerLocal`）。
    - **P1-03** §3.2 末尾加阅读链（§3.2 → §22 14 条 → §3.3 按需）。
    - **P1-04** §8.3 Encode 信任 Baker depth 说明（Encode 不重查；Decode 侧 MAX_LAYER_DEPTH 反拦兜底）。
    - **P1-05** §D.8 LayerBlock 子 Tag 唯一性约束（LayerTransform / LayerMaskRef / LayerFilters / LayerStyles 每种最多 1 次，第 2+ 次推 304 fatal + 整 LayerBlock 丢弃）。
    - **P1-06** §18.3bis `AppendGarbageAfterEnd` 必须搭配 `SetBodyLength(newTotal)` 才触发 409（单独使用 = End 后挂垃圾场景，bodyLength 不变不触发 409）；§G.6 双方法联合构造模板。
    - **P1-07** §18.3pre `asCompositionRef` 延迟符号表 resolve 契约（支持前向引用 + `MakeCompositionRefCycle`）；`withMask` BNF `root("/" uint32)*`（跨 composition assert）。
    - **P1-08** §D.2 `limits::PATH_QUANTIZATION_*` 5 处调用点全迁 `path_format::QUANTIZATION_*`。
    - **P1-09** §15.1 ABI 表补 106 / 409 / 607 contextIndex 语义（607 → layerIndex；106/409 → UINT32_MAX，byteOffset 已定位）。
    - **P1-10** §3.3 `InflaterContext` 术语补 `visitingComposition / currentCompositionDepth / totalInflatedLayers` 3 字段。
    - **P1-11** §8.5 Baker error 消息 `BAKE_MAX_TOTAL_COMPOSITIONS exceeded` → `MAX_COMPOSITIONS exceeded (Baker pre-pass)`（alias 化后用户日志面一致）。
    - **P1-12** `PAGDocumentEquals` 比较规则 `shared_ptr<const tgfx::Data>` 扩三子句（nullptr 双侧等价 / 单侧不等 / 大图 SHA1 hash fast-path，消除 Inflater reset 后比较假阳性）。
    - **P1-13** §16 目录补 `DiagnosticCollector.h` / `DecodeContext.h` / `EncodeSession.h`。
    - **P1-14** §3.1 L105 数据流图 `[EncodeContext]` → `[EncodeSession]`，消除半迁移残影。
    - **P1-15** §D.14 AnimationData sub-Tag 草案（动画期落地前置，本期不产出）：挂载点枚举（ForLayerTransform=160 / ForVectorElement=161 / ForPayload=162）+ propertyId + keyframe 布局模板 + 静态/动画双写优先级 + `MAX_KEYFRAMES_PER_PROPERTY=4096` 预留。
    - **P1-16** §4.3 删 P2-8 bit 6 continuation 语义预定（跟 hasExt 扩字节同构陷阱）；PropertyEncoding 池扩展合法路径收敛到 encoding bit 0-3 的 5-15 槽位 + 升 FORMAT_VERSION 两条。
    - **P1-17** §6.5 ② 禁用清单由 4 项扩为 6 项（补 Composition / LayerBlock）——Composition 缺失 = 整 composition 消失；LayerBlock 缺失 = 整 Layer 子树消失。
    - **P1-18** §8.3ter ErrorMarker 三档语义细化（top-level 间 → 直接 return；Composition 内 → 停当前 Composition 继续下一个；LayerBlock 内 → 丢当前 Layer 剩余子树继续读兄弟）+ 3 条测试登记。
    - **P1-19** §18.3ter Inflater fuzz harness 独立规约（与 Decode fuzz 区分职责；corpus seed 来自 `MakeMaskCycleBytes` / `MakeCompositionRefCycle` / `MakeWideSiblingTree` / `MakeLayerWithKMasksBytes`）。
    - **P1-20** §22 开工必读 12 条 → **15 条**（补 v2.19 架构基线 + 3 个安全校验新规 + 阅读链 / AnimationData 动画期准备）。
    - **P1-21** CorruptBuilder 补 `SetVersionByte(uint8_t)` / `SetMaskLayerPathAt(size_t, vector<uint32_t>)`；`PagxBytesLayout.maskPathOffsets` 由单 offset 扩为 vector。
  - **风格一致性 P2**（10 项）：
    - **P2-01** §6.1 UUID 注册机制简化（去"上游注册"流程；UUID v4 碰撞概率 2⁻¹²² 本就不需要注册；官方 Tencent 固定 UUID 常量化）。
    - **P2-02** 41/43/44 计数标签统一（DiagnosticCode 总数 43，修订项 41，v2.18 历史 41 码标注不同）。
    - **P2-03** §19 Phase 4 明示"不产出 `EncodeContext.h`"——Codec::Encode 内构造 `EncodeSession session{&diag, &sc};` 栈对象。
    - **P2-04** §D.6 `MAX_COMPOSITIONS` 与 Baker `BAKE_MAX_TOTAL_COMPOSITIONS` alias 命名对称注释（避免读感不对称）。
    - **P2-05** §I.3 `DecodeStream::currentReadablePtr()` Phase 1 通过非侵入 wrapper `DecodeStreamExt.h` 交付说明。
    - **P2-06** §18.4quinta `ZeroCopyScope.LeakingDataToTreeIsBlocked` 定稿唯一写法 `EXPECT_DEATH_IF_SUPPORTED` + ASAN 前置；`PAGX_BUILD_WITH_ASAN` CMake 宏。
    - **P2-07** `MakeLayerWithKMasks` 改走字节直喂路径（避免先撞 `BAKE_MAX_TOTAL_LAYERS`）；返回 `(bytes, hostLayerIndex)`。
    - **P2-08** §G.6 规则 0 测试矩阵补 2 行 `PropertyEncoding.EncodingWinsOverIsDefault` / `PropertyEncoding.HasExtOneByteSkip`。
    - **P2-09** §22 阅读链加 "动画期前必读 §D.14"。
    - **P2-10** `VersionReject.FutureVersionRejected` 测试闭环（`CorruptBuilder::SetVersionByte(0x03)` → 301 fatal）；§G.6 登记。
  - **扩展性预留结构性修复**（落地到上述 P0-R1 / P0-R2 / P1-16 / P1-17）：
    - 接收 agent 2 "repeat 裸结构字段级追加从根不成立" 判定，P0-R1 落地 asset 每实体独立 sub-Tag；
    - 接收 "LayerTransform sub-Tag 必须本期就落否则动画期字段级追加通道不通" 判定，P0-R2 落地；
    - 接收 "propHeader bit 6 continuation 与 hasExt 扩字节是同一陷阱" 判定，P1-16 删该预留；
    - 接收 "必选 top-level 清单漏 Composition / LayerBlock" 判定，P1-17 扩 6 项。
  - 未采纳：§18 三级化重排（24 个 bis/ter/quater/quinta 子节清理，P1-02）——本轮工作量已极满，降级为 v2.20 纯目录级整理任务；pre/nix/bis/ter/quater/quinta 导航说明（§1 目录前言）已足够读者导航，暂不阻塞 Phase 实现。

- v2.17 → v2.18 修订要点（5 位专家评审团第 9 轮综合：17 P0 + 16 P1 + 10 P2 = 44 项，**用户要求全部 33 项清零 + 11 项新边界追加**）：
  - **架构精简**（首轮"做减法"）：
    - **P0-2** EncodeContext 退化——原为空壳（warnings + streamContext + depth 重复 Baker），合并为 `DiagnosticCollector + StreamContext` 组合；§3.2 "4 Context" 改为 "3 Context"。
    - **P0-3** DiagnosticCollector 基类抽取——6 处 warn/error 字字相同代码消除 60 行冗余；BakeContext / DecodeContext / InflaterContext 继承；DecodeContext 多出 `currentStream` + 便利 wrapper 自动填 byteOffset。
    - **P2-1** `BAKE_MAX_TOTAL_COMPOSITIONS` alias 化（`= MAX_COMPOSITIONS`）——YAGNI 清理。
    - **P2-2** `MAX_PATH_COORDS / MAX_PATH_CONIC_WEIGHTS` 内联——纯派生量移除。
    - **P2-4** `PATH_QUANTIZATION_*` 移出 `limits::` 到 `path_format::` sub-namespace——语义污染纠正。
  - **ErrorMarker TagCode 矛盾清零**（P0-1）：§6.1 原表同时出现 "ErrorMarker=1 预留" / "改迁 999" / "TagCode=1022" 三处矛盾——统一保留 1022 唯一值。
  - **命名双轨规则写死**（P0-8）：ErrorCode（src/pagx/pag/）vs DiagnosticCode（include/pagx/、test/）按文件路径硬规则，不再含糊 alias。
  - **§G.3 Diagnostic 字段表补 contextIndex 行**（P0-7）——之前只列 3 字段，漏 ABI 必填第 4 字段。
  - **PAGDocumentEquals helper 定义**（P0-9）：§18.1 引用但全文无签名——新增 §18.3nix 规范；Phase 2 交付登记。
  - **§G.6 测试矩阵补行**（P0-6）：`ProducerFatal=106` / `MAX_PENDING_MASKS` 两新码原只在代码无测试；补 `Truncate.BakerProducedErrorMarker` + `InflaterParity.PendingMaskCapExceeded` + `InflaterParity.MaskCycleTwoLayers/MaskSelfReferenceByteStream` + `Truncate.PrematureEndWithUnreadBody`。
  - **StructBuilders 扩 3 helper**（P0-10）：`MakeWideSiblingTree / MakeLayerWithKMasks / MakeCompositionRefCycle`——覆盖宽度 / mask chain / composition cycle 结构性用例。
  - **CorruptBuilder 扩 4 方法**（P0-11）：`AppendErrorMarkerTag / SetCompressionByte / SetBodyLength / SetCompositionIndexAt / AppendGarbageAfterEnd` 消除 41 码中 106/302/305/306/409 触发路径缺口。（注：v2.19 起 DiagnosticCode 总数已 = 43 码；本条仍标 41 码仅为 v2.18 当时历史纪录，见 §5807 `kAllDiagnosticCodes` 当前权威数值。）
  - **可扩展性预补**（2 年后演进）：
    - **P0-4** §6.5 ② 明示"FileHeader/ImageAssetTable/FontAssetTable/CompositionList 必选 top-level 不走变体路径"——避免走 V2 路径让旧 Reader 丢失 canvas 尺寸整文件不可播。
    - **P0-5** §4.4 动画期 UnknownPropertyEncoding skip 粒度分层——Layer 级 Property 打包进 `TagCode::LayerTransform=15` 子 Tag，收窄丢失到 transform 而非整 Layer。
    - **P1-2** ImageAsset 加 `kind` 字段（Raster/Svg/Video/Hdr 预留）；FontAsset 加 `axes[]`（Variable Font 预留）——字段级追加，纯向前兼容。
    - **P1-3** VectorElement 段扩 40-79 → 40-119（80 槽）；相应 LayerFilter/Style/动画段顺延——为 3D + AE modifier 预留足够深度池。
    - **P1-5** 第三方实验段 900-1021 加 16 byte UUID 命名空间隔离规约——消除 fork 冲突。
    - **P1-4** §6.5 ② 变体 Tag 双写优先级规约——新 Writer 必须同时写旧 TagCode 作 fallback；新 Reader 遇两者并存以"后出现"为准（LIFO override）。
    - **P2-7 / P2-8** PropertyEncoding / propHeader bit 6-7 扩展通道设计——bit 6 预定义为 propHeader continuation 路径（非 hasExt 通道），避开 §4.3 hasExt 红线。
    - **P2-9** 动画静态 fallback 规约——Writer 同时写 Constant 首帧 value + 独立 AnimationData sub-Tag；旧 Reader 退化为静态首帧，非整层消失。
  - **边界条件补齐**（11 项新攻击面）：
    - **P0-12** ReadPathV1 warn 路径改 error——避免字段错位连锁污染整 Tag 语义。
    - **P0-13** varU32 非最短形式校验 → `ReadEncodedUint32Shortest` 严格 wrapper，强制用于 compositionIndex / imageIndex / fontIndex / childIndex 等安全关键索引字段——消除 normalization bypass + 二次 hang。
    - **P0-14** ChoosePathFormat `std::abs(INT32_MIN)` UB 修复——int64_t 中间结果 + `llroundf`。
    - **P0-15** 中途 End Tag 截断检测 + 新错误码 `PrematureEndTag=409`。
    - **P0-16** Layer mask 循环 A↔B↔A 检测 + 新错误码 `InflateMaskCycle=607`；§12.2 Pass 2.5 加 Tarjan SCC。
    - **P0-17** §H.5 字体 magic 白名单（TTF/OTF/TTC/WOFF/WOFF2）——对齐图片侧 §H.4，堵 FreeType/CoreText CVE 投递路径。
    - **P1-13** `ReadRatio` 强制 denominator ≥ 1——防 frameRate/stretch 除零 SIGFPE。
    - **P1-14** `ImageAsset.width/height` i32 负数校验——防 negative×negative 回绕 heap OOB。
    - **P1-15** propHeader 三位优先级规则 0——消除 isDefault=1 + encoding 非 0（0x11）的解析歧义。
    - **P1-16** §4.3 extHeader bit 语义红线——"不得影响 value 字节数或字段顺序，仅承载纯信号位"。
    - **P2-11** mask 自指 + Composition 0×0 + `-0.0 vs +0.0` round-trip 字节稳定三条防御。
  - **兼容性对齐**（v1 对称）：
    - **P1-1** `constexpr uint8_t KnownVersion = 0x02;` 引入——与 v1 对称，为未来 0x03 升级铺路。
    - **P1-8** §2.1 伪码风格约束——消除 §D.2.1 checkCoord / §D.2.2 popPt / §18.2 renderLocal / §D.2.2 path.decompose 4 处 lambda 违规 Code.md；ReadPathV1 骨架改为自由函数。
    - **P1-7** §19 Phase 表加"阶段依赖矩阵"+ Phase 4 可拆 4a/4b + Phase 12 首次基线缺失流程说明。
    - **P1-11** 测试命名 §19 vs §G.6 统一为 gtest 二段式 `Suite.CaseName`。
    - **P1-9 / P1-10 / P1-12** Fuzz 独立 PAGInflaterFuzzTest harness / Phase 12 首次 PR ~12 基线 FAIL 预期 / ZeroCopyScope 默认 owning + UAF-positive regression 三类测试覆盖补齐。
  - **可维护性**：
    - **P2-5** §3.3 术语与权威定义索引——22 个核心术语 → 唯一章节表，消除散落。
    - **P2-6** §G.1 新开段规则（Fatal 偶百段 / Warning 奇百段）——`StageOf` 稳定判定。
    - **P2-10** §18.12bis CI 时长分拆规划（PR 10-12min / nightly 24h fuzz matrix）+ CR 维护增量清单 8 步。
    - **P1-6** 目录加 "bis/ter/quater 章节导航说明"——未做全文重排（风险大），但点明现状 + 未来重构路径。

- v2.16 → v2.17 修订要点（5 位专家评审团第 8 轮综合：6 P0 + 10 P1 + 6 P2，**用户要求再次清零**）：
  - **P0-1（安全）§D.1 `ReadLengthPrefixedBytes` 默认 owning + ZeroCopyScope 显式开关**。v2.16 P1-9 把 wrapper 改返回 `shared_ptr<const tgfx::Data>` 优先 `MakeWithoutCopy`——但 tgfx::Image 惰性解码会把 `shared_ptr<Data>` 持有到 Codec::Decode 退栈之后，PAGLoader 释放 backing buffer 即 UAF。本版本默认走 `MakeAdopted(unique_ptr<uint8_t[]>)` owning 路径；零拷贝须在显式 `ZeroCopyScope::DecodeLocal` 作用域内才允许；`ImageAsset::data` / `FontAsset::data` 必须 owning，禁止 MakeWithoutCopy 产物进入 PAGDocument 树。
  - **P0-2（安全）§H.1 + §9.4 `MAX_PENDING_MASKS=262144` 独立 cap**。v2.16 P0-3 加 606 `reserveLayerBudget` 管 Layer 总数，但单 Layer 可挂 N 个 mask——恶意构造 N×MAX_INFLATED_LAYER_COUNT 个 PendingMask 条目绕过 606 耗 vector 内存。本版本新增 `MAX_PENDING_MASKS=262144` + `InflaterContext::reservePendingMaskSlot()`；Pass 1 push pendingMasks 前校验，超限丢弃绑定 + warn 603。
  - **P0-3（API）§9.2 / §11.1 / §12.2 / §G.5 补 `ctx->warn` 3-arg 调用示例**。v2.16 P0-1 把 Context warn 签名改 3-arg 但伪码无一处演示 contextIndex 传参。本版本补 4 个关键点：`§11.1` ResourceBaker.bakeImages `ImageSourceMissing` 传 PAGX images[] 索引；`§12.2` Pass 2 `InflateMaskResolveFailed` 传 pendingMasks 索引；`§G.5` ReadEnum `InvalidEnumValue` 传越界 raw u8 值；`§9.4` reserveLayerBudget 参数 `uint32_t layerIndex`。
  - **P0-4（实现）§9.2 Pass 1 C++ 骨架**。v2.16 P0-3 "空壳占位/nullptr 槽位/addChild 跳过" 只有散文描述，Phase 9 实现者无参考。本版本补完整 inflateLayer 伪 C++（reserveLayerBudget 先行 + children 循环 + reservePendingMaskSlot + layerByPath 写入），闭环所有降级路径逻辑。
  - **P0-5（实现）§18.3bis `CapturePagxLayout` 实现方案**。v2.16 P0-5 钉 `PagxBytesLayout` 结构 + helper 签名，但"offset 从哪来"只说 "debug build 从 PAGExporter 读取"——实现方式无参考，Phase 4 测试阻塞。本版本加 "(a) PAGExporter 测试钩子" 推荐方案（`TagWriterScope` 在写 ShapeStyleData 时记 offset + DecodeContext 挂 layoutCapture 副产物）+ "(b) 字节流反向扫描" 回退方案；双条件守卫 `#if defined(PAGX_DEBUG_OFFSETS) && defined(PAG_BUILD_TESTS)`。
  - **P0-6（实现）§19 Phase 2 登记 `support/StructBuilders.h` + `FontAsset` 类型迁移**。v2.16 P0-4 在 §18.3bis 引入 `StructBuilders.h`（`MakeDeepLayerStack`）但 §19 Phase 2 表未登记；P1-6 钉 FontAsset 升级 shared_ptr 但同样无 Phase 表交付项。本版本 Phase 2 产品代码列补两项：StructBuilders.h + FontAsset.data 类型迁移；PAGXBuilderTest 同批扩至 ≥13 条。
  - **P1-1（API）§9.4 `reserveLayerBudget(uint32_t layerIndex)` + `CompositionVisitScope` idx 透传**。v2.16 给 `Diagnostic.contextIndex` + 4 Context 3-arg 签名，但 606 `InflaterLayerBudgetExceeded` / 605 `InflateCompositionCycle` 的 contextIndex 来源没有闭环。本版本：reserveLayerBudget 加 layerIndex 参数透传到 warn；CompositionVisitScope dtor 前 idx 传入 warn；§15.1 ABI 表 606/605 → "first Layer triggering budget exhaustion" / "compositionIndex of cyclic reference"。
  - **P1-2（API）§15.1 docstring + §G.4 消费示例 UINT32_MAX 守卫强制**。v2.16 P1-10 加了 3 个消费场景，但示例 (b) LoadFromBytes / (c) InflateCompositionCycle 不检查 `contextIndex != UINT32_MAX` 就直接打 `#%u`——某些 bridge path 会推 sentinel。本版本 `Diagnostic.contextIndex` docstring 加 "**MANDATORY GUARD**" 段；两个示例补 `if (d.contextIndex == UINT32_MAX)` 分支。
  - **P1-3（架构）§6.1 + §8.3ter + 错误码 106 Baker 中断字节流预警**。Baker fatal 发生在 Codec 已产出部分字节后，下游 Decoder 无法区分 "legit EOF" 与 "producer fatal truncation"。本版本：§6.1 TagCode 段保留 1022 `ErrorMarker`；§8.3ter 规定 Codec 在 Baker fatal 时写入 `TagCode::ErrorMarker` 零 body Tag；Decoder 读到后推 `ProducerFatal=106` warning + 停止当前 Composition 解析；kAllDiagnosticCodes / CodeToString 同步。
  - **P1-4（实现）§19 Phase 1/2 exit gate + `.github/workflows/pagx-fuzz.yml` 登记**。Phase 1 补 `grep -rn "std::vector<uint8_t>" src/pagx/pag/` 零命中于 ValueCodec/Codec 上下文；Phase 12 登记 fuzz yaml 交付物。
  - **P1-5（实现）§6.1 Tag 段 ErrorMarker=1022 预留**。v2.16 及之前 TagCode 段"实验性"900-1022 无具体分配，ErrorMarker 新增时无归属。本版本 §6.1 表把 1022 独立分段为 "ErrorMarker" 专用，900-1021 保持实验性。
  - **P1-6（实现）§19 Phase 12 CI yaml 登记**。§18.3ter Fuzz CI 有详细 spec 但 §19 阶段表未列 `.github/workflows/pagx-fuzz.yml` 为交付物。本版本 Phase 12 产品列补 CI yaml。
  - **P1-7（实现）§19 Phase 2 `FontAsset.data` 类型迁移登记**。v2.16 P1-6 钉了 `vector<uint8_t>` → `shared_ptr<const tgfx::Data>` 升级，但 §19 无任务归属，AI 在 Phase 2 落 Baker 时会漏升级。本版本 Phase 2 表添加"FontAsset::data 类型迁移" 显式交付项。
  - **P1-8（实现）§18.3pre Phase 2 exit 钉 13 条**。v2.16 P2-9 只说 "PAGXBuilderTest ≥ 15 条" 含糊——核心 7 + 扩展 8 = 15 但扩展后半 8 条用 ">= 8" 描述。本版本钉死 **13 条**（7 核心 + 6 扩展非 trivial；trivial passthrough 8 个 setter 豁免），Phase 2 交付判定按 13 计数。
  - **P1-9（安全）§22 "上次开工必读" 加 #11 enter/exit 纪律 + #12 contextIndex 强制传参**。200 + 行日志仍未能让 AI 接手时看到 v2.16 P1-2 + P0-1 最关键两个闭环约束。本版本 10 条扩为 12 条，第 11/12 条对准"实现阶段最容易漏"的两个底线。
  - **P1-10（同 P1-9）**：见 #11 enter/exit 纪律摘要合并。
  - **P2-1（实现）§19 Phase 8 测试名称分裂 `ResourceOversize`→`ImageOversize/FontOversize`**。v2.16 P0-2 拆了 402/408 两码，但 Phase 8 测试命名仍为 `TruncatedDecodeTest.ResourceOversize` 合并形式，会让 AI 写一个测试同时触发两码模糊边界。本版本分裂为 `Truncate.ImageOversize` + `Truncate.FontOversize`。
  - **P2-2（实现）§18.3pre PAGXBuilder 公有/私有可见性纪律**。v2.16 P0-6 引入 fluent API 但未说状态字段公私可见性。本版本补 "fluent 方法 public、状态 private，测试不访问字段" 约束。
  - **P2-3（API）§15.2 + §15.3 Result 对称性交叉引用**。v2.16 两个 Result 独立定义，"ok ⟺ errors.empty() 契约"重复三遍。本版本两处 docstring 互相 `see §15.x Result` 强制语义同步。
  - **P2-4（实现）§19 Phase 0 CR 维护清单**。v2.16 P0-6 钉了 `kAllDiagnosticCodes` 但新增 enum 依赖手动同步 5 个地方（enum 声明 / 数组 / 数组 size / CodeToString / G.6 矩阵 / 15.1 ABI 表），缺一 Phase 0 挂。本版本 Phase 0 交付判定列新增 5 条 checklist。
  - **P2-5（同 P2-4）**：并入 CR checklist 条。
  - **P2-6（实现）§9.4 visitingComposition 无重入契约**。v2.16 P2-1 清了"多次 Inflate 旧 doc 路径"散文但 debug assert 缺失。本版本加 `assert(visitingComposition.empty())` 入口断言，明示无重入意图。

- v2.15 → v2.16 修订要点（5 位专家评审团第 7 轮综合：7 P0 + 11 P1 + 10 P2，**用户要求再次清零**）：
  - **P0-1（架构）§8.5 4 Context warn/error 加 `contextIndex` 参数**。v2.15 加 `Diagnostic.contextIndex` ABI 字段，但 4 Context 的 warn/error 只接 `(code, msg)`，推 push 时 contextIndex 恒 UINT32_MAX——ABI 承诺永远违约。本版本 EncodeContext/DecodeContext/BakeContext/InflaterContext 四处 warn/error 签名改 `(code, msg={}, uint32_t contextIndex = UINT32_MAX)`，Diagnostic 构造 4 字段齐备；`syncFromStreamContext` 的 bridge push 显式传 UINT32_MAX（跨 Tag sync 无 contextIndex 语义）。
  - **P0-2（API）§15.1 `ResourceSizeExceeded=402` 拆码**。原 402 同时服务 image/font，§15.1 ABI 表写"用 message prefix 区分 image/font index"——直接违反 `message` 禁 switch 红线。本版本重命名 `ResourceSizeExceeded=402` 为 `ImageResourceSizeExceeded=402`（ABI 数值不变）+ 新增 `FontResourceSizeExceeded=408`（段内追加）；`kAllDiagnosticCodes` / CodeToString / G.6 / §H.1 / §H.2 同步。`ReadLengthPrefixedBytes` 加 `errorCode` 参数让调用点传入正确的 code。
  - **P0-3（安全）§9.2 空壳占位也必须 `reserveLayerBudget`**。v2.15 P1-3 加 606 `InflaterLayerBudgetExceeded` + `MAX_INFLATED_LAYER_COUNT=1e6`，但 §9.2 Pass 1 空壳占位 `tgfx::Layer::Make()` 不走预算——攻击者构造 10^7 次降级路径即可绕过 606 爆 GPU 堆。本版本 Pass 1 明示：占位前 `ctx->reserveLayerBudget()`，失败则该槽位 `layerByPath[path] = nullptr`、父 addChild 跳过，Pass 2 Mask 查到 nullptr 走 603 降级。
  - **P0-4（实现）§18.3bis `MakeDeepLayerStack` 签名/实现样板**。v2.15 只列 helper 调用示例，签名/参数传递/内部实现全缺，Phase 4 测试作者无输入。本版本补完整样板：`inline std::unique_ptr<PAGXDocument> MakeDeepLayerStack(PAGXBuilder&& builder, int depth)` + 10 行实现（open root + N×enterLayer 叶子 addRectangle + N×exitLayer + close + Build），放 `test/src/pag/support/StructBuilders.h`。
  - **P0-5（实现）§18.3bis CorruptBuilder offset 定位助手**。v2.15 加 6 个 `*At(offset)` 方法但 offset 怎么得到没说，测试作者手算字节布局易错。本版本引入 `PagxBytesLayout` 结构 + `CapturePagxLayout(bytes)` 助手（debug build 从 PAGExporter 读取关键字段 offset），测试用 `auto layout = CapturePagxLayout(v); builder.WrapInnerLengthAt(layout.firstShapeStyleStart)` 风格调用，消除手算。
  - **P0-6（实现）§G.3bis `kAllDiagnosticCodes[]` 表归属**。v2.15 `CodeToString.AllEnumValues` 测试依赖此表但"手写展开 / 独立表"二选一没钉死，维护者易漏。本版本钉死 `DiagBuild.h` 提供 `inline constexpr std::array<DiagnosticCode, 40> kAllDiagnosticCodes`；§19 Phase 0 交付物加一行登记；新增 enum 值必须同步加入本表否则 static_assert + 运行时断言双挂。
  - **P0-7（实现）§19 Phase 4 出口 scope 补 304**。v2.15 Phase 4 scope 写 `300/301/302/303/305/306`，漏 `304 MalformedTag`——但 §G.6 矩阵把 `TruncatedDecodeTest.Truncate.InMidTagPayload`（必触 304）放 Phase 4 交付，AI 按 scope 跳过 304 实现测试必挂。本版本 Phase 4 scope 改为 `300/301/302/303/304/305/306`；§22 "开工必读" 第 10 条同步。
  - **P1-1（安全）§D.2.1 format=0 Path NaN/Inf 校验**。v2.14 P0-5 钉了 FileHeader canvas 尺寸防 NaN/Inf，但 format=0 裸 float Path 的坐标和 conicWeight 未校验。攻击者塞 `0x7FC00000`(NaN)/`0x7F800000`(Inf) 让 rasterizer UB 或死循环。本版本 §D.2.1 Decode 伪码加 `checkCoord` lambda（`isfinite && |v| < PATH_QUANTIZATION_MAX_COORD`）+ `READ_POINT_SAFE` 宏；conicWeight 校验 `isfinite && (0, 1e6]`；任一不过即 warn MalformedTag + 返回空 Path。format=1 量化路径天然受 int24 钳制，无需补。
  - **P1-2（安全）§8.5 BakeContext enter/exit depth 下溢防护**。v2.15 P0-4 enterLayer/enterVectorGroup 返回 bool 但 exitLayer/exitVectorGroup 无条件 `--depth`——调用侧若"enter 失败后误配对 exit"或早退路径错误，depth uint32_t 从 0 下溢到 UINT32_MAX，后续所有 enter 立即失败把 MAX_DIAGNOSTICS=1000 塞满掩盖真实信号。本版本两处 exit 加饱和保护 `if (depth > 0) --depth`；纪律注释"enter 返回 false 即整子树 return，不配对 exit"。
  - **P1-3（API）§G.3bis `FormatDiagnostic` + DiagnosticTest 覆盖 `contextIndex`**。v2.15 加 contextIndex ABI 字段但 FormatDiagnostic 未输出，用户 `std::cerr << FormatDiagnostic(d)` 看不到 "哪张图 missing"。本版本实现加 `if (contextIndex != UINT32_MAX) snprintf " #ctx=%u"`；§15.1 FormatDiagnostic docstring 补 3 个示例；DiagnosticTest 从 9 → 10 条加 `FormatDiagnostic.WithContextIndex` 断言。
  - **P1-4（API）§15.1 contextIndex 表补 606**。v2.15 P0-3 Diagnostic.contextIndex + P1-3 InflaterLayerBudgetExceeded=606 同轮引入，交叉覆盖漏；606 的 contextIndex 语义无权威。本版本表加 "606 → layerIndex of first Layer triggering budget exhaustion in failing subtree"；rationale "14 个码" → "15 个码"。
  - **P1-5（实现）§10.3 GlyphRunBlob 字段集钉死**。v2.14 P1-I4 加了 Fuzz 层但 §10.3 仍有 `...（TODO：开工时查 GlyphRunRenderer 确认）` Phase 8 阻塞。本版本直接对照 `src/renderer/GlyphRunRenderer.h` + `src/pagx/TextLayout.h:TextLayoutGlyphRun` + `include/pagx/nodes/GlyphRun.h` 三个 source-of-truth 文件，钉死字段集：kind=0 LayoutRun 取 `{glyphs, positions, xforms?}`；kind=1 GlyphRun fallback 取 `{offsetX, offsetY, glyphs, positions, optMask: anchors/scales/rotations/skews}`；font 由 fontIndex 查表重建；bounds/fontLineHeight 运行时推算不序列化。
  - **P1-6（性能）§11.2 FontAsset.data 升级 shared_ptr<tgfx::Data>**。v2.15 P1-7 rationale 说"开工首周同步升级"但无任务归属，§19/§22 都没提。本版本直接落地：FontAsset.data 从 `vector<uint8_t>` 改 `shared_ptr<const tgfx::Data>`（与 ImageAsset 对称），Inflater `MakeFromBytes` 成功后立即 `data.reset()`；`PAGLoaderTest.FontBytesReleasedAfterInflate` 断言 `use_count==1`；§11.2 去重 Layer 2 Embedded 直接用 `tgfx::Data*` 指针 key（不再保留 vector 兼容层）。
  - **P1-7（实现）§18.3pre asCompositionRef 时序**。v2.15 PAGXBuilder API 列 `asCompositionRef(id)` 但与 enterLayer/withName/withMatrix 并列，时序歧义。本版本明示："先 `enterLayer(LayerType::CompositionReferenceLayer)` 后调 `asCompositionRef(targetId)`"——与其他 setter 同模式；内部 assert 当前 layer 是 CompositionReferenceLayer 否则 debug 死 / release silent skip。Build 路径新增 `BuildWithoutLayout()` 备份为测试 `LayoutNotApplied=100` 路径使用。
  - **P1-8（性能）§9.4 PackLayerPath inline + span 签名**。v2.15 只 decl 不 def + `const vector<uint32_t>&` 参数；10000 Layer pre-pass 非 inline 调用 + 每路径 vector heap alloc。本版本 header-only `inline` 定义 + 签名 `(const uint32_t*, size_t)` 消除 vector 分配；保留 `(vector)` 重载兼容；打包实现 6 级 × 10 bit + 6 bit depth。
  - **P1-9（性能）§D.1 `ReadLengthPrefixedBytes` 跳零初始化**。v2.15 wrapper 产出 `vector<uint8_t>(n)` 触发 n 字节 memset-0，50 MB 字体单次 ~20 ms 纯浪费。本版本改返回 `shared_ptr<const tgfx::Data>`：优先 `MakeWithoutCopy` 零拷贝引用 DecodeContext backing buffer；否则 `unique_ptr<uint8_t[]>`（不初始化）+ `MakeAdopted` 接管。
  - **P1-10（API）§G.4 contextIndex 端到端消费示例**。v2.15 引入 contextIndex 但 §G.4 惯用写法只演示 `code==LayoutNotApplied` 分支，零示例 contextIndex 回读。本版本补 3 个典型场景代码片段：(a) Exporter ImageSourceMissing → `pagxDoc->images()[d.contextIndex].filePath` 提示 UI；(b) Loader ImageResourceSizeExceeded → `LogWarn("Skipped oversized image #%u")`；(c) InflateCompositionCycle → `LogError("Composition cycle at %u")`；特别提醒 306 InvalidCompositionIndex 的 contextIndex 是原始字节值可能越界，消费前必须 range-check。
  - **P1-11（API）§15 `Options::strict` 选型指南**。原 docstring 只讲"promoted to errors"，没告诉用户 strict=true vs 默认+post-hoc SeverityOf 过滤如何选。本版本加 2 条决策线：(a) 批量/CI 场景用 `strict=true`（任何 degradation 即拒）；(b) UI/播放器场景保默认 + 调用方按 `SeverityOf == Warning && d.code == 特定码` 精细分档（允许部分资源缺失降级）。
  - **P2-1（架构）§9.4 visitingComposition resize 契约修正**。v2.15 注释说"Decoder 产出 PAGDocument 可被多次 Inflate"与 v2.15 P0-1 `Inflate(unique_ptr)` 取所有权冲突（doc 移出后不可再用）。本版本注释改为"单次入口 assign，无重入承诺；Inflate 取 unique_ptr 所有权已在签名层禁止多次 Inflate"。
  - **P2-2（API）§15.1 306 contextIndex 范围注释**。306 `InvalidCompositionIndex` 的 contextIndex 是字节流中未校验的原始值（可能 ≥ `doc.compositions.size()`），消费方若裸 `compositions[contextIndex]` 即 OOB。本版本 ABI 表显式注明 "raw value; MAY be ≥ doc.compositions.size()! Consumers MUST range-check before indexing"。
  - **P2-3（架构）§H.2 `checkLimit` 死引用清理**。原 §H.2 实施点列 `ctx.enterLayer / enterVectorGroup / checkLimit(...)` 三个 helper，但 BakeContext 只定义了前两个 + `registerComposition`；`checkLimit` 是死引用。本版本改为列举 `enterLayer / enterVectorGroup / registerComposition` 三个，并加"enter 返回值不得忽略"纪律代码对比（✅ / ❌）。
  - **P2-4（架构）§H.1 `BAKE_MAX_TOTAL_COMPOSITIONS` 独立**。v2.15 P0-4 Baker 独立上限但 composition 这一项复用 `MAX_COMPOSITIONS=1000`，语义依赖 Decoder 常量破坏 Baker 独立性。本版本新增 `constexpr uint32_t BAKE_MAX_TOTAL_COMPOSITIONS = 1000;`（初值同 MAX_COMPOSITIONS 但解耦），BakeContext.registerComposition 改用新常量。
  - **P2-5（API）§15 Result 命名规则锚**。未来加 facade（`PAGValidator` 等）时命名可能抖动。本版本补"所有 facade 类采用嵌套 `ClassName::Result { ok, errors, warnings, [payload...] }`，`ok ⟺ errors.empty()` 契约一致；轻量变体加后缀如 `PeekResult`/`ValidateResult`"。
  - **P2-6（API）§15 Options 独立 rationale**。防未来评审 AI 重复提议合并 `Options` 为 `CommonOptions`。本版本补 "Options 保持独立——`fontMode` 仅 Exporter；未来 `maxLayerBudget`/`renderThread` 可能仅 Loader；强合会反复撕裂语义。**不合并**是刻意设计"。
  - **P2-7（性能）§8.5 `imageIndexByKey` 嵌入路径优化**。v2.15 P1-7 两层去重但 Layer 2 用 `variant<Data*, string>` 构造/哈希开销重。本版本拆三层：`imageIndexByNode`（PAGX ptr）+ `imageIndexByDataPtr`（内嵌 `tgfx::Data*` 独立表）+ `imageIndexByKey`（URI/绝对路径 string）。查表按来源分支，无 variant 开销。FontAsset 同。
  - **P2-8（实现）§18.3ter Fuzz CI 工程化**。v2.15 只说"24 CPU·小时/周"，漏 GitHub Actions 6h 硬超时 + corpus 持久化。本版本补完整 CI 规划：4 parallel job × 6h matrix shard、`-fork=1 -max_total_time=21000`、corpus 用 `actions/cache@v4`（key 含分支/sha）+ 超 5GB 退化 S3/OSS、crash 最小化后自动 commit 到 `test/fuzz_corpus/crashes/` 开 issue、coverage `feat=` 指标 2 周不增长提示 corpus 耗尽。
  - **P2-9（实现）§18.3pre PAGXBuilder 测试覆盖扩至 15+ 条**。原 7 条断言无法覆盖 15 个 setter。本版本核心 7 条 + 扩展 8 条（Doc 默认值 / frameRate+duration / alpha / Ellipse/Path/Stroke / CompositionRef 时序 assert / BuildWithoutLayout），并明示 "trivial passthrough 豁免" 规则。
  - **P2-10（实现）§G.6 Baker 累计器独立用例**。v2.15 P0-4 加了 3 个 `BAKE_MAX_TOTAL_*` 累计器但 §G.6 矩阵没有对应测试行，调用侧靠 message 区分。本版本加 3 行：`BakerEdgeCases.BakeTotalLayerCount` / `BakeTotalVectorElementCount` / `BakeTotalCompositionCount`（均 PAGXBuilder 构造超限树）。

- v2.14 → v2.15 修订要点（5 位专家评审团第 6 轮综合：7 P0 + 13 P1 + 8 P2，**用户要求清零**）：
  - **P0-1（架构）§9.1 Inflate 签名改取所有权**。原 `static Result Inflate(const PAGDocument&)` 与 v2.14 §11.1 ImageAsset.data shared_ptr 释放纪律冲突（`const doc` 禁 `data.reset()`）；mutable 成员又违反 §Code.md 反 mutable。本版本改 `static Result Inflate(std::unique_ptr<PAGDocument>)`，docstring 明示"Inflate 后 doc 不可再用"；PAGLoader.cpp 骨架 + smoke 代码同步 `std::move(decoded.doc)`。
  - **P0-2（API）§15.2 tgfx-free 措辞修正**。原"整条链路不触碰 tgfx 符号"语焉不详——PAGXDocument.h 明明传递依赖 `tgfx::Data`。本版本改词"自身不依赖 tgfx 渲染层（不出现 `tgfx::Layer` / `tgfx::Surface`），但经 PAGXDocument.h 传递依赖 `tgfx::Data` / `Color` / `Matrix` 等核心数据类型；渲染子系统不拉入 export-only 用户的 include 图"。
  - **P0-3（API）§15.1 Diagnostic 加 contextIndex**。原结构只有 `{code, message, byteOffset}`；高频需求"哪张图 missing / 哪个 layer 循环"只能 grep 英文 message，违反 `message 不稳定`纪律。本版本加 `uint32_t contextIndex = UINT32_MAX` + "code → contextIndex 语义"ABI 表（覆盖 200/201/202/203/206/208/306/402/407/600/601/602/603/605 共 14 个码）。
  - **P0-4（架构）§8.5 + §H.1 Baker 侧独立 count 上限**。原 `limits::MAX_*` 仅约束 Decoder 字节流；PAGX XML 恶意输入可构造 2^31 Layer 让 Baker 先 OOM。本版本 BakeContext 新增 `totalLayerCount / totalVectorElementCount / totalCompositionCount` 累计器，`enterLayer / enterVectorGroup / registerComposition` 辅助自带上限校验；§H.1 新增 `BAKE_MAX_TOTAL_LAYERS=1e5` / `BAKE_MAX_TOTAL_VECTOR_ELEMENTS=1e6`。
  - **P0-5（安全）§D.5 FileHeader canvas 尺寸上限**。原 `f32 width/height` 读完后 `static_cast<int>` 暴露到 PeekResult——NaN/Inf/> 2^31 触发 UB；下游 `tgfx::Surface::Make(w,h)` 按 `w*h*4` 算纹理字节数 32-bit 乘法回绕 → heap OOB。本版本 §D.5 Read 伪码补 `isfinite + 0<w≤MAX_CANVAS_DIM` 校验，§H.1 新增 `MAX_CANVAS_DIM=16384`。
  - **P0-6（实现）§18.3pre PAGXBuilder API 规范**。v2.14 把 PAGXBuilder 提前到 Phase 2 但 0 spec，Phase 2 写单测的 AI 无输入。本版本新增 §18.3pre 列 fluent API（`Doc / withFrameRate / openComposition / enterLayer / withMask / enterVectorGroup / addRectangle / addFill / withText / withImageBytes / asCompositionRef / Build`），与 §18.3bis CorruptBuilder 对称。
  - **P0-7（实现）§19 Phase 4 出口 307 错位**。Phase 4 scope 写"TruncatedDecodeTest 覆盖 300/301/302/303/305/306/307"，但 307=FileReadFailed 只能 `LoadFromFile` 触发，不属 Codec::Decode。本版本 Phase 4 scope 改为 `300/301/302/303/305/306`，307 推至 Phase 10.5（`PAGLoader.LoadFromFile_Missing`）；605 推至 Phase 9；404/405/402/403 按 Tag 分批。
  - **P1-1（架构）§9.2 inflateCompositionRef nullptr 降级**。605 表格说"降级为空 Layer"但 inflateCompositionRef 返回 nullptr，调用侧未定义处理——直接跳过 addChild 会让 Baker 的 layerPathByPagxLayer 与 Inflater 的 layerByPath 子索引错位，连带触发 603 误报。本版本 §9.2 Pass 1 伪码明示"子 Layer 为 nullptr 时用 `tgfx::Layer::Make()` 空壳占位"保持索引连续。
  - **P1-2（架构）§22 维护日志加"上次开工必读"摘要**。200+ 行日志无优先级索引，新接手 AI 被淹。本版本文档维护段开头加 10 条硬约束摘要 + "历史完整修订记录"子节折叠，v2.0-v2.15 按版本倒序列出但明示"实现阶段不通读"。
  - **P1-3（安全）§9.4 InflaterContext 全局 Layer 预算**。Decoder 单 Layer 上限合法不等于累积合法——root→10 子→10×10→... 深 6 层 10^6 单层全过，Inflater 实例化百万级 tgfx::Layer 耗尽 GPU 内存。本版本 InflaterContext 加 `totalInflatedLayers` + `reserveLayerBudget()` + 新错误码 `InflaterLayerBudgetExceeded=606` + `MAX_INFLATED_LAYER_COUNT=1e6`。
  - **P1-4（API）§15.3 Peek(filePath) 失败错误码钉死**。docstring 明示"open/read 失败 → 307；open 成功但 <9 字节 → 303；magic/version 无效 → 300/301"。实现与 `LoadFromFile` 对齐，单测可机械判定。
  - **P1-5（API）§15.3 PeekResult 字段 validity**。`canvasWidth/canvasHeight/formatVersion` 明示"仅 ok==true 时有定义"。避免失败路径读到 0 被误判为"版本 0"或"空画布"。
  - **P1-6（API）§15.1 SeverityOf/StageOf ABI 措辞修正**。原"非 ABI 承诺（实现可换）"引起歧义。改为"**函数实现可替换**（switch/查表），但**返回的 enum 值 + code→段映射是 ABI**"，调用方可放心 switch。
  - **P1-7（性能）§11.1/§11.2 两层去重 key**。原 imageIndexByNode/fontIndexByNode 只按 PAGX 指针；不同 node 指向同一 URI / 绝对路径 / `tgfx::Data*` 会重复入表（50 Layer 共享 5MB 图标 → 250MB 浪费）。本版本 BakeContext 加 Layer 2 `imageIndexByKey` / `fontIndexByKey`（content key variant<Data*, string>），先 byNode 后 byKey。
  - **P1-8（性能）§D.2 小 Path format 择优**。format=1 前缀 ≥5 byte，≤10 verb 的小 Path 反而胀 10-15%。本版本新增 `ChoosePathFormat`：verbCount < 3 或 ≤ 10 或坐标超 int24 → format=0，否则 format=1。
  - **P1-9（实现）§18.3bis CorruptBuilder API 补齐**。补 `WrapInnerLengthAt`（P0-S1 回归）/ `SetNumBitsAt`（P1-S3）/ `SetContinuationBitsAt`（P1-S4）/ `SetChildIndexAt` / `SetPatternImageIndexAt`（P1-S5）/ `SetCanvasSize`（P0-5）六个新方法；`LayerDepthLimitExceeded=406` 明示走 PAGXBuilder + `MakeDeepLayerStack(depth)` 辅助（结构性输入，不走 CorruptBuilder）。
  - **P1-10（实现）§18.4ter ValueCodec Safe wrapper 专项测试**。4 个 wrapper × 2 条（成功 + 越界拒绝）= 8 条用例，Phase 1 交付 `ValueCodecSafeTest.cpp`。
  - **P1-11（实现）§18.4quater PackLayerPath 专项测试**。BakeContextTest 加 3 条：BasicRoundTrip / DepthOverflow / ChildIndexCap。
  - **P1-12（实现）§I.4 CMake fuzz target**。加 `if(Clang) add_executable(PAGDecodeFuzz) target_compile_options(-fsanitize=fuzzer,address,undefined) endif()`——libFuzzer 仅 clang 可用，非 clang 构建时静默不生成 target，CI 在 clang 容器跑。Phase 12 出口条件可执行。
  - **P1-13（实现）§18.8 pag_v1_load_ms 采样路径**。明示走 libpag 主干 `CommandExport` 的 `ExportToPAGv1` + `pag::PAGFile::LoadFromBytes` 同进程同批采样。若 `ExportToPAGv1` 开工时不可用，退化门槛为"pag_load_ms ≤ 基线自身 × 1.05"并在 §17 D 档标记"v1 对比待补"。
  - **P2-1（架构）§3.2 四态三映射四 Context 全览表**。数据流图箭头旁标注携带 Context（`--Bake[BakeContext]→`），加表整理"4 状态 × 3 映射 × 4 Context × 错误码段"对应关系，接手 AI 5 分钟明架构。
  - **P2-2（实现）§19 Phase 9 拆 9.5 RenderUtil**。RenderUtil 是 Phase 12 RenderEquivalenceTest 才需要的测试基建，Phase 9 InflaterParityTest 只断言 layer 树结构不渲染像素。拆 9.5 专注 Inflater 对齐。
  - **P2-3（架构）§9.4 visitingComposition resize 契约**。明示"Inflate() 入口显式 `visitingComposition.assign(doc->compositions.size(), false)`——不在 Decoder 出口 resize"，允许多次 Inflate 复位状态。
  - **P2-4（API）§15.3 Peek/Load TOCTOU docstring**。明示"Peek(filePath) + LoadFromFile(filePath) 非原子；并发写入下观察字节可不同。需一致用 LoadFromBytes + 缓存，或接受重读差异重试。pagx 不内部缓存文件"。
  - **P2-5（API）§15.2/15.3 Result.ok 契约强化**。docstring 改为"Derived: ok ⟺ errors.empty(). Filled by implementation; callers MUST NOT write"——明示派生字段防 caller 漏同步。
  - **P2-6（API）§15 Options designated initializers**。补"推荐 `Options{.strict=true}` 风格防位置初始化冻结字段序。若项目 C++ 标准 < C++20，改用 `Options opts; opts.strict = true;`"。
  - **P2-7（实现）§G.3bis DiagnosticTest 全枚举覆盖**。原 8 条只碰 6 个具体 code；加第 9 条 `CodeToString.AllEnumValues` 遍历所有已定义 enum 值，保护"新增 code 忘加 switch case"。
  - **P2-8（实现）§18.8 baseline 首次 commit 归属**。明示"首次 baseline 由 tech lead 在项目 README 指定的参考机跑；日常 CI 只追 size_ratio / load_ratio，不追 abs_times"。换参考机需显式记录原因。

- v2.13 → v2.14 修订要点（5 位专家评审团第 5 轮综合：3 P0 安全新 CVE + 1 P0 架构 + 2 P0 性能 + 3 P0 实现 + 13 P1）：
  - **P0-S1（安全）§D.11 ShapeStyleData.innerLength u32 回绕防护**。`shapeStyleContentEnd = shapeStyleStart + innerLenBytes + innerLen` 同 §D.3 Tag length 同模式——攻击者构造 `innerLen=0xFFFFFFE0` 回绕到小值让 `setPosition` 指针回退重复派发 → 无限循环。本版本 §D.11 Read 伪码补"**P0 安全校验**"段——用 `uint64_t` 中间结果 + `shapeStyleContentEnd64 > tagEnd` 上界校验，fatal `MalformedTag=304`。
  - **P0-S2（安全）§D.1 `ReadUtf8String` / `ReadLengthPrefixedBytes` 统一入口 + 前置 length 校验**。原"warn + skip bytes"策略误导 AI 先 `resize(length)` 再 warn——恶意 `length=0x7FFFFFFF` 立即触发 2GB alloc，32-bit 进程 OOM 崩溃。本版本 §D.1 新增强制入口规范 + 调用清单：所有 varU32-prefixed utf8string / bytes 字段必须走 ValueCodec 的统一 wrapper，`length > maxBytes || length > bytesAvailable()` 任一条件触发即 warn + 返回空，绝不 resize。强制覆盖 ImageAssetTable.data / FontAssetTable.data/family/style / Composition.id / Layer.name / ElementText.text / GlyphRunBlob 内 utf8 字段。
  - **P0-A1（架构）§9.4 Composition 循环引用防护**。原 InflaterContext 无 `visitingComposition` / `currentCompositionDepth`——恶意 .pag 构造 `compositions[0].layers[0]=CompositionRef(0)` 自环或多 composition 互引成环，`inflateCompositionRef → inflateComposition → inflateCompositionRef` 无界递归 → 栈溢出。本版本：(a) InflaterContext 新增 `visitingComposition` 数组 + `currentCompositionDepth`；(b) §H.1 新增 `MAX_COMPOSITION_REF_DEPTH=32`；(c) 新增 `CompositionVisitScope` RAII guard；(d) 新增 warning 码 `InflateCompositionCycle=605`。进环或超深 → warn + 该 CompositionRef 降级为空 Layer，兄弟节点继续渲染。
  - **P0-P1（性能）§15.3 `Peek(filePath)` 重载**。原 Peek 只接 `(data, length)`，缩略图列表场景（64 张大文件）仍需整文件入内存，峰值 64×50MB=3GB，违反"O(1)"承诺。本版本新增 `static PeekResult Peek(const std::string&)`，内部 pread 仅前 ~64 字节，1000 张缩略图从 ~50GB I/O 降至 ~64KB I/O。
  - **P0-P2（性能）§11.1 ImageAsset.data 零拷贝**。原 `std::vector<uint8_t>` 在 PAGX 源、PAGDocument、tgfx decode cache 三处驻留，50MB PNG × 5 图峰值约 1.5GB。本版本 `ImageAsset::data` 改为 `std::shared_ptr<const tgfx::Data>`（tgfx 既有零拷贝封装），并新增 Inflater 生命周期纪律：`MakeFromEncoded` 成功后立即 `data.reset()`，`PAGLoaderTest.ImageBytesReleasedAfterInflate` 断言 `use_count==1`。低端 Android 2GB RAM 设备加载大文件不再 OOM。
  - **P0-I1（实现）§19 Phase 2 前置 PAGXBuilder**。原 Phase 2 `BakeContextTest` / `ResourceBakerTest` 需要 PAGX DOM 驱动，但 `PAGXBuilder` 在 Phase 3 才交付——时间悖论。本版本把 PAGXBuilder 提前到 Phase 2 批次，附带 PAGXBuilderTest。
  - **P0-I2（实现）§G.6 错误码矩阵按阶段拆**。原 Phase 4 出口条件"TruncatedDecodeTest 全绿"引用 404/405/402/403 错误码，但对应 Tag 要到 Phase 5/8 才实现。本版本阶段表把 §G.6 矩阵按 Tag 分批：Phase 4 只覆盖 300/301/302/303/305/306/307；Path 404/405 推至 Phase 5；Text/Resource 402/403 推至 Phase 8；Composition 605 推至 Phase 9。每阶段出口条件可机械判定。
  - **P0-I3（实现）§18.3bis CorruptBuilder 工具规范**。原计划在 v2.14 polish 批次处理，但 Phase 4 regression test 全依赖它——阻塞开工。本版本提前到 Phase 1（与 ValueCodec 同批）交付 `test/src/pag/support/CorruptBuilder.h+cpp`，提供 fluent API（FromValid/TruncateAt/FlipByteAt/ReplaceBytes/SetVersion/SetMagic/InflateVarU32At/WrapTagLengthAt/IntroduceCompositionSelfRef）。
  - **P1-S3/S4（安全）§D.1 readInt32List / readEncodedUint32 硬上界**。(a) readInt32List / readFloatList / readUBits 调用前前置断言 `numBits ≤ 32`，否则 OOB read / info leak；(b) `readEncodedUint32` 第 5 字节 / `readEncodedUint64` 第 10 字节若仍带 continuation bit → warn + 返回 0，防攻击者塞连续 continuation bit 字节让 decoder 线性扫描至 stream 末尾（二次放大 hang）。实现统一走 `src/pagx/pag/ValueCodec.h` 的 `ReadInt32ListSafe` / `ReadEncodedUint32Safe` wrapper，若 v1 已原生校验则退化为转发。
  - **P1-S5（安全）§D.9 / §D.11 单值 index 上限**。`LayerMaskRef.childIndex` 单值无上限（攻击者塞 UINT32_MAX-1 让 PackLayerPath 打包出异常位图）；`ImagePattern.patternImageIndex` 只规定 `UINT32_MAX` 哨兵未规定 `< doc.images.size()` 校验。本版本 §D.9 `childIndex` 校验 `< MAX_CHILDREN_PER_LAYER`；§D.11 ImagePattern 校验 `patternImageIndex < doc.images.size()`（在 ImageAssetTable 解码后），超限 warn + 重置为哨兵降级。
  - **P1-A1（架构）§4.3 propHeader extHeader 永久锁 1 字节纪律**。原 §4.4 规则 2 要求旧 Reader 在 `hasExt=1` 时 `readUint8()` 消费 1 byte——这**隐式**把 extHeader 尺寸永久冻结为 1 byte；若未来单方面扩成 2+ byte，旧 Reader 仍只读 1 byte，整个 Tag 错位读入。本版本明示红线："extHeader 尺寸**永久**固定 1 byte；扩展能力位的合法路径只有两条——启用新 encoding 值（§4.4 规则 1 整 Tag skip）或升 FORMAT_VERSION。严禁保留 hasExt 语义的同时扩 extHeader 尺寸"。
  - **P1-A2（架构）§8.5 strictMode 死字段清理**。`EncodeContext::strictMode` / `DecodeContext::strictMode` / `BakeContext::strictMode` 三处字段无任何消费点，仅在 docstring 说"PAGExporter 传入"但 Codec 主循环里没人读。本版本：(a) 从 3 个 Context 删除 `strictMode` 字段；(b) §15.2 `PAGExporter::Options::strict` 改文档说明"PAGExporter.cpp 在 Result 装配阶段通过 `SeverityOf` 做 warning→error 晋升，不在 Codec/Baker 内承载策略"；(c) §15.3 新增 `PAGLoader::Options { strict }` + LoadFromBytes/LoadFromFile 重载。Codec 保持纯字节流 I/O，策略抬到 SDK 层。
  - **P1-A3（架构）§8.1 Codec::Encode 签名注释**。明示 Encode 内部自建 EncodeContext，Encode 失败只产生 warnings，不产生 errors；无需调用侧传策略位。解消"§8.4 所有 Read/Write 必须接 Context*"与"Encode 签名无 ctx"的冲突。
  - **P1-API1（SDK）§15.1 VersionInfo + §15.3 PeekResult formatVersion 类型统一**。原 VersionInfo 是 `uint16_t`，PeekResult 是 `uint8_t`，类型漂移让 `-Wsign-compare` 触发，且未来 minor 升级会栽在 uint8 上。本版本统一为 `uint16_t`（低字节 major=0x02，高字节 minor）。
  - **P1-API2（SDK）§15.3 LoadFromBytes / LoadFromFile 生命周期承诺**。原 docstring 无 data 生命周期说明，调用方不知能否立即 free。本版本补 "data only needs to be valid for the duration of this call; Result retains no reference" + "file handle is closed before return"。mmap / 零拷贝用户可据此决策。
  - **P1-API3（SDK）§15.2 Options::strict 晋升范围**。原 docstring 只说"treat warnings as errors"，未明确哪些 warning 被晋升。本版本明示"PAGExporter 只晋升 Baker (200-299) + Codec (400-499) warning 码；Inflater 不在此路径所以无影响"；PAGLoader 类似（Codec 晋升，Inflater 不晋升）。
  - **P1-API4（SDK）§15.3 PeekResult.warnings 预留**。为未来 minor-version 降级提示预留 `std::vector<Diagnostic> warnings;` 字段，目前恒空。ABI 后加字段=SAME-BUILD 重编译，预留比后补便宜。
  - **P1-P3（性能）§9.4 layerByPath `std::unordered_map<PackedLayerPath, ...>`**。原 `std::map<vector<uint32_t>, ...>` 为防 hash-flood DoS 走红黑树——但 layerPath 是 Baker 拼装的结构坐标（非字节流字段），不由攻击者控制。本版本打包成 uint64（6 bit depth + 6 级 × 10 bit 每级），换成 `unordered_map`，10000 layers 省 ~10000 次 vector heap 分配 + 约 2-5 ms 查询耗时。
  - **P1-P4（性能）§8.2 EncodeStream::reserve 预估公式**。body stream 未 reserve，50MB 输出触发 log2(50MB)≈26 次 realloc + memcpy。本版本按 §18.8 `size_ratio≈0.3` 反推 `estimateBodySize = pagxTotalDataBytes × 0.4`，实现放 `src/pagx/pag/EstimateSize.h`。大文件 encode 时间省 ~30%。
  - **P1-P5（性能）§18.8 基线加 `pag_v1_load_ms` + CI 门槛**。原基线只记 v2 自身比率，未与 PAG v1 对比；立项承诺"优于或至少不劣于 PAG v1"无量化兑现。本版本 `reference_abs_times` 追加 `pag_v1_load_ms`（同样本转 v1 同批采样），CI 门槛 `pag_load_ms ≤ pag_v1_load_ms × 1.2`。
  - **P1-I4（实现）§18.3ter Layer 6 Fuzz**。原测试金字塔无 fuzz 层，§H.1 的 ~25 个 MAX_* 上限 + v2.10-v2.14 P0 安全项无 state-space 覆盖。本版本新增 `PAGDecodeFuzzTest` libFuzzer harness（`test/src/pag/fuzz/decode_fuzz.cpp`），Phase 12 合入 main 前 ASAN+UBSAN ≥1 CPU·小时全绿；CI 每周增量 24 CPU·小时。
  - **P1-I5（实现）§I.5 tgfx 枚举预检清单**。Phase 0 开工前 30 秒的 grep 脚本检 22 个 tgfx 类型导出，缺失走 TgfxEnumCompat.h shim 或 tgfx 上游修复。避免 Phase 2 PAGDocument.h 编译阻塞。
  - **P1-I6（实现）§18.3quater 本地 tgfx 源码调试通路**。渲染一致性测试怀疑 bug 在 tgfx 时，`-DTGFX_DIR=../tgfx -B cmake-build-debuglocal` 用 add_subdirectory 把本地 tgfx 源码加入构建，支持改码 + 断点。对齐 `.codebuddy/rules/Test.md` 指令。

- v2.12 → v2.13 修订要点（5 位专家评审团综合输出：2 P0 + 7 P1 + 2 实现 P0，polish 留 v2.14）：
  - **P0-A（安全）§D.2.2 Path coordCount/conicCount 上限校验**。PathV1 读 `coordCount`/`conicCount` 后无任何 MAX 校验，40 字节 `.pag` 可触发 `std::vector<int32_t>(0xFFFFFFFF)` 16GB heap alloc → OOM 崩溃；32-bit 平台 size_t 乘法溢出后 heap OOB write。本版本 §D.2.2 Read 伪码补"按 verb 序列计算 expectedCoords/expectedConics 精确匹配"的 P0 校验 + §H.1 新增 `MAX_PATH_COORDS = 6 * MAX_PATH_VERBS` / `MAX_PATH_CONIC_WEIGHTS = MAX_PATH_VERBS` 常量。不匹配或超上限推 `MalformedTag=304` fatal。
  - **P0-B（安全）§D.3 Tag length u32 加法回绕防护**。`stream->setPosition(tagStart + headerSize + tagLength)` 裸 u32 加法，攻击者构造 `tagLength=0xFFFFFFFE` 回绕到小值让 Decoder 无限循环。本版本 §D.3 末尾补"**P0 安全校验：Tag length u32 加法回绕防护**"强制段——用 `uint64_t` 中间结果 + `tagEnd > stream->size()` 上界校验。
  - **P1-A（安全）其他 count 上限**。`LayerMaskRef.pathDepth`、`ElementTextModifier.rangeSelectorCount`、`ElementText.runCount`、`Property<vector<T>>.count` 之前均无 MAX 校验，每个都是独立 heap OOM 面。本版本 §H.1 新增 `MAX_MASK_PATH_DEPTH=64` / `MAX_RANGE_SELECTORS_PER_MODIFIER=16` / `MAX_RUNS_PER_TEXT=256` / `MAX_VECTOR_VALUE_ELEMENTS=1024`。
  - **P1-B（安全）整数溢出安全写法**（§H.1 触发条件段）。明示 "totalAllocatedBytes + x > MAX" 的错误写法在 32-bit `size_t` 上会回绕绕过限额，强制改为 "x > MAX - total" 减法形式；并指向 P0-B 的 seek 计算同理用 uint64_t。
  - **P1-C（安全）Diagnostic message 字节上限**。`MAX_DIAGNOSTICS=1000` 限条数不限 size——恶意文件 1000 × 1MB message 仍可撑爆 64MB+ 内存。本版本 §H.1 新增 `MAX_DIAGNOSTIC_MESSAGE_BYTES=256`，§8.5 `DecodeContext::error/warn` 内部对传入 msg 做 resize 截断。
  - **P1-D（安全）§9.4 layerByPath 换 std::map**。原 `std::unordered_map<vector<uint32_t>, ..., VectorU32Hash>` 用 FNV-1a 硬编码种子——攻击者离线算碰撞后构造 1 万条同 hash 的 layerPath 让查询退化 O(n²)，单文件 CPU 耗尽数秒。本版本直接换 `std::map`（红黑树，O(log N × depth) 上界保证，hash collision 免疫），删除 `VectorU32Hash` 定义。
  - **P1-E（安全）§8.5 currentStream RAII guard**。原 `DecodeContext::currentStream` 弱引用赋值，嵌套 SubStream scope 退栈后形成悬空指针 → 后续 `warn()`/`currentOffset()` UAF。本版本在 Bridge 使用纪律前补强制 `CurrentStreamScope` RAII guard 定义 + 典型用法示例；主循环退出显式 `currentStream = nullptr`。
  - **API P0（SDK）§15.3 PAGLoader.h 顶部 SAME-BUILD ABI 警告**。原 SAME-BUILD 声明埋在 §I.4 最后一段——用户拿到 PAGLoader.h 完全看不到，会误把预编译 pagx-pag 与不同版本 tgfx 混链 → 运行时 UB。本版本在 PAGLoader.h 文件顶部注释块（"This header depends on tgfx"并列）显式补 SAME-BUILD 声明 + 指向 §I.4。
  - **API P1（SDK）新增 Peek / Version / SeverityOf / StageOf + 文档承诺强化**。§15.1 Diagnostic.h 新增 `SeverityOf(code)` / `StageOf(code)` 分类函数 + `Version()` + `VersionInfo` 结构，把 32 码降维到 2 轴；Diagnostic 结构 docstring 强化为"code 数值 ABI 稳定可持久化 / message 是不稳定英文调试文本不得持久化"；§15.3 PAGLoader 新增 `Peek(data, length)` 只读 FileHeader（缩略图/批量场景 10×-100× 提速）；PAGExporter/PAGLoader thread-safety docstring 补 3 行（并行吞吐不保证、Layer 单线程亲和、ToBytes 返回后 doc 可销毁）。
  - **实现 P0 §18.2 Day-1 smoke 时间悖论修复**。原 smoke 用 `RenderLayerToSurface`——但 §19 阶段 9 才交付 `support/RenderUtil.cpp`，Day-1 无法运行（阶段时间悖论）。本版本 smoke 改用内联 lambda `renderLocal`（~20 行 Surface::Make + DisplayList + root->addChild + render），加前置说明"不依赖阶段 9 RenderUtil"；context 获取指向 `pag::TestUtils::GetContext()`。
  - **实现 P0 §G.3bis Diagnostic.cpp 实现规范**。原 §19 阶段 0 说 `DiagnosticTest` 断言 "FormatDiagnostic 格式 + ABI-appended 码不丢"，但实现规范缺失——CodeToString 手写 switch vs X-macro、unknown code fallback、`FormatDiagnostic({Ok})` 语义都未明示。本版本新增 §G.3bis 给出完整 Diagnostic.cpp 样板（37 个枚举 → 字符串 switch + `Code(%u)` fallback + FormatDiagnostic 实现 + SeverityOf/StageOf 实现）+ DiagnosticTest 8 条断言清单。
  - **Polish 批次延后**：架构 F1/F2/F3 文档澄清、§4.4 "最外层 Tag" 歧义修词、§D.1 补 "Write 顺序非 memcpy"、ValueCodec 特化归位纪律、tgfx 并发争用文档、ImageAsset 生命周期峰值文档、libwebp/libjpeg CVE 继承声明、附录 A 行号 CI 脚本提案、§18.4 每文件 case 清单、基准图首次 check-in 流程、CorruptBuilder 工具规范、"全绿"量化门槛——合计 12 项 polish/rationale 非阻塞项，归并到 v2.14 开工第一周清理，本轮不做。

- v2.11 → v2.12 修订要点（freeze 前补完：5 个 polish，无架构变更）：
  - **F-1（P2）§8.5 补 `BakeContext` 紧凑定义**。BakeContext 被全文档引用 11 次（§7.2 / §8.3bis / §H.2）但结构体字段 + warn/error 方法从未展开定义——v2.11 日志吹"MAX_DIAGNOSTICS 覆盖 5 places"时 BakeContext 漏了。本版本把 §8.5 标题扩展为 "EncodeContext / DecodeContext / BakeContext"，在 DecodeContext bridge 小节后补 BakeContext 定义（errors / warnings / currentLayerDepth / currentVectorElementDepth / strictMode / imageIndexByNode / fontIndexByNode / layerPathByPagxLayer + error/warn 加 MAX_DIAGNOSTICS cap + enterLayer/enterVectorGroup 辅助），末尾补"Context 四并列总览表"便于 AI 对比落代码。
  - **F-2（P3）§9.4 补 `VectorU32Hash` 定义**。`InflaterContext::layerByPath` 使用 `std::unordered_map<std::vector<uint32_t>, ..., VectorU32Hash>`，但 `VectorU32Hash` 在文档任何地方都没定义。本版本在 §9.4 InflaterContext struct 之前补 FNV-1a 变体哈希实现（~10 行）。
  - **F-3（P3）§I.4 补 tgfx SAME-BUILD ABI 声明**。`DiagnosticCode` append-only ABI 纪律（§15.1）只覆盖 pagx 自身，但 `PAGLoader::Result::layer` 暴露 `tgfx::Layer` 的 vtable——跨 tgfx 版本混链会 UB。本版本在 §I.4 "tgfx 暴露等级"引用块后追加 "tgfx ABI 模型" 引用块，明示"同一次编译中一同构建并链接，不承诺与其他 tgfx 预编译版本二进制兼容；升级 tgfx 时必须重新构建 pagx-pag"。
  - **F-4（P3）§5.1 顶层组成顺序对齐字节流序**。v2.10 加 §C.5-pre 定义 `PAGDocument { header, images, fonts, compositions }` 后，§5.1 叙述顺序仍是 v2.9 前的"header → compositions → images/fonts"，与字节流写入顺序（§8.2 FileHeader → ImageAssetTable → FontAssetTable → CompositionList）不同步，读者容易对 Tag 顺序产生错觉。本版本 §5.1 调整为 "header → images/fonts → compositions" 并补 "按字节流写入顺序" + 引 "§C.5-pre 权威定义"。
  - **F-5（P3）§8.5 `DecodeContext::error` 注释与代码一致化**。v2.11 代码是 `if (size >= cap) return;`（完全静默），注释却说 "只保留最后一条 meta 告警说明已达顶"——注释自相矛盾。本版本改注释为 "达顶后后续调用完全静默（不推 meta，保持幂等）"，与代码统一。
  - **全文重复排查**：扫 2-gram / 长行重复，未发现新的段落级粘贴回归（v2.11 已清理的 3 处为 v2.11 独有引入）。

- v2.10 → v2.11 修订要点（冻结前清理：3 个粘贴回归 + 1 个 DoS 加固）：
  - **F-1（P2）删除 §G.5 重复段**。v2.10 P2-2 新增 `Property<EnumT>` 通路纪律时粘贴两次（L4208-4227 与 L4229-4248 逐字相同），本版本删除后者。
  - **F-2（P3）删除 §D.10 CompositionRefPayload 重复校验说明**。v2.10 P3-3 同样粘贴两次，本版本删除后者。
  - **F-3（P3）删除 §G.6 `StructureLimitExceeded — Decoder 侧 count 攻击` 重复行**。v2.10 P2-1 加测试条目时粘贴两次。
  - **F-4（P3 加固）Diagnostic 数量封顶**（§H.1 + §8.5 + §G.3）。v2.10 前无 `MAX_DIAGNOSTICS` 上限，恶意文件构造 100K 个损坏 element 可让 `DecodeContext::warnings` 增长到数百 MB（每 `Diagnostic` 含 `std::string`）。本版本：§H.1 新增 `constexpr uint32_t MAX_DIAGNOSTICS = 1000`；§8.5 `DecodeContext::error/warn`、`EncodeContext::warn`、`InflaterContext::warn`、`syncFromStreamContext` 循环全部加达顶静默丢弃逻辑；§G.3 DecodeContext.cpp 示例同步。达顶后新告警静默丢弃（保持幂等），不影响 `hasError()` 判定或正确路径。

- v2.9 → v2.10 修订要点（冻结前最后一轮：1 P1 + 2 P2 + 3 P3）：
  - **P1-1：§C.5-pre 补 `struct FileHeader` + `struct PAGDocument` 顶层根结构**。v2.9 及以前，§22 附录 C 自称"PAGDocument 完整 C++ 定义"但实际只定义了 Property / enum / ImageAsset / Composition / Layer / VectorElement 等子结构，**顶层 `PAGDocument` 与 `FileHeader` 本身从未定义**——§8.2 / §8.3bis / §15.3 到处使用 `doc.header.width` / `doc.compositions` 却找不到权威字段表。本版本在 §C.4 末尾与 §C.5 之间新增 §C.5-pre 子节，钉死字段列表 + 默认值 + 成员访问风格（`doc->header.width`，四字段容器均为 `std::vector<std::unique_ptr<T>>`）。v2.0 → v2.9 十轮评审共同漏点。
  - **P2-1：补三个硬上限的 Decoder 强制校验**（§D.6 / §D.8 / §H.2 / §G.6）。`MAX_IMAGES` / `MAX_FONTS` / `MAX_CHILDREN_PER_LAYER` 三个常量在 §H.1 已定义但 Decoder Read 路径无 count 上界检查——攻击者构造 `childCount=0xFFFFFFFF` 的 .pag 可触发 `vector::reserve` OOM。本版本：§D.6 开头加"Decoder 强制上界"小节说明三个 count 校验时机；§D.8 LayerBlock.childCount 字段行追加内联校验注释；§H.2 Baker 侧列表加入三个常量并注明 "Decoder 侧同样校验"；§G.6 测试矩阵追加 `Truncate.ChildCountOverflow / ImageTableOverflow / FontTableOverflow` 覆盖。
  - **P2-2：§G.5 明文钉死 `Property<EnumT>` 通路纪律**。v2.9 里 §C.5 Layer 字段用 `Property<BlendMode>` + §D.8 "字节层 u8" + §G.5 BlendMode 有 EnumMeta，但**三者之间如何串联**未明文——AI 可能写出绕过 `ReadEnum<BlendMode>` 的实现（直接 `static_cast<BlendMode>(readUint8())`），让非法 u8 逃过 `InvalidEnumValue=407` 告警。本版本在 §G.5 EnumMeta 清单末尾新增纪律条款 + 正反例代码：`Property<EnumT>` 的 `ReadValue<T>` / `WriteValue<T>` 特化必须以 `ReadEnum<T>` / `WriteEnum<T>` 为实现体；`PropertyValueEquals<EnumT>` 走默认 `operator==`。
  - **P3-1：§G.6 删 `GlyphRunKindInferred=208` 测试矩阵重复行**。v2.9 P1-A 修复时误粘贴两次。
  - **P3-2：§G.5 `TileMode` 的 `EnumMeta::Default` Clamp → Decal**。与 §C.6 ImagePattern / §C.9 LayerFilter/LayerStyle 结构体字段默认值 `Decal` 对齐（2026-04-25 主干切换 Decal 时只改了结构体，EnumMeta 遗漏至今）。避免损坏字节流被回退为 Clamp 而合法字段保持 Decal 的混合状态。
  - **P3-3：§D.10 CompositionRefPayload 加 Decoder 校验点说明**。`InvalidCompositionIndex=306` 的触发时机（读 `u32 compositionIndex` 后立即 `!= UINT32_MAX && < doc.compositions.size()`）原 §G.2 注释里有但 §D.10 Tag 布局处缺——AI 可能把校验推到 Inflater 阶段导致 Codec 路径放过病态输入。

- v2.8 → v2.9 修订要点（独立审计补强：2 个 P1 + 7 个 P2 + 6 个 P3）：
  - **P1-A：§G.6 测试矩阵补 `GlyphRunKindInferred=208`**。v2.8 里 208 在 §G.2 声明（TextBaker 兜底分支使用），但 §G.6 测试矩阵找不到对应行——违反"每个 ErrorCode 必须至少一条单测触发"纪律。本版本补一行 `TextBakerTest.cpp / TextBaker.GlyphRunKindInferred`。
  - **P1-B：删除 `LayerFilter::blendMode` 死字段**（§C.9 + §5.6）。v2.8 里 `LayerFilter` 结构体声明 `BlendMode blendMode`，但 §D.12 所有 5 个 Filter Tag 的 body 都不写该字段——数据模型与字节流语义漂移，Baker 落该字段会被静默丢失。本版本删除 `LayerFilter::blendMode`（Blend filter 的混合入口由 `blendFilterMode` 承载，已在字节布局落地），§5.6 描述同步改为"`blendMode` 为 style 专用，Filter 若需混合用各自子类字段（如 `blendFilterMode`）"。注：`LayerStyle::blendMode` 保持不变——StyleDropShadow/InnerShadow/BackgroundBlur 的 Tag body 已显式序列化该字段。
  - **P2-A：§G.5 填 `BlendMode` / `MergePathOp` MaxValid 具体值**。v2.8 两条写作"参考 tgfx 枚举最大值"，AI 落代码无法直接写枚举特化。本版本依 §E.1 / §E.3 列出的值改为 `BlendMode(MaxValid=17, Default=SrcOver)` / `MergePathOp(MaxValid=4, Default=Append)`；顺便把 `SelectorUnit/Shape/Mode` 三条也补成具体数字。
  - **P2-B：§16 unit/ 补 `BakerEdgeCasesTest.cpp`**。§G.6 引用该文件 8 次（100-105 + 207），但 §16 目录树没列它。本版本目录树追加该文件，§19 阶段 3 "同批测试" 也追加。
  - **P2-C：§16 补 `codec/` 子目录**。§18.4bis 的 8 个压缩专项测试文件说"放在 `test/src/pag/codec/`"，但 §16 目录树无 `codec/` 节点。本版本追加 `codec/` 子目录 + 8 个文件。
  - **P2-D：§18.5 `InflaterParityTest` 条数 3 → 5**。v2.6 新增 `InflaterEmptyDocument=604` 后，§18.5 表格下限仍写 3 条，与 §G.6 实际 5 条（600-604）不符。
  - **P2-E：§16 `ErrorCode.h` 注释同步 Diagnostic alias**。§G.3 定义该文件同时 `using ErrorCode` 与 `using Diagnostic`，但 §16 注释只提 ErrorCode，AI 落代码会漏 Diagnostic alias。本版本注释改为 `using ErrorCode/Diagnostic = pagx::DiagnosticCode/Diagnostic;`。
  - **P2-F：§D.10 显式钉死 Payload 段 TagCode（20-26）**。v2.8 里 Shape/Text/Image/Solid 四个 "本期不产出" 的 Payload 没写具体 TagCode 枚举值，未来实现时可能被挪走破坏 ABI。本版本在 §D.10 开头加分配表：`ShapePayload=20 / TextPayload=21 / ImagePayload=22 / SolidPayload=23 / VectorPayload=24 / MeshPayload=25 / CompositionRefPayload=26`，明示"即便 Write 路径 no-op，枚举值必须显式声明"。
  - **P2-G：§6.1 顶层段 "4 个" → "5 个"，Payload 段 "6 (20-26 中 6 个)" → "7 (20-26)"**。顶层段实际 5 个 Tag（FileHeader/ImageAssetTable/FontAssetTable/CompositionList/Composition），Payload 段 20-26 共 7 个（因 P2-F 已钉 TagCode），"未来 payload" 条目删除。
  - **P3-A：版本号 2.7 → 2.9**（v2.8 只改修订日志未升版本号，本轮一并修到最新）。
  - **P3-B：§D.8 `Property<u8> blendMode` → `Property<BlendMode>`**。字节层仍是 u8，但类型记号与 §C.5 结构体字段一致（`Property<BlendMode>`），AI 落 `ReadProperty<BlendMode>` 可直接走 EnumMeta 值域校验，避免"Property<u8> vs Property<BlendMode>" 类型不对称。
  - **P3-C：§13.1 #1 "1 字节 encoding 位 + value" 表述修正**。`propHeader` 是位域（含 encoding/isDefault/hasExt），"1 字节 encoding 位"语义不准确，改为 "字节流前缀 1 byte `propHeader` 位域（含 encoding / isDefault / hasExt，见 §4.3）+ 可选 value"。
  - **P3-D：§D.11 ClassicGlyphRun `anchorXY` 精度补注**。其他量化字段（positionXY/scaleXY/rotations/skews）都有 precision 注释，只 anchorXY 空着。本版本追加 `precision = 2^(-glyphPrecisionLog2)（同 positionXY）`。
  - **P3-E：§18.4 "约 100 条，分 10 文件" → "约 110 条，分 12 文件"**。v2.4 加 `DiagnosticTest.cpp`（+1 文件）、v2.9 加 `BakerEdgeCasesTest.cpp`（+1 文件），现在 unit/ 共 12 个文件；单测条数同步估算 +10。
  - **P3-F：§9.1 Result `layer` 字段注释追加"所有权语义详见 §15.3"交叉引用**。避免 AI 只读 §9 Inflater 章节就按"普通 shared_ptr"理解，漏掉 §15.3 "独占所有权" 的强约束。

- v2.7 → v2.8 修订要点（评估漏洞清理：5 个小修，无架构变更）：
  - **P2-1：§9.4 表头计数修正**。表头 "4 个 Inflater 告警码的触发点" 与下方 5 行表格（600-604）不一致——v2.6 新增 `InflaterEmptyDocument=604` 后未同步更新表头。本版本改为 "5 个"。
  - **P2-2：§9.2 "两趟 vs Pass 1/2/3" 措辞统一**。文字声称 "严格同构的两趟" 但伪码列 Pass 1/2/3（其中 Pass 3 是退栈前的 warnings move，非数据遍历）。本版本改为 "两趟遍历（外加一次退栈前的 warnings 转移）"，伪码里 `Pass 3（退栈前）` 改为 `Finalize（退栈前，非数据遍历）`；§9.4 `Pass 3 收尾规则` 同步改为 `Finalize 收尾规则`。
  - **P3-1：`strict` vs `strictMode` 对称性澄清**。§15.2 `Options::strict`（外部）与 §8.5 `EncodeContext::strictMode` / `DecodeContext::strictMode`（内部）名字不对称。本版本在 §15.2 `strict` 字段的 docstring 上加一句说明："Mapped to internal EncodeContext::strictMode / DecodeContext::strictMode one-to-one by PAGExporter.cpp"——语义等价，命名由对外简洁 vs 对内明确两种风格共存决定。
  - **P3-2：§G.5 EnumMeta 清单补 `ScaleMode`**。`ScaleMode`（C.1 L2149 定义，D.11 L3599 以 u8 写入）未列入 EnumMeta 特化清单，读取时会绕过 `ReadEnum<T>` 的值域校验，值域外数值强转为非法枚举值（UB）。本版本补 `ScaleMode（MaxValid=3, Default=LetterBox）`。
  - **P3-3：§8.1 Codec 契约补 `EncodeResult↔外部 ok` 桥接说明**。内部 `EncodeResult { bytes, warnings }` 无 ok 字段，成功判据隐含为 `bytes != nullptr`；外部 `PAGExporter::Result.ok` 的契约是 `errors.empty()`——两者如何对接先前未明示。本版本在 §8.1 契约列表补一行：外部 Result 的 `errors` 仅来源于 Baker fatal，由 §15.2 `ToBytes/ToFile` 内部拼装 Baker→Encoder 链时填充；Encode 阶段本身只产生 warnings，不产生 errors。

- v2.6 → v2.7 修订要点（对外契约语义修正 + 错误码语义去污染）：
  - **P0：PAGLoader `ok` 契约修复**（§15.3 + §15.2 + §G.4 + §17 C1）。v2.6 的实现骨架 `out.ok = (out.layer != nullptr)` 在"空文档合法场景"（Inflater 返回 `layer=nullptr + warning 604`）下产生 `ok=false + errors=[]` 的自相矛盾状态——违反 docstring "`!ok ⟹ errors non-empty`"。本版本统一两个 Result 的 ok 契约为 **`ok == true ⟺ errors.empty()`**，PAGLoader 实现骨架改为 `out.ok = out.errors.empty()`；docstring 明示"空文档合法场景下 ok=true 但 layer=nullptr，由 warning 604 标明原因"。PAGExporter::Result docstring 同步对称化。§G.4 第 3 项改写为契约单一叙述。
  - **P4：§17 C1 验收项同步**。原文字"产出空 tgfx::Layer 树"与 v2.6 Inflater 新契约冲突（空场景返回 nullptr 而非"空树"），本版本改为"PAGLoader 返回 ok=true + layer=nullptr + warnings=[InflaterEmptyDocument(604)]"，与 §15.3 ok 契约对齐。
  - **P1：错误码语义去污染**（§H.2 + §15.1 / §G.2 的 105 注释）。v2.6 的 §H.2 让"字符串超限截断"复用 `StringInvalidUtf8=403`（原语义是"UTF-8 字节非法"），属于错误码语义污染。本版本把 `MAX_NAME_STRING_BYTES` / `MAX_TEXT_STRING_BYTES` 超限并入 `StructureLimitExceeded=105`（结构性硬上限统一码），行为从"warning 截断继续"升级为"fatal 放弃产出"——与其它 MAX_* 行为一致。§H.2 新增一段 rationale 明示两码独立："105 = 长度超限（结构性病态）；403 = UTF-8 字节损坏（编码非法）"。105 枚举注释扩充对应场景。
  - **P2：§9.1 Result 注释补 604**。Inflater `Result::warnings` 注释原只列 600-603 四个码，遗漏 v2.6 新增的 `InflaterEmptyDocument=604`。本版本改为引用 §9.4 触发点表（权威），避免列表维护负担。
  - **P3：§16 InflaterContext.h 注释同步**。原注释"pendingMasks / layerByPath 等"不完整（v2.6 §9.4 已定义完整结构含 warnings + warn 方法），本版本改为"warnings + pendingMasks + layerByPath（权威定义见 §9.4）"。

- v2.5 → v2.6 修订要点（评估补强：2 个新码 + InflaterContext 定义 + 契约澄清）：
  - **C-1：新增错误码 `StructureLimitExceeded = 105`**（Baker fatal 段）。v2.5 附录 H.2 引用了**不存在的** `ErrorCode::InvalidDocument`（该码在 v1.2 已拆为 102/103，不应再出现）。本版本为 MAX_COMPOSITIONS / MAX_LAYERS_PER_COMPOSITION / MAX_VECTOR_ELEMENTS_PER_PAYLOAD / MAX_FILTERS_PER_LAYER / MAX_STYLES_PER_LAYER / MAX_GRADIENT_STOPS / MAX_PATH_VERBS / MAX_GLYPHS_PER_RUN 八类结构性超限统一分配 `StructureLimitExceeded = 105`，message 附具体字段名。§15.1 / §G.2 / §H.2 / §G.6 同步更新（§G.6 新增 `BakerEdgeCases.StructureLimitExceeded` 测试）。
  - **E-1：新增错误码 `InflaterEmptyDocument = 604`**（Inflater warning 段）。v2.5 §9.1 docstring 提到 "layer == nullptr only when compositions[0] missing" 但**没有对应的告警码**——调用方无法区分"输入空"vs"内部失败"，只能字符串匹配 message。本版本为"compositions 为空 / compositions[0]->layers 为空"场景分配专用码，明示 nullptr 原因。不复用 Baker 段 `EmptyDocument=207`（跨段语义污染违反 §G.1 分段约束）。§15.1 / §G.2 / §9.1 docstring / §G.6（新增 `InflaterParity.EmptyDocument` 测试）同步。
  - **A-1：新增 §9.4 InflaterContext 紧凑定义**。v2.5 §9.2 引用 `InflaterContext::warnings` 但结构未定义，与 §8.5 EncodeContext/DecodeContext 风格不对称。本版本补小节 §9.4（结构体 + warn 方法 + pendingMasks/layerByPath 字段），并列出 Inflater 5 个告警码（600-604）的精确触发位置表——锁死 §G.2 Inflater 段的"码 ↔ 代码位置"映射，消除阶段 9 LayerInflater 实现期的决策抖动。
  - **A-2 / E-2：PAGLoader `data` 参数契约澄清**。v2.5 §15.3 docstring "Must not be null" 与实现骨架"null 时返回 TruncatedData error"自相矛盾。本版本 docstring 改为"null or length < 9 returns TruncatedData"（与实现一致，不新增码——data 为 null 与 length 不足本质都是"输入不构成合法 PAG v2 头部"，共享 TruncatedData 语义合理且最简）。实现骨架注释同步，错误 message 区分 "null" vs "< 9 bytes"。
  - **LayerInflater 命名复核**。结合 Android `LayoutInflater` 先例（XML → View 树）确认：`LayerInflater::Inflate(doc) → tgfx::Layer 树` 命名准确，与 Baker 对称（Baker = 编辑态→渲染态数据，Inflater = 渲染态数据→运行时对象），**保留不改**。

- v2.4 → v2.5 修订要点（v2.4 文档一致性清理 + Inflater Result 改造）：
  - **P0-1：附录 C 位置修复**。v2.4 里 §22 附录 C（PAGDocument 完整 C++ 定义）物理位置错乱——位于 §28 附录 I 之后、文档末尾；本版本把该章节整体前移到 §21（附录 B）和 §23（附录 D）之间，恢复 TOC 顺序。同时删除 §21 之后孤立的 `### C.9 LayerFilter / LayerStyle` 重复块（v1.2 代码片段迁移遗留）。
  - **P0-2：§8.1 ErrorCode 定义同步为 alias**。v2.4 里 §8.1 仍保留 `enum class ErrorCode : uint16_t { ... };` 的独立定义，与 §G.2 权威声明（"通过 `using ErrorCode = pagx::DiagnosticCode;` alias 访问"）冲突。本版本把 §8.1 改为 `#include "pagx/pag/ErrorCode.h"`，删除独立枚举块，明示"Diagnostic / ErrorCode 权威定义在 §15.1"，避免 AI 落代码时写出第二套枚举。
  - **P1-1：Diagnostic.h 同名冲突消除**。§G.3 里的"便捷构造辅助"内部头原定名 `src/pagx/pag/Diagnostic.h`，与对外 `include/pagx/Diagnostic.h` 文件名冲突（内部 `#include "pagx/Diagnostic.h"` 会在 include path 上产生工具链相关的歧义）。本版本重命名内部辅助头为 `src/pagx/pag/DiagBuild.h`（表达"构造 Diagnostic 的辅助"），§16 目录结构同步登记。
  - **P1-2：§19 TDD 计划补齐 v2.4 新增文件**。阶段表新增 **阶段 0**（`Diagnostic.h` + `Diagnostic.cpp` + `ErrorCode.h` alias + `DiagBuild.h` + `DiagnosticTest`）作为所有后续阶段的 include 基础；新增 **阶段 10.5**（`PAGLoader.h` + `PAGLoader.cpp` + `PAGLoaderTest`），验证 LoadFromBytes / LoadFromFile 成功路径 + `FileReadFailed=307` 错误路径 + Inflater warnings 收集。§16 test 目录结构补 `DiagnosticTest.cpp`（unit/）+ `PAGLoaderTest.cpp`（e2e/），§G.6 测试矩阵里 `LoaderTest.cpp` → `PAGLoaderTest.cpp`（与类名一致）。
  - **P1-3：§16 CMake 描述同步**。v2.4 的 §16 "CMake 集成"段落仅提 `pagx-pag = src/pagx/pag/ 全部 + PAGExporter.h`，未提 Diagnostic.cpp / PAGLoader.cpp / 三公共头 / tgfx PUBLIC 依赖。本版本重写为完整清单（三公共头 + src/pagx/Diagnostic.cpp + src/pagx/pag/ 内部全部 + tgfx PUBLIC 依赖），与 §I.4 详细 CMake 配置保持一致。
  - **E-1：LayerInflater::Inflate 改为返回 Result**（§9.1 / §9.2 / §15.3 / §18.7 / §3.1 数据流图 / §17 A5）。原签名 `static std::shared_ptr<tgfx::Layer> Inflate(const PAGDocument&)` 丢失 Inflater 侧告警（`InflateImageDecodeFailed=600` 等 600-699 段），导致 PAGLoader::Result::warnings 只承载 Codec 阶段告警、漏掉 Inflater 阶段告警。新签名 `static Result Inflate(...)` 返回 `{layer, warnings}`，PAGLoader 实现骨架更新为合并两阶段告警；§18.7 三处测试代码同步改用 `inflated.layer`；§3.1 数据流图的最后一态改为 `LayerInflater::Result { layer, warnings }`；§17 档 A A5 验收项增加 "healthy 样本 warnings 为空" 的判据。
  - **J-1：PAGLoader::Result::layer 所有权语义澄清**（§15.3 新增"Layer 所有权语义"子节）。明确说明：虽然类型是 `std::shared_ptr<tgfx::Layer>`（受 tgfx 工厂签名被动约束），但**语义上为独占所有权**——严禁 clone 到第二个 DisplayList、严禁多持有者共享。已交叉验证 `tgfx::Layer::Make()` 的实际签名是 `static std::shared_ptr<Layer> Make();`（`/Users/henryjpxie/workspace/tgfx/include/tgfx/layers/Layer.h:80`），证实 shared_ptr 是无法规避的被动妥协，不是设计鼓励。

- v2.3 → v2.4 修订要点（对外 API 补全 + 命名重构）：
  - **P1：LayerDoc → PAGDocument 重命名**。`pagx::pag::LayerDoc` 顶层数据模型改名为 `pagx::pag::PAGDocument`，与 `pagx::PAGXDocument`（编辑态）形成对偶命名。改动范围：§3.1 数据流图、§5 数据模型章、§C 附录全部、§8 Codec、§15 API、§16 目录结构（`LayerDoc.h` → `PAGDocument.h`）、§18 测试（`LayerDocParityTest` → `PAGDocumentParityTest`，`LayerDocEquals` → `PAGDocumentEquals`，`BuildMinimalVectorLayerDoc` → `BuildMinimalVectorPAGDocument`）。**保留**：`LayerBaker` / `LayerInflater` / `InflaterContext` 等符号——它们的 `Layer` 前缀指向各自作用域（`struct Layer` 字段 / 输出 `tgfx::Layer` 树），不指 LayerDoc，语义上不应改。
  - **P1：新增 `include/pagx/PAGLoader.h`**（§15.3 新增）。对外暴露 `PAGLoader::LoadFromBytes / LoadFromFile` 两个静态方法，返回含 `std::shared_ptr<tgfx::Layer>` + 画布尺寸 + 结构化 diagnostic 的 Result。这是 v2 **唯一**允许依赖 tgfx 的对外头，头部有显式警告注释；纯导出用户继续用 tgfx-free 的 `PAGExporter.h`。§16 目录结构新增 `src/pagx/pag/PAGLoader.cpp`，§I.1 拆分为 "tgfx-free 公共头 (I.1.a)" 和 "tgfx-dependent 公共头 (I.1.b)" 两段，§I.4 CMake 升级 `pagx-pag` 的 `tgfx` 依赖到 `PUBLIC`。附录 G.2 新增错误码 `FileReadFailed = 307`（LoadFromFile 专用），§G.6 测试矩阵补 `PAGLoader.LoadFromFile_Missing` 一条。
  - **P1：Result 暴露结构化 Diagnostic**（§15.1 / §15.2 / §15.3 / §G.2 / §G.3 / §G.4）。对外新增 `include/pagx/Diagnostic.h`，定义 `pagx::DiagnosticCode` 枚举 + `pagx::Diagnostic` 结构 + `pagx::FormatDiagnostic` 函数。采用**方案 A：共享枚举**——内部 `pagx::pag::ErrorCode` 变为 `using ErrorCode = pagx::DiagnosticCode;` alias（`src/pagx/pag/ErrorCode.h`），内部所有 `ErrorCode::LayoutNotApplied` 等调用点零改动。`PAGExporter::Result.errors / .warnings` 从 `vector<string>` 改为 `vector<Diagnostic>`，调用方按 code 分支处理或调 `FormatDiagnostic` 获取字符串（§14.2 CLI 惯用写法示例）。**ABI 纪律**：`DiagnosticCode` 枚举值只能段内追加，不得复用编号、不得跨段，新增即对外 ABI 变更。

- v2.2 → v2.3 修订要点（主干同步后数据模型对齐）：
  - **P0：Gradient 基类 + ScaleMode 枚举**（`#e6251827` + `#14828930`）。4 个 gradient 在 PAGX 侧统一继承新基类 `pagx::Gradient`（`include/pagx/nodes/Gradient.h`），共享 `matrix / colorStops / fitsToGeometry=true`。ShapeStyleData 新增 `bool fitsToGeometry` 字段，4 个 gradient 分支新增序列化该字段。ImagePattern 新增 `ScaleMode scaleMode = LetterBox`，默认 tileMode 从 Clamp 改 Decal。附录 C.1 新增 `ScaleMode` 枚举定义，附录 C.6 更新 ShapeStyleData，§5.5 补 Gradient 基类 / fitsToGeometry 语义 / ImagePattern ScaleMode 说明，附录 E.9 新增 ScaleMode 枚举映射。§D.11 字节布局同步更新。
  - **P0：LayoutNode 百分比尺寸**（`#7236b3f3`）。PAGX `Layer/Group` 的 `width/height` 迁移到基类 `LayoutNode`，新增 `percentWidth/percentHeight`。由于百分比尺寸在 `applyLayout()` 阶段已解算为绝对值，Baker 只读取解算后的值——PAGDocument 字节流**不承载** `percentWidth/percentHeight`，设计文档无需新增 PAGDocument 字段，§5.3 补 Sizing 语义说明。
  - **P0：附录 A / F 行号全量重扫**（`#7236b3f3` + `#e6251827` 使 LayerBuilder.cpp 从 924 行变为 929 行）。附录 A 的 20+ 个函数行号全部同步当前源码；附录 F.1/F.3 行号引用同步；§7/§8/§11/§12 散落行号引用同步。
  - **P1：附录 F.3 字段表**。gradient 5 条新增 `fitsToGeometry`；ImagePattern 新增 `scaleMode`。LinearGradient `startPoint/endPoint` 默认值更新为 `{0,0}/{1,0}`（对齐 tgfx relative-fill）。
  - **P1：xsd 引用检查**（`#3db527cd`）。确认文档无 xsd / DimensionType 引用，无需改动。

- v2.1 → v2.2 修订要点（深度评审补强）：
  - **P0-1 Property<T> 相等性语义**（附录 C.2 补）。明确 `isDefault` 位判断规则：所有 T（tgfx::Path / Matrix / Matrix3D / Color / Point / Rect / std::vector / std::array）均依赖 `operator==`（经 `tgfx/core/*.h:45/129/218/300/874` 验证齐全）；浮点走精确 `==` 非容差（理由详见 §C.2）；引入 `PropertyValueEquals<T>` 单一入口方便未来类型扩展。消除 AI 在 Baker 实现 `WriteProperty<tgfx::Path>` 时卡壳的风险。
  - **P1-1 PAGDocument 错误路径内存管理**（§8.3bis 新增）。统一用 `std::unique_ptr<PAGDocument>` 管理根对象，fatal 立即 `reset()`，依赖析构链式释放子对象；禁止栈对象/裸 new/shared_ptr 三种反面模式；补充 `BakeResult` 结构定义与 `DecodeResult` 对称；§18.4bis 追加 `BakerMemoryTest.FatalDuringLayerBakeNoLeak` ASAN 测试。
  - **P1-2 Diagnostic byteOffset 字段**（§8.1 + §8.5 + 附录 G.3 补）。Diagnostic 结构新增 `uint32_t byteOffset`，Decoder 路径由 `DecodeContext::currentStream` 自动填充 `stream->position()`，Baker/Encoder 填 0；`FormatDiagnostic` 输出 `" @0x<hex>"` 后缀；大幅降低线上 bug 定位成本。

- v2.0 → v2.1 修订要点（测试体系补强 + 小完善）：
  - **压缩机制专项测试**（§18.4bis 新增）。针对 A1/A3/B1/B2/C1/C2/D2 共 7 个压缩/扩展机制，新增 25 条单元测试，分散到 8 个 cpp 文件（`ColorCodecTest` / `PropertyCodecTest` / `MatrixCodecTest` / `PathCodecTest` / `ShapeStyleCodecTest` / `GlyphRunBlobCodecTest` / `LayerBitFlagsCodecTest` / `VersionedTagTest`）。锁死压缩机制的往返正确性和兼容行为，作为基准图测试（§18.7）的前置防线。
  - **PathA vs PathB 交叉渲染测试**（§18.7bis 新增）。响应"pagx 转 pag 后直接对比渲染图"的思路，但作为**辅助诊断测试**而非主门槛——因 v2.0 量化损失 + GPU 非确定性会造成可预期的亚像素差异。采用 PSNR ≥ 30 dB 宽松阈值，非阻塞，配合 §18.7 基准图测试形成"主门槛 + 辅助预警 + 诊断矩阵"双层体系。
  - **ShapeStyleData 双重 length ASCII 图示**（§D.11 新增，P2-2 修复）。明确区分外层 TagHeader.length 与内层 innerLength 的作用域；补 `ReadElementFillStyle` 伪码防止 AI 落代码时把两层 length 搞混。
  - **ChoosePathFormat 阈值常量**（附录 H.1 新增，P2-3 修复）。`PATH_QUANTIZATION_MIN_VERBS = 3` / `PATH_QUANTIZATION_MAX_COORD = 1<<23` / `PATH_QUANTIZATION_DEFAULT_LOG2 = 4` 三个常量入表；§D.2.2 补 `ChoosePathFormat` 实现伪码。
  - **Baseline::Compare 使用边界**（§18.1 + §18.4bis 补强）。明确 `pag::Baseline::Compare` **只在 Layer 4 (Render) 使用**（其内部是 MD5 精确匹配 + version.json 托管，见 `test/src/utils/Baseline.cpp:150-178`），Layer 1-3 一律用 `EXPECT_EQ / EXPECT_NEAR / EXPECT_TRUE`；§18.1 新增"各层工具对照表"，§18.4bis 新增"工具选择约束"段 + 正反例 + 断言模板，防止 AI 误用 Baseline::Compare 做字节流/对象级验证。

- v1.4 → v2.0 修订要点（基于 v1 压缩手法复盘的系统性重构；文档版本跨越一位反映字节布局重大变化）：
  - **方案 A1：Color u8×4 RGBA**。附录 D.1 Color 从 `4 × f32 = 16 B` 改为 `4 × u8 = 4 B`（sRGB 8-bit）；HDR 路径通过新 TagCode 变体（§6.5）扩展。`ValueCodec` 新增 `FloatToU8` / `U8ToFloat` + `WriteColorRGBA8` / `ReadColorRGBA8`。预估 5-10% 体积节省。
  - **方案 A2：bool 聚合为 bitflags**。LayerBlock 的 5 个 bool 聚为 `u8 layerFlags`（§D.8），ElementTextPath 的 3 个 bool 聚为 `u8 pathFlags`，ElementTextModifier 的 3 个 hasXxx 聚为 `u8 modifierFlags`；新增 `layer_flags` / `path_flags` / `modifier_flags` 三个位域常量命名空间。
  - **方案 A3：Property isDefault 位**。§4.3 重写 `propHeader` 为位域（bit0-3 encoding / bit4 isDefault / bit5 hasExt / bit6-7 reserved）；§4.4 重写未知 encoding 兜底策略；附录 C.2 补 defaultValue 唯一来源纪律；`ValueCodec` 新增 `WriteProperty<T>` / `ReadProperty<T>` 模板。isDefault 省略 value 字节，静态场景预估 40% Property 空间节省。
  - **方案 B1：Path 量化编码**。附录 D.2 拆为 D.2.1（裸 float，pathFormat=0）和 D.2.2（量化 + 3-bit verb + 动态位宽坐标 + 量化 conicWeight，pathFormat=1，默认）；Baker 通过 `ChoosePathFormat` 自动选择；预估复杂矢量场景 30-50% Path 空间节省。
  - **方案 B2：ShapeStyleData 分支序列化**。§D.11 从"无条件序列化全字段"改为"u16 innerLength 包裹 + 按 sourceType 分支"；SolidColor 从 ~300 B 降到 ~6 B。
  - **方案 C1：Matrix/Matrix3D 默认值位**。附录 D.1 增加 matrixHeader（isIdentity / hasTranslateOnly）；I 矩阵从 24 B → 1 B；预估静态 Layer 场景 10-30 KB 节省。
  - **方案 C2：GlyphRun 量化**。GlyphRunBlob inline 布局重构为 `glyphPrecisionLog2` + 分量数组（position/anchor/scale/rotation/skew 各自动态位宽）；中文大段文字场景典型压缩比 ~80%。
  - **方案 D1：TagCode 分段预留**。§6.1 分段表扩大每段预留区间；新增"动画专用 120-199 / 资源扩展 200-299 / 版本化变体 300-599 / 实验性 900-1022"四个段；附录 D.12 Filter / Style TagCode 从 70-84 迁移到 80-102（v2 未发布，无兼容性代价）。
  - **方案 D2：兼容性三级保障**。新增 §6.5 显式定义三级兼容机制：字段级追加 / Tag 级版本化（借鉴 v1 `LayerAttributesV2/V3` 机制）/ 文件级 FORMAT_VERSION 升级。
  - **位级工具清单**。附录 D.1 新增"流工具复用清单"+"v2 新增工具表"，锁死 v2 不得重写底层位/变长编码，只能包装 v1 `writeInt32List` / `writeFloatList` / `writeUBits` 等。
  - **预估综合体积节省**：10MB 级 PAGX 转出 PAG 可缩小 **15-25%**。

- v1.3 → v1.4 修订要点（P0 阻塞修复）：
  - 修正 §8.5 `DecodeContext::syncFromStreamContext()`：v1 `StreamContext`（`src/codec/utils/StreamContext.h:30-47`）**没有** `errorMessage()` / `clearException()` 方法，实际只有 `hasException()` 和 public field `errorMessages`（`std::vector<std::string>`）。实现改为遍历 `errorMessages` 后 `clear()`，避免 AI 按文档落地时编译失败（E1）；
  - 修正附录 C.7 `ElementRepeaterData::order` 默认值拼写：`RepeaterOrder::Below` → `RepeaterOrder::BelowOriginal`，与附录 E.8 和 `EnumMeta<RepeaterOrder>::Default` 保持一致（E2）；
  - 补充附录 D.1 `ColorSpace` 处理强约束：`pagx::Color` 含 `colorSpace` 字段（`SRGB` / `DisplayP3`），`tgfx::Color` 和 PAGDocument 字节流**只承载 sRGB**。Baker **必须**通过 `ToTGFX(const pagx::Color&)`（`src/renderer/ToTGFX.cpp:70-85`）完成色域转换，不得手写 `{c.red, c.green, c.blue, c.alpha}` 直接赋值，否则 `DisplayP3` 样本会渲染偏色（E3）。

- v1.2 → v1.3 修订要点（基于"交叉验证实际项目代码"发现的事实性阻塞）：
  - 修正 `RenderUtil` API 形态：从返回 `std::vector<uint8_t>` 改为返回 `std::shared_ptr<tgfx::Surface>`，与项目既有 `Baseline::Compare` 的接口对齐（B-1）；
  - 修正 `Baseline.h` 路径引用：`test/framework/Baseline.h` → `test/src/utils/Baseline.h`（B-2）；
  - 修正 `tgfx::DisplayList` 使用模式：按 `test/src/PAGXTest.cpp:264-270` 的真实 API（栈对象 + `root()->addChild` + `render(Surface*, clearAll)`），不再使用文档虚构的 `setRoot()`/`draw(Canvas*)`（B-3）；
  - 补齐 `DecodeContext` 与 v1 `StreamContext` 的 bridge 机制：包含 `streamContext` 字段 + `syncFromStreamContext()` 方法，保证 v1 流级错误正确桥接到 v2 诊断体系（B-4）；
  - 敲定 TagHeader 字节格式：紧凑位打包（code 占 10 bit + length 占 6 bit；length≥63 时追加 u32），与 `src/codec/TagHeader.cpp:23-48` 字节兼容但使用 v2 自己的 TagCode 枚举，v2 独立实现（B-5、E-1）；
  - 附录 H 增加 Baker 侧资源约束（Edge-6）；
  - 附录 H.1 拆分 `MAX_STRING_BYTES` 为 `MAX_NAME_STRING_BYTES(64KB)` 和 `MAX_TEXT_STRING_BYTES(10MB)`（N-3）；
  - 附录 H.3 补充最小 UTF-8 校验算法伪码（含 4-byte UTF-8 支持，emoji 不漏）。

- v1.1 → v1.2 修订要点：
  - §5/§6 改为概念性描述，代码片段全部移至附录 C/D（消除与附录的双重定义不一致，S-1/S-2 问题）；
  - `Property<T>` 去掉 `reservedKeyframeBlob` 字段以减少百万级 VectorElement 场景内存开销（M-1）；
  - `LayerBlock` 的 `scrollRect` 改为条件序列化（`hasScrollRect==true` 时才写），节省体积并避免默认值歧义（M-2）；
  - §4.4 明确 Decoder 遇到未知 PropertyEncoding 的兜底策略（立即 seek 到 Tag 末尾，不尝试跳字节，M-3）；
  - `VectorElement` 结构从 14 unique_ptr 重构为 `std::variant`，节省每 element 约 100 字节（M-4）；
  - `ErrorCode` 102 `InvalidDocument` 拆分为 `NullDocument(102)` + `EmptyCompositions(103)`；新增 `InvalidCompositionIndex(306)` 与 `InvalidEnumValue(407)`（M-5/M-6）；
  - 新增 `ReadEnum<T>` 安全模板 + `EnumMeta<T>` 特化清单（M-6，附录 G.5）；
  - `Color` 字节布局定稿为 `4 × f32 RGBA`（基于 `tgfx::Color::RGBA4f`），不复用 v1 `WriteColor`（3 × u8）；新增 `WriteColorRGBA/ReadColorRGBA` 规范（S-4，附录 D.1）；
  - `RepeaterOrder` 映射定稿（PAGX/tgfx 完全一致，S-3，附录 E.8）；
  - 新增 §8.5 `EncodeContext`/`DecodeContext` 字段定义（R-2）；
  - Day-1 smoke 改为 pixel-to-pixel 直接比对、不依赖基准图（R-7），定义点唯一化到 §18.2；
  - `ErrorCode → 测试文件` 对应表（R-5，附录 G.6）；
  - 附录 H 增加 `MAX_VECTOR_ELEMENT_DEPTH = 64`（VectorGroup 嵌套上限，O-4）；
  - 附录 G 增加错误码编号分配规则（L-4，G.1）。

- v1.0 → v1.1 修订要点：
  - 渲染判据从"零容忍度 memcmp"改为 `Baseline::Compare`（§1.2、§17、§18）；
  - 取消 v1 播放器改动（v1 已能 graceful reject 0x02）（§4.1、§16、§19）；
  - 新增附录 C/D/E/F/G/H/I（PAGDocument 完整定义 / 字节布局 / 枚举映射 / 字段对照 / 错误码 / 安全上限 / 依赖规范）；
  - 公共 API 去掉 static last\* 改为 Result 结构并明确线程安全契约（§15）；
  - Property 壳的 `reservedKeyframeBlob` 明确不参与序列化（v1.1 定义；v1.2 完全移除该字段）；
  - GlyphRunBlob 字段定稿（附录 C.4）；
  - tgfx::Path 序列化方案定稿（附录 D.2）；
  - 错误码枚举化（附录 G）；
  - 安全上限量化（附录 H）。

---
