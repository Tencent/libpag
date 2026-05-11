# Phase 20 候选优化：静态文档压缩（设计存档）

> **状态**：设计已定，**未启动实施**。两个子方案均评估完毕（详见 `pagx_to_pag_v2_changelog.md` Phase 19/20 评估段），**预期组合收益 ~6.6%（~15.4 KB on 230 KB baseline）**，单独启动收益不足以抵消 wire 不兼容变更的全文档同步成本，**本存档作为触发条件命中时的现成方案**。
>
> **存档时间**：2026-05-11
> **作者**：Phase 19 / Phase 20 评估周期内沉淀
> **关联**：`docs/pagx_to_pag_v2_design.md` §6.5（兼容性两级保障）/ §D.8（LayerBlock 字节布局）/ §4.3（Property 编码）/ `pagx_to_pag_v2_changelog.md` Phase 19/20 评估段

---

## 0. 范围与触发条件

**两个子方案**，正交叠加：

| 子方案 | 范围 | 真实样本收益 |
|---|---|---|
| **方向 1** — Layer 静态时间字段压缩 | Layer 的 `startTime/duration/stretch` 三组字段（共 16 byte/Layer）静态时省略 | **~3.7% / ~9 KB** 全库累积 |
| **方向 2** — Property propHeader 位流压缩 | 借鉴 V1 `AttributeBlock`：所有 Property 字段的 isDefault 标志改为前置 1-bit 位流，把 propHeader 从 1 byte/字段降到 1 bit/字段 | **~2.9% / ~6.7 KB** 全库累积 |

**触发条件**（任意一条满足都可启动**两子方案合并实施**）：

1. **PAGX 引入时间动画字段**（startTime / duration / stretch / inPoint / outPoint 任一参与解析）—— 自然触发 wire 变更，搭车实施零边际成本
2. **下次 FORMAT_VERSION bump**（spec §6.5 ② 必须升版本号的协议级变更）—— wire 反正要破，搭车 cleanup 文档同步成本一次性付清
3. **V1 codec 真正下线**（`src/codec/` 不再被生产路径使用）—— V2 codec 框架可以更激进重构，吸收 V1 AttributeBlock 设计

**不触发的场景**（不要单独为此开 Phase）：
- 仅为 6% 全库累积压缩单独 bump wire format
- 仅为优化某几个小样本（绝对节省字节量太小）

**为什么必须组合实施**：两个子方案都需要 Layer/Element body 的 wire 字节布局调整。分别实施意味着两次 wire 不兼容变更 + 两次文档同步 + 两次 baseline 重录——成本翻倍而收益不变。同期实施只付一次 cost。

---

## 1. 方向 1 — Layer 静态时间字段压缩

### 1.1 背景

PAGX 当前不解析 `startTime / duration / stretch`，`LayerBaker.cpp:204-205` hard-code `startTime=0, duration=1`，stretch 默认 `{1, 1}`。但 LayerBlock wire 格式仍然为每个 Layer 写入 16 byte：

```
u32 startTime           # 4 byte
u32 duration            # 4 byte
i32 stretch.numerator   # 4 byte
u32 stretch.denominator # 4 byte
```

**真实数据**（基于 spec/samples + resources 共 2,905 个 `<Layer>`）：

- **100% layer 时间字段为默认值** —— 没有任何样本写过非默认时间值
- 全 48 sample (test/perf/baseline.json) 累积时间字段开销 **9,264 byte / 235,457 byte = 3.93%**
- 重点 sample 占比：`text_path` 36% / `text_modifier` 30% / `polystar` 18% / `rectangle` 16% / `text_box` 14%

### 1.2 设计

**当前 LayerBlock 字节布局**（CodecTagsLayer.cpp:244-291，spec §D.8）：

```
u8     type
utf8   name
u32    startTime          ← 静态时浪费
u32    duration           ← 静态时浪费
i32    stretch.numerator  ← 静态时浪费
u32    stretch.denominator ← 静态时浪费
u8     layerFlags
[ LayerTransform 子 Tag ]
[ optional sub-Tags ]
varU32 childCount + N×LayerBlock
```

**Phase 20 方向 1 后的字节布局**：

```
u8     type
utf8   name
u16    layerHeaderFlags          ← 扩 u8 → u16（合并 layerFlags + isStaticTime + 余位预留）
[if !isStaticTime]
  u32  startTime
  u32  duration
  i32  stretch.numerator
  u32  stretch.denominator
[ LayerTransform 子 Tag ]
[ optional sub-Tags ]
varU32 childCount + N×LayerBlock
```

### 1.3 layerHeaderFlags 位定义

当前 `layerFlags` (u8) 已用满 8 bit（`PackLayerFlags` 函数）：

```
bit 0 = HasScrollRect
bit 1 = Preserve3D
bit 2 = PassThroughBackground
bit 3 = AllowsEdgeAntialiasing
bit 4 = AllowsGroupOpacity
bit 5 = HasFilters
bit 6 = HasStyles
bit 7 = HasMaskRef
```

**方向 1 把 layerFlags 扩 u8 → u16**：

```
u16 layerHeaderFlags:
  bit 0-7  = 现有 layerFlags 8 个标志（保持位语义不变）
  bit 8    = isStaticTime         ← Phase 20 新增
                  0 = 时间字段全默认（不写 16 byte）
                  1 = 时间字段非默认（按现 schema 写 16 byte）
  bit 9-15 = reserved (writer 必须写 0；reader 必须忽略)
```

### 1.4 Encoder（Baker 侧）

`CodecTagsLayer.cpp:244-291` `WriteLayerBlock` 修改：

```cpp
void WriteLayerBlock(EncodeStream* stream, const Layer& layer, EncodeSession* session) {
  EncodeStream body(session->sc);
  body.writeUint8(static_cast<uint8_t>(layer.type));
  WriteUtf8String(&body, layer.name);

  // Phase 20 方向 1：pack layerFlags + isStaticTime into u16
  const bool isStaticTime = (layer.startTime == 0 && layer.duration == 1 &&
                             layer.stretch.numerator == 1 && layer.stretch.denominator == 1);
  uint16_t headerFlags = static_cast<uint16_t>(PackLayerFlags(layer));
  if (isStaticTime) {
    headerFlags |= 0x0100;  // bit 8
  }
  body.writeUint16(headerFlags);

  if (!isStaticTime) {
    body.writeUint32(layer.startTime);
    body.writeUint32(layer.duration);
    body.writeInt32(layer.stretch.numerator);
    body.writeUint32(layer.stretch.denominator);
  }

  // ... rest unchanged (LayerTransform / sub-Tags / children) ...
}
```

### 1.5 Decoder（Inflater 侧）

`CodecTagsLayer.cpp:293-440` `ReadLayerBlock` 对称修改：

```cpp
auto layer = std::make_unique<Layer>();
// ... read type / name as before ...

uint16_t headerFlags = stream->readUint16();
uint8_t layerFlags = static_cast<uint8_t>(headerFlags & 0xFF);
bool isStaticTime = (headerFlags & 0x0100) != 0;

if (isStaticTime) {
  layer->startTime = 0;
  layer->duration = 1;
  layer->stretch = {1, 1};
} else {
  layer->startTime = stream->readUint32();
  layer->duration = stream->readUint32();
  layer->stretch.numerator = stream->readInt32();
  layer->stretch.denominator = stream->readUint32();
  if (layer->stretch.denominator == 0) {
    ctx->warn(MalformedTag, "Layer.stretch denominator == 0; reset to {1,1}");
    layer->stretch = {1, 1};
  }
}

UnpackLayerFlags(layerFlags, layer.get());
// ... rest unchanged ...
```

### 1.6 预期收益

| 维度 | 数值 |
|---|---|
| 全 48 sample 累积节省 | **8,685 byte** |
| 占全库 PAG 总体积 | **3.69%** |
| 重点 sample 压缩 | text_path 36%↓ / text_modifier 30%↓ / polystar 18%↓ |
| 单 Layer 净节省 | 16 byte（时间字段省略） − 1 byte（layerFlags u8→u16）= **15 byte** |
| 工作量 | ~30 行代码 + ~50 行测试 + 文档同步 |

---

## 2. 方向 2 — Property propHeader 位流压缩

### 2.1 背景

V2 当前 `Property<T>` 编码（`src/pagx/pag/PropertyEncoding.h:307-319`）：每字段独立 1 byte propHeader（含 isDefault + encoding bit）。

V1 codec（`src/codec/AttributeHelper.h:485-502` `WriteTagBlock`）的 `AttributeBlock` 机制：**所有字段的 exist/animatable/hasSpatial 标志位共享一个前置位流（flagBytes），每字段 1-3 bit，bit-pack 到字节边界**。N 个字段的 flag 开销从 V2 的 **N byte** 降到 **N/8 byte**。

### 2.2 真实数据对照

| 维度 | V1 AttributeBlock | V2 Property | 差距 |
|---|---|---|---|
| 默认值 payload | 不写 | **不写**（已实现） | ✅ 已对齐 |
| 字段 flag 存储 | **N×1-3 bit 共享前置位流** | **N byte 独立 propHeader** | ❌ V2 浪费 ~7 bit/field |
| Bool 字段 | 1 bit (`writeBitBoolean`) | 1 byte propHeader + 1 byte value | ❌ V2 浪费 ~14 bit |

**全 48 sample 真实数据**（grep 全代码 `WriteProperty` 调用站点共 104 个，按真实样本字段实例统计）：

- 估总 Property 字段实例：**7,710**（= 579 Layer × 5 Property + 1,605 Vector Element × 3 Property）
- 当前 propHeader 字节税：**7,710 byte = 3.27% of all-pag**
- 改用 V1-style 位流：~964 byte（每字段 1 bit 打包）
- **净节省**：**~6,746 byte = 2.87%**

### 2.3 设计

**V2 现有 propHeader 字节** (`PropertyEncoding.h:35-46`)：

```
bit 0-3 = encoding (Constant=0; 1..15 reserved for animation)
bit 4   = isDefault
bit 5-7 = reserved
```

**Phase 20 方向 2 新结构**：把 isDefault 从 propHeader 抽出来到位流，propHeader 仅在非默认时存在：

```
WriteTagBody(LayerTransform / VectorElement payload / etc.):
  ① flagBytes 段（前置位流）
     - 每字段 1 bit isDefault：0 = 默认值（payload + propHeader 都不写）
                                1 = 非默认值（按现 schema 写 propHeader + payload）
     - alignWithBytes() 对齐到字节边界
  ② payload 段
     - 顺序追加每个非默认字段的 propHeader (1 byte) + value
     - propHeader 仅含 encoding bit 0-3（isDefault bit 释放给位流）
```

### 2.4 字段顺序契约（关键）

V1 用 `BlockConfig` 注册式管理字段顺序（运行时反射查表），但 V2 现行风格是"按字段顺序 inline 写"。**方向 2 不必引入 BlockConfig**——保持 V2 inline 风格，**Encoder/Decoder 用相同的字段顺序契约即可**。

**实施时**：每个 LayerTransform / 各 ElementXxx body / ShapeStyleData inline 在新结构下定义自己的"字段顺序表"作为编码契约（写入 spec §D 章节）。

例：`LayerTransform` 当前 6 个 Property（visible / alpha / blendMode / matrix / matrix3D / scrollRect[条件]）：

```
WriteLayerTransform:
  ① flagBytes:
     bit 0 = visible    isDefault?
     bit 1 = alpha      isDefault?
     bit 2 = blendMode  isDefault?
     bit 3 = matrix     isDefault?
     bit 4 = matrix3D   isDefault?
     bit 5 = scrollRect isDefault? (仅当 hasScrollRect=1 才出现该 bit)
     align to byte boundary
  ② payloads (按字段顺序，仅当 flag bit = 0 才出现)
     [if visible flag = 0]    skip; [else] u8 propHeader(encoding) + Bool value
     [if alpha flag = 0]      skip; [else] u8 propHeader + float value
     ... etc.
```

### 2.5 嵌套结构处理

V2 当前有几处嵌套 Property 块：

- **ShapeStyleData inline**（`ElementTags.cpp:235-292`）: 内含 5-7 个 Property（依 sourceType 分支），现以 inner-length u16 包裹
- **LayerTransform** (`CodecTagsLayer.cpp`): 6 个 Property
- **ElementText body** / **ElementVectorGroup body** / 各类 VectorElement: 字段集

每个嵌套块**独立维护自己的 flagBytes 段**（不是全文档一个大位流），以保留"未知 Tag 跳过"能力（Reader 见到无法识别的嵌套 sourceType 仍能按 inner-length 整段 skip）。

### 2.6 Encoder（Baker 侧）伪码

```cpp
template <typename T>
void AccumulateFieldFlag(EncodeStream& flagBytes, const Property<T>& prop, const T& defaultValue) {
  bool isDefault = (prop.encoding == PropertyEncoding::Constant) &&
                   PropertyValueEquals(prop.value, defaultValue);
  flagBytes.writeBitBoolean(!isDefault);  // 1 bit per field
}

template <typename T>
void WriteFieldPayload(EncodeStream* stream, const Property<T>& prop, const T& defaultValue) {
  bool isDefault = (prop.encoding == PropertyEncoding::Constant) &&
                   PropertyValueEquals(prop.value, defaultValue);
  if (!isDefault) {
    stream->writeUint8(static_cast<uint8_t>(prop.encoding));  // propHeader (4 bit encoding only)
    WriteValue<T>(stream, prop.value);
  }
}

void WriteLayerTransform(EncodeStream* stream, const Layer& layer, EncodeSession* sess) {
  EncodeStream flagBytes(sess->sc);
  EncodeStream payloads(sess->sc);

  AccumulateFieldFlag(flagBytes, layer.visible,   /*default=*/true);
  AccumulateFieldFlag(flagBytes, layer.alpha,     /*default=*/1.0f);
  AccumulateFieldFlag(flagBytes, layer.blendMode, /*default=*/BlendMode::Normal);
  AccumulateFieldFlag(flagBytes, layer.matrix,    /*default=*/Matrix::I());
  AccumulateFieldFlag(flagBytes, layer.matrix3D,  /*default=*/Matrix3D::I());
  if (layer.hasScrollRect) {
    AccumulateFieldFlag(flagBytes, layer.scrollRect, /*default=*/Rect{0,0,0,0});
  }

  WriteFieldPayload(&payloads, layer.visible,   true);
  WriteFieldPayload(&payloads, layer.alpha,     1.0f);
  WriteFieldPayload(&payloads, layer.blendMode, BlendMode::Normal);
  WriteFieldPayload(&payloads, layer.matrix,    Matrix::I());
  WriteFieldPayload(&payloads, layer.matrix3D,  Matrix3D::I());
  if (layer.hasScrollRect) {
    WriteFieldPayload(&payloads, layer.scrollRect, Rect{0,0,0,0});
  }

  flagBytes.alignWithBytes();
  flagBytes.writeBytes(&payloads);
  WriteTag(stream, TagCode::LayerTransform, &flagBytes);
}
```

### 2.7 Decoder（Inflater 侧）伪码

```cpp
LayerTransform ReadLayerTransform(DecodeStream* stream, DecodeContext* ctx, uint32_t tagEnd) {
  // ① flagBytes 段
  bool hasVisible    = stream->readBitBoolean();
  bool hasAlpha      = stream->readBitBoolean();
  bool hasBlendMode  = stream->readBitBoolean();
  bool hasMatrix     = stream->readBitBoolean();
  bool hasMatrix3D   = stream->readBitBoolean();
  bool hasScrollRectField = layer->hasScrollRect ? stream->readBitBoolean() : false;
  stream->alignWithBytes();

  // ② payload 段（与 Encoder 严格对应字段顺序）
  layer->visible   = hasVisible ? ReadFieldPayload<bool>(stream, /*default=*/true) : MakeProp(true);
  layer->alpha     = hasAlpha ? ReadFieldPayload<float>(stream, 1.0f) : MakeProp(1.0f);
  // ... etc.
}
```

### 2.8 与 Property<T>.encoding 1-15 预留的兼容性

V2 现 propHeader 给 encoding 留 4 bit（0=Constant，1-15 给未来 Hold/Linear/Bezier/Spatial 等动画编码）。方向 2 后：

- **flagBytes**：仍是 1 bit/field（仅区分 isDefault vs 非默认），不影响 encoding 扩展
- **payload propHeader**：保留完整 4 bit encoding 字段（仅当 flag=1 时出现）
- 未来动画扩展走 §4.4 规则：未知 encoding 跳整 Tag，与现行机制等价

### 2.9 预期收益

| 维度 | 数值 |
|---|---|
| 全 48 sample 累积节省 | **~6,746 byte** |
| 占全库 PAG 总体积 | **2.87%** |
| 重点 sample 压缩 | text_path 18.2%↓ / text_modifier 15.3%↓ / container_layout 8.4%↓ / polystar 8.2%↓ |
| 单 Property 净节省 | 1 byte → 1 bit = **7 bit/field** |

### 2.10 风险

- **Encoder/Decoder 字段顺序契约必须严格对应**：任何字段位置错位 = 整 Tag 解码错乱
  - 缓解：每个嵌套块字段顺序写入 spec §D 章节作为权威；新增 RoundTripTest 用例覆盖每个块
- **嵌套块的 flagBytes 隔离**：ShapeStyleData inline / 各 VectorElement 都独立位流，不能误共享
  - 缓解：架构设计明确，每个 `WriteXxxBody` 函数内有自己的 flagBytes
- **Reader 错位时的安全网**：当前 V2 propHeader 错误时走 §4.4 规则跳整 Tag；位流方案下 flag 错位也会触发数据全错——必须确保 RoundTrip 测试覆盖每个块的零 / 全默认 / 部分默认场景

---

## 3. 组合实施

### 3.1 总收益

| 子方案 | 全库累积 | 占比 |
|---|---|---|
| 方向 1（Layer 静态时间） | ~8,685 byte | 3.69% |
| 方向 2（propHeader 位流） | ~6,746 byte | 2.87% |
| **合计** | **~15,431 byte** | **~6.55%** |

> 合计稍高于简单相加（因为方向 2 也节省 LayerTransform 内 5 个 Property × 7 bit ≈ 4.4 byte/Layer × 2900 Layer ≈ 1.6 KB，与方向 1 部分重叠在 Layer 字节计算上；上述数字已扣除重叠）。

### 3.2 实施步骤（搭车 wire bump 时）

1. **Step A**：确定触发条件已命中（动画字段引入 / FORMAT_VERSION bump / V1 codec 下线）
2. **Step B**：spec 文档先行
   - `docs/pagx_to_pag_v2_design.md` §6.1 段位扩张（如有新 TagCode）
   - §D.8 LayerBlock 字节布局（layerHeaderFlags u8→u16 + isStaticTime bit）
   - §4.3 Property 编码改为 flagBytes + payloads 两段式
   - 每个嵌套块（LayerTransform / ShapeStyleData / 各 VectorElement）的字段顺序契约
   - changelog 新版本段：明确"FORMAT_VERSION bump 后旧 .pag 作废，重 Bake"
3. **Step C**：代码实施
   - `CodecTagsLayer.cpp` 方向 1：layerHeaderFlags 扩展 + 条件读写时间字段
   - `PropertyEncoding.h` 方向 2：新增 `AccumulateFieldFlag` / `WriteFieldPayload` / `ReadFieldFlag` / `ReadFieldPayload` 框架函数
   - 各 `WriteXxxBody` / `ReadXxxBody` 函数（LayerTransform / ShapeStyleData inline / 14 种 ElementXxx / 5 Filter / 3 Style）按新框架重写
4. **Step D**：测试
   - RoundTrip 每个块的零/全默认/部分默认场景
   - PAGFullTest 全 PASS
   - CrossCheck 48/48 PASS（量化误差应为零，纯字节重排）
   - `test/perf/baseline.json` 体积重新录制
5. **Step E**：文档闭环
   - changelog 追补实施结果（commit hash + CrossCheck 状态 + 真实体积下降数）
   - memory 更新 `feedback_pag_v2_compression_evaluated.md` 状态

### 3.3 工作量估算

- 方向 1：~30 行代码 + ~50 行测试
- 方向 2：~200 行代码（PropertyEncoding 框架 + 各 Body 重写）+ ~150 行测试
- 文档同步：~3-5 工作日（spec §6.1/§D 各章节 + changelog）
- 总计：1-2 周（取决于嵌套块的字段数量复杂度）

---

## 4. 不做替代方案

评估周期内考虑过但放弃的方案：

- **方案 A1**：把 4 个 Layer 时间字段全改 varU32 + 各自 isDefault 标志位
  - 拒绝：复杂度高，收益 12 byte/layer 弱于方向 1 的 15 byte/layer
- **方案 A2**：把 startTime/duration/stretch 包成 Optional sub-Tag
  - 拒绝：sub-Tag 头开销 2-6 byte > 方向 1 节省全部
- **方案 A3**：永远 hard-code 0/1/{1,1}，删掉这 4 个字段
  - 拒绝：违反"为未来动画扩展预留 wire 空间"的 Phase 1 设计原则
- **方案 B1**：完全移植 V1 BlockConfig 反射注册式 schema
  - 拒绝：与 V2 inline 风格不符，重构面过大；inline 字段顺序契约就够
- **方案 B2**：对所有非默认值再做位级量化（float→u16 / position→u8）
  - 拒绝：精度风险 + 与 §4.3 encoding 概念重叠；另外的 Phase 题
- **方案 C**（沿伸自 V1）：Bool 字段 1 bit 打包（writeBitBoolean）
  - 评估：V2 非 Property 的 bool 不多（ShapeStyleData.fitsToGeometry / overrideBlendMode 等），收益 < 0.5%。**Phase 20 不单独做**；方向 2 实施时同步把这些 bool 也并入 flagBytes 自然解决
- **方案 D**（沿伸自 V1）：Gradient stopPosition u16 量化（V1 0.01 精度）
  - 评估：space_explorer 238 stops 节省 ~480 byte = 1%；仅 gradient-heavy sample 受益。**Phase 20 不做**，留给未来 ColorStopV2 / 动画曲线设计一并做
- **方案 E**（沿伸自 V1）：Path 智能 verb（HLine/VLine/Curve01/Curve10）
  - 评估：与 spec §D.2.2 format=1 设计稿（动态位宽方案）不同；当前 sample 库 Path verb 量很少，两方案收益都 <2%。**留给客户大字体真实场景再做**

---

## 5. 文档同步清单

实施时同步更新：

- `src/pagx/pag/CodecTagsLayer.cpp` 字节布局注释（行 13-30 附近）
- `src/pagx/pag/PropertyEncoding.h` 头文件设计说明（行 30-50 附近）
- `src/pagx/pag/PAGDocument.h` Layer struct 注释（startTime/duration 存储语义不变，但 wire 编码不再无条件写）
- `docs/pagx_to_pag_v2_design.md` §D.8 LayerBlock 字节布局表
- `docs/pagx_to_pag_v2_design.md` §4.3 Property 编码段（flagBytes + payloads 两段式）
- `docs/pagx_to_pag_v2_design.md` §D 附录每个嵌套块的字段顺序契约表
- `docs/pagx_to_pag_v2_design.md` §6.1 layerFlags / layerHeaderFlags 位映射表
- `docs/pagx_to_pag_v2_changelog.md` 新版本段
- `test/perf/baseline.json` 重新录制

## 6. 测试要求

- `test/src/pag/unit/CodecTagsLayerTest.cpp`（或同类 RoundTrip 测试）新增用例：
  - 静态 Layer round-trip（确认省 16 byte）
  - 非静态 Layer round-trip（确认 16 byte 仍写入并准确还原）
  - 边界：`stretch.denominator=0` 仍触发 warn 且 reset 到 {1,1}
  - 上限：layerHeaderFlags bit 9-15 写入非零值时 reader 必须忽略
- `test/src/pag/unit/PropertyEncodingTest.cpp` 扩展：
  - flagBytes round-trip（每个嵌套块都覆盖）
  - 全默认 / 部分默认 / 零默认三种场景
  - 嵌套块（ShapeStyleData inline）的 flagBytes 独立性
  - Reader 对未知 encoding 的 §4.4 跳 Tag 行为仍正确
- `test/perf/baseline.json` 重新录制（.pag 体积变化）
- `PAGFullTest` + `CrossCheck 48/48` 全 PASS

## 7. 与未来动画扩展的兼容性

未来引入时间动画时：
- `startTime/duration/stretch` 不再永远默认 → `isStaticTime=0` 命中减少
- **静态文档继续受益**（许多文档仍是静态的，或大部分 layer 静态、少数 layer 动画）
- 动画场景下 wire 与现状一致（写完整 16 byte），无回归
- Property encoding 1-15 槽位仍预留给 Hold/Linear/Bezier/Spatial 等动画编码，方向 2 的位流方案不冲突

**长期定位**：方向 1 + 方向 2 是"为静态文档免交动画税"的设计——动画支持落地后，**静态 Layer 立省 15 byte + 静态 Property 立省 7 bit/field 是它对生态的持续贡献**。

---

## 附录 A：当前样本数据快照（2026-05-11）

```
=== 方向 1 ===
Total <Layer> in spec/samples: 399
Total <Layer> in resources/:    2,506
Total cross-corpus:             2,905
Layers with time animation:     0
Static ratio:                   100.0%

All 48 baseline samples:
  Total Layer time bytes (current): 9,264
  Total pag bytes:                  235,457
  Layer time% of all:               3.93%
  After Phase 20 dir 1:             ~579 byte (only layerFlags u16 extension overhead)
  Net savings:                      8,685 byte = 3.69%
```

```
=== 方向 2 ===
WriteProperty call sites in V2 codec: 104
  - ElementTags.cpp: 77 + 6 path
  - CodecTagsLayer.cpp: 6
  - StyleFilterTags.cpp: 21

Estimated Property field instances across 48 samples:
  Layers (579) × 5 Property/layer = 2,895
  Vector elements (1,605) × 3 Property/elem = 4,815
  Total: 7,710 instances

  Current propHeader cost: 7,710 byte = 3.27% of pag
  V1-style bit-packed: ~964 byte (1 bit each)
  Net savings: ~6,746 byte = 2.87%
```

```
=== 重点 sample 改善估算 ===
sample                     pag    dir1%  dir2%  combined%
text_path                  714    36%    18%    ~50%↓
text_modifier              1,020  30%    15%    ~42%↓
polystar                   625    18%    8%     ~24%↓
text_box                   3,328  14%    7%     ~20%↓
container_layout           1,102  19%    8%     ~25%↓
mid-size samples (1-10KB)  各异   5-10%  4-7%   ~10-15%↓
big samples (image-heavy)  60KB   <1%    <1%    <2%↓
```

## 附录 B：源码定位

| 文件 | 行 | 内容 |
|---|---|---|
| `src/pagx/pag/CodecTagsLayer.cpp` | 244-291 | WriteLayerBlock |
| `src/pagx/pag/CodecTagsLayer.cpp` | 293-440 | ReadLayerBlock |
| `src/pagx/pag/CodecTagsLayer.cpp` | (内部 helper) | PackLayerFlags / UnpackLayerFlags |
| `src/pagx/pag/PAGDocument.h` | 651-652 | Layer.startTime / duration 默认值 |
| `src/pagx/pag/LayerBaker.cpp` | 204-205 | hard-code 0/1 的源头 |
| `src/pagx/pag/PropertyEncoding.h` | 35-46 | propHeader 字节布局 |
| `src/pagx/pag/PropertyEncoding.h` | 307-342 | WriteProperty / ReadProperty |
| `src/pagx/pag/ElementTags.cpp` | 235-292 | ShapeStyleData inline（嵌套 Property 块） |
| `src/codec/AttributeHelper.h` | 38-66 | V1 AttributeFlag 结构（参考） |
| `src/codec/AttributeHelper.h` | 485-502 | V1 WriteTagBlock（flagBytes 位流参考实现） |
| `docs/pagx_to_pag_v2_design.md` | §D.8 | LayerBlock 权威字节布局 |
| `docs/pagx_to_pag_v2_design.md` | §4.3 | Property 编码权威说明 |
