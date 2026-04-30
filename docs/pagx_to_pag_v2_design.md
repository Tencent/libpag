# PAGX → PAG v2 转换技术方案设计文档

**版本**：2.19
**日期**：2026-04-30
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

> **bis/ter/quater 章节导航说明**（P1-6 v2.18）：文档经 9 轮评审累计新增补丁小节（§8.3bis/ter、§18.3pre/nix/bis/ter/quater、§18.4bis/ter/quater/quinta、§G.3bis、§C.5-pre 共 13+ 个），均在其父章节之下扩展，**不跨节跳跃**。读者按 §N + §N-bis/ter 同段连续消费；完整章节结构（带子节）可用 `grep -n "^### " docs/pagx_to_pag_v2_design.md` 列出。未来 v2.19+ 若重构为 §N.M.K 标准三级嵌套，是纯目录级改动，无语义影响。

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

### 2.1 伪码风格约束（P1-8 v2.18，对齐 `.codebuddy/rules/Code.md`）

本文档所有伪码片段必须遵循项目编码规范：
- **禁 lambda**：所有原先以 `auto X = [](){...};` 表达的局部辅助，改为**静态自由函数**（`static bool CheckCoord(float);`）或**类内私有方法**。Path decomposition 回调改为显式 `for (uint32_t i = 0; i < path.countVerbs(); ++i) { getVerbAt(i, ...) }` 迭代，不使用 `path.decompose([&](...){...})` 风格。
- **禁 `dynamic_cast` 与 C++ 异常**（`throw/try/catch`）——Codec / Baker / Inflater 全部通过 `DiagnosticCollector` 返回错误。
- **禁 `mutable` 成员**——若 const 方法需修改状态，优先把方法改非 const；不走 mutable。
- 伪码违规 = 实现违规。code review 时若发现 lambda / dynamic_cast，一律回退 PR。已知已修复点：§D.2.1 `checkCoord`、§D.2.2 `popPt`、§D.2.2 `path.decompose` 回调、§18.2 Day-1 smoke `renderLocal`、§18.3pre 相关注释病句。

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
    ▼ pagx::pag::Baker::Bake         [BakeContext]      ◀── 新增
  PAGDocument
    │
    ▼ pagx::pag::Codec::Encode       [EncodeSession]    ◀── 新增（v2.19 P0-D：DiagnosticCollector* + StreamContext* 2 指针聚合）
  *.pag (version = 0x02)

[解码]
  *.pag (version = 0x02)
    │
    ▼ pagx::pag::Codec::Decode       [DecodeContext]    ◀── 新增
  PAGDocument
    │
    ▼ pagx::pag::LayerInflater::Inflate [InflaterContext] ◀── 新增
  LayerInflater::Result { layer, warnings }
    │
    ▼ tgfx::DisplayList                          （已有）
  屏幕渲染
```

### 3.2 四态三映射三 Context 全览（接手 AI 5 分钟必读）

| 维度 | 成员 |
|---|---|
| **4 状态** | `PAGXDocument`（XML 模型） → `PAGDocument`（中间数据模型） → `bytes`（PAG v2 字节流） → `PAGDocument`（解码态） → `tgfx::Layer`（渲染态） |
| **3 映射** | `Baker::Bake`（状态 1→2） / `Codec::Encode`+`Codec::Decode`（状态 2↔3） / `LayerInflater::Inflate`（状态 4→5） |
| **3 Context + 1 EncodeSession**（v2.19 P0-D 钉死；原 4 Context，Encode 退化为 `struct EncodeSession { DiagnosticCollector*; StreamContext*; };` 轻量聚合）| `BakeContext`（Baker 用） / `DecodeContext`（Decode 用） / `InflaterContext`（Inflate 用）——均继承 `DiagnosticCollector`（protected pushError/pushWarning helper + 子类 3 参 public error/warn wrapper）；Encode 阶段直接用 EncodeSession 2 指针聚合，不再独立 Context——见 §8.5 / §9.4 |
| **错误码段** | Baker: 100-199 (fatal) + 200-299 (warn) / Codec: 300-399 (fatal) + 400-499 (warn) / Inflater: 500-599 (fatal, 本期未用) + 600-699 (warn)——见 §G.1 |
| **权威定义位置** | 状态→§5/§C；映射→§7-§9；Context→§8.5/§9.4；错误码→§G.2 |

- Baker 与 LayerInflater **以 `LayerBuilder.cpp` 为共同蓝本**，保证编解码对称与渲染一致。
- 3 Context 全部继承 `DiagnosticCollector` 基类（P0-3 v2.18 抽取 + P0-C v2.19 钉死契约）——基类仅暴露 protected `pushError(Diagnostic)` / `pushWarning(Diagnostic)` helper（内置 MAX_DIAGNOSTICS cap + MAX_DIAGNOSTIC_MESSAGE_BYTES 截断），子类各自定义 **3 参 public `error(code, msg, contextIndex)` / `warn(...)` wrapper**（C++ 名称查找永远落子类，无 name hiding 歧义）。DecodeContext 另持 `currentStream` 并在 wrapper 内自动填 byteOffset；BakeContext / InflaterContext 的 byteOffset 恒 0；InflaterContext 物理屏蔽 `errors` / `hasError()`（"无 fatal" 语义）。
- **阅读链**（P1-03 v2.19）：先读本 §3.2 全览表 → 再读 §22 开工必读 14 条（v2.19 版硬约束清单）→ 编码期按需查 §3.3 术语索引。不建议直接从 §4 开始顺读，会在 §6-§8 出现"概念前置"困扰。

### 3.3 术语与权威定义索引（P2-5 v2.18 新增）

新接手 AI 通过本表快速定位任意术语的唯一权威章节，无需跨节猜测。所有术语**只有一个**权威定义点；其他章节出现仅为引用。

| 术语 | 含义 | 权威章节 |
|---|---|---|
| `PAGX` | 内部 XML 动画编辑格式（`.pagx`） | §1.1 背景 |
| `PAGXDocument` | PAGX 运行时内存对象树（`pagx::PAGXDocument`），Baker 输入 | `include/pagx/PAGXDocument.h` |
| `PAG v1` | 旧二进制 `.pag` 格式（version=0x01），v1 播放器消费 | `include/pag/file.h` |
| `PAG v2` | 新二进制 `.pag` 格式（version=0x02），本文档主题 | §4.1 容器头 |
| `PAGDocument` | PAG v2 的中间数据模型（C++ 结构），Baker 产出 / Codec 输入 / Inflater 输入 | §C.5-pre |
| `bytes` | PAG v2 字节流（`.pag` 文件或内存 buffer） | §4 |
| `tgfx::Layer` | tgfx 渲染层树，最终渲染对象 | `include/tgfx/layers/Layer.h` |
| `Baker` | PAGX → PAGDocument 转换（状态 1→2），实现于 `src/pagx/pag/Baker.cpp` | §7 / §8.3bis |
| `Codec` | PAGDocument ↔ bytes 编解码（状态 2↔3），实现于 `src/pagx/pag/Codec.cpp` | §8 |
| `Inflater` / `LayerInflater` | PAGDocument → tgfx::Layer（状态 4→5），实现于 `src/pagx/pag/LayerInflater.cpp` | §9 |
| `BakeContext` | Baker 的 Context 对象，持错误收集 + 深度 + 全局计数熔断 | §8.5 |
| `DecodeContext` | Codec Decode 的 Context 对象，持 streamContext + byteOffset | §8.5 |
| `InflaterContext` | Inflater 的 Context 对象，持 warnings + layerByPath + pendingMasks + visitingComposition + currentCompositionDepth + totalInflatedLayers（P1-10 v2.19 补）| §9.4 |
| `DiagnosticCollector` | 所有 Context 的诊断收集基类（P0-3 v2.18） | §8.5 |
| `BakeResult` / `DecodeResult` / `EncodeResult` | Baker / Codec::Decode / Codec::Encode 的内部返回类型 | §8.1 / §8.3bis |
| `PAGExporter::Result` / `PAGLoader::Result` | 对外 API 的返回类型（含 ok + errors + warnings 契约） | §15.2 / §15.3 |
| `Diagnostic` | 对外诊断结构 `{code, message, byteOffset, contextIndex}` | §15.1 |
| `DiagnosticCode` / `ErrorCode` | 同一枚举的对外名 / 内部名（alias） | §G.2 + §15.1 |
| `layerPath` | Layer 在 composition 树中的结构坐标（uint32 array） | §12.1 |
| `PackedLayerPath` | layerPath 打包为 uint64 的 hashable key | §9.4 |
| `visitingComposition` | Inflater 的 composition 引用图追踪数组（防环） | §9.4 |
| `pendingMasks` | Inflater Pass 1 收集的 mask 延迟绑定列表 | §9.4 / §12.2 |
| `layerByPath` | Inflater `PackedLayerPath → tgfx::Layer*` 查表 | §9.4 |
| `CurrentStreamScope` / `CompositionVisitScope` / `ZeroCopyScope::DecodeLocal` | RAII guard（Codec SubStream / composition 环 / 零拷贝 scope） | §8.5 / §9.4 / §D.1 |

**用法**：搜索某术语的权威定义直接定位本表；搜索"谁在使用术语 X"用 `grep -n "X"`。本表与文档正文同步——PR 新增术语必须在此表加一行。

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

**版本常量**：
- `constexpr uint8_t pagx::pag::FORMAT_VERSION = 0x02;` —— **Writer 写入的版本号**。定义在 `src/pagx/pag/TagCode.h`，全代码仓库仅此一处硬编码。
- `constexpr uint8_t pagx::pag::KnownVersion = 0x02;` —— **Reader 能识别的最高版本号**（P1-1 v2.18，与 v1 `KnownVersion` 常量对称引入）。Decoder 读入 version 后立即校验 `if (version > KnownVersion) ctx->error(UnsupportedVersion=301) + reject`。未来 Writer 升级到 0x03 时，v2.0 Reader 按此校验 graceful reject，避免错字节解码。FORMAT_VERSION 与 KnownVersion 在本期同值（0x02）；升级周期中可能出现"Reader 识别 0x03 但仍写 0x02 保持后向兼容"的过渡期。

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
| bit 0-3 | `encoding` (4 bit) | `0=Constant`, `1=Hold`, `2=Linear`, `3=Bezier`, `4=Spatial`, `5-15 reserved`（P2-7 v2.18 讨论：若未来 `encoding` 池不够，**不**直接扩为 5-bit——见下方扩展通道）|
| bit 4   | `isDefault` | 1 = value 等于编译期 defaultValue，**跳过 value 字节**；0 = value 正常序列化 |
| bit 5   | `hasExt`    | 1 = 后续有 **严格 1 byte** extension 头（`extHeader`）承载新能力位；本期固定写 0 |
| bit 6-7 | reserved    | **永久约束**（v2.19 P1-16 替换 v2.18 P2-8 的 continuation 预定）：Writer **必须**写 0；旧 Reader **必须**忽略。**不设未来扩展通道**——"bit 6=1 意味 Reader 再读 1 byte" 与 §4.3 hasExt 扩字节永久红线结构同构（旧 Reader 不会多读，后续 value 起点与新 Reader 差 1 字节，错位）；真要启用必须升 FORMAT_VERSION，等价 §6.5 ③，不是本 bit 的合法通道。PropertyEncoding 池不足的合法扩展路径只有两条：(i) encoding bit 0-3 的 5-15 槽位（走 §4.4 规则 1 整 Tag skip）；(ii) 升 FORMAT_VERSION。bit 6-7 作为最终逃生门**仅用于 Reader 兼容性**，不承载未来语义 |

**`hasExt` 向前兼容纪律**（P1-A1，**永久冻结约定**）：一旦 v2.0 发布，`hasExt = 1` 场景下的 extHeader 尺寸**永久固定为 1 byte**——不得在未来任何 v2.x 升级中扩展为 2+ byte。理由：本期 §4.4 规则 2 要求旧 Reader 在遇 `hasExt=1` 时 `readUint8()` 消费一字节丢弃后继续按 encoding 处理——这把字节宽度**隐式**冻结了；若未来单方面扩成 2+ byte，旧 Reader 仍只读 1 byte，后续字节被当 value 解析，**整个 Tag** 被错位读入（远比 §4.4 规则 1 的"至多损失一个 Tag"严重）。扩展能力位的合法路径只有两条：

- **a) 启用新 encoding 值**（bit 0-3 的 5-15 段）走 §4.4 规则 1——未知 encoding → 整 Tag skip + `UnknownPropertyEncoding` warn，旧 Reader 安全降级。
- **b) 升 FORMAT_VERSION**（FileHeader 里的 `0x02` → `0x03`），v1 / v2 Decoder 直接 `UnsupportedVersion` 拒绝。

**严禁**保留 `hasExt` 位语义的同时扩 extHeader 尺寸，这是本协议的一条永久红线。

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

**规则 0**（优先级，P1-15 v2.18）：propHeader 解析按以下优先级决定分支，消除"isDefault=1 + 未知 encoding"（propHeader=0x11）的解析歧义：
  - **(a)** 先看 `encoding` 位段（bit 0-3）：非 0 → **规则 1 生效，忽略 isDefault/hasExt**；
  - **(b)** encoding=0 + `hasExt`=1 → **规则 2**；
  - **(c)** encoding=0 + hasExt=0 → 按 `isDefault` 决定是否读 value 字节。

**规则 1**（`encoding` 位段非 0，本期未知动画编码）：**按粒度分层 skip**（P0-5 v2.18，P0-R2 v2.19 字节布局落地）——
  - **Layer 级 Property**（`visible` / `alpha` / `blendMode` / `matrix` / `matrix3D` / `scrollRect`）：v2.19 起这 6 个 Property 已整体迁入**专属子 Tag** `TagCode::LayerTransform = 15`（见 §D.9，本期 Baker 恒写）；Decoder 见未知 encoding 时 `stream->setPosition(layerTransformTagEnd)`，**仅丢 transform，不丢整 LayerBlock**——Layer children / payload 仍能读出。本期字节流上 LayerTransform sub-Tag 已存在，规则 1 立即生效（v2.18 及之前为"动画期占位"，v2.19 已落地）。
  - **VectorElement 内 Property**：丢当前 Element Tag 剩余字段 → seek 到该 Element Tag 尾，同一 VectorPayload 内后续 Element 不受影响。
  - **Payload 内顶层 Property**（如 CompositionRef 的 `loopMode`）：丢当前 Payload Tag 剩余 → seek 到 Payload Tag 尾。

  推 warning `UnknownPropertyEncoding`（400），继续读下一 Tag。

**规则 2**（`hasExt` = 1，未来扩展能力）：本期 Reader 先 `readUint8()` 消费 1 byte extHeader 并丢弃（保持 stream 位置正确），然后按 `encoding` 位段的值继续处理。若同时 `encoding` 未知，优先走规则 1。**第三条红线**（P1-16 v2.18）：`extHeader` 字节的任何未来 bit 语义**不得影响后续 value 的字节数或字段顺序**——只能承载纯信号位（如 "启用某 debug flag"）。扩展字节数必须走规则 1（新 encoding 值）或文件版本升级（§6.5 ③），否则旧 Reader 按本期宽度读、新 Reader 按扩展宽度读 → 字段错位（严重于"丢一个 Tag"）。

**规则 3**（bit 6-7 非 0）：忽略（这两位是最终逃生门，Reader 不得因此报错）。

**动画期 Writer 静态 fallback 规约**（P2-9 v2.18，强制）：动画期 Writer 对所有可动画 Property 必须**同时写**：
  - propHeader encoding=Constant 的首帧 value（旧 Reader 读到这个作为静态 fallback）；
  - 独立的 `AnimationData` sub-Tag（§6.1 动画专用 160-239 段）承载 keyframe 数据——旧 Reader UnknownTagCode skip 此 Tag 只得静态首帧（不是整 Layer 消失）。
  新 Reader 读到 AnimationData Tag 时按动画 override 首帧静态值。这保证动画期文件在 v2.0 Reader 上**退化为静态首帧渲染**（非整层消失）。

**绝不尝试**"跳过若干字节继续读"——非 Constant encoding 的字节长度由未来版本决定，本期解码器不可能推断。规则 1 的兜底保证：
- 旧 Decoder 读新文件永远不会字段错位；
- 旧 Decoder 至多损失一个 Tag 的内容（视觉降级），不崩溃、不发散。

---

## 5. PAGDocument 数据模型

> **权威定义**：所有 PAGDocument 结构体、枚举、默认值的 C++ 完整定义在**附录 C**。本章只做概念描述，不再出现代码片段——AI 编码期间**按附录 C 落代码**，不看本章的伪代码推断。

### 5.1 顶层组成

- **`PAGDocument`**：全文档的根（完整字段定义见 §C.5-pre）。按字节流写入顺序（§8.2）包含：
  - `FileHeader`：画布尺寸、背景色、时间轴（frameRate/duration，本期静态均为默认值）；
  - `images` / `fonts`：资源池，由索引在 payload 中引用；
  - `compositions`：composition 列表，约定 `compositions[0]` 为 root composition。
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
| 顶层 | FileHeader / AssetTable / CompositionList / Composition / AssetItem（ImageAsset/FontAsset sub-Tag） | 1-9 | 7 (1,2,3,4,5,6,7) | 2 个（8-9）|
| Layer 及子 Tag | LayerBlock / LayerTransform / LayerMaskRef / LayerFilters / LayerStyles / 保留 | 10-19 | 5 (10,12,13,14,15) | 5 个 |
| Payload | Shape / Text / Image / Solid / Vector / Mesh / CompositionRef | 20-39 | 7 (20-26) | 13 个 |
| VectorElement | 14 种 element + 3D shape / Motion Path / AE modifier 扩展（P1-3 v2.18 扩至 80 槽）| 40-119 | 14 (40-53) | 66 个 |
| LayerFilter | 5 种 filter + 扩展（ChromaKey / Glow 等） | 120-139 | 5 (120-124)（**v2.18 迁移前是 80-99**） | 15 个 |
| LayerStyle | 3 种 style + 扩展 | 140-159 | 3 (140-142)（**v2.18 迁移前是 100-119**） | 17 个 |
| 动画专用 | 关键帧 / 插值曲线 / RangeSelector v2 等 | 160-239 | 0 | 80 个 |
| 资源扩展 | 新 asset 类型（SVG 片段 / Lottie / HDR Color 等） | 240-299 | 0 | 60 个 |
| 版本化变体 | 同语义不兼容升级（如 `FileHeaderV2`、`ShapeStyleHDR`） | 300-599 | 0 | 300 个 |
| 实验性 | 未入主干的实验功能、第三方扩展（见下"第三方注册纪律"） | 900-1021 | 0 | 122 个 |
| **ErrorMarker**（P1-5 v2.17）| Baker fatal 中止标记（见 §8.3ter）| 1022 | 1 | — |
| 保留 | 绝不分配（对齐 v1 `file.h:48` 10-bit 上限约束） | 1023 | — | — |

> **v1.3 → v2.0 TagCode 迁移**：Filter 段从 70-74 迁至 80-99，Style 段从 80-82 迁至 100-119。原因：为"Payload 段"扩展到 20-39 腾出空间。v2 尚未发布，迁移无兼容性代价。
>
> **v2.17 → v2.18 TagCode 二次迁移**（P1-3）：VectorElement 段 40-79 → 40-119（为 3D 20+ 种 + AE modifier 预留足够深度池）；LayerFilter 段 80-99 → 120-139；LayerStyle 段 100-119 → 140-159；动画专用 120-199 → 160-239；资源扩展 200-299 → 240-299。v2 尚未发布，纯数值重分段零代码成本。附录 D.11/D.12 TagCode 值同步更新。

**第三方实验段注册纪律**（P1-5 v2.18 + P2-01 v2.19 简化）：900-1021 区段采用**命名空间隔离**——任何实验 Tag body 必须以 **16 byte namespace UUID**（RFC 4122 v4）作为 body 前 16 字节前缀，Decoder 对 UUID 不认识即整 Tag 走 `UnknownTagCode=400` warn + length skip。同一 TagCode 数值 + 不同 UUID 视为不同扩展，**消除 fork 生态冲突**。官方 Tencent 扩展使用固定 UUID（见 `include/pagx/pag/ExperimentalUUID.h` 常量 `kTencentOfficialNamespaceUUID`）；第三方使用 UUID v4 自行生成即可——UUID v4 碰撞概率 2⁻¹²²，不需要向上游注册。Decoder 读入布局：

```
[TagHeader][16 byte namespace UUID][vendor 自定义 body]
```

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
- **v2 示例**：未来 HDR 支持需 Color 升为 f32×4，则分配 `TagCode::ShapeStyleHDR` 等**非必选** Tag；`FileHeaderHDR` **不走此路径**——见下方"必选 top-level 排除清单"；
- **TagCode 分配规则**：版本化变体统一占用 §6.1 的"版本化变体 300-599"段；命名后缀 `V<N>` 或语义后缀（`HDR` / `Animated`）。
- **双写优先级规约**（P1-4 v2.18）：Writer 使用 ② 路径时**必须**同时写旧 TagCode 作为 fallback（字段语义取交集）；新 Reader 遇两者并存时**以后出现为准**（LIFO override：新 Tag 在 body 中位于旧 Tag 之后）。旧 Reader 按 UnknownTagCode skip 新 Tag 仅读旧 Tag，功能降级但不崩溃。

**必选 top-level Tag 排除清单**（P0-4 v2.18 + P1-17 v2.19 扩展，强制）：**FileHeader / ImageAssetTable / FontAssetTable / CompositionList / Composition / LayerBlock 六个 Tag 必选**——旧 Reader 对这六个中任何一个走 UnknownTagCode skip = 丢失 canvas 尺寸 / 资源池 / composition 列表 / 整 composition / 整 Layer 子树 = **整文件或整 composition 不可播**（非视觉降级）。v2.19 P1-17 将 Composition 与 LayerBlock 补入原 4 项清单：Composition 缺失 = 整 composition 消失；LayerBlock 缺失 = 整 Layer 子树消失（含 Payload 与 Transform），两者均超出"字段追加兼容" 范畴。因此：
- 这六个 Tag 的**字段语义不兼容变更**禁止走 ②，必须升 FORMAT_VERSION（③）；
- 只能走 ① **字段级追加**（例：FileHeader 尾追加 `u8 featureFlags` 接 HDR 位，旧 Reader 按 TagHeader.length skip 不读但不影响 canvas width/height）；
- 其他 Tag（`ShapeStyleData` / `ElementText` / `LayerFilter*` / `LayerStyle*` / Payload 等）是**功能性 Tag**，旧 Reader skip 后仅降级单 Layer 渲染，整文件仍可播——可以走 ②。

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
  // Encode 内部构造 `EncodeSession session{&diag, &sc}` 栈对象（§8.5 P0-D v2.19）——
  // Encode 失败只产生 warnings，不产生 errors；无需调用侧传策略位。
  // PAGExporter::Options::strict 由 PAGExporter.cpp 在 Result 装配阶段通过 SeverityOf 做 warning→error 晋升，
  // 不在 Codec 内承载——保持 Codec 只管字节流 I/O，不混策略。
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
body.reserve(estimateBodySize(doc));         # P1-P4：预估大小避 log2(N) 次 realloc
WriteTag(body, FileHeader, doc.header);
WriteTag(body, ImageAssetTable, doc.images);
WriteTag(body, FontAssetTable, doc.fonts);
WriteTag(body, CompositionList, doc.compositions);
WriteEndTag(body);

构造 EncodeStream file；
file.reserve(body.length() + 16);            # 容纳 header + 1 byte compression 标志
file.write('P','A','G', 0x02);
file.writeUint32(body.length());
file.writeUint8(0x00);  // UNCOMPRESSED
file.writeBytes(body);
```

**预估公式**（`EncodeStream::estimateBodySize(doc)`）：根据 §18.8 基线 `size_ratio ≈ 0.3` 反推，`bodyBytes ≈ pagxTotalDataBytes × 0.4`（留 30% 余量）。实现放 `src/pagx/pag/EstimateSize.h`。若无 PAGX 侧的 `totalDataBytes` 提示，退化为累加 `doc.images[i].data->size() + doc.fonts[j].data.size() + 4 KB × layerCount`。50 MB 大工程文件上 encode 时间从 log2(50 MB) ≈ 26 次 realloc + memcpy 降到 1-2 次 ≈ 省 30% encode_ms。

**Encode 深度防线退化说明**（P1-04 v2.19）：v2.17 及之前，Encode 阶段通过 `EncodeContext::currentLayerDepth` / `currentVectorElementDepth` 字段在 Write Layer/VectorGroup 时重复校验一次深度（二道防线）。v2.18 P0-2 合并 EncodeContext、v2.19 P0-D 钉死 EncodeSession 后，Encode 侧不再记账深度——信任 Baker 已做 MAX_LAYER_DEPTH / MAX_VECTOR_ELEMENT_DEPTH 校验（§H.2 BakeContext::enterLayer/enterVectorGroup）。若 Baker 单测覆盖不足导致 PAGDocument 结构深度异常进入 Encode，将在 **Decode 侧** 由 DecodeContext::currentLayerDepth 按 MAX_LAYER_DEPTH 反拦推 `LayerDepthLimitExceeded=406` fatal（或 Inflater Pass 1 拒绝 inflateComposition），作为回归测试兜底——不会让畸形字节流污染下游 Inflater。

### 8.3 Decode 流程

1. 校验 magic `'P','A','G'` 与 version `0x02`，不匹配返回 error；
2. 读 `bodyLength`、`compression`（只接受 0x00）；
3. 构造 DecodeStream 覆盖 body 区间，循环读 Tag：
   ```
   const size_t bodyStart = stream.position();
   while (true) {
     TagHeader h = ReadTagHeader(stream);
     if (h.code == End) {
       // P0-15 v2.18：中途 End Tag 截断检测——bodyLength 声明的字节若未全部读完，
       // 说明上游 Baker fatal / 网络传输截断造成 body 被截断，静默接受等于鼓励未检出
       if (stream.position() - bodyStart != bodyLength) {
         ctx->warn(ErrorCode::PrematureEndTag,
                   "End Tag before bodyLength consumed (bodyLength=" +
                   std::to_string(bodyLength) + ", read=" +
                   std::to_string(stream.position() - bodyStart) + ")");
         // warn 而非 error——允许剩余字节继续处理（某些流式管线可能在 End 后附加 metadata）；
         // strict mode（§15.2 Options.strict）下 PAGExporter/PAGLoader 侧会把 409 升级为 error
       }
       break;
     }
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

### 8.3ter Baker fatal → Codec 字节流污染预警（P1-5 v2.17）

**场景**：部分上层（CLI `pagx export` 流式管线、网络导出到 server）在 Baker → Codec → bytes 链路中，Baker fatal 发生时 Codec 已经开始 `Encode` 并产出了**部分字节**（例如成功的 FileHeader + ImageAssetTable，失败发生在第 N 个 Composition 内部）。若简单丢弃这批字节，下游 Decoder 读到"legit EOF 提前"与"正常文件"无法区分。

**解决方案**：Codec 产出的字节流在 Baker fatal 时**必须**以 `TagCode::ErrorMarker` Tag 收尾——这是一个新预留 `TagCode = 1022`（见 §6.1 ErrorMarker 行）的零 body Tag：

```
TagCode::ErrorMarker = 1022
body: 空
```

**纪律**：
- Codec 对接 Baker 的桥接层：若 `BakeResult.errors` 非空（Baker fatal），Codec 在已产出的最后一个 Tag 之后写入 `WriteTag(body, ErrorMarker, {})`，然后 **不写 TagCode::End** 直接返回"污染字节流"状态。
- Decoder 读到 `ErrorMarker` 立即按 **三档语义**（P1-18 v2.19 细化，按 ErrorMarker 出现的字节流位置分派）：
  - **档 (i)** fatal 发生在 top-level Tag 之间（FileHeader/ImageAssetTable/FontAssetTable/CompositionList 等顶层序列中）：Decoder 推 `ProducerFatal=106` error + **直接 return**（无法部分恢复——缺失 images/fonts/compositions 任一将导致下游 Inflater 索引越界，丢全文件是唯一安全选项）。
  - **档 (ii)** fatal 发生在某个 Composition 的 body 内（CompositionList 已开始但某 Composition 未完整写完）：按原语义 (a)(b)(c)——推 106 warning + 停当前 Composition 后续字段 + 继续读下一个 Composition（部分恢复，便于日志上报）。
  - **档 (iii)** fatal 发生在 LayerBlock body 内（Composition 已入，某 Layer 子树未完整）：丢当前 Layer 剩余子树 + 按 LayerBlock sub-Tag 边界 seek 到兄弟 Layer 继续读；推 106 warning 附带 layerIndex。
- 新增回归测试 `Truncate.BakerErrorMarkerBeforeComposition`（覆盖档 i）/ `Truncate.BakerErrorMarkerInsideComposition`（覆盖档 ii）/ `Truncate.BakerErrorMarkerInsideLayer`（覆盖档 iii）——三档均不 crash。
- 对 CLI 的价值：`pagx export` 管线在中间件产出 Baker fatal 时，下游 `pag render` 能看到 "stream truncated by producer fatal"，而非静默拿到半成品渲染出乱象。
- 新增错误码 `ProducerFatal = 106`（Baker fatal 段追加；`contextIndex = UINT32_MAX`，byteOffset 已定位到 ErrorMarker Tag 位置）。

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

### 8.5 DiagnosticCollector / DecodeContext / BakeContext / EncodeSession

**v2.19 架构基线**（P0-C + P0-D，**权威定义**；覆盖 v2.18 "退化 / 继承" 叙述层承诺）：

- **DiagnosticCollector 基类**（v2.19 P0-C 钉死）：3 个 Context（BakeContext / DecodeContext / InflaterContext）共享 `errors` / `warnings` vector + MAX_DIAGNOSTICS cap + MAX_DIAGNOSTIC_MESSAGE_BYTES 截断逻辑——抽取为 **`struct DiagnosticCollector`** 基类，**只暴露 protected helper** `pushError(Diagnostic)` / `pushWarning(Diagnostic)`；不暴露 public `error(...)` / `warn(...)`，由子类各自定义 **3 参 public wrapper**（`(code, msg, contextIndex)`）。这样 C++ 名称查找永远落到子类 3 参版本，消除 "基类 4 参 vs 子类 3 参" name hiding 歧义。
- **`using DiagnosticCollector::pushError;` 禁用**：调用方只允许走子类 3 参 `error()/warn()`，不允许直接访问基类 `pushError/pushWarning`（protected 本就阻止外部访问，此处强调纪律）。
- **`hasError()` 归子类**：InflaterContext 规定"无 fatal"——物理上把 `errors` 字段 + `hasError()` 方法声明在 BakeContext / DecodeContext 侧，**不下沉基类**——防 Inflater 代码 `inflateCtx->hasError()` 误调。
- **EncodeSession 聚合体**（v2.19 P0-D 钉死）：v2.18 已声明"Encode 不再独立定义 EncodeContext"，v2.19 正式引入 **`struct EncodeSession { DiagnosticCollector* diag; StreamContext* sc; };`** 作为轻量 2 指针聚合。Encode 阶段所有 `Write<TagName>` 函数签名第三参改为 `EncodeSession* session`；`session->diag->pushWarning(...)` 推诊断、`session->sc` 传给 v1 EncodeStream。这保持 "4 对称" 叙述（BakeContext / DecodeContext / InflaterContext / EncodeSession 各一指针），同时物理上去掉空壳 EncodeContext。

> 架构心智（对齐 §3.2 全览表）：**3 Context + 1 EncodeSession** —— 前者用于 Bake/Decode/Inflate（持状态 + 诊断），后者用于 Encode（无状态聚合）。

```cpp
#include "codec/utils/StreamContext.h"    // v1 StreamContext

namespace pagx::pag {

// ---------- DiagnosticCollector 基类（v2.19 P0-C） ----------

struct DiagnosticCollector {
  std::vector<Diagnostic> warnings;

 protected:
  // 子类调用；基类不暴露 public error/warn，避免 C++ name hiding 坑。
  // MAX_DIAGNOSTICS cap + MAX_DIAGNOSTIC_MESSAGE_BYTES 截断统一实现。
  void pushWarning(Diagnostic d) {
    if (warnings.size() >= limits::MAX_DIAGNOSTICS) return;
    if (d.message.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      d.message.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
    }
    warnings.push_back(std::move(d));
  }

  // 默认 no-op；仅 DecodeContext / BakeContext 需要 fatal 时 override。
  // InflaterContext **不 override**——物理屏蔽 "无 fatal" 语义（Inflater 代码调 pushError 会走基类 no-op）。
  virtual void pushError(Diagnostic d) { /* no-op default */ }

  virtual ~DiagnosticCollector() = default;
};

// ---------- EncodeSession（v2.19 P0-D；Encode 阶段专用 2 指针聚合） ----------

struct EncodeSession {
  DiagnosticCollector* diag = nullptr;   // 持有方：PAGExporter / Codec::Encode
  pag::StreamContext*  sc   = nullptr;   // v1 EncodeStream 的上下文
  // Debug-only：仅 `PAGX_DEBUG_OFFSETS && PAG_BUILD_TESTS` 构建下非空；用于 PAGXBuilder/TestUtils 通过 §18.3bis CapturePagxLayout 记录关键 Tag 字节偏移。release build 恒 nullptr。
  PagxBytesLayout*     debugLayout = nullptr;
};

// ---------- DecodeContext（继承 DiagnosticCollector） ----------

struct DecodeContext : DiagnosticCollector {
  pag::StreamContext streamContext;
  pag::DecodeStream* currentStream = nullptr;   // 弱引用，Codec 主循环 dispatch 前更新；用于 byteOffset 填充

  std::vector<Diagnostic> errors;
  uint32_t currentLayerDepth = 0;
  uint32_t currentVectorElementDepth = 0;
  size_t totalAllocatedBytes = 0;

  uint32_t currentOffset() const {
    return currentStream ? static_cast<uint32_t>(currentStream->position()) : 0;
  }

  // 子类 3 参 public API——调用方永远见到这一个签名，无 name hiding 困扰。
  void error(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    // 错误硬上限：errors 达到 MAX_DIAGNOSTICS 后后续调用完全静默（不推 meta，保持幂等）。
    // 调用方按 hasError() 判定，不依赖条数。
    if (errors.size() >= limits::MAX_DIAGNOSTICS) return;
    if (msg.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      msg.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);   // P1-C：单条 message 截断
    }
    errors.push_back({code, std::move(msg), currentOffset(), contextIndex});
  }
  void warn(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    pushWarning({code, std::move(msg), currentOffset(), contextIndex});
  }
  bool hasError() const { return !errors.empty(); }

 protected:
  // override pushError 让基类 helper 用起来时也走 errors vector
  void pushError(Diagnostic d) override {
    if (errors.size() >= limits::MAX_DIAGNOSTICS) return;
    if (d.message.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      d.message.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
    }
    errors.push_back(std::move(d));
  }

 public:
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
        errors.push_back({ErrorCode::TruncatedData, msg, offset, UINT32_MAX});   // 跨 Tag sync 无 contextIndex
      }
      streamContext.errorMessages.clear();
    }
  }
};

}
```

**`currentStream` 悬空指针防护**（P1-E，强制 RAII 约束）：`currentStream` 是弱引用，可能指向嵌套 Tag 分发中临时构造的 `SubStream sub` 局部对象。**严禁**手写 `ctx->currentStream = &sub;` 裸赋值（子 sub 析构后旧值仍留在 ctx 里形成悬空指针，后续 `warn()` / `currentOffset()` 读已死对象 → UAF）。所有赋值**必须**走 RAII guard：

```cpp
namespace pagx::pag {

// 在 SubStream 作用域内临时切换 currentStream，退出自动还原。
struct CurrentStreamScope {
  DecodeContext* ctx;
  pag::DecodeStream* prev;
  CurrentStreamScope(DecodeContext* c, pag::DecodeStream* s) : ctx(c), prev(c->currentStream) {
    c->currentStream = s;
  }
  ~CurrentStreamScope() { ctx->currentStream = prev; }
  CurrentStreamScope(const CurrentStreamScope&) = delete;
  CurrentStreamScope& operator=(const CurrentStreamScope&) = delete;
};

}
```

典型用法：
```cpp
// Codec::Decode 主循环
while (true) {
  TagHeader h = ReadTagHeader(stream);
  if (h.code == End) break;
  SubStream sub = stream.slice(h.length);
  {
    CurrentStreamScope scope(ctx, &sub);   // 入域切换
    DispatchTagReader(h.code, sub, ctx);
  }                                        // 出域自动还原到 stream
  stream.seek(sub.end);
}
ctx->currentStream = nullptr;              // 主循环退出后显式置空
```

**Bridge 使用纪律**：
- v2 Codec 调用 v1 `ReadTagHeader(stream)` 前，`stream->context` **必须指向** `DecodeContext::streamContext`（而非自建 v1 context）——否则错误丢失；
- 每个 `Read<TagName>` 函数返回前（上层 Codec::Decode 主循环里）调一次 `ctx.syncFromStreamContext()`，确保 tag 级错误定位；
- 每个 tag 处理完后若 `ctx.hasError()` 为 true，**终止整个 Decode 流程**并返回 `DecodeResult{doc=nullptr, errors, warnings}`；
- Encode 侧同理：v1 `EncodeStream` 不会产生错误（只增长 buffer），`EncodeSession::sc` 作为 stream 构造参数传入，但不需要每 tag sync（§8.5 v2.19 P0-D）。

**使用纪律（通用）**：
- 所有 Read/Write 函数必须接受 `Context*` 参数；
- 深度追踪字段在 Read/Write LayerBlock / VectorGroup 的 进入/退出时 ++/--；
- DoS 记账在分配 vector/string 前累加 `totalAllocatedBytes`，超 `MAX_TOTAL_BODY_BYTES` 触发 `BodyLengthOutOfRange`。

**BakeContext**（Baker 内部使用，与 Encode/DecodeContext 对称但无字节流）：

```cpp
namespace pagx::pag {

struct BakeContext : DiagnosticCollector {
  std::vector<Diagnostic> errors;          // Baker fatal（100-199）
  uint32_t currentLayerDepth = 0;          // §H.2 MAX_LAYER_DEPTH
  uint32_t currentVectorElementDepth = 0;  // §H.2 MAX_VECTOR_ELEMENT_DEPTH

  // 全局结构性计数熔断（P0-4，§H.2 Baker 独立上限）：
  // PAGX XML 可控输入，攻击者可构造 2^31 级别的 Layer / VectorElement / image 计数；
  // 单节点校验（enterLayer 不累积跨 subtree）不足，需 Baker 级别的全局累加计数器。
  uint32_t totalLayerCount = 0;            // 累计进入的 Layer（Composition/Vector/CompositionRef/...）
  uint32_t totalVectorElementCount = 0;    // 累计进入的 VectorElement
  uint32_t totalCompositionCount = 0;      // 累计新增到 doc.compositions

  // 资源索引化（§7.2 pre-pass 结果；VectorBaker / TextBaker 查表用）
  // 三层去重（P1-7 v2.15 / P2-7 v2.16 优化）：先 byNode（快），再 byDataPtr（内嵌 tgfx::Data*，
  // 指针等价内容等价），再 byKey（URI/绝对路径 string）。三层分开避免 variant<Data*, string> key
  // 的构造/哈希开销，也更可读。
  std::unordered_map<const void*, uint32_t>        imageIndexByNode;     // Layer 1：PAGX node 指针
  std::unordered_map<const tgfx::Data*, uint32_t>  imageIndexByDataPtr;  // Layer 2a：内嵌 Data 指针
  std::unordered_map<std::string, uint32_t>        imageIndexByKey;      // Layer 2b：URI / 绝对路径
  std::unordered_map<const void*, uint32_t>        fontIndexByNode;
  std::unordered_map<const tgfx::Data*, uint32_t>  fontIndexByDataPtr;   // Embedded 字体 Data 指针
  std::unordered_map<std::string, uint32_t>        fontIndexByKey;       // "system\0family\0style"

  // LayerPath 索引（§12.1 Mask 两趟 pre-pass 结果；Baker Pass 2 查表用）
  std::unordered_map<const void*, std::vector<uint32_t>> layerPathByPagxLayer;

  void error(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    if (errors.size() >= limits::MAX_DIAGNOSTICS) return;   // §H.1 硬上限
    errors.push_back({code, std::move(msg), 0, contextIndex});   // Baker 无字节流，byteOffset=0
  }
  void warn(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    pushWarning({code, std::move(msg), 0, contextIndex});
  }
  bool hasFatal() const { return !errors.empty(); }

 protected:
  void pushError(Diagnostic d) override {
    if (errors.size() >= limits::MAX_DIAGNOSTICS) return;
    if (d.message.size() > limits::MAX_DIAGNOSTIC_MESSAGE_BYTES) {
      d.message.resize(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
    }
    errors.push_back(std::move(d));
  }

 public:

  // §H.2 检查点辅助。enter* 先累加计数并按上限返回 false 让调用侧推 fatal。
  // 纪律（P1-2 v2.16）：调用侧**必须**按"enter 返回 false 则整子树直接 return，不配对 exit"模式：
  //   if (!ctx->enterLayer()) return;
  //   /* ... recurse children ... */
  //   ctx->exitLayer();
  // exit 对 depth=0 做饱和保护，防误配对导致 uint32_t 下溢到 UINT32_MAX（下溢后所有 enter 立即
  // 失败，把 MAX_DIAGNOSTICS=1000 塞满，message 覆盖正常错误信号）。
  bool enterLayer() {
    if (currentLayerDepth >= limits::MAX_LAYER_DEPTH) {
      error(ErrorCode::CompositionCycleDepth, "MAX_LAYER_DEPTH exceeded");
      return false;
    }
    if (totalLayerCount >= limits::BAKE_MAX_TOTAL_LAYERS) {
      error(ErrorCode::StructureLimitExceeded, "BAKE_MAX_TOTAL_LAYERS exceeded");
      return false;
    }
    ++currentLayerDepth;
    ++totalLayerCount;
    return true;
  }
  void exitLayer() { if (currentLayerDepth > 0) --currentLayerDepth; }  // 饱和防下溢
  bool enterVectorGroup() {
    if (currentVectorElementDepth >= limits::MAX_VECTOR_ELEMENT_DEPTH) {
      error(ErrorCode::StructureLimitExceeded, "MAX_VECTOR_ELEMENT_DEPTH exceeded");
      return false;
    }
    if (totalVectorElementCount >= limits::BAKE_MAX_TOTAL_VECTOR_ELEMENTS) {
      error(ErrorCode::StructureLimitExceeded, "BAKE_MAX_TOTAL_VECTOR_ELEMENTS exceeded");
      return false;
    }
    ++currentVectorElementDepth;
    ++totalVectorElementCount;
    return true;
  }
  void exitVectorGroup() { if (currentVectorElementDepth > 0) --currentVectorElementDepth; }  // 饱和防下溢
  bool registerComposition() {
    if (totalCompositionCount >= limits::BAKE_MAX_TOTAL_COMPOSITIONS) {
      error(ErrorCode::StructureLimitExceeded, "MAX_COMPOSITIONS exceeded (Baker pre-pass)");  // v2.19 P1-11：alias 后统一命名
      return false;
    }
    ++totalCompositionCount;
    return true;
  }
};

}
```

**Context 总览（v2.18 简化至 3 Context + Encode 组合）**：

| Context | 用途 | errors | warnings | byteOffset | MAX_DIAGNOSTICS cap |
|---|---|---|---|---|---|
| Encode 阶段（**v2.18 无专属 Context**，直接 DiagnosticCollector + StreamContext） | 字节流编码 | —（Encoder 不产 error） | ✓ | 恒 0 | ✓（继承 DiagnosticCollector） |
| `DecodeContext : DiagnosticCollector` | 字节流解码 | ✓ | ✓ | stream->position() | ✓ |
| `BakeContext : DiagnosticCollector`   | PAGX→PAGDocument | ✓ | ✓ | 恒 0 | ✓ |
| `InflaterContext : DiagnosticCollector` | PAGDocument→tgfx::Layer | —（无 fatal） | ✓（见 §9.4）| 恒 0 | ✓ |

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
   * Ownership: Takes ownership of `doc` (moved in). Rationale: Inflater needs
   * to release individual `ImageAsset::data` shared_ptrs after
   * `tgfx::Image::MakeFromEncoded` succeeds — see §11.1 "Inflater 生命周期
   * 纪律". `const PAGDocument&` would forbid that release path. Callers who
   * need the PAGDocument after Inflate must copy it themselves (rare —
   * PAGLoader never does this).
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
  static Result Inflate(std::unique_ptr<PAGDocument> doc);
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
            * 递归 children：对每个 childPagLayer 调 inflateLayer 得 childTgfxLayer；
              **若 childTgfxLayer 为 nullptr（CompositionRef 进环 / 超 MAX_COMPOSITION_REF_DEPTH /
              MAX_INFLATED_LAYER_COUNT 熔断等降级路径）**，**必须**先调 `ctx->reserveLayerBudget()`
              再用 `tgfx::Layer::Make()` 生成空壳 Layer 占位——保持 layerPath 子索引序号连续；
              **若 reserveLayerBudget 也失败（全局预算耗尽），则该槽位不实例化任何 Layer，
              layerByPath[path] = nullptr 占位，父 addChild 跳过**。原因：空壳占位本身也是
              真实 `tgfx::Layer` 堆对象（≥64B + GPU 句柄），若不计入预算，攻击者构造
              10^7 次降级路径即可绕过 606 熔断重新爆内存（P0-3 v2.16 闭环）。
            * 然后 addChild(childTgfxLayer)（childTgfxLayer 为 nullptr 时跳过 addChild）
            * 若 maskLayerPath 非空，先 `ctx->reservePendingMaskSlot()` 校验（P0-2 v2.17），
              通过则 `pendingMasks.push_back(...)`；失败则丢弃绑定，warn 603 一次即可
            * 把 (layerPath, tgfxLayer) 写入 layerByPath（包括空壳占位 Layer；nullptr 槽位
              也写入——layerPath 是 Baker 指派的结构坐标，占位槽位仍有合法坐标，Pass 2 Mask
              查表时若目标指向 nullptr，走 603 `InflateMaskResolveFailed` 降级路径）
            * 资源解码/mask 解析失败 → 推 warning 到 InflaterContext::warnings
Pass 2: 应用 mask
        - 对 pendingMasks 逐条：查表、setVisible(true)、setMask、setMaskType
        - 查不到 / 查到 nullptr → warn `InflateMaskResolveFailed` 并跳过
Finalize（退栈前，非数据遍历）: 把 InflaterContext::warnings 一次性 move 到 Result::warnings
```

**Pass 1 C++ 骨架**（P0-4 v2.17，消除"空壳占位逻辑只在散文里"歧义）：

```cpp
std::shared_ptr<tgfx::Layer>
Inflater::inflateLayer(const Layer& src, uint32_t layerIndex, InflaterContext* ctx) {
  // 预算先行：无预算则整个子树不实例化，layerByPath 写 nullptr
  if (!ctx->reserveLayerBudget(layerIndex)) {
    ctx->layerByPath[PackLayerPath(src.layerPath)] = nullptr;
    return nullptr;
  }

  auto tgfxLayer = MakeTgfxLayerByType(src.type);          // 类型派发
  applyCommon(src, tgfxLayer.get(), ctx);                   // 通用字段
  applyPayload(src, tgfxLayer.get(), ctx);                  // subtype payload

  for (uint32_t i = 0; i < src.children.size(); ++i) {
    auto& childSrc = *src.children[i];
    auto childTgfx = inflateLayer(childSrc, i, ctx);        // 递归；失败得 nullptr
    if (childTgfx == nullptr) {
      // 降级路径仍消耗预算；预算耗尽则彻底不占位（父 addChild 也跳过）
      if (!ctx->reserveLayerBudget(i)) {
        ctx->layerByPath[PackLayerPath(childSrc.layerPath)] = nullptr;
        continue;                                            // 父 addChild 不调
      }
      childTgfx = tgfx::Layer::Make();                       // 空壳占位保连续 layerPath
    }
    tgfxLayer->addChild(childTgfx);
    ctx->layerByPath[PackLayerPath(childSrc.layerPath)] = childTgfx;
  }

  if (!src.maskLayerPath.empty() && ctx->reservePendingMaskSlot()) {
    ctx->pendingMasks.push_back({tgfxLayer, PackLayerPath(src.maskLayerPath), src.maskType});
  }
  return tgfxLayer;
}
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

Inflater 内部使用的轻量上下文，承载 warning 收集 + pendingMasks + layerByPath 查表 + composition 引用图追踪。结构与 §8.5 `DecodeContext` 对称但更精简（Inflater 无字节流，无 DoS 记账需求）。

```cpp
namespace pagx::pag {

// layerPath 打包成 uint64：最深 64 层（≤ MAX_LAYER_DEPTH），每级 10 bit（≤ MAX_CHILDREN_PER_LAYER=1024
// 已覆盖 PAG 实际最大 child 数），剩余位记 depth（6 bit）。此打包 hash 值不由攻击者可控
// 序列生成（path 是 Baker 内部拼装的结构坐标，非字节流字段），故可安全用 unordered_map。
struct PackedLayerPath {
  uint64_t value = 0;
  bool operator==(const PackedLayerPath& o) const { return value == o.value; }
};
struct PackedLayerPathHash {
  size_t operator()(const PackedLayerPath& p) const { return std::hash<uint64_t>{}(p.value); }
};
// Pack 规则：bit[0..5]=depth, bit[6..15]=level0 child index, bit[16..25]=level1, ...
// 最多 6 级足以覆盖 §H.1 的 MAX_LAYER_DEPTH=64 场景（单路径深度 ≤ 6 在实际 PAG 样本中占 99%；
// 极深路径退化为哈希碰撞，但非攻击者可构造）——超 6 级仍可通过取低 6 级 + depth 唯一化。
// P1-8 v2.16：header-only inline + span 签名消除 Baker pre-pass 10000 Layer × 1 次 vector heap 分配。
// 原 `const std::vector<uint32_t>&` 即使传已有 vector 也会触发 allocation（tail padding / move 保护），
// 换 pointer + size 签名更确定。调用点 `PackLayerPath(path.data(), path.size())` 无额外分配。
inline PackedLayerPath PackLayerPath(const uint32_t* data, size_t n) {
  PackedLayerPath out{};
  const size_t depth = std::min<size_t>(n, 6);  // 低 6 级承载
  out.value = static_cast<uint64_t>(n & 0x3F);  // bit[0..5] 存真实深度（不截断，用于区分 {1,2} 与 {1,2,0}）
  for (size_t i = 0; i < depth; ++i) {
    const uint64_t idx10 = static_cast<uint64_t>(data[i] & 0x3FF);   // 每级 10 bit，饱和截断到 1023
    out.value |= idx10 << (6 + i * 10);
  }
  return out;
}
// 便捷重载（保留 vector 接口，调用点迁移中）
inline PackedLayerPath PackLayerPath(const std::vector<uint32_t>& path) {
  return PackLayerPath(path.data(), path.size());
}

struct InflaterContext : DiagnosticCollector {
  // 告警聚合继承自 DiagnosticCollector::warnings（Inflater 无字节流，byteOffset 恒 0）
  // **不声明 errors / hasError()**——v2.19 P0-C 物理屏蔽 Inflater "无 fatal" 语义，
  // 防止基类暴露 fatal 接口被 Inflater 代码误调。

  // Mask 两趟状态（§12）——key 为 uint64 打包 path，不可被攻击者构造碰撞（path 不来自字节流，
  // 由 Baker 拼装 Layer 结构坐标生成），故用 unordered_map 比 std::map 快 ~5-10×，
  // 且免去 vector<uint32_t> 的 heap 分配（10000 layers 省 ~10000 次 malloc）。
  std::unordered_map<PackedLayerPath,
                     std::shared_ptr<tgfx::Layer>,
                     PackedLayerPathHash>  layerByPath;   // Pass 1 填充，Pass 2 查
  struct PendingMask {
    std::shared_ptr<tgfx::Layer> host;
    PackedLayerPath              targetPath;
    LayerMaskType                maskType;
  };
  std::vector<PendingMask> pendingMasks;

  // P0-2 v2.17（安全）：pendingMasks 等 pending 集合的独立上限。`reserveLayerBudget` 只管 Layer 实例
  // 总数（1e6），但单 Layer 可挂 N 个 mask；恶意构造 N×MAX_INFLATED_LAYER_COUNT 个 PendingMask
  // 条目绕过 606 熔断、塞爆 vector 内存。每次 push 前按 `limits::MAX_PENDING_MASKS` 校验，
  // 超限 warn 605/603 对应码 + 跳过 push（Pass 2 查表见 nullptr 自然降级）。
  // trackMattes / references 若后续引入独立 pending 表同此约束。
  bool reservePendingMaskSlot() {
    if (pendingMasks.size() >= limits::MAX_PENDING_MASKS) {
      warn(ErrorCode::InflateMaskResolveFailed,
           "MAX_PENDING_MASKS exceeded; mask binding dropped");
      return false;
    }
    return true;
  }

  // Composition 引用图追踪（P0-A1，强制）：防止恶意 .pag 构造 compositions[0].layers[0]=
  // CompositionRef(0) 的自环或多 composition 互引成环，导致 inflateComposition → inflateLayer
  // → inflateCompositionRef → inflateComposition 无界递归 → 栈溢出。
  // Decoder 侧走 flat Tag，不检查引用图；防护责任落 Inflater。
  //
  // Resize 契约（P2-3 v2.15 / P2-1 v2.16 修正 / P2-6 v2.17 清理）：`visitingComposition` 必须在
  // Inflate() 入口显式 `visitingComposition.assign(doc->compositions.size(), false)`。
  // 单次入口 assign，无重入承诺——§9.1 Inflate 取 unique_ptr 所有权（v2.15 P0-1），doc 在 Inflate
  // 返回后不可再用；InflaterContext 本身也不对外暴露，每次 Inflate 内部构造新 ctx。Inflate 入口
  // debug build 额外 `assert(visitingComposition.empty())`——本 ctx 刚构造，assign 前必空；否则
  // 意味着 InflaterContext 被误重入（是 bug 而非合法场景）。
  std::vector<bool> visitingComposition;           // size == doc.compositions.size()，init false
  uint32_t currentCompositionDepth = 0;            // 见 §H.1 MAX_COMPOSITION_REF_DEPTH=32

  // 全局 Layer 预算熔断（P1-3 v2.15，强制）：Decoder 单 Layer/Composition 上限合法不等于累积合法。
  // 构造 root → 10 子 → 每子 10 子 → ... 深 6 层即 10^6，单层校验全过，但 Inflater 实例化
  // 百万级 tgfx::Layer 触发 GPU 内存耗尽。totalInflatedLayers 累加到 MAX_INFLATED_LAYER_COUNT=1e6
  // 后续 inflateLayer 直接返回 nullptr + warn 606，调用侧按 §9.2 "空壳占位" 规则处理。
  uint32_t totalInflatedLayers = 0;                // 见 §H.1 MAX_INFLATED_LAYER_COUNT=1e6

  void warn(ErrorCode code, std::string msg = {}, uint32_t contextIndex = UINT32_MAX) {
    pushWarning({code, std::move(msg), 0, contextIndex});    // Inflater 无字节流，byteOffset=0
  }
  // **不 override pushError**：基类 pushError 为 no-op，Inflater 代码调不到 fatal 路径；
  // 这是对 "Inflater 无 fatal" 语义的物理约束（v2.19 P0-C）。

  // 在每个 inflateLayer 入口调；超预算返回 false 让调用侧推 606 warn + 返回 nullptr。
  // P1-1 v2.17：layerIndex 透传到 warn 的 contextIndex 字段，让消费方精确定位"第几个 Layer 爆预算"
  // （§15.1 ABI 表 606 语义：first Layer triggering budget exhaustion）。
  bool reserveLayerBudget(uint32_t layerIndex = UINT32_MAX) {
    if (totalInflatedLayers >= limits::MAX_INFLATED_LAYER_COUNT) {
      warn(ErrorCode::InflaterLayerBudgetExceeded, "MAX_INFLATED_LAYER_COUNT exceeded", layerIndex);
      return false;
    }
    ++totalInflatedLayers;
    return true;
  }
};

}
```

**进入/退出 composition 的使用纪律**（与 `CurrentStreamScope` 同构的 RAII guard）：

```cpp
namespace pagx::pag {

struct CompositionVisitScope {
  InflaterContext* ctx;
  uint32_t idx;
  bool entered = false;   // 为 false 时 dtor 不做 pop，用于循环/超深拒绝路径
  CompositionVisitScope(InflaterContext* c, uint32_t compositionIndex) : ctx(c), idx(compositionIndex) {
    if (idx >= ctx->visitingComposition.size()) return;            // Decoder 已保证索引合法
    if (ctx->visitingComposition[idx]) {
      // P1-1 v2.17：把导致环的 composition 索引（idx）透传到 Diagnostic.contextIndex，
      // 消费方按 §G.4 示例 (c) 定位"哪个 composition 在环里"。
      ctx->warn(ErrorCode::InflateCompositionCycle, "composition self-reference or cycle", idx);
      return;
    }
    if (ctx->currentCompositionDepth >= limits::MAX_COMPOSITION_REF_DEPTH) {
      ctx->warn(ErrorCode::InflateCompositionCycle, "composition ref depth exceeds limit", idx);
      return;
    }
    ctx->visitingComposition[idx] = true;
    ++ctx->currentCompositionDepth;
    entered = true;
  }
  ~CompositionVisitScope() {
    if (!entered) return;
    ctx->visitingComposition[idx] = false;
    --ctx->currentCompositionDepth;
  }
  CompositionVisitScope(const CompositionVisitScope&) = delete;
  CompositionVisitScope& operator=(const CompositionVisitScope&) = delete;
  explicit operator bool() const { return entered; }
};

}
```

典型用法（`inflateCompositionRef`）：

```cpp
std::shared_ptr<tgfx::Layer> inflateCompositionRef(uint32_t idx, InflaterContext* ctx, ...) {
  CompositionVisitScope scope(ctx, idx);
  if (!scope) return nullptr;            // 自环 / 超深 / 非法索引 → warn 已推送，返回 null 由调用侧降级
  return inflateComposition(doc.compositions[idx], ctx, ...);
}
```

**7 个 Inflater 告警码的触发点**（与附录 G.2 的 600-699 段严格一一对应）：

| ErrorCode | 触发函数/位置 | 降级行为 |
|---|---|---|
| `InflateImageDecodeFailed` (600) | `inflateImagePattern` 内 `tgfx::Image::MakeFromEncoded` 返回 null | 该 paint 的 colorSource 置 null，paint 回退为空填充 |
| `InflateFontCreateFailed` (601) | `resolveFontAsset` 内 `tgfx::Typeface::MakeFromBytes`（Embedded）或系统字体查询失败 | 降级到系统默认字体 |
| `InflateGlyphRunBuildFailed` (602) | `inflateText` 内 `GlyphRunRenderer::BuildTextBlob`/`BuildTextBlobFromLayoutRuns` 返回 null | 该 ElementText 被跳过，其外层 VectorGroup 其他子元素仍渲染 |
| `InflateMaskResolveFailed` (603) | Pass 2 `layerByPath` 查不到 `maskLayerPath` | 该 layer 的 mask 丢弃，layer 本身仍渲染 |
| `InflaterEmptyDocument` (604) | 入口检查 `compositions.empty() \|\| compositions[0]->layers.empty()` | 返回 `Result{layer=nullptr, warnings=[604]}` |
| `InflateCompositionCycle` (605) | `CompositionVisitScope` 检测到自环 / 超 `MAX_COMPOSITION_REF_DEPTH=32` | 返回 nullptr；**调用侧（父 Layer 的 children 构造路径）必须替换为 `tgfx::Layer::Make()` 空壳占位**（见 §9.2 Pass 1），layerPath 索引连续，兄弟节点继续渲染 |
| `InflaterLayerBudgetExceeded` (606) | `reserveLayerBudget()` 检测到累计 `totalInflatedLayers` 超 `MAX_INFLATED_LAYER_COUNT=1e6` | 返回 nullptr；调用侧同 605 用空壳占位 |

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

Baker 不直接写 `tgfx::TextBlob`（非跨平台可复现），改为序列化"构造 TextBlob 所需原材料"。字段集**已按主干代码（P1-5 v2.16 钉死）**：source-of-truth 为 `src/renderer/GlyphRunRenderer.h` + `src/pagx/TextLayout.h:TextLayoutGlyphRun` + `include/pagx/nodes/GlyphRun.h`，取两种输入的最小公倍字段集。

```
GlyphRunBlob {
  u8 kind                    // 0 = LayoutRun (Text->glyphData->layoutRuns 非空；对应 TextLayoutGlyphRun)
                             // 1 = GlyphRun fallback (Text->glyphRuns 非空；对应 pagx::GlyphRun)
  varU32 fontIndex           // 指向 PAGDocument.fonts；UINT32_MAX=缺失哨兵
  f32 fontSize               // GlyphRun.fontSize（默认 12）；LayoutRun 由 font.getSize() 取
  bool vertical              // LayoutRun：xforms 非空且 xforms[i] 走竖排 → true；GlyphRun：经 GlyphRun 自身的 vertical 字段
  Matrix inverseMatrix       // TextBox 子 Text 才非 identity；BuildTextBlob*(runs, inverseMatrix) 需此值做坐标逆变换

  varU32 count               // glyph 数量（≤ MAX_GLYPHS_PER_RUN）
  if kind == 0 (LayoutRun):
    # 从 TextLayoutGlyphRun 抽取：glyphs + positions + xforms 三组，count 对齐
    repeat count:
      u16 glyphId                            // glyphs[i]
      f32 posX, posY                         // positions[i]
    # xforms (RSXform) 仅在 vertical=true 时写入，走 u8 hasXforms=1 前缀
    if hasXforms:
      repeat count:
        f32 scos, ssin, tx, ty               // RSXform 四字段，承载旋转+缩放+平移

  if kind == 1 (GlyphRun fallback):
    # 从 pagx::GlyphRun 抽取：glyphs + positions + (optional) anchors/scales/rotations/skews
    f32 offsetX, offsetY                     // GlyphRun.x, y
    repeat count:
      u16 glyphId                            // glyphs[i]
      f32 posX, posY                         // positions[i]；xOffsets[i] 已合并入 posX
    # 可选动画场（存在则写 u8 bitmask 前缀指示哪些 vector 非空）
    u8 optMask                               // bit0=anchors, bit1=scales, bit2=rotations, bit3=skews
    if (optMask & 0x01) repeat count: f32 anchorX, anchorY
    if (optMask & 0x02) repeat count: f32 scaleX, scaleY
    if (optMask & 0x04) repeat count: f32 rotationDeg
    if (optMask & 0x08) repeat count: f32 skewDeg
}
```

Inflater 侧按 kind 分别构造对应输入：
- kind=0 → 重组 `TextLayoutGlyphRun{font, glyphs, positions, xforms}`，调 `GlyphRunRenderer::BuildTextBlobFromLayoutRuns(runs, inverseMatrix)`；font 由 `fontIndex` 查 `PAGDocument.fonts` 经 `tgfx::Typeface::MakeFromBytes / System` 构造。
- kind=1 → 重组 `Text` 对象的 `glyphRuns`（`pagx::GlyphRun` 列表）+ `glyphData->glyphRuns` 路径，调 `GlyphRunRenderer::BuildTextBlob(text, inverseMatrix)`。

**字段取舍说明**：
- **不**序列化 `tgfx::Font` 整对象——font 由 fontIndex 查表重建。
- **不**序列化 `glyphData->fontLineHeight`——可从 font.getMetrics().descent - ascent 运行时推算。
- **不**序列化 `GlyphRun.bounds` —— Inflater 重算（TextBlob 自带 bounds）。
- 未出现在此清单的 `pagx::GlyphRun` 字段（`xOffsets` 等）在 Baker 写入前合并到 `positions`，减少字段数。
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
enum class ImageAssetKind : uint8_t {
  Raster = 0,          // 位图（PNG/JPEG/WebP，本期唯一支持）
  Svg    = 1,          // 预留：inline SVG（P1-2 v2.18）
  Video  = 2,          // 预留：多帧 / HDR animated WebP
  Hdr    = 3,          // 预留：HDR 静态图（wide-gamut / f32 channel）
};

struct ImageAsset {
  // 原始编码 bytes（PNG/JPG/WebP）。使用 shared_ptr<const tgfx::Data> 以支持零拷贝：
  // - Baker 阶段：PAGX 的 tgfx::Data 可直接 ref（不再深拷到 std::vector<uint8_t>）
  // - Codec Encode：WriteBytes 从 data->bytes() 直接读，不走中间 buffer
  // - Codec Decode：ReadLengthPrefixedBytes 产出 vector，再 tgfx::Data::MakeAdopted 接管，
  //                 或走 MakeWithoutCopy 引用 PAGLoader 持有的 bytes
  // - Inflater：tgfx::Image::MakeFromEncoded(data) 后立即释放 ImageAsset::data，避免 decode cache + 原 bytes 双重驻留
  // 相比 v2.13 前的 std::vector<uint8_t> 语义，单张 50MB 图在 4 状态（PAGX/PAGDocument/bytes/tgfx::Image）
  // 中不再产生 2-3 份深拷贝副本；大图多实例场景峰值节省 1/2-2/3。
  std::shared_ptr<const tgfx::Data> data;
  int width = 0;
  int height = 0;
  // v2.18 P1-2 预留 kind；v2.19 P0-G 钉死：Baker 本期恒写 Raster=0；
  // Decoder 强制校验 `kind == 0`，kind != 0 但 <= 3 → warn UnsupportedFeature=104 + 回退 Raster，
  // kind > 3 → warn MalformedTag=304 + 整 ImageAsset Tag skip。
  // 严禁未来 Inflater 路径基于字节流 kind 分派不同解码器——只信 Baker 侧原生 PAGX 节点 kind。
  ImageAssetKind kind = ImageAssetKind::Raster;
};
```

**来源三分支**（对齐 LayerBuilder::convertImagePattern 545-576）：

| PAGX Image 来源 | ResourceBaker 处理 |
|---|---|
| `imageNode->data` 非空 | 直接透传 bytes |
| `filePath` 为 `data:...` URI | base64 解码后得到 bytes |
| 普通文件路径 | 读磁盘文件得到 bytes（导入阶段已 resolve 为绝对路径）|

**去重 key**（三层，P1-7 v2.15 / P2-7 v2.16 优化）：
- Layer 1（快速）：**PAGX node 指针** → 同一 PAGX Image 节点被多 Layer 引用时 O(1) 命中（`BakeContext::imageIndexByNode`）；
- Layer 2a（内嵌指针）：**内嵌 `tgfx::Data*` 指针**（`tgfx::Data` 一旦构造内容不可变，指针等价内容等价）→ `imageIndexByDataPtr` 独立表；
- Layer 2b（字符串 key）：data URI 字符串 / 文件**已解析的绝对路径** → `imageIndexByKey`（`std::unordered_map<std::string, uint32_t>`）。

ResourceBaker pre-pass 查表顺序：先 `imageIndexByNode[nodePtr]`，不命中依据 PAGX Image 来源分支——内嵌走 `imageIndexByDataPtr`，URI/文件走 `imageIndexByKey`——任一命中即复用 index，全 miss 才 `push_back` 新 ImageAsset。规模估算：图标模板被 50 Layer 共享的场景，从 50× push 降为 1× push + 49× Layer 2 命中，省 ~245 MB（50 × 5 MB PNG）+ 49 次解码。FontAsset 同。

BakeContext 对应字段见 §8.5 三层 map 定义。

**失败处理**：文件读取/URI 解码失败 → warning + 该 paint 的 `imageIndex = UINT32_MAX`（哨兵）→ Inflater 见到哨兵返回 null colorSource（对齐 LayerBuilder 行 559-561）。

**`ctx->warn` 3-arg 示例**（P0-3 v2.17，对应 §15.1 ABI 表 `ImageSourceMissing` 语义 = "imageIndex in PAGDocument::images[]"）：

```cpp
// ResourceBaker::bakeImages（§11.1 实现点；BakeContext 版本）
for (uint32_t i = 0; i < pagxDoc.images().size(); ++i) {
  auto bytes = ResolveImageBytes(*pagxDoc.images()[i]);
  if (bytes == nullptr) {
    bakeCtx->warn(ErrorCode::ImageSourceMissing,
                  "image source resolve failed",
                  i /* contextIndex = PAGX images[] 索引 */);
    doc->images.emplace_back(/* 空 ImageAsset，保持 index 连续性 */);
    continue;
  }
  // ... push_back(new ImageAsset{data=bytes, width, height});
}
```

**Inflater 生命周期纪律**（P0-P2，强制）：`inflateImagePattern` 调 `tgfx::Image::MakeFromEncoded(imageAsset.data)` 成功后，**必须立刻** `imageAsset.data.reset()` 释放源 bytes 的强引用——tgfx::Image 内部已产生解码纹理/mipmap 缓存，保留源 bytes 会造成 (源 bytes + GPU/CPU 解码缓存) 双重驻留。测试点 `PAGLoaderTest.ImageBytesReleasedAfterInflate` 断言 `Inflate` 结束后 PAGDocument 所持 `ImageAsset::data` 的 `use_count() == 1`。

### 11.2 FontAsset

```cpp
enum class FontSourceKind : uint8_t {
  System = 0,     // family + style，播放端查本地字体
  Embedded = 1,   // 内嵌字体 bytes（TTF/OTF）
  // 预留（P1-2 v2.18）：VariableEmbedded = 2（Variable Font，axes 字段非空）——Baker/Inflater 本期不产出/不识别
};

// Variable Font 轴（OpenType fvar 表）。本期不消费，仅字节流前向兼容。
struct FontAxis {
  uint32_t tag = 0;                // 4-char ASCII tag 打包 uint32（如 'wght' = 0x77676874）
  float    defaultValue = 0.0f;
  float    minValue = 0.0f;
  float    maxValue = 0.0f;
};

struct FontAsset {
  FontSourceKind kind = FontSourceKind::System;
  std::string family = {};
  std::string style = {};
  // 仅 Embedded 非空。与 ImageAsset::data 对称使用 shared_ptr<tgfx::Data> 零拷贝（P1-6 v2.16）——
  // Baker 直接 ref PAGX 源 Data；Codec Encode 直读 bytes()；Decode 产出 vector 后 MakeAdopted；
  // Inflater `tgfx::Typeface::MakeFromBytes` 成功后立即 `data.reset()` 释放源字节（decode cache 已生成）。
  std::shared_ptr<const tgfx::Data> data;
  // v2.18 P1-2 预留 axes；v2.19 P0-H 钉死：Baker 本期恒写空数组；
  // Decoder 读完 `varU32 axisCount` 后立即校验 `axisCount ≤ limits::MAX_FONT_AXES = 64`（OpenType fvar 上限），
  // 超限 error StructureLimitExceeded=105 fatal + return——防 axisCount=0x0FFFFFFF 导致 4GB OOM / heap overflow。
  // 字段级追加走 FontAsset sub-Tag 末尾 length skip（P0-R1 v2.19）。
  std::vector<FontAxis> axes = {};
};
```

**Inflater 生命周期纪律**（P1-6 v2.16，与 ImageAsset 对称）：`resolveFontAsset` 调 `tgfx::Typeface::MakeFromBytes` / `MakeFromData` 成功后必须立刻 `fontAsset.data.reset()`。测试点 `PAGLoaderTest.FontBytesReleasedAfterInflate` 断言 `Inflate` 结束后 PAGDocument 所持 `FontAsset::data` 的 `use_count() == 1`（System 字体 data 始终为 nullptr，直接跳过断言）。

**来源**：
- PAGX Font node `data` 空 → System
- PAGX Font node `data` 非空 → Embedded

**去重 key**（两层，P1-7 v2.15 / P1-6 v2.16）：
- Layer 1（快速）：**PAGX node 指针** → `fontIndexByNode`；
- Layer 2（内容）：
  - System：`(family, style, "")`
  - Embedded：以 `tgfx::Data*` 为内容 key（v2.16 FontAsset.data 已统一为 `shared_ptr<tgfx::Data>`，与 ImageAsset 对称；不再保留 `vector<uint8_t>` 的兼容路径）

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
- **P2-11 v2.18**：若 `maskTgfxLayer == host`（字节流构造 A.mask=A 自指；Baker 侧 203 无法拦截字节直喂场景）→ warn 607 `InflateMaskCycle` + 跳过 mask 赋值，host 本身仍渲染；
- 调 `maskTgfxLayer->setVisible(true)` + `tgfxLayer->setMask(maskTgfxLayer)` + `tgfxLayer->setMaskType(maskType)`；
- 查不到 / 查到 nullptr → warn 603 + 跳过。

**Pass 2.5：Mask 图环检测**（P0-16 v2.18，强制）：Pass 2 串行 setMask 无法识别 A↔B↔A 互指（A.mask=B + B.mask=A）——tgfx 后续渲染路径可能递归爆栈。Pass 2.5 对 `pendingMasks` 构有向图（node = Layer ptr，edge = host → maskTarget）做 **Tarjan SCC** 检测（N ≤ MAX_PENDING_MASKS=262144，O(N+E) 可接受）；环上任一边 → warn `InflateMaskCycle=607` + 跳过环上**所有** mask 赋值。实现伪码：

```cpp
// §12.2 Pass 2.5：Tarjan SCC 环检测
std::unordered_map<tgfx::Layer*, int> indices, lowlinks;
std::vector<tgfx::Layer*> stack;
// ... 标准 Tarjan：对每个 pendingMasks 条目的 host/target edge 跑 DFS
//     SCC 中节点数 >= 2（或单节点但 self-edge）即环；记录 edge 集合
for (auto& pm : ctx->pendingMasks) {
  if (IsOnCycle(pm.host, pm.targetPath, cycles)) {
    ctx->warn(ErrorCode::InflateMaskCycle,
              "layer mask cycle detected; edge skipped",
              /*contextIndex=*/ layerByPath索引);
    continue;   // 跳过本条赋值
  }
  // 正常 setMask 路径
}
```

**`ctx->warn` 3-arg 示例**（P0-3 v2.17，对应 §15.1 ABI 表 `InflateMaskResolveFailed=603` 语义 = "layerIndex"）：

```cpp
// LayerInflater::resolvePendingMasks（§9.2 Pass 2）
for (uint32_t i = 0; i < ctx->pendingMasks.size(); ++i) {
  auto& pm = ctx->pendingMasks[i];
  auto it = ctx->layerByPath.find(pm.targetPath);
  if (it == ctx->layerByPath.end() || it->second == nullptr) {
    ctx->warn(ErrorCode::InflateMaskResolveFailed,
              "mask target layer unresolved or placeholder",
              i /* contextIndex = pendingMasks 索引 = host layer 的逻辑编号 */);
    continue;
  }
  it->second->setVisible(true);
  pm.host->setMask(it->second);
  pm.host->setMaskType(pm.maskType);
}
```

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
- 只需要导出能力的 SDK 用户只 include `PAGExporter.h`，该头**自身不依赖 tgfx 渲染层**（不出现 `tgfx::Layer` / `tgfx::Surface` / `tgfx::DisplayList` 等渲染态符号），但**经 `PAGXDocument.h` 传递依赖** `tgfx::Data` / `tgfx::Color` / `tgfx::Matrix` 等纯数据类型——因为 PAGXDocument 的图像资源字段本身就是 `shared_ptr<const tgfx::Data>`。语义：导出路径**不拉入 tgfx 渲染子系统**，但链接仍需 tgfx 核心（Data/Matrix 等 header-only 类型其实无额外链接成本）；
- 需要渲染消费的用户 include `PAGLoader.h`，该头显式依赖 tgfx 渲染层（`tgfx::Layer`）；
- `Diagnostic.h` 同时被两个入口共用，作为基础公共结构，**零 tgfx 依赖**（纯 stdlib 类型）。

**不对外暴露**（内部实现）：`PAGDocument`、`Codec`、`LayerInflater`、`Baker`——全部作为内部实现细节放在 `src/pagx/pag/`。

**Result/Options 设计规则锚**（P2-5 / P2-6 v2.16，防未来加 facade 时命名抖动）：
- **Result 形态**：所有 facade 类采用嵌套 `ClassName::Result { ok, errors, warnings, [payload...] }` 结构，`ok ⟺ errors.empty()` 契约全局一致；轻量变体加后缀如 `PeekResult`、`ValidateResult`。未来新加 `PAGValidator`（dry-run 校验 PAG 字节流合法性不产出 Layer）时，采用 `PAGValidator::Result { ok, errors, warnings, validationTime }` 同款模式；
- **Options 独立（不合并 CommonOptions）**：`PAGExporter::Options` / `PAGLoader::Options` 保持独立定义——虽然都含 `strict` 字段，但承载字段集会随阶段分化（如 `fontMode` 仅 Exporter；`maxLayerBudget` / `renderThread` 未来可能仅 Loader），强行合并为 `pagx::CommonOptions` 会反复撕裂语义。**不合并**是刻意设计，后续评审勿重复提议。
- **Options 初始化风格**：推荐 C++20 designated initializers（`Options{.strict=true}`）而非位置聚合（`Options{FontMode::OutlineAll, true}`）——位置聚合会把字段序冻死为 ABI，未来添加新字段时老代码错位赋值。C++ 标准低于 C++20 时改用一次一行 `Options opts; opts.strict = true;`。

**`Options::strict` 使用选型**（P1-11 v2.16）：
- **用 `strict = true`**：批量导入/CI 管道场景——任何 degradation 即拒整个文件，让构建挂在产物质量问题上而非运行时发现。典型：资源预处理流水线、服务端 batch 转换；
- **保持 `strict = false`（默认）+ 调用方 post-hoc 按 `SeverityOf(d.code) == Warning` + `d.code == <特定码>` 精细分档**：UI/播放器场景——允许部分资源缺失继续渲染降级体验，只对某些严重 warning（如 `InflateCompositionCycle=605`）主动降级。典型：移动 App、Web 预览；
- `strict=true` 的晋升范围已在 §15.2/§15.3 docstring 明示（Exporter: Baker+Codec warning；Loader: 仅 Codec warning，Inflater warning 不晋升）。

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
  ProducerFatal              = 106,   // P1-5 v2.17：Decoder 读到 TagCode::ErrorMarker——
                                      // 上游 Baker fatal 已中断字节流（§8.3ter）。
                                      // 不 reset doc（已成功部分可用），但主 body 后续不再解析

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
  ImageResourceSizeExceeded  = 402,   // 图片资源单张字节数超 MAX_IMAGE_BYTES（contextIndex = imageIndex）
  StringInvalidUtf8          = 403,
  PathVerbLimitExceeded      = 404,
  GlyphCountLimitExceeded    = 405,
  LayerDepthLimitExceeded    = 406,
  InvalidEnumValue           = 407,
  FontResourceSizeExceeded   = 408,   // 字体资源单份字节数超 MAX_FONT_BYTES（contextIndex = fontIndex；v2.16 由 402 拆出）
  PrematureEndTag            = 409,   // P0-15 v2.18：中途遇到 TagCode::End=0 但 bodyLength 声明的字节尚未读完——上游字节被截断

  // Inflater warning
  InflateImageDecodeFailed   = 600,
  InflateFontCreateFailed    = 601,
  InflateGlyphRunBuildFailed = 602,
  InflateMaskResolveFailed   = 603,
  InflaterEmptyDocument      = 604,   // compositions[0] 缺失或为空，无可渲染内容；Result.layer == nullptr
  InflateCompositionCycle    = 605,   // compositions 引用图存在自环或链式环，或引用深度超 MAX_COMPOSITION_REF_DEPTH；该 CompositionRef 降级为空 Layer
  InflaterLayerBudgetExceeded = 606,  // 累计实例化 tgfx::Layer 超 MAX_INFLATED_LAYER_COUNT=1e6；子树降级为空壳 Layer
  InflateMaskCycle           = 607,   // P0-16 v2.18：Layer mask 循环（A.mask=B + B.mask=A，或自指 A.mask=A 的字节流路径）；环上 mask 全部跳过
};

/**
 * Structured diagnostic emitted by export/load operations.
 *   - `code`         : machine-readable code; callers should dispatch on this.
 *                      NUMERIC VALUE is part of the stable ABI — callers MAY
 *                      persist the underlying uint16_t in logs/crash reports
 *                      and expect it to retain semantic meaning across pagx
 *                      versions. Deprecating a code means leaving its number
 *                      reserved, never reusing it for new semantics.
 *   - `message`      : optional human-readable context; may be empty.
 *                      WARNING: `message` text is unstable English debug text,
 *                      NOT a translation key. Do NOT persist or switch on it;
 *                      future pagx may introduce a separate `messageKey` field
 *                      for i18n (see §G.4).
 *   - `byteOffset`   : decoder path = stream->position() at error time; all
 *                      other paths (baker/encoder/inflater) = 0.
 *   - `contextIndex` : STRUCTURED resource/layer reference for codes where
 *                      the caller typically needs to identify WHICH asset
 *                      failed (see table below). UINT32_MAX means "not
 *                      applicable / unknown". Callers MAY dispatch on this.
 *                      **MANDATORY GUARD (P1-2 v2.17)**: before using
 *                      contextIndex as an array index, callers MUST check
 *                      `d.contextIndex != UINT32_MAX` — some code paths
 *                      (bridge push across Tag streams, fatal error without
 *                      resource association) push UINT32_MAX sentinel even
 *                      for codes in the semantic table. Raw indexing without
 *                      the guard triggers UB (OOB read) on sentinel rows.
 *
 * `code → contextIndex` semantic table (stable ABI):
 *   200 ImageSourceMissing         → imageIndex in PAGDocument::images[]
 *   201 FontSourceMissing          → fontIndex in PAGDocument::fonts[]
 *   202 MaskTargetMissing          → layerIndex in enclosing Composition
 *   203 MaskSelfReference          → layerIndex
 *   206 TextGlyphDataEmpty         → layerIndex (the ElementText holder)
 *   208 GlyphRunKindInferred       → layerIndex
 *   306 InvalidCompositionIndex    → compositionIndex AS READ FROM BYTES (raw value).
 *                                    May be ≥ doc.compositions.size()! Consumers MUST
 *                                    range-check before indexing into compositions[].
 *   402 ImageResourceSizeExceeded  → imageIndex
 *   407 InvalidEnumValue           → raw enum byte (0-255 zero-extended)
 *   408 FontResourceSizeExceeded   → fontIndex
 *   600 InflateImageDecodeFailed   → imageIndex
 *   601 InflateFontCreateFailed    → fontIndex
 *   602 InflateGlyphRunBuildFailed → layerIndex
 *   603 InflateMaskResolveFailed   → layerIndex
 *   605 InflateCompositionCycle    → compositionIndex of the self/cyclic reference
 *   606 InflaterLayerBudgetExceeded → layerIndex of the first Layer that triggered budget exhaustion in the failing subtree
 *   607 InflateMaskCycle            → layerIndex of any host layer participating in the SCC mask cycle (v2.19 P1-09)
 *   106 ProducerFatal               → UINT32_MAX (byteOffset locates ErrorMarker Tag position, v2.19 P1-09)
 *   409 PrematureEndTag             → UINT32_MAX (byteOffset locates End Tag position, v2.19 P1-09)
 *   All other codes                → UINT32_MAX (not meaningful)
 */
struct Diagnostic {
  DiagnosticCode code         = DiagnosticCode::Ok;
  std::string    message;
  uint32_t       byteOffset   = 0;
  uint32_t       contextIndex = UINT32_MAX;
};

/**
 * Format a Diagnostic as "[<CodeName>] <message>" with a " @0x<hex>" suffix
 * when byteOffset is non-zero, and a " #ctx=<n>" suffix when contextIndex is
 * not UINT32_MAX. Example:
 *   "[TruncatedData] unexpected EOF @0x4a2c"
 *   "[LayoutNotApplied] document must have applyLayout() called first"
 *   "[ImageSourceMissing] path not found #ctx=3"
 *   "[InflateCompositionCycle] cycle @0x14 #ctx=0"
 */
std::string FormatDiagnostic(const Diagnostic& d);

// ---- 辅助分类函数（cheap adds）——函数实现可替换，返回的 enum 值 + code→段映射是 ABI ----

/** Severity category derived from code segment. */
enum class DiagnosticSeverity : uint8_t { Fatal, Warning };
DiagnosticSeverity SeverityOf(DiagnosticCode code);

/** Stage category derived from code segment. */
enum class DiagnosticStage : uint8_t { Baker, Codec, Inflater };
DiagnosticStage StageOf(DiagnosticCode code);

// ---- Version query（cheap add）----
struct VersionInfo {
  uint16_t    formatVersion;   // PAG v2 = 0x0002 (low byte = major, high byte = minor).
                               // Kept in sync with PeekResult::formatVersion — same width and semantics.
  const char* gitSha;          // build-time git SHA, short. Process-lifetime pointer into .rodata.
  const char* buildDate;       // __DATE__.             Process-lifetime pointer into .rodata.
};
VersionInfo Version();

}  // namespace pagx
```

**ABI 纪律**：
- `DiagnosticCode` 是 ABI 承诺的一部分——枚举值**只能在段内追加**，不得复用已删除编号，不得跨段迁移；**数值稳定**——调用方可持久化 `uint16_t(code)` 到日志/崩溃报告，跨版本保留语义；
- 内部 `pagx::pag::ErrorCode` **是** `pagx::DiagnosticCode` 的 `using` 别名（见附录 G.2），保持单一真相；
- 新增内部错误码等同于新增公共枚举值，走"段内追加"即可，不升 FORMAT_VERSION；
- `SeverityOf` / `StageOf` **函数实现可随版本替换**（例如从手写 switch 改查表），但**返回的 enum 值 + code→段映射是 ABI**——Fatal 恒对应 100-199/300-399/500-599 段，Warning 恒对应 200-299/400-499/600-699 段，`DiagnosticSeverity` / `DiagnosticStage` 枚举值数字恒定。调用方可安全 `switch(SeverityOf(c))` 且结果跨版本稳定；
- `Diagnostic::contextIndex` 的"code → contextIndex 语义"表（见上方 struct 注释）是 ABI——新增 code 若需要 contextIndex 语义必须在此表补一行；已发布 code 的 contextIndex 含义不得修改；
- `Diagnostic::message` 是**不稳定的英文调试文本**，不得持久化或做字符串 switch——未来可能新增 `messageKey` 字段走 i18n 路径。

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
     * If true, all Diagnostic whose `SeverityOf(code) == Warning` produced by
     * Baker / Codec stages during this export are promoted to `errors` (which
     * causes `ok == false` per the Result invariant). Inflater warnings are
     * not reachable from PAGExporter (export path never runs Inflater), so
     * this flag only affects Baker (200-299) + Codec (400-499) warning codes.
     * Default false.
     *
     * Implementation: PAGExporter.cpp inspects Diagnostic.code via SeverityOf
     * at the boundary between internal contexts and the returned Result —
     * there is no internal `strictMode` state on the Encode/Bake contexts.
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
   *
   * **P2-3 v2.17（严格交叉引用）**：本 Result 的 ok / errors / warnings 契约与
   * §15.3 `PAGLoader::Result` 对称：`ok == true ⟺ errors.empty()`，不论产出
   * 是字节（Exporter）还是 Layer（Loader）。新增 Diagnostic 字段或修改
   * errors/warnings 语义时必须两处同步更新，否则破坏 ABI 对称性。
   */
  struct Result {
    bool ok = false;                         // Derived: ok ⟺ errors.empty(). Filled by implementation;
                                             // callers MUST NOT write to this field — single source of truth is errors.
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
   * multiple threads, but the PAGXDocument itself (and any Data/strings it
   * holds) must not be mutated during export. Reentrancy guarantees
   * correctness, not parallel throughput — downstream OS font/image
   * subsystems may serialize. After this function returns, the input document
   * may be destroyed; the returned Result is fully self-contained.
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

**依赖约束**（强制）：`include/pagx/PAGExporter.h` **自身不 include 任何 tgfx 渲染层头文件**（`tgfx/layers/*`、`tgfx/gpu/*`、`tgfx/core/Surface.h` 等），但经 `PAGXDocument.h` 传递依赖 tgfx 核心数据类型（`tgfx::Data` / `tgfx::Color` / `tgfx::Matrix`）。保证：**渲染子系统不被拉入 export-only 用户的 include 图**。内部实现头（`src/pagx/pag/` 下）可自由 include 任何 tgfx 头。

**Options 初始化风格纪律**（P2-6 v2.15）：推荐使用 C++20 designated initializers（`Options{.strict=true}`）而非位置聚合（`Options{FontMode::OutlineAll, true}`）。位置聚合会把字段**序**冻死为 ABI——未来添加新字段时任何老代码 `Options{a, b}` 都会错位赋值。若项目 C++ 标准低于 C++20，改用一次一行 `Options opts; opts.strict = true;` 风格，仍避免位置依赖。

### 15.3 `include/pagx/PAGLoader.h`

```cpp
// Copyright (C) 2026 Tencent. All rights reserved.
#pragma once

// IMPORTANT: This header depends on tgfx. Include it only if your module
// consumes rendered tgfx::Layer trees. For export-only use cases that do
// not need rendering, include <pagx/PAGExporter.h> instead — that header
// is tgfx-free.
//
// ABI model: SAME-BUILD. pagx-pag and tgfx MUST be built together in the
// same compilation unit chain. Do NOT mix a prebuilt pagx-pag with a
// different-version tgfx — tgfx::Layer's vtable/layout is not version-
// stable. Rebuild pagx-pag whenever you upgrade tgfx. See §I.4 for details.

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
  struct Options {
    /**
     * If true, Codec warnings (400-499) encountered during Decode are promoted
     * to `errors` (which causes `ok == false`). Inflater warnings (600-699)
     * are NOT promoted — they describe per-layer degradations that callers
     * typically want visible but non-fatal (e.g. a missing image shouldn't
     * reject the whole file). Default false.
     *
     * Implementation: PAGLoader.cpp inspects Diagnostic.code via SeverityOf +
     * StageOf at Result assembly — no internal strictMode on DecodeContext.
     */
    bool strict = false;
  };

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
   *
   * **P2-3 v2.17（严格交叉引用）**：本 Result 的 ok / errors / warnings 契约与
   * §15.2 `PAGExporter::Result` 对称。`ok == true ⟺ errors.empty()` 在两处
   * 保持一致；修改语义必须两处同步。
   */
  struct Result {
    bool ok = false;                      // Derived: ok ⟺ errors.empty(). Filled by implementation;
                                          // callers MUST NOT write to this field — single source of truth is errors.
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
   * Memory ownership: `data` only needs to be valid for the duration of this
   * call; the returned Result retains NO reference to it. Callers may free or
   * unmap the backing storage immediately after return.
   *
   * Thread-safety: Reentrant. Each call produces an independent Layer tree;
   * safe to invoke concurrently. The returned Layer tree inherits tgfx::Layer's
   * thread-affinity rules — treat it as a single-owner, single-render-thread
   * resource. Reentrancy guarantees correctness, not parallel throughput:
   * tgfx's font/image global caches may serialize concurrent Inflate calls;
   * do not exceed CPU core count for best throughput.
   */
  static Result LoadFromBytes(const uint8_t* data, size_t length,
                              const Options& options = {});

  /**
   * Convenience wrapper. Reads the file then calls LoadFromBytes.
   * On read failure, Result has ok=false and a single FileReadFailed error.
   * The file handle is closed before return; callers may delete/rename
   * `filePath` immediately after.
   */
  static Result LoadFromFile(const std::string& filePath,
                             const Options& options = {});

  /**
   * Lightweight header-only peek. Decodes only the PAG v2 FileHeader (canvas
   * size + version) without reading the body. Use for thumbnail lists or
   * bulk-scanning scenarios where the full Layer tree is unnecessary.
   *
   * Two overloads:
   *   - `Peek(data, length)`: caller-supplied in-memory buffer. Costs ~O(1)
   *     (reads <64 bytes of `data`). `data` only needs to be valid during the
   *     call; PeekResult retains no reference.
   *   - `Peek(filePath)`: opens the file, pread-only's the first ~64 bytes,
   *     then closes. Bulk-scanning 1000 thumbnails costs ~1000 × 64 B of I/O,
   *     NOT 1000 × fileSize — this is the key reason the overload exists.
   *     Use this instead of `LoadFromFile` + discard when only the header is
   *     needed.
   *
   * Failure modes for Peek(filePath):
   *   - open/read fails (file not found, permission denied, I/O error)
   *       → ok=false + single `FileReadFailed` (307) error.
   *   - open succeeds but file has <9 bytes (below PAG FileHeader minimum)
   *       → ok=false + single `TruncatedData` (303) error.
   *   - header bytes present but magic/version invalid
   *       → ok=false + `InvalidMagic` (300) / `UnsupportedVersion` (301) error.
   *
   * PeekResult field validity:
   *   `canvasWidth` / `canvasHeight` / `formatVersion` are ONLY meaningful when
   *   `ok == true`. On ok==false they are 0 (not a special sentinel) — callers
   *   MUST gate on ok first and not compare the defaulted fields to known
   *   version numbers (e.g. `if (r.formatVersion >= 2)` without `r.ok` first
   *   will misread failures as "old version").
   *
   * TOCTOU: `Peek(filePath)` and a subsequent `LoadFromFile(filePath)` are NOT
   * atomic. If the file is modified between the two calls (concurrent writer
   * / rename / truncate), the two calls' observed bytes may differ — callers
   * that need a consistent pair should either (a) `LoadFromBytes` with a
   * caller-cached `std::vector<uint8_t>`, or (b) accept a possible re-read
   * discrepancy and retry on failure. pagx does NOT internally cache files.
   *
   * @param data    Pointer to PAG v2 bytes. Null or length<9 → ok=false + TruncatedData.
   * @param length  Byte count.
   * @param filePath  Path to a PAG v2 file on disk.
   * @return        PeekResult.
   */
  struct PeekResult {
    bool     ok = false;             // Derived: ok ⟺ errors.empty(). Implementation-filled;
                                     // callers MUST NOT write to this field.
    int      canvasWidth  = 0;       // Valid only when ok==true.
    int      canvasHeight = 0;       // Valid only when ok==true.
    uint16_t formatVersion = 0;      // Valid only when ok==true. 0x0002 for PAG v2.
    std::vector<Diagnostic> errors;
    std::vector<Diagnostic> warnings; // Reserved; currently always empty. Future minor versions
                                      // may push degradation hints (e.g. minor-version downgrade).
  };
  static PeekResult Peek(const uint8_t* data, size_t length);
  static PeekResult Peek(const std::string& filePath);
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
  // Inflate 取 doc 的所有权（见 §9.1 docstring）——decoded.doc 在此点后不可再用。
  auto inflated = pagx::pag::LayerInflater::Inflate(std::move(decoded.doc));
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
├── DiagnosticCollector.h         # v2.19 P1-13：3 Context 共享基类（protected pushError/pushWarning helper；见 §8.5）
├── EncodeSession.h               # v2.19 P1-13：Encode 阶段 2 指针聚合（DiagnosticCollector* + StreamContext*；见 §8.5）
├── Baker.h / Baker.cpp           # 顶层 Bake 编排（内部头）
├── BakeContext.h / .cpp          # 资源索引、mask 上下文、错误/告警（继承 DiagnosticCollector）
├── DecodeContext.h / .cpp        # v2.19 P1-13：Decode 阶段 Context（继承 DiagnosticCollector + currentStream + syncFromStreamContext；权威定义见 §8.5）
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
└── InflaterContext.h             # warnings + pendingMasks + layerByPath + visitingComposition + currentCompositionDepth + totalInflatedLayers（继承 DiagnosticCollector；权威定义见 §9.4）

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
          │  Fuzz (Layer 6)     │  ← §18.3ter，Phase 12 门槛
          └──────────┬──────────┘
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

> **实施前置**：Day-1 smoke **不依赖** §19 阶段 9 才交付的 `support/RenderUtil.cpp`——使用内联的静态自由函数 `RenderLayerLocal(context, doc, layer)`（~20 行，直接 `tgfx::Surface::Make + DisplayList + root->addChild + render`；对齐 §2.1 禁 lambda 纪律，不使用 `[&]{}` 捕获式 lambda）。阶段 9 统一的 RenderUtil 落地后，smoke 已经被删，不构成依赖悖论。`tgfx::Context*` 通过项目既有的 `pag::TestUtils::GetContext()`（见 `test/src/utils/TestUtils.h`）在 fixture SetUp 里获取。

```cpp
static std::shared_ptr<tgfx::Surface> RenderLayerLocal(
    tgfx::Context* context, const pagx::pag::PAGDocument* doc,
    std::shared_ptr<tgfx::Layer> layer) {
  auto surface = tgfx::Surface::Make(context, doc->width, doc->height);
  tgfx::DisplayList list;
  list.root()->addChild(layer);
  list.render(surface.get(), /*clearAll=*/true);
  return surface;
}

TEST(PAGRenderSmoke, LayerBuilderIsDeterministic) {
  auto context = pag::TestUtils::GetContext();
  auto doc = PAGXImporter::FromFile("spec/samples/<pick_one>.pagx");
  doc->applyLayout();

  auto r1 = LayerBuilder::Build(doc.get());
  auto s1 = RenderLayerLocal(context, doc.get(), r1);
  tgfx::Bitmap bitmap1(s1->width(), s1->height(), false, false);
  tgfx::Pixmap pixmap1(bitmap1);
  ASSERT_TRUE(s1->readPixels(pixmap1.info(), pixmap1.writablePixels()));

  auto r2 = LayerBuilder::Build(doc.get());
  auto s2 = RenderLayerLocal(context, doc.get(), r2);
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

### 18.3pre PAGXBuilder（`support/PAGXBuilder.h`，P0-6，Phase 2 前置交付）

构造 `pagx::PAGXDocument` 对象树的 fluent 测试工具。Phase 2 `BakeContextTest` / `ResourceBakerTest` / `PAGXBuilderTest` 以及后续所有 Baker 单测都基于此工具手构最小 DOM，**不依赖磁盘样本**。本节列出必要 API——实现的空间由开发者决定，但签名必须与下列一致（测试代码按此写）：

```cpp
namespace pagx::test {

// Fluent builder：每个 with* 返回当前 builder 引用；Build* 产出 unique_ptr 并 applyLayout()。
// 所有 with* 的参数顺序 = 对应 PAGXDocument/Layer/... 的必填 + 默认可选。
class PAGXBuilder {
 public:
  // 根：canvas 尺寸 + frameRate（默认 24）+ duration（默认 60 帧）
  static PAGXBuilder Doc(float width, float height);
  PAGXBuilder& withFrameRate(int num, int den) { /* Ratio */ return *this; }
  PAGXBuilder& withDuration(uint32_t frames)    { return *this; }
  PAGXBuilder& withBackground(tgfx::Color c)    { return *this; }

  // Composition 作用域。每次调用 openComposition 切换当前 composition 上下文；
  // compositions[0] 为 root。closeComposition() 返回 root 作用域。
  PAGXBuilder& openComposition(const std::string& id);
  PAGXBuilder& closeComposition();

  // Layer：以 lambda 避免引入 lambda 风格，改用 nested builder 的 enter/exit 形态：
  PAGXBuilder& enterLayer(pagx::LayerType type);     // 进入一个新 Layer，成为当前父
  PAGXBuilder& exitLayer();                          // 退回上一级父
  PAGXBuilder& withName(const std::string& name)           { return *this; }
  PAGXBuilder& withMatrix(const tgfx::Matrix& m)           { return *this; }
  PAGXBuilder& withAlpha(float a)                          { return *this; }
  PAGXBuilder& withMask(pagx::LayerMaskType t, const std::string& targetPath);
  // P1-07 v2.19：`targetPath` BNF：
  //   <path>     ::= "root" ( "/" <uint32> )*
  //   例："root"（当前 composition 的 root 本身，罕见）、"root/0"（root 的第 0 个 child）、
  //       "root/0/1"（root 的第 0 个 child 的第 1 个 child）。
  //   本期限制：仅同 composition 内相对根寻址；跨 composition（"otherComp/0"）**未支持**，
  //   传入 assert 死——PAGDocument::maskLayerPath 本就不跨 composition，PAGXBuilder 与之对齐。
  //   解析失败（非法 token / 越界 child index）→ applyLayout() 时 fatal。

  // VectorElement（在当前 Layer 是 VectorLayer 时）
  PAGXBuilder& enterVectorGroup();
  PAGXBuilder& exitVectorGroup();
  PAGXBuilder& addRectangle(const tgfx::Rect& r);
  PAGXBuilder& addEllipse(const tgfx::Point& center, float rx, float ry);
  PAGXBuilder& addPath(const tgfx::Path& p);
  PAGXBuilder& addFill(tgfx::Color color);
  PAGXBuilder& addStroke(tgfx::Color color, float width);

  // Text（在当前 Layer 是 TextLayer 时）
  PAGXBuilder& withText(const std::string& utf8, const std::string& fontFamily,
                        const std::string& fontStyle, float fontSize,
                        tgfx::Color fillColor);

  // Image（在当前 Layer 是 ImageLayer 时）
  PAGXBuilder& withImageBytes(std::shared_ptr<const tgfx::Data> encodedBytes,
                              int width, int height);
  PAGXBuilder& withImageFile(const std::string& filePath);   // Phase 2+ 测试才会用到；走 PAGX 的 filePath resolve

  // Composition reference layer
  // 语义（P1-7 v2.16 钉死）：**必须**先 `enterLayer(LayerType::CompositionReferenceLayer)` 进入新 layer
  // 上下文，再调 asCompositionRef 在当前 layer 上设置目标 compositionId。与其他 setter（withName /
  // withMatrix / withAlpha / withMask）时序一致，不是独立进入新 layer。调用方式：
  //   builder.enterLayer(LayerType::CompositionReferenceLayer)
  //          .withName("ref1")
  //          .asCompositionRef("targetCompId")
  //          .exitLayer();
  // 若 PAGXBuilder 当前 layer 不是 CompositionReferenceLayer，asCompositionRef 内部 assert + 返回 *this
  // 不改变状态（debug build 死；release build silent skip），让测试作者立即看到错误用法。
  //
  // P1-07 v2.19 延迟 resolve 契约：`asCompositionRef` **仅登记目标 id 字符串到当前 layer 的符号表槽**，
  // 不即时查找目标 Composition——target id 可以**指向尚未 openComposition 的 composition**（前向引用）。
  // 真正的 id → Composition* 指针解析发生在 `Build()` 内部的 `applyLayout()` 阶段（批量扫 symbol table）。
  // 若 applyLayout 时 target id 未在任何 openComposition 中出现 → fatal（debug assert + Build 返回 nullptr）。
  // 本规约支持 `MakeCompositionRefCycle({"A", "B"})` 这类双向互指场景（A.asRef(B) + B.asRef(A)）。
  PAGXBuilder& asCompositionRef(const std::string& targetCompositionId);

  // 产出：applyLayout 后返回独占所有权的 PAGXDocument。
  std::unique_ptr<pagx::PAGXDocument> Build();
  std::unique_ptr<pagx::PAGXDocument> BuildWithoutLayout();   // 仅用于测试 LayoutNotApplied=100 路径
};

}  // namespace pagx::test
```

**最小用法（Phase 2 `BakeContextTest.SimpleFill`）**：

```cpp
auto doc = PAGXBuilder::Doc(200, 200)
              .openComposition("root")
                .enterLayer(LayerType::VectorLayer)
                  .withName("square")
                  .withMatrix(tgfx::Matrix::MakeTrans(50, 50))
                  .enterVectorGroup()
                    .addRectangle(tgfx::Rect::MakeWH(100, 100))
                    .addFill(tgfx::Color::FromRGBA(255, 0, 0, 255))
                  .exitVectorGroup()
                .exitLayer()
              .closeComposition()
              .Build();
```

**纪律**：
- `Doc()` 是唯一入口；无 `operator()(...)` 和 copy ctor（按 §Code.md 禁 lambda/异常纪律），仅 move；
- 所有 fluent setter 返回 `PAGXBuilder&`，**不返回** nested builder——保持调用点线性可读；
- `Build()` **必须** 调用 `applyLayout()`，避免 Baker 侧因 `LayoutNotApplied=100` fatal 路径掩盖真实测试意图；若测试本身要验 100 码，另设 `BuildWithoutLayout()`。
- 所有填入字段按 PAGX 默认值语义对齐——测试代码不该显式写默认值，以便 defaultValue 变更时测试自动跟随。
- **公有/私有可见性纪律**（P2-2 v2.17）：所有 fluent 方法 `public`；内部状态（`currentComposition_ / currentLayer_ / docRoot_ / vectorGroupStack_` 等）一律 `private`——测试代码**不直接访问字段**，只通过 fluent API；这样 `PAGXBuilder` 实现即可在未来无缝替换（如改为字节流缓存）而不破坏测试。

**配套测试** `PAGXBuilderTest.cpp`（Phase 2 交付，P2-9 v2.16 扩至 ≥15 条）：

核心用例（7 条）——每条验证 builder 构出的 DOM 字段与手工构造等价：
- `Minimal.Root`：仅 `Doc(W, H).Build()`，验证 root composition 存在且 width/height 正确
- `Composition.Nested`：root composition 内 `enterLayer(CompositionReferenceLayer).asCompositionRef("child")` + `openComposition("child")`，验证引用指针正确解析
- `Layer.Mask.ByPath`：`withMask(type, "root/0/1")` 生成 maskLayerPath 字段与手构路径一致
- `VectorGroup.Depth`：`enterVectorGroup` × N 嵌套，验证深度与 layerType 树对齐
- `Text.BasicRun`：`withText(...)` 生成 Text node 带 GlyphRun
- `Image.EmbeddedBytes`：`withImageBytes(data)` 生成 Image node + data 引用
- `Build.LayoutApplied`：`Build()` 后 `doc->isLayoutApplied() == true`

扩展用例（P2-9 v2.16 补齐，≥8 条）——覆盖剩余 setter / 边界：
- `Doc.DefaultFrameRateDuration` / `Doc.WithBackgroundColor`
- `Timeline.WithFrameRateAndDuration`（`withFrameRate(30, 1).withDuration(120)`）
- `Layer.WithAlpha` / `VectorGroup.Ellipse` / `VectorGroup.Path` / `VectorGroup.Stroke`
- `CompositionRef.TimingMismatch`（进错 LayerType 调 asCompositionRef 触发 assert，仅 debug build 生效）
- `Build.WithoutLayout`（验证 `BuildWithoutLayout()` 产出 `isLayoutApplied==false`，供 `LayoutNotApplied=100` 测试使用）

**Phase 2 test deliverable 硬定数**（P1-8 v2.17）：**Phase 2 必须 ship 的 `PAGXBuilderTest` 用例数 = 13 条**（7 核心 + 6 扩展）。`BakeContextTest` / `ResourceBakerTest` 的 PAGXBuilder 消费算作 *integration 验证*，不计入本 13 条。Phase 2 exit 清单在交付判定时按此计数，少于 13 条视为 Phase 2 不通过。

**"trivial passthrough 豁免"**：`withFrameRate / withDuration / withBackground / closeComposition / exitLayer / addEllipse / addPath / addStroke` 若实现为单行赋值，测试可合并进上述扩展用例内断言，不需独立 case。

### 18.3nix PAGDocumentEquals（`support/PAGDocumentEquals.h`，P0-9 v2.18，Phase 2 前置交付）

Baker 单测断言目标是"Baker 产出 PAGDocument 与期望 PAGDocument 等价"——PAGDocument 含 200+ 个 `Property<T>` 字段（附录 C.5-C.9）。**没有此 helper 时 BakeContextTest/ResourceBakerTest 里每个用例都要手写 200+ 字段 EXPECT_EQ**。

```cpp
namespace pagx::test {

/**
 * 深度比较两个 PAGDocument 所有字段（header / images / fonts / compositions 递归），返回是否等价。
 * @param a / b       两个 PAGDocument，空指针同视为"空文档"等价；单空单非空视为不等价
 * @param diffPath    可选——若返回 false，写入"第一个不等字段的路径"（如 "compositions[0].layers[3].matrix"）
 * @return true 等价，false 某字段不等
 *
 * 比较规则：
 *   - 数值字段：精确等（浮点用 `std::bit_cast<uint32_t>` 防 -0.0 vs +0.0 误判，P2-11 v2.18）；
 *   - shared_ptr<const tgfx::Data>（P1-12 v2.19 三子句钉死，消除 Inflater reset 后比较的假阳性）：
 *       (i)  `a->data == nullptr && b->data == nullptr` → **等价**（Inflate 后双侧源字节均已释放，合法等价状态）；
 *       (ii) 单侧 nullptr（如 Baker 产出 vs Inflate 后）→ **不等**（生命周期阶段不同不应互比）；
 *       (iii) 双侧非空 → 先比 `size()`；相等则对 ≤ 4 MB 小图走字节级 `memcmp`，> 4 MB 大图走 SHA1
 *             hash 比较（fast-path，避免 50 MB 图 O(N) memcmp 导致 test timeout）；hash 相等即等价，
 *             bytes 全比仅在 debug build 断言（二次验证 hash 无碰撞）。
 *   - Property<T>：比较 encoding + value（本期 encoding 恒 Constant，value 按 T 比较）；
 *   - vector<unique_ptr<T>>：size 相等 + 每元素递归比较；
 *   - 字符串：字节精确等（不做 NFC/NFD 归一化——这是 Baker 语义而非 helper 语义）。
 */
bool PAGDocumentEquals(const pagx::pag::PAGDocument* a, const pagx::pag::PAGDocument* b,
                       std::string* diffPath = nullptr);

}  // namespace pagx::test
```

**测试示例**（`BakeContextTest.SimpleFillRoundTrip`）：
```cpp
auto docA = MakeSimpleFillDoc();  // PAGXBuilder 构造
auto bakedA = Baker::Bake(*docA, &ctx);
auto encoded = Codec::Encode(*bakedA.doc, &encCtx);
auto decoded = Codec::Decode(encoded.bytes.data(), encoded.bytes.size());
std::string diff;
ASSERT_TRUE(PAGDocumentEquals(bakedA.doc.get(), decoded.doc.get(), &diff)) << "diverged at: " << diff;
```

**Phase 2 交付登记**：`test/src/pag/support/PAGDocumentEquals.h+cpp` 加入 §19 Phase 2 产品代码列；支持所有 Baker/Codec 单测的对比断言，避免字段级耦合。

### 18.3bis CorruptBuilder（`support/CorruptBuilder.h`，P0-I3，Phase 1 前置交付）

构造恶意/截断 `.pag` 字节流的测试工具。Phase 4 `TruncatedDecodeTest`、Phase 9 `InflaterParity.CompositionSelfRef`、Phase 12 fuzz corpus 都依赖本工具，**必须在 Phase 1 ValueCodec 阶段同步落地**（不是 Phase 4 再赶工）。

```cpp
namespace pagx::test {

// 从合法 PAG v2 字节流出发，链式构造畸形输入。所有方法返回 *this 以支持 fluent 调用。
class CorruptBuilder {
 public:
  static CorruptBuilder FromValid(std::vector<uint8_t> validBytes);

  // 截断：保留前 n 字节，其余丢弃。用于 TruncatedData=303。
  CorruptBuilder& TruncateAt(size_t n);

  // 在指定 offset 处翻转一个字节（XOR mask）。用于 MalformedTag=304 / 位翻转攻击模拟。
  CorruptBuilder& FlipByteAt(size_t offset, uint8_t xorMask);

  // 替换连续字节。用于 magic 替换（InvalidMagic=300）、version 替换（UnsupportedVersion=301）。
  CorruptBuilder& ReplaceBytes(size_t offset, const std::vector<uint8_t>& patch);

  // 重写 FileHeader 里的 version 字节（第 4 字节，偏移 3）
  CorruptBuilder& SetVersion(uint8_t v)                     { return ReplaceBytes(3, {v}); }

  // 重写 magic（前 3 字节）
  CorruptBuilder& SetMagic(const std::array<uint8_t, 3>& m) { return ReplaceBytes(0, {m[0], m[1], m[2]}); }

  // 在第一个匹配到的 varU32 前写一个超大值（默认 0x7FFFFFFF），用于触发 count 上限类错误
  // （PathVerbLimitExceeded=404 / GlyphCountLimitExceeded=405 / StructureLimitExceeded=105）。
  CorruptBuilder& InflateVarU32At(size_t offset, uint32_t giantValue = 0x7FFFFFFFu);

  // 令 TagHeader.length = 0xFFFFFFFE 模拟 u32 回绕攻击（P0-B regression test）
  CorruptBuilder& WrapTagLengthAt(size_t tagHeaderOffset);

  // 令 ShapeStyleData.innerLength = 0xFFFFFFFE 模拟 u32 回绕（P0-S1 regression test）
  CorruptBuilder& WrapInnerLengthAt(size_t shapeStyleStart);

  // 将动态位宽数组前的 numBits 字段改写为 > 32 的非法值（P1-S3 regression test）
  CorruptBuilder& SetNumBitsAt(size_t offset, uint8_t numBits);

  // 把 varU32 字段改写为"第 5 字节带 continuation bit"的畸形 ULEB128（P1-S4 regression test）
  CorruptBuilder& SetContinuationBitsAt(size_t offset);

  // 改写 LayerMaskRef 或其他上下文中的 childIndex varU32 字段为超 MAX_CHILDREN_PER_LAYER 的值
  // （P1-S5 regression test）
  CorruptBuilder& SetChildIndexAt(size_t offset, uint32_t childIndex);

  // 改写 ImagePattern 的 patternImageIndex u32 字段为 ≥ doc.images.size() 的越界值
  // （P1-S5 regression test）
  CorruptBuilder& SetPatternImageIndexAt(size_t offset, uint32_t patternImageIndex);

  // 构造 CompositionRef 自环（Phase 9 InflaterParity.CompositionSelfRef 用）
  // 前置条件：validBytes 必须含 ≥ 1 composition；replaces the first CompositionRefPayload's
  // compositionIndex with 0 indicating self-ref (assumes it lives in composition 0)
  CorruptBuilder& IntroduceCompositionSelfRef();

  // 改写 FileHeader.width/height 为 NaN / Inf / 0 / > MAX_CANVAS_DIM（P0-5 regression test）
  CorruptBuilder& SetCanvasSize(float width, float height);

  // 在合法字节末尾 End(0) Tag 之前插入 TagCode::ErrorMarker=1022 零 body Tag（P0-11 v2.18）
  // 用于触发 Decoder ProducerFatal=106 路径——上游 Baker fatal 导致 Codec 半写的字节流预警。
  // 实现：seek 到首个 TagCode::End=0 出现位置，before-insert 序列 [ErrorMarker TagHeader 2 bytes; length=0]，
  // 然后 End 紧随其后。
  CorruptBuilder& AppendErrorMarkerTag();

  // 改写 FileHeader 的 compression 字节为非 0 值（触发 UnsupportedCompression=302，P1-T6 v2.18）
  CorruptBuilder& SetCompressionByte(uint8_t v);

  // 改写 FileHeader 的 bodyLength u32 为指定值（通常用于构造 305 BodyLengthOutOfRange 或
  // 让 Decoder 以为 body 比实际字节长导致提前 EOF；P1-T6 v2.18）
  CorruptBuilder& SetBodyLength(uint32_t v);

  // 改写 CompositionRefPayload 指定 offset 处的 compositionIndex 为任意越界值（如 UINT32_MAX；
  // 与 IntroduceCompositionSelfRef 不同，此方法允许构造 >= doc.compositions.size() 的越界值，
  // P1-T6 v2.18）
  CorruptBuilder& SetCompositionIndexAt(size_t offsetOfPayload, uint32_t value);

  // 在合法字节末尾 End Tag 之前追加 bodyLength 声明之外的 N 字节垃圾（构造 PrematureEndTag=409
  // 测试场景——bodyLength 声明 X 字节但实际多出 N 字节；P0-15 v2.18）
  // **v2.19 P1-06 纪律**：触发 409 必须配合 `SetBodyLength(newTotal)` 一起调，使 Decoder 在 End 处检测到
  // `position - bodyStart < bodyLength`；单独调用 AppendGarbageAfterEnd 不改 bodyLength = End 之后挂垃圾场景
  // （bodyLength 声明 OK，End 之后是 trailer 字节），不触发 409。§G.6 Truncate.PrematureEndWithUnreadBody
  // 示例按联合构造模板给出。
  CorruptBuilder& AppendGarbageAfterEnd(size_t n);

  // 改写 FileHeader 的 version 字节（本期 0x02）为指定值（P2-10 v2.19，为 FutureVersionRejected
  // 测试铺路——v3-reject 需 Decoder 对 version > KnownVersion 推 UnsupportedVersion=301 fatal）。
  CorruptBuilder& SetVersionByte(uint8_t v);

  // 改写指定 LayerMaskRef sub-Tag 的 maskLayerPath 为任意路径（P0-R/P1-21 v2.19，为 InflaterParity
  // MaskCycleTwoLayers 字节级构造铺路——直接改两个 LayerMaskRef 的 childIndex 序列互指）。
  // offsetOfLayerMaskRef 来自 `PagxBytesLayout::maskPathOffsets[]`（v2.19 P1-21 扩为 vector）。
  CorruptBuilder& SetMaskLayerPathAt(size_t offsetOfLayerMaskRef,
                                     const std::vector<uint32_t>& path);

  std::vector<uint8_t> Build() const { return bytes; }

 private:
  std::vector<uint8_t> bytes;
};

}  // namespace pagx::test

// —— LayerDepthLimitExceeded=406 的 regression 用例 ——
// 该测试需要构造"65 层嵌套 LayerBlock"的**结构性**输入，属于合法字节但结构恶意；
// CorruptBuilder 做不到（它只翻字节，不改结构拓扑）。改由 PAGXBuilder + 额外 helper 构造：
//
//   auto deepDoc = MakeDeepLayerStack(PAGXBuilder::Doc(100, 100), /*depth=*/65);
//   // 然后走正常 Export → Load 路径，Decoder 侧触发 LayerDepthLimitExceeded=406
//
// `MakeDeepLayerStack` 规范（P0-4 v2.16，Phase 4 交付，放 `test/src/pag/support/StructBuilders.h`）：
namespace pagx::test {

// 返回已 applyLayout 的 unique_ptr<PAGXDocument>，root composition 内构造单链 `depth` 层嵌套 Layer。
// 通过 rvalue 引用取走 builder 所有权，内部 open root composition 后 depth 次 `enterLayer(LayerType::Layer)`
// 加一个 addRectangle 填充，最终 exitLayer×depth + closeComposition + Build。
inline std::unique_ptr<pagx::PAGXDocument> MakeDeepLayerStack(PAGXBuilder&& builder, int depth) {
  builder.openComposition("root");
  for (int i = 0; i < depth; ++i) {
    builder.enterLayer(pagx::LayerType::Layer);
    builder.withName(std::string("L") + std::to_string(i));
  }
  // 叶子节点挂一个最小可渲染内容，避免空树被 Inflater 当空文档 (604) 处理掩盖 406 信号：
  builder.addRectangle(tgfx::Rect::MakeWH(10, 10));
  builder.addFill(tgfx::Color::FromRGBA(255, 255, 255, 255));
  for (int i = 0; i < depth; ++i) builder.exitLayer();
  builder.closeComposition();
  return builder.Build();
}

// 宽度 helper（P0-10 v2.18）：root composition 内构造 n 个兄弟 Layer（flat 扁平树），
// 用于触发 BAKE_MAX_TOTAL_LAYERS 熔断（`BakerEdgeCases.BakeTotalLayerCount` 测试需 > 1e5）。
inline std::unique_ptr<pagx::PAGXDocument> MakeWideSiblingTree(PAGXBuilder&& builder, int n) {
  builder.openComposition("root");
  for (int i = 0; i < n; ++i) {
    builder.enterLayer(pagx::LayerType::VectorLayer);
    builder.withName(std::string("W") + std::to_string(i));
    builder.enterVectorGroup();
    builder.addRectangle(tgfx::Rect::MakeWH(1, 1));
    builder.addFill(tgfx::Color::FromRGBA(255, 0, 0, 255));
    builder.exitVectorGroup();
    builder.exitLayer();
  }
  builder.closeComposition();
  return builder.Build();
}

// Mask chain helper（P0-10 v2.18 + P2-07 v2.19 字节直喂改版）：构造"单个 host Layer 挂 k 个 mask
// 路径引用" 的字节流（测 MAX_PENDING_MASKS 熔断）。
// **v2.19 改动**：原版走 PAGXBuilder + `withMask` k 次——当 k > BAKE_MAX_TOTAL_LAYERS 时先撞 Baker 上限
// 而非 Inflater 的 MAX_PENDING_MASKS=262144；改为**字节直喂**路径：直接拼装合法字节流，host LayerBlock
// body 内追加 k 个独立 LayerMaskRef sub-Tag（**注意**：§D.8 LayerBlock 规约每 LayerMaskRef 至多 1 次，
// 此处为**字节级构造**特例用于测试 Inflater 熔断；Decoder 走 P1-05 重复拒绝走 304 fatal 之前 Pass 1
// 已把 k 个 pending 条目 push 入 InflaterContext.pendingMasks，足以验证熔断）。
// 返回 `(bytes, hostLayerIndex)` 供 InflaterParity 直接投喂 Decoder → Inflater。
struct MakeLayerWithKMasksResult {
  std::vector<uint8_t> bytes;
  uint32_t hostLayerIndex;
};
inline MakeLayerWithKMasksResult MakeLayerWithKMasksBytes(int k) {
  // 实现位于 StructBuilders.cpp：CapturePagxLayout → locate LayerBlock body → 插入 k 个
  // `LayerMaskRef` sub-Tag（TagCode=12 + length=pathDepth×varU32）指向 "root/0"。
  // 原 PAGXBuilder 版本废弃，测试直接用本字节版（P2-07 v2.19）。
  return {};
}

// Composition ref cycle helper（P0-10 v2.18）：构造 `ids` 列表按序环形引用（如 ["A","B","A"] 构成
// A→B→A 环），用于 Phase 9 `InflaterParity.CompositionSelfRef` / `CompositionRefTooDeep` 结构级测试。
// 每个 id 各为一个 composition，最后一个 composition 的 CompositionReferenceLayer 指回首个 id。
inline std::unique_ptr<pagx::PAGXDocument> MakeCompositionRefCycle(PAGXBuilder&& builder,
                                                                    const std::vector<std::string>& ids) {
  for (size_t i = 0; i < ids.size(); ++i) {
    builder.openComposition(ids[i]);
    builder.enterLayer(pagx::LayerType::CompositionReferenceLayer);
    builder.withName(std::string("ref_to_") + ids[(i + 1) % ids.size()]);
    builder.asCompositionRef(ids[(i + 1) % ids.size()]);
    builder.exitLayer();
    builder.closeComposition();
  }
  return builder.Build();
}

// —— CorruptBuilder offset 定位助手（P0-5 v2.16）——
// `SetNumBitsAt` / `SetContinuationBitsAt` / `WrapInnerLengthAt` / `SetChildIndexAt` /
// `SetPatternImageIndexAt` 五个方法的 offset 参数需要"知道目标字段在哪"——手算字节布局易错。
// 解决方案：Codec 侧在 debug build 下（`PAGX_DEBUG_OFFSETS=1`）写字节流时顺手记录每个关键
// 字段的起始 offset 到 `doc.header` 之后的空间（不进最终字节），测试通过下列 helper 读出：

struct PagxBytesLayout {
  size_t firstTagHeaderOffset   = 0;   // 进入 body 的第一个 Tag
  size_t firstShapeStyleStart   = 0;   // 第一个 ShapeStyleData inline 的起点（用于 WrapInnerLengthAt）
  size_t firstVarU32Offset      = 0;   // 第一个 user-controlled varU32（用于 InflateVarU32At 默认）
  size_t firstNumBitsOffset     = 0;   // 第一个动态位宽数组的 numBits 字段（SetNumBitsAt）
  size_t firstEncodedUint32Offset = 0; // 第一个 readEncodedUint32 读取点（SetContinuationBitsAt）
  size_t firstChildIndexOffset  = 0;   // LayerMaskRef 第一个 childIndex（SetChildIndexAt）
  size_t firstPatternImageIndexOffset = 0; // ImagePattern patternImageIndex u32（SetPatternImageIndexAt）
  size_t fileHeaderWidthOffset  = 3 + 1 + 4 + 1;  // magic(3) + version(1) + bodyLen(4) + compression(1) = 9; width 紧随 FileHeader TagHeader
};

// 由 PAGExporter 在 debug build 下返回；release build 返回空 layout（要求测试 only 在 debug 跑）
PagxBytesLayout CapturePagxLayout(const std::vector<uint8_t>& bytes);

// 典型用法：
//   auto v = ExportValid();
//   auto layout = CapturePagxLayout(v);
//   auto bad = CorruptBuilder::FromValid(std::move(v))
//                .WrapInnerLengthAt(layout.firstShapeStyleStart)
//                .Build();

}  // namespace pagx::test
```

**CapturePagxLayout 实现方案**（P0-5 v2.17，闭环 "offset 从哪来"）：

**方案选择**：优先 **(a) PAGExporter 测试钩子**——确定性、O(1)、不依赖字节回扫；回退 **(b) 字节流反向扫描**仅在 PAGExporter 无法编译 debug 钩子时使用。

**(a) PAGExporter 测试钩子（推荐）**：

```cpp
// 1. 编译期开关，仅在 Debug + PAG_BUILD_TESTS 同时开时激活（见 P2 PAGX_DEBUG_OFFSETS 两条件守卫）
#if defined(PAGX_DEBUG_OFFSETS) && defined(PAG_BUILD_TESTS)

// 2. src/pagx/pag/Codec.h 内 Baker 写 Tag 时由 TagWriterScope 顺手记录：
class TagWriterScope {
 public:
  TagWriterScope(EncodeStream* s, TagCode code, EncodeSession* session)
      : stream_(s), session_(session), startOffset_(s->position()) {
    auto* debugLayout = session_ ? session_->debugLayout : nullptr;   // EncodeSession 额外携带的调试层指针；release build 恒 nullptr
    if (code == TagCode::ShapeStyleData && debugLayout) {
      if (debugLayout->firstShapeStyleStart == 0) {
        debugLayout->firstShapeStyleStart = startOffset_;
      }
    }
    // ... 其他关键 Tag 同样模式：firstTagHeaderOffset / firstNumBitsOffset / ...
  }
  // dtor 写 length；内部方法 captureVarU32 / captureNumBits 在对应写入点被调用
};

// 3. CapturePagxLayout 实现（src/pagx/pag/DebugLayout.cpp）：
PagxBytesLayout CapturePagxLayout(const std::vector<uint8_t>& bytes) {
  // 重新跑一遍 Decode，但只为了提取 offset——独立于被测逻辑，确保无状态耦合。
  // DecodeContext 挂一个 layout 收集器：每进入 ShapeStyleData 前记录 position()。
  DecodeContext ctx;
  ctx.layoutCapture = std::make_unique<PagxBytesLayout>();
  pag::DecodeStream stream(bytes.data(), bytes.size());
  Codec::Decode(&stream, &ctx);   // 副产物：layoutCapture 填满
  return *ctx.layoutCapture;
}

#else
// Release 或未启用 PAG_BUILD_TESTS：CapturePagxLayout 返回全零 layout
// 测试用 `ASSERT_NE(layout.firstShapeStyleStart, 0)` 在 Release build 自动失败
PagxBytesLayout CapturePagxLayout(const std::vector<uint8_t>&) { return {}; }
#endif
```

**(b) 回退：字节流反向扫描**（若 Codec 侧无法改）：由 `CapturePagxLayout` 调 `pagx::pag::Codec::Decode` 到一个"仅记录 offset 不构造 PAGDocument"的轻量 DecodeContext 变体——本质仍是单次 O(n) 扫描，但无须触及 Codec 内部。后续若出现"两路维护"负担再切换 (a)。

**测试纪律**：所有使用 `CapturePagxLayout` 的 CorruptBuilder 测试必须在 `TEST()` 首行加 `#if !defined(PAGX_DEBUG_OFFSETS) GTEST_SKIP() << "requires PAGX_DEBUG_OFFSETS debug build"; #endif`（或 `ASSERT_NE(layout.firstShapeStyleStart, 0)` 硬失败），避免 Release build 静默跑过。

---

**使用示例**：

```cpp
TEST(Truncate, CorruptedLengthField) {
  auto good = MakeMinimalValidPAGBytes();
  auto bad  = CorruptBuilder::FromValid(std::move(good)).WrapTagLengthAt(9).Build();
  auto r = pagx::pag::Codec::Decode(bad.data(), bad.size());
  ASSERT_EQ(r.errors.size(), 1u);
  EXPECT_EQ(r.errors[0].code, DiagnosticCode::MalformedTag);
}
```

**测试点对应**：每个 v2.10-v2.13 已修的 P0 安全项必须通过 CorruptBuilder 构造 regression 用例（Path coord overflow / Tag length wrap / innerLength wrap / utf8 length 2GB / composition self-ref）。

### 18.3ter Layer 6 Fuzz（`PAGDecodeFuzzTest`，P1-I4，Phase 12 交付）

libFuzzer harness 直接喂 `Codec::Decode` + `LayerInflater::Inflate`，覆盖枚举用例无法穷尽的 state space。单个 harness 文件：

```cpp
// test/src/pag/fuzz/decode_fuzz.cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto r = pagx::pag::Codec::Decode(data, size);
  if (r.doc) {
    (void)pagx::pag::LayerInflater::Inflate(std::move(r.doc));
  }
  return 0;   // libFuzzer 不依赖返回值；ASAN/UBSAN 自动捕获违规
}
```

**门槛**：
- 使用 CorruptBuilder 构造的 seed corpus + spec/samples 合法样本一并喂 fuzzer；
- Phase 12 合入 main 前**必须** `≥ 1 CPU·小时` ASAN + UBSAN 全绿，没有新 crash / OOB / timeout / leak；
- CI 每周跑 1 次增量 fuzz（24 CPU·小时），发现 crash 自动开 issue。

### 18.3ter Inflater Fuzz harness（`PAGInflaterFuzzTest`，P1-19 v2.19，Phase 12 交付）

**与 PAGDecodeFuzzTest 的职责分工**：Decode fuzz 喂字节流后 `if (r.doc) Inflate(...)`——Inflater 执行路径被 Decode 成功与否决定，Inflater 专有逻辑（Pass 2.5 Tarjan SCC / pendingMasks cap / layerBudget cap / 生命周期 reset）的 state-space **被 Decode 侧解析失败掩盖**。Inflater 独立 harness 需要用**合法但结构恶意**的字节流作 corpus，专打 Inflater 内部。

```cpp
// test/src/pag/fuzz/inflater_fuzz.cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // harness 链路：字节流 → Decode → 仅在 doc 构造成功时调 Inflate；不直接 mutate PAGDocument
  // 对象树（unique_ptr<Composition> / shared_ptr<Data> 树不能 raw-byte mutate，破坏 vtable/refcount）。
  auto r = pagx::pag::Codec::Decode(data, size);
  if (!r.doc) return 0;
  auto tree = pagx::pag::LayerInflater::Inflate(std::move(r.doc));
  (void)tree;
  return 0;
}
```

**corpus seed 来源**（合法但结构恶意）：
- `StructBuilders::MakeMaskCycleBytes({A, B})`——A.mask = B + B.mask = A（触发 607 Pass 2.5 SCC）；
- `StructBuilders::MakeCompositionRefCycle({"A", "B"})`（触发 605 InflateCompositionCycle）；
- `StructBuilders::MakeWideSiblingTree(N)`（测 606 InflaterLayerBudgetExceeded）；
- `StructBuilders::MakeLayerWithKMasksBytes(262145)`（测 603 InflateMaskResolveFailed 熔断）；
- 常规 CorruptBuilder corpus 的一部分（主要是已通过 Decode 合法性但结构可疑的样本）。

**断言**：专查 `InflateMaskCycle=607 / InflaterLayerBudgetExceeded=606 / InflateMaskResolveFailed=603` 三类 fatal 路径**不 crash**（ASAN/UBSAN 全绿、无死循环、推 warning 正常）。

**Phase 12 登记**：`PAGInflaterFuzzTest` 纳入 `.github/workflows/pagx-fuzz.yml` 4-shard matrix，其中 2 shard 跑 Decode fuzz，另 2 shard 跑 Inflater fuzz；合入 main 前 ≥ 1 CPU·小时 ASAN 全绿。

**CI 工程化规划**（P2-8 v2.16）：GitHub Actions 单 job 上限 6 小时硬超时 → 24 CPU·小时必须**分片并行**。具体：
- **分片**：4 个 parallel job × 6 小时 × 1 CPU = 24 CPU·小时；matrix strategy `shard=[0,1,2,3]`，libFuzzer 用 `-fork=1 -ignore_crashes=0 -max_total_time=21000`（6h - 缓冲）；
- **Corpus 持久化**：采用 GitHub Actions cache（`actions/cache@v4`，key 含 pagx 分支/sha），每次 run 读上周 corpus、写本周扩增。退化方案：若 corpus 超 cache 5GB 上限，改 AWS S3 / OSS bucket（CI secret 鉴权）；
- **Build artifact**：每次 run 把 `./cmake-build-fuzz/PAGDecodeFuzz` binary + `corpus/` tarball 上传为 GitHub Release attachment（rolling 4 周）；
- **Crash triage**：`-fork=1 -minimize_crash=1` 最小化后的 `crash-*` 文件自动提交到 repo `test/fuzz_corpus/crashes/` + 开对应 issue；开发者 fix 后 cherry-pick 到 regression corpus 防回归；
- **Coverage 门槛**：`libFuzzer -print_final_stats=1` 的 `feat=` 指标每周对比，若连续 2 周不增长提示 corpus 耗尽，需补 seed。

### 18.3quater 本地 tgfx 源码调试（P1-I6）

当 Layer 4 渲染一致性测试失败且怀疑 bug 出在 tgfx（Path 量化、GlyphRun 重建、Mask 合成等渲染路径）时，用独立 build 目录 + 本地 tgfx 源码切换排查（`.codebuddy/rules/Test.md` 已有指令）：

```bash
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
      -DTGFX_DIR=../tgfx -B cmake-build-debuglocal
cmake --build cmake-build-debuglocal --target PAGFullTest
```

路径 `../tgfx` 是 libpag 同级的本地 tgfx 源码仓库。CMake 以 `add_subdirectory` 方式加入，支持改 tgfx 代码后重新编译与断点调试，不影响常规 `cmake-build-debug`。

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

### 18.4ter ValueCodec Safe wrapper 专项测试（P1-10 v2.15，`ValueCodecSafeTest.cpp`，Phase 1 交付）

P0-S2/P1-S3/P1-S4/P0-S1 的安全修复依赖 §D.1 的 4 个 wrapper（`ReadUtf8String` / `ReadLengthPrefixedBytes` / `ReadInt32ListSafe` / `ReadEncodedUint32Safe`）——每 wrapper 2 条，共 8 条：

| 测试名 | 覆盖 wrapper | 场景 |
|---|---|---|
| `ValueCodecSafe.Utf8.Valid` | ReadUtf8String | 合法 UTF-8 输入 → 正确返回 |
| `ValueCodecSafe.Utf8.Oversized` | ReadUtf8String | length=0x7FFFFFFF → warn StringInvalidUtf8 + 空串，不 resize |
| `ValueCodecSafe.Bytes.Valid` | ReadLengthPrefixedBytes | 合法图片字节 → 正确返回 |
| `ValueCodecSafe.Bytes.Oversized` | ReadLengthPrefixedBytes | length > MAX_IMAGE_BYTES → warn ImageResourceSizeExceeded（或 FontResourceSizeExceeded，按调用点传入的 code 参数） + 空 + 对齐 skip |
| `ValueCodecSafe.Int32List.Valid` | ReadInt32ListSafe | numBits=16 + count=10 → 正确返回 |
| `ValueCodecSafe.Int32List.NumBitsOverflow` | ReadInt32ListSafe | numBits=100 → warn + 空数组 |
| `ValueCodecSafe.EncUint32.Valid` | ReadEncodedUint32Safe | 合法 ULEB128 → 正确返回 |
| `ValueCodecSafe.EncUint32.ContinuationAt5th` | ReadEncodedUint32Safe | 第 5 字节带 continuation bit → warn MalformedTag + 返回 0，位置推至首个非 continuation 字节 |

### 18.4quater PackLayerPath 专项测试（P1-11 v2.15，`BakeContextTest.cpp` 内 3 条，Phase 3 交付）

- `PackLayerPath.BasicRoundTrip`：`{1, 2, 3}` 打包后解包等价；
- `PackLayerPath.DepthOverflow`：`> 6` 层路径退化为"低 6 级 + depth 唯一化"；
- `PackLayerPath.ChildIndexCap`：`{1023}` 打包成功，`{1024}` 触发断言 / 静默截断为 1023（择一实现）。

### 18.4quinta ZeroCopyScope 专项测试（P1-12 v2.18，`ValueCodecSafeTest.cpp` 追加 3 条）

- `ZeroCopyScope.DefaultIsOwning`：**默认路径**（无 scope）下 `ReadLengthPrefixedBytes` 返回的 `shared_ptr<const tgfx::Data>` 必须走 `MakeAdopted`——内部 probe 或 `data->data() != stream.currentReadablePtr()` 断言。
- `ZeroCopyScope.InScopeMakesWithoutCopy`：在 `ZeroCopyScope::DecodeLocal local(ctx)` 生命周期内 `ReadLengthPrefixedBytes` 返回 zero-copy Data，`data->data() == stream.currentReadablePtr() + <dataLen prefix>` 断言。
- `ZeroCopyScope.LeakingDataToTreeIsBlocked`（P2-06 v2.19 **定稿唯一写法**）：构造"`MakeWithoutCopy` 产物逃逸到 `PAGDocument::images[0]->data`"的反例，在 PAGLoader 释放 backing buffer 后调 Inflater 解码路径。实现：`EXPECT_DEATH_IF_SUPPORTED(trigger_flow(), ".*heap-use-after-free.*")` + 构建期 `ASSERT_TRUE(pagx::test::CompiledWithAsan())` 前置；非 ASAN build 测试体内 `GTEST_SKIP() << "Requires ASAN";`。CMake 识别宏 `PAGX_BUILD_WITH_ASAN`（由 `cmake -DPAG_BUILD_WITH_ASAN=ON` 时 `add_definitions(-DPAGX_BUILD_WITH_ASAN=1)` 定义）。**禁止**使用 `testing::GTEST_FLAG(death_test_style)` / `EXPECT_NONFATAL_FAILURE` 替代路径——多写法并存导致 CI 稳定性低（三工程师三实现）。

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
  auto inflated = pagx::pag::LayerInflater::Inflate(std::move(decoded.doc));
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
  auto inflated = pagx::pag::LayerInflater::Inflate(std::move(decoded.doc));
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
  auto inflated = pagx::pag::LayerInflater::Inflate(std::move(decoded.doc));
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
        "pag_v1_load_ms": 2.0,
        "bake_ms": 8.2,
        "encode_ms": 2.1,
        "decode_ms": 1.2,
        "platform": "Darwin arm64 M1 Max 10c/32g"
      }
    }
  }
}
```

**CI 门槛**（P1-P5）：`pag_load_ms ≤ pag_v1_load_ms × 1.2`——立项承诺"显著优于或至少不劣于 PAG v1"的量化兑现。`pag_v1_load_ms` 每次生成 baseline 时同批采样（同样本先转 v1 再跑 `PAGFile::Load`），避免"v1 基线漂移与 v2 退化"互相污染。

**pagx → PAG v1 采样路径**（P1-13 v2.15）：libpag 主干的 PAGX → PAG v1 导出走 `src/cli/CommandExport.cpp`（pagx-cli 的 `export` 子命令），输出 `.pag` 文件格式为 v1。PerformanceTest 调用方式：

```cpp
// 方法 A：同进程调用 CommandExport（首选，避免 fork 噪声）
auto v1Bytes = pagx::cli::ExportToPAGv1(*pagxDoc);   // 若现函数私有，开工首周将其提到 public/internal
auto t0 = now();
pag::PAGFile::LoadFromBytes(v1Bytes.data(), v1Bytes.size());
double pag_v1_load_ms = (now() - t0).ms();
```

若 `ExportToPAGv1` 开工时不可用：退化门槛为 `pag_load_ms ≤ (previous baseline pag_load_ms) × 1.05`（允许 5% 漂移，但丢失"显著优于 v1"的立项约束），同步在 §17 D 档标记"v1 对比待补"。

**baseline.json 首次 commit 归属**（P2-8 v2.15）：首次 `test/perf/baseline.json` 由 tech lead 在项目 README 指定的**参考机**上跑（目前建议 `Darwin arm64 M1 Max 10c/32g`；在 `reference_abs_times.platform` 字段记录）。日常 CI 只追踪 `size_ratio` / `load_ratio` 的相对漂移（±20%），**不**追绝对 abs_times——后者随 CI 机型浮动，无参考价值。若未来需换参考机，先跑一次生成新 abs_times 入 git，comment 里写明换机原因。

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

### 18.12bis CI 时长分拆（P2-10 v2.18）

PR 验证与 nightly 的测试分拆策略——单次 CI 预算约束 15 分钟内：

| 场景 | 测试范围 | 预估时长 | 触发方式 |
|---|---|---|---|
| **PR gate**（必须全绿才合入） | Layer 1 单元（~110 条）+ Layer 2 集成 + Layer 3 E2E + Layer 4 RenderEquivalence（6 samples × 2 modes）+ Fuzz 1h sanity | ~10-12 min | 每个 PR 自动 |
| **Nightly** | Layer 5 Perf + Layer 6 Fuzz（4-shard × 6h matrix = 24 CPU·小时）+ 全 spec/samples 扩到 77 个 × 2 modes | ~6h × 4 shard | `cron: "0 2 * * *"` 日跑 |
| **Manual / Release candidate** | 全 nightly + ASAN 48 CPU·小时 fuzz 加深 | 2-3 天 | 发版前手动触发 |

**PR gate 保护**：`Truncate.*` / `InflaterParity.*` / `PAGRenderEquivalenceTest.*` 任一挂即阻止合入；Fuzz 1h sanity 挂仅警告不阻止（真 bug 会在 nightly 6h 里放大显现）。

**CR 维护增量清单**（P2-10 v2.18，合并 §19 Phase 0 已有 5 步）——任何 PR 涉及 Diagnostic 改动或新 Phase 交付物，必须依次打勾：

1. **Diagnostic 段内追加**（§15.1 枚举声明）
2. **kAllDiagnosticCodes[] 同步**（DiagBuild.h 数组 + size `N+1`）
3. **CodeToString switch 追加 case**
4. **§G.6 测试矩阵增加行**
5. **§15.1 ABI 表补 contextIndex 语义行**（若有结构语义）
6. **§22 维护日志加修订条目**（版本号 / P 级别 / 修订说明）
7. **§3.3 术语表补行**（新术语）
8. **§19 Phase 表同步依赖矩阵**（若影响 Phase 工件顺序）

### 18.13 覆盖率脚本（`tools/coverage.sh`）

用 clang `-fprofile-instr-generate -fcoverage-mapping`；独立 build 目录 `cmake-build-coverage`；产出 HTML 到 `coverage/html/`（gitignore）；PR 模板要求附覆盖率摘要。

---

## 19. 工作拆分与 TDD 执行顺序

每阶段产品代码与对应测试**同批次**产出（TDD 严格纪律）。

| # | 产品代码 | 同批测试 | 交付判定 |
|---|---|---|---|
| 0 | `include/pagx/Diagnostic.h` / `src/pagx/Diagnostic.cpp` / `src/pagx/pag/ErrorCode.h` (alias) / `src/pagx/pag/DiagBuild.h`（含 `kAllDiagnosticCodes[]` 全枚举表，P0-6 v2.16） | `DiagnosticTest`（FormatDiagnostic 格式 + MakeDecodeDiag / MakeDiag 往返 + ABI-appended 码不丢 + `CodeToString.AllEnumValues` 遍历 `kAllDiagnosticCodes` 断言） | 对外头齐备，作为后续阶段的 include 基础；**CR 维护清单**（P2-5 v2.17，任何 PR 新增 `DiagnosticCode` 必须依次打勾）：(a) 在枚举段内追加（`enum class DiagnosticCode` 声明）；(b) 在 `kAllDiagnosticCodes[]` 数组追加并把 `std::array<DiagnosticCode, N>` 中 N +1；(c) 在 `CodeToString` switch 追加 case；(d) 在 §G.6 测试矩阵增加对应行；(e) 若 contextIndex 有结构语义，在 §15.1 ABI 表补一行。五步缺一 Phase 0 `DiagnosticTest.CodeToString.AllEnumValues` 即挂，禁止合并 |
| 1 | `ValueCodec.h` / `PropertyEncoding.h` / `TagCode.h` / TagHeader util + `test/src/pag/support/CorruptBuilder.h+cpp`（§18.3bis） | `ValueCodecTest` / `PropertyEncodingTest` / `TagHeaderTest` / `CorruptBuilderTest` | 四文件全绿；CorruptBuilder 本阶段交付以便后续 Phase 4 用；**Phase 1 exit gate**（P1-4 v2.17）：`grep -rn "std::vector<uint8_t>" src/pagx/pag/ValueCodec.h src/pagx/pag/Codec.cpp` 在 `ReadLengthPrefixedBytes` / `ReadUtf8String` / ImageAsset / FontAsset 上下文零命中（全部改用 `std::shared_ptr<const tgfx::Data>`）|
| 2 | `PAGDocument.h` / `BakeContext.h+cpp` / `ResourceBaker.cpp` / **`test/src/pag/support/PAGXBuilder.h+cpp`**（Phase 2 前置交付，原计划 Phase 3）/ **`test/src/pag/support/StructBuilders.h+cpp`**（P0-6 v2.17 登记：`MakeDeepLayerStack` + P0-10 v2.18 扩 `MakeWideSiblingTree / MakeLayerWithKMasks / MakeCompositionRefCycle`）/ **`test/src/pag/support/PAGDocumentEquals.h+cpp`**（P0-9 v2.18 Baker 单测对比 helper）+ **`FontAsset::data` 类型迁移**（`std::vector<uint8_t>` → `std::shared_ptr<const tgfx::Data>`，P1-6 v2.16 落地；Baker/Codec/Inflater 三处消费点同批更新）+ **`ImageAsset::kind` / `FontAsset::axes` 字段追加**（P1-2 v2.18） | `BakeContextTest` / `ResourceBakerTest` / `PAGXBuilderTest`（≥13 条 fluent API smoke，P1-8 v2.17） | 全绿；PAGXBuilder fluent builder 可用；PAGDocumentEquals 可用；`grep -rn "std::vector<uint8_t>" src/pagx/` 零命中于 ImageAsset/FontAsset/ReadLengthPrefixedBytes 上下文（P1-4 v2.17 exit gate） |
| 3 | `LayerBaker.cpp`（通用字段）| `LayerBakerTest` + `BakerEdgeCasesTest`（Baker fatal 段 100-105 + 207） | 全绿 |
| 4 | `Codec.cpp` 基础 Tag（FileHeader/Composition/LayerBlock）——**不产出 `EncodeContext.h`**（v2.19 P2-03 + §8.5 P0-D）：Encode 在 Codec.cpp 函数栈内构造 `EncodeSession session{&diag, &sc};` 临时对象，2 指针聚合体不需要独立头文件。`DiagnosticCollector` 由 `include/pagx/pag/DiagnosticCollector.h` 提供（Phase 0 交付）。 | `RoundTripTest` 前半 + `VersionRejectTest` + `TruncatedDecodeTest`（**仅** 300/301/302/303/304/305/306 段。FileReadFailed=307 只能由 LoadFromFile 触发，推至 Phase 10.5；Path/Glyph/Resource 相关 402/403/404/405/408 推至 Phase 5/8；CompositionCycle=605 推至 Phase 9；LayerBudgetExceeded=606 推至 Phase 9） | 全绿 |
| 5 | `VectorBaker.cpp` + `ElementTags.cpp` | `VectorBakerTest` + `RoundTripTest` 剩余 + `TruncatedDecodeTest.PathTooManyVerbs/GlyphCountOverflow`（新增：Path 相关 404/405） | 全绿 |
| 6 | PaintBaker 代码（融入 VectorBaker 文件） | `PaintBakerTest` | 全绿 |
| 7 | `StyleFilterBaker.cpp` + `FilterTags.cpp` + `StyleTags.cpp` | `StyleFilterBakerTest` | 全绿 |
| 8 | `TextBaker.cpp` + GlyphRun 序列化 | `TextBakerTest` + `TruncatedDecodeTest.InvalidUtf8/ImageOversize/FontOversize`（403 Utf8 + 402 image + 408 font 独立行，不再共用"Resource" 模糊说法，P2-1 v2.17） | 全绿 |
| 9 | `LayerInflater.cpp` | `InflaterParityTest`（含 `CompositionSelfRef / CompositionRefTooDeep`=605 / `LayerBudgetExceeded`=606） + `PAGDocumentParityTest` | 全绿。不依赖 RenderUtil——Inflater 层测试只断言 layer 树结构，不渲染像素 |
| 9.5 | `support/RenderUtil.cpp`（测试基建；拆出 Phase 9 是因为 Phase 9 InflaterParityTest 不需要 Surface 渲染） | — | RenderUtil 就绪，为 Phase 12 RenderEquivalenceTest + Day-1 smoke 内联 lambda 之外的场景铺垫 |
| 10 | `PAGExporter.cpp`（导出对外 API） | — | API review 过 |
| 10.5 | `include/pagx/PAGLoader.h` / `src/pagx/pag/PAGLoader.cpp`（加载对外 API） | `PAGLoaderTest`（LoadFromBytes 成功路径 / `PAGLoader.LoadFromFile_Missing` 触发 `FileReadFailed=307` / `Peek(filePath)` O(1) 路径 / `ImageBytesReleasedAfterInflate` data.use_count==1 断言 / Inflater warnings 被 Result.warnings 收集） | 对外加载 API review 过 + 全绿 |
| 11 | `CommandExport.cpp` 扩展 | `EndToEndTest` + `EdgeCasesTest` | 全绿 |
| 12 | **`.github/workflows/pagx-fuzz.yml`**（P1-6 v2.17 登记：§18.3ter CI 规划正式落地产物，4-shard matrix × 6h + corpus cache + crash auto-commit） | `RenderEquivalenceTest` + `PAGDecodeFuzzTest`（§18.3ter Layer 6 Fuzz，≥ 1 CPU·小时 ASAN/UBSAN 全绿） | Render/OutlineAll 模式 Baseline::Compare 全绿；Fuzz 全绿；CI yaml 首次上绿 |
| 13 | —（取消 v1 改动，v1 已能 graceful reject 0x02）| — | — |
| 14 | — | `PerformanceTest` + 首次生成 `test/perf/baseline.json`（含 `pag_v1_load_ms` 参照） | 基线入 git |
| 15 | `tools/coverage.sh` | — | 覆盖率 ≥85%，报告附 PR |

**阶段门槛**：
- 阶段 0 全绿 → 解锁后续所有阶段（Diagnostic 是所有模块的 include 基础）
- 阶段 1–3 全绿 → 可 code review
- 阶段 4–8 全绿 → 可合入 feature 分支
- 阶段 9–11 全绿 → 可开 PR（含 10.5 PAGLoader）
- 阶段 12–15 全绿 + D 类软指标达标 → 可合入 main

**阶段依赖矩阵**（P1-7 v2.18）——若某 Phase 挂，依赖的后续 Phase 无法独立完成：

| Phase | 硬依赖（必须先通过的前置 Phase） | 关键工件 |
|---|---|---|
| 0 | — | `include/pagx/Diagnostic.h`（所有后续 include 基础） |
| 1 | Phase 0 | `CorruptBuilder`（Phase 4/8/9/12 测试用）、ValueCodec Safe wrappers |
| 2 | Phase 0, 1 | `PAGXBuilder` / `PAGDocumentEquals` / `StructBuilders` / `BakeContext`（Phase 3-11 全依赖）|
| 3 | Phase 2 | LayerBaker（Phase 5/7/8 Baker 子模块的 shared 层）|
| 4 | Phase 0, 1, 2, 3 | `Codec.cpp` 基础 Tag（Phase 5-11 的 Encode/Decode 骨架）；需 `PAGX_DEBUG_OFFSETS` debug build |
| 5 | Phase 1 (CorruptBuilder), 3, 4 | VectorBaker |
| 6-8 | Phase 5 | Paint/Style/Text Baker 子模块 |
| 9 | Phase 2, 4, 8 | `LayerInflater.cpp`；**不**依赖 Phase 9.5（不走 Surface） |
| 9.5 | Phase 4（LoadFromBytes）、本地 tgfx 构建 | `RenderUtil.cpp` |
| 10 | Phase 4, 9 | `PAGExporter.cpp` |
| 10.5 | Phase 10, 9.5 | `PAGLoader.cpp` |
| 11 | Phase 10.5 | CLI 扩展 |
| 12 | Phase 10.5, 9.5 (RenderUtil), 1 (CorruptBuilder for fuzz seed) | `RenderEquivalenceTest` + `PAGDecodeFuzzTest` + `PAGInflaterFuzzTest`（P1-9 v2.18 独立 harness）+ `.github/workflows/pagx-fuzz.yml` |

**Phase 4 拆分**（P1-7 v2.18，可选实施）：若 Phase 4 一次吞"FileHeader + ImageAssetTable + Composition + LayerBlock"过大，可拆为：
- **Phase 4a**：容器头 + FileHeader + End + CompositionList（仅框架 + 空 composition 可走通）
- **Phase 4b**：ImageAssetTable + FontAssetTable + Composition + LayerBlock 主体

**Phase 12 首次提交基线缺失说明**（P1-10 v2.18）：`RenderEquivalenceTest` 首次跑时 `test/baseline/PAGRenderTest_Render/` 与 `PAGRenderTest_OutlineAll/` 目录**根本不存在**——预计 ~12 个基线 FAIL（6 spec samples × 2 modes）。交付判定应分两步：(a) tech lead 人工审图通过；(b) 用户执行 `/accept-baseline` 批量接受基线，再回跑转绿。AI 遇首跑 FAIL 不应误判为 bug，而应按此流程处理。

**测试命名统一**（P1-11 v2.18，强制）：全文档测试引用**必须**用 `Suite.CaseName` 二段式（gtest 规范），不用 `Suite.File.CaseName` 三段。文件名按 `<SuiteBase>Test.cpp` 命名（如 `Truncate` 套件 = `TruncatedDecodeTest.cpp`）。§19 产品列提及的 "TruncatedDecodeTest.xxx" 是文件路径通称，实际 suite 名见 §G.6 二段式写法。运行过滤用 `--gtest_filter="Truncate.*"` 按 suite 名过滤。已知冲突修订：§19 Phase 4/5/8 的"TruncatedDecodeTest.xxx" 应理解为 `Truncate.xxx`（文件 = `TruncatedDecodeTest.cpp`）。

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

enum class ImageAssetKind : uint8_t {
  Raster = 0,  // 本期唯一支持；数据走 tgfx::Image::MakeFromEncoded 路径
  Svg = 1,     // 预留；Decoder warn UnsupportedFeature=104 + 回退 Raster
  Video = 2,   // 预留；同上
  Hdr = 3,     // 预留；同上
};

struct ImageAsset {
  // 原始编码字节（PNG/JPEG/WebP），magic 头部必须匹配；shared_ptr 零拷贝（P0-P2 v2.14）。
  std::shared_ptr<const tgfx::Data> data;
  int32_t width = 0;
  int32_t height = 0;
  ImageAssetKind kind = ImageAssetKind::Raster;  // v2.19 P0-I；本期 Baker 恒写 Raster
};

struct FontAxis {
  uint32_t tag = 0;          // 4-char ASCII tag（e.g. 'wght' 'wdth' 'slnt'）
  float defaultValue = 0;
  float minValue = 0;
  float maxValue = 0;
};

struct FontAsset {
  FontSourceKind kind = FontSourceKind::System;
  std::string family = {};
  std::string style = {};
  std::shared_ptr<const tgfx::Data> data;   // 仅 Embedded 非空；shared_ptr 零拷贝（P1-6 v2.16，与 ImageAsset 对称）
  std::vector<FontAxis> axes = {};          // v2.19 P0-I；Variable Font 轴列表（本期 Baker 恒写空；Decoder 校验 size ≤ MAX_FONT_AXES=64）
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
| `WriteRatio`/`ReadRatio` | `void/WriteRatio(stream, pag::Ratio)` | `writeEncodedInt32` + `writeEncodedUint32`；**P1-13 v2.18**：Reader 强制 `denominator ≥ 1`——==0 → warn MalformedTag + 回退为默认 `{0, 1}`（等效禁用 stretch / frameRate）。防止下游 `numerator / denominator` 除零 SIGFPE 或 NaN 污染 matrix |
| `WriteFloat` 零值语义 | — | **P2-11 v2.18**：`-0.0f` 和 `+0.0f` 按 IEEE 754 语义**字节不同**（0x8000_0000 vs 0x0000_0000）。Baker 对比 Property<float> 是否 isDefault 时**必须**用 `std::bit_cast<uint32_t>(v) == std::bit_cast<uint32_t>(defaultV)` 精确字节比较，而非 `v == defaultV`（后者把 -0 和 +0 视相等导致 round-trip 字节不稳）。`PAGDocumentEquals` helper 同样按 bit_cast 比较 |
| `WriteProperty<T>`/`ReadProperty<T>` | 见 §4.3 | propHeader 位域 + isDefault 省略 + 分支写 value |
| `WriteEnum<T>`/`ReadEnum<T>` | 见 附录 G.5 | u8 + `EnumMeta<T>` 值域校验 |
| `WritePath`/`ReadPath` | `void/WritePath(stream, tgfx::Path, ctx)` | format byte + D.2.1 或 D.2.2 分发 |
| `WriteGlyphRunBlob`/`ReadGlyphRunBlob` | 同上 | 见 D.11 GlyphRunBlob inline 布局 |
| `ChoosePathFormat` | `uint8_t/(tgfx::Path)` | 回退判断（极短 path 走 format=0） |

**纪律**：
- 以上所有 v1 工具必须**直接引用**，不得在 `pagx::pag::` 内另写同名函数；
- v2 新增工具必须**放在 `src/pagx/pag/ValueCodec.h` 一个文件**，保持可审查；
- 任何 Tag 的 Read/Write 实现**禁止**直接操作字节 / 拆位，只能调用上述工具。

**P0 安全校验：攻击者可控 length 的 `resize` / `reserve` 必须前置上限校验**（强制约束）：`std::string::resize(N)` / `std::vector::reserve(N)` 如果直接吃用户控制的 varU32/varU64，恶意文件塞 `length = 0x7FFFFFFF` 立即触发 2 GB allocator 尝试——32-bit 进程直接 OOM 崩溃，64-bit 触发 overcommit 后 mmap 长延迟。`utf8string` / 图片 dataLen / 字体 dataLen / Glyph payload 等**所有**按 "varU32 length + N bytes" 模式读取的字段，必须通过下述 ValueCodec 统一入口；禁止直接写 `length = stream->readEncodedUint32(); s.resize(length);` 这类裸读法：

```cpp
// src/pagx/pag/ValueCodec.h —— 统一的 length-prefixed 读取入口

// 读 varU32 length 前缀 + length 字节 UTF-8 字符串。
// length 必须同时满足：
//   length <= maxBytes                        （按类型限定上限，如 MAX_NAME_STRING_BYTES）
//   length <= stream->bytesAvailable()        （流内剩余字节，防 over-read）
// 任一不满足：warn StringInvalidUtf8（或 StructureLimitExceeded）并返回空串，绝不 resize。
inline std::string ReadUtf8String(pag::DecodeStream* s, DecodeContext* ctx, size_t maxBytes) {
  uint32_t n = s->readEncodedUint32();
  if (n > maxBytes || n > s->bytesAvailable()) {
    ctx->warn(ErrorCode::StringInvalidUtf8, "utf8string length exceeds limit or stream");
    return {};
  }
  std::string out;
  out.resize(n);                                           // 仅在校验通过后 resize
  s->readBytes(reinterpret_cast<uint8_t*>(out.data()), n);
  if (!pagx::utils::IsValidUTF8(out)) {
    ctx->warn(ErrorCode::StringInvalidUtf8);
    return {};
  }
  return out;
}

// 读 varU32 length 前缀 + length 字节二进制数据（图片/字体/GlyphBlob 等）。
// 同样的 maxBytes + bytesAvailable 前置校验。
// errorCode 参数指定超限时推什么 warn（ImageResourceSizeExceeded 或 FontResourceSizeExceeded）。
//
// **P0-1 v2.17 生命周期纪律（取代 v2.16 的"默认零拷贝"）**：
// 默认实现走 `MakeAdopted(unique_ptr<uint8_t[]>)` ——拥有分配但跳过 `vector(n)` 的零初始化，
// 50 MB 字体仍省 ~20 ms（向 vector<uint8_t> 基线）。**零拷贝 `MakeWithoutCopy` 必须显式开启**：
// 调用方构造 `ZeroCopyScope::DecodeLocal local_scope(ctx)` 显式声明"本作用域内产出的 Data 只在
// Decoder 栈内使用、不会进入 PAGDocument 树、不会被 tgfx::Image 异步持有"——否则 tgfx::Image 的
// 延迟解码会把 shared_ptr<Data> 存活到 PAGLoader 释放 backing buffer 之后，形成 UAF。
//
// **硬约束**：`ImageAsset::data` / `FontAsset::data` 必须始终持有 **owning** Data——禁止
// PAGDocument 树内出现任何 `MakeWithoutCopy` 产物。Codec::Decode 出栈前若 `ZeroCopyScope` 活跃，
// 遍历所有产出 Data，对每个 owning 测试 `data.unique() && !scope.owning` 失败即 `MakeAdopted`
// 升级（一次 memcpy 兜底）。DecodeContext::streamBufferOwned() 已不再是判据——只有显式 scope 才开零拷贝。
inline std::shared_ptr<const tgfx::Data>
    ReadLengthPrefixedBytes(pag::DecodeStream* s, DecodeContext* ctx,
                            size_t maxBytes, ErrorCode errorCode) {
  uint32_t n = s->readEncodedUint32();
  if (n > maxBytes || n > s->bytesAvailable()) {
    ctx->warn(errorCode, "bytes length exceeds limit or stream");
    s->setPosition(std::min<size_t>(s->position() + std::min<uint64_t>(n, s->bytesAvailable()),
                                    s->size()));   // 尽力 skip 保持对齐
    return nullptr;
  }
  if (ctx->zeroCopyScopeActive()) {   // 仅在 ZeroCopyScope::DecodeLocal 内才允许零拷贝
    const uint8_t* slicePtr = s->currentReadablePtr();
    if (slicePtr != nullptr) {
      s->advance(n);
      return tgfx::Data::MakeWithoutCopy(slicePtr, n);   // lifetime 由 scope 负责
    }
  }
  // 默认路径：owning Data（alloc + read，跳过 vector 的零初始化）
  auto buf = std::make_unique<uint8_t[]>(n);
  s->readBytes(buf.get(), n);
  return tgfx::Data::MakeAdopted(buf.release(), n, tgfx::Data::DeleteArrayProc);
}
```

`ZeroCopyScope::DecodeLocal` 定义（`src/pagx/pag/DecodeContext.h`）：

```cpp
// 显式作用域，在内部允许 MakeWithoutCopy。仅适用于 Decoder 栈内使用、不入 PAGDocument 树的场景
// （例如 DecodeProbe 快速扫描 FileHeader）。范围退出时 ctx->zeroCopyScopeActive 复位为 false。
// ImageAsset.data / FontAsset.data **禁止**通过此 scope 构造——会导致 PAGLoader 释放后 UAF。
class ZeroCopyScope {
 public:
  class DecodeLocal {
   public:
    explicit DecodeLocal(DecodeContext* ctx) : ctx_(ctx) { ctx_->setZeroCopyActive(true); }
    ~DecodeLocal() { ctx_->setZeroCopyActive(false); }
    DecodeLocal(const DecodeLocal&) = delete;
    DecodeLocal& operator=(const DecodeLocal&) = delete;
   private:
    DecodeContext* ctx_;
  };
};
```

**强制清单**（所有按 "varU32 length + N bytes" 模式的字段必须走上述入口）：
- §D.6 `ImageAssetTable.data`（maxBytes = `MAX_IMAGE_BYTES`）
- §D.6 `FontAssetTable.family` / `style`（maxBytes = `MAX_NAME_STRING_BYTES`）
- §D.6 `FontAssetTable.data`（maxBytes = `MAX_FONT_BYTES`）
- §D.7 `Composition.id` / §D.8 `Layer.name`（maxBytes = `MAX_NAME_STRING_BYTES`）
- §D.11 `ElementText.text` / `fauxText`（maxBytes = `MAX_TEXT_STRING_BYTES`）
- §D.11 `GlyphRunBlob` 内所有 `utf8string` 字段（name/postScriptName/family/style 走 `MAX_NAME_STRING_BYTES`）

**Write 侧对称约束**：Baker 同样需在 Write 前检查 `string.size() <= maxBytes` / `bytes.size() <= maxBytes`，超限推 `StructureLimitExceeded=105` fatal（见 §H.2）。

### v1 `readEncodedUint32` / `readEncodedUint64` 的 5 / 10 字节硬停约束（P1-S4，强制）

ULEB128 u32 合法最多 5 字节（第 5 字节 continuation bit 必须为 0），u64 最多 10 字节。若底层 v1 实现未强制此上界，攻击者塞连续 continuation bit 字节让 decoder 线性扫描至 stream 末尾——每处 varU32 读取消耗 O(剩余字节)，数万字段级联即放大成二次 hang。**v2 约束**：

- 所有 `readEncoded*` 调用路径在读到第 5（u32）/ 10（u64）字节时，若该字节仍带 continuation bit（bit 7 = 1），必须：(a) warn `MalformedTag` + 返回 0 / 停止解析；(b) 位置推进到首个非 continuation 字节或 stream 末尾，择近者。
- 若 v1 `EncodeStream::readEncodedUint32` 未实现此约束，在 `src/pagx/pag/ValueCodec.h` 新增 `ReadEncodedUint32Safe(stream, ctx)` wrapper 替代直接调用——Phase 1 落 ValueCodec 时一并验证 v1 行为，不达标则走 wrapper。

### varU32 最短形式校验（P0-13 v2.18，强制用于安全关键字段）

ULEB128 允许同一数值有多种编码（`0x00` = 0 和 `0x80 0x00` = 0 都合法）。这打开两个侧道：
1. **Normalization bypass**：若下游任何去重/缓存/签名逻辑基于**字节序列**而非**解码值**做 key（layerPathByPagxLayer hash / fuzz corpus 签名 / content-addressable 缓存），同一值的两种编码会生成不同 key，重复计数或绕过去重；
2. **二次 hang 放大**：小值（< 128）被编为 5 字节仍触发第 5 字节校验路径的扫描成本。

**v2 约束**：
- `src/pagx/pag/ValueCodec.h` 新增 `ReadEncodedUint32Shortest(stream, ctx)` **严格** wrapper——检测非最短形式直接推 warn `MalformedTag`（非 warn，因严格关键路径不允许模糊）+ 返回 0 / 停止解析该 Tag；
- **强制用于**以下"安全关键索引字段"（值控制内存分配或循环上界）：
  - `CompositionRefPayload.compositionIndex`（§D.10）
  - `ImagePattern.patternImageIndex`（§D.9）
  - `Layer.fontIndex` / `GlyphRunBlob.fontIndex`（§D.11）
  - `LayerMaskRef.childIndex`（§D.9）
- 普通 varU32（length 字段、count 字段）继续走 `ReadEncodedUint32Safe` 宽松路径——非最短形式产生 warn 但不 reject（避免误杀合法 Writer 的懒编码）。
- 最短形式检测算法：`while ((byte & 0x80) != 0) ++consumedBytes; if (consumedBytes > 1 && decodedValue < (1u << (7 * (consumedBytes - 1)))) warn MalformedTag + reject`。

### 动态位宽数组的 numBits 上界（P1-S3，强制）

`readInt32List` / `readFloatList` / `readUBits` 读前缀 `numBits` 位宽——若 v1 未强制 `numBits ≤ 32`，攻击者塞 `numBits = 100` 让 `readBits(100)` 越界读 / OOB read / info leak。**v2 约束**：所有 `readInt32List` / `readFloatList` / `readUBits` 调用必须走 `src/pagx/pag/ValueCodec.h` 的 `ReadInt32ListSafe` / `ReadFloatListSafe` wrapper，读 `numBits` 后立即断言 `numBits <= 32`（float list 同样 32 位上限，超出即 warn + 返回空数组）；v1 侧若已原生校验则 wrapper 退化为直接转发。

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
  = 0: 裸 float 格式（调试/参考用；大样本体积大；**也用作短 Path 最优选择**）
  = 1: 量化格式（借鉴 v1 动态位宽列表编码，对齐 DataTypes.cpp:27-36 的 path record 3-bit verb 思路）
```

**Baker 侧格式择优**（P1-8 v2.15，强制）：Baker `WritePath` 前必须按 Path 规模择优——format=1 前缀至少 5 byte（`u8 precisionLog2 + varU32 verbCount + varU32 coordCount + numBits` + 可能 `varU32 conicCount`），对 ≤ 10 verb 的小 Path 反而比 format=0 胀。`ChoosePathFormat(path)` 规则：

```cpp
uint8_t ChoosePathFormat(const tgfx::Path& p) {
  uint32_t verbCount = p.countVerbs();
  if (verbCount < path_format::QUANTIZATION_MIN_VERBS) return 0;   // 极短直接 format=0
  if (verbCount <= 10) return 0;                                   // 边界：5 byte 前缀摊不下来
  // 坐标超 int24 范围（量化后溢出）也回退 format=0
  if (HasOutOfRangeCoord(p)) return 0;
  return 1;
}
```

不做运行时估算对比（开销 > 节省），固定按 verbCount 阈值分流。小 Path 样本（按钮/小图标）累积可省 10-15% body。

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
// P1-1 v2.16 强制 + P1-01 v2.19：坐标/权重合法性校验辅助——任何 NaN/Inf/超 int24 立即 warn MalformedTag + 返回空 Path
// 使用 static 自由函数而非 lambda（§2.1 禁 lambda 纪律）
static bool PathCoordIsFinite(float v) {
  return std::isfinite(v) &&
         std::abs(v) < static_cast<float>(path_format::QUANTIZATION_MAX_COORD);
}

tgfx::Path ReadPathV1(DecodeStream& stream, DecodeContext* ctx) {
  tgfx::Path out;
  Point currentEnd = {0, 0};
#define READ_POINT_SAFE(ptr)                                           \
  do {                                                                 \
    ptr = ReadPoint(stream);                                           \
    if (!PathCoordIsFinite(ptr.x) || !PathCoordIsFinite(ptr.y)) {      \
      ctx->warn(ErrorCode::MalformedTag, "Path coord NaN/Inf/OOR");    \
      return tgfx::Path{};                                             \
    }                                                                  \
  } while (0)

for (u32 i = 0; i < verbCount; i++) {
  u8 v = stream.readUint8();
  switch (v) {
    case 0: { Point p; READ_POINT_SAFE(p);
              out.moveTo(p.x, p.y); currentEnd = p; break; }
    case 1: { Point p; READ_POINT_SAFE(p);
              out.lineTo(p.x, p.y); currentEnd = p; break; }
    case 2: { Point p1, p2; READ_POINT_SAFE(p1); READ_POINT_SAFE(p2);
              out.quadTo(p1.x, p1.y, p2.x, p2.y); currentEnd = p2; break; }
    case 3: { Point p1, p2; READ_POINT_SAFE(p1); READ_POINT_SAFE(p2);
              float w = stream.readFloat();
              if (!std::isfinite(w) || w <= 0.0f || w > 1e6f) {   // conicWeight 合法区间 (0, +∞)，
                                                                  // 实用上限 1e6 防 rasterizer 退化
                ctx->warn(ErrorCode::MalformedTag, "conicWeight NaN/Inf/OOR");
                return tgfx::Path{};
              }
              out.conicTo(p1.x, p1.y, p2.x, p2.y, w); currentEnd = p2; break; }
    case 4: { Point p1, p2, p3;
              READ_POINT_SAFE(p1); READ_POINT_SAFE(p2); READ_POINT_SAFE(p3);
              out.cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y); currentEnd = p3; break; }
    case 5: { out.close(); break; }
    default: /* warn + abort this path */ return tgfx::Path{};
  }
  // 安全上限：见附录 H。Path 内 verb 数不超过 MAX_PATH_VERBS；否则返回空 path + warn。
  }
#undef READ_POINT_SAFE
  return out;
}
```

**理由**：format=0 的坐标和 conicWeight 读 `readFloat()` 直出，未做任何校验；tgfx::Path 接受 NaN/Inf 后续 rasterizer/PathMeasure 进入 UB（部分实现 `while(!converged)` 死循环），`int32_t(float)` 转换在越界 float 上也是 UB。format=1 量化格式天然受 int24（`path_format::QUANTIZATION_MAX_COORD`）钳制，无需补。

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
  // P0-12 v2.18：warn + return 空 Path 会让后续字段从错位位置读，外层 Tag 语义污染。
  // 所有失败路径改为 ctx->error（fatal）——触发 hasError() 中止主循环；上层 ReadLayerBlock
  // / ReadElementShapePath 见 hasError 后立即 return，不继续读该 Tag 内残余字段。
  uint8_t precisionLog2 = stream->readUint8();
  if (precisionLog2 > 12) {
    ctx->error(ErrorCode::MalformedTag, "path precisionLog2 out of range");
    return tgfx::Path{};
  }
  float scale = 1.0f / static_cast<float>(1u << precisionLog2);
  uint32_t verbCount = stream->readEncodedUint32();
  if (verbCount > limits::MAX_PATH_VERBS) {
    ctx->error(ErrorCode::PathVerbLimitExceeded, "path verb count exceeds MAX_PATH_VERBS");
    return tgfx::Path{};
  }

  std::vector<uint8_t> verbs(verbCount);
  for (uint32_t i = 0; i < verbCount; ++i) {
    verbs[i] = static_cast<uint8_t>(stream->readUBits(3));  // 3-bit verb kind
  }
  stream->alignWithBytes();

  uint32_t coordCount = stream->readEncodedUint32();
  // P0 安全校验：coordCount 必须与 verb 序列精确匹配（Move/Line=2, Quad=4, Conic=4, Cubic=6, Close=0），
  // 不匹配或超 MAX_PATH_COORDS 一律 fatal——防 16GB heap alloc 攻击。
  uint32_t expectedCoords = ExpectedCoordCountFromVerbs(verbs);    // 按 verb 数组计算
  if (coordCount != expectedCoords || coordCount > limits::MAX_PATH_COORDS) {
    ctx->error(ErrorCode::MalformedTag, "path coordCount mismatch or overflow");
    return tgfx::Path{};
  }
  std::vector<int32_t> rawCoords(coordCount);
  stream->readInt32List(rawCoords.data(), coordCount);      // 动态位宽反解

  uint32_t conicCount = stream->readEncodedUint32();
  // P0 安全校验：conicCount 必须 ≤ verb 序列中 Conic verb 数，且 ≤ MAX_PATH_CONIC_WEIGHTS。
  uint32_t expectedConics = CountConicVerbs(verbs);
  if (conicCount != expectedConics || conicCount > limits::MAX_PATH_CONIC_WEIGHTS) {
    ctx->error(ErrorCode::MalformedTag, "path conicCount mismatch or overflow");
    return tgfx::Path{};
  }
  std::vector<float> conicWeights(conicCount);
  if (conicCount > 0) {
    stream->readFloatList(conicWeights.data(), conicCount, 0.005f);
  }

  tgfx::Path out;
  uint32_t coordIdx = 0;
  uint32_t conicIdx = 0;
  // P1-8 v2.18：消除 lambda；改为内联 2 行或拆静态函数（按 Code.md 纪律）
  for (uint8_t v : verbs) {
    float x0, y0, x1, y1, x2, y2;
    switch (v) {
      case 0: {
        x0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        out.moveTo(x0, y0); break;
      }
      case 1: {
        x0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        out.lineTo(x0, y0); break;
      }
      case 2: {
        x0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        x1 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y1 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        out.quadTo(x0, y0, x1, y1); break;
      }
      case 3: {
        x0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        x1 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y1 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        float w = conicWeights[conicIdx++];
        out.conicTo(x0, y0, x1, y1, w); break;
      }
      case 4: {
        x0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y0 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        x1 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y1 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        x2 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        y2 = static_cast<float>(rawCoords[coordIdx++]) * scale;
        out.cubicTo(x0, y0, x1, y1, x2, y2); break;
      }
      case 5: { out.close(); break; }
      default:
        ctx->error(ErrorCode::MalformedTag, "path verb invalid");
        return tgfx::Path{};
    }
  }
  return out;
}
```

**编码侧选择策略**（Baker）：
- **默认使用 `pathFormat = 1`**（量化）+ `precisionLog2 = path_format::QUANTIZATION_DEFAULT_LOG2 = 4`（0.0625 px，对齐 v1 `SPATIAL_PRECISION`=0.05 的同级别精度）；
- 若 path 坐标绝对值 ≥ `path_format::QUANTIZATION_MAX_COORD`（量化后溢出 int24），或 verbCount < `path_format::QUANTIZATION_MIN_VERBS`（极短 path，量化头部开销大于收益），Encoder **自动回退** `pathFormat = 0`。

**`ChoosePathFormat` 伪码**（`src/pagx/pag/ValueCodec.h`）：

```cpp
inline uint8_t ChoosePathFormat(const tgfx::Path& path) {
  uint32_t verbCount = path.countVerbs();
  if (verbCount < path_format::QUANTIZATION_MIN_VERBS) {
    return 0;  // 极短 path，量化的 precisionLog2 + numBits 头部开销超过 float 的收益
  }

  // 扫描所有坐标，检查是否有值超出量化可承载的 int24 范围
  float scale = static_cast<float>(1u << path_format::QUANTIZATION_DEFAULT_LOG2);
  int64_t limit = path_format::QUANTIZATION_MAX_COORD;
  bool overflow = false;
  // P0-14 v2.18：用 int64 中间结果 + llroundf 避免 std::abs(INT32_MIN) UB
  // （roundf(-134217728 * 16) 得 INT32_MIN，abs(INT32_MIN) 仍负数，误判非溢出 → 量化乱飞）
  for (uint32_t vi = 0; vi < verbCount; ++vi) {
    tgfx::PathVerb verb;
    tgfx::Point points[4];
    float weight;
    path.getVerbAt(vi, &verb, points, &weight);              // 显式取 verb，避免回调 lambda
    int pointCount = PointCountOfVerb(verb);
    for (int i = 0; i < pointCount && !overflow; ++i) {
      int64_t qx64 = llroundf(points[i].x * scale);
      int64_t qy64 = llroundf(points[i].y * scale);
      if (qx64 >= limit || qx64 <= -limit || qy64 >= limit || qy64 <= -limit) {
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

**P0 安全校验：Tag length u32 加法回绕防护**（强制）：读到 `TagHeader` 后、seek 到 tag 末尾之前，必须用 `uint64_t` 做加法校验：

```cpp
// ❌ 错误：32-bit u32 加法会回绕（攻击者构造 tagLength=0xFFFFFFFE 让 Decoder 无限循环）
stream->setPosition(tagStart + headerSize + tagLength);

// ✅ 正确：uint64_t 中间结果 + 上界校验
uint64_t tagEnd = static_cast<uint64_t>(tagStart)
                + static_cast<uint64_t>(headerSize)
                + static_cast<uint64_t>(tagLength);
if (tagEnd > stream->size()) {
  ctx->error(ErrorCode::MalformedTag, "tag length overflow or exceeds body");
  return;
}
stream->setPosition(static_cast<size_t>(tagEnd));
```

此校验点同样适用于 §D.10 payload 内部的 SubStream slice 操作（`stream.slice(h.length)` 若 `length` 超剩余字节需 fatal）。

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

**P0 安全校验：canvas 尺寸有限且有限正整数**（强制）：读完 `width` / `height` 后立即校验：

```cpp
uint32_t ReadFileHeaderCanvasDims(DecodeStream* s, DecodeContext* ctx, float* outW, float* outH) {
  float w = s->readFloat();
  float h = s->readFloat();
  // 拒 NaN / Inf / 非正数 / 超 MAX_CANVAS_DIM（float→int 整型溢出防护 + GPU 纹理/缓冲大小防护）
  if (!std::isfinite(w) || !std::isfinite(h) ||
      w <= 0.0f || h <= 0.0f ||
      w > static_cast<float>(limits::MAX_CANVAS_DIM) ||
      h > static_cast<float>(limits::MAX_CANVAS_DIM)) {
    ctx->error(ErrorCode::MalformedTag, "FileHeader canvas size out of range");
    return 0;
  }
  *outW = w;
  *outH = h;
  return 1;
}
```

理由：`float → int` 转换在 NaN / Inf / 溢出 float 范围时是 C++ UB；随后 `tgfx::Surface::Make(w, h)` 按 `w × h × 4` 计算纹理字节数，32-bit 乘法可能回绕为小值分配但写入按 w × h 展开 → heap OOB。`MAX_CANVAS_DIM = 16384` 对齐 GPU 常见纹理硬上限（WebGL / iOS Metal 最保守值），本期一次性钉死。

### D.6 ImageAssetTable / FontAssetTable

> **Decoder 强制上界**：读完 `varU32 assetCount` 后立即校验：ImageAssetTable 要求 `assetCount ≤ limits::MAX_IMAGES`；FontAssetTable 要求 `assetCount ≤ limits::MAX_FONTS`。超限推 `StructureLimitExceeded=105` fatal 并 return（见 §H.1）。CompositionList 的 `compositionCount` 同理校验 `≤ MAX_COMPOSITIONS`（Baker 侧等价走 alias `BAKE_MAX_TOTAL_COMPOSITIONS`，P1-C v2.19）。

> **每实体独立 sub-Tag 约束**（P0-R1 v2.19，**字节布局重构**）：Round 9 v2.18 曾规划把 `kind` / `axes` 作为"repeat 裸结构末尾追加字段"（§6.5 ①），但裸 repeat 中**没有单 asset 的 TagHeader.length 可作边界切分**——追加 1 字节后，旧 Reader 读第 1 个 asset 仍按旧宽度读完，立即把第 2 个 asset 的 `dataLen` 首字节当成残余；从第 2 个 asset 起**全部字段错位**。v2.19 改为：`ImageAssetTable` / `FontAssetTable` body 仅携 `varU32 assetCount` + **repeat 个独立 `ImageAsset`/`FontAsset` sub-Tag**，每个 sub-Tag 自带 TagHeader（code + length），追加字段走 sub-Tag 内 length skip（真正的字段级追加通道）。本期就落此结构，动画期无兼容风险。

**ImageAssetTable**（容器 Tag，每个 asset 为 `ImageAsset=6` sub-Tag）：

```
TagCode::ImageAssetTable = 2
body:
  varU32 assetCount
  repeat assetCount:
    [ImageAsset Tag]              # 每个独立 TagHeader + body
```

```
TagCode::ImageAsset = 6
body:
  varU32 dataLen
  dataLen bytes  # 原始编码字节
  i32 width      # Decoder 强制 w > 0 && w <= MAX_CANVAS_DIM，否则 warn MalformedTag + 降级（P1-14 v2.18）
  i32 height     # 同上约束 h > 0 && h <= MAX_CANVAS_DIM——防负值×负值回绕到小 alloc 大 write 的 heap OOB
  u8  kind       # 0 = Raster（本期唯一支持）；1 = Svg / 2 = Video / 3 = Hdr 预留
                 # Decoder 强制校验 `kind == 0`：
                 #   - kind != 0 但 <= 3：warn UnsupportedFeature=104 + 强制回退为 Raster + 对 data 按 MakeFromEncoded 走
                 #   - kind > 3：warn MalformedTag=304 + 整 ImageAsset Tag skip
                 # 严禁未来任何 Inflater 路径基于字节流 kind 分派不同解码器（VideoDecoder/SvgParser 等）
                 # ——只信 Baker 侧原生 PAGX 节点 kind（防字节级攻击面扩散）。
  # 未来字段级追加：u8 colorSpace / varU32 pixelStride / ... 皆走本 sub-Tag 末尾追加，
  # 旧 Reader 按本 sub-Tag TagHeader.length skip 至下一 ImageAsset Tag，不会错位。
```

**FontAssetTable**（容器 Tag，每个 asset 为 `FontAsset=7` sub-Tag）：

```
TagCode::FontAssetTable = 3
body:
  varU32 assetCount
  repeat assetCount:
    [FontAsset Tag]               # 每个独立 TagHeader + body
```

```
TagCode::FontAsset = 7
body:
  u8          kind            # FontSourceKind (0=System, 1=Embedded, 2=VariableEmbedded 预留)
                              # Decoder 校验 `kind <= 2`；kind == 2 但本期无 Variable Font
                              # render 路径：同样 warn UnsupportedFeature=104 + 降级
                              # （按 Embedded 处理 + axes[] 字段忽略）
  utf8string  family
  utf8string  style
  varU32      dataLen         # kind == Embedded 时非零；Decoder 必须走 §H.5 magic 白名单（P0-17 v2.18）
  dataLen bytes
  varU32      axisCount       # Variable Font 轴数量（本期 Baker 恒写 0）
                              # Decoder 强制校验 `axisCount ≤ limits::MAX_FONT_AXES`（= 64，P0-H v2.19）
                              # 超限推 StructureLimitExceeded=105 fatal 并 return——
                              # 防 axisCount=0x0FFFFFFF → sizeof(FontAxis)*N ≈ 4GB OOM
  repeat axisCount:
    u32       tag             # 4-char ASCII tag
    f32       defaultValue
    f32       minValue
    f32       maxValue
  # 未来字段级追加走本 sub-Tag 末尾 length skip，不会错位。
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
  u32 width       # P2-11 v2.18：Decoder 强制 > 0（== 0 → warn MalformedTag + 回填为 1）；未校验会让 root Surface 0×0 触发 tgfx 未定义行为
  u32 height      # 同上
  varU32 layerCount
  repeat layerCount:
    [LayerBlock Tag]
```

### D.8 LayerBlock

**本表是 AI 落 LayerBlock Read/Write 代码的权威依据。**

> **v2.19 P0-R2 字节布局重构**：v2.18 及之前，Layer 级 Property（visible/alpha/blendMode/matrix/matrix3D/scrollRect）内联在 LayerBlock body 顶层。v2.19 把这 6 个 Property 整体挪入独立 `LayerTransform=15` sub-Tag（见下文 §D.9）——理由：**本期就打通 §4.4 规则 1 的"未知 encoding 收窄到 LayerTransform skip"兜底路径**。若保持内联，动画期给 Layer Property 引入未知 encoding 时 Reader 无合法 skip 粒度（只能丢整 Layer），或只能升 FORMAT_VERSION（打脸 §13 "不升版本号"承诺）。v2 未发布，纯字节布局重构零兼容代价。

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

  # 可选子 Tag 序列（按出现顺序；不出现则跳过，由 tagLength 兜底）
  # 每个子 Tag 独立 TagHeader + body
  [LayerTransform Tag]             # v2.19 P0-R2：强制 1 次（承载 visible/alpha/blendMode/matrix/matrix3D/scrollRect）
                                   # Baker 本期恒写；Reader 侧若缺失推 MalformedTag=304 fatal + 整 LayerBlock 丢弃
                                   # 未来未知 encoding 触发 §4.4 规则 1 时 `seek(layerTransformTagEnd)` 合法
  [LayerMaskRef Tag]?              # 仅当 !maskLayerPath.empty()；Reader 校验同 LayerBlock 内至多 1 次，第 2+ 次 304 fatal
  [LayerFilters Tag]?              # 仅当 !filters.empty()；同上唯一性约束
  [LayerStyles Tag]?               # 仅当 !styles.empty()；同上唯一性约束

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

**LayerBlock 子 Tag 唯一性约束**（P1-05 v2.19，强制）：Reader 循环读子 Tag 时维护 `seen` 位集；同一子 TagCode（LayerTransform / LayerMaskRef / LayerFilters / LayerStyles）**最多出现 1 次**，第 2+ 次推 `MalformedTag=304` fatal 并丢弃整个 LayerBlock（含子 Layer 子树）。防 Writer bug / 攻击者注入两个 LayerMaskRef（一个合法路径 + 一个指向环）造成 Pass 2.5 SCC "哪个赢" 不确定。

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

### D.9 LayerTransform / LayerMaskRef / LayerFilters / LayerStyles

```
TagCode::LayerTransform = 15                # v2.19 P0-R2 新增；LayerBlock 内强制 1 次
body:
  # 6 个 Property 字段（每个带 1 byte propHeader 前缀，见 §4.3）——
  # 从 v2.18 LayerBlock body 内联迁出；Baker 本期恒写；Reader 缺失本 sub-Tag 推 304 fatal
  Property<bool>      visible
  Property<float>     alpha
  Property<BlendMode> blendMode    # 字节层 u8；ReadProperty<BlendMode> 走 EnumMeta 值域校验
  Property<Matrix>    matrix
  Property<Matrix3D>  matrix3D

  # 条件字段：仅当 LayerBlock.layerFlags.bit0 (hasScrollRect) == 1 时序列化
  # Reader 从 LayerBlock 继承 layerFlags 决定是否读 scrollRect（父子共享上下文）
  if (parent LayerBlock).layerFlags & 0x01:
    Property<Rect>    scrollRect

  # 未来字段级追加：LayerTransform sub-Tag 末尾追加 `[AnimationData Tag]?`（§D.14）
  # 承载 keyframe 序列；旧 Reader 按本 sub-Tag TagHeader.length skip，静态 fallback 为 Constant 首帧。
```

**条件字段的 Read 侧兜底**：若 `layerFlags & 0x01 == 0` 但 LayerTransform body 尾部仍有未读字节（动画期 AnimationData 追加或 scrollRect 残留），由本 sub-Tag 的 `TagHeader.length` 兜底 seek 到 tagEnd。

```
TagCode::LayerMaskRef = 12
body:
  u8              maskType        # LayerMaskType
  varU32          pathDepth       # Decoder 校验 pathDepth ≤ limits::MAX_MASK_PATH_DEPTH；超限 StructureLimitExceeded=105 fatal
  repeat pathDepth:
    varU32 childIndex             # Decoder 校验 childIndex < limits::MAX_CHILDREN_PER_LAYER
                                  # （虽然 Pass 2 Inflater 查 layerByPath 时不会越界，但这里的上限避免
                                  # 攻击者塞 UINT32_MAX 使 PackLayerPath 打包出异常位图，弱化索引唯一性
                                  # 或让后续 hash 分布退化）；超限 → MalformedTag=304 fatal
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
  // P0 安全校验（与 §D.3 Tag length u32 回绕同模式）：uint64_t 中间结果 + 上界校验。
  // tagEnd 由 Codec 主循环传入；32-bit size_t 加法会回绕让指针回退重复派发 → 无限循环。
  uint64_t shapeStyleContentEnd64 = static_cast<uint64_t>(shapeStyleStart)
                                  + static_cast<uint64_t>(innerLenBytes)
                                  + static_cast<uint64_t>(innerLen);
  if (shapeStyleContentEnd64 > tagEnd) {
    ctx->error(ErrorCode::MalformedTag, "ShapeStyle innerLength overflow or exceeds tag body");
    return {};
  }
  auto shapeStyleContentEnd = static_cast<size_t>(shapeStyleContentEnd64);

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
    u32 patternImageIndex     # UINT32_MAX = 图片缺失哨兵；否则 Decoder 读后校验
                              # `patternImageIndex < doc.images.size()`（在 ImageAssetTable 解码后），
                              # 不合法 → warn InvalidEnumValue（407）并重置为 UINT32_MAX 哨兵降级
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
TagCode::FilterBlur (120)        body: Property<float> blurX, Property<float> blurY, u8 tileMode
TagCode::FilterDropShadow (121)  body: Property<float> offsetX, Property<float> offsetY,
                                       Property<float> blurX, Property<float> blurY,
                                       Property<Color> color, bool shadowOnly
TagCode::FilterInnerShadow (122) body: 同上（offset/blur/color/shadowOnly）
TagCode::FilterColorMatrix (123) body: Property<array<float,20>> colorMatrix
TagCode::FilterBlend (124)       body: Property<Color> blendColor, u8 blendFilterMode

TagCode::StyleDropShadow (140)   body: bool excludeChildEffects, u8 blendMode,
                                       Property<float> offsetX, Property<float> offsetY,
                                       Property<float> blurX, Property<float> blurY,
                                       Property<Color> color, bool showBehindLayer
TagCode::StyleInnerShadow (141)  body: bool excludeChildEffects, u8 blendMode,
                                       Property<float> offsetX, Property<float> offsetY,
                                       Property<float> blurX, Property<float> blurY,
                                       Property<Color> color
TagCode::StyleBackgroundBlur (142) body: bool excludeChildEffects, u8 blendMode,
                                         Property<float> blurX, Property<float> blurY,
                                         u8 tileMode
```

### D.13 每个 Tag 的 Read/Write 函数签名模板（强制约束）

**写入函数统一签名**：
```cpp
namespace pagx::pag {

// 非嵌套 Tag（写 TagHeader + body + 对齐）
void Write<TagName>(EncodeStream* stream, const <Data>& data, EncodeSession* session);

// 嵌套 element/payload 无 TagHeader 的情况使用 inline 变体
void Write<TagName>Inline(EncodeStream* stream, const <Data>& data, EncodeSession* session);

// 读取
std::unique_ptr<<Data>> Read<TagName>(DecodeStream* stream, DecodeContext* ctx);
<Data> Read<TagName>Inline(DecodeStream* stream, DecodeContext* ctx);

}
```

**风格强约束**：
- Write 函数：接受 `const ref`，返回 `void`，通过 `session->diag->pushWarning(...)` 推送告警；
- Read 函数：接受 `stream + ctx`，成功返回 `unique_ptr<Data>` / `Data`；失败返回 `nullptr` / 默认构造并推送 error；
- 每个 Read 函数开头检查 `stream->bytesAvailable() >= MinimumTagSize<TagName>`（常量放在 Tag 对应 cpp 文件顶部）；
- 所有 Read\*Tag 完成后由上层统一 `stream->setPosition(tagEnd)` 强制对齐。

### D.14 AnimationData sub-Tag 草案（动画期落地，本期不产出）

**设计背景**（P1-15 v2.19）：§4.4 规则 1 + P2-9 "静态/动画双写" 规约要求动画期 Writer 在可动画 Property 的 propHeader encoding=Constant 首帧 value 之后，**追加** `AnimationData` sub-Tag 承载 keyframe 序列；旧 Reader UnknownTagCode skip 此 Tag 只得静态首帧。本期 Baker 不产出，但为动画期 ship 前就必须钉死布局——避免届时 scope 漂移导致 2 次结构变更。

**挂载点枚举**（`AnimationData` 作为其他容器 sub-Tag 的末尾字段级追加）：

| TagCode 值 | 枚举项 | 挂载语义 |
|---|---|---|
| 160 | `AnimationData::ForLayerTransform` | 挂在 `LayerTransform=15` body 末尾，承载 visible/alpha/blendMode/matrix/matrix3D/scrollRect 的 keyframe 序列（§D.9） |
| 161 | `AnimationData::ForVectorElement` | 挂在 VectorElement Tag body 末尾（§D.11），承载该 element 内 Property 的 keyframe |
| 162 | `AnimationData::ForPayload` | 挂在 Payload Tag body 末尾（§D.10），承载 Payload 内顶层 Property 的 keyframe（如 CompositionRef.loopMode） |

**字节布局**（本期草案，动画期开工前可微调但不得改宽度约束）：

```
TagCode::AnimationData{ForLayerTransform | ForVectorElement | ForPayload}
body:
  varU32 propertyCount
  repeat propertyCount:
    u8     propertyId        # 对照该容器的 Property 字段顺序（enum 化）
                             # ForLayerTransform 枚举：
                             #   0 = visible, 1 = alpha, 2 = blendMode,
                             #   3 = matrix, 4 = matrix3D, 5 = scrollRect
                             # ForVectorElement 枚举：按 §D.11 各 element 字段顺序（局部 0..N）
                             # ForPayload 枚举：按 §D.10 各 payload 字段顺序（局部 0..N）
    u8     keyframeEncoding  # KeyframeEncoding: 0=Hold, 1=Linear, 2=Bezier, 3=Spatial
    varU32 keyframeCount     # Decoder 校验 ≤ limits::MAX_KEYFRAMES_PER_PROPERTY（动画期新增，本期预留 4096）
    repeat keyframeCount:
      u32         time       # ms，相对 Layer.startTime
      T           value      # 该 Property 的值类型（float/bool/Matrix/Color/...）
      if keyframeEncoding in {Bezier, Spatial}:
        f32×4     bezierHandles   # 时间维度 in/out
      if keyframeEncoding == Spatial:
        Point×2   spatialHandles  # 空间维度 in/out（仅 matrix/matrix3D/Point 类 Property）
```

**优先级规约**（对齐 §6.5 ② 双写）：

- 旧 Reader（v2.0 或不识别 AnimationData 的版本）：UnknownTagCode 400 warn + skip Tag → 只得 Constant 首帧 value，**退化为静态首帧渲染**（非整层消失，关键 property）。
- 新 Reader：**后写覆盖**——静态首帧 value 先读入 `PAGDocument`；随后读 AnimationData 时按 `propertyId` 定位字段并 override 为动画版 `AnimatableValue<T>`；两者 LIFO 语义（对齐 §6.5 ② 变体 Tag 的"新 Tag 赢"纪律）。

**双写纪律**：静态文件（本期 Baker 产出）**不写 AnimationData**——"双写" 仅对动画文件生效，消除"静态文件也要写 AnimationData 冗余 Constant" 的误读。

**MAX_KEYFRAMES_PER_PROPERTY 预留**（P1-15 v2.19）：`limits::MAX_KEYFRAMES_PER_PROPERTY = 4096` 在 §H.1 预留声明；动画期开工时 Decoder 按此校验防 OOM。

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

**新开段规则**（P2-6 v2.18）：当未来需要新 stage（如动画期 `AnimationEvaluator`），按"**Fatal 偶百段 / Warning 奇百段**"规则分配：
- Fatal：偶数百段（100 / 300 / 500 / **700** / **900**）
- Warning：紧随其后奇数百段（200 / 400 / 600 / **800** / **1000**）

例：动画期新 stage = Fatal 700-799 + Warning 800-899；`StageOf()` switch 同时扩展。`kAllDiagnosticCodes[]` 数组 size 按新增条数 + 原长度刷新。段号**不跨 100 边界**，确保 `SeverityOf(code)` 可以稳定按 `(code / 100) % 2 == 0 ? Fatal : Warning` 判定。

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
  ProducerFatal             = 106,  // P1-5 v2.17：Decoder 读到 TagCode::ErrorMarker（见 §8.3ter）

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
  ImageResourceSizeExceeded = 402,  // 图片单张字节数超 MAX_IMAGE_BYTES（v2.16 从旧 ResourceSizeExceeded=402 重命名；枚举值 ABI 保持不变）
  // 向后兼容别名：using ResourceSizeExceeded = ImageResourceSizeExceeded;（在 ErrorCode.h 提供 alias，v2.16 代码禁用旧名）
  StringInvalidUtf8         = 403,
  PathVerbLimitExceeded     = 404,
  GlyphCountLimitExceeded   = 405,
  LayerDepthLimitExceeded   = 406,
  InvalidEnumValue          = 407,  // u8 枚举值域外数值，已回填默认值
  FontResourceSizeExceeded  = 408,  // 字体单份字节数超 MAX_FONT_BYTES（v2.16 新增，从旧 402 的合并语义拆出）
  PrematureEndTag           = 409,  // P0-15 v2.18：中途 End Tag 截断（见 §8.3）

  // ---------------- Inflater 告警 (600-699) ----------------
  InflateImageDecodeFailed  = 600,  // tgfx::Image::MakeFromEncoded 返回 null
  InflateFontCreateFailed   = 601,  // tgfx::Typeface 创建失败，已降级系统字体
  InflateGlyphRunBuildFailed = 602, // GlyphRunRenderer 返回 null，Text element 被跳过
  InflateMaskResolveFailed  = 603,  // maskLayerPath 解析失败
  InflaterEmptyDocument     = 604,  // compositions[0] 缺失或为空；Result.layer == nullptr 且本 warning 明示原因
  InflateCompositionCycle   = 605,  // Inflater 检测到 composition 引用环或超 MAX_COMPOSITION_REF_DEPTH；局部降级
  InflaterLayerBudgetExceeded = 606, // 累计 tgfx::Layer 实例超 MAX_INFLATED_LAYER_COUNT；子树降级为空壳
  InflateMaskCycle          = 607,   // P0-16 v2.18：Layer mask 循环；环上 mask 全部跳过
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

**命名双轨硬规则**（P0-8 v2.18，强制）：ErrorCode 与 DiagnosticCode 是同一枚举的两个名字，但调用点**必须**按文件路径选择写法：

| 文件路径 | 必用枚举名 | 原因 |
|---|---|---|
| `src/pagx/pag/**` 内部文件（Baker/Codec/Inflater 实现） | `ErrorCode::X` | 保持与 v2.5 及以前内部代码调用点一致；alias 零改动 |
| `include/pagx/**` 公共头（PAGExporter/PAGLoader/Diagnostic） | `DiagnosticCode::X` | 对外 API 语义明确——"这是诊断码"而非"内部错误码" |
| `test/src/**` 测试断言 | `DiagnosticCode::X` | 测试消费公共 API 路径，与对外语义一致 |
| `tools/**` CLI / Fuzz harness | `DiagnosticCode::X` | 工具调用对外 API |

违反示例：`test/src/pag/BakerTest.cpp` 里写 `ErrorCode::LayoutNotApplied` 虽然编译通过（alias 解析），但跨 include 路径可能触发 `pagx/pag/ErrorCode.h` 与 `pagx/Diagnostic.h` 的双包含歧义；code review 应一律回退为 `DiagnosticCode::`。

### G.3 Diagnostic 结构（对外 struct，权威定义见 §15.1）

`Diagnostic` 类型定义在对外公共头 `include/pagx/Diagnostic.h`（§15.1）。字段语义：

| 字段 | 类型 | 语义 |
|---|---|---|
| `code` | `pagx::DiagnosticCode` | 机器可读码；调用方按 code 分支处理 |
| `message` | `std::string` | 人类补充信息（如 "image at index 3 missing"），可为空；**不稳定英文调试文本，禁用于 switch / 持久化**（§15.1） |
| `byteOffset` | `uint32_t` | Decoder 路径 = `stream->position()` 快照；其他路径 = 0 |
| `contextIndex` | `uint32_t` | 结构化资源/Layer 引用（P0-1 v2.16）；`UINT32_MAX` = 不适用；消费**必须**先检查 `!= UINT32_MAX`（P0-7 v2.18）；code → 语义映射见 §15.1 ABI 表 |

**内部 alias**：`src/pagx/pag/ErrorCode.h` 在引入 `using ErrorCode = pagx::DiagnosticCode;` 的同时，也 `using Diagnostic = pagx::Diagnostic;`——内部代码写 `Diagnostic d{ErrorCode::X, msg, offset}` 完全兼容。

**byteOffset 填写规则**：

| 路径 | 填写时机 | 填写值 |
|---|---|---|
| **Decoder（Codec::Decode）** | 每次 `ctx->error(code, msg)` / `ctx->warn(code, msg)` 调用时 | `stream->position()`（当前读指针，即错误发生时已读到的字节偏移） |
| **Encoder（Codec::Encode）** | warning 时（Encoder 不产生 error） | `0`（Encoder 是线性 append，偏移对调试帮助不大，留空） |
| **Baker（Baker::Bake）** | `ctx->error(code, msg)` / `ctx->warn(code, msg)` 时 | `0`（Baker 操作的是 PAGXDocument 对象树，没有字节流上下文） |
| **Inflater（LayerInflater::Inflate）** | warning 时 | `0`（操作的是 PAGDocument 对象树，无字节流） |
| **PAGLoader（LoadFromFile）** | 文件 open/read 失败产出 `FileReadFailed` 时 | `0`（未进入字节流） |

**便捷构造辅助 + 全枚举表**（`src/pagx/pag/DiagBuild.h`，内部辅助头——**特意避开与对外 `pagx/Diagnostic.h` 同名**以防止 include 歧义）：

```cpp
#include <array>
#include "pagx/Diagnostic.h"     // 公共类型
#include "pagx/pag/ErrorCode.h"  // 内部 alias

namespace pagx::pag {

// 用于 Decoder：自动填充 byteOffset（contextIndex 可选，默认 UINT32_MAX）
inline Diagnostic MakeDecodeDiag(ErrorCode code, pag::DecodeStream* stream,
                                 std::string msg = {},
                                 uint32_t contextIndex = UINT32_MAX) {
  return {code, std::move(msg), static_cast<uint32_t>(stream->position()), contextIndex};
}

// 用于 Baker/Inflater/Loader：byteOffset 为 0
inline Diagnostic MakeDiag(ErrorCode code, std::string msg = {},
                           uint32_t contextIndex = UINT32_MAX) {
  return {code, std::move(msg), 0, contextIndex};
}

// 全枚举值表（P0-6 v2.16，Phase 0 交付）：DiagnosticTest.CodeToString.AllEnumValues 依赖此表
// 遍历断言 CodeToString(c) 非空。新增 enum 值时**必须**在本表补一项，否则 Phase 0 DiagnosticTest 挂，
// 由此保证 §G.3bis Diagnostic.cpp switch case 与 enum 声明不发生 desync。
inline constexpr std::array<DiagnosticCode, 43> kAllDiagnosticCodes = {
  DiagnosticCode::Ok,
  // Baker fatal 100-199
  DiagnosticCode::LayoutNotApplied,
  DiagnosticCode::UnresolvedImports,
  DiagnosticCode::NullDocument,
  DiagnosticCode::EmptyCompositions,
  DiagnosticCode::CompositionCycleDepth,
  DiagnosticCode::StructureLimitExceeded,
  DiagnosticCode::ProducerFatal,   // P1-5 v2.17
  // Baker warning 200-299
  DiagnosticCode::ImageSourceMissing,
  DiagnosticCode::FontSourceMissing,
  DiagnosticCode::MaskTargetMissing,
  DiagnosticCode::MaskSelfReference,
  DiagnosticCode::InverseMatrixNonInvertible,
  DiagnosticCode::TextGlyphDataEmpty,
  DiagnosticCode::EmptyDocument,
  DiagnosticCode::GlyphRunKindInferred,
  DiagnosticCode::BlendModeUnmapped,          // v2.19 P0-E 回补：枚举声明已有但未列入，致 Phase 0 AllEnumValues 静默豁免
  // Codec fatal 300-399
  DiagnosticCode::InvalidMagic,
  DiagnosticCode::UnsupportedVersion,
  DiagnosticCode::UnsupportedCompression,
  DiagnosticCode::TruncatedData,
  DiagnosticCode::MalformedTag,
  DiagnosticCode::BodyLengthOutOfRange,
  DiagnosticCode::InvalidCompositionIndex,
  DiagnosticCode::FileReadFailed,
  // Codec warning 400-499
  DiagnosticCode::UnknownTagCode,
  DiagnosticCode::UnknownPropertyEncoding,
  DiagnosticCode::ImageResourceSizeExceeded,
  DiagnosticCode::StringInvalidUtf8,
  DiagnosticCode::PathVerbLimitExceeded,
  DiagnosticCode::GlyphCountLimitExceeded,
  DiagnosticCode::LayerDepthLimitExceeded,
  DiagnosticCode::InvalidEnumValue,
  DiagnosticCode::FontResourceSizeExceeded,
  DiagnosticCode::PrematureEndTag,            // v2.19 P0-E 补：409 End 出现时 body 未读完
  // Inflater warning 600-699
  DiagnosticCode::InflateImageDecodeFailed,
  DiagnosticCode::InflateFontCreateFailed,
  DiagnosticCode::InflateGlyphRunBuildFailed,
  DiagnosticCode::InflateMaskResolveFailed,
  DiagnosticCode::InflaterEmptyDocument,
  DiagnosticCode::InflateCompositionCycle,
  DiagnosticCode::InflaterLayerBudgetExceeded,
  DiagnosticCode::InflateMaskCycle,           // v2.19 P0-E 补：607 Pass 2.5 Tarjan SCC 环检测
};
// static_assert 强制随 enum 增长同步扩表
// 实现：DiagnosticTest 遍历 kAllDiagnosticCodes，断言 `CodeToString(c)` 非 nullptr 且非空。
// 同时 `static_assert(kAllDiagnosticCodes.size() == <当前枚举总数>)` 硬绑定（在 DiagBuild.cpp 或 test）。

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

### G.3bis Diagnostic.cpp 实现规范（阶段 0 落地）

**`FormatDiagnostic` 形态**（`src/pagx/Diagnostic.cpp`）：

```cpp
#include "pagx/Diagnostic.h"
#include <cstdio>

namespace pagx {

// 手写 switch（非 X-macro）—— 显式枚举 ⇄ 字符串映射，便于 grep；
// default 分支返回 "Code(<数字>)" 供未来新增枚举值的 forward-compat 路径。
static const char* CodeToString(DiagnosticCode c) {
  switch (c) {
    case DiagnosticCode::Ok:                         return "Ok";
    case DiagnosticCode::LayoutNotApplied:           return "LayoutNotApplied";
    case DiagnosticCode::UnresolvedImports:          return "UnresolvedImports";
    case DiagnosticCode::NullDocument:               return "NullDocument";
    case DiagnosticCode::EmptyCompositions:          return "EmptyCompositions";
    case DiagnosticCode::CompositionCycleDepth:      return "CompositionCycleDepth";
    case DiagnosticCode::StructureLimitExceeded:     return "StructureLimitExceeded";
    case DiagnosticCode::ProducerFatal:              return "ProducerFatal";   // P1-5 v2.17
    case DiagnosticCode::ImageSourceMissing:         return "ImageSourceMissing";
    case DiagnosticCode::FontSourceMissing:          return "FontSourceMissing";
    case DiagnosticCode::MaskTargetMissing:          return "MaskTargetMissing";
    case DiagnosticCode::MaskSelfReference:          return "MaskSelfReference";
    case DiagnosticCode::BlendModeUnmapped:          return "BlendModeUnmapped";
    case DiagnosticCode::InverseMatrixNonInvertible: return "InverseMatrixNonInvertible";
    case DiagnosticCode::TextGlyphDataEmpty:         return "TextGlyphDataEmpty";
    case DiagnosticCode::EmptyDocument:              return "EmptyDocument";
    case DiagnosticCode::GlyphRunKindInferred:       return "GlyphRunKindInferred";
    case DiagnosticCode::InvalidMagic:               return "InvalidMagic";
    case DiagnosticCode::UnsupportedVersion:         return "UnsupportedVersion";
    case DiagnosticCode::UnsupportedCompression:     return "UnsupportedCompression";
    case DiagnosticCode::TruncatedData:              return "TruncatedData";
    case DiagnosticCode::MalformedTag:               return "MalformedTag";
    case DiagnosticCode::BodyLengthOutOfRange:       return "BodyLengthOutOfRange";
    case DiagnosticCode::InvalidCompositionIndex:    return "InvalidCompositionIndex";
    case DiagnosticCode::FileReadFailed:             return "FileReadFailed";
    case DiagnosticCode::UnknownTagCode:             return "UnknownTagCode";
    case DiagnosticCode::UnknownPropertyEncoding:    return "UnknownPropertyEncoding";
    case DiagnosticCode::ImageResourceSizeExceeded:  return "ImageResourceSizeExceeded";
    case DiagnosticCode::StringInvalidUtf8:          return "StringInvalidUtf8";
    case DiagnosticCode::PathVerbLimitExceeded:      return "PathVerbLimitExceeded";
    case DiagnosticCode::GlyphCountLimitExceeded:    return "GlyphCountLimitExceeded";
    case DiagnosticCode::LayerDepthLimitExceeded:    return "LayerDepthLimitExceeded";
    case DiagnosticCode::InvalidEnumValue:           return "InvalidEnumValue";
    case DiagnosticCode::FontResourceSizeExceeded:   return "FontResourceSizeExceeded";
    case DiagnosticCode::PrematureEndTag:            return "PrematureEndTag";        // v2.19 P0-F 补
    case DiagnosticCode::InflateImageDecodeFailed:   return "InflateImageDecodeFailed";
    case DiagnosticCode::InflateFontCreateFailed:    return "InflateFontCreateFailed";
    case DiagnosticCode::InflateGlyphRunBuildFailed: return "InflateGlyphRunBuildFailed";
    case DiagnosticCode::InflateMaskResolveFailed:   return "InflateMaskResolveFailed";
    case DiagnosticCode::InflaterEmptyDocument:      return "InflaterEmptyDocument";
    case DiagnosticCode::InflateCompositionCycle:    return "InflateCompositionCycle";
    case DiagnosticCode::InflaterLayerBudgetExceeded: return "InflaterLayerBudgetExceeded";
    case DiagnosticCode::InflateMaskCycle:           return "InflateMaskCycle";       // v2.19 P0-F 补
  }
  return nullptr;   // caller 走 "Code(%u)" fallback
}

std::string FormatDiagnostic(const Diagnostic& d) {
  char buf[32];
  const char* name = CodeToString(d.code);
  std::string out = "[";
  if (name) { out += name; }
  else      { std::snprintf(buf, sizeof(buf), "Code(%u)", static_cast<unsigned>(d.code));
              out += buf; }
  out += "]";
  if (!d.message.empty()) { out += ' '; out += d.message; }
  if (d.byteOffset != 0) { std::snprintf(buf, sizeof(buf), " @0x%x", d.byteOffset); out += buf; }
  if (d.contextIndex != UINT32_MAX) {
    std::snprintf(buf, sizeof(buf), " #ctx=%u", d.contextIndex);
    out += buf;
  }
  return out;
}

DiagnosticSeverity SeverityOf(DiagnosticCode c) {
  uint16_t v = static_cast<uint16_t>(c);
  // Fatal 段：100-199 / 300-399 / 500-599；Warning 段：200-299 / 400-499 / 600-699。
  if ((v >= 100 && v < 200) || (v >= 300 && v < 400) || (v >= 500 && v < 600)) {
    return DiagnosticSeverity::Fatal;
  }
  return DiagnosticSeverity::Warning;
}

DiagnosticStage StageOf(DiagnosticCode c) {
  uint16_t v = static_cast<uint16_t>(c);
  if (v >= 100 && v < 300) return DiagnosticStage::Baker;
  if (v >= 300 && v < 500) return DiagnosticStage::Codec;
  return DiagnosticStage::Inflater;  // 500-699 及 Ok（0）兜底
}

}  // namespace pagx
```

**DiagnosticTest.cpp 断言清单**（阶段 0 必过，共 10 条）：

| 测试名 | 断言 |
|---|---|
| `FormatDiagnostic.OkCode` | `FormatDiagnostic({Ok}) == "[Ok]"` |
| `FormatDiagnostic.NoMessageNoOffset` | `FormatDiagnostic({LayoutNotApplied}) == "[LayoutNotApplied]"` |
| `FormatDiagnostic.WithMessage` | `FormatDiagnostic({LayoutNotApplied, "must call applyLayout", 0}) == "[LayoutNotApplied] must call applyLayout"` |
| `FormatDiagnostic.WithOffset` | `FormatDiagnostic({TruncatedData, "unexpected EOF", 0x4a2c}) == "[TruncatedData] unexpected EOF @0x4a2c"` |
| `FormatDiagnostic.UnknownCodeFallback` | 构造 `static_cast<DiagnosticCode>(9999)` → 格式为 `"[Code(9999)]"` |
| `FormatDiagnostic.WithContextIndex` | `FormatDiagnostic({ImageSourceMissing, "missing", 0, 3}) == "[ImageSourceMissing] missing #ctx=3"`；同时验证 `contextIndex=UINT32_MAX` 时不输出 `#ctx` 段（P1-3 v2.16）|
| `SeverityOf.FatalSegments` | `SeverityOf(LayoutNotApplied) == Fatal`，`SeverityOf(TruncatedData) == Fatal` |
| `SeverityOf.WarningSegments` | `SeverityOf(ImageSourceMissing) == Warning`，`SeverityOf(InflateImageDecodeFailed) == Warning` |
| `StageOf.AllThreeStages` | `StageOf(LayoutNotApplied) == Baker`，`StageOf(InvalidMagic) == Codec`，`StageOf(InflaterEmptyDocument) == Inflater` |
| `CodeToString.AllEnumValues` | **遍历 `kAllDiagnosticCodes`（DiagBuild.h 中定义的 `constexpr std::array<DiagnosticCode, N>` 全枚举表，P0-6 v2.16）**，断言每个 code 的 `CodeToString(c) != nullptr && string(CodeToString(c)).size() > 0`。保护：新增 enum 值未在 `kAllDiagnosticCodes` 同步添加时 DiagnosticTest 挂 static_assert；未在 switch case 添加时 `CodeToString` 走 fallback `Code(%u)` 路径（字符串不匹配 enum 名）测试挂。双重防御。|

**ABI-appended 码兼容性**：测试断言 "FormatDiagnostic 对 `static_cast<DiagnosticCode>(9999)` 返回 `[Code(9999)]`" 即覆盖——保证未来段内追加新码时旧二进制不崩。

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

**`contextIndex` 端到端消费示例**（P1-10 v2.16）：Diagnostic 的 `contextIndex` 字段提供结构化的资源/layer/composition 下标（见 §15.1 ABI 表），SDK 用户无需 grep message 英文串即可定位失败的具体资源——以下 3 个典型场景：

```cpp
// 场景 1：Exporter 导出时某张图缺失，UI 回显 "图 X 路径无效，请重选"
auto r = pagx::PAGExporter::ToBytes(*pagxDoc);
for (const auto& d : r.warnings) {
  if (d.code == pagx::DiagnosticCode::ImageSourceMissing &&
      d.contextIndex != UINT32_MAX &&
      d.contextIndex < pagxDoc->images().size()) {
    const auto& imageNode = pagxDoc->images()[d.contextIndex];
    ShowToast("Image missing: " + imageNode.filePath);
  }
}

// 场景 2：Loader 解码时 ImageResourceSizeExceeded，前端降级（跳过该图）
auto lr = pagx::PAGLoader::LoadFromFile(path);
for (const auto& d : lr.warnings) {
  if (d.code == pagx::DiagnosticCode::ImageResourceSizeExceeded) {
    // P1-2 v2.17：UINT32_MAX 守卫——跨 Tag bridge push 可能推 sentinel
    if (d.contextIndex == UINT32_MAX) { LogWarn("Skipped oversized image"); continue; }
    LogWarn("Skipped oversized image #%u", d.contextIndex);
  }
}

// 场景 3：检测 composition 循环引用，回溯构图阶段
for (const auto& d : lr.warnings) {
  if (d.code == pagx::DiagnosticCode::InflateCompositionCycle) {
    if (d.contextIndex == UINT32_MAX) { LogError("Composition cycle (index unknown)"); continue; }
    // contextIndex 是引用环起点 composition 下标
    LogError("Composition cycle at index %u — check your PAGX composition links",
             d.contextIndex);
  }
}
```

**特别注意 306 `InvalidCompositionIndex`**：其 `contextIndex` 为字节流中**未校验的原始值**，可能 ≥ `doc.compositions.size()`——消费前必须先 range-check（`< size()`）再索引。

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
/**
 * ReadEnum: u8 + EnumMeta<T>::MaxValid 范围校验；越界 warn + 返回 Default。
 *
 * P0-3 v2.17（API）：`raw` 越界时作为 Diagnostic.contextIndex 透传给消费方，
 * 让用户日志看到 "enum #%u out of range"。Diagnostic ABI 表（§15.1）：
 * `InvalidEnumValue` 的 contextIndex 语义 = 越界的原始 u8 值（0..255）。
 */
template <typename T>
struct EnumMeta;  // 需要为每个 T 特化

template <typename T>
inline T ReadEnum(DecodeStream* stream, DecodeContext* ctx) {
  uint8_t raw = stream->readUint8();
  if (raw > EnumMeta<T>::MaxValid) {
    ctx->warn(ErrorCode::InvalidEnumValue,
              "enum value out of range for " + std::string(typeid(T).name()),
              raw /* contextIndex: 越界 u8 值 */);
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
template <> inline void WriteValue<BlendMode>(EncodeStream* s, BlendMode v, EncodeSession*) {
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
| StructureLimitExceeded (105) — Baker `BAKE_MAX_TOTAL_LAYERS`（P2-10 v2.16）| BakerEdgeCasesTest.cpp | BakerEdgeCases.BakeTotalLayerCount（PAGXBuilder 构造 > 1e5 层扁平树触发） |
| StructureLimitExceeded (105) — Baker `BAKE_MAX_TOTAL_VECTOR_ELEMENTS` | BakerEdgeCasesTest.cpp | BakerEdgeCases.BakeTotalVectorElementCount（PAGXBuilder 构造 > 1e6 VectorElement 触发） |
| StructureLimitExceeded (105) — Baker `BAKE_MAX_TOTAL_COMPOSITIONS` | BakerEdgeCasesTest.cpp | BakerEdgeCases.BakeTotalCompositionCount（> 1000 composition 触发） |
| ProducerFatal (106) | TruncatedDecodeTest.cpp | Truncate.BakerProducedErrorMarker（CorruptBuilder::AppendErrorMarkerTag 注入；P0-6 v2.18）|
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
| UnsupportedVersion (301) — future version reject（P2-10 v2.19） | VersionRejectTest.cpp | VersionReject.FutureVersionRejected（`CorruptBuilder::SetVersionByte(0x03)` 构造 version > KnownVersion，验证 Decoder 推 301 fatal + decode 失败；为未来 0x03 升级链路闭环）|
| UnsupportedCompression (302) | TruncatedDecodeTest.cpp | Truncate.UnsupportedCompression |
| TruncatedData (303) | TruncatedDecodeTest.cpp | Truncate.InHeader / InMidTag |
| MalformedTag (304) | TruncatedDecodeTest.cpp | Truncate.InMidTagPayload |
| BodyLengthOutOfRange (305) | TruncatedDecodeTest.cpp | Truncate.CorruptedLengthField |
| InvalidCompositionIndex (306) | TruncatedDecodeTest.cpp | Truncate.InvalidCompositionIndex |
| UnknownTagCode (400) | RoundTripTest.cpp | RoundTrip.UnknownTagForwardCompat |
| UnknownPropertyEncoding (401) | RoundTripTest.cpp | RoundTrip.UnknownEncodingForwardCompat |
| UnknownPropertyEncoding (401) — 规则 0 encoding vs isDefault 分叉（P2-08 v2.19） | PropertyEncodingTest.cpp | PropertyEncoding.EncodingWinsOverIsDefault（`CorruptBuilder::FlipByteAt(propHeaderOffset, 0x11)` 构造 isDefault=1 + encoding=未知，验证 Reader 走规则 1 整 Tag skip 而非规则 0(c) 按 isDefault 跳过 0 value 字节） |
| UnknownPropertyEncoding (401) — 规则 2 hasExt 1 byte skip（P2-08 v2.19） | PropertyEncodingTest.cpp | PropertyEncoding.HasExtOneByteSkip（构造 hasExt=1 + encoding=Constant，验证 Reader 先消费 1 byte extHeader 再读 value，stream 位置精确） |
| ImageResourceSizeExceeded (402) | TruncatedDecodeTest.cpp | Truncate.ImageOversize |
| FontResourceSizeExceeded (408) | TruncatedDecodeTest.cpp | Truncate.FontOversize |
| StringInvalidUtf8 (403) | TruncatedDecodeTest.cpp | Truncate.InvalidUtf8 |
| PathVerbLimitExceeded (404) | TruncatedDecodeTest.cpp | Truncate.PathTooManyVerbs |
| GlyphCountLimitExceeded (405) | TruncatedDecodeTest.cpp | Truncate.GlyphCountOverflow |
| LayerDepthLimitExceeded (406) | TruncatedDecodeTest.cpp | Truncate.LayerDepthOverflow |
| InvalidEnumValue (407) | RoundTripTest.cpp | RoundTrip.InvalidEnumValue |
| PrematureEndTag (409) — 中途 End Tag 截断（P0-15 v2.18）| TruncatedDecodeTest.cpp | Truncate.PrematureEndWithUnreadBody |
| FileReadFailed (307) | PAGLoaderTest.cpp | PAGLoader.LoadFromFile_Missing |
| InflateImageDecodeFailed (600) | InflaterParityTest.cpp | InflaterParity.CorruptImageAsset |
| InflateFontCreateFailed (601) | InflaterParityTest.cpp | InflaterParity.InvalidFontBytes |
| InflateGlyphRunBuildFailed (602) | InflaterParityTest.cpp | InflaterParity.CorruptGlyphRunBlob |
| InflateMaskResolveFailed (603) | InflaterParityTest.cpp | InflaterParity.MaskPathUnresolvable |
| InflateMaskResolveFailed (603) — MAX_PENDING_MASKS 熔断（P0-6 v2.18）| InflaterParityTest.cpp | InflaterParity.PendingMaskCapExceeded（StructBuilders::MakeLayerWithKMasks(262145) 构造）|
| InflateMaskCycle (607) — Layer mask 循环 A↔B↔A（P0-16 v2.18）| InflaterParityTest.cpp | InflaterParity.MaskCycleTwoLayers / InflaterParity.MaskSelfReferenceByteStream |
| InflaterEmptyDocument (604) | InflaterParityTest.cpp | InflaterParity.EmptyDocument |
| InflateCompositionCycle (605) | InflaterParityTest.cpp | InflaterParity.CompositionSelfRef / InflaterParity.CompositionRefTooDeep |
| InflaterLayerBudgetExceeded (606) | InflaterParityTest.cpp | InflaterParity.LayerBudgetExceeded |

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
constexpr uint32_t MAX_FONT_AXES        = 64;                      // P0-H v2.19：FontAsset.axes[] 上限（对齐 OpenType fvar 表惯例）——
                                                                    // 防 axisCount=0x0FFFFFFF → sizeof(FontAxis)*N ≈ 4GB OOM / heap overflow

// Layer/结构级
constexpr uint32_t MAX_LAYER_DEPTH      = 64;                      // composition + child 合计
constexpr uint32_t MAX_LAYERS_PER_COMPOSITION = 10000;
constexpr uint32_t MAX_COMPOSITIONS     = 1000;

// FileHeader / canvas 尺寸（P0-5 v2.15）：防 NaN/Inf/整型溢出
constexpr uint32_t MAX_CANVAS_DIM       = 16384;                   // 对齐 GPU 纹理硬上限（WebGL/iOS Metal 保守值）

// Path/Glyph
constexpr uint32_t MAX_PATH_VERBS           = 100000;
// P2-2 v2.18：MAX_PATH_COORDS / MAX_PATH_CONIC_WEIGHTS 为 MAX_PATH_VERBS 纯派生量——
// 移出 `limits::` 命名空间避免常量膨胀。调用点直接内联 `MAX_PATH_VERBS * 6` 或 `MAX_PATH_VERBS`。
// （6 是 Cubic 的 coord/verb 上限；魔数在 Path 解码语义下 self-explanatory）
constexpr uint32_t MAX_GLYPHS_PER_RUN       = 100000;

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

// 其他 count 上限（P1-A，见 §D.9 / §D.11）
constexpr uint32_t MAX_MASK_PATH_DEPTH              = 64;           // LayerMaskRef.pathDepth（layerPath 深度 ≤ MAX_LAYER_DEPTH）
constexpr uint32_t MAX_RANGE_SELECTORS_PER_MODIFIER = 16;           // ElementTextModifier.rangeSelectorCount
constexpr uint32_t MAX_RUNS_PER_TEXT                = 256;          // ElementText.runCount
constexpr uint32_t MAX_VECTOR_VALUE_ELEMENTS        = 1024;         // Property<vector<T>>.count（lineDashPattern / anchors / stopColors 等）

// Composition 引用图（P0-A1，见 §9.4）：防止自环/链式环/超深嵌套
constexpr uint32_t MAX_COMPOSITION_REF_DEPTH        = 32;           // Inflater 递归进入 composition 的最大深度（栈安全 + DoS 防护）

// Inflater 全局 Layer 预算（P1-3 v2.15）：防 Decoder 单节点合法但 N^6 累积膨胀
constexpr uint32_t MAX_INFLATED_LAYER_COUNT         = 1000000;      // Inflater 累计实例化 tgfx::Layer 的总数；超限该子树降级为空 Layer + warn 606

// P0-2 v2.17（安全）：pending 集合独立熔断。`reserveLayerBudget` 只管 Layer 实例总数，单 Layer 可挂
// N 个 mask；恶意构造 N×MAX_INFLATED_LAYER_COUNT 个 PendingMask 绕过 606。独立 cap 按 Layer 总数 ×
// 常数因子保守估：1e6 Layer × 平均 0.26 mask ≈ 262144 足够真实素材，又远低于 vector 分配爆表。
constexpr uint32_t MAX_PENDING_MASKS                = 262144;       // InflaterContext.pendingMasks 上限；超限 push 拒绝 + warn 603

// Baker 侧全局结构性累加上限（P0-4 v2.15）：PAGX XML 可控，需 Baker 独立累计熔断
// （Decoder 的 MAX_* 只管单节点/单 Tag；Baker 管 pre-pass 全树累积）
constexpr uint32_t BAKE_MAX_TOTAL_LAYERS            = 100000;       // 累计进入的 Layer 节点
constexpr uint32_t BAKE_MAX_TOTAL_VECTOR_ELEMENTS   = 1000000;      // 累计进入的 VectorElement
// P2-1 v2.18：BAKE_MAX_TOTAL_COMPOSITIONS 原为独立值 = 1000，与 MAX_COMPOSITIONS 重复命名 YAGNI。
// alias 化；未来 Decoder 松绑时改为独立值仍兼容调用点。
constexpr uint32_t BAKE_MAX_TOTAL_COMPOSITIONS      = MAX_COMPOSITIONS;

// Diagnostic 累计上限（防止恶意文件让 warnings vector 增长到数百 MB）
constexpr uint32_t MAX_DIAGNOSTICS              = 1000;              // Decoder/Baker/Inflater 共用
constexpr size_t   MAX_DIAGNOSTIC_MESSAGE_BYTES = 256;               // 单条 message 硬上限；warn/error 内部截断
}  // namespace pagx::pag::limits

// P2-4 v2.18：Path 编码格式选择阈值移出 limits::——这些不是"安全硬上限"而是"格式选择参数"，
// 放 limits:: 语义污染。独立 sub-namespace `pagx::pag::path_format::` 承载。
namespace pagx::pag::path_format {
constexpr uint32_t QUANTIZATION_MIN_VERBS    = 3;         // verbCount < 此值 → 回退 format=0
constexpr int64_t  QUANTIZATION_MAX_COORD    = (1LL << 23); // |coord| >= 此值 → 回退 format=0（int64 避免 P0-14 UB）
constexpr uint8_t  QUANTIZATION_DEFAULT_LOG2 = 4;         // 0.0625 px 精度
}

**触发条件**：
- Decoder 读取 `varU32 count` 后立即与对应 MAX 比较，超限 → push `Diagnostic{<对应 ErrorCode>}` + 返回 nullptr；
- Read 单个资源 size 超 MAX_IMAGE_BYTES/MAX_FONT_BYTES → warn `ImageResourceSizeExceeded` / `FontResourceSizeExceeded` + skip bytes + 跳过该资源（index 记为 UINT32_MAX）。

**整数溢出安全写法**（P1-B，**强制约束**）：所有"累加后与上限比较"的 DoS 记账**必须**写成减法形式，禁止直接加法（32-bit 平台 `size_t` 加法会回绕绕过上限）：

```cpp
// ❌ 错误：32-bit size_t 加法可能回绕到 < MAX 而绕过检查
if (totalAllocatedBytes + x > MAX_TOTAL_BODY_BYTES) return error;
totalAllocatedBytes += x;

// ✅ 正确：减法永不溢出（a - b ≥ 0 等价于 a ≥ b）
if (x > MAX_TOTAL_BODY_BYTES - totalAllocatedBytes) return error;
totalAllocatedBytes += x;
```

同理，所有 `stream.setPosition(base + length)` 形式的 seek 必须用 `uint64_t` 中间结果并前置校验 `base + length ≤ stream.size()`（P0-B，见 §D.3）。

### H.2 Baker 侧资源约束

**同 Decoder 的 H.1 上限在 Baker 侧也生效**，防止病态 PAGX（深嵌套、超量 element）拖垮 Baker 产物：

- **MAX_LAYER_DEPTH (64)**：Baker 深度遍历 PAGX Layer 树时，`BakeContext::currentLayerDepth` 累加；超限时立即推 `ErrorCode::CompositionCycleDepth` fatal，放弃产出（`doc=nullptr`）。
- **MAX_VECTOR_ELEMENT_DEPTH (64)**：VectorGroup 嵌套同理；共用 `BakeContext::currentVectorElementDepth`。
- **MAX_COMPOSITIONS / MAX_LAYERS_PER_COMPOSITION / MAX_CHILDREN_PER_LAYER / MAX_VECTOR_ELEMENTS_PER_PAYLOAD / MAX_FILTERS_PER_LAYER / MAX_STYLES_PER_LAYER / MAX_GRADIENT_STOPS / MAX_PATH_VERBS / MAX_GLYPHS_PER_RUN / MAX_IMAGES / MAX_FONTS / MAX_NAME_STRING_BYTES / MAX_TEXT_STRING_BYTES**：Baker 在向 PAGDocument 推入每批次数据前检查，超限推 fatal 错误 `ErrorCode::StructureLimitExceeded`（见 §G.2 枚举定义；message 附具体字段名如 `"MAX_VECTOR_ELEMENTS_PER_PAYLOAD exceeded (got 120000)"` 或 `"MAX_NAME_STRING_BYTES exceeded for Layer::name (got 128KB)"`）。Decoder 侧同样校验——见 §D.6 / §D.8 Read 伪码的 `count > MAX_*` 即时 return 分支。
- **MAX_IMAGE_BYTES / MAX_FONT_BYTES**：ResourceBaker 读图片/字体字节前检查；超限分别推 warning `ErrorCode::ImageResourceSizeExceeded` / `ErrorCode::FontResourceSizeExceeded`，该资源 index 记为 UINT32_MAX，引用点降级。

> **错误码语义划分**：
> - `StructureLimitExceeded` (105, Baker fatal) = **结构性尺寸/计数硬上限**超限——视为病态输入（MB 级 name、十万级 layer），放弃产出；
> - `StringInvalidUtf8` (403, Codec warning) = **UTF-8 字节序列非法**——Decoder 读到损坏字节，降级为空串继续；两者语义独立，不复用。

**实施点**：Baker 各子模块（LayerBaker/VectorBaker/ResourceBaker/TextBaker）在遍历入口调 `ctx.enterLayer()` / `ctx.enterVectorGroup()` / `ctx.registerComposition()`（见 §8.5 BakeContext 定义），每个都返回 `bool`——**纪律：返回值不得忽略**（P2-3 v2.16 纪律强化）：

```cpp
// ✅ 正确：enter 返回 false 即整子树直接 return
void LayerBaker::bakeLayer(const pagx::Layer* node, BakeContext* ctx) {
  if (!ctx->enterLayer()) return;     // 超深/超总数 → ctx 已推 fatal，直接退出
  // ... bake children ...
  ctx->exitLayer();                   // exitLayer 本身饱和保护防下溢（P1-2 v2.16）
}

// ❌ 错误：忽略返回值导致递归继续 + totalLayerCount 溢出
void LayerBaker::bakeLayer(...) {
  ctx->enterLayer();                  // 超限时 ctx 已推 fatal 但调用侧仍继续递归
  // ... children ...
  ctx->exitLayer();
}
```

标量上限（`MAX_NAME_STRING_BYTES` / `MAX_TEXT_STRING_BYTES` / `MAX_GRADIENT_STOPS` 等）不走 enter* helper，各调用点就地 `if (size > limits::MAX_X) ctx->error(ErrorCode::StructureLimitExceeded, ...)` + return。没有 `ctx.checkLimit(...)` 统一接口——刻意不聚合以保持每个字段的上限检查在调用点直接可见。

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

### H.5 字体魔数校验（P0-17 v2.18）

`FontAsset::data` 的前 4 字节做 magic 白名单检查——对齐图片侧（H.4）。TTF/OTF 解析器（FreeType/CoreText）历史 CVE 较多（OOB read / UAF），magic 白名单是第一道防护：

| 格式 | magic |
|---|---|
| TTF | `00 01 00 00` 或 `74 72 75 65` (`true`) |
| OTF | `4F 54 54 4F` (`OTTO`) |
| TTC（TrueType Collection） | `74 74 63 66` (`ttcf`) |
| WOFF | `77 4F 46 46` (`wOFF`) |
| WOFF2 | `77 4F 46 32` (`wOF2`) |

Decoder 读入 FontAsset.data 后立即 magic 检查；不在白名单内 → warn `FontSourceMissing=201` + 丢弃 bytes + 降级为 System（family / style 仍可走本地字体查询）。System 字体的 data 始终 nullptr，不触发本检查。

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

**`DecodeStream::currentReadablePtr()` Phase 1 交付**（P2-05 v2.19）：§D.1 / §18.4quinta `ZeroCopyScope` 测试依赖 `currentReadablePtr()` 返回当前 `data_ + position_`。v1 `DecodeStream` 未提供此 API——Phase 1 通过 **非侵入 wrapper** `src/pagx/pag/DecodeStreamExt.h` 定义 inline 自由函数 `const uint8_t* CurrentReadablePtr(pag::DecodeStream& s)`（实现走 v1 DecodeStream public API `position()` + `data()` 组合）；或若 v1 同批扩 API 则直接走成员函数。`DecodeStream.currentReadablePtr()` 在 §18.4quinta / §D.1 Zero-copy 代码片段中是**简写**，实际实现为 `CurrentReadablePtr(stream)`。Phase 1 exit gate：ValueCodec / DecodeStreamExt 齐备即可供 §18.4quinta ZeroCopyScope 三条测试编译通过。

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

# Fuzz target（P1-12 v2.15；§18.3ter Phase 12 出口条件）
# libFuzzer 仅 clang 可用——非 clang 构建时静默不生成 target，CI 在 clang 容器里跑。
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_executable(PAGDecodeFuzz test/src/pag/fuzz/decode_fuzz.cpp)
  target_link_libraries(PAGDecodeFuzz PRIVATE pagx-pag)
  target_compile_options(PAGDecodeFuzz PRIVATE
    -g -O1
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(PAGDecodeFuzz PRIVATE
    -fsanitize=fuzzer,address,undefined)
  # seed corpus：Phase 12 前用 CorruptBuilder 生成 + spec/samples 合法字节
  # 目录约定：test/fuzz_corpus/{valid,corrupt}/*.pag
endif()
```

> **tgfx 暴露等级**：由于 `include/pagx/PAGLoader.h` 在头文件签名里暴露 `std::shared_ptr<tgfx::Layer>`，依赖 pagx-pag 的目标**传递**地获得 tgfx 的头可见性（CMake 中用 `PUBLIC tgfx`）。仅使用 `PAGExporter.h` / `Diagnostic.h` 的消费方虽然编译时会拿到 tgfx 头路径，但不会在自己的代码里出现 tgfx 符号——这是"在头文件层面隔离依赖"可以达到的最强保证；若要彻底隔离 tgfx 路径污染，需要把 PAGLoader 拆成独立静态库 `pagx-pag-loader`，**本期不做**（过度设计，用户量极小）。

> **tgfx ABI 模型**：pagx 对 tgfx 采用 **SAME-BUILD ABI** 模型——pagx-pag 与 tgfx 必须在**同一次编译中一同构建并链接**，不承诺与其他 tgfx 预编译版本二进制兼容。原因：`PAGLoader::Result::layer` 的 `tgfx::Layer` 类型随 tgfx 版本可能变更 vtable / 虚函数签名 / 成员布局，跨版本混链会产生未定义行为。`DiagnosticCode` 的 append-only ABI 纪律（§15.1）仅覆盖 pagx 自身接口，**不** cascades 到 tgfx 符号。升级 tgfx 时必须重新构建 pagx-pag。

### I.5 tgfx 枚举预检清单（P1-I5，Phase 0 开工前 30 分钟执行）

§C.1 的 `using T = tgfx::T;` 依赖下列 tgfx 头导出**全部存在**，否则 Phase 2 `PAGDocument.h` 编译即挂。开工前用 `grep` 一次性确认：

```bash
# 本地 tgfx 仓库（与 libpag 同级）
TGFX=../tgfx
for sym in PolystarType MergePathOp TrimPathType RepeaterOrder \
           SelectorUnit SelectorShape SelectorMode \
           LayerMaskType LineCap LineJoin StrokeAlign LayerPlacement \
           TileMode FilterMode MipmapMode FillRule SamplingOptions \
           BlendMode Color Point Rect Matrix Matrix3D Path; do
  if ! grep -rqE "(enum class|struct|class)\s+$sym\b" "$TGFX/include/"; then
    echo "MISSING: tgfx::$sym"
  fi
done
```

**预期输出**：空（全部存在）。若有 `MISSING` 行，Phase 0 前必须：
- (a) 在 tgfx 上游补上缺失类型，或
- (b) 在 `src/pagx/pag/TgfxEnumCompat.h` 提供 shim 枚举 + `ToTGFX(shim)` 空转换。

**为何 Phase 0**：阶段 0 的 Diagnostic.h 虽然不依赖 tgfx 枚举，但阶段 2 的 `PAGDocument.h` 全面铺开 `using` alias，此时再发现缺失会阻塞整个编码链。开工前 30 秒的 grep 脚本是 cheapest 保险。

### I.6 预编译 tgfx 的 `ToTGFX.h` 复用纪律

`src/renderer/ToTGFX.h` 对 `BlendMode` / `Color` / `pagx::TextAlign → tgfx` 等提供了纯函数转换（libpag 主干既有），Baker / Inflater **直接 include 复用**，不在 `pagx::pag` 命名空间内另写。依赖方向 `pagx::pag → renderer`，单向、无循环。PAGX 扩展新 ColorSpace 或新枚举值时，先更新 `ToTGFX`，再更新本文档附录 E 映射表。

---

## 文档维护

- 本文档所有行号引用基于 `src/renderer/LayerBuilder.cpp` 当前实现；LayerBuilder 修改时需同步更新本文档附录 A。
- PAGX 规范版本：`spec/pagx_spec.zh_CN.md`。
- PAG v2 版本：0x02（本期）；动画扩展不升版本号。

### 上次开工必读（开工前 AI / 人类 follow-up 的精简摘要）

v2.0 → v2.17 累计 8 轮专家评审，~85 个 P0/P1/P2 修复。**开工前只需记下这些硬约束**，其他版本日志作"为什么这样设计"的补充阅读：

1. **ABI 红线**：`DiagnosticCode` 数值只能段内追加，不得复用 / 跨段迁移；`propHeader.hasExt=1` 的 extHeader 尺寸**永久锁 1 byte**（§4.3）——扩展走新 encoding 值（§4.4 规则 1）或升 FORMAT_VERSION。
2. **签名纪律**：`LayerInflater::Inflate(std::unique_ptr<PAGDocument> doc)` 取所有权（§9.1）；所有 Read/Write 函数接 `Context*`（§8.4）；所有 `stream.setPosition(base + length)` 形式的 seek 用 `uint64_t` 中间结果（§D.3）；所有 DoS 累加写 `if (x > MAX - total)` 减法形式（§H.1）。
3. **安全前置**：所有 varU32-prefixed utf8string / bytes 走 `ValueCodec.h` 的 `ReadUtf8String` / `ReadLengthPrefixedBytes` wrapper（§D.1）；所有 `readInt32List` / `readFloatList` / `readUBits` 走 `ReadInt32ListSafe` 确保 `numBits ≤ 32`；`readEncodedUint32` 第 5 字节带 continuation bit 即 warn + 返回 0。`ReadLengthPrefixedBytes` 默认走 **owning** `MakeAdopted`——零拷贝 `MakeWithoutCopy` 必须在显式 `ZeroCopyScope::DecodeLocal` 内才允许，防 tgfx::Image 异步持有造成 UAF（P0-1 v2.17）。
4. **循环防护**：`CurrentStreamScope` RAII guard 管 SubStream 嵌套（§8.5）；`CompositionVisitScope` RAII guard 管 Composition 引用图 + `MAX_COMPOSITION_REF_DEPTH=32`（§9.4）；Inflater `reserveLayerBudget(layerIndex)` 管 `MAX_INFLATED_LAYER_COUNT=1e6`；`reservePendingMaskSlot()` 管 `MAX_PENDING_MASKS=262144`（P0-2 v2.17，防止单 Layer 挂 N 个 mask 绕过 606）。
5. **资源零拷贝**：`ImageAsset::data` / `FontAsset::data` 都是 `shared_ptr<const tgfx::Data>`（§11.1 / §11.2）；Inflater `MakeFromEncoded` / `MakeFromBytes` 成功后立即 `data.reset()`；`PAGLoaderTest.{Image,Font}BytesReleasedAfterInflate` 断言 `use_count==1`。
6. **Baker 独立上限**：PAGX XML 可控，BakeContext 有独立 `totalLayerCount` / `totalVectorElementCount` / `totalCompositionCount` 累计熔断（§8.5 + §H.2），不依赖 Decoder 的 MAX_*。
7. **Diagnostic 结构**：`{code, message, byteOffset, contextIndex}`——`message` 不稳定禁 switch；`contextIndex` 是 ABI 的（资源/layer/composition 索引，见 §15.1 code→contextIndex 表）；消费 contextIndex 前**必须**检查 `!= UINT32_MAX` 守卫（P1-2 v2.17）。
8. **测试前置工具**：Phase 1 交付 `CorruptBuilder`（字节级攻击注入）+ Phase 2 交付 `PAGXBuilder`（结构级 DOM 构造，13 条 smoke）+ Phase 2 交付 `StructBuilders.h`（`MakeDeepLayerStack` 等结构 helper）；`CapturePagxLayout(bytes)` 在 `PAGX_DEBUG_OFFSETS + PAG_BUILD_TESTS` 双条件 debug build 下提供 offset 定位；所有 regression test 建立在这四件套上。
9. **双入口分层**：`PAGExporter.h` 不依赖 tgfx 渲染层（可传递 `tgfx::Data` 等核心数据类型）；`PAGLoader.h` 是唯一允许 `tgfx::Layer` 暴露的对外头；SAME-BUILD ABI 模型——升级 tgfx 必须重建 pagx-pag。
10. **Phase 出口条件**：§19 阶段表严格按 Tag/功能拆错误码覆盖范围——Phase 4 只覆盖 300/301/302/303/304/305/306（Codec fatal 全段），其他推至对应 Phase（307→10.5，Path/Resource/Text 400 段→5/8，Inflater 600 段→9）；Phase 1 exit gate: `grep -rn "std::vector<uint8_t>" src/pagx/pag/` 在 ValueCodec/Codec 上下文零命中；Phase 2 exit: 13 条 PAGXBuilderTest；`PAGDecodeFuzz` 目标在 Phase 12 ≥1 CPU·小时 ASAN/UBSAN 全绿 + `.github/workflows/pagx-fuzz.yml` 交付。
11. **enter/exit 纪律**（P1-2 v2.16 / P1-10 v2.17）：`BakeContext::enterLayer/enterVectorGroup` 返回 `bool`——false 即**整个子树 return，不配对 exit**；`exitLayer/exitVectorGroup` 内置饱和保护 `if (depth>0) --depth` 防误配对下溢把 MAX_DIAGNOSTICS=1000 塞满。
12. **contextIndex 强制传参**（P0-1 v2.16 / P0-3 v2.17）：3 Context（v2.19 合并后）的 `warn(code, msg, contextIndex=UINT32_MAX)` / `error(...)` 三参签名——**16+ 个码**在 §15.1 ABI 表有结构语义（ImageSourceMissing/InflateCompositionCycle/InflaterLayerBudgetExceeded/InflateMaskCycle 等），调用点**必须**显式传入；`DiagnosticTest.FormatDiagnostic.WithContextIndex` 断言 `#ctx=N` 后缀输出。`ctx->warn` 3-arg 示例见 §11.1 Baker / §12.2 Pass 2 / §G.5 ReadEnum / §9.4 CompositionVisitScope。
13. **架构基线 v2.19**（P0-C + P0-D + P0-R1 + P0-R2 钉死硬约束）：(a) **`DiagnosticCollector` 基类只暴露 protected `pushError/pushWarning` helper**，3 Context 各自 3 参 public `error/warn` wrapper（C++ 名称查找永远落子类，无 name hiding 歧义）；InflaterContext **不 override `pushError`**（物理屏蔽 "无 fatal"）。(b) **`struct EncodeSession { DiagnosticCollector* diag; StreamContext* sc; };`** 2 指针聚合体——Encode 阶段所有 `Write<TagName>` 签名第三参为 `EncodeSession*`；§16 目录**不产出 `EncodeContext.h`**。(c) **ImageAssetTable / FontAssetTable 每 asset 独立 sub-Tag**（ImageAsset=6 / FontAsset=7）——字段级追加走 sub-Tag 内 length skip，v2.1+ 激活不炸 v2.0 Reader。(d) **`LayerTransform=15` 本期就落**——LayerBlock body 顶层仅留身份/时间轴/layerFlags，visible/alpha/blendMode/matrix/matrix3D/scrollRect 挪入 LayerTransform sub-Tag；动画期 §4.4 规则 1 "未知 encoding 收窄到 transform skip" 立即可用。
14. **3 个安全校验新规 v2.19**：(a) `ImageAsset::kind` Decoder 强制 `kind == 0`（Raster）——kind != 0 但 <= 3 → warn `UnsupportedFeature=104` + 回退 Raster，kind > 3 → warn 304 整 Tag skip；**严禁** Inflater 基于字节流 kind 分派解码器（防未来加 Video/Hdr 解码器时字节级攻击面扩散）。(b) `FontAsset.axes[]` 读 `axisCount` 后立即校验 `≤ MAX_FONT_AXES = 64`（对齐 OpenType fvar 上限）——超限推 105 fatal 防 4GB OOM。(c) DiagnosticCode 总数 **43 码**（v2.18 的 40 + v2.19 补 `BlendModeUnmapped=204` / `PrematureEndTag=409` / `InflateMaskCycle=607`）；`std::array<DiagnosticCode, 43> kAllDiagnosticCodes` + `CodeToString` switch 必须同步。
15. **阅读链** / **AnimationData 动画期准备**：开工阅读顺序 §3.2 → §22 14 条 → §3.3 术语索引（按需）；动画期开工**前**必读 §D.14 AnimationData sub-Tag 草案（本期不产出，但字段布局 + 挂载点 + propertyId 枚举 + keyframe 模板已钉死，避免届时设计返工）。

历史完整修订记录（v2.0 → v2.18）见下文。**实现阶段不需通读历史**，读完上述 15 条（12 基线 + 3 v2.19 新增）即可开工。

### 历史修订记录

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
