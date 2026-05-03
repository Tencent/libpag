# Phase 16 — PAGX→PAG Text Runtime-Shape 回退设计

**作者**：待接手 AI 填  
**日期**：2026-05-03  
**前置**：Phase 0–15（含 11.5/11.6）全部已落地  
**状态**：设计草案，待用户审阅

---

## 1. 背景

### 1.1 Phase 15 实测发现的文字完全不渲染

`test/out/PAGRenderTest_Render/text.webp` 文件大小 **46 bytes**（几乎空白），与 PAGX 原生渲染（`test/out/pagx_native/text.webp` = 8568 bytes）完全不一致。

`Phase12ProbeFixture.DISABLED_TextInflateDiag` 对 `spec/samples/text.pagx` 做的 Bake→Encode→Decode→Inflate 全流程诊断，**每个 ElementText 产出两条致命 warning**：

```
inflate.warn code=601 msg=tgfx::Typeface construction returned null
inflate.warn code=602 msg=BuildTextBlobFromLayoutRuns returned null; ElementText dropped
```

3 个 `<Text>` 节点 × 2 warning = 6 条，对应 3 个 Layer 全部丢失文字内容。

### 1.2 根因

`src/pagx/pag/LayerInflater.cpp::resolveFontAsset` 用 `tgfx::Typeface::MakeFromName(asset.family, asset.style)` 重建 typeface。**macOS 上 `tgfx::SystemFont::MakeFromName` 未实现**（`third_party/tgfx/src/core/vectors/freetype/SystemFont.cpp:267,276` 在非 `_WIN32` 分支硬 `return nullptr`）。

即使 MakeFromName 在别的平台能工作，**同一 family 在不同系统/版本上 glyph ID 表不同**，Phase 8 把 TextLayout shape 结果的 glyph IDs 存入 .pag 的做法本质上**绑定了序列化时的 typeface 实例**。加载端只能："要么拿到同一个 typeface，要么全废"。

### 1.3 Phase 8 设计意图与 v1 差异

| 维度 | v1 | Phase 8 TextBaker |
|---|---|---|
| 序列化内容 | 原始 `text` 字符串 + `fontFamily/fontStyle` + 样式字段 | shape 后的 glyph IDs + positions + xforms + FontAsset family/style |
| 加载端 | TextShaper 重新 shape；Glyph::BuildFromText 用 runtime registered typeface | 直接用 glyph IDs 构造 TextBlob，依赖 FontAsset 重建 typeface |
| 字体来源 | `PAGFont::RegisterFont(path)` / `SetFallbackFontPaths` + `FontManager` 注册表 | FontAsset.family → `tgfx::Typeface::MakeFromName`（macOS 永远 null） |
| 跨平台/跨版本 | 稳定（text+family 通吃） | 脆弱（glyph IDs 绑定 shape 时 typeface） |
| .pag 体积 | 小（只存文本） | 中（shape 结果 + xforms） |
| 首帧 CPU | 略高（需 shape） | 略低（直接 blob） |

Phase 8 选型的明面原因是"加载期省 shape 开销"，但用例约束是 **glyph IDs 必须能匹配原始 typeface** —— 这在非受控分发场景（跨平台、跨字体版本）几乎不可能保证。在 macOS 测试环境里这一缺陷直接爆炸为"整段文字空白"。

### 1.4 与 Phase 11.6 已发现的 15 个 CrossCheck FAIL 的关系

Phase 15 coverage 报告出来后，CrossCheck 的 15 个独立 FAIL 里 **8 个是文字相关**（`text/text_box/text_modifier/text_path/glyph_run/rich_text/pagx_features/complete_example`），其他 7 个（`composition/nebula_cadet/game_hud/image_pattern/trim_path/container_layout_include_in_layout/constraint_textbox_and_group`）包含文字层的样本也受这个根因影响。Phase 16 修完后，CrossCheck 的通过率将会显著提升。

---

## 2. 方向决策（用户确认）

**回退到 v1 runtime-shape 模式**：
- TextBaker 不再存 shape 后的 glyph IDs
- ElementTextData 只存**原始文本串 + 字体名 + 样式字段**（与 v1 TextDocument 对齐）
- Inflater 调 `TextShaper::Shape(text, typeface)` 动态 shape
- 字体由**调用方预注册**（类似 v1 `PAGFont::RegisterFont`），Inflater 通过可注入的 `FontProvider` 接口解析

**为什么选 v1 方案而不是"Bytes 嵌入"**：用户决策理由 —— 跟 v1 行为对齐，跨平台稳定、.pag 小、调用方集成负担与 v1 相同（用户已经为 v1 做过字体注册工作）。

---

## 3. 格式变更（核心）

### 3.1 ElementTextData 字段重设

**Phase 8 旧定义**（`src/pagx/pag/PAGDocument.h:389-393`）：
```cpp
struct ElementTextData {
  Property<Point> position;
  Property<std::vector<Point>> anchors;
  std::vector<GlyphRunBlob> glyphRuns;  // ← 要删
};
```

**Phase 16 新定义**（对齐 v1 `TextDocument`，`include/pag/types.h:class PAG_API TextDocument`）：
```cpp
struct ElementTextData {
  Property<Point> position = MakeProp(Point{});
  Property<std::vector<Point>> anchors = MakeProp<std::vector<Point>>({});  // 保留，Text modifier 还会用到

  // 内容
  std::string text = "";
  std::string fontFamily = "";
  std::string fontStyle = "";
  float fontSize = 12.0f;

  // 朝向 / 排版
  TextDirection direction = TextDirection::Default;   // 新增 enum，对齐 v1
  ParagraphJustification justification = ParagraphJustification::LeftJustify;  // 对齐 v1
  float leading = 0.0f;    // 行间距；0 = auto (fontSize * 1.2)
  float tracking = 0.0f;   // 字符间距
  float firstBaseLine = 0.0f;
  float baselineShift = 0.0f;

  // BoxText
  bool boxText = false;
  Point boxTextPos = {};
  Point boxTextSize = {};

  // Faux style（没真的 bold/italic typeface 时由渲染器合成）
  bool fauxBold = false;
  bool fauxItalic = false;

  // Paint
  bool applyFill = true;
  bool applyStroke = false;
  bool strokeOverFill = true;
  Color fillColor = {};
  Color strokeColor = {};
  float strokeWidth = 1.0f;

  // 背景（v1 v2 TextSourceV2+ 才有）
  Color backgroundColor = {};
  uint8_t backgroundAlpha = 0;
};
```

**删除的结构**（`src/pagx/pag/PAGDocument.h`）：
- `struct GlyphRunBlob`（含嵌套 `LayoutGlyph` / `ClassicGlyph`）
- `enum class GlyphRunKind`
- `FontAsset` 结构**收窄**：不再需要 `axes` / `data` / `style`，本身可以直接移除（family/style 下沉到 ElementTextData）。**或**保留空壳以维持 TagCode 数字不变，置 `deprecated = true` 注释。建议直接删除 + TagCode 永久保留不重用（§6.4 向前兼容性）。

### 3.2 TagCode 调整

**`src/pagx/pag/TagCode.h`**：
- `FontAssetTable = 3`、`FontAssetItem = 7`、`TextPayload = 21`、`ElementText = 50`、`ElementTextPath = 51`、`ElementTextModifier = 52` —— **全部保留**，不重新分配。
- **`ElementText = 50` 的 payload schema 改变**（不兼容）。
- **`FontAssetTable = 3` 从 ImageAssetTable 之后的必选 Tag 中移除**：新 ElementTextData 自带 family/style，不再需要独立的 font asset pool。需要同步更新 §6.2 body 顶层写入顺序（`FileHeader → ImageAssetTable → ~~FontAssetTable~~ → CompositionList → End`）。
- `PAGDocument::fonts` 向量移除；`FontAsset` 结构体移除。

### 3.3 FORMAT_VERSION bump

设计文档 §4.1.1 定义的版本管理决策表要求：**必选 Tag 字段语义不兼容变更 → FORMAT_VERSION 必须升**。ElementText payload schema 重排算重大变更，**FORMAT_VERSION 从 2 升到 3**（或按设计文档实际数字）。老 .pag v2 文件将被拒绝（当前 v2 尚未对外发布，不构成用户破坏）。

---

## 4. Baker 侧重写

### 4.1 TextBaker::BakeText 新版伪代码

```
std::unique_ptr<VectorElement> TextBaker::BakeText(BakeContext& ctx, PAGDocument& doc,
                                                   const pagx::Text& src) {
  auto data = std::make_unique<ElementTextData>();
  // Position / anchors 逻辑不变
  data->position = MakeProp(tgfx::Point{src.position.x, src.position.y});
  // anchors 字段 v1 没有，但 PAGX Text 有 anchors —— 保留；如果空则发空向量

  // 内容 & 字体名 & 样式
  data->text = src.text;
  data->fontFamily = src.fontFamily;
  data->fontStyle = src.fontStyle;
  data->fontSize = src.fontSize;

  // 样式字段拷贝（PAGX 节点字段 → ElementTextData 字段，一对一映射）
  data->justification = ConvertPAGXAlignToPAGJustify(src.align);
  data->direction = ConvertPAGXWritingMode(src.writingMode);
  data->leading = src.lineHeight;
  data->tracking = src.letterSpacing;
  data->firstBaseLine = src.firstBaseline;
  data->baselineShift = src.baselineShift;
  // ...（Fill/Stroke/Background 参照现有 Baker 做法，走 Fill/Stroke 子 element 聚合逻辑）

  // BoxText 判定：PAGX 的 TextBox 或 Text 带 maxWidth 等
  if (auto* box = PAGXTextBoxOf(src)) {
    data->boxText = true;
    data->boxTextPos = ...;
    data->boxTextSize = ...;
  }

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Text;
  el->payload = std::move(data);
  return el;
}
```

### 4.2 TextBaker::BakeTextPath / BakeTextModifier

- **BakeTextPath**：保持现状（`ElementTextPathData` 只存路径 + 基线参数，不涉及 glyph）。PAGX 的 TextPath 语义是"把文本沿曲线排"，曲线本身 serialize 即可；具体文字排布在 Inflater 端 shape 之后再做。
- **BakeTextModifier**：保持现状。TextModifier 不涉及字体/文本本身，只是 transform + range selector。

### 4.3 FontEmbedder 模块

`src/renderer/FontEmbedder.cpp` 原本是为 Phase 10 "OutlineAll 模式 + Embedded FontAsset" 准备的。Phase 16 重定方向后，**FontEmbedder 在 .pag 格式中失去意义**（Baker 不再产出 FontAsset），但它仍是 PAGX 自身的 HTML/SVG 导出链路组件（`src/pagx/html/` / `src/pagx/svg/` 用）——**不要删除**，只是从 PAG Baker 链路里剥离。

### 4.4 pagx::Text 节点的 GlyphRuns 预 shape 模式

PAGX 本身支持两种 Text：
1. runtime shaping（常见，`text.glyphRuns.empty()`）
2. pre-shaped（罕见，`text.glyphRuns` 非空，作者把 glyph ID + 字形几何都打包在 pagx 里）

v1 PAG 不支持 pre-shaped —— v1 TextDocument 根本没有 glyph ID 字段。**Phase 16 策略**：pre-shaped 的 PAGX Text **无损编码到 .pag 不可能**，Baker 遇到 `text.glyphRuns` 非空时：
- 选项 A：降级为 runtime shaping（把 glyph ID 展开回原文本，丢失 per-glyph xform/scale/rotation）+ 发 warning（新错误码 `TextGlyphRunsDowngraded`）
- 选项 B：拒绝（抛 fatal，让用户自己转换 .pagx）

**建议选项 A**：PAGX 的 pre-shaped 场景用得少；真正用的人（需要 per-glyph scale/rotation）可以通过 PAGX native 渲染链路，不走 .pag 通道。

---

## 5. Inflater 侧重写

### 5.1 FontProvider 注入接口（新增）

```cpp
// include/pagx/pag/FontProvider.h（新建）
namespace pagx::pag {

class FontProvider {
 public:
  virtual ~FontProvider() = default;

  // 按 family+style 查 typeface。返回 nullptr 表示未注册，调用方决定后续 fallback。
  virtual std::shared_ptr<tgfx::Typeface> getTypeface(const std::string& fontFamily,
                                                     const std::string& fontStyle) = 0;

  // 获取 fallback 列表（按顺序尝试，第一个能 shape 出 glyph 的为准）。
  virtual std::vector<std::shared_ptr<tgfx::Typeface>> getFallbackTypefaces() = 0;
};

// 默认实现：查 tgfx::Typeface::MakeFromName（Windows 可用，macOS 返回 null）；无 fallback。
std::shared_ptr<FontProvider> MakeDefaultFontProvider();

}  // namespace pagx::pag
```

### 5.2 LayerInflater::Options 扩展

```cpp
// src/pagx/pag/LayerInflater.h
struct Options {
  std::shared_ptr<FontProvider> fontProvider = nullptr;  // 新增
  // ...其他字段
};

static Result Inflate(std::unique_ptr<PAGDocument> doc, Options opts = {});
```

`Inflate` 内部若 `opts.fontProvider == nullptr` 就用 `MakeDefaultFontProvider()`。

### 5.3 inflateElementText 新版

```cpp
std::shared_ptr<tgfx::VectorElement> inflateElementText(const ElementTextData& pay,
                                                        InflaterContext* ctx) {
  // 1. 解析 typeface
  auto typeface = ctx->fontProvider->getTypeface(pay.fontFamily, pay.fontStyle);
  if (!typeface) {
    auto fallbacks = ctx->fontProvider->getFallbackTypefaces();
    if (!fallbacks.empty()) typeface = fallbacks.front();
  }
  if (!typeface) {
    ctx->warn(ErrorCode::InflateFontCreateFailed,
              "no typeface for " + pay.fontFamily + "/" + pay.fontStyle);
    return nullptr;  // 或者返回一个 "text missing" 占位 element
  }

  // 2. tgfx Font 构造
  tgfx::Font font(typeface, pay.fontSize);
  font.setFauxBold(pay.fauxBold);
  font.setFauxItalic(pay.fauxItalic);

  // 3. TextShaper::Shape 复用现有逻辑
  auto shapedGlyphs = pag::TextShaper::Shape(pay.text, typeface);
  if (shapedGlyphs.empty()) {
    ctx->warn(ErrorCode::InflateGlyphRunBuildFailed, "TextShaper::Shape returned empty");
    return nullptr;
  }

  // 4. 基于 shapedGlyphs + pay.tracking + pay.leading + pay.justification + pay.boxText
  //    做布局（paragraph），得到 TextBlob
  //    —— 这一步是新的布局器（或复用 v1 的 TextLayout 子系统，见 §5.4）
  auto textBlob = LayoutParagraphToTextBlob(shapedGlyphs, font, pay);

  // 5. tgfx::Text::Make + setPosition
  auto text = tgfx::Text::Make(textBlob, pay.anchors.value);
  text->setPosition(pay.position.value);

  // 6. Paint（fill/stroke/color）—— 直接走现有 ShapeStyleData 逻辑，或者内联 ElementTextData 上
  ApplyPaint(text, pay);

  return text;
}
```

### 5.4 布局器：LayoutParagraphToTextBlob 的选择

**两套现成布局器**：
1. v1 `src/rendering/utils/shaper/TextLayout.cpp` —— v1 TextRenderer 用的那套，`GetLines()` + `Justify()`，直接就地复用最省事
2. `src/pagx/TextLayout.cpp` —— PAGX 自己的布局器，参数结构稍不同

**建议**：复用 v1 `TextLayout`，因为输入结构（`TextDocument`）与新 `ElementTextData` 字段**一对一对齐**，只要 wrapper 函数把 `ElementTextData` 塞进 `TextDocument` 实例即可。这样 Inflater 和 v1 渲染链路**同源**，避免双维护。

**代价**：Inflater 需要依赖 `src/rendering/graphics/Text.cpp` 及其附近的 v1 渲染代码。由于 `pag` 静态库本身已经把 v1 rendering 编译进去，符号在同一个 lib 里，直接 include 不难。

### 5.5 FontProvider 的默认实现

`MakeDefaultFontProvider()` 在 macOS 上 `MakeFromName` 永远返回 null。调用方必须注入自己的 FontProvider —— **测试环境使用 PAGXTest 已经在用的字体注册逻辑**，生产使用 libpag v1 的 `FontManager` 适配器：

```cpp
class FontManagerAdapter : public FontProvider {
  std::shared_ptr<tgfx::Typeface> getTypeface(const std::string& f, const std::string& s) override {
    return pag::FontManager::GetTypefaceWithoutFallback(f, s);  // 复用 v1 全局 FontManager
  }
  std::vector<std::shared_ptr<tgfx::Typeface>> getFallbackTypefaces() override {
    std::vector<std::shared_ptr<tgfx::Typeface>> out;
    for (auto& h : pag::FontManager::GetFallbackTypefaces()) out.push_back(h->getTypeface());
    return out;
  }
};
```

---

## 6. 测试重写与删除

### 6.1 删除的测试

- `test/src/pag/unit/GlyphRunBlobCodecTest.cpp`（272 行）—— GlyphRunBlob 结构删除后，整个测试文件失去意义。
- `test/src/pag/unit/TextBakerTest.cpp`（268 行）中所有检查 `ElementTextData::glyphRuns` 字段的用例 —— **整个文件大改**（估计保留约 20-40% 用例，其余重写）。

### 6.2 重写的测试

- `test/src/pag/unit/TextBakerTest.cpp` —— 新断言针对 `ElementTextData.text / fontFamily / fontStyle / fontSize / justification / leading / tracking / fauxBold / ...` 字段的正确序列化。
- `test/src/pag/unit/LayerInflaterParityTest.cpp` —— Inflate 后检查 tgfx::Text 是否被生成（之前检查 glyphRuns.size()）。需新增 FontProvider 注入路径。
- `test/src/pag/unit/LayerInflaterTest.cpp` —— 同上。
- `test/src/pag/unit/RoundTripTest.cpp` 文本相关用例 —— 更新 schema。

### 6.3 新增的测试

- `test/src/pag/unit/FontProviderTest.cpp`（新）—— 测试 FontProviderAdapter / FallbackChain / 缺字体 warning 路径。
- `test/src/pag/unit/TextInflateShapingTest.cpp`（新）—— 对 `text.pagx` / `text_box.pagx` / `rich_text.pagx` 等 fixtures，**验证 Inflate 出来的 tgfx::Layer 树带有非空 TextBlob**（不依赖渲染像素对比）。

### 6.4 Phase 15 RenderEquivalenceTest 48 张基线

48 个样本中的**文字相关 ≥ 8 个**渲染结果会从"空白"变成"接近 PAGX 原生"。其余不含文字的 ≥ 40 个样本**应当**不受影响（仅 ElementText 路径重写，其他 Vector/Shape/Style 路径不动）。

Phase 16 完成后，需要用户**重新 `/accept-baseline`**（48 个里至少文字那几张会变）。

### 6.5 TextBakerTest.cpp 改动清单（预估）

当前用例主题（根据文件名和结构猜）：
- BakeText_WithPreshapedGlyphRuns → **删除**（PAGX pre-shaped 降级已统一在新路径）
- BakeText_EmitsClassicGlyphRunBlob → **删除**
- BakeText_RuntimeShaping_EmitsLayoutRun → **重写**（断言改为 `data->text == "..."` 和 `data->fontFamily == "Arial"`）
- BakeText_FontSourceMissing_Warn → **保留**（改为：fontFamily empty 时发 `FontSourceMissing=201`）
- BakeTextPath_PathFields → **保留**（TextPath 不变）
- BakeTextModifier_AllProperties → **保留**（TextModifier 不变）

---

## 7. 实施阶段拆分

| 子 Phase | 交付 | 估算粒度 | 测试门槛 |
|---|---|---|---|
| **16.0** 设计定稿 | 本文档 + 用户审阅意见融合 + docs/pagx_to_pag_v2_design.md §C.6/§G.2 同步标注 "v2 legacy, replaced by Phase 16" | 1 轮设计对话 | 文档 review 通过 |
| **16.1** PAGDocument 结构 + FontProvider 接口 | 新 `ElementTextData` 字段、删 `GlyphRunBlob`、FontProvider 接口 header + MakeDefault 实现 | M | `PAGDocumentTest` 编译通过 + FontProviderTest 绿 |
| **16.2** TextBaker 重写 | BakeText 用新 fields；BakeTextPath/TextModifier 微调；pre-shaped 降级路径 + 新 warning | M | `TextBakerTest` 新用例全绿 |
| **16.3** Codec 读写 | `ElementText` / `ElementTextPath` / `ElementTextModifier` tag 的 Write/Read 对齐新 schema；**删 `FontAssetTable` / `FontAssetItem` 写路径**（读路径按 §6.4 向前兼容性保留读空骨架） | M | `RoundTripTest` 文字用例绿 |
| **16.4** LayerInflater inflateElementText 重写 | TextShaper 动态 shape + 布局器选择 + FontProvider 注入 | M-L | `LayerInflaterTest` / `LayerInflaterParityTest` 绿 + 新 TextInflateShapingTest 绿 |
| **16.5** 测试基础设施 | RenderEquivalenceTest 注入 FontProvider；PAGXTest.SpecSamples 使用的 font 注册逻辑下沉到共享 test util | S | 48 个 RenderEquivalence 中文字相关的 8+ 个文字能渲染出来 |
| **16.6** Baseline 接受 | 用户 `/accept-baseline` 接受新截图；AI 确认所有 CrossCheck 文字类 fail 转绿 | — | 用户操作 + 新跑 coverage 验证 |

### 7.1 风险 + 开放问题

1. **TextShaper 依赖 `PAG_USE_HARFBUZZ`**：项目已开启（`CMakeLists.txt` PAG_BUILD_PAGX 开 HarfBuzz）——但 Phase 16 代码依赖链会从"src/pagx/pag 不涉及 shaper"升级到"Inflater 里调 v1 TextShaper"。意味着 `pag` 静态库的 Inflater 层需要链 HarfBuzz 符号。应当已链好（v1 TextRenderer 已用 TextShaper），验证一下即可。
2. **`pagx::Text` anchors 字段**：v1 TextDocument 没有 anchors，PAGX 有。保留 `Property<std::vector<Point>> anchors` 字段，但渲染阶段怎么用 anchors 要考证 tgfx::Text API 是否支持（PAGX 原生渲染本身就用了，说明 API 存在）。
3. **BoxText 宽高与 firstBaseLine 的依赖关系**：v1 TextDocument 里 `boxTextPos / boxTextSize / firstBaseLine` 有特定约束，复用 v1 布局器时要注意输入结构等价。
4. **TextModifier + RangeSelector 路径**：这些在 Phase 16 中维持 Phase 8 行为。但 TextModifier 选区单位（Character / Word / Line）目前按 glyph 运算，新 runtime-shape 后单位语义有微妙差异——**标记为 Phase 16.7 follow-up**，Phase 16 先过基本文字，Modifier 作二轮。
5. **Pre-shaped PAGX GlyphRuns 完全降级的信息损失**：如果用例文件有用 per-glyph scale / rotation，会丢失。Phase 16.2 发 `TextGlyphRunsDowngraded` warning 提示作者，但产物仍能播放。

### 7.2 依赖矩阵

| 子 Phase | 硬依赖 |
|---|---|
| 16.0 | —（设计） |
| 16.1 | 16.0 |
| 16.2 | 16.1 |
| 16.3 | 16.1, 16.2 |
| 16.4 | 16.1, 16.3, v1 TextShaper + FontManager（已有） |
| 16.5 | 16.4 |
| 16.6 | 16.5 + 用户手动 accept-baseline |

---

## 8. 对主设计文档 (`pagx_to_pag_v2_design.md`) 的同步改动清单

设计文档以下章节将过时，需 Phase 16.0 同步打 "Replaced by Phase 16" 标记 + 链接本文档：

- **§C.6 ElementText** —— schema 完全重写
- **§C.7 FontAsset** —— 整节删除或标记废弃
- **§G.2 FontSourceMissing=201** —— 保留但 context 变更
- **§G.2 601/602 (InflateFontCreateFailed / InflateGlyphRunBuildFailed)** —— 仍然有效，但触发路径变化
- **§6.1 TagCode 分段** —— FontAssetTable / FontAssetItem 标记 "reserved, unused"
- **§6.2 body 顶层写入顺序** —— 去掉 `FontAssetTable`
- **§7 Baker 子模块与映射规则 / TextBaker 行** —— 职责范围更新
- **§19 Phase 表** —— 新增 Phase 16 行
- **§22 维护日志** —— 新增条目 v2.20（或实际版号）说明回退原因

---

## 9. 用户需审阅确认的关键决策

请在本文档 reviewed 后明确以下选项，AI 才会开始 Phase 16.1 编码：

- [ ] **Q1**：Phase 8 FontAsset 结构是直接删除还是保留空壳？（建议直接删除；保留会产生持续维护负担）
- [ ] **Q2**：PAGX pre-shaped GlyphRuns 在 Baker 遇到时选项 A（降级 warning）还是选项 B（拒绝 fatal）？（建议 A）
- [ ] **Q3**：Inflater 布局器复用 v1 `TextLayout` 还是引入 `src/pagx/TextLayout`？（建议 v1，同源）
- [ ] **Q4**：是否把 FontProvider 设计成 Inflate 必传参数（无默认），迫使调用方显式提供？还是像当前 v1 那样默认全局 FontManager？（建议后者，与 v1 UX 对齐）
- [ ] **Q5**：FORMAT_VERSION 要不要升？（按设计文档 §4.1.1 必选 Tag 字段语义不兼容变更规则—— ElementText schema 不兼容变更确实要升。确认后在 §4.1 改数字）
- [ ] **Q6**：Phase 16 是否包含 TextModifier + RangeSelector 的字符单位语义重校准？（建议不包含，作 Phase 16.7 follow-up）

---

## 10. 交付完成标准

- Phase 16.0 ~ 16.5 全部子 Phase 合入
- 48 个 RenderEquivalence 样本里所有含文字的样本 inflate 后有非空 TextBlob
- Phase 11.6 遗留的 15 个 CrossCheck FAIL 减少到 ≤ 7 个（文字相关 FAIL 清零；其他 image_pattern/trim_path 等非文字 FAIL 仍可能在后续 Phase 17+ 处理）
- `./tools/coverage.sh` 重跑后 `src/pagx/pag/LayerInflater.cpp` 和 `src/pagx/pag/TextBaker.cpp` 覆盖率 ≥ 75%（因为新的测试多了）
- 设计文档 §C.6 / §C.7 / §6 / §19 / §22 同步更新
- 用户 `/accept-baseline` 接受新 baseline 图
