# PAGX → PAG v2 转换技术方案设计文档

**版本**：2.11
**日期**：2026-04-29
**状态**：待评审 / 待开工

---

## 目录

1. 背景与目标
2. 核心设计原则
3. 总体架构
4. PAG v2 文件格式设计
5. PAGDocument 数据模型
6. Tag 表设计
7. Baker 子模块与映射规则
8. Codec 设计（Encode / Decode）
9. LayerInflater 设计（PAGDocument → tgfx::Layer）
10. 字体双模式与文本处理
11. 资源管理（图片 / 字体）
12. Mask 两趟处理
13. 动画可扩展性保留
14. CLI 集成（pagx export --format pag）
15. 对外 API
16. 目录结构与代码组织
17. 验收标准
18. 测试方案
19. 工作拆分与 TDD 执行顺序
20. 附录 A：Baker / Inflater / LayerBuilder 行号映射索引
21. 附录 B：开工前待确认事项
22. 附录 C：PAGDocument 完整 C++ 定义
23. 附录 D：Tag 字节布局规范
24. 附录 E：枚举映射表（PAGX ↔ PAGDocument ↔ tgfx）
25. 附录 F：字段名对照表（LayerBuilder ↔ PAGX DOM ↔ PAGDocument ↔ tgfx）
26. 附录 G：错误码与诊断
27. 附录 H：安全上限与资源约束
28. 附录 I：依赖与 include 规范

---

## 1. 背景与目标

### 1.1 背景

PAGX 是基于 XML 的纯文本矢量描述格式，为 AI 矢量生成而设计。在矢量图形比较复杂、数量较多、Path 数据较多的场景下，PAGX 文件体积可能超过 10MB，实测加载解析耗时较长。

PAG v1 是现有的渲染二进制格式，服务于 PAGX 之前的 AE 动画工作流；其 Tag 体系与 PAGX 的节点体系差异较大，不能直接承载 PAGX 的数据语义。

### 1.2 目标

- 新增 PAGX → PAG 的二进制转换能力，输出一个**新版本（v2）** 的 PAG 二进制文件。
- v2 文件**只包含渲染所需数据**，不保留编辑时元数据，加载解析性能显著优于 PAGX。
- **本期仅实现静态矢量场景**，但数据模型与字节布局必须为未来动画扩展**预留无破坏性的升级路径**。
- 渲染正确性判据：**源 PAGX 渲染像素 与 转出 PAG 渲染像素一致**，使用项目现有的 `Baseline::Compare` 做对比（而非严格 memcmp；原因见 §18.2）。

### 1.3 非目标

- PAG → PAGX 反向转换。
- 与 PAG v1 的前向/后向兼容（v1 播放器对 v2 文件 graceful reject，不支持播放）。
- 本期不实现动画逻辑（关键帧插值、TextModifier 求值、播放器驱动），仅保留扩展点。

---

## 2. 核心设计原则

1. **渲染保真，而非信息保真**：PAGX 中用于 AI 编辑的元数据（自动布局、约束、导入指令、资源引用、富文本编辑态结构）全部在转换时**预解算并丢弃**。lossless 的含义是"渲染像素一致"，不是"字段一致"。
2. **PAG v2 是 `tgfx::Layer` 树的序列化形式**：v2 的数据模型直接对应 tgfx 的 Layer / VectorElement / ColorSource / Filter / Style / Mask，解码端反序列化后可直接构造 tgfx::Layer 树交给 `tgfx::DisplayList` 渲染，无中间运行时模型。
3. **单一映射规则源头**：`src/renderer/LayerBuilder.cpp` 已实现 `PAGXDocument → tgfx::Layer`。Baker 和 LayerInflater 各自以它为蓝本保持同构——**三者对 tgfx::Layer 的 setter 序列完全一致**。
4. **版本号 +1，v1 播放器拒绝加载**：二进制头部 version byte 从 `0x01` 升到 `0x02`；v1 播放器检测到 `0x02` 必须 graceful reject。
5. **Tag 体系重新设计**：不复用 v1 的 TagCode；v2 在独立命名空间 `pagx::pag` 下定义全新 Tag 表。
6. **保留几何语义**：Rectangle/Ellipse/Polystar 不归一化为 Path，对齐 tgfx VectorElement 原生能力。
7. **字体双模式**：默认走字体渲染（System / Embedded FontAsset），可选 OutlineAll 模式把所有 glyph 烘焙为矢量路径。与 PAGX 现有设计对齐。
8. **动画扩展性保留**：所有可动画字段以 `Property<T>` 包装；本期只实现 Constant 编码分支，非 Constant 分支占位为 TODO。字节布局稳定：动画接入不升版本号、不新增字节布局、不新增 Tag。

---

## 3. 总体架构

### 3.1 数据流

```
[编码]
  *.pagx (XML)
    │
    ▼ PAGXImporter::FromFile                     （已有）
  PAGXDocument
    │
    ▼ doc->applyLayout()                         （已有）
  PAGXDocument (layout applied)
    │
    ▼ pagx::pag::Baker::Bake                     ◀── 新增
  PAGDocument
    │
    ▼ pagx::pag::Codec::Encode                   ◀── 新增
  *.pag (version = 0x02)

[解码]
  *.pag (version = 0x02)
    │
    ▼ pagx::pag::Codec::Decode                   ◀── 新增
  PAGDocument
    │
    ▼ pagx::pag::LayerInflater::Inflate          ◀── 新增
  LayerInflater::Result { layer, warnings }
    │
    ▼ tgfx::DisplayList                          （已有）
  屏幕渲染
```

### 3.2 四态三映射

- 四态：`PAGXDocument` → `PAGDocument` → `bytes` → `PAGDocument` → `tgfx::Layer`
- 三映射：`Baker`（PAGXDocument→PAGDocument） / `Codec`（PAGDocument↔bytes） / `LayerInflater`（PAGDocument→tgfx::Layer）
- Baker 与 LayerInflater **以 `LayerBuilder.cpp` 为共同蓝本**，保证编解码对称与渲染一致。

---

## 4. PAG v2 文件格式设计

### 4.1 容器头

```
Offset  Size  Field            Value
  0      3    magic            'P', 'A', 'G'
  3      1    version          0x02           ← v1 播放器检测到非 0x01 graceful reject
  4      4    bodyLength       uint32         ← little endian
  8      1    compression      0x00           ← UNCOMPRESSED 唯一合法值（本期）
  9      N    body             Tag 序列 + End(0) 终结
```

**字节序约定**：所有 multi-byte 数值一律 **little endian**（对齐 v1 的 `DecodeStream`，见 `src/codec/utils/DecodeStream.h:28-49` 的类级注释 `The byte order of DecodeStream is always little-endian`）。

**版本常量**：`constexpr uint8_t pagx::pag::FORMAT_VERSION = 0x02;` 定义在 `src/pagx/pag/TagCode.h`，全代码仓库仅此一处硬编码，其他位置必须引用该常量。

**v1 播放器改动**：`src/codec/Codec.cpp` 的 `ReadBodyBytes` 在 line 191 读 version，line 196 做上界检查 `if (version > KnownVersion)`。v1 播放器无需改动代码，`KnownVersion` 保持为 `0x01`；`0x02` 版本会自动命中"Invalid PAG file header"分支并 graceful reject。本期**不修改 v1 播放器**（文档 §19 阶段 13 的任务由此取消）。

### 4.2 Tag 封装（复用 v1 TagHeader 字节格式）

```
TagHeader：
  u16 codeAndLength    # 高 10 bit = code（≤ 1023），低 6 bit = length
  [u32 extendedLength] # 仅当低 6 bit == 63 时出现；此时真实 length 为 extendedLength
TagBody { ... tag-specific fields ... }
```

详细字节规则和 v2 独立实现代码见**附录 D.3**（与 `src/codec/TagHeader.cpp:23-48` 字节格式完全一致，但使用 v2 自己的 `pagx::pag::TagCode` 枚举）。

- 每个 Tag 体积边界由 `length` 明确，**未知 Tag / 未知字段可被 skip**——这是向前兼容的硬前提；
- body 最末一个 Tag 约定为 `TagCode::End = 0`，解码器读到 End 即终止。

### 4.3 Property<T> 编码

每个可动画字段在字节流中的布局：

```
[1 byte]  propHeader (位域，见下)
若 isDefault == 1:
    (无 value 字节)      # value 取编译期 defaultValue；本期静态默认值占比 ~60%
否则若 encoding == Constant:
    [T] value            # T 的字节布局见附录 D.1 / ValueCodec
否则（encoding != Constant，本期不产出；Decoder 遇到按 §4.4 策略处理）:
    ...                  # future keyframe sequence
```

**propHeader 位域（u8）**：

| bit | 字段 | 说明 |
|---|---|---|
| bit 0-3 | `encoding` (4 bit) | `0=Constant`, `1=Hold`, `2=Linear`, `3=Bezier`, `4=Spatial`, `5-15 reserved` |
| bit 4   | `isDefault` | 1 = value 等于编译期 defaultValue，**跳过 value 字节**；0 = value 正常序列化 |
| bit 5   | `hasExt`    | 1 = 后续有 1 byte extension 头（`extHeader`）承载新能力位；本期固定写 0 |
| bit 6-7 | reserved    | 本期固定 0，Reader 必须忽略（向前兼容） |

**本期约束**：所有 Property 的 `encoding` 位段 = 0（Constant），`hasExt` = 0，`isDefault` 由 Baker 按字段值与默认值的相等性决定。

**默认值来源（defaultValue 的权威声明）**：每个 Property 字段的 `defaultValue` **必须**在其包裹结构体（附录 C.5–C.9）的 `= MakeProp(X)` 初始化器里显式写出——`X` 即该字段的 defaultValue。Codec 的 Read/Write 实现**必须**使用同一套常量，不得各自硬编码。为此 v2 在 `src/pagx/pag/ValueCodec.h` 提供：

```cpp
template <typename T>
inline void WriteProperty(pag::EncodeStream* stream, const Property<T>& prop, const T& defaultValue);

template <typename T>
inline Property<T> ReadProperty(pag::DecodeStream* stream, DecodeContext* ctx,
                                const T& defaultValue, uint32_t enclosingTagEnd);
```

Baker / Codec 调用约定：
```cpp
// 写入：Baker 用字段结构体里的初始化器来源（避免硬编码）
WriteProperty(stream, data.alpha, /*defaultValue=*/ 1.0f);

// 读取：Reader 必须传入同一常量；isDefault=1 时直接返回 MakeProp(defaultValue)
data.alpha = ReadProperty(stream, ctx, /*defaultValue=*/ 1.0f, tagEnd);
```

**内存结构稳定性**：本期 `Property<T>` **不含** `reservedKeyframeBlob` 字段（见附录 C.2），避免为百万级 VectorElement 场景引入数百 MB 无用内存开销。动画期接入时再扩 `Property<T>` 的内存结构；字节流布局**不受影响**（字节流本就不写 blob，`hasExt`/`encoding` 新值才是未来入口）。

### 4.4 Decoder 遇到未知 propHeader 的兜底

三条独立规则：

1. **`encoding` 位段非 0（本期未知动画编码）**：立即放弃当前最外层 Tag 的剩余所有字段：`stream->setPosition(enclosingTagEnd)`，Tag 内其他未读字段保持 defaultValue，推 warning `UnknownPropertyEncoding`（400），继续读下一 Tag。
2. **`hasExt` = 1（未来扩展能力）**：本期 Reader 先 `readUint8()` 消费 1 byte extHeader 并丢弃（保持 stream 位置正确），然后按 `encoding` 位段的值继续处理。若同时 `encoding` 未知，优先走规则 1。
3. **bit 6-7 非 0**：忽略（这两位是最终逃生门，Reader 不得因此报错）。

**绝不尝试**"跳过若干字节继续读"——非 Constant encoding 的字节长度由未来版本决定，本期解码器不可能推断。规则 1 的兜底保证：
- 旧 Decoder 读新文件永远不会字段错位；
- 旧 Decoder 至多损失一个 Tag 的内容（视觉降级），不崩溃、不发散。

---

## 5. PAGDocument 数据模型

> **权威定义**：所有 PAGDocument 结构体、枚举、默认值的 C++ 完整定义在**附录 C**。本章只做概念描述，不再出现代码片段——AI 编码期间**按附录 C 落代码**，不看本章的伪代码推断。

### 5.1 顶层组成

- **`PAGDocument`**：全文档的根。包含：
  - `FileHeader`：画布尺寸、背景色、时间轴（frameRate/duration，本期静态均为默认值）；
  - `compositions`：composition 列表，约定 `compositions[0]` 为 root composition；
  - `images` / `fonts`：资源池，由索引在 payload 中引用。
- **`Composition`**：一棵图层树的容器，带 id、width/height（root composition 的 w/h 承载 canvas 尺寸）、`layers[]`。
- **`Layer`**：图层节点，按 `LayerType` 派发到 7 种类型之一（本期实际产出 3 种：`Layer` 容器 / `Vector` / `CompositionRef`）。

### 5.2 Property<T> 抽象

所有**可动画字段**以 `Property<T>` 包装，承载 `encoding` 标记位 + 常量 `value`（本期 encoding 恒为 Constant）。**Baker 赋值统一走 `MakeProp(v)` 辅助**，不得手写三参数字面量——这是为动画扩展留的唯一修改点（见 §13）。

### 5.3 Layer 字段分类

- **身份 / 时间轴**：type、name、startTime、duration、stretch——静态场景均为默认值。
- **可动画变换**：visible / alpha / blendMode / matrix / matrix3D / scrollRect（全部 `Property<T>`）。
- **非动画开关**：preserve3D / passThroughBackground / allowsEdgeAntialiasing / allowsGroupOpacity / hasScrollRect。
- **Mask 引用**：`maskLayerPath`（同 composition 内 child index 链）+ `maskType`。两趟策略见 §12。
- **Effects**：`filters[]` / `styles[]`（顺序敏感）。
- **子层**：`children[]`（递归）。
- **类型专属 payload**：按 `type` 激活 1 个 payload（其余为 nullptr）。

> **Sizing 语义**（2026-04-25 主干更新后）：PAGX 的 `Layer/Group` **不再**直接持有 `width/height`——这两个字段连同新增的 `percentWidth/percentHeight` 统一迁移到基类 `LayoutNode`（`include/pagx/nodes/LayoutNode.h:78-100`）。优先级排序：**`percentWidth/Height`（相对父容器尺寸）> `width/Height`（绝对像素）> `preferredWidth/Height`（内容测量结果）**。百分比尺寸由父容器在 `PerformConstraintLayout` 阶段解算——**Baker 只读取已解算后的绝对值**，不处理百分比表达式本身（LayerBuilder 也走这个契约，详见 LayerBuilder.cpp 的 `applyLayerAttributes`）。因此 PAGDocument 字节流里**不承载** `percentWidth/Height`——它们是编辑时模型，不是渲染时模型。

### 5.4 Payload 总览（各 type 的详情见附录 C.8）

| LayerType | Payload | 本期是否产出 |
|---|---|---|
| Layer | 无 | 产出（容器型） |
| Shape | ShapePayload | **不产出**（PAGX 无对应入口） |
| Text | TextPayload | **不产出**（富文本全走 VectorLayer + ElementText） |
| Image | ImagePayload | **不产出**（PAGX 图像走 VectorLayer 的 ImagePattern） |
| Solid | SolidPayload | **不产出** |
| Vector | VectorPayload | 产出（主力路径） |
| Mesh | MeshPayload | **不产出**（PAGX 暂无 Mesh） |
| CompositionRef | `compositionIndex` (无独立 payload 结构) | 产出 |

### 5.5 VectorElement 14 种 + ColorSource 6 种

详见附录 C.7 和 C.6。种类列表：
- **Geometry**：Rectangle、Ellipse、Polystar、ShapePath
- **Paint**：FillStyle、StrokeStyle（内嵌 ShapeStyleData）
- **Path modifier**：TrimPath、RoundCorner、MergePath、Repeater
- **Text**：Text、TextPath、TextModifier
- **Container**：VectorGroup

ColorSource 类型：SolidColor / LinearGradient / RadialGradient / ConicGradient / DiamondGradient / ImagePattern。

**Gradient 基类**（2026-04-25 主干更新后）：4 种 gradient 在 PAGX 侧统一继承新基类 `pagx::Gradient`（`include/pagx/nodes/Gradient.h`），共享字段 `matrix / colorStops / fitsToGeometry=true`。PAGDocument `ShapeStyleData` 在字节流层**把这些共享字段铺平**（不引入显式基类 payload），`gradientMatrix / stopColors / stopPositions` 始终序列化，`fitsToGeometry` 作为独立 bool 写入（见附录 F）。

**fitsToGeometry 语义**：`true`（默认）= gradient 坐标系是 `(0,0)-(1,1)` 规范化空间，**逐 geometry 按包围盒自动拟合**；`false` = gradient 坐标系是**父容器**（Layer/Group）坐标空间，同容器内多个 geometry **共享一份连续填充**。Baker 必须把 PAGX 的该 bool 值透传到 PAGDocument。

**ImagePattern ScaleMode**（2026-04-25 新增）：ImagePattern 新增 `ScaleMode scaleMode = LetterBox`（默认），4 个值：`None / Stretch / LetterBox / Zoom`（详见附录 E.9）。`tileModeX/Y` 默认值从 v2 旧版的 `Clamp` 改为 `Decal`（与 tgfx relative-fill 默认对齐）。

### 5.6 Filter（5）与 Style（3）

- **Filter**：Blur、DropShadow、InnerShadow、ColorMatrix、Blend
- **Style**：DropShadow、InnerShadow、BackgroundBlur

所有 filter/style 公共字段：`blendMode`（style 专用——见 §C.9 LayerStyle）、`excludeChildEffects`（style 专用）。Filter 若有混合需求（如 FilterBlend），由各自子类字段（如 `blendFilterMode`）承载。子类字段按 tgfx 对应类 1:1 映射，详情附录 C.9。

---

## 6. Tag 表设计

> **权威定义**：所有 TagCode 的枚举值、字节布局、Read/Write 函数签名在**附录 D**。本章只做概念描述。

### 6.1 TagCode 分段

**设计原则**：每段预留 ~50% 扩展余量；新增同类 Tag 就地追加枚举值；跨段语义变更用"版本化 TagCode 变体"（见 §6.5）。

| 段 | 用途 | 编号范围 | 当前使用 | 预留 |
|---|---|---|---|---|
| 终结 | End tag | 0 | 1 | — |
| 顶层 | FileHeader / Asset table / Composition list / Composition | 1-9 | 5 (1,2,3,4,5) | 4 个（6-9）|
| Layer 及子 Tag | LayerBlock / LayerMaskRef / LayerFilters / LayerStyles / 保留 | 10-19 | 4 (10,12,13,14) | 6 个 |
| Payload | Shape / Text / Image / Solid / Vector / Mesh / CompositionRef | 20-39 | 7 (20-26) | 13 个 |
| VectorElement | 14 种 element + 未来扩展（3D shape / Motion Path 等） | 40-79 | 14 (40-53) | 26 个 |
| LayerFilter | 5 种 filter + 扩展（ChromaKey / Glow 等） | 80-99 | 5 (80-84)（**旧值 70-74 已迁移**） | 15 个 |
| LayerStyle | 3 种 style + 扩展 | 100-119 | 3 (100-102)（**旧值 80-82 已迁移**） | 17 个 |
| 动画专用 | 关键帧 / 插值曲线 / RangeSelector v2 等 | 120-199 | 0 | 80 个 |
| 资源扩展 | 新 asset 类型（SVG 片段 / Lottie / HDR Color 等） | 200-299 | 0 | 100 个 |
| 版本化变体 | 同语义不兼容升级（如 `FileHeaderV2`、`ShapeStyleHDR`） | 300-599 | 0 | 300 个 |
| 实验性 | 未入主干的实验功能、第三方扩展 | 900-1022 | 0 | 123 个 |
| 保留 | 绝不分配（对齐 v1 `file.h:48` 10-bit 上限约束） | 1023 | — | — |

> **v1.3 → v2.0 TagCode 迁移**：Filter 段从 70-74 迁至 80-99，Style 段从 80-82 迁至 100-119。原因：为"Payload 段"扩展到 20-39 腾出空间。v2 尚未发布，迁移无兼容性代价。附录 D.11/D.12 中所有 Filter/Style TagCode 值已同步更新。

### 6.2 body 顶层写入顺序

```
FileHeader → ImageAssetTable → FontAssetTable → CompositionList → End
```

### 6.3 嵌套结构

- `CompositionList` 内嵌 N 个 `Composition` Tag；
- 每个 `Composition` 内嵌 `LayerBlock` 序列；
- 每个 `LayerBlock` 内嵌可选子 Tag（Transform3D/MaskRef/Filters/Styles）、按 type 选择的 Payload Tag、子 Layer 列表；
- `VectorPayload` 内嵌 14 种 `Element<Type>` Tag；
- `LayerFilters` / `LayerStyles` 分别内嵌 `Filter<Name>` / `Style<Name>` Tag。

### 6.4 向前兼容性

每个 Tag 的边界由 `TagHeader.length` 明确界定——新版本**追加字段到 Tag 尾部**、**新增 TagCode** 都**不需要升版本号**；旧 Decoder 遇到未知字段或未知 Tag 一律按 length skip（见附录 D.3 和附录 G.UnknownTagCode）。

### 6.5 兼容性三级保障

v2 显式定义三级兼容机制，**由小到大依次使用**，避免仓促升 `FORMAT_VERSION`：

**① 字段级追加**（日常首选）：
- 向已有 Tag 的 body 末尾追加字段；
- 旧 Decoder 按 `TagHeader.length` seek 到 tagEnd 跳过新字段；
- 适用：新增 bit flag、新增可选 Property、新增 enum 值（通过 `EnumMeta<T>` 的 reserved range）；
- **示例**：`layerFlags` 预留 bit 5-7 接纳 3 个新开关而不破坏字节流。

**② Tag 级版本化**（字段语义不兼容变更时）：
- 为不兼容字段变更分配**新 TagCode**，命名 `<Name>V2` / `V3`；
- 保留旧 TagCode 的语义与字节格式**不变**；
- Writer 优先写新版本；Reader 同时接受新旧版本（`Read<Name>V2` 调用成功则走新路径，否则回退老路径）；
- 旧 Reader 忽略新 Tag（走 `UnknownTagCode` warning + length skip），至多丢失该 Tag 的信息，不崩溃；
- **v1 对照**：参考 v1 `ImageFillRule` (54) / `ImageFillRuleV2` (67) / `LayerAttributes` (6) / `LayerAttributesV2` (52) / `LayerAttributesV3` (62)（`include/pag/file.h:51-152`）；
- **v2 示例**：未来 HDR 支持需 Color 升为 f32×4，则分配 `TagCode::FileHeaderHDR`、`TagCode::ShapeStyleHDR` 等；旧 FileHeader / ShapeStyle 保持 u8×4 RGBA 不变；
- **TagCode 分配规则**：版本化变体统一占用 §6.1 的"版本化变体 300-599"段；命名后缀 `V<N>` 或语义后缀（`HDR` / `Animated`）。

**③ 文件级版本号升级**（核选项，极端情况）：
- 升 `FORMAT_VERSION`（目前 0x02）当且仅当**容器头或 Tag 封装机制本身**不兼容变更（如 magic 改变、TagHeader 位宽变化、新增必选 top-level 段落）；
- 本期**动画扩展明确不升版本号**（靠 Property 壳的 `encoding` 位段 + Tag 级版本化即可）；
- 旧播放器检测到新版本号 → graceful reject（不尝试解析，避免错字节风险）。

**优先级判定流程**（给未来版本设计者）：
```
新需求
 ├─ 只加字段？ → 用 ①
 ├─ 字段语义或字节格式不兼容变更？ → 用 ②
 └─ 容器/Tag 封装本身变更？ → 用 ③（需 RFC 级别评审）
```

**测试要求**：
- 每次用 ② 新增版本化 Tag 时，`RoundTripTest` 必须新增一条"新 Writer → 旧 Reader（graceful skip）→ 字段保持默认"用例；
- 每次用 ① 追加字段时，`RoundTripTest` 必须新增"旧 Writer（缺该字段）→ 新 Reader（拿默认值）"用例。

---

## 7. Baker 子模块与映射规则

详见**附录 A** 的行号映射索引。此处列模块与职责：

| Baker 子模块 | 文件 | 职责 | 对齐 LayerBuilder 行号段 |
|---|---|---|---|
| 顶层入口 | `Baker.cpp` | 构造 PAGDocument / FileHeader / 编排两趟 | 137-163 |
| **LayerBaker** | `layer_bakers/LayerBaker.cpp` | Layer 通用字段 + 类型分发 + 递归 children | 166-202, 731-796 |
| **CompositionBaker** | 同上文件 | Composition 去重、CompositionRef 产出 | 204-216 |
| **VectorBaker** | `layer_bakers/VectorBaker.cpp` | VectorLayer 内容 + 14 种 VectorElement | 219-231, 262-307, 309-344, 360-368, 573-616, 618-630, 681-729 |
| **PaintBaker** | 同上文件 | 6 种 ColorSource + gradient stops 兜底 | 448-571 |
| **TextBaker** | `layer_bakers/TextBaker.cpp` | Text/TextBox/TextPath + 双字体模式 | 236-260, 370-385, 584-602, 296-303 |
| **StyleFilterBaker** | `layer_bakers/StyleFilterBaker.cpp` | 3 style + 5 filter | 798-870 |
| **ResourceBaker** | `layer_bakers/ResourceBaker.cpp` | 图片/字体去重 + 索引化（两趟 pre-pass） | 543-571 |

### 7.1 共同约束

- 所有对 PAGDocument 字段的赋值**按 LayerBuilder 的 setter 顺序排布**，便于和 LayerInflater 同步修改。
- 所有可动画字段赋值走 `BakeProperty<T>(staticValue)` 统一入口，未来动画引入只改这一处。
- Mask 采用两趟策略（见 §12）。

### 7.2 两趟 pre-pass 设计

1. **资源 pre-pass**：深度遍历 `doc.layers`，把 Image / Font 节点索引化到 `BakeContext`；保证 VectorBaker 单趟可直接查 index。
2. **LayerPath pre-pass**：同趟记录每个 PAGX Layer\* 的 `layerPath`（composition 内从 root 到该 layer 的 child index 链），供 mask 解析使用。
3. **Main pass**：递归 Baker 子模块产出 PAGDocument 子树。

---

## 8. Codec 设计

### 8.1 对外（内部头）接口

```cpp
#include "pagx/Diagnostic.h"         // 对外 Diagnostic 结构 + DiagnosticCode 枚举
#include "pagx/pag/ErrorCode.h"      // 内部 alias: using ErrorCode = pagx::DiagnosticCode;

namespace pagx::pag {

// ErrorCode / Diagnostic 的权威定义在 include/pagx/Diagnostic.h（见 §15.1）。
// 内部通过 ErrorCode.h 的 using alias 访问 pagx::DiagnosticCode；
// Diagnostic 类型也通过同一 alias 头访问 pagx::Diagnostic——无需在此重复定义。

struct EncodeResult {
  std::unique_ptr<ByteData> bytes;           // 成功时非空；失败为 nullptr
  std::vector<Diagnostic> warnings;          // 成功但降级告警
};

struct DecodeResult {
  std::unique_ptr<PAGDocument> doc;          // 成功时非空；errors 非空时必为 nullptr
  std::vector<Diagnostic> errors;            // 致命错误
  std::vector<Diagnostic> warnings;          // 非致命告警
};

// Baker 内部使用的产物（PAGExporter 内部拼装 Encode 链时用），形态与 DecodeResult 对称
struct BakeResult {
  std::unique_ptr<PAGDocument> doc;          // 成功时非空；errors 非空时必为 nullptr
  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
};

class Codec {
 public:
  static EncodeResult Encode(const PAGDocument& doc);
  static DecodeResult Decode(const uint8_t* data, size_t length);
};

}
```

**Encode/Decode 结果契约**（见附录 G）：
- `EncodeResult.bytes == nullptr` ⟺ Encode 失败（当前仅在输入 doc 结构严重不合法时触发，如 `compositions` 为空但被引用）；
- `DecodeResult.doc == nullptr` ⟺ `errors` 非空；反之 doc 非空时 errors 必为空（可能含 warnings）；
- `EncodeResult` 不含 `ok` 字段（Encode 路径只产生 warnings，不产生 errors）；外部 `PAGExporter::Result.ok` 由 `errors.empty()` 判定（见 §15.2）——对外 Result 的 `errors` 仅来源于 Baker 的 fatal 错误（§15.2 `ToBytes` / `ToFile` 内部拼装 Baker→Encoder 链时填充）；
- 所有 ByteData 生命周期归调用方，Codec 本身无静态缓存。

### 8.2 Encode 流程

```
构造 EncodeStream body；
WriteTag(body, FileHeader, doc.header);
WriteTag(body, ImageAssetTable, doc.images);
WriteTag(body, FontAssetTable, doc.fonts);
WriteTag(body, CompositionList, doc.compositions);
WriteEndTag(body);

构造 EncodeStream file；
file.write('P','A','G', 0x02);
file.writeUint32(body.length());
file.writeUint8(0x00);  // UNCOMPRESSED
file.writeBytes(body);
```

### 8.3 Decode 流程

1. 校验 magic `'P','A','G'` 与 version `0x02`，不匹配返回 error；
2. 读 `bodyLength`、`compression`（只接受 0x00）；
3. 构造 DecodeStream 覆盖 body 区间，循环读 Tag：
   ```
   while (true) {
     TagHeader h = ReadTagHeader(stream);
     if (h.code == End) break;
     SubStream sub = stream.slice(h.length);
     DispatchTagReader(h.code, sub, ctx);
     stream.seek(sub.end);   // 未知 Tag 或字段不匹配时仍能安全推进
   }
   ```
4. 装配完成 PAGDocument，返回 DecodeResult。

### 8.3bis 错误路径内存管理（强制实现规范）

> **目的**：Bake/Decode 过程中遇 fatal error 时，避免已构造的子对象（unique_ptr Composition/Layer/VectorElement 等）泄漏。本节用**唯一一种**实现范式消除二义性。

**规范**：**所有 Bake/Decode 入口必须用 `std::unique_ptr<PAGDocument>` 管理根对象**，fatal 时通过 `.reset()` 显式释放，不依赖函数退栈自动析构（因为 `doc` 会在成功路径被 move 出去）。

**Encode 路径（Baker）**：

```cpp
// src/pagx/pag/Baker.cpp
BakeResult Baker::Bake(const pagx::PAGXDocument& pagxDoc, BakeContext* ctx) {
  BakeResult result;
  auto doc = std::make_unique<PAGDocument>();       // ✅ 统一用 unique_ptr

  // 1) pre-pass：资源索引化
  ResourceBaker::bake(pagxDoc, *doc, ctx);
  if (ctx->hasFatal()) {                         // 立即 reset，释放已分配的 images/fonts
    doc.reset();
    result.warnings = std::move(ctx->warnings);
    result.errors   = std::move(ctx->errors);
    return result;  // doc=nullptr
  }

  // 2) main pass：Layer 树遍历
  LayerBaker::bake(pagxDoc, *doc, ctx);
  if (ctx->hasFatal()) {
    doc.reset();   // 已 push 的 compositions[]/layers[] 随 PAGDocument 析构链式释放
    result.warnings = std::move(ctx->warnings);
    result.errors   = std::move(ctx->errors);
    return result;
  }

  // 3) 成功路径：move 根对象到 result
  result.doc      = std::move(doc);              // unique_ptr 所有权转移
  result.warnings = std::move(ctx->warnings);
  return result;
}
```

**Decode 路径（Codec）**：镜像相同模式——用 `std::make_unique<PAGDocument>()` 构造根，主循环任一 Tag 产生 error 立即 `doc.reset() + return`。

**关键约束**：
1. **`BakeResult` / `DecodeResult::doc` 字段类型为 `std::unique_ptr<PAGDocument>`**（非裸指针，非 shared_ptr）；
2. **所有 push 到 `doc->compositions[]` / `composition->layers[]` 的子对象也必须是 unique_ptr**（附录 C.5-C.9 结构体字段已满足）；
3. **Fatal 判定点明确**：`BakeContext::hasFatal()` / `DecodeContext::hasError()`（已在 §8.5 定义）为 true 时立即 reset；
4. **Warning 不触发 reset**：降级告警不影响对象构造，继续推进；
5. **PAGDocument 析构链**：`unique_ptr<PAGDocument>` 析构时，其 `std::vector<std::unique_ptr<Composition>>` 自动析构，Composition 的 `std::vector<std::unique_ptr<Layer>>` 递归析构，链式释放所有子对象 —— **无需手写任何 delete**。

**禁止反面模式**：
```cpp
// ❌ 禁止：栈对象会在返回前被 move，但 fatal 路径可能不 move
PAGDocument doc;  ResourceBaker::bake(pagxDoc, doc, ctx);
if (ctx->hasFatal()) return {nullptr, ...};   // doc 在此被析构 ✓ 但让代码 intent 模糊

// ❌ 禁止：裸 new 没有 RAII 保障
PAGDocument* doc = new PAGDocument();
if (fatal) { delete doc; return {}; }         // 遇异常/早退路径易漏 delete

// ❌ 禁止：shared_ptr 引入不必要的原子计数开销（PAGDocument 是独占所有权）
auto doc = std::make_shared<PAGDocument>();
```

**测试要求**（§18.4bis 追加 1 条）：
- `BakerMemoryTest.FatalDuringLayerBakeNoLeak` —— 构造深度 65 的 PAGX 触发 `CompositionCycleDepth`，验证 `result.doc == nullptr` 且内存无泄漏（ASAN + valgrind 双跑）。

### 8.4 防御性

**强制使用 v1 DecodeStream**。`src/codec/utils/DecodeStream.h` 已提供：
- 所有 read\* API 在越界时返回 0 并 `PAGThrowError` 推送错误到 `DecodeStream::context`；越界不 crash；
- `bytesAvailable()` / `position()` / `setPosition()` / `skip()` 支持精确位置控制；
- Little endian 保证；
- `readEncodedInt32()` / `readEncodedUint32()` 支持 varint 解码。

**Codec::Decode 使用纪律**：
- 每个 `Read*Tag` 函数开头调用 `stream->bytesAvailable() >= MinimumTagSize` 预检查；
- 读完一个 Tag 后验证 `stream->position() == tagEnd`，若不等则 warn `ErrorCode::MalformedTag` 并 `stream->setPosition(tagEnd)` 强制对齐；
- 未知 TagCode → warn `ErrorCode::UnknownTagCode` + seek 到 tag 末尾（按 length）；
- 未知 PropertyEncoding → 按 §4.4 兜底策略：立即 seek 到**包裹该 Property 的最外层 Tag 末尾**，此 Tag 其余字段保持 PAGDocument 默认值（**不继续读取**），warn `UnknownPropertyEncoding`；
- 未知 enum 值（LayerType/BlendMode/... 等 u8 枚举的合法值域外数值）→ warn `ErrorCode::InvalidEnumValue` + 回填该字段的默认值（见附录 G 的 `ReadEnum<T>` 模板）；
- **所有校验失败都产生 `Diagnostic`，不抛异常**（项目禁用 C++ 异常）。

**多层校验**（Decoder 入口）：
1. `length >= 9`（最小文件头长度）；
2. Magic `'P','A','G'`；
3. Version `0x02`（不匹配 → `ErrorCode::UnsupportedVersion`）；
4. bodyLength 合理（<= 剩余 + MAX_TOTAL_BODY_BYTES 上限，见附录 H）；
5. Compression == 0（当前唯一合法）；
6. 所有资源/Tag 的大小上限（见附录 H）。

### 8.5 EncodeContext / DecodeContext

Codec 内部使用 Context 结构承载跨 Tag 的状态（错误收集、深度追踪、DoS 记账）。v2 Context **包装** v1 `StreamContext`，负责把 v1 流级错误桥接到 v2 诊断体系。

```cpp
#include "codec/utils/StreamContext.h"    // v1 StreamContext

namespace pagx::pag {

struct EncodeContext {
  // v1 EncodeStream 的上下文（传给 v1 读写工具）
  pag::StreamContext streamContext;

  std::vector<Diagnostic> warnings;
  uint32_t currentLayerDepth = 0;          // 用于 MAX_LAYER_DEPTH 检查
  uint32_t currentVectorElementDepth = 0;  // VectorGroup 嵌套深度
  bool strictMode = false;                 // 由 Baker/PAGExporter 传入

  void warn(ErrorCode code, std::string msg = {}) {
    if (warnings.size() >= limits::MAX_DIAGNOSTICS) return;   // §H.1 告警硬上限
    warnings.push_back({code, std::move(msg), 0});  // Encoder 不追踪 byteOffset
  }
};

struct DecodeContext {
  pag::StreamContext streamContext;
  pag::DecodeStream* currentStream = nullptr;   // 弱引用，Codec 主循环在每次 dispatch 前更新；用于 byteOffset 填充

  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
  uint32_t currentLayerDepth = 0;
  uint32_t currentVectorElementDepth = 0;
  bool strictMode = false;
  size_t totalAllocatedBytes = 0;

  uint32_t currentOffset() const {
    return currentStream ? static_cast<uint32_t>(currentStream->position()) : 0;
  }

  void error(ErrorCode code, std::string msg = {}) {
    // 错误硬上限：errors 达到 MAX_DIAGNOSTICS 则静默丢弃（避免恶意文件撑爆内存）。
    // 只保留最后一条"meta"告警说明已达顶；调用方按 hasError() 判定，不依赖条数。
    if (errors.size() >= limits::MAX_DIAGNOSTICS) return;
    errors.push_back({code, std::move(msg), currentOffset()});
  }
  void warn(ErrorCode code, std::string msg = {}) {
    // 告警硬上限（见上）。达顶时再次调用直接丢弃；不重复推"meta"，保持幂等。
    if (warnings.size() >= limits::MAX_DIAGNOSTICS) return;
    warnings.push_back({code, std::move(msg), currentOffset()});
  }
  bool hasError() const { return !errors.empty(); }

  /**
   * 在每个 Tag 解码完毕后调用：把 v1 StreamContext 累积的流级错误
   * （越界读、PAGThrowError 等）转成 v2 Diagnostic 并清空 StreamContext，
   * 以保证后续 Tag 的错误定位精度。
   *
   * NOTE: v1 StreamContext（src/codec/utils/StreamContext.h:30-47）仅暴露
   * `hasException()` + public field `errorMessages`（std::vector<std::string>）。
   * 没有 `errorMessage()` / `clearException()` 辅助方法，需直接操作 errorMessages。
   */
  void syncFromStreamContext() {
    if (streamContext.hasException()) {
      uint32_t offset = currentOffset();
      for (const auto& msg : streamContext.errorMessages) {
        if (errors.size() >= limits::MAX_DIAGNOSTICS) break;   // §H.1 硬上限
        errors.push_back({ErrorCode::TruncatedData, msg, offset});
      }
      streamContext.errorMessages.clear();
    }
  }
};

}
```

**Bridge 使用纪律**：
- v2 Codec 调用 v1 `ReadTagHeader(stream)` 前，`stream->context` **必须指向** `DecodeContext::streamContext`（而非自建 v1 context）——否则错误丢失；
- 每个 `Read<TagName>` 函数返回前（上层 Codec::Decode 主循环里）调一次 `ctx.syncFromStreamContext()`，确保 tag 级错误定位；
- 每个 tag 处理完后若 `ctx.hasError()` 为 true，**终止整个 Decode 流程**并返回 `DecodeResult{doc=nullptr, errors, warnings}`；
- Encode 侧同理：v1 `EncodeStream` 不会产生错误（只增长 buffer），`EncodeContext::streamContext` 作为 stream 构造参数传入，但不需要每 tag sync。

**使用纪律（通用）**：
- 所有 Read/Write 函数必须接受 `Context*` 参数；
- 深度追踪字段在 Read/Write LayerBlock / VectorGroup 的 进入/退出时 ++/--；
- DoS 记账在分配 vector/string 前累加 `totalAllocatedBytes`，超 `MAX_TOTAL_BODY_BYTES` 触发 `BodyLengthOutOfRange`。

---

## 9. LayerInflater 设计

### 9.1 接口

```cpp
#include "pagx/pag/ErrorCode.h"     // Diagnostic / ErrorCode 内部 alias

namespace pagx::pag {

class LayerInflater {
 public:
  struct Result {
    std::shared_ptr<tgfx::Layer> layer;        // 成功时通常非空；空文档合法场景为 nullptr（见下）。
                                               // 所有权语义详见 §15.3 "Layer 所有权语义"——实为独占所有权。
    std::vector<Diagnostic> warnings;          // Inflater 侧告警（600-699 段，详见 §9.4 触发点表）
  };

  /**
   * Inflate a PAGDocument into a tgfx::Layer tree.
   *
   * Never produces fatal errors in this design: any per-layer/element failure
   * is degraded (layer skipped, image/font/mask dropped) and reported as a
   * warning in `Result::warnings`.
   *
   * `Result::layer == nullptr` occurs only when the document itself has no
   * renderable content (compositions empty or compositions[0] has no layers).
   * In that case, exactly one `InflaterEmptyDocument` (604) warning is pushed
   * so callers can distinguish "input contained no scene" from "internal
   * failure" without string matching.
   */
  static Result Inflate(const PAGDocument& doc);
};

}
```

**无 frame 参数**（本期静态）。动画期扩展为 `Inflate(doc, frame)`，内部走 `PropertyEvaluator::Evaluate(prop, frame)` 分支，`Result::warnings` 语义不变。

> **为什么返回 Result 而非裸 `shared_ptr`**：Inflater 在资源解码/mask 解析阶段会产生告警（附录 G.2 的 600-699 段）。若仅返回 `shared_ptr<Layer>`，PAGLoader 链路**会丢失**这些告警（v2.3 → v2.4 评估 E-1 发现的问题）。Result 形态与 `DecodeResult` / `BakeResult` 对称，上游（PAGLoader）可直接 merge warnings。

### 9.2 实现策略

和 `LayerBuilder::build`（行 137-163）严格同构的两趟遍历（外加一次退栈前的 warnings 转移）：

```
Pass 1: 按 compositions[0] 构造 tgfx::Layer 树
        - 每个 PAGDocument::Layer：
            * 按 type 派发到 tgfx::Layer::Make / VectorLayer::Make / ...
            * applyCommon(src, tgfxLayer) 逐字段 setter（镜像 LayerBuilder::applyLayerAttributes 736-800）
            * 按 payload 调 subtype apply
            * 递归 children 并 addChild
            * 若 maskLayerPath 非空，记入 pendingMasks
            * 把 (layerPath, tgfxLayer) 写入 layerByPath
            * 资源解码/mask 解析失败 → 推 warning 到 InflaterContext::warnings
Pass 2: 应用 mask
        - 对 pendingMasks 逐条：查表、setVisible(true)、setMask、setMaskType
        - 查不到 → warn `InflateMaskResolveFailed` 并跳过
Finalize（退栈前，非数据遍历）: 把 InflaterContext::warnings 一次性 move 到 Result::warnings
```

### 9.3 字段应用模板

```cpp
// 本期静态：直读 Property<T>::value
tgfxLayer->setAlpha(src.alpha.value);
tgfxLayer->setMatrix(src.matrix.value);
// ...
// TODO(animation): tgfxLayer->setAlpha(PropertyEvaluator::Evaluate(src.alpha, frame));
```

每行 `// TODO(animation)` 注释作为动画期的改造锚点。

### 9.4 InflaterContext

Inflater 内部使用的轻量上下文，承载 warning 收集 + pendingMasks + layerByPath 查表。结构与 §8.5 `DecodeContext` 对称但更精简（Inflater 无字节流，无 DoS 记账需求）。

```cpp
namespace pagx::pag {

struct InflaterContext {
  // 告警聚合（Inflater 无字节流，byteOffset 恒 0）
  std::vector<Diagnostic> warnings;

  // Mask 两趟状态（§12）
  std::unordered_map<std::vector<uint32_t>,
                     std::shared_ptr<tgfx::Layer>,
                     VectorU32Hash>  layerByPath;   // Pass 1 填充，Pass 2 查
  struct PendingMask {
    std::shared_ptr<tgfx::Layer> host;
    std::vector<uint32_t>        targetPath;
    LayerMaskType                maskType;
  };
  std::vector<PendingMask> pendingMasks;

  void warn(ErrorCode code, std::string msg = {}) {
    if (warnings.size() >= limits::MAX_DIAGNOSTICS) return;   // §H.1 告警硬上限
    warnings.push_back({code, std::move(msg), 0});
  }
};

}
```

**5 个 Inflater 告警码的触发点**（与附录 G.2 的 600-699 段严格一一对应）：

| ErrorCode | 触发函数/位置 | 降级行为 |
|---|---|---|
| `InflateImageDecodeFailed` (600) | `inflateImagePattern` 内 `tgfx::Image::MakeFromEncoded` 返回 null | 该 paint 的 colorSource 置 null，paint 回退为空填充 |
| `InflateFontCreateFailed` (601) | `resolveFontAsset` 内 `tgfx::Typeface::MakeFromBytes`（Embedded）或系统字体查询失败 | 降级到系统默认字体 |
| `InflateGlyphRunBuildFailed` (602) | `inflateText` 内 `GlyphRunRenderer::BuildTextBlob`/`BuildTextBlobFromLayoutRuns` 返回 null | 该 ElementText 被跳过，其外层 VectorGroup 其他子元素仍渲染 |
| `InflateMaskResolveFailed` (603) | Pass 2 `layerByPath` 查不到 `maskLayerPath` | 该 layer 的 mask 丢弃，layer 本身仍渲染 |
| `InflaterEmptyDocument` (604) | 入口检查 `compositions.empty() \|\| compositions[0]->layers.empty()` | 返回 `Result{layer=nullptr, warnings=[604]}` |

**Finalize 收尾规则**（与 §9.2 一致）：Inflater 返回前将 `ctx.warnings` 一次性 move 到 `Result::warnings`，不做去重——调用方（如 PAGLoader）再 append 到自己的 Result 即可。

---

## 10. 字体双模式与文本处理

### 10.1 模式定义

```cpp
enum class FontMode { Render, OutlineAll };
```

- **Render**（默认）：字体走 FontAsset（System 或 Embedded），TextBlob 在运行时由 tgfx 按字体光栅化产生。
- **OutlineAll**：Baker 在转换阶段将所有 glyph 烘焙为 `tgfx::Path`，作为 `ElementShapePath` 写入 VectorLayer。生成的 pag 文件不依赖字体。

### 10.2 Render 模式：单 span / 多 span 分叉

按用户决策 b 方案：

```
独立 Text VectorElement（非 TextBox 子节点）                → ElementText
TextBox 含单 span                                            → VectorGroup 包 1 个 ElementText
TextBox 含多 span（不同 family/size/color/stroke）            → VectorGroup 包 N 个 ElementText（每 span 一个）
TextBox.elements.empty()                                     → 跳过
```

**重要**：本期**不产出** `LayerType::Text`（tgfx::TextLayer 只支持单 style，PAGX 富文本一律走 VectorLayer）。TextPayload 结构预留，为未来可能的简单文本路径。

### 10.3 GlyphRun 序列化结构

Baker 不直接写 `tgfx::TextBlob`（非跨平台可复现），改为序列化"构造 TextBlob 所需原材料"：

```
GlyphRunBlob {
  uint8  kind              // 0 = LayoutRun (优先，Text->glyphData->layoutRuns 非空)
                           // 1 = GlyphRun fallback (Text->glyphRuns 非空)
  uint32 fontIndex         // 指向 PAGDocument.fonts
  float  fontSize
  bool   vertical
  varint count
  repeat count:
    uint16 glyphId
    float  posX, posY
    if kind == LayoutRun:
      float  advance
      uint32 codepoint
      ...（TODO：开工时查 GlyphRunRenderer 确认最小字段集）
  Matrix inverseMatrix     // TextBox 子 Text 才非 identity
}
```

Inflater 侧按 kind 分别调 `GlyphRunRenderer::BuildTextBlobFromLayoutRuns` 或 `BuildTextBlob` 重建。

### 10.4 TextBox 的 inverseMatrix 处理

对齐 `LayerBuilder::prepareTextBoxTextBlobs`（248-260）：
1. 用 `TextLayout::CollectTextElements` 收集 TextBox 内所有 Text 子节点 + 累计 matrices；
2. 对每个 Text 计算 `inverseMatrix = matrices[i].invert()`；
3. invert 失败时**跳过该 Text**（和 LayerBuilder 第 256 行的 continue 对齐）；
4. 成功的 inverseMatrix 写入该 Text 的 GlyphRunBlob，Inflater 重建 TextBlob 时使用。

### 10.5 OutlineAll 模式

- Baker 调 `GlyphRunRenderer` 产出 `tgfx::TextBlob`；
- 遍历每 glyph → 导出 `tgfx::Path` → 产生 `ElementShapePath`（带 position + path）；
- 外层用 `ElementVectorGroup` 承载 anchor/position/scale/rotation（同 Group bake 逻辑）；
- **不产出** `ElementText`；涉及的 Font 节点不进入 `PAGDocument::fonts`（ResourceBaker 只收集被 Render 路径引用的字体）。

---

## 11. 资源管理

### 11.1 ImageAsset

```cpp
struct ImageAsset {
  std::vector<uint8_t> data = {};  // 原始编码 bytes（PNG/JPG/WebP）
  int width = 0;
  int height = 0;
};
```

**来源三分支**（对齐 LayerBuilder::convertImagePattern 545-576）：

| PAGX Image 来源 | ResourceBaker 处理 |
|---|---|
| `imageNode->data` 非空 | 直接透传 bytes |
| `filePath` 为 `data:...` URI | base64 解码后得到 bytes |
| 普通文件路径 | 读磁盘文件得到 bytes（导入阶段已 resolve 为绝对路径）|

**去重 key**：
- 内嵌：`Data*` 指针
- data URI：完整 URI 字符串
- 文件：绝对路径字符串

**失败处理**：文件读取/URI 解码失败 → warning + 该 paint 的 `imageIndex = UINT32_MAX`（哨兵）→ Inflater 见到哨兵返回 null colorSource（对齐 LayerBuilder 行 559-561）。

### 11.2 FontAsset

```cpp
enum class FontSourceKind : uint8_t {
  System = 0,     // family + style，播放端查本地字体
  Embedded = 1,   // 内嵌字体 bytes（TTF/OTF）
};

struct FontAsset {
  FontSourceKind kind = FontSourceKind::System;
  std::string family = {};
  std::string style = {};
  std::vector<uint8_t> data = {};  // Embedded only
};
```

**来源**：
- PAGX Font node `data` 空 → System
- PAGX Font node `data` 非空 → Embedded

**去重 key**：
- System：`(family, style, "")`
- Embedded：`(family, style, dataPtr)`

**失败处理**：外部字体文件读取失败 → warning + 降级为 System（保留 family/style）；Inflater 走系统字体兜底。

### 11.3 索引化时机

Resource pre-pass 在 `LayerBaker::bake` 之前完成：
1. 深度遍历 `doc.layers`，收集所有 Image / Font node；
2. 按去重 key 分配 index，写入 `BakeContext::imageIndexByNode` / `fontIndexByNode`；
3. VectorBaker / TextBaker 在 main pass 中直接查 index 即可，不需回填。

---

## 12. Mask 两趟处理

**对齐 LayerBuilder 149-160 + 189-191。**

### 12.1 Baker 侧

**Pass 1**：为每个 PAGX Layer\* 分配 `layerPath`（所在 composition 内从 root 到该 layer 的 child index 链），写入 `BakeContext::layerPathByPagxLayer`。

**Pass 2**：main pass 中，遇 `pagxLayer->mask != nullptr`：
- 查表得目标 layer 的 layerPath；
- 写入 `outLayer->maskLayerPath` + `outLayer->maskType`；
- 查不到 → warning + `maskLayerPath` 保持为空（Inflater 会跳过）。

### 12.2 Inflater 侧

**Pass 1**：建树 + 记录 `layerByPath[path] = tgfxLayer`；凡 `maskLayerPath 非空` 的 layer 加入 `pendingMasks`。

**Pass 2**：对 pendingMasks 每条：
- 查 `layerByPath` 得 maskTgfxLayer；
- 调 `maskTgfxLayer->setVisible(true)` + `tgfxLayer->setMask(maskTgfxLayer)` + `tgfxLayer->setMaskType(maskType)`；
- 查不到 → 跳过（Baker 侧已 warn）。

---

## 13. 动画可扩展性保留

### 13.1 保留位

1. **Property<T> 壳**：每个可动画字段已是 Property<T>，字节流前缀 1 byte `propHeader` 位域（含 encoding / isDefault / hasExt，见 §4.3）+ 可选 value。本期始终写 Constant。
2. **PropertyEncoding 枚举**：预留 Hold / Linear / Bezier / Spatial 等编码类型。
3. **向前兼容解码策略**：本期 Decoder 对未知 PropertyEncoding 按 §4.4 兜底（放弃当前 Tag 剩余字段），保证旧 Decoder 读动画期生成的文件不崩溃。
4. **Timeline 字段**：`FileHeader::frameRate/duration`、`Layer::startTime/duration/stretch` 已存在，本期写 "静态默认值"。
5. **TextModifier / RangeSelector Tag**：Tag code 与字段布局已定义，求值逻辑留待动画期。
6. **Read/WriteProperty<T> 模板**：当前只实现 Constant 分支，非 Constant 分支留 TODO 注释。

### 13.2 动画期改动点（非破坏性）

| 改动位置 | 内容 |
|---|---|
| `Property<T>` 内存结构 | 新增类型化 keyframe 容器字段（字节流不变） |
| PAGX DOM | 升级字段为 AnimatedValue<T> |
| `BakeProperty<T>` | 新增 `AnimatedValue<T>` 重载 |
| `WriteProperty<T>` | 填满非 Constant 分支 |
| `ReadProperty<T>` | 对称填满 |
| `LayerInflater` | 直读 `.value` 替换为 `PropertyEvaluator::Evaluate(prop, frame)` |
| 播放器层 | 新增 `PAGAnimationPlayer` 驱动 currentFrame，逐帧重建/patch tgfx::Layer 树 |

**不需要**：新增 Tag、改字节布局、升版本号。

---

## 14. CLI 集成

### 14.1 复用 `pagx export`

扩展 `src/cli/CommandExport.cpp`：

```
Usage: pagx export [options]

Output formats: svg, html, pag

PAG options:
  --pag-font-mode <mode>   render | outline-all (default: render)
  --pag-strict             Treat warnings as errors

Examples:
  pagx export --input icon.pagx --format pag
  pagx export --input icon.pagx --output icon.pag
  pagx export --input icon.pagx --format pag --pag-font-mode outline-all
```

### 14.2 `ExportToPAG` 实现要点

1. `LoadDocument` → 检查 `hasUnresolvedImports` → `applyLayout`；
2. 调 `PAGExporter::ToFile(*document, outputFile, opts)`；
3. 打印 errors/warnings 到 stderr——CLI 调 `pagx::FormatDiagnostic(d)` 将 `Diagnostic` 格式化为 `"[<CodeName>] <message> @0x<hex>"` 逐行输出；
4. `--pag-strict` 下 warnings 非空视为 failure，退出码非 0。

**诊断输出示例**（CLI 惯用写法）：

```cpp
for (const auto& d : result.errors)   std::cerr << "error: "   << pagx::FormatDiagnostic(d) << "\n";
for (const auto& d : result.warnings) std::cerr << "warning: " << pagx::FormatDiagnostic(d) << "\n";
```

---

## 15. 对外 API

v2 对外共暴露三个公共头，按角色隔离依赖：

| 公共头 | 用途 | 是否依赖 tgfx |
|---|---|---|
| `include/pagx/Diagnostic.h` | 错误/告警码 + `Diagnostic` 结构 + `FormatDiagnostic` | 否 |
| `include/pagx/PAGExporter.h` | PAGX → PAG v2 字节流导出 | 否 |
| `include/pagx/PAGLoader.h` | PAG v2 字节流 → `tgfx::Layer` 树加载 | **是** |

**分层约束**（强制）：
- 只需要导出能力的 SDK 用户只 include `PAGExporter.h`，**整条链路不触碰 tgfx 符号**；
- 需要渲染消费的用户 include `PAGLoader.h`，该头显式依赖 tgfx；
- `Diagnostic.h` 同时被两个入口共用，作为基础公共结构。

**不对外暴露**（内部实现）：`PAGDocument`、`Codec`、`LayerInflater`、`Baker`——全部作为内部实现细节放在 `src/pagx/pag/`。

### 15.1 `include/pagx/Diagnostic.h`

```cpp
// Copyright (C) 2026 Tencent. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

namespace pagx {

/**
 * Diagnostic codes for PAG v2 export/load operations. Codes are partitioned
 * by module + severity; new codes must be appended within a segment (never
 * reuse deleted numbers, never cross segments).
 *
 *   100-199: Baker fatal    (document cannot be produced)
 *   200-299: Baker warning  (document produced with degradation)
 *   300-399: Codec fatal    (decode/encode failed)
 *   400-499: Codec warning  (decode succeeded with skipped content)
 *   600-699: Inflater warning (layer tree produced with degradation)
 *
 * The full code list is in 附录 G.2 of the design doc. Adding a new code
 * here is an ABI-level change; follow the segment-append rule strictly.
 */
enum class DiagnosticCode : uint16_t {
  Ok = 0,

  // Baker fatal
  LayoutNotApplied           = 100,
  UnresolvedImports          = 101,
  NullDocument               = 102,
  EmptyCompositions          = 103,
  CompositionCycleDepth      = 104,
  StructureLimitExceeded     = 105,   // 任一结构性计数/长度超出附录 H 硬上限
                                      // （MAX_COMPOSITIONS / MAX_LAYERS_PER_COMPOSITION /
                                      //  MAX_VECTOR_ELEMENTS_PER_PAYLOAD / MAX_FILTERS_PER_LAYER /
                                      //  MAX_STYLES_PER_LAYER / MAX_GRADIENT_STOPS /
                                      //  MAX_PATH_VERBS / MAX_GLYPHS_PER_RUN /
                                      //  MAX_NAME_STRING_BYTES / MAX_TEXT_STRING_BYTES 等）。
                                      // 注：此码与 StringInvalidUtf8(403) 语义独立——
                                      //   前者 = 长度超限（结构性病态），
                                      //   后者 = UTF-8 字节非法（编码损坏）。
                                      // message 字段应附具体超限字段名，如
                                      // "MAX_VECTOR_ELEMENTS_PER_PAYLOAD exceeded (got 120000)".

  // Baker warning
  ImageSourceMissing         = 200,
  FontSourceMissing          = 201,
  MaskTargetMissing          = 202,
  MaskSelfReference          = 203,
  BlendModeUnmapped          = 204,
  InverseMatrixNonInvertible = 205,
  TextGlyphDataEmpty         = 206,
  EmptyDocument              = 207,
  GlyphRunKindInferred       = 208,

  // Codec fatal
  InvalidMagic               = 300,
  UnsupportedVersion         = 301,
  UnsupportedCompression     = 302,
  TruncatedData              = 303,
  MalformedTag               = 304,
  BodyLengthOutOfRange       = 305,
  InvalidCompositionIndex    = 306,
  FileReadFailed             = 307,   // PAGLoader::LoadFromFile 路径专用

  // Codec warning
  UnknownTagCode             = 400,
  UnknownPropertyEncoding    = 401,
  ResourceSizeExceeded       = 402,
  StringInvalidUtf8          = 403,
  PathVerbLimitExceeded      = 404,
  GlyphCountLimitExceeded    = 405,
  LayerDepthLimitExceeded    = 406,
  InvalidEnumValue           = 407,

  // Inflater warning
  InflateImageDecodeFailed   = 600,
  InflateFontCreateFailed    = 601,
  InflateGlyphRunBuildFailed = 602,
  InflateMaskResolveFailed   = 603,
  InflaterEmptyDocument      = 604,   // compositions[0] 缺失或为空，无可渲染内容；Result.layer == nullptr
};

/**
 * Structured diagnostic emitted by export/load operations.
 *   - `code`       : machine-readable code; callers should dispatch on this.
 *   - `message`    : optional human-readable context; may be empty.
 *   - `byteOffset` : decoder path = stream->position() at error time; all
 *                    other paths (baker/encoder/inflater) = 0.
 */
struct Diagnostic {
  DiagnosticCode code       = DiagnosticCode::Ok;
  std::string    message;
  uint32_t       byteOffset = 0;
};

/**
 * Format a Diagnostic as "[<CodeName>] <message>" with a " @0x<hex>" suffix
 * when byteOffset is non-zero. Example:
 *   "[TruncatedData] unexpected EOF @0x4a2c"
 *   "[LayoutNotApplied] document must have applyLayout() called first"
 */
std::string FormatDiagnostic(const Diagnostic& d);

}  // namespace pagx
```

**ABI 纪律**：
- `DiagnosticCode` 是 ABI 承诺的一部分——枚举值**只能在段内追加**，不得复用已删除编号，不得跨段迁移；
- 内部 `pagx::pag::ErrorCode` **是** `pagx::DiagnosticCode` 的 `using` 别名（见附录 G.2），保持单一真相；
- 新增内部错误码等同于新增公共枚举值，走"段内追加"即可，不升 FORMAT_VERSION。

### 15.2 `include/pagx/PAGExporter.h`

```cpp
// Copyright (C) 2026 Tencent. All rights reserved.
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "pagx/Diagnostic.h"
#include "pagx/PAGXDocument.h"

namespace pagx {

class PAGExporter {
 public:
  struct Options {
    /**
     * Controls how text is emitted.
     *   Render:      Emit text as reference to system or embedded fonts; renderer
     *                performs glyph rasterization at playback time. Default.
     *   OutlineAll:  Bake every glyph to vector paths at conversion time. Produces
     *                self-contained files at the cost of size.
     */
    enum class FontMode { Render, OutlineAll };
    FontMode fontMode = FontMode::Render;

    /**
     * If true, treat warnings as errors and abort. Default false.
     * Mapped to internal EncodeContext::strictMode / DecodeContext::strictMode
     * one-to-one by PAGExporter.cpp (the external `strict` and internal
     * `strictMode` are semantically equivalent).
     */
    bool strict = false;
  };

  /**
   * Export result.
   *
   *   ok == true  ⟺  errors.empty()
   *
   * On ok:
   *   - `ToBytes` populates `bytes`; `ToFile` writes the file and leaves `bytes` empty.
   *   - `warnings` may be non-empty (degradations such as `ImageSourceMissing`).
   * On !ok:
   *   - `errors` is non-empty; `bytes` is empty (ToBytes) or the file may be
   *     partially written (ToFile — callers should remove it if unwanted).
   *
   * All fields are owned by this struct.
   */
  struct Result {
    bool ok = false;
    std::vector<uint8_t> bytes;              // ToBytes only
    std::vector<Diagnostic> errors;          // structured; use FormatDiagnostic for logging
    std::vector<Diagnostic> warnings;
  };

  /**
   * Exports the document to a file.
   * @param document  Source document. Must have applyLayout() called.
   * @param filePath  Output path. Parent directory must exist.
   * @param options   Export options; defaults suitable for most cases.
   * @return Result with ok=true on success. On failure, filePath may be partially
   *         written; callers should remove it if unwanted.
   *
   * Thread-safety: Reentrant; no shared mutable state. Each call independently
   * produces its own Result. Same document MAY be exported concurrently from
   * multiple threads, but the PAGXDocument itself must not be mutated during export.
   */
  static Result ToFile(const PAGXDocument& document, const std::string& filePath,
                       const Options& options = {});

  /**
   * Exports the document to an in-memory byte buffer.
   * @see ToFile for semantics.
   */
  static Result ToBytes(const PAGXDocument& document,
                        const Options& options = {});
};

}  // namespace pagx
```

**线程安全约定**：所有 API **可重入**，但 `PAGXDocument` 在 export 期间**不得被其他线程修改**。Result 各字段归调用方所有。

**依赖约束**（强制）：`include/pagx/PAGExporter.h` **不得 include 任何 tgfx 头文件**，保持仅需导出能力的 SDK 用户不被迫依赖 tgfx。内部实现头（`src/pagx/pag/` 下）可自由 include tgfx。

### 15.3 `include/pagx/PAGLoader.h`

```cpp
// Copyright (C) 2026 Tencent. All rights reserved.
#pragma once

// IMPORTANT: This header depends on tgfx. Include it only if your module
// consumes rendered tgfx::Layer trees. For export-only use cases that do
// not need rendering, include <pagx/PAGExporter.h> instead — that header
// is tgfx-free.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "pagx/Diagnostic.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

class PAGLoader {
 public:
  /**
   * Load result.
   *
   *   ok == true  ⟺  errors.empty()
   *
   * On ok:
   *   - `errors` is empty. `warnings` may be non-empty (degradations).
   *   - `layer` is usually non-null. It CAN be null with a single
   *     `InflaterEmptyDocument` (604) warning when the document legitimately
   *     contains no renderable content (compositions empty or
   *     compositions[0]->layers empty). This is a success case — callers
   *     must not treat null layer alone as failure.
   *   - `canvasWidth/Height` are populated from the PAG FileHeader.
   *
   * On !ok:
   *   - `errors` is non-empty (at least one fatal diagnostic).
   *   - `layer` is null; `canvasWidth/Height` may be 0 if decode failed
   *     before the FileHeader was parsed.
   */
  struct Result {
    bool ok = false;
    std::shared_ptr<tgfx::Layer> layer;
    int canvasWidth  = 0;
    int canvasHeight = 0;
    std::vector<Diagnostic> errors;
    std::vector<Diagnostic> warnings;
  };

  /**
   * Load a PAG v2 byte stream and inflate it into a tgfx::Layer tree.
   *
   * @param data    Pointer to PAG v2 bytes. If null or `length < 9`, returns a
   *                Result with ok=false and a single `TruncatedData` (303) error
   *                (treated as "input does not constitute a valid PAG v2 header").
   * @param length  Byte count. Valid PAG v2 bytes always satisfy length >= 9.
   * @return        Result with ok=true on success (layer may be null for
   *                empty-but-valid documents — see Result docstring).
   *
   * Thread-safety: Reentrant. Each call produces an independent Layer tree;
   * safe to invoke concurrently. The returned Layer tree is not itself
   * thread-safe after this call returns (standard tgfx::Layer contract).
   */
  static Result LoadFromBytes(const uint8_t* data, size_t length);

  /**
   * Convenience wrapper. Reads the file then calls LoadFromBytes.
   * On read failure, Result has ok=false and a single FileReadFailed error.
   */
  static Result LoadFromFile(const std::string& filePath);
};

}  // namespace pagx
```

**设计要点**：
- Result 持有 `std::shared_ptr<tgfx::Layer>` 直接交给调用方——可 `tgfx::DisplayList::root()->addChild(...)` 渲染；**语义上为独占所有权**（见下方"Layer 所有权语义"），使用 `shared_ptr` 仅因 tgfx Layer 工厂返回 shared_ptr，并非鼓励共享；
- `canvasWidth/Height` 放入 Result 顶层，避免调用方再反查 FileHeader 才能创建 Surface；
- `LoadFromFile` 的文件读取失败产出 `DiagnosticCode::FileReadFailed`（错误码见附录 G.2）；
- Loader **不维护任何全局状态或缓存**——想复用字节流需调用方自己缓存 `std::vector<uint8_t>` 再多次 `LoadFromBytes`；
- Result.warnings **同时收集** Codec::Decode 和 LayerInflater::Inflate 两个阶段的告警（见 §9.1 Inflater Result 设计）。

**Layer 所有权语义（重要）**：

虽然 `Result::layer` 类型是 `std::shared_ptr<tgfx::Layer>`，但**语义上属独占所有权**，不应被多个持有者共享：

- 同一 `tgfx::Layer` 树**不得被多个 `tgfx::DisplayList` 同时持有**——这是 tgfx 的使用契约（见 §18.2 渲染 helper 的约束说明）；
- 调用方拿到 `Result::layer` 后，**典型用法是**立即挂到自己的 DisplayList 下（`displayList.root()->addChild(r.layer)`），然后释放 `r`；
- **严禁**把 `r.layer` `clone()` 或 `copy` 到第二个 DisplayList；
- 多次渲染同一 PAG 文件的场景应**重新调用 `LoadFromBytes`**（或调用方自己缓存字节流），不要复用同一 Layer 树；
- 使用 `shared_ptr` 而非 `unique_ptr` 的原因：**tgfx::Layer::Make 系列工厂方法返回 `std::shared_ptr<Layer>`**（tgfx 内部使用 enable_shared_from_this 模式），PAGLoader 无法无代价转换为 unique_ptr；采用 shared_ptr 是**被动妥协**，不是鼓励共享。

**内部实现骨架**（位于 `src/pagx/pag/PAGLoader.cpp`）：

```cpp
Result PAGLoader::LoadFromBytes(const uint8_t* data, size_t length) {
  Result out;
  // data==null 或 length<9 统一归为 TruncatedData——语义为"输入不构成合法 PAG v2 头部"
  if (data == nullptr || length < 9) {
    out.errors.push_back({DiagnosticCode::TruncatedData,
                          data == nullptr ? "input data is null"
                                          : "input too small (< 9 bytes)",
                          0});
    return out;    // ok 默认为 false（errors 非空）
  }
  auto decoded = pagx::pag::Codec::Decode(data, length);
  out.errors   = std::move(decoded.errors);
  out.warnings = std::move(decoded.warnings);
  if (!decoded.doc) return out;                       // fatal already in errors

  out.canvasWidth  = static_cast<int>(decoded.doc->header.width);
  out.canvasHeight = static_cast<int>(decoded.doc->header.height);

  // LayerInflater::Inflate 返回 Result{layer, warnings}——合并其 warnings
  // 空文档场景：inflated.layer == nullptr + warnings 含 InflaterEmptyDocument(604)，
  // 这是合法成功场景；ok 由 errors 而非 layer 判定。
  auto inflated = pagx::pag::LayerInflater::Inflate(*decoded.doc);
  out.layer = std::move(inflated.layer);
  out.warnings.insert(out.warnings.end(),
                      std::make_move_iterator(inflated.warnings.begin()),
                      std::make_move_iterator(inflated.warnings.end()));

  // ok 契约：errors 为空 ⟺ 成功（layer 可能为 null 但仍是合法的空场景）
  out.ok = out.errors.empty();
  return out;
}
```

**依赖约束**：`PAGLoader.h` 是 v2 **唯一**允许依赖 tgfx 的公共头。其他公共头保持 tgfx-free 约束不变（§I.1）。

---

## 16. 目录结构与代码组织

```
include/pagx/
├── Diagnostic.h                  # 对外 Diagnostic 结构 + DiagnosticCode 枚举 + FormatDiagnostic
├── PAGExporter.h                 # 导出公共头（tgfx-free）
└── PAGLoader.h                   # 加载公共头（依赖 tgfx，渲染消费方使用）

src/pagx/
├── Diagnostic.cpp                # FormatDiagnostic 实现

src/pagx/pag/
├── PAGDocument.h                 # PAGDocument / Layer / VectorElement / Property 等全量定义
├── TagCode.h                     # TagCode 枚举
├── PropertyEncoding.h            # Read/WriteProperty<T> 模板
├── ValueCodec.h                  # Read/WriteValue<T> 每 T 特化
├── ErrorCode.h                   # using ErrorCode/Diagnostic = pagx::DiagnosticCode/Diagnostic; 内部 alias（§G.3）
├── DiagBuild.h                   # MakeDecodeDiag / MakeDiag 内部辅助（避开与公共 Diagnostic.h 同名）
├── Baker.h / Baker.cpp           # 顶层 Bake 编排（内部头）
├── BakeContext.h / .cpp          # 资源索引、mask 上下文、错误/告警
├── PAGExporter.cpp               # 导出对外 API 实现
├── PAGLoader.cpp                 # 加载对外 API 实现（调用 Codec::Decode + LayerInflater::Inflate）
├── layer_bakers/
│   ├── LayerBaker.cpp            # 通用 Layer 字段 + CompositionBaker
│   ├── VectorBaker.cpp           # VectorLayer + 14 种 VectorElement + PaintBaker
│   ├── TextBaker.cpp             # 双模式文本处理
│   ├── StyleFilterBaker.cpp      # 3 style + 5 filter
│   └── ResourceBaker.cpp         # Image/Font 收集去重
├── Codec.h / Codec.cpp           # Encode / Decode
├── tags/
│   ├── FileHeaderTag.cpp
│   ├── CompositionTag.cpp
│   ├── LayerBlockTag.cpp
│   ├── ShapePayloadTag.cpp
│   ├── TextPayloadTag.cpp
│   ├── ImagePayloadTag.cpp
│   ├── SolidPayloadTag.cpp
│   ├── VectorPayloadTag.cpp
│   ├── MeshPayloadTag.cpp        # 空实现占位
│   ├── CompositionRefPayloadTag.cpp
│   ├── ElementTags.cpp           # 14 种 element 读写
│   ├── FilterTags.cpp            # 5 种 filter
│   ├── StyleTags.cpp             # 3 种 style
│   └── ResourceTags.cpp          # ImageAssetTable / FontAssetTable
├── LayerInflater.h / .cpp        # PAGDocument → tgfx::Layer
└── InflaterContext.h             # warnings + pendingMasks + layerByPath（权威定义见 §9.4）

src/cli/
└── CommandExport.cpp              # 扩展 --format pag 分支

# 注：pag v1 播放器（src/codec/）本期不做改动；v1 对版本 0x02 已有 graceful reject 路径
# （src/codec/Codec.cpp::ReadBodyBytes:196 的 KnownVersion 检查）

test/src/pag/
├── support/
│   ├── RenderUtil.h / .cpp        # tgfx::DisplayList 渲染 helper + diff 辅助
│   ├── SampleEnumerator.h / .cpp  # spec/samples 枚举 + hasText 扫描
│   └── PAGXBuilder.h / .cpp       # 单测 DOM 构造 fluent builder
├── unit/
│   ├── ValueCodecTest.cpp
│   ├── PropertyEncodingTest.cpp
│   ├── TagHeaderTest.cpp
│   ├── DiagnosticTest.cpp        # FormatDiagnostic / MakeDecodeDiag / MakeDiag
│   ├── BakeContextTest.cpp
│   ├── BakerEdgeCasesTest.cpp    # Baker fatal 段（100-105 + 207）集中测试
│   ├── LayerBakerTest.cpp
│   ├── VectorBakerTest.cpp
│   ├── PaintBakerTest.cpp
│   ├── TextBakerTest.cpp
│   ├── StyleFilterBakerTest.cpp
│   └── ResourceBakerTest.cpp
├── codec/                           # §18.4bis 压缩机制专项（25 条，8 文件）
│   ├── ColorCodecTest.cpp
│   ├── PropertyCodecTest.cpp
│   ├── MatrixCodecTest.cpp
│   ├── PathCodecTest.cpp
│   ├── ShapeStyleCodecTest.cpp
│   ├── GlyphRunBlobCodecTest.cpp
│   ├── LayerBitFlagsCodecTest.cpp
│   └── VersionedTagTest.cpp
├── integration/
│   ├── RoundTripTest.cpp
│   ├── PAGDocumentParityTest.cpp
│   ├── InflaterParityTest.cpp
│   ├── VersionRejectTest.cpp
│   └── TruncatedDecodeTest.cpp
├── e2e/
│   ├── EndToEndTest.cpp
│   ├── EdgeCasesTest.cpp
│   └── PAGLoaderTest.cpp         # 对外加载 API 端到端测试
├── render/
│   └── RenderEquivalenceTest.cpp
└── perf/
    └── PerformanceTest.cpp

test/perf/
└── baseline.json                  # 性能基线（入 git）

tools/
└── verify_pag_conversion.sh       # 一键验收脚本
```

### CMake 集成

- 新增静态库 `pagx-pag`，包含：
  - 三个对外公共头：`include/pagx/Diagnostic.h` / `include/pagx/PAGExporter.h` / `include/pagx/PAGLoader.h`
  - 对外 Diagnostic 格式化实现：`src/pagx/Diagnostic.cpp`
  - 内部实现全部：`src/pagx/pag/` 下所有 `.h` / `.cpp`（含 `PAGExporter.cpp` / `PAGLoader.cpp` / Codec / Baker / Inflater / tags / layer_bakers 等）
- `tgfx` 作为 `pagx-pag` 的 **PUBLIC** 依赖（因为 `PAGLoader.h` 在签名中暴露 `tgfx::Layer`），详细配置见 §I.4；
- `pagx-cli` 链接 `pagx-pag`；
- `PAGFullTest` 链接 `pagx-pag` + 新增 test 源文件（含阶段 0 `DiagnosticTest.cpp`、阶段 10.5 `PAGLoaderTest.cpp` 等）。

---

## 17. 验收标准

分四档，从硬性到软性。全部自动化可判定。

### 档 A：功能正确性（必过）

| ID | 项 | 判定 |
|---|---|---|
| A1 | spec/samples 全部能 export 成 pag | 无 crash、退出码 0、输出非空、头部正确 |
| A2 | 所有产出的 pag 能被 Decode 成功 | 无 error、字段非空 |
| A3 | `Encode(Decode(Encode(doc)))` 字节等价 `Encode(doc)` | memcmp 一致 |
| A4 | `Decode(Encode(doc))` 字段等价 `doc` | 深度字段等价 |
| A5 | `LayerInflater::Inflate(...).layer` 非空，结构与 LayerBuilder 树同构；`.warnings` 在健康样本上为空 | 深度对比 + warning 计数 |

### 档 B：渲染一致性（必过，使用 Baseline::Compare）

| ID | 项 | 判定 |
|---|---|---|
| B1 | Render 模式：path A（LayerBuilder 渲染的 surfaceA）与 path B（PAG v2 全链路渲染的 surfaceB）使用 `Baseline::Compare` 比对通过 | 基准图同 key 统一处理，使用项目既定容差 |
| B2 | Render 模式基准图回归 `PAGRenderTest_Render/{sample}` | `Baseline::Compare(surfaceB, "PAGRenderTest_Render/" + sample)` |
| B3 | OutlineAll 模式：surfaceB 独立基准图 `PAGRenderTest_OutlineAll/{sample}` | `Baseline::Compare(surfaceB, "PAGRenderTest_OutlineAll/" + sample)`；path A 不作基准 |

**采用 Baseline::Compare 的原因**：
1. tgfx 渲染在部分平台（含 GPU backend、浮点 reduce、SIMD 优化）可能非严格确定性；
2. OutlineAll 模式的 path A（字体光栅化）与 path B（矢量 outline 光栅化）本质不同路径，无法严格 memcmp；
3. 项目已有完善的 `Baseline::Compare(tgfx::Surface, key)` 基础设施（基准 webp 比较 + version.json 托管），直接复用减少新增代码。RenderUtil 产出 `std::shared_ptr<tgfx::Surface>`，Baseline::Compare 直接吃（接口定义见 `test/src/utils/Baseline.h`）。

**B1 的具体比对方式**：`Baseline::Compare(surfaceB, "PAGRenderTest_Render/" + sample)` 通过即可；surfaceA 不进入 CI，仅在 Day-1 smoke（见 §18.2）验证 tgfx 渲染确定性。

**B3 的独立基准**：OutlineAll 模式因 path A 不等价于 path B，surfaceA 不再作为 surfaceB 的参照；surfaceB 独立保存 `PAGRenderTest_OutlineAll/{sample}` 基准图。

### 档 C：鲁棒性（必过）

| ID | 场景 | 断言 |
|---|---|---|
| C1 | 空 pagx | 全链路不 crash；PAGLoader 返回 `ok=true + layer=nullptr + warnings=[InflaterEmptyDocument(604)]`（空场景视为合法成功，见 §15.3 ok 契约）|
| C2 | 未 applyLayout | Baker 立即返回 error |
| C3 | 有未解析 import | Baker 立即返回 error，提示 run 'pagx resolve' |
| C4 | mask 指向不存在的 layer | warning + 该 layer 无 mask |
| C5 | 图片缺失 / data URI 损坏 | warning + paint 降级 |
| C6 | 字体全部找不到 | warning + 系统默认字体兜底 |
| C7 | Composition 引用循环 | PAGXImporter 拦截；Baker 冗余防护不爆栈 |
| C8 | 非 v2 pag 文件 | Decoder 返回 version error |
| C9 | 截断/损坏的 pag 文件 | Decoder 不越界不 crash |
| C10 | 超大文件（MB 级内嵌） | 不 OOM，可完成 |

### 档 D：非功能性（软指标，列 checklist 但不卡发布）

| ID | 指标 | 目标 |
|---|---|---|
| D1 | 10MB 级 PAGX 转出的 PAG 体积 | ≤30%（统计参考，非硬阈值） |
| D2 | PAG Decode + Inflate 耗时 / PAGX Import + applyLayout 耗时 | ≤20%（统计参考） |
| D3 | Baker / Codec / Inflater 单测覆盖率 | ≥85% |
| D4 | `include/pagx/PAGExporter.h` 通过 review | 人工 |
| D5 | `./codeformat.sh` 通过、无 lint 警告 | 自动 |

---

## 18. 测试方案

### 18.1 金字塔

```
          ┌─────────────────────┐
          │  Render Tests  (B)  │  ← 最关键，像素级
          └──────────┬──────────┘
        ┌──────────┴───────────┐
        │  End-to-End Tests    │
        └──────────┬───────────┘
     ┌───────────┴──────────────┐
     │  Integration Tests       │
     └───────────┬──────────────┘
  ┌────────────┴───────────────────┐
  │  Unit Tests  （最多）           │
  └────────────────────────────────┘
```

全部基于项目 Google Test 框架，归入 `PAGFullTest` target。

**各层工具对照**（避免 AI 错用工具；详细纪律见 §18.4bis "工具选择约束"）：

| 层 | 节 | 工具 | 断言形态 | 示例 |
|---|---|---|---|---|
| Unit | §18.4、§18.4bis | `EXPECT_EQ` / `EXPECT_NEAR` / `EXPECT_TRUE` | 字段值、字节长度、字节内容 | `EXPECT_EQ(1u, enc.length())` |
| Integration | §18.5 | 同上 + `PAGDocumentEquals` helper | PAGDocument 对象往返相等 | `EXPECT_TRUE(PAGDocumentEquals(a, b))` |
| End-to-End | §18.6 | 同上 + CLI 退出码 | 端到端状态 | `EXPECT_EQ(0, system("pagx export ..."))` |
| Render | §18.7 | `pag::Baseline::Compare(Surface, key)` | 像素 MD5 == 基准 webp | `EXPECT_TRUE(Baseline::Compare(surfaceB, k))` |
| Render-extra | §18.7bis | `ComputePSNR(a, b)` + `EXPECT_GE(psnr, 30)` | 感知相似度 | PathA vs PathB 辅助诊断 |
| Perf | §18.8 | 比率计算 + JSON 基线 | 软指标 | `size_ratio`, `load_ratio` |

**核心纪律**：`pag::Baseline::Compare` **仅在 Render 层使用**，其他层严禁使用——见 §18.4bis。

### 18.2 渲染 helper（`test/src/pag/support/RenderUtil.h`）

> 本期测试基础设施**直接复用项目已有的 `test/src/utils/Baseline.h`**（`Baseline::Compare` 的 5 个重载：`std::shared_ptr<tgfx::Surface>` / `tgfx::Bitmap` / `tgfx::Pixmap` / `std::shared_ptr<PAGSurface>` / `std::shared_ptr<ByteData>`）。RenderUtil 的设计围绕**产出 `std::shared_ptr<tgfx::Surface>`** 组织——它就是项目里既有测试的通用渲染输出形态，Baseline::Compare 直接吃。

```cpp
namespace pagx::test {

/**
 * Renders a tgfx::Layer tree to an offscreen tgfx::Surface.
 *
 * Implementation mirrors the pattern already used by test/src/PAGXTest.cpp:264-270
 * (the LayerBuilder render path). Keeping the implementation identical guarantees
 * that any Baseline::Compare baseline captured for LayerBuilder path A can be reused
 * for PAG v2 path B without re-capture.
 */
std::shared_ptr<tgfx::Surface> RenderLayerToSurface(
    tgfx::Context* context,
    std::shared_ptr<tgfx::Layer> layer,
    int canvasWidth, int canvasHeight,
    float scale = 1.0f);

}
```

**实现模板**（严格对齐 `test/src/PAGXTest.cpp:264-270`）：

```cpp
std::shared_ptr<tgfx::Surface> RenderLayerToSurface(
    tgfx::Context* context,
    std::shared_ptr<tgfx::Layer> layer,
    int canvasWidth, int canvasHeight,
    float scale) {
  auto surface = tgfx::Surface::Make(context, canvasWidth, canvasHeight);
  if (surface == nullptr || layer == nullptr) {
    return nullptr;
  }
  tgfx::DisplayList displayList;              // 栈对象，不是 shared_ptr
  auto container = tgfx::Layer::Make();       // 包裹一层以承载 scale
  if (scale != 1.0f) {
    container->setMatrix(tgfx::Matrix::MakeScale(scale, scale));
  }
  container->addChild(layer);
  displayList.root()->addChild(container);    // root() 返回 Layer*，再 addChild
  displayList.render(surface.get(), false);   // render(Surface*, clearAll)
  return surface;
}
```

**关键 API 备忘**（避免 AI 凭记忆写错）：
- `tgfx::DisplayList` 是**栈对象**（无 `std::shared_ptr<DisplayList>` 等概念）；
- 没有 `setRoot()` API——通过 `displayList.root()->addChild(...)` 挂载子树（`root()` 返回 `tgfx::Layer*`）；
- 渲染方法是 `render(tgfx::Surface*, bool clearAll)`，**不是** `draw(tgfx::Canvas*)`；
- 同一个 `tgfx::Layer` 不应被多个 DisplayList 同时持有——每次 `RenderLayerToSurface` 调用独立构造 DisplayList + 独立 container；
- `tgfx::Context*` 由上层测试固件提供（项目已有 `test/src/utils/TestUtils.h` 的 context 获取接口，开工阶段 9 实现 RenderUtil 时查取）。

**测试断言统一走 Baseline::Compare**：

```cpp
auto surfaceB = RenderLayerToSurface(context, rootB, width, height);
EXPECT_TRUE(Baseline::Compare(surfaceB, "PAGRenderTest_Render/" + sample));
```

**Day-1 smoke test**（开工第 1 天 — 本地运行；**不产出基准图、不进入常态 CI、通过后从代码删除**）：

```cpp
TEST(PAGRenderSmoke, LayerBuilderIsDeterministic) {
  auto doc = PAGXImporter::FromFile("spec/samples/<pick_one>.pagx");
  doc->applyLayout();

  auto r1 = LayerBuilder::Build(doc.get());
  auto s1 = RenderLayerToSurface(context, r1, doc->width, doc->height);
  tgfx::Bitmap bitmap1(s1->width(), s1->height(), false, false);
  tgfx::Pixmap pixmap1(bitmap1);
  ASSERT_TRUE(s1->readPixels(pixmap1.info(), pixmap1.writablePixels()));

  auto r2 = LayerBuilder::Build(doc.get());
  auto s2 = RenderLayerToSurface(context, r2, doc->width, doc->height);
  tgfx::Bitmap bitmap2(s2->width(), s2->height(), false, false);
  tgfx::Pixmap pixmap2(bitmap2);
  ASSERT_TRUE(s2->readPixels(pixmap2.info(), pixmap2.writablePixels()));

  // 对比 readback 像素字节：验证 tgfx 对同一 tgfx::Layer 树渲染的确定性
  ASSERT_EQ(pixmap1.rowBytes() * pixmap1.height(),
            pixmap2.rowBytes() * pixmap2.height());
  EXPECT_EQ(0, std::memcmp(pixmap1.pixels(), pixmap2.pixels(),
                           pixmap1.rowBytes() * pixmap1.height()))
      << "tgfx rendering is not deterministic for identical inputs; "
      << "Baseline::Compare tolerance strategy needs revisiting.";
}
```

此 smoke 验证 tgfx 渲染对**完全相同的 tgfx::Layer 树**是否产出相同像素。若通过：下游所有 `Baseline::Compare` 的差异只能来自数据路径（Baker/Codec/Inflater），与渲染后端无关。若失败：先调研 tgfx 渲染确定性问题，或退而采用 `Baseline::Compare` 的容差模式。

**Day-1 smoke 的唯一定义点**。§17 B 档与 §18.7 仅引用此处，不再重复表述。

**Baseline 更新纪律**：项目 `.codebuddy/rules/Test.md` 已明确 —— 严禁手动执行 `accept_baseline.sh` / `UpdateBaseline` target / 修改 `version.json`；接受基准变更的**唯一**方式是用户主动执行 `/accept-baseline` 斜杠命令。

### 18.3 样本枚举（`support/SampleEnumerator.h`）

```cpp
std::vector<PAGXSample> EnumerateSpecSamples();
```

- 递归扫描 `spec/samples/*.pagx`；
- 每个样本做一次轻量文本扫描（grep `<Text` / `<TextBox`）标记 `hasText`；
- OutlineAll 测试仅对 `hasText == true` 的样本启用。

### 18.4 Layer 1 单元测试（约 110 条，分 12 文件）

详见上一轮"测试用例清单"的表格，此处不赘述。每个 Baker 子模块对应一个 test 文件，test 命名对齐 LayerBuilder 行号段。

### 18.4bis 压缩机制专项测试（v2.0 新增，共约 25 条）

> **目的**：v2.0 引入 7 个压缩/扩展机制（A1/A3/B1/B2/C1/C2/D2），错误码路径已由附录 G.6 矩阵覆盖，但**成功路径的往返正确性 + 边界行为**需要独立测试。**这些测试放在 `test/src/pag/codec/` 目录下**，与 Baker 单测并列，不依赖 PAGX 样本，纯 PAGDocument 构造驱动。

| 文件 | Test 组 | 断言要点 |
|---|---|---|
| `ColorCodecTest.cpp` | `ColorCodec.RoundTripAllU8Values` | 0-255 × 4 通道往返；`U8ToFloat(FloatToU8(v))` 量化误差 ≤ 1/255 |
| | `ColorCodec.FloatToU8Boundary` | NaN → 0，-0.1f → 0，1.5f → 255，Inf → 255；边界值 0.5f → 128 |
| | `ColorCodec.RoundTripViaStream` | `EncodeStream → DecodeStream` 往返，bytes == 4 |
| `PropertyCodecTest.cpp` | `PropertyCodec.IsDefaultSkipValue` | `MakeProp(1.0f)` with defaultValue=1.0f → 字节流仅 1 B |
| | `PropertyCodec.IsDefaultRoundTrip` | 非默认值往返后 `Property<T>::value` 一致 |
| | `PropertyCodec.UnknownEncodingSkipsTag` | 写入 encoding=0xF（非法），Reader seek 到 enclosingTagEnd + warn `UnknownPropertyEncoding` |
| | `PropertyCodec.HasExtBitConsumed` | 写 hasExt=1 + 1 B extHeader，Reader 消费 extHeader 不错位 |
| | `PropertyCodec.ReservedBitsIgnored` | bit6-7 = 0b11 时 Reader 不报错，语义视为 Constant |
| `MatrixCodecTest.cpp` | `MatrixCodec.IdentityElision` | `Matrix::I()` 写出 1 byte（header=0x01） |
| | `MatrixCodec.TranslateOnlyElision` | scaleX=1, skewX=0, skewY=0, scaleY=1 → 9 bytes |
| | `MatrixCodec.FullMatrix` | 任意 scale+skew → 25 bytes |
| | `MatrixCodec.Matrix3DIdentity` | `Matrix3D::I()` 写出 1 byte；非 I → 65 bytes |
| | `MatrixCodec.RoundTrip` | 随机生成 100 个 Matrix，encode→decode 后 `operator==` 成立 |
| `PathCodecTest.cpp` | `PathCodec.QuantizedRoundTripSimple` | Move+Line+Close，format=1，量化后 decode 回来 verb 序列一致，坐标误差 ≤ precision |
| | `PathCodec.QuantizedRoundTripConic` | 含 Conic verb 时 conicWeights 数组独立编码正确 |
| | `PathCodec.FallbackToFloatFormat` | verbCount=2（极短 path）自动选 format=0 |
| | `PathCodec.LargeCoordFallback` | 坐标绝对值 > 2^23 自动选 format=0 |
| | `PathCodec.PrecisionSmokeAt4` | precisionLog2=4（0.0625 px）的量化误差在 1080p canvas 下 < 0.01% |
| `ShapeStyleCodecTest.cpp` | `ShapeStyleCodec.SolidColorBranchSize` | SolidColor 的 ShapeStyleData 编码后字节数 ≤ 10（innerLength + 公共头 + color Property） |
| | `ShapeStyleCodec.GradientBranchRoundTrip` | Linear/Radial/Conic/Diamond 各自分支往返 |
| | `ShapeStyleCodec.UnknownSourceTypeSkipped` | 写入 sourceType=99，Reader 降级为 SolidColor 默认 + `InvalidEnumValue` warning |
| | `ShapeStyleCodec.InnerLengthSkipsNewField` | 模拟"未来版本追加字段"：encode 时手动在分支末尾多写 4 byte，Reader 靠 innerLength 跳过不报错 |
| `GlyphRunBlobCodecTest.cpp` | `GlyphRunBlob.LayoutRunRoundTrip` | 构造 10 个 LayoutGlyph（半数带 xform），量化往返后 position/xform 误差 ≤ precision |
| | `GlyphRunBlob.ClassicGlyphRunRoundTrip` | 构造 50 个 ClassicGlyph（全 anchor=0, scale=1），编码后 anchor/scale 数组应该占极少字节（动态位宽 ≈ 1 bit/值） |
| | `GlyphRunBlob.EmptyGlyphCount` | glyphCount=0 不应崩溃，各 List 写出 numBits header 即可 |
| `LayerBitFlagsCodecTest.cpp` | `BitFlags.LayerFlagsRoundTrip` | `layerFlags` 5 位任意组合往返，未设置位应为 0 |
| | `BitFlags.ModifierFlagsConditionalRead` | `modifierFlags.bit0=1` 时才读 `fillColor`，bit0=0 时 Reader 不读；字节流位置正确 |
| | `BitFlags.PathFlagsReservedBitsIgnored` | bit3-7 = 0b11111 时 Reader 忽略不报错 |
| `VersionedTagTest.cpp` | `VersionedTag.UnknownTagCodeSkipped` | 写入 `TagCode=301`（版本化变体段未分配值），Reader skip 该 Tag + warn `UnknownTagCode` |
| | `VersionedTag.OldReaderSkipsNewTagInMiddle` | 合法 Tag 序列 `[FileHeader, TagCode=301 未知, LayerBlock]`，老 Reader 跳过中间未知 Tag 后继续读 LayerBlock |

**执行约束**：
- 所有测试使用 GoogleTest `TEST` 宏；
- 不依赖 spec/samples；构造最小 PAGDocument / 最小 tgfx::Path / 最小 ShapeStyleData 驱动；
- 每组测试 ≤ 50ms，保证 `PAGFullTest` 整体运行时间可控；
- 禁止使用 GPU 渲染，纯字节流验证（若误用 Surface 会撞进 Layer 4 测试层级）。
- **严禁使用 `pag::Baseline::Compare`**——见下方"工具选择约束"。

**工具选择约束（Layer 1 专属，强制纪律）**：

> Layer 1 是**字节流 / 对象字段 / 数值**验证，不是图像验证。
> `pag::Baseline::Compare` 是专为 **Layer 4 渲染输出**设计的 MD5 精确匹配基础设施（见 `test/src/utils/Baseline.cpp:150-178`，其内部逻辑为 `DumpMD5(pixels)` 后比对 `test/baseline/version.json` 托管的基准条目）。若 Layer 1 测试使用 `Baseline::Compare`，有三条硬问题：
>
> 1. **语义错位**——Layer 1 没有 Surface/Bitmap/Pixmap，强行造图只为走 MD5 通道是反模式；
> 2. **基准图膨胀**——每条测试对应一个 `test/baseline/{key}_base.webp` + `version.json` 条目，入 git 仓库体积失控；
> 3. **失败信号丢失**——MD5 不匹配只告诉"整体不等"，不告诉"哪个字段错了/量化超限多少"，排查成本高。

| 测试层 | 使用工具 | 断言什么 |
|---|---|---|
| **Layer 1（§18.4、§18.4bis）** | `EXPECT_EQ` / `EXPECT_NEAR` / `EXPECT_TRUE` | 字段值、字节长度、字节内容、PAGDocument 对象相等 |
| **Layer 2（§18.5）** | 同上 + 手写字节流对比辅助 | PAGDocument 往返对象语义相等 |
| **Layer 3（§18.6）** | 同上 + CLI 退出码、文件存在性 | 端到端流程状态 |
| **Layer 4（§18.7）** | `pag::Baseline::Compare(surfaceB, key)` | 渲染像素 MD5 == 基准 webp MD5 |
| **Layer 4-extra（§18.7bis）** | `ComputePSNR(pixelsA, pixelsB)` + `EXPECT_GE(psnr, 30.0)` | PathA vs PathB 感知级相似度 |

**Layer 1 断言模板**（AI 落代码时**照此模板**，不自创）：

```cpp
// ✅ 正例 1：字节流长度 + 内容（不产生 Surface）
TEST(MatrixCodec, IdentityElision) {
  pag::EncodeStream enc(nullptr);
  WriteMatrix(&enc, tgfx::Matrix::I());
  EXPECT_EQ(1u, enc.length());
  EXPECT_EQ(0x01, enc.data()[0]);   // isIdentity bit
}

// ✅ 正例 2:数值容差（量化机制专用）
TEST(ColorCodec, RoundTripAllU8Values) {
  tgfx::Color in{0.5f, 0.25f, 0.75f, 1.0f};
  pag::EncodeStream enc(nullptr);
  WriteColor(&enc, in);
  pag::DecodeStream dec(enc.data(), enc.length());
  auto out = ReadColor(&dec);
  EXPECT_NEAR(in.red,   out.red,   1.0f / 255.0f);  // u8 量化最大误差
  EXPECT_NEAR(in.green, out.green, 1.0f / 255.0f);
  EXPECT_NEAR(in.blue,  out.blue,  1.0f / 255.0f);
  EXPECT_NEAR(in.alpha, out.alpha, 1.0f / 255.0f);
}

// ✅ 正例 3：PAGDocument 对象往返
TEST(RoundTrip, VectorLayerSimple) {
  pagx::pag::PAGDocument doc1 = BuildMinimalVectorPAGDocument();  // helper
  auto bytes = pagx::pag::Codec::Encode(doc1);
  auto decoded = pagx::pag::Codec::Decode(bytes.data(), bytes.size());
  ASSERT_NE(decoded.doc, nullptr);
  EXPECT_TRUE(PAGDocumentEquals(doc1, *decoded.doc));  // deep field-by-field
}

// ❌ 反例：禁止把字节流塞进假 Pixmap 走 Baseline::Compare
//    Baseline::Compare 是为 Surface→Pixels→MD5→基准 webp 比对设计的，
//    不是给字节流用的通用 MD5 比较器。
```

**作用**：
1. **锁死压缩机制的往返正确性**——"encode 出来的字节 decode 回来必须等价"是编码实现的硬门槛，这层比渲染测试快 1000 倍且无 GPU 噪声；
2. **锁死扩展机制的兼容行为**——`UnknownEncoding / UnknownSourceType / UnknownTagCode / InnerLengthSkip` 这些"别人先写了我还能读"的场景必须可测；
3. **为基准图测试提供前置防线**——如果压缩机制本身错了，渲染层测试再好也只能捕捉症状，不能定位病因。

### 18.5 Layer 2 集成测试

| 文件 | Test 条数下限 |
|---|---|
| RoundTripTest.cpp | 9 |
| PAGDocumentParityTest.cpp | 2 |
| InflaterParityTest.cpp | 5 |
| VersionRejectTest.cpp | 6 |
| TruncatedDecodeTest.cpp | 8 |

### 18.6 Layer 3 端到端测试

| 文件 | Test 条数下限 |
|---|---|
| EndToEndTest.cpp | 6（含 4 条 CLI） |
| EdgeCasesTest.cpp | 11（C1-C10 + Large） |

### 18.7 Layer 4 渲染一致性测试（最关键）

`RenderEquivalenceTest.cpp`：参数化遍历 spec/samples，每样本产出：

```cpp
TEST_P(PAGRenderEquivalenceTest, Render_Baseline) {
  auto sample = GetParam();
  auto doc = PAGXImporter::FromFile("spec/samples/" + sample + ".pagx");
  ASSERT_NE(doc, nullptr);
  doc->applyLayout();

  // Path B: via PAG v2 (fontMode=Render)
  PAGExporter::Options opts;
  auto r = PAGExporter::ToBytes(*doc, opts);
  ASSERT_TRUE(r.ok);

  auto decoded = pagx::pag::Codec::Decode(r.bytes.data(), r.bytes.size());
  ASSERT_NE(decoded.doc, nullptr);
  auto inflated = pagx::pag::LayerInflater::Inflate(*decoded.doc);
  ASSERT_NE(inflated.layer, nullptr);
  auto surfaceB = RenderLayerToSurface(context, inflated.layer, doc->width, doc->height);

  // 直接喂 tgfx::Surface 给 Baseline::Compare（接口见 test/src/utils/Baseline.h）
  EXPECT_TRUE(Baseline::Compare(surfaceB, "PAGRenderTest_Render/" + sample));
}

TEST_P(PAGRenderEquivalenceTest, OutlineAll_Baseline) {
  auto sample = GetParam();
  if (!SampleHasText(sample)) GTEST_SKIP();
  auto doc = PAGXImporter::FromFile("spec/samples/" + sample + ".pagx");
  ASSERT_NE(doc, nullptr);
  doc->applyLayout();

  PAGExporter::Options opts;
  opts.fontMode = PAGExporter::Options::FontMode::OutlineAll;
  auto r = PAGExporter::ToBytes(*doc, opts);
  ASSERT_TRUE(r.ok);
  auto decoded = pagx::pag::Codec::Decode(r.bytes.data(), r.bytes.size());
  auto inflated = pagx::pag::LayerInflater::Inflate(*decoded.doc);
  auto surfaceB = RenderLayerToSurface(context, inflated.layer, doc->width, doc->height);

  EXPECT_TRUE(Baseline::Compare(surfaceB, "PAGRenderTest_OutlineAll/" + sample));
}
```

**基准图 key 命名**（统一采用"独立 folder"方案）：
- Render 模式：`PAGRenderTest_Render/{sampleName}` → `test/baseline/PAGRenderTest_Render/{sampleName}_base.webp`
- OutlineAll 模式：`PAGRenderTest_OutlineAll/{sampleName}` → 独立基准

**Day-1 smoke（唯一定义见 §18.2）**：Day-1 验证 tgfx 渲染对同一 tgfx::Layer 树的确定性（pixel-to-pixel 直接比对，不涉基准图）。通过后从代码删除，不进入常态 CI。

### 18.7bis Layer 4-extra：PathA vs PathB 交叉渲染（辅助诊断，非阻塞）

> **动机**：§18.7 的基准图测试是主门槛（surfaceB 独立对基准图），但**基准图可能随时间漂移**（例如 Baseline::Compare 容差被人工调宽后失去灵敏度）。本节新增"直接对比 LayerBuilder 原生渲染（Path A）与 PAG v2 链路渲染（Path B）"的辅助测试作为**第二道防线**，快速感知两条路径的语义漂移。

#### 为什么**不直接**作为渲染一致性的主判据？

本测试**必定**会有可预期的微小像素差异，来源有三：

1. **v2.0 量化损失（不可消除）**：
   - A1 Color u8×4：每通道误差 ≤ 1/255（~0.39%）
   - B1 Path 量化 0.0625 px：叠加到 GPU 光栅化后 ≤ 亚像素级
   - C2 Glyph 量化 0.0625 px：同上
2. **GPU 非确定性（见 §18.2 Day-1 smoke）**：
   - 两次构造的 tgfx::Layer 树对象身份不同，可能命中不同 filter/mask 临时 surface 路径
3. **tgfx 渲染管线中间状态**：
   - mask/filter 的中间 surface 分配顺序可能影响抗锯齿

因此 **PathA vs PathB 的直接对比不适合作为 CI 硬门槛**（容差阈值难以稳定），但非常适合作为：
- 开发期快速自检
- 基准图漂移预警
- Bug 定位辅助（PathA 正常、PathB 异常 → 问题必在 v2 链路）

#### 测试实现

```cpp
// test/src/pag/RenderCrossCheckTest.cpp
TEST_P(PAGRenderCrossCheck, PathA_vs_PathB) {
  auto sample = GetParam();
  auto doc = PAGXImporter::FromFile("spec/samples/" + sample + ".pagx");
  ASSERT_NE(doc, nullptr);
  doc->applyLayout();

  // Path A: LayerBuilder 原生渲染
  auto rootA = LayerBuilder::Build(doc.get());
  auto surfaceA = RenderLayerToSurface(context, rootA, doc->width, doc->height);

  // Path B: PAG v2 全链路
  PAGExporter::Options opts;
  auto r = PAGExporter::ToBytes(*doc, opts);
  ASSERT_TRUE(r.ok);
  auto decoded = pagx::pag::Codec::Decode(r.bytes.data(), r.bytes.size());
  auto inflated = pagx::pag::LayerInflater::Inflate(*decoded.doc);
  auto surfaceB = RenderLayerToSurface(context, inflated.layer, doc->width, doc->height);

  // 读 readback 像素
  auto pixelsA = ReadSurfacePixels(surfaceA);
  auto pixelsB = ReadSurfacePixels(surfaceB);
  ASSERT_EQ(pixelsA.size(), pixelsB.size());

  // 度量 1（宽松）：PSNR，阈值 30 dB（对应平均 ~3/255 误差，感知不可见）
  double psnr = ComputePSNR(pixelsA, pixelsB, doc->width, doc->height);
  EXPECT_GE(psnr, 30.0)
      << "PathA/PathB semantic drift: PSNR below warning threshold. "
      << "Sample=" << sample
      << ". 可能原因：(1) v2 压缩机制 bug；(2) Baker 映射漏；"
      << "(3) 新增压缩超出预算。请先看 surfaceB 独立基准图是否还通过。";

  // 度量 2（参考）：像素差异直方图（仅在 PSNR 失败时输出）
  if (psnr < 30.0) {
    DumpPixelDiffHistogram(pixelsA, pixelsB, sample);
  }
}

INSTANTIATE_TEST_SUITE_P(AllSamples, PAGRenderCrossCheck,
                         ::testing::ValuesIn(EnumerateSamples()));
```

#### 配套工具

```cpp
// test/src/pag/support/PixelDiff.h
namespace pagx::test {

std::vector<uint8_t> ReadSurfacePixels(std::shared_ptr<tgfx::Surface> surface);

// PSNR = 20 * log10(255 / sqrt(MSE))
// 30 dB ≈ 平均误差 8/255 (3.1%)；40 dB ≈ 2.55/255 (1%)
double ComputePSNR(const std::vector<uint8_t>& a,
                   const std::vector<uint8_t>& b,
                   int width, int height);

// 写入 test/out/PixelDiff/{sample}_histogram.txt
// 含：最大差值、差值直方图（0, 1-3, 4-10, 11-50, >50 各通道像素数）
void DumpPixelDiffHistogram(const std::vector<uint8_t>& a,
                            const std::vector<uint8_t>& b,
                            const std::string& sample);

}
```

#### 容差选择依据

| 阈值 | 对应平均误差 | 适用场景 |
|---|---|---|
| **PSNR ≥ 40 dB** | ≤ 1% | 理想情况；若所有样本都能过说明量化影响极低 |
| **PSNR ≥ 30 dB**（本测试选用） | ≤ 3.1% | 实战可用阈值；覆盖 8-bit Color 量化 + 0.0625px 几何量化叠加 |
| **PSNR ≥ 20 dB** | ≤ 10% | 过于宽松，失去预警价值 |

**为什么选 30 dB 而非 40 dB**：v2.0 叠加压缩后，某些极端样本（大量小尺寸 Conic Gradient + 复杂 Path）的量化误差在 GPU 放大后可能短暂跌破 35 dB；30 dB 是兼顾灵敏度和稳定性的折衷。若实测有样本稳定低于 30 dB，**不是自动放宽阈值**，而是走 P2-1 排查量化机制是否存在实现 bug。

#### 与 §18.7 基准图测试的协同

```
    ┌── Layer 4: §18.7 基准图测试（硬门槛）
    │      surfaceB  vs  baseline.webp     ──► CI pass/fail
    │
    └── Layer 4-extra: §18.7bis 交叉渲染（辅助）
           surfaceA  vs  surfaceB (PSNR)   ──► 警戒信号 + 诊断辅助
```

**失败矩阵判读**：

| §18.7 基准图 | §18.7bis 交叉 | 判读 |
|---|---|---|
| ✅ | ✅ | 一切正常 |
| ✅ | ❌ | 基准图可能被意外接受了错误版本；复查近期 `/accept-baseline` 记录 |
| ❌ | ✅ | Path B 渲染结果与 Path A 一致，但基准图失效（最常见场景：tgfx 升级后渲染微变）；走 `/accept-baseline` 审阅更新基准 |
| ❌ | ❌ | **Path B 存在真实 bug**（Baker/Codec/Inflater 某处有映射错误）；优先看 Path B 的 Baker 日志、再看 PAGDocument dump、最后看字节流 |

#### 执行策略

- **非阻塞**：CI 中以 warning 形式报告，不 block PR merge；
- **按需运行**：`tools/run_cross_check.sh`，本地验证/新样本接入时手动跑；
- **不维护基准**：不存储 surfaceA 或 surfaceB 作基准，只比两者的相对差异；
- **计入档 D**（见 §17）：作为"非功能性软指标"的一部分，列 checklist 不卡发布。

### 18.8 Layer 5 性能测试（非阻塞）

`PerformanceTest.cpp` 对 spec/samples 每样本测：
- **size_ratio** = `pag_size_bytes / pagx_size_bytes`（核心指标）
- **load_ratio** = `pag_load_ms / pagx_load_ms`（核心指标）
- 绝对时间（bake_ms、encode_ms、decode_ms、pagx_load_ms、pag_load_ms）仅作 informational

**基线**：`test/perf/baseline.json`（入 git）——**只记比率和相对百分比**，不记绝对时间，避免环境噪音。
```json
{
  "format_version": 1,
  "notes": "Ratios are environment-insensitive; absolute times are reference-only.",
  "samples": {
    "app_icon": {
      "size_ratio": 0.300,
      "load_ratio": 0.144,
      "reference_abs_times": {
        "pagx_load_ms": 12.5,
        "pag_load_ms": 1.8,
        "bake_ms": 8.2,
        "encode_ms": 2.1,
        "decode_ms": 1.2,
        "platform": "Darwin arm64 M1 Max 10c/32g"
      }
    }
  }
}
```

**行为**：
- 首次运行时若 baseline 缺失：PASS + 打印 "baseline created"，**本地生成文件**；开发者必须显式 commit 才进仓库；
- CI 环境若 baseline 缺失：**FAIL**（防止漏提）；
- 后续对比 size_ratio / load_ratio 与基线，超过 +20% 时打印 WARNING（非 FAIL）；
- 显式设 `PAGX_PERF_UPDATE_BASELINE=1` 才重写基线。

### 18.9 测试数据来源

- **spec/samples/ 现有 6 个样本**（C.1-C.6）首期覆盖；
- **用户后续补 77 个样本**时 `EnumerateSpecSamples()` 自动发现，参数化测试自动扩展；
- **不新增 test/assets** 目录（按用户决策 4）；
- 单元测试用 `PAGXBuilder` 在代码里手构 DOM，不依赖磁盘样本。

### 18.10 失败诊断要求

渲染一致性测试 `Baseline::Compare` 失败时（基准图不匹配且非接受变更情形）：
- Baseline 机制会自动把当前产出保存到 `test/out/PAGRenderTest_Render/{sample}.webp` / `test/out/PAGRenderTest_OutlineAll/{sample}.webp`（参考 `test/src/utils/Baseline.cpp` 的 OUT_ROOT 机制）；
- gtest message 输出 sample 名 + 画布 width/height + surface 路径；
- 人工 diff：对比 `test/baseline/{key}_base.webp` 与 `test/out/{key}.webp` 即可；
- 确认视觉变更合理时，用户执行 `/accept-baseline` 斜杠命令接受（项目规则，见 `.codebuddy/rules/Test.md`）。

### 18.11 基准更新纪律

**严禁**手动执行 `accept_baseline.sh` / `UpdateBaseline` / 修改 `version.json`。接受基准变更的**唯一方式**是用户主动执行 `/accept-baseline` 斜杠命令。

### 18.12 一键验收脚本（`tools/verify_pag_conversion.sh`）

```bash
#!/usr/bin/env bash
set -e
./codeformat.sh 2>/dev/null || true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest -j
BIN="./cmake-build-debug/PAGFullTest"

$BIN --gtest_filter="ValueCodec.*:Property.*:TagHeader.*:BakeContext.*:*Baker.*"
$BIN --gtest_filter="RoundTrip.*:*Parity*:VersionReject.*:Truncate.*"
$BIN --gtest_filter="E2E.*:EdgeCase.*:PAGRenderEquivalenceTest.*"
$BIN --gtest_filter="PAGPerformance.*" || true
```

### 18.13 覆盖率脚本（`tools/coverage.sh`）

用 clang `-fprofile-instr-generate -fcoverage-mapping`；独立 build 目录 `cmake-build-coverage`；产出 HTML 到 `coverage/html/`（gitignore）；PR 模板要求附覆盖率摘要。

---

## 19. 工作拆分与 TDD 执行顺序

每阶段产品代码与对应测试**同批次**产出（TDD 严格纪律）。

| # | 产品代码 | 同批测试 | 交付判定 |
|---|---|---|---|
| 0 | `include/pagx/Diagnostic.h` / `src/pagx/Diagnostic.cpp` / `src/pagx/pag/ErrorCode.h` (alias) / `src/pagx/pag/DiagBuild.h` | `DiagnosticTest`（FormatDiagnostic 格式 + MakeDecodeDiag / MakeDiag 往返 + ABI-appended 码不丢） | 对外头齐备，作为后续阶段的 include 基础 |
| 1 | `ValueCodec.h` / `PropertyEncoding.h` / `TagCode.h` / TagHeader util | `ValueCodecTest` / `PropertyEncodingTest` / `TagHeaderTest` | 三文件全绿 |
| 2 | `PAGDocument.h` / `BakeContext.h+cpp` / `ResourceBaker.cpp` | `BakeContextTest` / `ResourceBakerTest` | 全绿 |
| 3 | `LayerBaker.cpp`（通用字段）+ 测试工具 `PAGXBuilder` | `LayerBakerTest` + `BakerEdgeCasesTest`（Baker fatal 段 100-105 + 207） | 全绿 |
| 4 | `Codec.cpp` 基础 Tag（FileHeader/Composition/LayerBlock） | `RoundTripTest` 前半 + `VersionRejectTest` + `TruncatedDecodeTest` | 全绿 |
| 5 | `VectorBaker.cpp` + `ElementTags.cpp` | `VectorBakerTest` + `RoundTripTest` 剩余 | 全绿 |
| 6 | PaintBaker 代码（融入 VectorBaker 文件） | `PaintBakerTest` | 全绿 |
| 7 | `StyleFilterBaker.cpp` + `FilterTags.cpp` + `StyleTags.cpp` | `StyleFilterBakerTest` | 全绿 |
| 8 | `TextBaker.cpp` + GlyphRun 序列化 | `TextBakerTest` | 全绿 |
| 9 | `LayerInflater.cpp` + `support/RenderUtil.cpp` | `InflaterParityTest` + `PAGDocumentParityTest` | 全绿 |
| 10 | `PAGExporter.cpp`（导出对外 API） | — | API review 过 |
| 10.5 | `include/pagx/PAGLoader.h` / `src/pagx/pag/PAGLoader.cpp`（加载对外 API） | `PAGLoaderTest`（LoadFromBytes 成功路径 / `PAGLoader.LoadFromFile_Missing` 触发 `FileReadFailed=307` / Inflater warnings 被 Result.warnings 收集） | 对外加载 API review 过 + 全绿 |
| 11 | `CommandExport.cpp` 扩展 | `EndToEndTest` + `EdgeCasesTest` | 全绿 |
| 12 | — | `RenderEquivalenceTest` | Render 模式 Baseline::Compare 全绿；OutlineAll 模式 Baseline::Compare 全绿（独立基准） |
| 13 | —（取消 v1 改动，v1 已能 graceful reject 0x02）| — | — |
| 14 | — | `PerformanceTest` + 首次生成 `test/perf/baseline.json` | 基线入 git |
| 15 | `tools/coverage.sh` | — | 覆盖率 ≥85%，报告附 PR |

**阶段门槛**：
- 阶段 0 全绿 → 解锁后续所有阶段（Diagnostic 是所有模块的 include 基础）
- 阶段 1–3 全绿 → 可 code review
- 阶段 4–8 全绿 → 可合入 feature 分支
- 阶段 9–11 全绿 → 可开 PR（含 10.5 PAGLoader）
- 阶段 12–15 全绿 + D 类软指标达标 → 可合入 main

---

## 20. 附录 A：Baker / Inflater / LayerBuilder 行号映射索引

所有行号基于 `src/renderer/LayerBuilder.cpp`（版权 2026 Tencent）。

| 模块 | LayerBuilder 行号 | Baker 文件/函数 | Inflater 函数 |
|---|---|---|---|
| 顶层 build | 143-163 | `Baker::Bake` | `LayerInflater::Inflate` |
| Layer 分发 | 167-202 | `LayerBaker::bake` | `inflateLayer` |
| Composition | 205-216 | `LayerBaker`（同文件） | `inflateComposition` |
| VectorLayer | 220-231 | `VectorBaker::bakeVectorLayer` | `inflateVectorLayer` |
| Element 派发 | 263-307 | `VectorBaker::bakeVectorElement` | `inflateVectorElement` |
| Rectangle | 310-317 | `bakeRectangle` | `inflateRectangle` |
| Ellipse | 320-326 | `bakeEllipse` | `inflateEllipse` |
| Polystar | 329-344 | `bakePolystar` | `inflatePolystar` |
| Path | 361-368 | `bakePath` | `inflatePath` |
| Text（独立） | 371-385, 237-243 | `TextBaker::bakeText` | `inflateText` |
| Fill | 388-410 | `bakeFill` | `inflateFill` |
| Stroke | 413-446 | `bakeStroke` | `inflateStroke` |
| ColorSource 派发 | 449-477 | `PaintBaker::bakeColorSource` | `inflateColorSource` |
| LinearGradient | 510-518 | `bakeLinearGradient` | `inflateLinearGradient` |
| RadialGradient | 520-527 | `bakeRadialGradient` | `inflateRadialGradient` |
| ConicGradient | 528-535 | `bakeConicGradient` | `inflateConicGradient` |
| DiamondGradient | 537-544 | `bakeDiamondGradient` | `inflateDiamondGradient` |
| ImagePattern | 545-576 | `bakeImagePattern` | `inflateImagePattern` |
| Gradient stops 兜底 | 487-500 | `ExtractGradientStops` 复刻 | 同 |
| TrimPath | 578-587 | `bakeTrimPath` | `inflateTrimPath` |
| TextPath | 589-607 | `TextBaker::bakeTextPath` | `inflateTextPath` |
| RoundCorner | 609-613 | `bakeRoundCorner` | `inflateRoundCorner` |
| MergePath | 615-621 | `bakeMergePath` | `inflateMergePath` |
| Repeater | 623-635 | `bakeRepeater` | `inflateRepeater` |
| TextModifier | 637-684 | `VectorBaker::bakeTextModifier` | `inflateTextModifier` |
| RangeSelector | 666-678 | 嵌 `bakeTextModifier` | 嵌 `inflateTextModifier` |
| Group | 686-734 | `bakeGroup` | `inflateGroup` |
| TextBox | 250-260, 298-305 | `TextBaker::bakeTextBox` | `inflateTextBoxAsGroup` |
| applyLayerAttributes | 736-800 | `LayerBaker::bakeCommon` | `applyLayerAttributes` |
| LayerStyle 派发 | 803-840 | `StyleFilterBaker::bakeStyle` | `inflateStyle` |
| LayerFilter 派发 | 842-875 | `StyleFilterBaker::bakeFilter` | `inflateFilter` |
| Mask 两趟 | 149-160, 189-191 | Baker 第二趟 | Inflater 第二趟 |
| 资源索引化 | —（LayerBuilder 现场解码） | `ResourceBaker`（pre-pass） | `InflaterContext::buildImages/Fonts` |

---

## 21. 附录 B：开工前待确认事项（v1.1 更新）

以下待确认项已在 v1.1 敲定：

1. **GlyphRun 序列化最小字段集**：已摸清，定稿见附录 C 的 `GlyphRunBlob`。来源 `src/pagx/TextLayout.h:41-53` + `include/pagx/nodes/GlyphRun.h:35-117` + `src/renderer/GlyphRunRenderer.cpp:159-314`。
2. **ColorMatrix 20-float 特化**：见附录 D 的 `WriteValue<std::array<float, 20>>` 规范。
3. **OutlineAll 模式比对策略**：定案——**不走 memcmp**；Render 和 OutlineAll 各自走 `Baseline::Compare` + 独立基准图 folder（见 §17 B 档 + §18.7）。
4. **pag v1 播放器版本拒绝**：**无需改动 v1**。`src/codec/Codec.cpp::ReadBodyBytes` line 196 的 `if (version > KnownVersion)` 已能 graceful reject 0x02 为 "Invalid PAG file header"。
5. **样本覆盖面补齐**：用户后续补 77 样本时附"特性覆盖矩阵"便于 review。
6. **tgfx::Path 序列化**：tgfx 无官方 serialize API；Codec 侧按附录 D 的 `WriteValue<tgfx::Path>` 规范走 verb 迭代自行序列化。
7. **bit-exact 渲染**：Day-1 smoke（§18.2）做 pixel-to-pixel 直接比对，验证 tgfx 对相同输入的渲染确定性；通过后删除 smoke。

---

## 22. 附录 C：PAGDocument 完整 C++ 定义

> 本附录是 PAGDocument 的权威来源。§5 是概览，以下是完整字段表——AI 编码期间按此附录落 `PAGDocument.h`。所有 `Property<T>` 含义见 §5.1；所有 enum 映射见附录 E；所有字节布局见附录 D。

### C.1 通用类型别名

```cpp
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include "pag/file.h"              // 复用 pag::Ratio（int32_t numerator, uint32_t denominator）
#include "tgfx/core/Path.h"
#include "tgfx/core/Color.h"       // tgfx::Color (4 f32 RGBA)
#include "tgfx/core/Point.h"
#include "tgfx/core/Rect.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Matrix3D.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/PathTypes.h"   // tgfx 的 LineCap/LineJoin 等
#include "tgfx/core/TileMode.h"
#include "tgfx/core/SamplingOptions.h"
#include "tgfx/layers/LayerMaskType.h"
#include "tgfx/layers/StrokeAlign.h"
#include "tgfx/layers/LayerPlacement.h"
#include "tgfx/layers/vectors/Repeater.h"

namespace pagx::pag {

// 版本常量
constexpr uint8_t FORMAT_VERSION = 0x02;
constexpr uint8_t COMPRESSION_UNCOMPRESSED = 0x00;

// 便携类型别名：PAGDocument 一律使用 tgfx 枚举语义（PAGX→tgfx 在 Baker 一次完成）
using BlendMode       = tgfx::BlendMode;
using LayerMaskType   = tgfx::LayerMaskType;
using LineCap         = tgfx::LineCap;
using LineJoin        = tgfx::LineJoin;
using StrokeAlign     = tgfx::StrokeAlign;
using LayerPlacement  = tgfx::LayerPlacement;
using TileMode        = tgfx::TileMode;
using FilterMode      = tgfx::FilterMode;
using MipmapMode      = tgfx::MipmapMode;
using FillRule        = tgfx::FillRule;
using PolystarType    = tgfx::PolystarType;
using MergePathOp     = tgfx::MergePathOp;
using TrimPathType    = tgfx::TrimPathType;
using RepeaterOrder   = tgfx::RepeaterOrder;
using SelectorUnit    = tgfx::SelectorUnit;
using SelectorShape   = tgfx::SelectorShape;
using SelectorMode    = tgfx::SelectorMode;
using SamplingOptions = tgfx::SamplingOptions;
using Color           = tgfx::Color;

// 2026-04-25 新增：ImagePattern 填充拟合模式
// 注：pagx::ScaleMode 与 tgfx 无直接对应枚举（tgfx 由 Inflater 按值分支处理），
// 因此在 pagx::pag 命名空间内重定义一份值对齐的枚举（同 TextAlign 处理方式）。
enum class ScaleMode : uint8_t { None = 0, Stretch = 1, LetterBox = 2, Zoom = 3 };
using Point           = tgfx::Point;
using Rect            = tgfx::Rect;
using Matrix          = tgfx::Matrix;
using Matrix3D        = tgfx::Matrix3D;
using Path            = tgfx::Path;
using Ratio           = pag::Ratio;

// TextAlign 复用 pagx::TextAlign 的语义（tgfx 无此枚举，渲染期由 TextLayer 处理）：
// 为避免 PAGDocument 依赖 pagx 头文件，在 pagx::pag 命名空间内重新定义一份值对齐的枚举。
enum class TextAlign : uint8_t { Start = 0, Center = 1, End = 2, Justify = 3 };

}  // namespace pagx::pag
```

### C.2 Property / PropertyEncoding / MakeProp

```cpp
namespace pagx::pag {

enum class PropertyEncoding : uint8_t {
  Constant = 0,
  // 1..15 reserved for animation (Hold/Linear/Bezier/Spatial...)
};

/**
 * Property<T> 内存结构。字节流布局不由此结构直接映射——Write/Read 流程在
 * ValueCodec.h 中实现，按 §4.3 propHeader 位域分发（isDefault 省略 value
 * 字节、encoding 决定 value 字节数、hasExt 预留能力扩展）。
 */
template <typename T>
struct Property {
  PropertyEncoding encoding = PropertyEncoding::Constant;
  T value = {};
  // NOTE: animation-era will add typed keyframe storage here; not present in v2.0.
};

template <typename T>
inline Property<T> MakeProp(T value) {
  return {PropertyEncoding::Constant, std::move(value)};
}

}
```

**为什么本期 Property<T> 不含 keyframes 字段**：本期 100% Constant 编码，额外字段会给百万级 VectorElement 场景引入数百 MB 运行时内存开销。动画期接入时再扩 `Property<T>` 的内存结构（字段顺序调整不影响字节流，因字节流格式由 propHeader 的 `encoding` 位段分发）。

**defaultValue 唯一来源纪律**：每个 `Property<T>` 字段的 defaultValue **有且仅有** 在其包裹结构体（附录 C.5–C.9）的 `= MakeProp(X)` 初始化器里声明一次。Codec 的 Read/Write 函数必须把**同一个 X** 作为参数传入 `WriteProperty<T>` / `ReadProperty<T>`（见 §4.3），不得在 cpp 文件里重复硬编码 `1.0f` / `{}` 等字面量，以避免 isDefault 省略失效或误判。

**Property<T> 相等性判断规则**（影响 isDefault 位的写入）：`WriteProperty<T>` 内部通过 `PropertyValueEquals<T>(a, b)` 判断 `value == defaultValue`；各类型的 equality 规则如下（已验证 tgfx 类型均实现 `operator==`）：

| 类型 T | 相等性实现 | 依据 |
|---|---|---|
| `bool` / `float` / `int32_t` / `uint32_t` / `uint8_t` | 直接 `a == b` | 基本类型 |
| `tgfx::Color` | `a == b`（**精确匹配 RGBA 四通道 f32**） | `tgfx/core/Color.h:129` `RGBA4f::operator==` |
| `tgfx::Point` | `a == b`（**精确 x/y 匹配**） | `tgfx/core/Point.h:113` friend |
| `tgfx::Rect` | `a == b`（**精确 l/t/r/b 匹配**） | `tgfx/core/Rect.h:218` friend |
| `tgfx::Matrix` | `a == b` | `tgfx/core/Matrix.h:874` friend |
| `tgfx::Matrix3D` | `a == b` | `tgfx/core/Matrix3D.h:300` member |
| `tgfx::Path` | `a == b` | `tgfx/core/Path.h:45` friend |
| `std::vector<Color>` | 长度 + 逐元素 `==`（等价 `operator==` 默认行为） | STL 内建 |
| `std::vector<float>` | 同上 | STL 内建 |
| `std::array<float, 20>` | 长度定长 + 逐元素**按位相等**（STL 内建 `==`） | STL 内建 |

**⚠ 浮点精确匹配而非容差**：

- `WriteProperty` 判断 `isDefault` 用**精确 ==**，不用 `fabs(a-b) < epsilon` 之类的容差比较；
- 理由：（1）Baker 产出的 Property 值由同一 `tgfx::Matrix::I()` / `0.0f` / 零向量构造，不应有浮点舍入误差；（2）若值是用户编辑产生的 `0.99999f`，isDefault=0 忠实写值正是期望行为，不应误判为默认。
- **NaN 规则**：STL 和 IEEE 754 约定 `NaN != NaN`，因此若 value 或 defaultValue 含 NaN，isDefault 必为 0（保守写入 value）——这是可接受的（NaN 本身是异常输入）。

**v2 ValueCodec.h 提供的 equality 入口**：

```cpp
namespace pagx::pag {

// 通用版本：直接调用 T::operator==
template <typename T>
inline bool PropertyValueEquals(const T& a, const T& b) {
  return a == b;
}

// 这是唯一入口；若未来 T 类型无 operator== 需要特化，在此处追加 template specialization

}
```

**编码实现模板**：
```cpp
template <typename T>
void WriteProperty(pag::EncodeStream* stream, const Property<T>& prop, const T& defaultValue) {
  uint8_t propHeader = static_cast<uint8_t>(prop.encoding);  // bit 0-3
  bool isDefault = (prop.encoding == PropertyEncoding::Constant)
                   && PropertyValueEquals(prop.value, defaultValue);
  if (isDefault) propHeader |= 0x10;   // bit 4
  stream->writeUint8(propHeader);
  if (!isDefault && prop.encoding == PropertyEncoding::Constant) {
    WriteValue(stream, prop.value);
  }
  // 非 Constant 分支：本期不实现（TODO 标注）
}
```

### C.3 LayerType / VectorElementType / ColorSourceType / FilterType / StyleType

```cpp
namespace pagx::pag {

enum class LayerType : uint8_t {
  Layer = 0,
  Shape = 1,
  Text = 2,
  Image = 3,
  Solid = 4,
  Vector = 5,
  Mesh = 6,              // 本期不产出；占位
  CompositionRef = 7,
};

enum class VectorElementType : uint8_t {
  Rectangle = 0,
  Ellipse = 1,
  Polystar = 2,
  ShapePath = 3,
  FillStyle = 4,
  StrokeStyle = 5,
  TrimPath = 6,
  RoundCorner = 7,
  MergePath = 8,
  Repeater = 9,
  Text = 10,
  TextPath = 11,
  TextModifier = 12,
  VectorGroup = 13,
};

enum class ColorSourceType : uint8_t {
  SolidColor = 0,
  LinearGradient = 1,
  RadialGradient = 2,
  ConicGradient = 3,
  DiamondGradient = 4,
  ImagePattern = 5,
};

enum class LayerFilterType : uint8_t {
  Blur = 0,
  DropShadow = 1,
  InnerShadow = 2,
  ColorMatrix = 3,
  Blend = 4,
};

enum class LayerStyleType : uint8_t {
  DropShadow = 0,
  InnerShadow = 1,
  BackgroundBlur = 2,
};

enum class FontSourceKind : uint8_t {
  System = 0,
  Embedded = 1,
};

}
```

### C.4 ImageAsset / FontAsset / GlyphRunBlob

```cpp
namespace pagx::pag {

struct ImageAsset {
  std::vector<uint8_t> data = {};  // 原始编码字节（PNG/JPEG/WebP），magic 头部必须匹配
  int32_t width = 0;
  int32_t height = 0;
};

struct FontAsset {
  FontSourceKind kind = FontSourceKind::System;
  std::string family = {};
  std::string style = {};
  std::vector<uint8_t> data = {};  // 仅 Embedded 非空
};

/**
 * 文本 GlyphRun 的序列化形式：Baker 侧从 pagx::Text 的 glyphData 摘取构造 tgfx::TextBlob
 * 所需原材料，Inflater 侧通过 GlyphRunRenderer 重建。
 *
 * 选择 kind 的规则：
 *   - text->glyphData->layoutRuns 非空 → LayoutRun（优先，TextBox 内 TextLayout 路径）
 *   - 否则若 text->glyphRuns 非空     → ClassicGlyphRun（独立 Text 走此路径）
 *   - 两者都空                         → Baker 不产出 ElementText（跳过）
 */
enum class GlyphRunKind : uint8_t {
  LayoutRun = 0,       // 对应 BuildTextBlobFromLayoutRuns
  ClassicGlyphRun = 1, // 对应 BuildTextBlob
};

struct GlyphRunBlob {
  GlyphRunKind kind = GlyphRunKind::LayoutRun;
  uint32_t fontIndex = 0;            // 指向 PAGDocument::fonts 的索引；UINT32_MAX 表示字体缺失降级
  float fontSize = 12.0f;
  Matrix inverseMatrix = Matrix::I();  // TextBox 内子 Text 非 identity；独立 Text 为 identity

  // —— kind == LayoutRun 的字段 ——
  // 源自 pagx::TextLayoutGlyphRun（src/pagx/TextLayout.h:41-46）
  struct LayoutGlyph {
    uint16_t glyphId = 0;
    Point position = {};
    // RSXform：若 Baker 观察到该 run 的 xforms 非空，下面三项依序记录；
    // 若 xforms 为空，hasXform=false 时后续 Inflater 直接走 MakeFromPosText 路径。
    bool hasXform = false;
    float scos = 1.0f, ssin = 0.0f, tx = 0.0f, ty = 0.0f;
  };
  std::vector<LayoutGlyph> layoutGlyphs = {};

  // —— kind == ClassicGlyphRun 的字段 ——
  // 源自 pagx::GlyphRun（include/pagx/nodes/GlyphRun.h:35-117），在 Baker 已应用 position→x/y+xOffsets
  struct ClassicGlyph {
    uint16_t glyphId = 0;
    float xOffset = 0.0f;
    Point position = {};           // 相对于 run 的 (x,y)
    Point anchor = {};             // 可全 0 时省略；见字节布局注释
    Point scale = {1.0f, 1.0f};
    float rotation = 0.0f;         // degrees
    float skew = 0.0f;             // degrees
  };
  float baseX = 0.0f, baseY = 0.0f;  // GlyphRun 的 x/y 基准
  std::vector<ClassicGlyph> classicGlyphs = {};
};

}
```

> **注**：GlyphRunBlob 的字段集在开工阶段 8（TextBaker）**开始前**需要对照 `src/renderer/GlyphRunRenderer.cpp:159-314` 再精细核对一次；当前结构已涵盖 `BuildTextBlob`/`BuildTextBlobFromLayoutRuns` 所需的所有运行时字段（font、glyphs、positions、xforms、xOffsets、anchors、scales、rotations、skews）。

### C.5-pre FileHeader / PAGDocument（顶层根结构）

```cpp
namespace pagx::pag {

// 画布与时间轴头（对应 §D.5 FileHeader Tag 的字节布局）
struct FileHeader {
  float    width           = 0.0f;
  float    height          = 0.0f;
  Color    backgroundColor = Color{};         // sRGB；由 Baker 经 ToTGFX(pagx::Color) 得出
  Ratio    frameRate       = {24, 1};         // 静态场景默认 24/1 fps
  uint32_t duration        = 1;               // 单位：frame；本期静态写 1
};

// PAG v2 数据模型根对象。由 Baker 构造，由 Codec 编码/解码，由 LayerInflater 消费。
// 生命周期一律 std::unique_ptr<PAGDocument>（见 §8.3bis RAII 纪律）。
struct PAGDocument {
  FileHeader                                 header       = {};
  std::vector<std::unique_ptr<ImageAsset>>   images       = {};
  std::vector<std::unique_ptr<FontAsset>>    fonts        = {};
  std::vector<std::unique_ptr<Composition>>  compositions = {};
};

}
```

**字段约定**：
- 四个字段与 §8.2 Encode 流程的 Tag 写入顺序严格对应（FileHeader → ImageAssetTable → FontAssetTable → CompositionList）；
- `compositions[0]` 约定为 root composition（§5.1），Inflater 的遍历入口；
- 全部 `std::unique_ptr` 子对象析构链式释放（§8.3bis "PAGDocument 析构链"）；
- 成员访问风格：Baker 拿到 `doc` 后用 `doc->header.width`、`doc->images.push_back(...)`、`doc->compositions[i]->layers[j]` 等形态（`->` 因为 `unique_ptr<PAGDocument>`）。

### C.5 Composition / Layer

```cpp
namespace pagx::pag {

struct Composition {
  std::string id = {};
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<std::unique_ptr<Layer>> layers = {};
};

// Forward declarations（payload 类型）
struct ShapePayload;
struct TextPayload;
struct ImagePayload;
struct SolidPayload;
struct VectorPayload;
struct MeshPayload;   // 本期空结构
struct LayerFilter;
struct LayerStyle;

struct Layer {
  LayerType type = LayerType::Layer;
  std::string name = {};

  // Timeline（静态：startTime=0, duration=1, stretch=1/1）
  uint32_t startTime = 0;
  uint32_t duration = 1;
  Ratio stretch = {1, 1};

  // Animatable core
  Property<bool>      visible    = MakeProp(true);
  Property<float>     alpha      = MakeProp(1.0f);
  Property<BlendMode> blendMode  = MakeProp(BlendMode::SrcOver);
  Property<Matrix>    matrix     = MakeProp(Matrix::I());
  Property<Matrix3D>  matrix3D   = MakeProp(Matrix3D::I());
  Property<Rect>      scrollRect = MakeProp(Rect{});

  // Non-animatable flags
  bool hasScrollRect = false;
  bool preserve3D = false;
  bool passThroughBackground = true;
  bool allowsEdgeAntialiasing = true;       // 对齐 PAGX antiAlias：allowsEdgeAntialiasing = antiAlias
  bool allowsGroupOpacity = true;           // 对齐 PAGX groupOpacity

  // Mask: 同 composition 内从 root 到 mask 目标的 child index 链；空表示无 mask
  std::vector<uint32_t> maskLayerPath = {};
  LayerMaskType maskType = LayerMaskType::Alpha;

  // Effects
  std::vector<std::unique_ptr<LayerFilter>> filters = {};
  std::vector<std::unique_ptr<LayerStyle>>  styles  = {};

  // Hierarchy
  std::vector<std::unique_ptr<Layer>> children = {};

  // Subtype payload (exactly one matching `type` should be populated)
  std::unique_ptr<ShapePayload>  shape;
  std::unique_ptr<TextPayload>   text;
  std::unique_ptr<ImagePayload>  image;
  std::unique_ptr<SolidPayload>  solid;
  std::unique_ptr<VectorPayload> vector;
  std::unique_ptr<MeshPayload>   mesh;                   // 本期不产出
  uint32_t compositionIndex = UINT32_MAX;                // type == CompositionRef
};

}
```

### C.6 ShapeStyleData（Fill/Stroke 的 ColorSource 承载）

```cpp
namespace pagx::pag {

struct ShapeStyleData {
  ColorSourceType sourceType = ColorSourceType::SolidColor;

  // 通用
  Property<float> alpha = MakeProp(1.0f);
  BlendMode blendMode = BlendMode::SrcOver;           // 非 SrcOver 时 Inflater 才 setter
  bool overrideBlendMode = false;                     // Baker 标记：源是否显式非 Normal/SrcOver

  // SolidColor
  Property<Color> color = MakeProp(Color{});

  // Gradient 通用（PAGX Gradient 基类的铺平表示，`include/pagx/nodes/Gradient.h`）
  Property<std::vector<Color>> stopColors    = MakeProp<std::vector<Color>>({});
  Property<std::vector<float>> stopPositions = MakeProp<std::vector<float>>({});
  Property<Matrix>             gradientMatrix = MakeProp(Matrix::I());
  bool                         fitsToGeometry = true;   // 2026-04-25 新增。true=规范化 (0,0)-(1,1) 坐标空间，逐 geometry 拟合；false=父容器坐标空间，多 geometry 共享

  // Linear（默认值 2026-04-25 与 tgfx relative-fill 对齐）
  Property<Point> startPoint = MakeProp(Point{0.0f, 0.0f});
  Property<Point> endPoint   = MakeProp(Point{1.0f, 0.0f});

  // Radial / Conic / Diamond
  Property<Point> center  = MakeProp(Point{});
  Property<float> radius  = MakeProp(0.0f);
  // Conic only
  Property<float> startAngle = MakeProp(0.0f);
  Property<float> endAngle   = MakeProp(360.0f);

  // ImagePattern（默认值 2026-04-25 与 tgfx relative-fill 对齐）
  uint32_t patternImageIndex = UINT32_MAX;             // 哨兵：图片缺失时该字段保持 UINT32_MAX
  TileMode tileModeX = TileMode::Decal;                // 2026-04-25 改：之前 Clamp → Decal
  TileMode tileModeY = TileMode::Decal;                // 同上
  FilterMode filterMode = FilterMode::Linear;
  MipmapMode mipmapMode = MipmapMode::None;
  ScaleMode  scaleMode  = ScaleMode::LetterBox;        // 2026-04-25 新增。None/Stretch/LetterBox/Zoom 见附录 E.9
  Property<Matrix> patternMatrix = MakeProp(Matrix::I());
};

}
```

### C.7 VectorElement 及各 payload

```cpp
namespace pagx::pag {

// Forward declarations of all element payload structs
struct ElementRectangleData;
struct ElementEllipseData;
struct ElementPolystarData;
struct ElementShapePathData;
struct ElementFillStyleData;
struct ElementStrokeStyleData;
struct ElementTrimPathData;
struct ElementRoundCornerData;
struct ElementMergePathData;
struct ElementRepeaterData;
struct ElementTextData;
struct ElementTextPathData;
struct ElementTextModifierData;
struct ElementVectorGroupData;

/**
 * VectorElement 使用 std::variant 承载 14 种 payload，避免为每个 element 实例化
 * 14 个 unique_ptr（每个 8 bytes，固定开销 112 bytes/element）。
 *
 * 纪律：
 *   - `type` 字段必须与 `payload` 内活跃 variant alternative 严格对应（Baker 侧通过
 *     构造函数一次性设定，不允许后续分别修改 type 和 payload）；
 *   - Read 侧先读 type，据此 emplace 对应 alternative；
 *   - Write 侧 std::visit 分发。
 */
using VectorElementPayload = std::variant<
    std::monostate,                    // 非法/未初始化状态，不应出现在 Baker 产物中
    std::unique_ptr<ElementRectangleData>,
    std::unique_ptr<ElementEllipseData>,
    std::unique_ptr<ElementPolystarData>,
    std::unique_ptr<ElementShapePathData>,
    std::unique_ptr<ElementFillStyleData>,
    std::unique_ptr<ElementStrokeStyleData>,
    std::unique_ptr<ElementTrimPathData>,
    std::unique_ptr<ElementRoundCornerData>,
    std::unique_ptr<ElementMergePathData>,
    std::unique_ptr<ElementRepeaterData>,
    std::unique_ptr<ElementTextData>,
    std::unique_ptr<ElementTextPathData>,
    std::unique_ptr<ElementTextModifierData>,
    std::unique_ptr<ElementVectorGroupData>>;

struct VectorElement {
  VectorElementType type;
  VectorElementPayload payload;
};

struct ElementRectangleData {
  Property<Point> position  = MakeProp(Point{});
  Property<Point> size      = MakeProp(Point{});
  Property<float> roundness = MakeProp(0.0f);
  bool reversed = false;
};

struct ElementEllipseData {
  Property<Point> position = MakeProp(Point{});
  Property<Point> size     = MakeProp(Point{});
  bool reversed = false;
};

struct ElementPolystarData {
  Property<Point>  position        = MakeProp(Point{});
  Property<float>  pointCount      = MakeProp(5.0f);
  Property<float>  outerRadius     = MakeProp(0.0f);
  Property<float>  innerRadius     = MakeProp(0.0f);
  Property<float>  outerRoundness  = MakeProp(0.0f);
  Property<float>  innerRoundness  = MakeProp(0.0f);
  Property<float>  rotation        = MakeProp(0.0f);
  bool             reversed        = false;
  PolystarType     polystarType    = PolystarType::Polygon;
};

struct ElementShapePathData {
  Property<Point> position = MakeProp(Point{});
  Property<Path>  path     = MakeProp(Path{});
  bool            reversed = false;
};

struct ElementFillStyleData {
  std::unique_ptr<ShapeStyleData> style;
  FillRule fillRule = FillRule::Winding;
  LayerPlacement placement = LayerPlacement::Background;
};

struct ElementStrokeStyleData {
  std::unique_ptr<ShapeStyleData> style;
  Property<float>            strokeWidth      = MakeProp(1.0f);
  LineCap                    lineCap          = LineCap::Butt;
  LineJoin                   lineJoin         = LineJoin::Miter;
  Property<float>            miterLimit       = MakeProp(4.0f);
  Property<std::vector<float>> lineDashPattern = MakeProp<std::vector<float>>({});
  Property<float>            lineDashPhase    = MakeProp(0.0f);
  bool                       lineDashAdaptive = false;
  StrokeAlign                strokeAlign      = StrokeAlign::Center;
  LayerPlacement             placement        = LayerPlacement::Background;
};

struct ElementTrimPathData {
  Property<float> start  = MakeProp(0.0f);
  Property<float> end    = MakeProp(100.0f);
  Property<float> offset = MakeProp(0.0f);
  TrimPathType trimType  = TrimPathType::Simultaneously;   // Separate→Simultaneously；Continuous→Continuous
};

struct ElementRoundCornerData {
  Property<float> radius = MakeProp(0.0f);
};

struct ElementMergePathData {
  MergePathOp mode = MergePathOp::Append;
};

struct ElementRepeaterData {
  Property<float> copies   = MakeProp(1.0f);
  Property<float> offset   = MakeProp(0.0f);
  RepeaterOrder  order     = RepeaterOrder::BelowOriginal;
  Property<Point> anchor   = MakeProp(Point{});
  Property<Point> position = MakeProp(Point{});
  Property<float> rotation = MakeProp(0.0f);
  Property<Point> scale    = MakeProp(Point{1.0f, 1.0f});
  Property<float> startAlpha = MakeProp(1.0f);
  Property<float> endAlpha   = MakeProp(1.0f);
};

struct ElementTextData {
  Property<Point> position = MakeProp(Point{});
  Property<std::vector<Point>> anchors = MakeProp<std::vector<Point>>({});
  std::vector<GlyphRunBlob> glyphRuns = {};   // 一个 Text element 通常产出 1-N 个 blob；见 §10.3
};

struct ElementTextPathData {
  Property<Path>  path            = MakeProp(Path{});
  Property<Point> baselineOrigin  = MakeProp(Point{});
  Property<float> baselineAngle   = MakeProp(0.0f);
  Property<float> firstMargin     = MakeProp(0.0f);
  Property<float> lastMargin      = MakeProp(0.0f);
  bool            perpendicular   = false;
  bool            reversed        = false;
  bool            forceAlignment  = false;
};

struct ElementTextModifierData {
  // Transform
  Property<Point> anchor   = MakeProp(Point{});
  Property<Point> position = MakeProp(Point{});
  Property<float> rotation = MakeProp(0.0f);
  Property<Point> scale    = MakeProp(Point{1.0f, 1.0f});
  Property<float> skew     = MakeProp(0.0f);
  Property<float> skewAxis = MakeProp(0.0f);
  Property<float> alpha    = MakeProp(1.0f);

  // Optional paint overrides（has* 为 true 时对应 Property 有效）
  bool hasFillColor = false;
  Property<Color> fillColor = MakeProp(Color{});
  bool hasStrokeColor = false;
  Property<Color> strokeColor = MakeProp(Color{});
  bool hasStrokeWidth = false;
  Property<float> strokeWidth = MakeProp(0.0f);

  // Selectors
  std::vector<std::unique_ptr<struct RangeSelectorData>> rangeSelectors = {};
};

struct RangeSelectorData {
  Property<float>  start        = MakeProp(0.0f);
  Property<float>  end          = MakeProp(100.0f);
  Property<float>  offset       = MakeProp(0.0f);
  SelectorUnit     unit         = SelectorUnit::Percentage;
  SelectorShape    shape        = SelectorShape::Square;
  Property<float>  easeIn       = MakeProp(0.0f);
  Property<float>  easeOut      = MakeProp(0.0f);
  SelectorMode     mode         = SelectorMode::Add;
  Property<float>  weight       = MakeProp(100.0f);
  bool             randomOrder  = false;
  uint16_t         randomSeed   = 0;
};

struct ElementVectorGroupData {
  std::vector<std::unique_ptr<VectorElement>> elements = {};
  Property<Point> anchor   = MakeProp(Point{});
  Property<Point> position = MakeProp(Point{});
  Property<Point> scale    = MakeProp(Point{1.0f, 1.0f});
  Property<float> rotation = MakeProp(0.0f);
  Property<float> alpha    = MakeProp(1.0f);
  Property<float> skew     = MakeProp(0.0f);
  Property<float> skewAxis = MakeProp(0.0f);
};

}  // namespace pagx::pag
```

### C.8 Layer Payload（Shape/Text/Image/Solid/Vector/Mesh）

```cpp
namespace pagx::pag {

struct ShapePayload {
  // 本期 PAGX 不产出此 payload（PAGX 走 VectorLayer 路径）；结构预留以便未来 tgfx::ShapeLayer 接入
  Property<Path> path = MakeProp(Path{});
  std::vector<std::unique_ptr<ShapeStyleData>> fillStyles = {};
  std::vector<std::unique_ptr<ShapeStyleData>> strokeStyles = {};
  Property<float>            strokeWidth      = MakeProp(1.0f);
  LineCap                    lineCap          = LineCap::Butt;
  LineJoin                   lineJoin         = LineJoin::Miter;
  Property<float>            miterLimit       = MakeProp(4.0f);
  Property<std::vector<float>> lineDashPattern = MakeProp<std::vector<float>>({});
  Property<float>            lineDashPhase    = MakeProp(0.0f);
  bool                       lineDashAdaptive = false;
  StrokeAlign                strokeAlign      = StrokeAlign::Center;
  bool                       strokeOnTop      = false;
};

struct TextPayload {
  // 本期 PAGX 不产出此 payload（富文本一律走 VectorLayer + ElementText）；结构预留
  Property<std::string> text       = MakeProp<std::string>({});
  Property<Color>       textColor  = MakeProp(Color{});
  uint32_t              fontIndex  = UINT32_MAX;
  Property<float>       fontSize   = MakeProp(12.0f);
  Property<float>       width      = MakeProp(0.0f);
  Property<float>       height     = MakeProp(0.0f);
  TextAlign             textAlign  = TextAlign::Start;
  bool                  autoWrap   = false;
};

struct ImagePayload {
  uint32_t imageIndex = UINT32_MAX;
  SamplingOptions sampling = {};
};

struct SolidPayload {
  Property<float> width   = MakeProp(0.0f);
  Property<float> height  = MakeProp(0.0f);
  Property<float> radiusX = MakeProp(0.0f);
  Property<float> radiusY = MakeProp(0.0f);
  Property<Color> color   = MakeProp(Color{});
};

struct VectorPayload {
  std::vector<std::unique_ptr<VectorElement>> contents = {};
};

struct MeshPayload {
  // 本期空结构；Codec Read/Write 遇到直接返回空对象（不产出、不消费实际字节）。
};

}
```

### C.9 LayerFilter / LayerStyle

```cpp
namespace pagx::pag {

struct LayerFilter {
  LayerFilterType type;

  // Blur / InnerShadow 共用 blurX/blurY
  Property<float> blurX = MakeProp(0.0f);
  Property<float> blurY = MakeProp(0.0f);
  TileMode tileMode = TileMode::Decal;        // Blur 使用

  // DropShadow / InnerShadow 共用 offset / color
  Property<float> offsetX = MakeProp(0.0f);
  Property<float> offsetY = MakeProp(0.0f);
  Property<Color> color   = MakeProp(Color{});
  bool shadowOnly = false;

  // Blend
  Property<Color> blendColor = MakeProp(Color{});
  BlendMode blendFilterMode = BlendMode::SrcOver;  // Filter 的混合模式入口

  // ColorMatrix
  Property<std::array<float, 20>> colorMatrix = MakeProp(std::array<float, 20>{});
};

struct LayerStyle {
  LayerStyleType type;
  BlendMode blendMode = BlendMode::SrcOver;
  bool excludeChildEffects = false;

  // DropShadow / InnerShadow / BackgroundBlur 共用 blurX/blurY
  Property<float> blurX = MakeProp(0.0f);
  Property<float> blurY = MakeProp(0.0f);

  // DropShadow / InnerShadow 共用 offset / color
  Property<float> offsetX = MakeProp(0.0f);
  Property<float> offsetY = MakeProp(0.0f);
  Property<Color> color   = MakeProp(Color{});
  bool showBehindLayer = false;              // DropShadow only

  // BackgroundBlur
  TileMode tileMode = TileMode::Decal;
};

}
```

---

## 23. 附录 D：Tag 字节布局规范

### D.1 通用约定

**字节序**：**全部 little endian**（复用 v1 `DecodeStream` / `EncodeStream`）。

**流工具复用清单**（v2 直接调用 v1 提供的位级/变长编解码 API，**禁止**自行实现）：

| 工具 | v1 路径 | 用途 |
|---|---|---|
| `writeUint8/16/32`, `readUint8/16/32` | `src/codec/utils/EncodeStream.h`, `DecodeStream.h` | 定长整数 |
| `writeFloat`, `readFloat` | 同上 | f32 |
| `writeEncodedUint32/64`, `readEncodedUint32/64` | `EncodeStream.cpp:166-184` | varU32/varU64（ULEB128） |
| `writeEncodedInt32/64`, `readEncodedInt32/64` | `EncodeStream.cpp:159-163` | varI32/varI64（ZigZag + ULEB128） |
| `writeBits(int32_t, numBits)` | `EncodeStream.cpp:196-215` | 有符号位级写入 |
| `writeUBits(uint32_t, numBits)` | 同上 | 无符号位级写入 |
| `readBits`, `readUBits` | `DecodeStream.cpp` 对应 | 位级读取 |
| `alignWithBytes()` | `EncodeStream.h` | 位流对齐字节边界 |
| `writeInt32List(int32_t*, count)` | `EncodeStream.cpp:239-273` | **动态位宽整数数组**（自动扫描最大值选位宽） |
| `readInt32List(int32_t*, count)` | `DecodeStream.cpp` 对应 | 动态位宽整数数组 |
| `writeFloatList(float*, count, precision)` | `EncodeStream.cpp:275-287` | 量化浮点数组 |
| `readFloatList(float*, count, precision)` | `DecodeStream.cpp` 对应 | 量化浮点数组 |
| `writeBitBoolean`, `readBitBoolean` | 同上 | 1-bit bool（用于 hasXformBits 等 bitstream） |
| `readNumBits` / `writeNumBits`（若有） | `AttributeHelper.h` 内联 | numBits 头（ULEB128 形式写位宽） |

**v2 新增工具**（`src/pagx/pag/ValueCodec.h`，**只在 v1 工具基础上薄包装**，不重新实现底层位操作）：

| 工具 | 签名 | 实现策略 |
|---|---|---|
| `WriteColor`/`ReadColor` | `void/WriteColor(stream, tgfx::Color)` | 见 D.1 Color 节；内部 `writeUint8` × 4 |
| `WriteMatrix`/`ReadMatrix` | `void/WriteMatrix(stream, tgfx::Matrix)` | header byte + 可选 f32×6；见 D.1 Matrix 节 |
| `WriteMatrix3D`/`ReadMatrix3D` | 同上 | header byte + 可选 f32×16 |
| `WritePoint`/`ReadPoint` | `void/WritePoint(stream, tgfx::Point)` | `writeFloat` × 2 |
| `WriteRect`/`ReadRect` | 同上 | `writeFloat` × 4 |
| `WriteRatio`/`ReadRatio` | `void/WriteRatio(stream, pag::Ratio)` | `writeEncodedInt32` + `writeEncodedUint32` |
| `WriteProperty<T>`/`ReadProperty<T>` | 见 §4.3 | propHeader 位域 + isDefault 省略 + 分支写 value |
| `WriteEnum<T>`/`ReadEnum<T>` | 见 附录 G.5 | u8 + `EnumMeta<T>` 值域校验 |
| `WritePath`/`ReadPath` | `void/WritePath(stream, tgfx::Path, ctx)` | format byte + D.2.1 或 D.2.2 分发 |
| `WriteGlyphRunBlob`/`ReadGlyphRunBlob` | 同上 | 见 D.11 GlyphRunBlob inline 布局 |
| `ChoosePathFormat` | `uint8_t/(tgfx::Path)` | 回退判断（极短 path 走 format=0） |

**纪律**：
- 以上所有 v1 工具必须**直接引用**，不得在 `pagx::pag::` 内另写同名函数；
- v2 新增工具必须**放在 `src/pagx/pag/ValueCodec.h` 一个文件**，保持可审查；
- 任何 Tag 的 Read/Write 实现**禁止**直接操作字节 / 拆位，只能调用上述工具。

**符号说明**（每表中列"类型"列使用）：

| 符号 | 含义 | 字节数 |
|---|---|---|
| `u8` | uint8 | 1 |
| `u16` | uint16 little endian | 2 |
| `u32` | uint32 little endian | 4 |
| `i32` | int32 little endian | 4 |
| `f32` | IEEE 754 float32 little endian | 4 |
| `f64` | IEEE 754 float64 little endian | 8 |
| `varU32` | `EncodeStream::writeEncodedUint32` 格式（varint，1~5 字节） | 1~5 |
| `varI32` | 对应有符号 varint | 1~5 |
| `utf8string` | `varU32 length` + `length` 字节 UTF-8 | 1+N |
| `bool` | `u8`，非 0 为 true | 1 |

**复合类型**（复用 v1 序列化工具函数，见 `src/codec/tags/*` 的 `ReadColor` / `ReadPoint` / `WriteMatrix` 等）：

| 类型 | 字段 | 字节数 |
|---|---|---|
| `Color` | `u8 red, u8 green, u8 blue, u8 alpha`（4 × u8，8-bit 量化 RGBA，sRGB 空间） | 4 |
| `Point` | `f32 x, f32 y` | 8 |
| `Rect` | `f32 left, f32 top, f32 right, f32 bottom` | 16 |
| `Matrix` | 见下方「Matrix 变长编码」说明 | 1 / 9 / 25 |
| `Matrix3D` | 见下方「Matrix3D 变长编码」说明 | 1 / 65 |
| `Ratio` | `varI32 numerator, varU32 denominator` | 2~10 |
| `Path` | 见 D.2 | 可变 |
| `vector<T>` | `varU32 count` + count×T | 1+N |
| `array<float, 20>` | 20×`f32` | 80 |

**Matrix 变长编码**（借鉴 v1 "默认值省略"思路）：

```
u8 matrixHeader:
  bit 0 = isIdentity       # 1 = 矩阵 == I，跳过后续所有字节
  bit 1 = hasTranslateOnly # 1 = 仅 transX/transY 非默认，其他 4 元素 = I 的对应值
                           # （scaleX=1, skewX=0, skewY=0, scaleY=1）
  bit 2-7 = reserved

if isIdentity == 1:
  (no payload; 总 1 byte)

else if hasTranslateOnly == 1:
  f32 transX, f32 transY   (总 1 + 8 = 9 byte)

else:
  f32 scaleX, skewX, transX, skewY, scaleY, transY   (总 1 + 24 = 25 byte)
```

**Matrix3D 变长编码**：

```
u8 matrix3DHeader:
  bit 0 = isIdentity
  bit 1-7 = reserved

if isIdentity == 1: (1 byte)
else:               (1 + 64 = 65 byte，f32 m[16] row-major)
```

**v2 实现函数**（`src/pagx/pag/ValueCodec.h`）：
```cpp
void WriteMatrix(pag::EncodeStream* s, const tgfx::Matrix& m);
tgfx::Matrix ReadMatrix(pag::DecodeStream* s);
void WriteMatrix3D(pag::EncodeStream* s, const tgfx::Matrix3D& m);
tgfx::Matrix3D ReadMatrix3D(pag::DecodeStream* s);
```

**Write 逻辑关键点**：
- `isIdentity = (m == tgfx::Matrix::I())`；`hasTranslateOnly = !isIdentity && m.scaleX==1 && m.skewX==0 && m.skewY==0 && m.scaleY==1`；
- 两个位互斥，isIdentity 优先；
- 新增 header byte 本身只占 1 byte，最坏情况（完整矩阵）总 25 byte，比原 24 byte 仅 +1 byte。

**节省估算**：静态场景 >70% Layer matrix 为 I（PAGX 典型样本如此），从 24 B → 1 B 每处省 23 B；纯平移矩阵从 24 B → 9 B 省 15 B。万级 Layer 场景节省 150-250 KB。

> **注**：原表里的 `Matrix (24 B)` / `Matrix3D (64 B)` 指的是**完整编码的 payload 部分字节数**，不含 1 byte header。

**Color 序列化说明**：PAGDocument **内存层**使用 `tgfx::Color`（4 × f32 RGBA，`tgfx/core/Color.h:34-124` 的 `RGBA4f<Unpremultiplied>`），但**字节流层**使用 **4 × u8 量化 RGBA**（相比 v1 `WriteColor` 的 3 × u8 RGB 增加 alpha 通道；相比 f32×4 每色省 12 B）。理由：

- **精度充分**：8-bit RGBA 能精确表达 16M 色，覆盖 PAGX 设计色的 100% 场景（PAGX Color 源也是 8-bit 设计来源）；
- **体积最优**：典型万级 Color 文件可省约 120 KB（5-10% 总体积）；
- **HDR 预留路径**：未来若需要支持 HDR / wide-gamut，通过**新 TagCode 变体**（如 `TagCode::FileHeaderHDR`、`TagCode::ShapeStyleHDR`）写入 f32×4，不破坏 8-bit 主路径。

v2 在 `src/pagx/pag/ValueCodec.h` 实现 `WriteColorRGBA8` / `ReadColorRGBA8`：

```cpp
inline uint8_t FloatToU8(float v) {
  // clamp to [0, 1] then round to nearest u8；NaN → 0
  if (!(v > 0.0f)) return 0;
  if (v >= 1.0f) return 255;
  return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

inline float U8ToFloat(uint8_t v) {
  return static_cast<float>(v) * (1.0f / 255.0f);
}

inline void WriteColorRGBA8(pag::EncodeStream* stream, const tgfx::Color& c) {
  stream->writeUint8(FloatToU8(c.red));
  stream->writeUint8(FloatToU8(c.green));
  stream->writeUint8(FloatToU8(c.blue));
  stream->writeUint8(FloatToU8(c.alpha));
}

inline tgfx::Color ReadColorRGBA8(pag::DecodeStream* stream) {
  tgfx::Color c{};
  c.red   = U8ToFloat(stream->readUint8());
  c.green = U8ToFloat(stream->readUint8());
  c.blue  = U8ToFloat(stream->readUint8());
  c.alpha = U8ToFloat(stream->readUint8());
  return c;
}
```

> **命名约定**：v2 统一用 `ReadColor` / `WriteColor` 作为 public API，内部实现即 `ReadColorRGBA8` / `WriteColorRGBA8`。全文档其余位置的"Color（4 byte）"均指此 u8×4 编码。HDR 变体如未来出现，另起 `ReadColorF32` / `WriteColorF32`。

**ColorSpace 处理（强制约束）**：`pagx::Color`（`include/pagx/types/Color.h:52`）含 `ColorSpace colorSpace = SRGB` 字段（取值 `SRGB` / `DisplayP3`）；`tgfx::Color` **只表示 sRGB**、无 colorSpace 字段。PAGDocument 字节流也**只承载 sRGB RGBA**。Baker 侧**必须**通过 `ToTGFX(const pagx::Color&)`（`src/renderer/ToTGFX.cpp:70-85`）完成 PAGX→tgfx 转换——该函数对 `DisplayP3` 源色应用 P3→sRGB 矩阵，对 `SRGB` 源色直接透传：

```cpp
// Baker 示例：禁止手写 {c.red, c.green, c.blue, c.alpha} 直接赋值
#include "renderer/ToTGFX.h"
tgfx::Color bakedColor = ToTGFX(pagxColor);  // 色域转换在此一次完成
WriteColor(stream, bakedColor);              // 内部实现 = WriteColorRGBA8
```

**禁止事项**：Baker 任何 `ShapeStyleData::color` / `FilterBlock::color` / `TextModifier::fillColor` 等字段的写入路径都**不得**绕过 `ToTGFX`（否则 P3 色的 PAGX 样本在转出 PAG 中会渲染偏色）。项目现有 `ColorSpace` 枚举固定为 `SRGB/DisplayP3` 两值，将来若 pagx 扩展新 ColorSpace 值，需同步在 `ToTGFX(Color)` 和本节补充处理策略——不在本期范围。

### D.2 tgfx::Path 的序列化格式

tgfx 无原生 serialize API（`/Users/henryjpxie/workspace/tgfx/include/tgfx/core/Path.h` 确认），Codec 按如下自定义格式序列化。**v2 定义两种 Path 编码格式**，用 `u8 pathFormat` 前缀区分：

```
u8 pathFormat
  = 0: 裸 float 格式（调试/参考用；大样本体积大）
  = 1: 量化格式（默认；借鉴 v1 动态位宽列表编码，对齐 DataTypes.cpp:27-36 的 path record 3-bit verb 思路）
```

### D.2.1 pathFormat = 0（裸 float）

```
varU32 verbCount                  # 总 verb 数（不含 Done）
repeat verbCount times:
  u8 verbKind                     # 0=Move, 1=Line, 2=Quad, 3=Conic, 4=Cubic, 5=Close
  if verbKind == Move:
    Point p0
  elif verbKind == Line:
    Point p1                      # p0 由前一 verb 的终点隐式给出，不写
  elif verbKind == Quad:
    Point p1, p2
  elif verbKind == Conic:
    Point p1, p2
    f32 conicWeight
  elif verbKind == Cubic:
    Point p1, p2, p3
  elif verbKind == Close:
    （no data）
```

**编码逻辑**（Encode 侧）：
```cpp
for (auto& segment : path) {
  if (segment.verb == PathVerb::Done) break;
  // 按 verb 写 u8 + 对应 points
}
```

**解码逻辑**（Decode 侧）：
```cpp
tgfx::Path out;
Point currentEnd = {0, 0};
for (u32 i = 0; i < verbCount; i++) {
  u8 v = stream.readUint8();
  switch (v) {
    case 0: { auto p = ReadPoint(stream); out.moveTo(p.x, p.y); currentEnd = p; break; }
    case 1: { auto p = ReadPoint(stream); out.lineTo(p.x, p.y); currentEnd = p; break; }
    case 2: { auto p1 = ReadPoint(stream); auto p2 = ReadPoint(stream);
              out.quadTo(p1.x, p1.y, p2.x, p2.y); currentEnd = p2; break; }
    case 3: { auto p1 = ReadPoint(stream); auto p2 = ReadPoint(stream);
              float w = stream.readFloat();
              out.conicTo(p1.x, p1.y, p2.x, p2.y, w); currentEnd = p2; break; }
    case 4: { auto p1 = ReadPoint(stream); auto p2 = ReadPoint(stream);
              auto p3 = ReadPoint(stream);
              out.cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y); currentEnd = p3; break; }
    case 5: { out.close(); break; }
    default: /* warn + abort this path */ return tgfx::Path{};
  }
  // 安全上限：见附录 H。Path 内 verb 数不超过 MAX_PATH_VERBS；否则返回空 path + warn。
}
```

### D.2.2 pathFormat = 1（量化，默认）

布局（借鉴 v1 `EncodeStream::writeFloatList` + `writeInt32List` 的动态位宽策略，`src/codec/utils/EncodeStream.cpp:239-287`）：

```
u8  precisionLog2      # 精度位移：实际 precision = 2^(-precisionLog2)
                       #   = 0 → 1 px       （适合大矢量图）
                       #   = 4 → 0.0625 px  （一般矢量默认）
                       #   = 6 → 0.015625 px（印刷级精度）
                       #   约束：precisionLog2 ∈ [0, 12]
varU32 verbCount

# —— 第一段：verb kind 连续 3-bit 位打包（借鉴 v1 DataTypes.cpp:27-36 的 path record）——
repeat verbCount:
  3-bit verbKind       # 0=Move 1=Line 2=Quad 3=Conic 4=Cubic 5=Close 6-7 reserved
# 3-bit 流结束后 align 到字节边界（EncodeStream::alignWithBytes()）

# —— 第二段：坐标量化数组 ——
# 所有 Move/Line/Quad/Conic/Cubic 的坐标按 path 顺序连续存一个 int32 序列；
# Close 无坐标；conicWeight 存在独立数组（精度不同）。
# 用 v1 writeInt32List 接口：先自动扫描最大绝对值计算 bitLength，
# 再用 numBits 统一位宽写所有值。
varU32 coordCount      # = Σ (per-verb 坐标分量数)：
                       #   Move=2 Line=2 Quad=4 Conic=4 Cubic=6 Close=0
int32List coords       # v1 EncodeStream::writeInt32List（内含动态位宽）
                       # 每个值 = roundf(floatValue * (1 << precisionLog2))

# —— 第三段：conicWeight 数组（若 path 含 Conic verb）——
varU32 conicCount      # Conic verb 的数量；若为 0 后续字段省略
floatList conicWeights # 精度 BEZIER_PRECISION = 0.005（对齐 v1 DataTypes.h:27-29 SPATIAL/BEZIER 量化）
                       # 实现：v1 EncodeStream::writeFloatList
```

**解码骨架**：
```cpp
tgfx::Path ReadPathV1(pag::DecodeStream* stream, DecodeContext* ctx) {
  uint8_t precisionLog2 = stream->readUint8();
  if (precisionLog2 > 12) {
    ctx->warn(ErrorCode::TruncatedData, "path precisionLog2 out of range");
    return tgfx::Path{};
  }
  float scale = 1.0f / static_cast<float>(1u << precisionLog2);
  uint32_t verbCount = stream->readEncodedUint32();
  if (verbCount > limits::MAX_PATH_VERBS) {
    ctx->warn(ErrorCode::PathVerbLimitExceeded);
    return tgfx::Path{};
  }

  std::vector<uint8_t> verbs(verbCount);
  for (uint32_t i = 0; i < verbCount; ++i) {
    verbs[i] = static_cast<uint8_t>(stream->readUBits(3));  // 3-bit verb kind
  }
  stream->alignWithBytes();

  uint32_t coordCount = stream->readEncodedUint32();
  std::vector<int32_t> rawCoords(coordCount);
  stream->readInt32List(rawCoords.data(), coordCount);      // 动态位宽反解

  uint32_t conicCount = stream->readEncodedUint32();
  std::vector<float> conicWeights(conicCount);
  if (conicCount > 0) {
    stream->readFloatList(conicWeights.data(), conicCount, 0.005f);
  }

  tgfx::Path out;
  uint32_t coordIdx = 0;
  uint32_t conicIdx = 0;
  auto popPt = [&]() -> tgfx::Point {
    float x = static_cast<float>(rawCoords[coordIdx++]) * scale;
    float y = static_cast<float>(rawCoords[coordIdx++]) * scale;
    return {x, y};
  };
  for (uint8_t v : verbs) {
    switch (v) {
      case 0: { auto p = popPt(); out.moveTo(p.x, p.y); break; }
      case 1: { auto p = popPt(); out.lineTo(p.x, p.y); break; }
      case 2: { auto p1 = popPt(); auto p2 = popPt();
                out.quadTo(p1.x, p1.y, p2.x, p2.y); break; }
      case 3: { auto p1 = popPt(); auto p2 = popPt();
                float w = conicWeights[conicIdx++];
                out.conicTo(p1.x, p1.y, p2.x, p2.y, w); break; }
      case 4: { auto p1 = popPt(); auto p2 = popPt(); auto p3 = popPt();
                out.cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y); break; }
      case 5: { out.close(); break; }
      default: ctx->warn(ErrorCode::TruncatedData, "path verb invalid"); return tgfx::Path{};
    }
  }
  return out;
}
```

**编码侧选择策略**（Baker）：
- **默认使用 `pathFormat = 1`**（量化）+ `precisionLog2 = limits::PATH_QUANTIZATION_DEFAULT_LOG2 = 4`（0.0625 px，对齐 v1 `SPATIAL_PRECISION`=0.05 的同级别精度）；
- 若 path 坐标绝对值 ≥ `limits::PATH_QUANTIZATION_MAX_COORD`（量化后溢出 int24），或 verbCount < `limits::PATH_QUANTIZATION_MIN_VERBS`（极短 path，量化头部开销大于收益），Encoder **自动回退** `pathFormat = 0`。

**`ChoosePathFormat` 伪码**（`src/pagx/pag/ValueCodec.h`）：

```cpp
inline uint8_t ChoosePathFormat(const tgfx::Path& path) {
  uint32_t verbCount = path.countVerbs();
  if (verbCount < limits::PATH_QUANTIZATION_MIN_VERBS) {
    return 0;  // 极短 path，量化的 precisionLog2 + numBits 头部开销超过 float 的收益
  }

  // 扫描所有坐标，检查是否有值超出量化可承载的 int24 范围
  float scale = static_cast<float>(1u << limits::PATH_QUANTIZATION_DEFAULT_LOG2);
  int32_t limit = limits::PATH_QUANTIZATION_MAX_COORD;
  bool overflow = false;
  path.decompose([&](tgfx::PathVerb verb, const tgfx::Point points[], float /*weight*/) {
    int pointCount = /* 按 verb 类型决定 1/2/3/0 */;
    for (int i = 0; i < pointCount && !overflow; ++i) {
      int32_t qx = static_cast<int32_t>(std::round(points[i].x * scale));
      int32_t qy = static_cast<int32_t>(std::round(points[i].y * scale));
      if (std::abs(qx) >= limit || std::abs(qy) >= limit) {
        overflow = true;
      }
    }
  });
  return overflow ? 0 : 1;
}
```

> **注**：`path.decompose` 签名以 tgfx 实际 API 为准（开工阶段读 `tgfx/core/Path.h` 确认；若无 decompose 则用 `PathIterator` 遍历）。此处伪码示意逻辑，AI 落代码时按真实 API 调整。

**渲染保真论证**：
- 0.0625 px 精度在 1080p 画布上对应 < 0.006% 的最大误差；
- Baseline::Compare 是感知相似度（Δ 颜色 + Δ 位置），不是 memcmp，容忍 sub-pixel 量化；
- §18.7 `RenderEquivalenceTest` 全量 spec/samples 比对是最终防线；
- §18.7bis `PathA vs PathB` PSNR ≥ 30 dB 是辅助诊断防线。

**体积估算**（参考 v1 动态位宽原理）：
- 典型矢量 path 坐标绝对值 < 4096（画布尺寸级），量化后 int16 级，单坐标 ~14-18 bit；
- 原裸 float：每坐标 32 bit；每 verb 还多 5-7 bit（u8 verbKind 对齐浪费）；
- 量化后每 Move/Line 约 `3 bit verb + 2×16 bit = 35 bit`（原 72 bit），**压缩比 ~49%**；
- 复杂 Cubic：`3 bit + 6×16 bit = 99 bit`（原 200 bit），**压缩比 ~50%**。

### D.3 TagHeader

**v2 复用 v1 TagHeader 的紧凑位打包格式**（参考 `src/codec/TagHeader.cpp:23-48`）：

```
打包规则：
  code 占高 10 bit，length 占低 6 bit，组成 u16 codeAndLength
  - 若 length < 63：
      u16 codeAndLength = (code << 6) | length
  - 若 length ≥ 63：
      u16 codeAndLength = (code << 6) | 63   # 低 6 bit 全 1 作为"延伸"标记
      u32 extendedLength                      # 随后 4 字节写真实 length

字节序列：
  u16 codeAndLength
  [u32 extendedLength]   # 仅当 codeAndLength & 0x3F == 63 时出现
```

**约束**：
- `code ∈ [0, 1023]`（10 bit 上限）；v2 TagCode 枚举值必须 ≤ 1023（当前所有 TagCode 均 ≤ 82，安全）；
- `length` 无硬上限（extended 4 字节即 4 GB），但实际受 `MAX_TOTAL_BODY_BYTES` 约束（附录 H）；
- **TagHeader 字节大小**：小 tag 为 2 bytes，大 tag 为 6 bytes。

**v2 实现**：**不直接复用 v1 的 `ReadTagHeader` / `WriteTagHeader`**（v1 的 code 字段类型是 `pag::TagCode`，不兼容 v2 的 `pagx::pag::TagCode`），但**字节格式完全一致**。v2 在 `src/pagx/pag/TagHeader.cpp` 独立实现同格式：

```cpp
// src/pagx/pag/TagHeader.cpp (v2)
namespace pagx::pag {

struct TagHeader {
  TagCode code;
  uint32_t length;
};

TagHeader ReadTagHeader(pag::DecodeStream* stream) {
  auto codeAndLength = stream->readUint16();
  uint32_t length = codeAndLength & 0x3F;
  uint16_t code = codeAndLength >> 6;
  if (length == 63) {
    length = stream->readUint32();
  }
  return {static_cast<TagCode>(code), length};
}

void WriteTagHeader(pag::EncodeStream* stream, pag::EncodeStream* tagBytes, TagCode code) {
  uint16_t code16 = static_cast<uint16_t>(code);
  uint32_t length = tagBytes->length();
  uint16_t typeAndLength = static_cast<uint16_t>(code16 << 6);
  if (length < 63) {
    typeAndLength |= static_cast<uint16_t>(length);
    stream->writeUint16(typeAndLength);
  } else {
    typeAndLength |= 63;
    stream->writeUint16(typeAndLength);
    stream->writeUint32(length);
  }
  stream->writeBytes(tagBytes);
}

void WriteEndTag(pag::EncodeStream* stream) {
  stream->writeUint16(0);  // code=0(End), length=0
}

}
```

**Tag body 边界对齐规则**：读完 tag body 后须 `stream->setPosition(tagStart + headerSize + tagLength)` 强制对齐（`headerSize` 为 2 或 6 字节，由该 tag 的 codeAndLength 低 6 bit 是否为 63 决定）。

### D.4 文件容器

```
'P' 'A' 'G'       # 3 bytes magic
u8  version       # 0x02
u32 bodyLength
u8  compression   # 0x00 = UNCOMPRESSED（唯一合法）
body              # bodyLength bytes：Tag 序列 + End(0) 终结
```

### D.5 FileHeader Tag

```
TagCode::FileHeader = 1
body:
  f32      width
  f32      height
  Color    backgroundColor
  Ratio    frameRate
  u32      duration
```

### D.6 ImageAssetTable / FontAssetTable

> **Decoder 强制上界**：读完 `varU32 assetCount` 后立即校验：ImageAssetTable 要求 `assetCount ≤ limits::MAX_IMAGES`；FontAssetTable 要求 `assetCount ≤ limits::MAX_FONTS`。超限推 `StructureLimitExceeded=105` fatal 并 return（见 §H.1）。CompositionList 的 `compositionCount` 同理校验 `≤ MAX_COMPOSITIONS`。

**ImageAssetTable**：

```
TagCode::ImageAssetTable = 2
body:
  varU32 assetCount
  repeat assetCount:
    varU32 dataLen
    dataLen bytes  # 原始编码字节
    i32 width
    i32 height
```

**FontAssetTable**：

```
TagCode::FontAssetTable = 3
body:
  varU32 assetCount
  repeat assetCount:
    u8          kind            # FontSourceKind
    utf8string  family
    utf8string  style
    varU32      dataLen         # kind == Embedded 时非零
    dataLen bytes
```

### D.7 CompositionList / Composition

```
TagCode::CompositionList = 4
body:
  varU32 compositionCount
  repeat compositionCount:
    [Composition Tag]           # 每个是嵌入的 TagHeader + body
```

```
TagCode::Composition = 5
body:
  utf8string id
  u32 width
  u32 height
  varU32 layerCount
  repeat layerCount:
    [LayerBlock Tag]
```

### D.8 LayerBlock

**本表是 AI 落 LayerBlock Read/Write 代码的权威依据。**

```
TagCode::LayerBlock = 10
body:
  u8         type              # LayerType（ReadEnum 校验）
  utf8string name
  u32        startTime
  u32        duration
  Ratio      stretch

  u8         layerFlags        # 5 bool 聚合
    # bit 0 = hasScrollRect
    # bit 1 = preserve3D
    # bit 2 = passThroughBackground
    # bit 3 = allowsEdgeAntialiasing
    # bit 4 = allowsGroupOpacity
    # bit 5-7 = reserved（Reader 忽略；未来新开关直接占位）

  # Property 字段（每个都带 1 byte propHeader 前缀，见 §4.3）
  Property<bool>      visible
  Property<float>     alpha
  Property<BlendMode> blendMode    # 字节层 u8；ReadProperty<BlendMode> 走 EnumMeta 值域校验
  Property<Matrix>    matrix
  Property<Matrix3D>  matrix3D

  # 条件字段：仅当 layerFlags.bit0 (hasScrollRect) == 1 时序列化
  if layerFlags & 0x01:
    Property<Rect>    scrollRect

  # 可选子 Tag 序列（按出现顺序；不出现则跳过，由 tagLength 兜底）
  # 每个子 Tag 独立 TagHeader + body
  [LayerMaskRef Tag]?              # 仅当 !maskLayerPath.empty()
  [LayerFilters Tag]?              # 仅当 !filters.empty()
  [LayerStyles Tag]?               # 仅当 !styles.empty()

  # Payload Tag（按 type 选择其一；Mesh 本期跳过）
  [<Payload> Tag]
    type=Shape          → ShapePayload
    type=Text           → TextPayload
    type=Image          → ImagePayload
    type=Solid          → SolidPayload
    type=Vector         → VectorPayload
    type=Mesh           → MeshPayload（本期空 body）
    type=CompositionRef → CompositionRefPayload
    type=Layer          → 无 payload Tag

  # 子 Layer 列表
  varU32 childCount              # Decoder 校验 childCount ≤ limits::MAX_CHILDREN_PER_LAYER；超限推 StructureLimitExceeded=105 fatal
  repeat childCount:
    [LayerBlock Tag]
```

**条件字段的 Read 侧兜底**：若 `layerFlags & 0x01 == 0` 但字节流尾部仍有未读字节（动画期新增字段的前向兼容场景），由外层 `TagHeader.length` 兜底 seek 到 tagEnd，不尝试读 scrollRect。

**layerFlags 位域常量**（v2 统一定义，避免魔数）：
```cpp
namespace pagx::pag::layer_flags {
constexpr uint8_t HasScrollRect          = 1u << 0;
constexpr uint8_t Preserve3D             = 1u << 1;
constexpr uint8_t PassThroughBackground  = 1u << 2;
constexpr uint8_t AllowsEdgeAntialiasing = 1u << 3;
constexpr uint8_t AllowsGroupOpacity     = 1u << 4;
}
```

### D.9 LayerMaskRef / LayerFilters / LayerStyles

```
TagCode::LayerMaskRef = 12
body:
  u8              maskType        # LayerMaskType
  varU32          pathDepth
  repeat pathDepth:
    varU32 childIndex
```

```
TagCode::LayerFilters = 13
body:
  varU32 filterCount
  repeat filterCount:
    [Filter Tag]                   # 依 filter.type 选 FilterBlur/FilterDropShadow/... 之一
```

```
TagCode::LayerStyles = 14
body:
  varU32 styleCount
  repeat styleCount:
    [Style Tag]                    # 依 style.type 选 StyleDropShadow/... 之一
```

### D.10 Payload Tags

**Payload 段 TagCode 分配（20-39，本期钉死，未来扩展只往后追加）**：

| TagCode 值 | 枚举项 | 本期状态 |
|---|---|---|
| 20 | `TagCode::ShapePayload` | no-op（PAGX 无对应入口，Read/Write 路径占位） |
| 21 | `TagCode::TextPayload` | no-op |
| 22 | `TagCode::ImagePayload` | no-op |
| 23 | `TagCode::SolidPayload` | no-op |
| 24 | `TagCode::VectorPayload` | 产出 |
| 25 | `TagCode::MeshPayload` | 产出（空 body） |
| 26 | `TagCode::CompositionRefPayload` | 产出 |

> **ABI 纪律**：20-23 即便 Write 路径 no-op，也**必须在 `TagCode` 枚举里显式声明以上常量值**——防止后续实现时被挪走破坏二进制稳定性。Decoder 遇到 20-23 的 Tag 按 length 跳过（触发 `UnknownTagCode=400` 或按 payload.type 不匹配的 debug-only warning，均不中断）。

**CompositionRefPayload**：
```
TagCode::CompositionRefPayload = 26
body:
  u32 compositionIndex
```

> **Decoder 校验**：读完 `compositionIndex` 后立即校验 `compositionIndex != UINT32_MAX && compositionIndex < doc.compositions.size()`；任一条件不满足推 `InvalidCompositionIndex=306` fatal 并 return（见 §G.2）。

**VectorPayload**：
```
TagCode::VectorPayload = 24
body:
  varU32 elementCount
  repeat elementCount:
    [Element<Type> Tag]            # 14 选 1
```

**ShapePayload / TextPayload / ImagePayload / SolidPayload**：本期不产出（PAGX 无对应入口），Tag 的 Write 路径是 no-op，Read 路径按 length 跳过即可。具体字段见附录 C.8；未来实现时按 C.8 字段顺序，Property 字段前缀 1 byte encoding。

**MeshPayload**：
```
TagCode::MeshPayload = 25
body:
  （空）
```

### D.11 VectorElement Tags（14 种）

> 每个 element Tag 的 body 直接对应 §C.7 的 `Element<Type>Data` 结构，字段**按结构体声明顺序**写入。下表给字段清单，字节布局按 §D.1 符号规则展开。

| TagCode | Data struct | 字段（按序） |
|---|---|---|
| ElementRectangle (40) | ElementRectangleData | `Property<Point> position, Property<Point> size, Property<float> roundness, bool reversed` |
| ElementEllipse (41) | ElementEllipseData | `Property<Point> position, Property<Point> size, bool reversed` |
| ElementPolystar (42) | ElementPolystarData | `Property<Point> position, Property<float> pointCount, Property<float> outerRadius, Property<float> innerRadius, Property<float> outerRoundness, Property<float> innerRoundness, Property<float> rotation, bool reversed, u8 polystarType` |
| ElementShapePath (43) | ElementShapePathData | `Property<Point> position, Property<Path> path, bool reversed` |
| ElementFillStyle (44) | ElementFillStyleData | `[ShapeStyleData inline], u8 fillRule, u8 placement` |
| ElementStrokeStyle (45) | ElementStrokeStyleData | `[ShapeStyleData inline], Property<float> strokeWidth, u8 lineCap, u8 lineJoin, Property<float> miterLimit, Property<vector<float>> lineDashPattern, Property<float> lineDashPhase, bool lineDashAdaptive, u8 strokeAlign, u8 placement` |
| ElementTrimPath (46) | ElementTrimPathData | `Property<float> start, Property<float> end, Property<float> offset, u8 trimType` |
| ElementRoundCorner (47) | ElementRoundCornerData | `Property<float> radius` |
| ElementMergePath (48) | ElementMergePathData | `u8 mode` |
| ElementRepeater (49) | ElementRepeaterData | `Property<float> copies, Property<float> offset, u8 order, Property<Point> anchor, Property<Point> position, Property<float> rotation, Property<Point> scale, Property<float> startAlpha, Property<float> endAlpha` |
| ElementText (50) | ElementTextData | `Property<Point> position, Property<vector<Point>> anchors, varU32 runCount, runCount × [GlyphRunBlob 内联]` |
| ElementTextPath (51) | ElementTextPathData | `Property<Path> path, Property<Point> baselineOrigin, Property<float> baselineAngle, Property<float> firstMargin, Property<float> lastMargin, u8 pathFlags` |
| ElementTextModifier (52) | ElementTextModifierData | `Property<Point> anchor, Property<Point> position, Property<float> rotation, Property<Point> scale, Property<float> skew, Property<float> skewAxis, Property<float> alpha, u8 modifierFlags, Property<Color> fillColor [if bit0], Property<Color> strokeColor [if bit1], Property<float> strokeWidth [if bit2], varU32 rangeSelectorCount, selectorCount × [RangeSelectorData 内联]` |
| ElementVectorGroup (53) | ElementVectorGroupData | `varU32 elementCount, elementCount × [Element<Type> Tag], Property<Point> anchor, Property<Point> position, Property<Point> scale, Property<float> rotation, Property<float> alpha, Property<float> skew, Property<float> skewAxis` |

**pathFlags / modifierFlags 位域定义**（v2 统一常量）：

```cpp
namespace pagx::pag::path_flags {
constexpr uint8_t Perpendicular  = 1u << 0;
constexpr uint8_t Reversed       = 1u << 1;
constexpr uint8_t ForceAlignment = 1u << 2;
// bit 3-7 reserved
}

namespace pagx::pag::modifier_flags {
constexpr uint8_t HasFillColor   = 1u << 0;
constexpr uint8_t HasStrokeColor = 1u << 1;
constexpr uint8_t HasStrokeWidth = 1u << 2;
// bit 3-7 reserved
}
```

**约束**：
- 各 reserved bit 本期写 0，Reader 忽略（向前兼容新开关）；
- `ElementTextModifier` 的 `fillColor / strokeColor / strokeWidth` Property 字段仅当对应位为 1 时序列化——Read 侧按位决定是否读；Write 侧先写 flags，再按位顺序写 Property。

**ShapeStyleData inline** 布局：

**双重 length 包裹图示**（关键：AI 落代码时容易混淆外层 TagHeader.length 与内层 innerLength）：

```
ElementFillStyle Tag  (TagCode=44，是独立 Tag，有自己的 TagHeader)
┌────────────────────────────────────────────────────────────┐
│ TagHeader (2 or 6 B)                                       │
│   code=44, length = 整个下方 body 字节数                     │
├────────────────────────────────────────────────────────────┤
│ body                                                       │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ [ShapeStyleData inline]                              │   │
│ │ ┌──────────────────────────────────────────────┐     │   │
│ │ │ u16 innerLength (或 u16=0xFFFF + u32 ext)    │     │   │
│ │ │   = ShapeStyleData 自身**内容**字节数         │     │   │
│ │ │   ⚠ 不含 innerLength 自己占的 2 或 6 字节     │     │   │
│ │ ├──────────────────────────────────────────────┤     │   │
│ │ │ u8 sourceType                                │     │   │
│ │ │ Property<float> alpha                        │     │   │
│ │ │ u8 blendMode                                 │     │   │
│ │ │ bool overrideBlendMode                       │     │   │
│ │ │ 【按 sourceType 分支的字段...】               │     │   │
│ │ └──────────────────────────────────────────────┘     │   │
│ └──────────────────────────────────────────────────────┘   │
│ u8 fillRule       ← 属于 ElementFillStyleData，非 Style     │
│ u8 placement      ← 同上                                    │
└────────────────────────────────────────────────────────────┘
```

**Read 侧伪码**（防止 AI 把两层 length 搞混）：
```cpp
auto ReadElementFillStyle(DecodeStream* s, DecodeContext* ctx) {
  // 外层 TagHeader 已由 Codec 主循环读走，tagEnd 由主循环计算
  ElementFillStyleData data;

  // 1. 读 ShapeStyleData 内的 innerLength
  auto shapeStyleStart = s->position();
  uint32_t innerLen = s->readUint16();
  uint32_t innerLenBytes = 2;
  if (innerLen == 0xFFFF) {
    innerLen = s->readUint32();
    innerLenBytes = 6;
  }
  auto shapeStyleContentEnd = shapeStyleStart + innerLenBytes + innerLen;

  // 2. 读公共头
  data.style = std::make_unique<ShapeStyleData>();
  data.style->sourceType = ReadEnum<ColorSourceType>(s, ctx);
  data.style->alpha      = ReadProperty(s, ctx, /*default=*/1.0f, /*enclosingTagEnd=*/tagEnd);
  data.style->blendMode  = ReadEnum<BlendMode>(s, ctx);
  data.style->overrideBlendMode = (s->readUint8() != 0);

  // 3. 按 sourceType 分支读（此处略）
  switch (data.style->sourceType) { /* ... */ }

  // 4. 强制对齐到 ShapeStyleData 末尾（兜底向前兼容）
  s->setPosition(shapeStyleContentEnd);

  // 5. 读 ElementFillStyleData 自己的字段
  data.fillRule  = ReadEnum<FillRule>(s, ctx);
  data.placement = ReadEnum<LayerPlacement>(s, ctx);
  return data;
}
```

**完整字段清单**：

```
# —— 内嵌长度包裹（向前兼容关键）——
u16 innerLength          # 从下一字节起到 ShapeStyleData 末尾的字节数（不含本 2 字节）
                         # 若将来某字段字节数变化，老 Reader 用 innerLength skip 整段
                         # innerLength == 0xFFFF 表示扩展为 u32（见 D.3 同款机制）
[u32 innerLengthExt]     # 仅当 innerLength == 0xFFFF 时出现

# —— 通用头（所有 sourceType 公共）——
u8 sourceType            # ColorSourceType；ReadEnum 校验
Property<float> alpha
u8 blendMode
bool overrideBlendMode

# —— 按 sourceType 分支（节省冗余字段）——
switch (sourceType):
  case SolidColor:
    Property<Color> color

  case LinearGradient:
    Property<vector<Color>> stopColors
    Property<vector<float>> stopPositions
    Property<Matrix> gradientMatrix
    bool fitsToGeometry                    # Gradient 基类共享字段
    Property<Point>  startPoint
    Property<Point>  endPoint

  case RadialGradient | DiamondGradient:
    Property<vector<Color>> stopColors
    Property<vector<float>> stopPositions
    Property<Matrix> gradientMatrix
    bool fitsToGeometry
    Property<Point>  center
    Property<float>  radius

  case ConicGradient:
    Property<vector<Color>> stopColors
    Property<vector<float>> stopPositions
    Property<Matrix> gradientMatrix
    bool fitsToGeometry
    Property<Point>  center
    Property<float>  radius
    Property<float>  startAngle
    Property<float>  endAngle

  case ImagePattern:
    u32 patternImageIndex     # UINT32_MAX = 图片缺失哨兵
    u8  tileModeX, tileModeY  # 默认 Decal（v2.2 改，之前 Clamp）
    u8  filterMode, mipmapMode
    u8  scaleMode             # ScaleMode 枚举，附录 E.9；默认 LetterBox
    Property<Matrix> patternMatrix
```

**向前兼容纪律**（B-级扩展性骨架）：
- Reader 读完分支字段后，**必须** `stream->setPosition(shapeStyleStart + innerLengthBytes + innerLength)`，不管是否剩余字节；
- 新增 sourceType 值时分配新枚举值 + 在 switch 追加分支；老 Reader 遇到未知 sourceType 走 `InvalidEnumValue` 降级为 SolidColor 默认 + innerLength 跳过；
- 新增字段到某分支末尾同样向前兼容，老 Reader 读完已知字段后被 innerLength 截断。

**节省估算**：SolidColor 分支从原"全量 ~300 byte" 降到 ~6 byte（含 innerLength）；Gradient 分支省 50-60%；ImagePattern 分支省 ~40%。万级 style 场景节省 2-4% 总体积。

**v1 对照**：v1 的 SolidColor / GradientFill / ImageReference 是**分 Tag**（每个独立 TagCode），v2 保留内联形态但用 innerLength + sourceType 分支达到同样效果，避免破坏"ShapeStyleData 归属 FillStyle/StrokeStyle"的结构归属。

**RangeSelectorData inline** 布局：

```
Property<float> start, Property<float> end, Property<float> offset
u8 unit, u8 shape
Property<float> easeIn, Property<float> easeOut
u8 mode
Property<float> weight
bool randomOrder
u16 randomSeed
```

**GlyphRunBlob inline** 布局（量化编码，借鉴 v1 float list 动态位宽）：

```
u8  kind                  # GlyphRunKind，ReadEnum 校验
u32 fontIndex             # UINT32_MAX = 字体缺失哨兵
f32 fontSize
Matrix inverseMatrix      # 沿用 D.1 变长 Matrix 编码（isIdentity 时仅 1 byte）

u8  glyphPrecisionLog2    # 坐标量化精度：实际 precision = 2^(-n)
                          # 建议 n=4 (0.0625 px) 对齐 §D.2 path
varU32 glyphCount

# —— glyphId 数组：u16 定长（字体 glyph 表 < 65536）——
glyphCount × u16 glyphId

if kind == LayoutRun:
  # —— position 分量量化数组 ——
  int32List positionXY      # 长度 = 2 × glyphCount（v1 writeInt32List 动态位宽）
                            # 值 = roundf(v * (1 << glyphPrecisionLog2))

  # —— xform 可选（行级 1-bit 存在标志）——
  # 先写一段 1-bit bitstream，长度 = glyphCount，记录每个 glyph 是否有 xform
  glyphCount bit   hasXformBits  (align to byte after)
  # 然后只为 hasXform=1 的 glyph 写 xform 数据
  nHasXform × (f32 scos, f32 ssin, f32 tx, f32 ty)

if kind == ClassicGlyphRun:
  f32 baseX, f32 baseY
  floatList xOffsets        # v1 writeFloatList，precision = 2^(-glyphPrecisionLog2)
  int32List positionXY      # 长度 = 2 × glyphCount
  int32List anchorXY        # 长度 = 2 × glyphCount；precision = 2^(-glyphPrecisionLog2)（同 positionXY）；全 0 时 v1 writeInt32List 自动压缩到 ~glyphCount bit
  floatList scaleXY         # 长度 = 2 × glyphCount；全 (1,1) 时同上
                            # precision = 0.005（v1 BEZIER_PRECISION 级别）
  floatList rotations       # 长度 = glyphCount；precision = 0.1 (度)
  floatList skews           # 长度 = glyphCount；precision = 0.1 (度)
```

**编码工具**：全部复用 v1 `EncodeStream::writeInt32List` / `writeFloatList` / `writeUBits`（`src/codec/utils/EncodeStream.cpp:239-287, 196-215`）。`readInt32List` / `readFloatList` / `readUBits` 对应解码。

**节省估算**（典型 50-字形 ClassicGlyphRun）：
- 原裸布局：`36 × 50 = 1800 byte`
- 量化后：glyphId 100 byte + position ~200 byte（动态位宽后常 ~16 bit/值）+ anchor 全 0 时 ~10 byte + scale 全单位时 ~10 byte + rotation/skew 全 0 时 ~10 byte ≈ **~340 byte**
- **压缩比 ~81%**（大段文字场景效果最显著）

**渲染保真**：0.0625 px 位置精度在标准文字大小下完全不可见；Baseline::Compare 必跑 §18.7 全量 spec/samples 含 text 的样本验证。

**回退开关**：Baker 若 glyph 位置动态范围过大（绝对值 > 2^18），可改写 `glyphPrecisionLog2 = 0`（1 px 精度），大幅缓解动态位宽膨胀——这仍在量化路径内，不需要回退到裸 float。

**编码选择简化**：v2 **不提供** GlyphRunBlob 的"裸 float"回退路径——GlyphRunBlob 只在 ElementText 里出现，调试路径通过 §18.2 的 dump 工具而非字节流级别的回退。

### D.12 Filter / Style Tags

```
TagCode::FilterBlur (80)         body: Property<float> blurX, Property<float> blurY, u8 tileMode
TagCode::FilterDropShadow (81)   body: Property<float> offsetX, Property<float> offsetY,
                                       Property<float> blurX, Property<float> blurY,
                                       Property<Color> color, bool shadowOnly
TagCode::FilterInnerShadow (82)  body: 同上（offset/blur/color/shadowOnly）
TagCode::FilterColorMatrix (83)  body: Property<array<float,20>> colorMatrix
TagCode::FilterBlend (84)        body: Property<Color> blendColor, u8 blendFilterMode

TagCode::StyleDropShadow (100)   body: bool excludeChildEffects, u8 blendMode,
                                       Property<float> offsetX, Property<float> offsetY,
                                       Property<float> blurX, Property<float> blurY,
                                       Property<Color> color, bool showBehindLayer
TagCode::StyleInnerShadow (101)  body: bool excludeChildEffects, u8 blendMode,
                                       Property<float> offsetX, Property<float> offsetY,
                                       Property<float> blurX, Property<float> blurY,
                                       Property<Color> color
TagCode::StyleBackgroundBlur (102) body: bool excludeChildEffects, u8 blendMode,
                                         Property<float> blurX, Property<float> blurY,
                                         u8 tileMode
```

### D.13 每个 Tag 的 Read/Write 函数签名模板（强制约束）

**写入函数统一签名**：
```cpp
namespace pagx::pag {

// 非嵌套 Tag（写 TagHeader + body + 对齐）
void Write<TagName>(EncodeStream* stream, const <Data>& data, EncodeContext* ctx);

// 嵌套 element/payload 无 TagHeader 的情况使用 inline 变体
void Write<TagName>Inline(EncodeStream* stream, const <Data>& data, EncodeContext* ctx);

// 读取
std::unique_ptr<<Data>> Read<TagName>(DecodeStream* stream, DecodeContext* ctx);
<Data> Read<TagName>Inline(DecodeStream* stream, DecodeContext* ctx);

}
```

**风格强约束**：
- Write 函数：接受 `const ref`，返回 `void`，通过 `ctx->warn(...)` 推送告警；
- Read 函数：接受 `stream + ctx`，成功返回 `unique_ptr<Data>` / `Data`；失败返回 `nullptr` / 默认构造并推送 error；
- 每个 Read 函数开头检查 `stream->bytesAvailable() >= MinimumTagSize<TagName>`（常量放在 Tag 对应 cpp 文件顶部）；
- 所有 Read\*Tag 完成后由上层统一 `stream->setPosition(tagEnd)` 强制对齐。

---

## 24. 附录 E：枚举映射表（PAGX ↔ PAGDocument ↔ tgfx）

**核心原则**：PAGDocument 存储 **tgfx 枚举语义**（使用 C.1 的 `using`）；PAGX → tgfx 的映射在 Baker 一次完成，Inflater 直读。

### E.1 BlendMode

PAGX `pagx::BlendMode`（18 值，`include/pagx/types/BlendMode.h:26-103`）→ tgfx `tgfx::BlendMode`（via `src/renderer/ToTGFX.cpp:119-159`）：

| PAGX | tgfx | 备注 |
|---|---|---|
| Normal | SrcOver | PAGX "Normal" 实质即 SrcOver |
| Multiply | Multiply | |
| Screen | Screen | |
| Overlay | Overlay | |
| Darken | Darken | |
| Lighten | Lighten | |
| ColorDodge | ColorDodge | |
| ColorBurn | ColorBurn | |
| HardLight | HardLight | |
| SoftLight | SoftLight | |
| Difference | Difference | |
| Exclusion | Exclusion | |
| Hue | Hue | |
| Saturation | Saturation | |
| Color | Color | |
| Luminosity | Luminosity | |
| PlusLighter | PlusLighter | |
| PlusDarker | PlusDarker | tgfx 支持（`tgfx/core/BlendMode.h:146`） |

**Baker 侧强约束**：所有 `PAGX::BlendMode` 字段**必须**经过 `ToTGFX(mode)` 转换后再 `MakeProp` 存入 PAGDocument；Inflater 直读 tgfx 枚举，无需二次转换。

### E.2 LayerMaskType

PAGX `pagx::MaskType` (`include/pagx/types/MaskType.h:26-39`) → tgfx (`src/renderer/ToTGFX.cpp:198-208`)：

| PAGX | tgfx |
|---|---|
| Alpha | Alpha |
| Luminance | Luminance |
| Contour | Contour |

### E.3 PolystarType / MergePathMode / TrimType / FillRule

| PAGX enum | 源码位置 | PAGDocument/tgfx |
|---|---|---|
| `PolystarType::Polygon` | `include/pagx/types/PolystarType.h:30` | `tgfx::PolystarType::Polygon` |
| `PolystarType::Star` | `include/pagx/types/PolystarType.h:34` | `tgfx::PolystarType::Star` |
| `MergePathMode::Append` | `include/pagx/types/MergePathMode.h:30` | `tgfx::MergePathOp::Append` |
| `MergePathMode::Union` | 同 h:34 | `tgfx::MergePathOp::Union` |
| `MergePathMode::Intersect` | 同 h:38 | `tgfx::MergePathOp::Intersect` |
| `MergePathMode::Xor` | 同 h:42 | `tgfx::MergePathOp::XOR` |
| `MergePathMode::Difference` | 同 h:46 | `tgfx::MergePathOp::Difference` |
| `TrimType::Separate` | `include/pagx/types/TrimType.h:30` | `tgfx::TrimPathType::Simultaneously` ← 名称不同值对应 |
| `TrimType::Continuous` | 同 h:34 | `tgfx::TrimPathType::Continuous` |
| `FillRule::Winding` | `include/pagx/types/FillRule.h:30` | `tgfx::FillRule::Winding` |
| `FillRule::EvenOdd` | 同 h:34 | `tgfx::FillRule::EvenOdd` |

### E.4 LineCap / LineJoin / StrokeAlign / LayerPlacement

| PAGX | tgfx | 源 |
|---|---|---|
| `LineCap::{Butt, Round, Square}` | `tgfx::LineCap::{Butt, Round, Square}` | ToTGFX.cpp:100-110 |
| `LineJoin::{Miter, Round, Bevel}` | `tgfx::LineJoin::{Miter, Round, Bevel}` | ToTGFX.cpp:112-122 |
| `StrokeAlign::{Center, Inside, Outside}` | `tgfx::StrokeAlign::{Center, Inside, Outside}` | ToTGFX.cpp:170-180 |
| `LayerPlacement::{Background, Foreground}` | `tgfx::LayerPlacement::{Background, Foreground}` | ToTGFX.cpp:165-168 |

### E.5 TileMode / FilterMode / MipmapMode

| PAGX | tgfx |
|---|---|
| `TileMode::{Clamp, Repeat, Mirror, Decal}` | `tgfx::TileMode::{Clamp, Repeat, Mirror, Decal}` |
| `FilterMode::{Nearest, Linear}` | `tgfx::FilterMode::{Nearest, Linear}` |
| `MipmapMode::{None, Nearest, Linear}` | `tgfx::MipmapMode::{None, Nearest, Linear}` |

### E.6 SelectorUnit / SelectorShape / SelectorMode

| PAGX | tgfx |
|---|---|
| `SelectorUnit::{Index, Percentage}` | `tgfx::SelectorUnit::{Index, Percentage}` |
| `SelectorShape::{Square, RampUp, RampDown, Triangle, Round, Smooth}` | `tgfx::SelectorShape::{Square, RampUp, RampDown, Triangle, Round, Smooth}` |
| `SelectorMode::{Add, Subtract, Intersect, Min, Max, Difference}` | `tgfx::SelectorMode::{Add, Subtract, Intersect, Min, Max, Difference}` |

### E.7 TextAlign

PAGX `TextAlign::{Start, Center, End, Justify}`（`include/pagx/types/TextAlign.h:27-45`）→ PAGDocument `pagx::pag::TextAlign` 同名（C.1 已重定义以避免头文件污染）。tgfx 的 TextLayer 渲染处自行解析。

### E.8 RepeaterOrder

PAGX `pagx::RepeaterOrder`（`include/pagx/types/RepeaterOrder.h:26-35`）→ tgfx `tgfx::RepeaterOrder`（`tgfx/layers/vectors/Repeater.h:30-39`）全值 1:1 一致，见 `src/renderer/ToTGFX.cpp:210-218`：

| PAGX | tgfx | 说明 |
|---|---|---|
| `BelowOriginal` | `BelowOriginal` | 副本在原件下方 |
| `AboveOriginal` | `AboveOriginal` | 副本在原件上方 |

PAGDocument 直接 `using RepeaterOrder = tgfx::RepeaterOrder;`，Baker 调 `ToTGFX(order)` 一次性转换。

**默认值**：两端均 `BelowOriginal`。无不可映射值。

### E.9 ScaleMode（2026-04-25 新增）

PAGX `pagx::ScaleMode`（`include/pagx/types/ScaleMode.h:30-56`）→ PAGDocument `pagx::pag::ScaleMode`（新增 `using`），仅在 `ShapeStyleData::scaleMode`（ImagePattern 分支）使用：

| PAGX / PAGDocument | u8 值 | 说明 |
|---|---|---|
| `None` | 0 | 不拟合几何包围盒，图案处于父容器坐标空间 |
| `Stretch` | 1 | 拉伸填满包围盒（忽略宽高比）|
| `LetterBox` | 2 | 等比内拟合+居中（默认值）|
| `Zoom` | 3 | 等比外拟合+居中（可能裁剪）|

**EnumMeta 特化**：`MaxValid = 3, Default = LetterBox`。

**Baker 约束**：Baker 从 `pagx::ImagePattern::scaleMode` 透传到 `ShapeStyleData::scaleMode`（无需经过 `ToTGFX` 映射，枚举值 1:1 一致；若 tgfx 无此枚举则 Inflater 按分支逻辑处理）。

---

## 25. 附录 F：字段名对照表（LayerBuilder ↔ PAGX DOM ↔ PAGDocument ↔ tgfx）

**用途**：AI 编码 Baker 和 Inflater 时对照此表逐字段赋值，防止字段名漂移。

### F.1 Layer 通用字段（对应 LayerBuilder.cpp:736-800）

| LayerBuilder | PAGX `Layer` 字段 | PAGDocument `Layer` 字段 | tgfx `Layer` setter | 语义规则 |
|---|---|---|---|---|
| `layer->setVisible(node->visible)` | `visible` | `visible (Property<bool>)` | `setVisible` | 直传 |
| `layer->setAlpha(node->alpha)` | `alpha` | `alpha (Property<float>)` | `setAlpha` | 直传 |
| 731-736 blendMode | `blendMode` | `blendMode (Property<BlendMode>)` 存 tgfx 枚举 | `setBlendMode` | PAGX→tgfx 在 Baker 转换；Inflater 仅当 `!= SrcOver` 时调 setter |
| 739-746 matrix 合成 | `matrix, x, y` | `matrix (Property<Matrix>)` 已合成 `Trans(x,y)*matrix` | `setMatrix` | **Baker 必须合成**，Inflater 直读 |
| 749-751 matrix3D | `matrix3D` | `matrix3D (Property<Matrix3D>)` | `setMatrix3D` | 仅当 `!isIdentity()` Inflater 调 setter |
| 752-754 preserve3D | `preserve3D` | `preserve3D (bool)` | `setPreserve3D` | 仅当 true Inflater 调 setter |
| 757-759 antiAlias | `antiAlias` | `allowsEdgeAntialiasing (bool)` | `setAllowsEdgeAntialiasing` | **名称转换**：`allowsEdgeAntialiasing = antiAlias`；Inflater 仅当 false 调 setter |
| 760 groupOpacity | `groupOpacity` | `allowsGroupOpacity (bool)` | `setAllowsGroupOpacity` | 无条件调 setter |
| 761-763 passThroughBackground | `passThroughBackground` | `passThroughBackground (bool)` | `setPassThroughBackground` | 仅当 false 调 setter |
| 769-771 scrollRect | `scrollRect, hasScrollRect` | `scrollRect (Property<Rect>) + hasScrollRect (bool)` | `setScrollRect` | Inflater 仅当 `hasScrollRect` 调 setter |
| 768-782 styles[] | `styles` | `styles[]` | `setLayerStyles(vector)` | 顺序敏感 |
| 784-795 filters[] | `filters` | `filters[]` | `setFilters(vector)` | 顺序敏感 |
| 157-158 mask | `mask, maskType` | `maskLayerPath + maskType` | `setMask + setMaskType` | 两趟策略（§12） |

### F.2 VectorElement 字段对照（简表，完整映射见附录 A 行号）

| LayerBuilder 函数 | PAGX 节点类型 | PAGDocument Data |
|---|---|---|
| convertRectangle 310-317 | `pagx::Rectangle` | ElementRectangleData |
| convertEllipse 320-326 | `pagx::Ellipse` | ElementEllipseData |
| convertPolystar 329-344 | `pagx::Polystar` | ElementPolystarData |
| convertPath 361-368 | `pagx::Path + PathData` | ElementShapePathData（已应用 renderScale）|
| convertFill 388-410 | `pagx::Fill` | ElementFillStyleData |
| convertStroke 413-446 | `pagx::Stroke` | ElementStrokeStyleData |
| convertTrimPath 578-587 | `pagx::TrimPath` | ElementTrimPathData |
| convertTextPath 589-607 | `pagx::TextPath` | ElementTextPathData（path 已包含平移）|
| convertRoundCorner 609-613 | `pagx::RoundCorner` | ElementRoundCornerData |
| convertMergePath 615-621 | `pagx::MergePath` | ElementMergePathData |
| convertRepeater 623-635 | `pagx::Repeater` | ElementRepeaterData |
| convertTextModifier 637-684 | `pagx::TextModifier` | ElementTextModifierData |
| convertGroup 686-734 | `pagx::Group / TextBox` | ElementVectorGroupData（renderPosition 已合成）|
| convertText 371-385 + PrepareTextBlob 237-243 | `pagx::Text` | ElementTextData（GlyphRunBlob 原材料）|

### F.3 ColorSource 字段对照

| LayerBuilder 函数 | PAGX 节点 | ShapeStyleData 字段 |
|---|---|---|
| convertLinearGradient 510-518 | `pagx::LinearGradient` | sourceType=LinearGradient, startPoint, endPoint, stopColors, stopPositions, gradientMatrix, fitsToGeometry |
| convertRadialGradient 520-527 | `pagx::RadialGradient` | sourceType=RadialGradient, center, radius, stops, matrix, fitsToGeometry |
| convertConicGradient 528-535 | `pagx::ConicGradient` | sourceType=ConicGradient, center, startAngle, endAngle, stops, matrix, fitsToGeometry |
| convertDiamondGradient 537-544 | `pagx::DiamondGradient` | sourceType=DiamondGradient, center, radius, stops, matrix, fitsToGeometry |
| convertImagePattern 545-576 | `pagx::ImagePattern` | sourceType=ImagePattern, patternImageIndex, tileModeX/Y, filterMode, mipmapMode, scaleMode, patternMatrix |

**Gradient stops 兜底**（对齐 487-500）：Baker 必须在 stops 为空时注入 `[Black@0, White@1]`。

**Gradient `fitsToGeometry` 传递**（对齐 `include/pagx/nodes/Gradient.h:50-57`）：Baker 从 PAGX Gradient 基类的 `fitsToGeometry` 字段**透传**到 `ShapeStyleData::fitsToGeometry`，不做转换。Inflater 据此字段决定是将 gradient 映射到 geometry 包围盒（true）还是容器坐标空间（false）。

---

## 26. 附录 G：错误码与诊断

### G.1 编号分配规则

编号按 "模块 + 严重性" 分段，新增错误码必须遵循此规则：

| 段 | 模块 | 严重性 | 示例 |
|---|---|---|---|
| 100-199 | Baker | 致命（doc=nullptr） | LayoutNotApplied |
| 200-299 | Baker | 告警（降级继续） | ImageSourceMissing |
| 300-399 | Codec | 致命（doc=nullptr；含 Loader 的 FileReadFailed） | InvalidMagic |
| 400-499 | Codec | 告警 | UnknownTagCode |
| 500-599 | Inflater | 致命 | — |
| 600-699 | Inflater | 告警 | InflateImageDecodeFailed |

段内编号**顺序追加，不复用**被删除的编号（保持二进制诊断数据的可追溯性）。

**ABI 契约（v2.4 起生效）**：`DiagnosticCode` 是对外公共头 `include/pagx/Diagnostic.h` 的一部分（见 §15.1）；内部 `pagx::pag::ErrorCode` 经由 `src/pagx/pag/ErrorCode.h` 的 `using ErrorCode = pagx::DiagnosticCode;` 别名复用同一份枚举。新增错误码**既是内部改动，也是对外 ABI 改动**，必须段内追加、不得跨段、不得复用已删除编号。

### G.2 错误码枚举（权威定义在 `include/pagx/Diagnostic.h`）

**本节仅为文档索引**。枚举值的权威声明在对外公共头 `include/pagx/Diagnostic.h`（§15.1 内嵌代码），内部通过 `using ErrorCode = pagx::DiagnosticCode;` 别名访问。

**完整错误码清单**：

```cpp
// 对外定义：include/pagx/Diagnostic.h
namespace pagx {
enum class DiagnosticCode : uint16_t {
  Ok = 0,

  // ---------------- Baker 致命 (100-199) ----------------
  LayoutNotApplied          = 100,  // PAGXDocument 未调 applyLayout
  UnresolvedImports         = 101,  // 存在未解析 import
  NullDocument              = 102,  // document 为空指针或根 compositions 无法构造
  EmptyCompositions         = 103,  // 产出的 PAGDocument 无 compositions（不合法状态）
  CompositionCycleDepth     = 104,  // 嵌套 composition 超过 MAX_LAYER_DEPTH
  StructureLimitExceeded    = 105,  // 结构性计数/长度超出附录 H 硬上限（见 §G.2 枚举注释）

  // ---------------- Baker 告警 (200-299) ----------------
  ImageSourceMissing        = 200,  // 图片文件缺失 / data URI 损坏
  FontSourceMissing         = 201,  // 内嵌/外部字体读取失败，已降级为 System
  MaskTargetMissing         = 202,  // mask 指向不存在的 layer
  MaskSelfReference         = 203,  // layer 的 mask 指向自己
  BlendModeUnmapped         = 204,  // 预留（PAGX 全量值目前都能映射到 tgfx）
  InverseMatrixNonInvertible = 205, // TextBox 子 Text 的累计矩阵不可逆，该 Text 被跳过
  TextGlyphDataEmpty        = 206,  // pagx::Text 的 glyphData 为空，Text element 未产出
  EmptyDocument             = 207,  // 空 pagx（non-fatal informational）
  GlyphRunKindInferred      = 208,  // layoutRuns 与 glyphRuns 均空时走的兜底告警

  // ---------------- Codec 致命 (300-399) ----------------
  InvalidMagic              = 300,
  UnsupportedVersion        = 301,
  UnsupportedCompression    = 302,
  TruncatedData             = 303,
  MalformedTag              = 304,  // Tag length 与实际 body 不符
  BodyLengthOutOfRange      = 305,  // bodyLength 超过 MAX_TOTAL_BODY_BYTES
  InvalidCompositionIndex   = 306,  // CompositionRef 的 compositionIndex 越界或为 UINT32_MAX
  FileReadFailed            = 307,  // PAGLoader::LoadFromFile 打开/读取文件失败

  // ---------------- Codec 告警 (400-499) ----------------
  UnknownTagCode            = 400,
  UnknownPropertyEncoding   = 401,
  ResourceSizeExceeded      = 402,  // 单个资源超过上限（附录 H）
  StringInvalidUtf8         = 403,
  PathVerbLimitExceeded     = 404,
  GlyphCountLimitExceeded   = 405,
  LayerDepthLimitExceeded   = 406,
  InvalidEnumValue          = 407,  // u8 枚举值域外数值，已回填默认值

  // ---------------- Inflater 告警 (600-699) ----------------
  InflateImageDecodeFailed  = 600,  // tgfx::Image::MakeFromEncoded 返回 null
  InflateFontCreateFailed   = 601,  // tgfx::Typeface 创建失败，已降级系统字体
  InflateGlyphRunBuildFailed = 602, // GlyphRunRenderer 返回 null，Text element 被跳过
  InflateMaskResolveFailed  = 603,  // maskLayerPath 解析失败
  InflaterEmptyDocument     = 604,  // compositions[0] 缺失或为空；Result.layer == nullptr 且本 warning 明示原因
};
}  // namespace pagx

// 内部别名：src/pagx/pag/ErrorCode.h
#include "pagx/Diagnostic.h"
namespace pagx::pag {
  using ErrorCode = pagx::DiagnosticCode;
}
```

**AI 落代码纪律**：
- 内部 Baker/Codec/Inflater 代码继续写 `ErrorCode::LayoutNotApplied`——通过 alias 解析到 `DiagnosticCode::LayoutNotApplied`，**无需改动任何现有调用点**；
- 新增错误码**只改 `include/pagx/Diagnostic.h`**，内部 alias 自动跟随；
- 文档记录修订时，§G.2 和 §15.1 的清单保持完全一致——两处之一出现新码时另一处必须同步（本期 §15.1 为权威，§G.2 为索引）。

### G.3 Diagnostic 结构（对外 struct，权威定义见 §15.1）

`Diagnostic` 类型定义在对外公共头 `include/pagx/Diagnostic.h`（§15.1）。字段语义：

| 字段 | 类型 | 语义 |
|---|---|---|
| `code` | `pagx::DiagnosticCode` | 机器可读码；调用方按 code 分支处理 |
| `message` | `std::string` | 人类补充信息（如 "image at index 3 missing"），可为空 |
| `byteOffset` | `uint32_t` | Decoder 路径 = `stream->position()` 快照；其他路径 = 0 |

**内部 alias**：`src/pagx/pag/ErrorCode.h` 在引入 `using ErrorCode = pagx::DiagnosticCode;` 的同时，也 `using Diagnostic = pagx::Diagnostic;`——内部代码写 `Diagnostic d{ErrorCode::X, msg, offset}` 完全兼容。

**byteOffset 填写规则**：

| 路径 | 填写时机 | 填写值 |
|---|---|---|
| **Decoder（Codec::Decode）** | 每次 `ctx->error(code, msg)` / `ctx->warn(code, msg)` 调用时 | `stream->position()`（当前读指针，即错误发生时已读到的字节偏移） |
| **Encoder（Codec::Encode）** | warning 时（Encoder 不产生 error） | `0`（Encoder 是线性 append，偏移对调试帮助不大，留空） |
| **Baker（Baker::Bake）** | `ctx->error(code, msg)` / `ctx->warn(code, msg)` 时 | `0`（Baker 操作的是 PAGXDocument 对象树，没有字节流上下文） |
| **Inflater（LayerInflater::Inflate）** | warning 时 | `0`（操作的是 PAGDocument 对象树，无字节流） |
| **PAGLoader（LoadFromFile）** | 文件 open/read 失败产出 `FileReadFailed` 时 | `0`（未进入字节流） |

**便捷构造辅助**（`src/pagx/pag/DiagBuild.h`，内部辅助头——**特意避开与对外 `pagx/Diagnostic.h` 同名**以防止 include 歧义）：

```cpp
#include "pagx/Diagnostic.h"     // 公共类型
#include "pagx/pag/ErrorCode.h"  // 内部 alias

namespace pagx::pag {

// 用于 Decoder：自动填充 byteOffset
inline Diagnostic MakeDecodeDiag(ErrorCode code, pag::DecodeStream* stream,
                                 std::string msg = {}) {
  return {code, std::move(msg), static_cast<uint32_t>(stream->position())};
}

// 用于 Baker/Inflater/Loader：byteOffset 为 0
inline Diagnostic MakeDiag(ErrorCode code, std::string msg = {}) {
  return {code, std::move(msg), 0};
}

}
```

**DecodeContext::error / warn 更新**（§8.5 的 Context 已有 `error()` / `warn()` 方法）：

```cpp
// src/pagx/pag/DecodeContext.cpp
void DecodeContext::error(ErrorCode code, std::string msg) {
  if (errors.size() >= limits::MAX_DIAGNOSTICS) return;   // §H.1 硬上限
  errors.push_back({code, std::move(msg),
                    static_cast<uint32_t>(currentStream ? currentStream->position() : 0)});
}
void DecodeContext::warn(ErrorCode code, std::string msg) {
  if (warnings.size() >= limits::MAX_DIAGNOSTICS) return; // §H.1 硬上限
  warnings.push_back({code, std::move(msg),
                      static_cast<uint32_t>(currentStream ? currentStream->position() : 0)});
}
```

**日志/UI 展示建议**：`FormatDiagnostic`（§15.1 公共 API）的实现里，当 `byteOffset != 0` 时追加 `" @0x<hex>"`；例如：
```
[TruncatedData] truncated at tag body @0x4a2c
[UnknownTagCode] tag code 1200 not recognized @0x1f80
[LayoutNotApplied] document must have applyLayout() called first
[FileReadFailed] cannot open '/tmp/missing.pag'
```
`byteOffset == 0` 时不追加，保持 Baker/Loader 路径的简洁。

**测试影响**：附录 G.6 的错误码矩阵表里，**每条测试**在断言 `result.errors[0].code == ...` 后**可选**追加 `EXPECT_GT(result.errors[0].byteOffset, 0u)`（仅对 Decoder 路径的测试）。不强制。

### G.4 公共 API 结构化暴露

从 v2.4 起：
- `PAGExporter::Result.errors` / `.warnings` 类型是 `std::vector<pagx::Diagnostic>`（不再是 `vector<string>`）；
- `PAGLoader::Result.errors` / `.warnings` 同构；
- 两个 Result 的 `ok` 契约统一为 **`ok == true ⟺ errors.empty()`**（见 §15.2 / §15.3 docstring）；`warnings` 可独立出现——PAGLoader 在"空文档合法场景"下 `ok=true + layer=nullptr + warnings=[InflaterEmptyDocument(604)]`；
- **字符串格式化**仅当调用方显式调用 `pagx::FormatDiagnostic(d)` 时发生——Result 本身不预计算字符串，保持零开销；
- 调用方惯用写法：

```cpp
auto r = pagx::PAGExporter::ToBytes(doc);
if (!r.ok) {
  for (const auto& d : r.errors) {
    std::cerr << pagx::FormatDiagnostic(d) << "\n";      // 默认格式化
    // 或按 code 分支处理：
    if (d.code == pagx::DiagnosticCode::LayoutNotApplied) { /* prompt user to call applyLayout */ }
  }
}
```

**不提供**便利方法 `Result::formatErrors() / formatWarnings()`——一行 range-for 等价且可控；避免 API 膨胀。

### G.5 ReadEnum<T> 安全模板（通用枚举读取）

所有 u8 枚举字段必须经过 `ReadEnum<T>` 读取，自动做值域校验：

```cpp
namespace pagx::pag {

/**
 * ReadEnum<T>: 安全读取 u8 枚举。若值超出 [0, maxValue]，push warning
 * InvalidEnumValue 并返回 defaultValue。
 *
 * 每个枚举 T 必须在 EnumMeta<T> 特化中声明 MaxValid 和 Default：
 *
 *   template <> struct EnumMeta<LayerMaskType> {
 *     static constexpr uint8_t MaxValid = 2;   // Contour = 2
 *     static constexpr LayerMaskType Default = LayerMaskType::Alpha;
 *   };
 */
template <typename T>
struct EnumMeta;  // 需要为每个 T 特化

template <typename T>
inline T ReadEnum(DecodeStream* stream, DecodeContext* ctx) {
  uint8_t raw = stream->readUint8();
  if (raw > EnumMeta<T>::MaxValid) {
    ctx->warn(ErrorCode::InvalidEnumValue,
              "enum value out of range for " + std::string(typeid(T).name()));
    return EnumMeta<T>::Default;
  }
  return static_cast<T>(raw);
}

}
```

**需要特化 `EnumMeta<T>` 的枚举**（完整清单）：
- `LayerType`（MaxValid=7, Default=Layer）
- `VectorElementType`（MaxValid=13, Default=Rectangle）
- `ColorSourceType`（MaxValid=5, Default=SolidColor）
- `LayerFilterType`（MaxValid=4, Default=Blur）
- `LayerStyleType`（MaxValid=2, Default=DropShadow）
- `FontSourceKind`（MaxValid=1, Default=System）
- `BlendMode`（MaxValid=17, Default=SrcOver；对应 §E.1 的 18 个值，以 tgfx::BlendMode 为底层类型）
- `LayerMaskType`（MaxValid=2, Default=Alpha）
- `LineCap` / `LineJoin` / `StrokeAlign`（各自 MaxValid=2, Default=Butt/Miter/Center）
- `FillRule`（MaxValid=1, Default=Winding）
- `TileMode`（MaxValid=3, Default=Decal；与结构体字段默认值对齐，见 §C.6 / §C.9）
- `FilterMode`（MaxValid=1, Default=Linear）
- `MipmapMode`（MaxValid=2, Default=None）
- `ScaleMode`（MaxValid=3, Default=LetterBox）
- `PolystarType`（MaxValid=1, Default=Polygon）
- `MergePathOp`（MaxValid=4, Default=Append；对应 §E.3 的 5 个值）
- `TrimPathType`（MaxValid=1, Default=Simultaneously）
- `RepeaterOrder`（MaxValid=1, Default=BelowOriginal）
- `LayerPlacement`（MaxValid=1, Default=Background）
- `SelectorUnit`（MaxValid=1, Default=Percentage）/ `SelectorShape`（MaxValid=5, Default=Square）/ `SelectorMode`（MaxValid=5, Default=Add）
- `TextAlign`（MaxValid=3, Default=Start）
- `PropertyEncoding`（本期 MaxValid=0，值 > 0 走 §4.4 兜底而非 ReadEnum）
- `GlyphRunKind`（MaxValid=1, Default=LayoutRun）

**`Property<EnumT>` 通路纪律**（强制）：

当某枚举 `T` 被用于 `Property<T>` 字段（本期仅 `BlendMode`，见 §C.5 Layer），其 `ValueCodec` 特化**必须**以 `ReadEnum<T>` / `WriteEnum<T>` 为实现体，**不得**绕过 `EnumMeta<T>` 值域校验：

```cpp
// 正例（§D.8 Layer.blendMode 的实现形态）
template <> inline void WriteValue<BlendMode>(EncodeStream* s, BlendMode v, EncodeContext*) {
  s->writeUint8(static_cast<uint8_t>(v));
}
template <> inline BlendMode ReadValue<BlendMode>(DecodeStream* s, DecodeContext* ctx) {
  return ReadEnum<BlendMode>(s, ctx);   // 走 EnumMeta，非法值 → InvalidEnumValue=407 + fallback
}

// 反例（严禁——会让非法 u8 逃过告警）
template <> inline BlendMode ReadValue<BlendMode>(DecodeStream* s, DecodeContext*) {
  return static_cast<BlendMode>(s->readUint8());   // ❌ 跳过值域校验 = UB
}
```

`PropertyValueEquals<EnumT>` 走默认 `operator==`（enum 按 underlying type 比较，正确无歧义）。

### G.6 测试断言与覆盖矩阵

单测用 ErrorCode 枚举断言（`EXPECT_EQ(result.errors[0].code, ErrorCode::LayoutNotApplied)`），避免字符串比对脆弱。每个 ErrorCode **必须**至少一条单测触发：

| ErrorCode | 测试文件 | 测试名 |
|---|---|---|
| LayoutNotApplied (100) | BakerEdgeCasesTest.cpp | BakerEdgeCases.NoApplyLayout |
| UnresolvedImports (101) | BakerEdgeCasesTest.cpp | BakerEdgeCases.UnresolvedImports |
| NullDocument (102) | BakerEdgeCasesTest.cpp | BakerEdgeCases.NullDocument |
| EmptyCompositions (103) | BakerEdgeCasesTest.cpp | BakerEdgeCases.EmptyCompositions |
| CompositionCycleDepth (104) | BakerEdgeCasesTest.cpp | BakerEdgeCases.CompositionCycle |
| StructureLimitExceeded (105) | BakerEdgeCasesTest.cpp | BakerEdgeCases.StructureLimitExceeded |
| StructureLimitExceeded (105) — Decoder 侧 count 攻击 | TruncatedDecodeTest.cpp | Truncate.ChildCountOverflow / ImageTableOverflow / FontTableOverflow |
| ImageSourceMissing (200) | ResourceBakerTest.cpp | ResourceBaker.ImageFileMissing |
| FontSourceMissing (201) | ResourceBakerTest.cpp | ResourceBaker.FontSourceMissing |
| MaskTargetMissing (202) | LayerBakerTest.cpp | LayerBaker.MaskTargetNotFound |
| MaskSelfReference (203) | LayerBakerTest.cpp | LayerBaker.MaskSelfReference |
| InverseMatrixNonInvertible (205) | TextBakerTest.cpp | TextBaker.NonInvertibleInverseMatrix |
| TextGlyphDataEmpty (206) | TextBakerTest.cpp | TextBaker.GlyphDataEmpty |
| EmptyDocument (207) | BakerEdgeCasesTest.cpp | BakerEdgeCases.EmptyDocument |
| GlyphRunKindInferred (208) | TextBakerTest.cpp | TextBaker.GlyphRunKindInferred |
| InvalidMagic (300) | VersionRejectTest.cpp | VersionReject.BadMagic_PAX |
| UnsupportedVersion (301) | VersionRejectTest.cpp | VersionReject.V1 / V3 / VFF |
| UnsupportedCompression (302) | TruncatedDecodeTest.cpp | Truncate.UnsupportedCompression |
| TruncatedData (303) | TruncatedDecodeTest.cpp | Truncate.InHeader / InMidTag |
| MalformedTag (304) | TruncatedDecodeTest.cpp | Truncate.InMidTagPayload |
| BodyLengthOutOfRange (305) | TruncatedDecodeTest.cpp | Truncate.CorruptedLengthField |
| InvalidCompositionIndex (306) | TruncatedDecodeTest.cpp | Truncate.InvalidCompositionIndex |
| UnknownTagCode (400) | RoundTripTest.cpp | RoundTrip.UnknownTagForwardCompat |
| UnknownPropertyEncoding (401) | RoundTripTest.cpp | RoundTrip.UnknownEncodingForwardCompat |
| ResourceSizeExceeded (402) | TruncatedDecodeTest.cpp | Truncate.ResourceOversize |
| StringInvalidUtf8 (403) | TruncatedDecodeTest.cpp | Truncate.InvalidUtf8 |
| PathVerbLimitExceeded (404) | TruncatedDecodeTest.cpp | Truncate.PathTooManyVerbs |
| GlyphCountLimitExceeded (405) | TruncatedDecodeTest.cpp | Truncate.GlyphCountOverflow |
| LayerDepthLimitExceeded (406) | TruncatedDecodeTest.cpp | Truncate.LayerDepthOverflow |
| InvalidEnumValue (407) | RoundTripTest.cpp | RoundTrip.InvalidEnumValue |
| FileReadFailed (307) | PAGLoaderTest.cpp | PAGLoader.LoadFromFile_Missing |
| InflateImageDecodeFailed (600) | InflaterParityTest.cpp | InflaterParity.CorruptImageAsset |
| InflateFontCreateFailed (601) | InflaterParityTest.cpp | InflaterParity.InvalidFontBytes |
| InflateGlyphRunBuildFailed (602) | InflaterParityTest.cpp | InflaterParity.CorruptGlyphRunBlob |
| InflateMaskResolveFailed (603) | InflaterParityTest.cpp | InflaterParity.MaskPathUnresolvable |
| InflaterEmptyDocument (604) | InflaterParityTest.cpp | InflaterParity.EmptyDocument |

---

## 27. 附录 H：安全上限与资源约束

### H.1 硬上限（解码器校验，超限一律 error）

```cpp
namespace pagx::pag::limits {
// 文件级
constexpr size_t MAX_TOTAL_BODY_BYTES   = 1024ull * 1024 * 1024;   // 1 GB

// 资源级
constexpr size_t MAX_IMAGE_BYTES        = 100ull * 1024 * 1024;    // 100 MB / 单张图
constexpr size_t MAX_FONT_BYTES         = 50ull * 1024 * 1024;     // 50 MB / 单份字体
constexpr uint32_t MAX_IMAGES           = 10000;
constexpr uint32_t MAX_FONTS            = 1000;

// Layer/结构级
constexpr uint32_t MAX_LAYER_DEPTH      = 64;                      // composition + child 合计
constexpr uint32_t MAX_LAYERS_PER_COMPOSITION = 10000;
constexpr uint32_t MAX_COMPOSITIONS     = 1000;

// Path/Glyph
constexpr uint32_t MAX_PATH_VERBS       = 100000;
constexpr uint32_t MAX_GLYPHS_PER_RUN   = 100000;

// Path 编码格式选择阈值（B1 方案，见 §D.2.2）
constexpr uint32_t PATH_QUANTIZATION_MIN_VERBS  = 3;           // verbCount < 此值 → 回退 format=0
constexpr int32_t  PATH_QUANTIZATION_MAX_COORD  = (1 << 23);   // |coord| >= 此值 → 回退 format=0（量化后溢出 int24）
constexpr uint8_t  PATH_QUANTIZATION_DEFAULT_LOG2 = 4;         // 0.0625 px 精度（对齐 SPATIAL_PRECISION=0.05 的同级别）

// 字符串
constexpr size_t MAX_NAME_STRING_BYTES = 64 * 1024;              // 64 KB（name/id/family/style）
constexpr size_t MAX_TEXT_STRING_BYTES = 10 * 1024 * 1024;       // 10 MB（TextPayload::text 等正文文本）

// Gradient stops
constexpr uint32_t MAX_GRADIENT_STOPS   = 256;

// Filters / Styles
constexpr uint32_t MAX_FILTERS_PER_LAYER = 64;
constexpr uint32_t MAX_STYLES_PER_LAYER  = 64;

// VectorElement children
constexpr uint32_t MAX_VECTOR_ELEMENTS_PER_PAYLOAD = 100000;
constexpr uint32_t MAX_VECTOR_ELEMENT_DEPTH = 64;                   // VectorGroup 嵌套深度上限
constexpr uint32_t MAX_CHILDREN_PER_LAYER = 10000;

// Diagnostic 累计上限（防止恶意文件让 warnings vector 增长到数百 MB）
constexpr uint32_t MAX_DIAGNOSTICS = 1000;                          // Decoder/Baker/Inflater 共用
}
```

**触发条件**：
- Decoder 读取 `varU32 count` 后立即与对应 MAX 比较，超限 → push `Diagnostic{<对应 ErrorCode>}` + 返回 nullptr；
- Read 单个资源 size 超 MAX_IMAGE_BYTES/MAX_FONT_BYTES → warn `ResourceSizeExceeded` + skip bytes + 跳过该资源（index 记为 UINT32_MAX）。

### H.2 Baker 侧资源约束

**同 Decoder 的 H.1 上限在 Baker 侧也生效**，防止病态 PAGX（深嵌套、超量 element）拖垮 Baker 产物：

- **MAX_LAYER_DEPTH (64)**：Baker 深度遍历 PAGX Layer 树时，`BakeContext::currentLayerDepth` 累加；超限时立即推 `ErrorCode::CompositionCycleDepth` fatal，放弃产出（`doc=nullptr`）。
- **MAX_VECTOR_ELEMENT_DEPTH (64)**：VectorGroup 嵌套同理；共用 `BakeContext::currentVectorElementDepth`。
- **MAX_COMPOSITIONS / MAX_LAYERS_PER_COMPOSITION / MAX_CHILDREN_PER_LAYER / MAX_VECTOR_ELEMENTS_PER_PAYLOAD / MAX_FILTERS_PER_LAYER / MAX_STYLES_PER_LAYER / MAX_GRADIENT_STOPS / MAX_PATH_VERBS / MAX_GLYPHS_PER_RUN / MAX_IMAGES / MAX_FONTS / MAX_NAME_STRING_BYTES / MAX_TEXT_STRING_BYTES**：Baker 在向 PAGDocument 推入每批次数据前检查，超限推 fatal 错误 `ErrorCode::StructureLimitExceeded`（见 §G.2 枚举定义；message 附具体字段名如 `"MAX_VECTOR_ELEMENTS_PER_PAYLOAD exceeded (got 120000)"` 或 `"MAX_NAME_STRING_BYTES exceeded for Layer::name (got 128KB)"`）。Decoder 侧同样校验——见 §D.6 / §D.8 Read 伪码的 `count > MAX_*` 即时 return 分支。
- **MAX_IMAGE_BYTES / MAX_FONT_BYTES**：ResourceBaker 读图片/字体字节前检查；超限推 warning `ErrorCode::ResourceSizeExceeded`，该资源 index 记为 UINT32_MAX，引用点降级。

> **错误码语义划分**：
> - `StructureLimitExceeded` (105, Baker fatal) = **结构性尺寸/计数硬上限**超限——视为病态输入（MB 级 name、十万级 layer），放弃产出；
> - `StringInvalidUtf8` (403, Codec warning) = **UTF-8 字节序列非法**——Decoder 读到损坏字节，降级为空串继续；两者语义独立，不复用。

**实施点**：这些上限在 `BakeContext` 里集中检查；Baker 各子模块（LayerBaker/VectorBaker/ResourceBaker/TextBaker）在遍历入口调 `ctx.enterLayer()` / `ctx.enterVectorGroup()` / `ctx.checkLimit(...)`。

### H.3 UTF-8 合法性

所有 `utf8string` 读取后必须调用 `pagx::utils::IsValidUTF8(s)` 校验（项目已有工具；若无，新增 `src/pagx/pag/StringCheck.h`，实现需支持 4-byte UTF-8，如 emoji `U+1F3A8`）。非法字节 → warn `StringInvalidUtf8` + 替换为空串。

**最小合法 UTF-8 校验算法**（若项目无现成工具时新增，伪码）：

```
for i = 0; i < len; :
  b0 = bytes[i]
  if b0 < 0x80:               # ASCII
    width = 1
  elif (b0 >> 5) == 0b110:    # 2-byte
    width = 2
  elif (b0 >> 4) == 0b1110:   # 3-byte
    width = 3
  elif (b0 >> 3) == 0b11110:  # 4-byte
    width = 4
  else:
    return false
  if i + width > len: return false
  for j = 1; j < width; j++:
    if (bytes[i+j] >> 6) != 0b10: return false
  i += width
return true
```

### H.4 图片魔数校验

`ImageAsset::data` 的前 8 字节做宽泛 magic 检查：

| 格式 | magic |
|---|---|
| PNG | `89 50 4E 47 0D 0A 1A 0A` |
| JPEG | 前 3 字节 `FF D8 FF` |
| WebP | `52 49 46 46 ?? ?? ?? ?? 57 45 42 50` |

不在白名单内 → warn `InflateImageDecodeFailed` + 不送 tgfx 解码。

---

## 28. 附录 I：依赖与 include 规范

### I.1 公共头（`include/pagx/`）

v2 对外共三个公共头，分依赖等级管理：

#### I.1.a tgfx-free 公共头（`Diagnostic.h` / `PAGExporter.h`）

**严格禁止**包含的头：
- 任何 `tgfx/` 开头的头；
- 任何 `src/` 目录下的头；
- 任何 `pagx/pag/` 目录下的头（PAGDocument / Codec / Baker / Inflater）。

**允许包含**：
- `pagx/Diagnostic.h`（`PAGExporter.h` 依赖它）；
- `pagx/PAGXDocument.h`（仅 `PAGExporter.h`）；
- STL 头（`<memory>`, `<string>`, `<vector>`, `<cstdint>`, `<cstddef>`）；
- 项目内已有的对外工具类型（`pagx/types/` 下公开枚举，如需）。

**ABI 纪律**：新增 `DiagnosticCode` 枚举值必须**段内追加**，不得复用或跨段（详情见 §15.1）。

#### I.1.b tgfx-dependent 公共头（`PAGLoader.h`）

`PAGLoader.h` 是唯一允许依赖 tgfx 的对外头，适用于"渲染消费方"用户。

**允许包含**：
- `pagx/Diagnostic.h`；
- `tgfx/layers/Layer.h`；
- STL 头。

**禁止包含**：
- `pagx/PAGXDocument.h`（Loader 不碰 PAGX DOM）；
- 任何 `src/` 或 `pagx/pag/` 内部头。

**头文件顶部必须有显式警告注释**（样板见 §15.3），提醒用户此头依赖 tgfx，纯导出场景应改用 `PAGExporter.h`。

### I.2 内部头（`src/pagx/pag/`）

- 可自由 include tgfx 头（`tgfx/...`）、pag v1 类型（`pag/types.h`、`pag/file.h`）、pagx 内部工具；
- **禁止**反向依赖 `src/renderer/LayerBuilder.h` 或 `src/cli/`；
- Baker 与 Inflater **可共享** `src/renderer/ToTGFX.h` 的纯函数（`ToTGFX(BlendMode)` 等），路径规则：在 Baker/Inflater `.cpp` 里直接 `#include "renderer/ToTGFX.h"`。该依赖单向（pag → renderer），不引入循环。
- 内部代码引用错误码通过 `src/pagx/pag/ErrorCode.h` 的 `using ErrorCode = pagx::DiagnosticCode;` alias，保持调用点写法（`ErrorCode::LayoutNotApplied`）不变。

### I.3 测试代码依赖

- `test/src/pag/` 下的测试代码可 include tgfx 头、所有内部头；
- `test/src/pag/support/` 下的工具（RenderUtil/SampleEnumerator/PAGXBuilder）对上层 test 用例暴露简洁 API，避免 test 用例直接 include tgfx Surface 等。

### I.4 CMake 链接

```cmake
# 主库：pagx-pag 绑定 tgfx（内部实现依赖）
add_library(pagx-pag STATIC
  src/pagx/Diagnostic.cpp
  src/pagx/pag/...)
target_include_directories(pagx-pag
  PUBLIC include
  PRIVATE src)
target_link_libraries(pagx-pag
  PUBLIC tgfx                      # PAGLoader.h 的 tgfx::Layer 类型外露到消费方
  PRIVATE pagx-core)               # 具体名称以项目 CMakeLists 为准

# CLI
target_link_libraries(pagx-cli PRIVATE pagx-pag)

# 测试
target_link_libraries(PAGFullTest PRIVATE pagx-pag)
```

> **tgfx 暴露等级**：由于 `include/pagx/PAGLoader.h` 在头文件签名里暴露 `std::shared_ptr<tgfx::Layer>`，依赖 pagx-pag 的目标**传递**地获得 tgfx 的头可见性（CMake 中用 `PUBLIC tgfx`）。仅使用 `PAGExporter.h` / `Diagnostic.h` 的消费方虽然编译时会拿到 tgfx 头路径，但不会在自己的代码里出现 tgfx 符号——这是"在头文件层面隔离依赖"可以达到的最强保证；若要彻底隔离 tgfx 路径污染，需要把 PAGLoader 拆成独立静态库 `pagx-pag-loader`，**本期不做**（过度设计，用户量极小）。

---

## 文档维护

- 本文档所有行号引用基于 `src/renderer/LayerBuilder.cpp` 当前实现；LayerBuilder 修改时需同步更新本文档附录 A。
- PAGX 规范版本：`spec/pagx_spec.zh_CN.md`。
- PAG v2 版本：0x02（本期）；动画扩展不升版本号。
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
