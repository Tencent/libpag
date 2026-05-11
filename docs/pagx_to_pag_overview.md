# PAGX → PAG v2 技术方案总览

> **面向对象**：项目技术决策审阅者 / Tech Lead。
> **目的**：独立阅读即能理解 PAGX→PAG v2 链路的架构取舍、数据格式、压缩与扩展性机制。
> **体例**：少而精——6 个核心决策深度展开；周边机制列举不展开。
> **权威细节**：字节级字段表 + 全部 TagCode 列表见 `docs/pagx_to_pag_v2_design.md` 附录 D / §6.1；历史演进见 `docs/pagx_to_pag_v2_changelog.md`；压缩优化备选方案见 `docs/phase20_static_compression.md`。

---

## 1. 起点：问题与约束

### 1.1 问题域

PAGX 是作者层可编辑的 XML 格式（`<Layer>` / `<Rectangle>` / `<Text>` / `<LinearGradient>` ...），包含约束布局、运行时字体解析、SVG 导入等动态能力。运行时播放需要解码成扁平的 `tgfx::Layer` 树后才能渲染。

PAG v2 是 PAGX 的**二进制产物**：

- **PAGX → PAG**（Baker + Codec::Encode）：作者一次性生成，产出静态可播放字节流
- **PAG → tgfx::Layer**（Codec::Decode + LayerInflater）：运行时快速加载，无布局解析、无字体 shape 开销

设计目标是严格"运行时等价"——PathA（PAGX 直接渲染）与 PathB（PAGX→PAG→Inflater 渲染）应像素级一致（`CrossCheck 48/48 bit-perfect`，`ComparisonDumpTest` 273/273 ok）。

### 1.2 关键约束

1. **v2 尚未对外发布**，允许 wire format 破坏性演进（FORMAT_VERSION 保持 `0x02`；Phase 16/17/18 多次变更 schema 未升版本号）
2. **与 PAG v1 codec (`src/codec/`) 并存**——v2 不替换 v1，是独立命名空间（`pagx::pag::`）
3. **v1 能力不倒退**：关键帧 / 动画曲线 / MotionPath 等高级动画预留 wire 槽位，v2 当前仅支持静态文档（encoding = Constant）
4. **设计文档权威**：`pagx_to_pag_v2_design.md` 6820 行是字节级契约的唯一真源；本文档引用之，不重复字段表

### 1.3 决策窗口

当前阶段（2026-05）：

- CrossCheck 48/48 全绿（Phase 17 case A/B 双路径 + Phase 18 TextBox inverseMatrix）
- Phase 19/20 压缩优化评估完毕，**7 个候选合计仅 ~6.6%**，均不单独启动（详见 `phase20_static_compression.md`）
- 动画扩展未启动（预留 Property encoding 1-15 / Tag 段位 160-239）

---

## 2. 六个核心决策

下列 6 个决策是 v2 架构的支柱，直接决定了性能、扩展性与失败模式。

### 2.1 决策一：Baker/Inflater 非对称 —— fatal/warn 单向流

#### 核心规则

| 环节 | 诊断范围 | fatal 行为 | warn 行为 |
|---|---|---|---|
| **Baker** (`src/pagx/pag/Baker.cpp`) | 100-299 | 返回 `doc=nullptr`，拒绝产出字节 | 产出 doc，带降级警告 |
| **Codec::Encode** | 400-499 only | 无 fatal（Baker 已门控） | 写入空字段 + warn |
| **Codec::Decode** | 300-499 | 返回 `document=nullptr` | 跳过字段，保留主干 |
| **LayerInflater** (`LayerInflater.cpp`) | 600-699 **only warn** | **不存在 fatal** | 子树/元素降级为 empty 但不丢主干 |

Inflater **没有 fatal 通道**是 v2 最重要的稳定性承诺——播放器拿到 .pag 字节永远能渲染出"至少一部分"，不会因单元素损坏整屏黑。

#### 为什么这样设计

PAG 部署场景：
- 作者端（Baker）愿意为"干净数据"付复杂校验成本（MAX_LAYER_DEPTH / MAX_GLYPHS_PER_RUN 等 10+ 上限、mask 环检查、import 解析门控）
- 播放端（Inflater）不能因一个字体缺失或 image 尺寸超限就放弃整文件——必须**失败隔离**到 Layer 粒度

这决定了两边的代码风格：

- Baker：大量 `ctx.error(code, msg)` + `return false` 的门控，失败有契约
- Inflater：大量 `ctx.warn(code, msg)` + 降级为 `tgfx::Layer::Make()` 空节点，失败被吸收

#### 诊断码段位（`include/pagx/Diagnostic.h`）

```
100-199  Baker fatal      (LayoutNotApplied=100 / UnresolvedImports=101 / StructureLimitExceeded=105 / ProducerFatal=106)
200-299  Baker warning    (ImageSourceMissing=200 / FontSourceMissing=201 / TextSelectorTypeUnsupported=209)
300-399  Codec fatal      (InvalidMagic=300 / UnsupportedVersion=301 / MalformedTag=304 / PrematureEndTag=409)
400-499  Codec warning    (UnknownTagCode=400 / UnknownPropertyEncoding=401 / InvalidEnumValue=407)
600-699  Inflater warning (InflateImageDecodeFailed=600 / TextShapingHintMiss=608 / InflaterLayerBudgetExceeded=606)
```

段位之间**永远不迁移**，段内数值**永不复用**（`TextGlyphDataEmpty=206` 已退役但 206 数值不重分配）——这是 ABI 红线。

---

### 2.2 决策二：Tag Header 紧凑格式 + "长度自描述"前向兼容

#### 字节布局（`src/pagx/pag/TagHeader.cpp`）

```
u16 codeAndLength             # high 10 bits = TagCode (0..1023)
                              # low  6 bits = length    (0..62)
[u32 extendedLength]          # 仅当 low 6 bits == 63
```

- length < 63：2 byte 头开销
- length ≥ 63：2 + 4 = 6 byte 头开销（约 90% 小 Tag 走快速通道）

#### 为什么设计成 10:6 而不是 8:8 或 varU32

- **TagCode 10 bit = 1024 槽位**：足够 v2 长期扩展（目前已定义 42 个，占 4%），且与 v1 TagHeader 字节宽度同构（v1 也是 10+6）
- **6 bit length 覆盖 90% case**：v2 Tag 体积分布——ImageAssetItem / EmbeddedFontItem 常> 63 byte 走 extended，其它 Layer/Element 小 Tag 走 2 byte 快速通道
- **不用 varU32 length**：varU32 读取成本比 u16 高 3×，且 fast path（2 byte 固定）更利 Reader 预取对齐

#### 前向兼容机制（§6.4）

Reader 碰到未知 TagCode 时：

```cpp
// Codec.cpp:197-244 主循环
uint64_t tagEnd = stream.position() + header.length;
switch (header.code) {
  case FileHeader: ReadFileHeader(...); break;
  // ...已知 Tag...
  default:
    ctx.warn(UnknownTagCode, "unknown TagCode at top level; skipped");
    break;
}
SeekTo(&stream, tagEnd);  // 无条件跳至 tagEnd，不依赖 Reader 识别
```

这让**新版 Writer 产出的文件被旧 Reader 读取时**，未知 Tag 整段跳过，已知 Tag 正常解析——所有 append-only 的新增 TagCode / Tag 内末尾追加字段都天然前向兼容。

#### 实际应用

- Phase 17 新增 `EmbeddedFontTable=8` / `EmbeddedFontItem=9`：旧 Reader warn + skip
- Phase 17 退役 `FontAssetTable=3` / `FontAssetItem=7`：TagCode 数值冻结不复用（防止 Reader 按新 schema 误读旧数据）
- Phase 18 ElementText body 尾部追加 `textBoxInverseMatrix`：通过 `if (s->position() < te) d->textBoxInverseMatrix = ReadMatrix(s);` 优雅探测

---

### 2.3 决策三：Property\<T\> + propHeader 单字节默认值短路

#### 数据模型（`src/pagx/pag/PropertyEncoding.h`）

```cpp
template <typename T>
struct Property {
  PropertyEncoding encoding = PropertyEncoding::Constant;
  T value;
};

enum class PropertyEncoding : uint8_t {
  Constant = 0,
  // 1..15 reserved for future Hold / Linear / Bezier / Spatial keyframes
};
```

#### propHeader 字节布局

```
bit 0-3  encoding   (Constant=0; 1..15 reserved)
bit 4    isDefault  (1=payload 省略,Reader 回 defaultValue)
bit 5-7  reserved   (writer 必须 0 / reader 必须忽略)
```

#### Wire 行为

静态文档（v2 当前 100%）只用 encoding=Constant：

```
WriteProperty(prop, defaultValue):
  isDefault = (value == defaultValue)
  if isDefault:
    write u8(0x10)        # 1 byte propHeader,bit 4 置位,payload 省略
  else:
    write u8(0x00)        # 1 byte propHeader
    WriteValue<T>(value)  # 完整 payload
```

全 48 sample 统计：估计 ~7,700 个 Property 字段实例，大部分命中 isDefault → 仅付 1 byte header cost（若不含此机制每字段要付 4-100 byte payload）。

#### 未来动画编码的扩展点

Property encoding 4 bit 的 1-15 槽位预留给关键帧编码：

```
1 = Hold          (无插值,整段保持起始值)
2 = Linear        (线性插值)
3 = Bezier        (三次贝塞尔,带 In/Out 控制点)
4 = Spatial       (空间轨迹,带 2D 控制点)
5..15 = reserved
```

Reader 碰到未知 encoding 时（未来老 Reader 读新动画文件）走 §4.4 规则：

```cpp
// PropertyEncoding.h:334-341
if (encoding == Constant) { /* read payload */ }
else {
  // skip 到 enclosingTagEnd,丢弃整个 Tag 剩余字段
  if (enclosingTagEnd > stream->position())
    stream->skip(enclosingTagEnd - stream->position());
  return MakeProp(defaultValue);
}
```

关键性质：**未知 encoding 不导致 fatal**，Reader 静默降级为默认值。动画字段未启动前，这个机制让老 v2 Reader 对将来的动画 .pag 保持"至少可播静态快照"。

#### 已评估但未实施的优化

**propHeader 位流压缩（Phase 20 方向 2）**：借鉴 PAG v1 `AttributeBlock`，把 N 个 Property 的 isDefault 标志打包到前置 1-bit 位流。每字段从 1 byte 降到 1 bit，全库节省 ~2.87%。因单独收益不足触发 wire bump，留给动画扩展同期落地。详见 `phase20_static_compression.md` §2。

---

### 2.4 决策四：Matrix/Color/Path 按"常见情况"分档压缩

#### Matrix 三档（`ValueCodec.h:162-213`）

```
isIdentity           → 1 byte  (header bit 0)
hasTranslateOnly     → 1 + 8 = 9 byte  (header bit 1 + tx,ty)
full 6-float         → 1 + 24 = 25 byte (header + scale/skew/trans × 2)
```

合理性：PAG 文档中绝大多数 LayerTransform / gradientMatrix / patternMatrix 是 Identity（没设置过 transform）或仅平移（layout 产生的 position 偏移）。全 6 float 仅在作者显式旋转/缩放时出现。

#### Matrix3D 两档（`ValueCodec.h:215-251`）

```
isIdentity           → 1 byte
full 4×4 float       → 1 + 64 = 65 byte
```

Matrix3D 只有 Preserve3D 的 Layer 才用，分布极度偏向 Identity，两档足够。

#### Color 4×u8 量化（`ValueCodec.h:102-116`）

```
float rgba × 4  →  u8 rgba × 4  = 4 byte
量化精度:  1/255 ≈ 0.39% per channel
```

sRGB 颜色感知精度 ~2-3% 已够，4×4=16 byte 的 float 颜色对 UI 图形完全浪费。HDR 支持路径在 spec §6.5 预留——升 FORMAT_VERSION 时切到 f32×4，不破坏 8-bit 主路径。

#### Path 当前全量（`PathCodec.cpp`）

format=0 裸 float：`u8 format + varU32 verbCount + 每 verb { u8 verbKind + 2-6 × float }`

**spec §D.2.2 format=1（动态位宽量化）设计完整但代码占位（`ChoosePathFormat` 恒返 0）**。实施触发条件是"客户提供 ≥1 MB 真实嵌入字体 .pagx"——此时压缩比 ~5×，设计不变。

---

### 2.5 决策五：资源三层去重 + EmbeddedFont 内容哈希

#### Image 三层去重（`BakeContext.h:56-58` + `ResourceBaker.cpp:8-63`）

```cpp
std::unordered_map<const void*, uint32_t>         imageIndexByNode;      // PAGX 节点指针
std::unordered_map<const tgfx::Data*, uint32_t>   imageIndexByDataPtr;   // 嵌入 Data 共享指针
std::unordered_map<std::string, uint32_t>         imageIndexByKey;       // URI / 绝对路径语义键
```

`InternAsset` 模板依次查三层：

| Tier | 命中条件 | 典型场景 |
|---|---|---|
| P1 `imageIndexByNode` | 同 PAGX 节点指针 | 同 `<Image id="logo">` 被多 `<Layer>` 引用 |
| P2 `imageIndexByDataPtr` | 不同节点共享同 tgfx::Data | 运行时构造的 PAGX 文档中,多节点持有同一 Data |
| P3 `imageIndexByKey` | URI / file path 相同 | 不同 PAGXDocument 合并时,同 URI 引用的外部文件 |

Miss 时新建 index，**逐层回填所有适用缓存**（例如 P3 命中 → 同时回填 P1/P2），让后续相同节点直接走 P1 快速通道。

#### EmbeddedFont 内容哈希去重（`BakeContext.h:69` + `TextBaker.cpp:88-146`）

```cpp
std::unordered_map<std::string, uint32_t> embeddedFontIndexByHash;

// ComputeEmbeddedFontHash:
// hash = unitsPerEm + per-glyph(advance + path verbs + path points)
```

按**内容**而不是**指针**去重——两个不同的 `pagx::Font` 节点（例如同字体子集被两个作者管线各自 emit）承载完全相同的 glyph path 时，折叠成单个 `EmbeddedFont`。

#### 为什么不做字符串池化

曾评估（Phase 19 C2）把 ShapedGlyphRun 的 typefaceFamily/Style/Key 三字段建全局 StringTable：

- 多 run 共字体场景（`rich_text.pagx` 8 run）节省 +4.6%
- **单 run 小样本反向劣化 -4% ~ -6%**（StringTable ~80 byte 固定开销 > 1 run 节省 ~34 byte，break-even = 3 run）
- 全库累积 -0.2% ~ +0.3%

结论：收益不稳定，留给未来 FormatVersion bump 搭车做。

#### Mask 两趟索引（`BakeContext.h:71-74` + `LayerBaker.cpp:91-102, 226-241`）

Mask 引用（`<Layer mask="@otherLayer">`）的特殊性：mask 目标可能在 PAGX 树中任意位置，Baker 单遍树遍历时目标可能还没 bake。两趟策略：

- Pass 1 `recordLayerPaths`：走 PAGX 树，记录 `PAGX Layer 指针 → 索引链`（e.g., `[2, 0, 1]` 表示 root.children[2].children[0].children[1]）
- Pass 2 `bakeLayerList`：解析到 mask 时查 Pass 1 map，把索引链写入 `maskLayerPath`

Inflater 端按索引链回溯查找 target Layer，mask 环由 spec §12.1 `InflateMaskCycle=607` warn 机制拦截。

---

### 2.6 决策六：Text 数据 Case A / Case B 双路径

文本是 v2 最复杂的元素——PAGX 端有 3 种数据源（作者预 shaped 的 `<GlyphRun>` / 纯 `<Text text="...">` 运行时 shape / 嵌入 `<Font>` path 资源），v2 必须无损落盘并在 Inflater 端精确复现。Phase 16（runtime shape）→ Phase 17（path-based）的架构演进后，v2 稳定在双路径架构。

#### 路径选择（`TextBaker.cpp:204-350`）

```
Case A (glyphRuns):     gate = !src.glyphRuns.empty() && anyRunHasFont
  作者预 shape + 嵌入字体 path → PAG 存 glyph IDs 和 EmbeddedFont 引用
  Inflater: PathTypefaceBuilder 回放 path,生成 TextBlob

Case B (shapedGlyphs):  gate = 上述不成立时 fallback
  Baker 跑 PAGX TextLayout 把 text 解析到 glyph IDs + positions + typeface key
  Inflater: FontProvider 查 typeface → tgfx::TextBlob::MakeFrom 回放
```

两路径**可同时出现**在同一 ElementText body（例如部分 run 有嵌入字体、部分 run 用 host font），由 boxFlags bit 7/8 分别 gate：

```cpp
// ElementTags.cpp:713-716
if (!d.glyphRuns.empty())   boxFlags |= 0x80;  // bit 7 = hasGlyphRuns
if (!d.shapedGlyphs.empty()) boxFlags |= 0x100; // bit 8 = hasShapedGlyphs
```

#### 为什么 boxFlags 扩到 u16

Phase 16.6 原本 `boxFlags` 是 `u8`（bit 0-5 已用 + bit 6 `hasShapedHint`）。Phase 17 新增两位 bit 7/8 时：

- bit 6 `hasShapedHint` 退役但**数值冻结不复用**（避免老诊断工具误读）
- `boxFlags` 扩 u8 → u16，wire 上是破坏性变更（Phase 16 文件作废）
- **扩展窗口**：v2 未发布允许此破坏，Phase 17 changelog 显式声明"旧开发分支文件需重 Bake"

#### textBoxInverseMatrix 的前向追加（Phase 18）

嵌套在 `<TextBox>` 里的 `<Text>` 有累积 Group transform。Inflater 重建 VectorGroup 时会重新套一层相同 transform，导致 Text 位移加倍。Phase 18 解决方案：

Baker 侧：
```cpp
// VectorBaker.cpp:501-523 (TextBox 识别)
if (parentIsTextBox) {
  Matrix inv;
  if (groupMatrix.invert(&inv)) {
    ctx.textBoxInverseMatrixByText[text] = inv;
  }
}

// TextBaker.cpp:356-359
auto it = ctx.textBoxInverseMatrixByText.find(&src);
data->textBoxInverseMatrix = (it != end) ? it->second : Matrix::I();
```

Wire 侧（`ElementTags.cpp:821-824`）：
```cpp
// 写入侧：无条件末尾追加
WriteMatrix(body, d.textBoxInverseMatrix);

// 读取侧：探测式读取（§6.5 字段级追加）
if (s->position() < te) {
  d->textBoxInverseMatrix = ReadMatrix(s);
}
```

Phase 17 老文件不含此字段：Reader 到达 tagEnd 自动保 `Matrix::I()` 默认值——新 Reader 读老文件降级到"无 TextBox 反向补偿"，视觉上部分 TextBox Text 会错位（已知限制），但整体文件仍可播。

---

## 3. 数据流与模块分工

### 3.1 端到端流程

```
PAGX 文件 / DOM
  ↓
[PAGXImporter::FromFile]  ← 解析 XML,构造 PAGXDocument
  ↓
[PAGXDocument::applyLayout(FontConfig)]  ← 约束解析、字体 shape、TextLayout
  ↓
[PAGExporter::ToBytes]    ← 入口 API
  ├─ Baker::Bake(pagxDoc) → PAGDocument (in-memory model)
  │   ├─ Pre-flight: hasUnresolvedImports / isLayoutApplied 门控
  │   ├─ LayerBaker    → Layer/Composition 树
  │   │   ├─ VectorBaker  → 14 种 VectorElement + TextBox 识别
  │   │   │   ├─ TextBaker       → case A / case B 双路径
  │   │   │   └─ PaintBaker      → 6 种 ColorSource + gradient stops
  │   │   └─ StyleFilterBaker → 5 Filter + 3 Style
  │   ├─ ResourceBaker → Image 三层去重 + EmbeddedFont 内容哈希去重
  │   └─ Mask Pass-1: recordLayerPaths / Pass-2: bakeLayerList
  └─ Codec::Encode(pagDoc) → byte stream
      ├─ 9 byte container header
      └─ Top-level Tag seq: FileHeader → ImageAssetTable → EmbeddedFontTable
                            → CompositionList → End
  ↓
.pag 字节流
  ↓
[PAGLoader::LoadFromFile / LoadFromBytes]
  ↓
[Codec::Decode]  → PAGDocument
  ↓
[LayerInflater::Inflate(doc, Options{fontProvider})]
  ├─ inflateComposition / inflateLayer / inflateVectorElement
  ├─ 分派到 14 种 VectorElement / 5 Filter / 3 Style
  ├─ case A: PathTypefaceBuilder + TextBlob
  ├─ case B: FontProvider.getTypeface + TextBlob
  └─ Mask Pass: 按 maskLayerPath 索引链回溯,建 Layer mask 关联
  ↓
tgfx::Layer 树（可直接交 tgfx::Surface 渲染）
```

### 3.2 目录结构

| 路径 | 内容 | 行数 |
|---|---|---|
| `src/pagx/pag/` | v2 codec + Baker/Inflater（43 个文件） | ~10,100 |
| `src/pagx/pag/PAGDocument.h` | v2 数据模型（所有 struct 权威定义） | ~750 |
| `src/pagx/pag/Baker.cpp` + `LayerBaker.cpp` + `VectorBaker.cpp` + `TextBaker.cpp` + `StyleFilterBaker.cpp` + `ResourceBaker.cpp` | Baker 6 子模块 | ~3,500 |
| `src/pagx/pag/Codec.cpp` + `CodecTags*.cpp` + `ElementTags.cpp` + `StyleFilterTags.cpp` + `PathCodec.cpp` + `PropertyEncoding.h` + `ValueCodec.h` | 编解码框架 | ~3,200 |
| `src/pagx/pag/LayerInflater.cpp` | Inflater（14×element + 5×filter + 3×style 分派） | ~1,100 |
| `src/pagx/pag/TagCode.h` + `TagHeader.cpp` + `limits.h` + `ErrorCode.h` | 元定义 | ~400 |
| `docs/pagx_to_pag_v2_design.md` | 权威字节级规格 | 6,820 |
| `test/perf/baseline.json` | 48 个 sample 的 pag 体积基准（性能回归门） | — |
| `test/src/pag/unit/ComparisonDumpTest.cpp` | PathA/PathB 并列渲染对比（273/273 ok） | ~280 |
| `test/src/pag/unit/RenderCrossCheckTest.cpp` | PSNR bit-perfect 强相等（48/48） | — |

---

## 4. Tag 注册表（已定义 42 个）

### 4.1 段位布局（`TagCode.h:3-17`）

```
0          End / 流终止符
1-9        Top-level(7 used, 2 retired)
10-19      Layer 块 + 子 Tag (5 used)
20-39      Payload (7 used)
40-119     VectorElement (14 used; 40-53)
120-139    LayerFilter (5 used)
140-159    LayerStyle (3 used)
160-239    Animation (0 used, 全预留给关键帧/插值曲线/RangeSelector v2)
900-1021   实验/第三方 (0 used, 16-byte UUID 前缀的 vendor 扩展)
1022       ErrorMarker (Baker fatal 流终止哨兵; spec 已定义,代码未连通)
1023       reserved
```

**设计意图**：每段 50% 预留。当前占用率 ~4%（42/1024），v2 长期扩展空间充裕。Phase 17 新增 EmbeddedFontTable=8 / EmbeddedFontItem=9 填满了 Top-level 段位的剩余空位；未来新增顶层资源需要评估段位扩张（例如 Phase 20 方向 2 `StringTable` 考虑占 11，`1-9 top-level` 扩到 `1-11 top-level`）。

### 4.2 TagCode 完整清单

| Code | 名称 | 作用 | Phase |
|---|---|---|---|
| 0 | End | 流终止 | v2 初始 |
| 1 | FileHeader | width/height/bgColor/frameRate/duration | v2 初始 |
| 2 | ImageAssetTable | 全部 ImageAsset 资源表 | v2 初始 |
| ~~3~~ | ~~FontAssetTable~~ | 已退役（Phase 16 v2.20） | retired |
| 4 | CompositionList | 所有 Composition 容器 | v2 初始 |
| 5 | Composition | 单个 Composition + 其 Layer 列表 | v2 初始 |
| 6 | ImageAssetItem | 单个 Image 资源项 | v2 初始 |
| ~~7~~ | ~~FontAssetItem~~ | 已退役（Phase 16 v2.20） | retired |
| 8 | EmbeddedFontTable | 嵌入字体 path 资源表 | Phase 17 v2.23 |
| 9 | EmbeddedFontItem | 单个 EmbeddedFont（unitsPerEm + glyph path × N） | Phase 17 v2.23 |
| 10 | LayerBlock | Layer 节点 + 递归 children | v2 初始 |
| 12 | LayerMaskRef | Layer 的 mask 引用链 | v2 初始 |
| 13 | LayerFilters | Layer 的 filter 列表 | v2 初始 |
| 14 | LayerStyles | Layer 的 style 列表 | v2 初始 |
| 15 | LayerTransform | visible/alpha/blendMode/matrix/matrix3D/scrollRect | v2 初始 |
| 20-26 | 7 种 Payload | Shape/Text/Image/Solid/Vector/Mesh/CompositionRef | v2 初始 |
| 40-53 | 14 种 VectorElement | Rectangle/Ellipse/Polystar/ShapePath/FillStyle/StrokeStyle/TrimPath/RoundCorner/MergePath/Repeater/Text/TextPath/TextModifier/VectorGroup | v2 初始 |
| 120-124 | 5 种 Filter | Blur/DropShadow/InnerShadow/ColorMatrix/Blend | v2 初始 |
| 140-142 | 3 种 Style | DropShadow/InnerShadow/BackgroundBlur | v2 初始 |
| 1022 | ErrorMarker | Baker fatal 流终止（当前未连通） | spec §8.3ter |

---

## 5. 压缩机制全景

### 5.1 已生效的压缩

| 机制 | 位置 | 典型节省 | 说明 |
|---|---|---|---|
| **TagHeader 10+6 位打包** | `TagHeader.cpp` | 每 Tag 头 2 byte（> 63B 时 6 byte） | 90% 小 Tag 走 2 byte 快速通道 |
| **Property isDefault 短路** | `PropertyEncoding.h:307-319` | 默认值省整个 payload（4-100 byte → 1 byte header） | 全库 Property 字段约 60% 命中默认 |
| **Matrix 三档 1/9/25** | `ValueCodec.h:162-213` | Identity 省 24 byte / 仅平移省 16 byte | LayerTransform.matrix 绝大多数 Identity |
| **Matrix3D 两档 1/65** | `ValueCodec.h:215-251` | Identity 省 64 byte | 仅 Preserve3D Layer 用 |
| **Color u8×4 量化** | `ValueCodec.h:102-116` | 16 byte float → 4 byte u8 | 精度 1/255 对 sRGB 足够 |
| **ULEB128 varU32** | `writeEncodedUint32` | 小计数 1-2 byte（vs 定长 4 byte） | 用于 count/list-length 字段 |
| **Image 三层去重** | `BakeContext.h + ResourceBaker.cpp` | 重复引用的 image 只存一份 bytes | Layer / node / URI 三种共享场景 |
| **EmbeddedFont 内容哈希去重** | `TextBaker.cpp:88-146` | 结构相同的字体子集折叠为一份 | 多管线生成的重复子集 |
| **前向兼容 unknown skip** | `Codec.cpp + PropertyEncoding.h` | 未知 Tag / encoding 占原本字节数跳过 | 扩展时老 Reader 无需改动 |

### 5.2 评估过未启动的压缩（详见 `phase20_static_compression.md`）

| 候选 | 预期全库收益 | 被否决原因 |
|---|---|---|
| Layer 静态时间字段省略 | ~3.7% | 单独收益不够触发 wire bump,待动画引入同期做 |
| Property propHeader 位流 | ~2.9% | 同上,V1 AttributeBlock 机制借鉴 |
| StringTable typeface×3 池化 | -0.2% ~ +0.3% | 小样本反向劣化（break-even 阈值不达标） |
| Path format=1 量化 | 2.4% 全库 / 50%+ EmbeddedFont | 当前 sample 无大字体,客户真实场景再启动 |
| 10 处 count 字段 u32→varU32 | 0.16% | 收益过低 |
| Gradient stopPositions u8 量化 | ~0.5% | 引入精度不一致,留给 ColorStopV2 |
| compositionIndex varU32 | ~0.1% | 收益过低 |

总结：现有 7 个已生效压缩机制把 v2 格式压到接近结构性下限，新增候选单独做均不值。未来策略是**组合实施**——下次 FORMAT_VERSION bump 或动画引入时一次性落地多项，摊薄 wire 不兼容变更的文档同步成本。

---

## 6. 扩展性承诺

### 6.1 spec §6.5 兼容性两级

| 级别 | 适用场景 | 实现方式 | Reader 行为 |
|---|---|---|---|
| **① 字段级追加** | 新字段添到 Tag body 末尾；新增 TagCode；新增 enum 值 | append-only（例：Phase 18 textBoxInverseMatrix） | 未知字节按 length skip，老字段行为不变 |
| **② FORMAT_VERSION bump** | 容器头变更 / 现有字段字节格式不兼容 / 必选 top-level 变更 | magic/version/必选 Tag 变更 | 老 Reader 见新版本号 graceful reject |

v2 当前仍在**发布前破坏窗口期**——任何 schema 变更都可保持 FORMAT_VERSION=0x02 并要求旧分支文件重 Bake（Phase 16/17/18 均使用此特权）。发布后必须严守 §6.5 规则。

### 6.2 预留扩展槽位

- **TagCode 段位 160-239**（80 个槽位）：关键帧 / 插值曲线 / RangeSelector v2 / MotionPath
- **Property encoding 1-15**（15 个槽位）：Hold / Linear / Bezier / Spatial
- **layerFlags bit 5-7 reserved**：未来 Layer 新 bool 开关
- **propHeader bit 5-7 reserved**：动画相关标志位
- **boxFlags bit 9-15 reserved**：Text 新字段 gate

### 6.3 ABI 红线

- `DiagnosticCode` 数值段内追加，**永不跨段迁移**，**永不复用退役数值**（`TextGlyphDataEmpty=206` / `FontAssetTable=3` / `FontAssetItem=7` 均冻结）
- 必选 top-level Tag（FileHeader / ImageAssetTable / CompositionList / Composition / LayerBlock）五者缺失 = 文件不可播，**字段不兼容变更必须升 FORMAT_VERSION**
- Property<T> `defaultValue` 必须在 `MakeProp(X)` initializer 中集中定义，不允许在 Codec / Baker / Inflater 中重复 hardcode（§C.2）

---

## 7. 限制与已知盲区

### 7.1 已知不支持

- **动画**：关键帧 / 插值曲线 / MotionPath / Expression 全部预留未启动（Tag 段位 160-239 / Property encoding 1-15）
- **HDR / Wide-gamut 颜色**：当前 Color 固定 u8×4 sRGB，升级需升 FORMAT_VERSION（§6.5 ②）
- **SVG 导入**：PAGX 的 `<svg>` / `import="*.svg"` 必须经 `pagx resolve` CLI 转成 PAGX 原生节点，否则 `Baker.cpp:28-32` 触发 `UnresolvedImports=101`

### 7.2 运行时限制（`src/pagx/pag/limits.h`）

```
MAX_TOTAL_BODY_BYTES       = 1 GB
MAX_IMAGE_BYTES            = 100 MB / image
MAX_FONT_BYTES             = 50 MB / font
MAX_IMAGES                 = 10,000
MAX_FONTS                  = 1,000
MAX_LAYER_DEPTH            = 64
MAX_LAYERS_PER_COMPOSITION = 10,000
MAX_COMPOSITIONS           = 1,000
MAX_CANVAS_DIM             = 16,384
MAX_PATH_VERBS             = 100,000
MAX_GLYPHS_PER_RUN         = 100,000
MAX_GRADIENT_STOPS         = 256
MAX_FILTERS_PER_LAYER      = 64
MAX_STYLES_PER_LAYER       = 64
MAX_VECTOR_ELEMENTS_PER_PAYLOAD = 100,000
MAX_VECTOR_ELEMENT_DEPTH   = 64
MAX_COMPOSITION_REF_DEPTH  = 32
MAX_INFLATED_LAYER_COUNT   = 1,000,000 (Inflater 累计预算)
```

超限：Baker 侧 `StructureLimitExceeded=105` fatal，Inflater 侧 `InflaterLayerBudgetExceeded=606` warn + 降级。

### 7.3 测试覆盖盲区

- **`test/perf/baseline.json`** 仅 48 个 sample / 共 230 KB，不覆盖大字体 / 大图 / 深度嵌套 / 长动画等生产典型场景
- **`ComparisonDumpTest`** 渲染 273 sample（排除 6 个 `pagx resolve` CLI fixture），但只比对**可渲染正确性**，不比对字节结构
- **Fuzzing**（`cmake-build-fuzz`）覆盖 Codec 解码入口的恶意输入，不覆盖 Baker 生成链

---

## 8. 演进路线图（建议）

### 8.1 近期（无外部依赖）

- **动画基础能力**：Property encoding 1-3（Hold/Linear/Bezier）落地 → TagCode 160-169（关键帧容器）→ Inflater 时间轴播放
- **文档对外发布**：明确"v2 锁定"时间点，之后启用 §6.5 兼容性规则（不再允许不升 FORMAT_VERSION 的 schema 破坏）

### 8.2 中期（性能/体积客户提出时）

- **Phase 20 组合压缩**（`phase20_static_compression.md`）：Layer 静态时间 + Property propHeader 位流 ≈ 6.6%，搭车动画引入或下次 FORMAT_VERSION bump
- **Path format=1 量化**（spec §D.2.2）：客户提供 ≥1 MB 真实嵌入字体 .pagx 时启动，预期 50%+ 压缩

### 8.3 远期（生态扩展）

- **第三方 Tag 区 900-1021**：16-byte UUID 前缀的 vendor 扩展（spec §6.1 已定义，代码未连通）
- **顶层压缩**：容器头 compression byte 启用 zlib/lz4（当前固定 0x00，字段预留）
- **HDR 颜色路径**：升 FORMAT_VERSION 到 0x03，Color 切 f32×4

---

## 附录 A：关键文件引用表

| 文件 | 关键函数 | 行号 |
|---|---|---|
| `Baker.cpp` | `Bake()` | 38 |
| `LayerBaker.cpp` | `BakeAllLayers() / bakeLayer()` | 265, 177 |
| `VectorBaker.cpp` | `BakeVectorPayload() / BakeElement()` | 545, 100 |
| `TextBaker.cpp` | `BakeText()` (case A 204 / case B 247) / `ComputeEmbeddedFontHash()` | 181, 88 |
| `StyleFilterBaker.cpp` | `BakeLayerFilters() / BakeLayerStyles()` | 130, 151 |
| `ResourceBaker.cpp` | `RegisterImage() / InternAsset()` | 67, 8 |
| `Codec.cpp` | `Encode() / Decode()` | 38, 76 |
| `LayerInflater.cpp` | `Inflate() / inflateLayer()` | 53(header), 126 |
| `PropertyEncoding.h` | `WriteProperty<T>() / ReadProperty<T>()` | 307, 322 |
| `ValueCodec.h` | `WriteMatrix() / WriteColor()` | 162, 102 |
| `PathCodec.cpp` | `WritePath() / ReadPath()` | 62, 134 |
| `TagHeader.cpp` | `ReadTagHeader() / WriteTagHeader()` | 6, 19 |
| `ElementTags.cpp` | `WriteElementTextBody() / ReadElementTextBody()` | 683, 827 |

## 附录 B：术语表

| 术语 | 定义 |
|---|---|
| **PAGX** | 作者层可编辑 XML 格式 |
| **PAG v2** | PAGX 的二进制产物 |
| **Baker** | PAGX → PAGDocument（in-memory model） |
| **Codec** | PAGDocument ↔ byte stream |
| **Inflater** | PAGDocument → tgfx::Layer 树 |
| **Property\<T\>** | 带 encoding 标识的字段容器，未来支持关键帧 |
| **propHeader** | 1 byte 元数据（encoding + isDefault + reserved） |
| **TagHeader** | 2-6 byte（code:10 + length:6 + 可选 extLen:32） |
| **case A (glyphRuns)** | 作者预 shape glyph IDs + 嵌入字体 path |
| **case B (shapedGlyphs)** | Baker 运行时 shape + FontProvider 回放 |
| **InternAsset** | 资源去重 helper（三层 Image / 哈希 EmbeddedFont） |

## 附录 C：相关文档

- `docs/pagx_to_pag_v2_design.md` — 字节级权威规格（6820 行）
- `docs/pagx_to_pag_v2_changelog.md` — 全部 schema 演进历史
- `docs/phase20_static_compression.md` — 已评估未启动的压缩优化设计存档
- `docs/development_plan.md` — 长期开发路线图

---

**版本**：2026-05-11 撰写
**提交点**：`feature/henryjpxie_pagx_pag` HEAD（含 Phase 17/18 全部功能 + ComparisonDumpTest 273/273 ok）
