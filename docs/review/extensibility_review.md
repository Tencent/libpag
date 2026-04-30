# 可扩展性评审报告

**评审员角色**：协议演进专家  
**文档版本**：v2.2（v2.19）  
**评审日期**：2026-04-30  
**评审周期**：深度架构评估

---

## 一、整体判断

- **整体评分**：★★★★☆（4/5）
- **核心结论**：**(B) 扩展设计合理，但存在 3-4 处可精简的过度超前设计**
- **对比业界参照**：
  - **PNG chunk**：无版本化，纯 length skip（最简）
  - **Protobuf**：reserved 字段 + unknown field skip（中等）
  - **MessagePack ext**：2 层版本化（1 byte 类型 + payload 长度）
  - **本方案**：3 层 + 多个"为未来预留"的红线约束（较复杂）

**总体观点**：方案既不欠缺也不明显过度，但**三层并存的场景区分不够清晰**，某些位预留（bit 6-7、hasExt）的约束过度严苛（为防止「将来某人犯错」而冻结永久红线）。建议砍掉 1-2 层，换来更轻便的演进心智模型。

---

## 二、三层向前兼容机制评估

### 表格评分

| 层次 | 必要性 | 用例清晰度 | 是否重叠 | 建议 |
|-----|-------|-----------|--------|-----|
| ① 字段追加 | ✅ 极高 | ✅ 明确 | — | **保留** |
| ② Tag 版本化 | ⚠️ 中等 | ⚠️ 需补充 | ⚠️ 与③关系模糊 | **精简**（仅保留语义变更路径） |
| ③ FORMAT_VERSION | ✅ 必须 | ❌ 模糊 | ⚠️ 触发条件与②混淆 | **澄清触发条件** |

### 详细分析

#### ① 字段追加（日常首选）

**必要性评分**：★★★★★  
**理由**：
- 每个 Tag 的 `length` 明确界定边界，是协议自洽的根基
- 旧 Reader `seek(tagEnd)` 自动跳过新字段，零破坏性
- 大多数演进场景都走这条路：新增 flag、新增 Property、新增 enum 值

**现状**：
- 方案已明确定义（§6.5 ①，第 412-416 行）
- 给出了 `layerFlags` 预留 bit 的具体示例

**是否过度**：否。这是最轻量的兼容机制，应该是首选。

**建议**：保留，无需改进。

---

#### ② Tag 版本化（「为字段不兼容变更预留」）

**必要性评分**：★★★☆☆  
**用例清晰度**：⚠️ 需补充  
**理由与问题**：

1. **触发场景不明确**（文档第 418-425 行）：
   - 定义："字段语义或字节格式不兼容变更"
   - 但什么算"语义不兼容"？文档给的例子太少
   
   **问题**：是否包括：
   - 某个 flag 的含义变了？
   - 枚举值重新编号？
   - 字段尺寸升级（u8 → u16）？
   - 新字段插入中间（而非末尾）？
   
   文档没有清晰的决策树。

2. **与③ FORMAT_VERSION 的边界模糊**（第 428-436 行）：
   - 必选 top-level Tag（FileHeader/ImageAssetTable/CompositionList/Composition/LayerBlock 六个）的不兼容变更禁用②，必须用③
   - 其他"功能性 Tag"可用②
   
   **问题**：为何这个划分？是不是说：
   - 必选 Tag 变更 = 整文件不可播 = 必须让旧 Reader graceful reject = 升 FORMAT_VERSION
   - 非必选 Tag 变更 = 至多丢一个 Tag = 可以旧 Reader skip = 用②
   
   如果是这个逻辑，那③的触发条件应该是**"这个 Tag 是整文件渲染的必要组件"**，而不是"容器头或 Tag 封装机制本身"（第 434 行说法过宽）。

3. **v1 对照与命名规范**（第 423 行）：
   - 参考 v1 `LayerAttributes` / `LayerAttributesV2` / `LayerAttributesV3`（3 个版本共存）
   - 但本文档没有规定：新增版本化 Tag 时，是否必须保留旧 Tag？保留多久？
   - "双写优先级规约"（第 426 行）说 Writer 必须同时写旧 Tag 作 fallback，但这增加了文件体积和编码复杂度

4. **TagCode 空间的代价**（第 364-378 行 tag 分段表）：
   - 版本化变体段占 300-599（300 个 slot）
   - 已用 0 个，但"为未来版本化变体预留"
   - 如果大量 Tag 走版本化路线，这会迅速耗尽 300-599 段
   
   **问题**：本期仅静态，未来动画/3D 可能新增 VectorElement。若某个元素类型需要版本化（如 Path 格式变更），是占用版本化段还是继续用新 Tag Code？文档没说。

**对比业界**：
- **Protobuf**：字段有 reserved ranges，新增字段追加到 oneof，不需要版本化 Tag
- **PNG**：无版本化，扩展走新 chunk type
- **MessagePack ext**：固定类型 ID + 负载长度，无版本化 Tag

**是否过度**：⚠️ 可能过度。本期完全静态，**未来 5 年内真的会遇到字段语义不兼容变更吗**？
- 动画期加 keyframe 数据？走新 Tag（AnimationData）就够，不需要版本化旧 Property（属于方案 ③，见第 277-280 行）
- 新增颜色模式（RGB → HDR）？走新 Tag（ShapeStyleHDR）+双写，不修改旧 ShapeStyleData
- glyph 数据格式升级？可能是本期新增需求时就预留字段，不是后续版本化

**建议**：
- ✅ **保留此层**，但**改名为"Tag 版本化（有限使用）"**
- 新增补充文档说明触发场景：
  - ✅ 应该用②的：某个 Payload Tag 的字段需要重新编号、某个 Property 编码方式完全不同（但 encoding 位足够时，先用新 encoding 值）
  - ❌ 不应该用②的：直接新增字段（用①）、新增 Tag（用新 TagCode）、必选 top-level Tag 变更（用③）
- 钉死"双写保留期"：新版本 Writer 必须保留旧版本 fallback，至少 N 个小版本（建议 3 年或 10 个 minor 版本）；废弃前发公告

---

#### ③ FORMAT_VERSION 升级（「容器或封装机制变更」）

**必要性评分**：★★★★☆  
**用例清晰度**：❌ 模糊  
**理由与问题**：

1. **触发条件定义过宽**（第 434 行）：
   > "升 FORMAT_VERSION 当且仅当**容器头或 Tag 封装机制本身**不兼容变更（如 magic 改变、TagHeader 位宽变化、新增必选 top-level 段落）"
   
   关键词"新增必选 top-level 段落"是什么意思？
   - 是指容器头 magic 之后新增字段？
   - 还是 body 的 top-level Tag 列表新增强制项？
   
   第 429-430 行给了个例子（FileHeader 尾追加 featureFlags）但这明明走①字段追加，不需要升版本号。

2. **与"必选 top-level Tag 排除清单"逻辑混淆**（第 428-431 行）：
   - 六个必选 Tag 的**字段语义不兼容变更**禁止走②，**必须升③**
   - 但前面说③是"容器或封装机制"变更？
   - 这里变成了"任何必选 Tag 的语义变更"都要升③？
   
   **问题**：FileHeader 新增必选字段（如 canvas 变成 f64）是属于：
   - ① 字段追加（旧 Reader seek 跳过）？→ 但新 Reader 读 f64 时旧 Canvas 尺寸丢失，渲染错 decoding
   - ② Tag 版本化（新 FileHeaderV2）？→ 但 FileHeader 是必选，旧 Reader skip 了整个头
   - ③ 升 FORMAT_VERSION？→ 这才是正确答案，因为必选字段变更 = 整文件不可播
   
   **隐含规则**：其实③的真正触发是**"这个变更会让旧 Reader 要么崩溃、要么产出错误结果，而不是优雅降级"**，而不是"容器"。

3. **"不升版本号"承诺的兑现**（第 435 行）：
   > "本期**动画扩展明确不升版本号**（靠 Property 壳的 `encoding` 位段 + Tag 级版本化即可）"
   
   这是承诺，但有风险：
   - 若动画期发现 Property 壳的结构必须变更（如 keyframe 数据的格式完全不同于现在的 encoding 预留位），是否还能坚守"不升"？
   - 若硬要不升，就得继续堆砌 Tag 级版本化，最终版本化段耗尽 → 被迫升③
   
   **建议**：在 v2.0 发布前，团队需要共识"若 5 年内动画扩展必须升版本号，是否接受"。现在承诺"不升"可能 5 年后被打脸。

4. **升级频率预估缺失**（第 437 行流程图下方）：
   - 文档没说"FORMAT_VERSION 升级周期预估"
   - PNG：v1 → v1.2 跨度 30 年无版本化升级
   - TIFF：v42 版本（因为允许任意扩展）
   - 本方案：预计多久升一次？
   
   根据可扩展性设计，本应 5-10 年不升版本号（靠①②撑住）。但若动画期必须升，预期可能 2-3 年升一次。

**对比业界**：
- **PNG**：IHDR/PLTE/IDAT 固定，扩展走 ancillary chunk（不升版本）
- **ZIP**：支持多版本（central directory version），但极少升级
- **TIFF**：version 字段存在但名存实亡（无人升级）

**是否过度**：❌ 不是过度，而是**定义模糊导致"升不升"判断困难**。

**建议**：
- 明确重写③的触发条件为：**"旧 Reader 按现有规则解析会产出错误结果，而非优雅降级"**
  - 例：必选字段被删除 / 必选 Tag 被删除 / TagHeader 位宽变化 / compression 增加新值
  - 例：不需要升③：新增 Tag / 新增字段 / 新增 encoding 值 / 新 Reader 向下兼容 v1 格式
- 补充"升级决策树"：每个③升级前必须 RFC，团队投票通过（避免临时性升级）
- 预测升级周期：基于现有设计推估"本期 v2.0 至少 5 年无需升版本号"

---

### 三层的"机械叠加"vs"有意义的分层"判断

**问题**：三层是否有真实的场景区分，还是为了"看起来完整"？

**答**：**有意义但可精简为两层**。

核心是这个演进链：
```
新需求出现
├─ 只加字段？ → ① 字段追加 ✅
├─ 字段不兼容变更 ──→ ② Tag 版本化 ⚠️（可选）或直接新 Tag + 双写 ✅
└─ 必选组件破坏性变更 ──→ ③ FORMAT_VERSION 升级 ✅
```

**问题**：②是否真的必须？
- 若允许"新 Tag + 双写 fallback"替代②，方案变为**两层**：
  - ① 字段追加
  - ③ 格式根本性变更（容器头、必选结构）
- ② 只在"改现有 Tag 而非新增 Tag"的场景有价值，但实际项目中新增 Tag 往往更干净

**真实场景测试**：
- 新增 LayerType（如 3D Layer）→ 新 Payload Tag（20-39 预留 13 个）✅ 不需要②
- 新增 ShapeStyle 类型 → 新 ElementShape 或 ShapeStyleHDR Tag ✅ 不需要②
- TextLayer 加新字段（如 stroke-dash-pattern） → ① 追加 ✅ 不需要②
- Path 格式升级（新编码） → Property encoding 新值（5-15 预留 11 个）✅ 不需要②
- 某个字段的类型升级（u8 → u16） → 新 Tag + 双写 ✅ 不需要②
- 整个 Tag 废弃 → Writer 停写，Reader 容错 skip ✅ 不需要②

**结论**：②的核心价值是"避免新增 TagCode"，但 TagCode 空间充足（多数段 50%+ 预留）。从"最小化概念复杂度"角度，②可以删除或标记为"罕用"。

---

## 三、字段演进路径清晰度矩阵

| 演进场景 | 方案路径清晰度 | 对应条款 | 备注 |
|---------|-------------|--------|-----|
| 新增 LayerType（3D Layer） | ✅ 清晰 | §6.1 / Payload 段 20-39 | 新 Payload Tag（预留 13 个） |
| 新增 ShapeStyle（Pattern 以外） | ✅ 清晰 | §6.1 / VectorElement 段 40-119 | 新 ColorSource 或新 Style Tag（预留充足） |
| 给 TextLayer 加字段（stroke-dash） | ✅ 清晰 | §4.3 + §6.5① | ElementText Tag 尾追加（通过 length skip） |
| 字段类型升级（u8 → u16） | ✅ 清晰 | §6.5② | 新 Tag（如 LayerAttributesV2）+ 双写旧 Tag |
| 字段语义变化（flag 含义改变） | ⚠️ 模糊 | §6.5②/③ | 不清楚该用②还是③；建议：新 flag bit + 新 encoding 值更干净 |
| 整个 Tag 废弃 | ✅ 清晰 | §4.4 规则 1-3 | Writer 停写，Reader 按 UnknownTagCode 处理 |

**总体**：框架清晰，但演进新增字段时的"何时新 Tag、何时追加、何时版本化"判断树不够明确。

**建议补充**：在 §6.5 下新增决策树伪码：
```
新需求来临
├─ "我想在现有 Tag 里加新字段" → ① 字段追加（末尾追加）
├─ "我想改现有字段的语义" → 评估②vs新 Tag
│  ├─ 若改动会导致旧 Reader 解析错 → 新 Tag + 双写
│  └─ 若只改语义但值域不变 → 考虑新 flag 位或新 encoding 值
└─ "我想删除某个字段或改变容器头" → ③ 升 FORMAT_VERSION
```

---

## 四、过度超前设计清单

### E-1：propHeader 的 hasExt 位与 extHeader 的"永久 1 字节冻结"

**超前判断**：**过度超前 / 防守过度**

**规格描述**（第 225, 228-233 行）：
- `hasExt = 1` 表示后续有 extHeader 承载未来能力位
- **永久红线**：extHeader 尺寸永久固定为 1 byte，不得扩展为 2+ byte
- 理由：旧 Reader readUint8() 消费 1 byte；若新 Reader 写 2 byte，旧 Reader 只读 1 byte，后续字节被当 value 解析 → 整 Tag 错位

**问题评估**：

1. **错位风险的严重程度**（第 228 行说法）：
   - "整个 Tag 被错位读入（远比规则 1 的"至多损失一个 Tag"严重）"
   - 但能给具体例子吗？假设 hasExt=1 的 extHeader 尺寸从 1→2：
     ```
     [propHeader][extHeader 2 byte][value...]
     旧 Reader：[propHeader][1 byte as extHeader][1 byte of value 被当 value 解析]
     → 后续 value 字节错位
     ```
   - 这确实严重。但**这是设计时就应该预见的**，而非"为未来保险"。

2. **为何必须"永久"冻结**？
   - 因为 hasExt 位语义已经在 v2.0 发布后被旧 Reader 刻在二进制里
   - 若未来想扩 extHeader，唯一办法是**升 FORMAT_VERSION**（第 231 行认可）
   - 所以不是"为未来"，而是"v2.0 发布后无法再改"

3. **但这暴露了设计的提前性**：
   - hasExt 位在本期设定为 0（第 235 行"本期约束"）
   - **本期完全未使用**，没有真实用例
   - 为什么要在 v2.0 发布时就"永久冻结"一个未使用的扩展点？
   - 答案是：**防止未来某个维护者犯错**
   
   但这意味着：
   - 若 5 年后动画期真的需要 hasExt，已经无法扩展 extHeader
   - 只能升 FORMAT_VERSION（打脸"不升版本号"承诺）或用新 encoding 值
   - **用新 encoding 值替代 hasExt 本来就是可行方案**（第 226 行"bit 0-3 的 5-15 槽位"）

**对比业界**：
- **Protobuf**：field tag 留有高位预留（field 1 到 19 为保留），但不"永久冻结"——若需要更多 field，简单地用更高的 field ID
- **PNG**：ancillary chunk 的 5 个 bit flags，每个 bit 都定义了未来含义，**但没有"永久冻结"的约束**——未来 PNG v2 可以重新定义
- **MessagePack**：ext 格式固定，无扩展机制（设计阶段就穷尽了）

**是否过度**：✅ **过度超前**

**理由**：
- 本期 hasExt 完全未使用
- 三层兼容机制中②或③完全可以承载未来的 extHeader 扩展需求
- 为一个"防止将来人犯错"的假设而永久冻结设计，属于防守过度
- 真实风险：若 5 年后发现 hasExt 必须扩展，团队被迫升 FORMAT_VERSION

**建议**：
- **删除 hasExt 位**（v2 尚未发布，无兼容性代价）
- 改为：**所有 Property 扩展走新 encoding 值**（bit 0-3 的 5-15 槽，足够 11 个新编码）
- 或：**保留 hasExt 但改为"非冻结"**，明确说明"若未来需要扩 extHeader，走③ FORMAT_VERSION 升级"（这样设计者知道是预期的扩展点，而非禁忌）

---

### E-2：Tag 版本化机制的"必须双写"规约

**超前判断**：**合理超前，但执行成本高**

**规格描述**（第 426 行）：
- Writer 使用②时**必须**同时写旧 TagCode 作为 fallback
- 新 Reader 遇两者并存时以后出现为准（LIFO override）
- 旧 Reader 按 UnknownTagCode skip 新 Tag，仅读旧 Tag

**问题评估**：

1. **实现复杂度**：
   - 每次引入新 TagCode，Writer 要同时生成两份数据
   - 例：新增 `ShapeStyleHDR` 同时写旧 `ShapeStyleData`
   - 文件体积可能增加 30-50%（对性能敏感的场景问题）

2. **Reader 端复杂度**：
   - 需要同时 Read 新旧两个 Tag，然后 LIFO override
   - 若字段结构很不一样（v1 `LayerAttributes` → `LayerAttributesV3` 的情况），merge 逻辑复杂

3. **真实使用频率**：
   - v1 中 `LayerAttributesV2` 和 `V3` 因何引入？
   - 答：v1 发布后需求变化导致字段需要改变
   - **本期能预测哪些字段会需要版本化吗？** 
   - 文档没给出任何数据

4. **与①字段追加的关系**：
   - 如果本期设计充分考虑预留位，大多数变更可走①
   - 例：`layerFlags` 预留 bit 5-7（第 416 行），未来 flag 增加就不需要版本化
   - 这说明**提前做好预留比版本化更便宜**

**是否过度**：⚠️ **可能过度，但"必须双写"是合理的保守设计**

**理由**：
- 双写保证了旧 Reader 不丢失字段（最坏情况降级，不崩溃）
- 但成本是文件体积和编码复杂度
- 若②路径在实际中极少使用（大多变更走①or新 Tag），那"双写必须"就是维护负担

**建议**：
- 保留②，但明确标记为"罕用路径"
- 补充 RFC 流程：使用②前必须评估"能否改用①预留位"或"能否用新 Tag"
- 钉死双写保留期：新版本 Writer 最少保留 3 年双写，之后可停写旧 Tag

---

### E-3：propHeader bit 6-7 reserved 的"最终逃生门"设计

**超前判断**：**防守过度**

**规格描述**（第 226 行）：
- bit 6-7 reserved，Writer 必须写 0，Reader 必须忽略
- **"不设未来扩展通道"**——bit 6=1 意味 Reader 再读 1 byte，与 hasExt 同构陷阱
- 这两位是"最终逃生门，仅用于 Reader 兼容性，不承载未来语义"

**问题评估**：

1. **与 hasExt 的"同构陷阱"论**（第 226 行）：
   - bit 6=1 如果意味"再读 1 byte"，确实会导致旧 Reader 错位
   - 但这假设"某个未来维护者会在不升 FORMAT_VERSION 的情况下改变 bit 6 的语义"
   - **这是一个假设的错误，而非真实设计需求**

2. **预防措施的代价**：
   - 永久占据 2 个 bit，但声明"不用"
   - 浪费了潜在的 4 个信号位（bit 6-7 各自可以是 0/1）
   - 若未来真的需要新能力位，被迫升 FORMAT_VERSION 或用新 encoding 值

3. **vs Protobuf 的对比**：
   - Protobuf 的 field tag wire type (bit 0-2) 有 reserved 值 3 和 4（never used）
   - 不是"冻结"，而是"未定义值 → error"
   - 若未来需要新 wire type，增加代码即可，无需预留

4. **历史例子**：
   - IPv4 header flag bit（3 bit）：Reserved、DF、MF
   - DF 和 MF 后来改为 ECN 相关，并且成功扩展（通过新协议版本）
   - 从未因"冻结 Reserved bit"而导致协议僵化

**是否过度**：✅ **防守过度**

**理由**：
- bit 6-7 完全未定义使用场景
- "冻结"是一个假设的防护，代价是失去 2 bit 的未来扩展空间
- 若真有未来需求，升 FORMAT_VERSION 是更清晰的做法

**建议**：
- **删除"bit 6-7 reserved"的冻结约束**
- 改为："bit 6-7 当前未定义，Reader 应忽略（未来可能定义）"
- 或：若需要保险，可以说"bit 6-7 非 0 时按规则 1 整 Tag skip"（对称其他未知 encoding 处理）

---

### E-4：Diagnostic 的 byteOffset（诊断工具 vs 协议问题）

**超前判断**：**功能完整，但超出协议范畴**

**规格描述**（§15.1 对外 API，第 487 行）：
- `Diagnostic { code, message, byteOffset, contextIndex }`
- byteOffset 用来定位文件中的错误位置

**问题评估**：

1. **协议的职责边界**：
   - 协议规范应该定义"什么是错误"（错误码）
   - byteOffset 是**诊断工具**的职责，不是协议规范的事
   - 例：PNG spec 不规定"unexpected chunk offset 应该多少"

2. **实现成本**：
   - Decoder 需要在每个读取点记录 byteOffset
   - DecodeContext 持续追踪 currentStream.position()
   - 若有嵌套 SubStream（§8.5），需要管理 offset 栈

3. **真实价值**：
   - 对用户诊断有帮助（"error at byte 1234"）
   - 但对协议**兼容性**本身无帮助
   - 属于"运营友好"而非"设计必需"

4. **v1 中有吗**：
   - 文档未提及 v1 是否有 byteOffset
   - 若 v1 没有，这是 v2 的新增诊断能力（可选）
   - 若 v1 有，那这是向后兼容的强制项

**是否过度**：❌ **不是过度设计，而是"超出协议范畴的诊断扩展"**

**评价**：
- 本身是好的，但不应该作为"兼容性"设计的一部分讨论
- 放在"诊断体系"单独章节更合适

**建议**：不改，保留。这是良好的 API 设计。

---

### E-5：FORMAT_VERSION 的"至少 5 年无需升级"承诺

**超前判断**：**承诺过早**

**规格描述**（第 435 行）：
> "本期**动画扩展明确不升版本号**（靠 Property 壳的 `encoding` 位段 + Tag 级版本化即可）"

**问题评估**：

1. **承诺的具体性**：
   - "动画扩展明确不升版本号"是强约束
   - 但"动画"的完整需求还未 RFC，不清楚会涉及多少变更
   - 举例：若动画需要新增 keyframe 插值类型、新增 timeline marker、新增 time remap？

2. **Property encoding 位数的约束**（第 223 行）：
   - 4 bit 编码，0=Constant，1=Hold，2=Linear，3=Bezier，4=Spatial，5-15 reserved
   - 11 个预留槽位，看似充足
   - 但若动画过程中发现需要新增的**修饰符**（如 easing function 新类型）无法用 encoding 表示怎么办？
   - 例：若需要"Bezier2"（多控制点贝塞尔），能用新 encoding 值吗？能，但需要新的 value 格式

3. **"不升版本号"的真实代价**：
   - 若真的不升，所有变更必须走①②路由
   - 意味着**v2.0 文件格式的最终定稿必须非常周密**
   - 现在（v2.0 发布前）发现设计不足还能改；发布后改就困难了
   - 文档是否有"发布前最终评审清单"来验证这一点？没有。

4. **降级策略的复杂性**：
   - 若承诺不升，v2.0 Reader 遇到 v2.3 文件（新 encoding 值）需要降级
   - 降级策略清晰吗？（第 277-280 行的"静态 fallback"仅限动画期产生的 AnimationData）
   - 若是其他字段的新 encoding，降级到什么值？

**是否过度**：⚠️ **承诺过早**

**理由**：
- v2.0 尚未发布，就承诺"5 年不升版本号"
- 但未来 2-3 年的需求还不完全明确（特别是动画、3D 等）
- 过早承诺导致"若需求变化，要么破坏承诺、要么被迫凑合"

**建议**：
- 改为："v2.0 版本设计时，预期支持 Property encoding 值 0-4 及预留 5-15；若动画期需求超出现有设计，升 FORMAT_VERSION 决策由团队投票，预期频率 < 3 年"
- 在发布前完成"动画扩展需求评审"，列出所有预见的编码需求，验证是否真的不需要升版本号

---

### E-6：Tag 编号空间的三级段划分

**超前判断**：**合理分层，但浪费明显**

**规格描述**（§6.1，第 360-378 行）：

| 段 | 用途 | 范围 | 使用 | 预留 |
|---|---|---|---|---|
| 顶层 | FileHeader 等 | 1-9 | 7 | 2 |
| Layer | LayerBlock 等 | 10-19 | 5 | 5 |
| Payload | 7 种 payload | 20-39 | 7 | 13 |
| VectorElement | 14 种 element | 40-119 | 14 | 66 |
| LayerFilter | 5 种 filter | 120-139 | 5 | 15 |
| LayerStyle | 3 种 style | 140-159 | 3 | 17 |
| 动画专用 | keyframe 等 | 160-239 | 0 | 80 |
| 资源扩展 | 新 asset | 240-299 | 0 | 60 |
| 版本化变体 | Tag 版本化 | 300-599 | 0 | 300 |
| 实验性 | 第三方扩展 | 900-1021 | 0 | 122 |

**问题评估**：

1. **预留比例过高**：
   - VectorElement：14 用 80 槽（82% 预留）
   - LayerFilter：5 用 20 槽（75% 预留）
   - LayerStyle：3 用 20 槽（85% 预留）
   - 版本化变体：0 用 300 槽（100% 预留）
   - **总 TagCode 空间 10 bit → 1024 值，其中 500+ 完全预留**

2. **预留比例的合理性**：
   - PNG chunk：无预留（按需分配）
   - Protobuf：field ID 无上限（varint 编码），无需预留
   - ZIP：entry type 固定 6 个，无扩展（缺乏远见？）
   - **本方案 50% 预留是平衡点**（不是过多也不太少）

3. **但"版本化变体 300-599"完全未用的问题**：
   - 这 300 个槽是为了"未来 Tag 版本化"预留
   - 但我们已经讨论过②路径"可能过度"
   - 若②路径被砍掉，这 300 个槽就完全浪费了
   - 或者说，300 个槽预留了一个"可能过度的机制"

4. **实验性段的 UUID 隔离**（第 384-388 行）：
   - 虽然设计优雅（UUID 前缀），但 122 个槽有点多
   - 真的会有 100+ 并发实验扩展？
   - 抑或这是"以防万一"？

**是否过度**：⚠️ **分段合理，但版本化段的大小是"为可能过度的②路径服务"**

**理由**：
- 版本化段 (300-599) 占 30% 的总 TagCode 空间，却为②路径服务
- 若②路径被砍或改为"新 Tag 优先"，这 300 个槽就变成了最大的"为未来保险"的预留

**建议**：
- 缩小版本化段：300-399（100 槽足够真实使用，覆盖所有可能的不兼容 Tag 升级）
- 多出的 200 个槽回收到实验性段或保留段，使总空间结构更紧凑

---

## 五、真实兼容性风险清单

### 风险 R-1：hasExt 扩字节陷阱的"永久冻结"可能失效

**风险**：若 5 年后动画或其他扩展真的需要 hasExt extHeader 更大（2+ byte），团队被迫升 FORMAT_VERSION。

**影响**：违反"不升版本号"承诺，所有 v2.0 Reader 需要更新。

**现有防护**：§4.3 "永久冻结"红线（第 228-233 行）。

**评估**：防护是**事前约束**而非**技术防护**。若新维护者不知道这个约束，仍可能犯错。

**建议**：
- 在 TagCode.h 头注释里加 WARNING（每次 Reader 启动时可见）
- 或：改为"若需要扩 extHeader，升 FORMAT_VERSION"（事后清晰，不预防）

---

### 风险 R-2：默认值一致性跨版本

**风险**：新字段通过①追加添加时，旧 Reader 无法读到该字段，会用某个默认值。如果新 Reader 的默认值与旧 Reader 的行为不一致，语义错位。

**现有防护**：§4.3 "defaultValue 的权威声明"（第 237-256 行）；§C.2 补"默认值唯一来源纪律"。

**评估**：防护**机制清晰**（`WriteProperty` / `ReadProperty` 模板使用同一常量），但**执行依赖审查**。

**真实场景**：
- v2.0 TagA 有字段 colorSpaceMode（默认 sRGB）
- v2.2 在 TagA 尾追加 colorSpaceMode_override（默认 null，表示不覆盖）
- 旧 Reader 读 v2.2 文件时看不到该字段，按 null 处理（等同"保持原来的 colorSpaceMode"）✅ 正确
- 但若新字段的含义是"新的强制模式"（默认 scRGB），旧 Reader 仍按 sRGB → 色彩错位

**建议**：
- 在每个新增字段的 Comment 里标注"默认值含义"和"旧 Reader 行为"
- Add test case："旧 Writer（无字段）→ 新 Reader（补默认值）"来验证

---

### 风险 R-3：二进制兼容性的"灵活降级"陷阱

**风险**：文档中对"旧 Reader 遇新文件"的处理说法不一致：
- 第 202 行："未知 Tag / 未知字段可被 skip"✅
- 第 271 行："未知 encoding → skip Tag"✅
- 第 277-280 行："动画期双写静态首帧作 fallback"✅
- 但对"某个功能性 Tag 被丢后，渲染结果如何"没有定义
  - 例：LayerStyle 丢失 → 是"无 style"还是"使用默认 style"？
  - 例：Gradient 新增 colorSpace 字段，旧 Reader 读不到 → 是"按 sRGB"还是"错位"？

**现有防护**：§6.5 必选 vs 功能性 Tag 区分（第 428-431 行）。

**评估**：区分**清晰**，但"降级后的语义"需要逐 Tag 定义。

**建议**：
- 每个 Payload / Style / Filter Tag 在文档里加 "降级语义" 一节
- 例：`ShapeStyleData` 降级语义 = "仅读 color/opacity，skip gradient/pattern"
- Test case："新 Writer 写 ShapeStyleHDR，旧 Reader 只读 ShapeStyleData" → 验证渲染结果是否接受的质量

---

### 风险 R-4：TagCode 预留的"越界恐惧"

**风险**：若某个段的预留被高估，未来版本无足够槽位新增 Tag（特别是版本化变体段 300-599）。

**现有防护**：§6.1 的预留比例分析（50% 为目标）。

**评估**：预留**看起来充足**，但未来 10 年的需求仍不可知。

**例**：
- v2.0：VectorElement 14 种 → 预留 80 槽
- v2.5（3D 支持）：新增 20 种 3D element → 剩余 60 槽
- v3.0（光子追踪）：新增 40 种光学 element → 需求 > 剩余 60 槽
- 被迫新建新段或升 FORMAT_VERSION

**建议**：
- 每个发行版本记录"TagCode 消耗历史"
- 若某个段预留消耗超过 70%，提前规划扩展策略（如新建专用段）
- 或：改为"若需要扩展，升 FORMAT_VERSION"（不预留 300 个槽给版本化）

---

### 风险 R-5："二次迁移"的版本累积债

**风险**：文档第 381-382 行提到"v1→v2 TagCode 迁移"和"v2.17→v2.18 二次迁移"。

**问题**：
- v2 尚未发布，已经历 2 次内部迁移
- 若发布后又需迁移，兼容性会更复杂
- 如何保证最终发布版本不再迁移？

**建议**：
- 发布前 **Lock-in TagCode 分配**（即将所有已确定的值冻结为常量，不再调整）
- 若发布后需迁移，这是"微观架构失误"的信号，需要 RFC 级别的反思

---

## 六、边界场景覆盖

### 场景 1: PAG v1 文件被 v2 Reader 读取（向下兼容）

**现状**：✅ 清晰定义

**证据**：
- 第 62 行"与 PAG v1 的前向/后向兼容（v1 播放器对 v2 文件 graceful reject，不支持播放）"
- **v1 → v2 Reader 不支持**（只支持 v2 → v1 Reader 反向兼容）

**评估**：这是**有意设计**（v2 不读 v1），而非遗漏。符合"大版本升级"的通常做法。

**边界情况**：若某个产品同时部署 v1 和 v2 Reader，需要选择器（本文档第 187 行提到 KnownVersion 常量，v1 Reader 保持 0x01，v2 Reader 为 0x02）。

**风险**：✅ 低（设计清晰）

---

### 场景 2：v2 文件被 v1 Reader 读取（向前兼容）

**现状**：✅ 清晰定义

**证据**：
- 第 187 行 KnownVersion 校验：`if (version > KnownVersion) ctx->error(UnsupportedVersion=301) + reject`
- v1 Reader 的 KnownVersion 保持 0x01，遇 0x02 → graceful reject

**机制**：
- v1 播放器读 v2 文件 magic `'P','A','G'` ✅ 通过
- 读 version `0x02` ✅ 通过
- 检查 `0x02 > v1.KnownVersion(0x01)` → error + reject ✅

**边界情况**：v1 Reader 的 reject 是否清晰示意用户（"不支持该版本"vs"文件损坏"）？

**风险**：✅ 低（技术上清晰，UX 需打磨）

---

### 场景 3：某个 Tag 被废弃后，新 Reader 遇到老文件中的该 Tag

**现状**：⚠️ 部分清晰

**证据**：§4.4 UnknownTagCode 处理（第 406 行、第 400 行）。

**机制**：
- Writer 停止写该 Tag
- Reader（新或旧）遇到该 Tag → 按 `length` skip + warn `UnknownTagCode=400`
- **关键问题**：该 Tag 是"可选"还是"必选"？
  - 如果必选（FileHeader/CompositionList/LayerBlock），旧文件遗漏该 Tag → 整文件/composition 不可播（第 428 行定义）
  - 如果可选（LayerStyle/Filter），skip 后仅视觉降级 ✅

**建议**：文档需补充"Tag 废弃路径"：
- Deprecated tag 名单（标注废弃版本）
- 何时完全停写（如"v2.2 开始停写该 Tag"）
- 降级语义（"新 Reader skip 后的行为"）

**风险**：⚠️ 中（需要明确"废弃流程"避免手工错误）

---

### 场景 4：一个 Tag 的两个版本（v2.0 和 v2.1）共存

**现状**：✅ 清晰定义

**证据**：
- §6.5 ② Tag 版本化（第 419-426 行）
- "双写优先级规约"：Writer 同时写新旧版本，Reader 以后出现为准（LIFO）
- Test case：RoundTripTest 必须新增"新 Writer → 旧 Reader（skip 新 Tag）"用例（第 447 行）

**具体例**：
- v2.0 Writer 同时写 `ShapeStyleData` + `ShapeStyleHDR`（新）
- v2.0 Reader：读 `ShapeStyleData`（不知 HDR）
- v2.2 Reader：读 `ShapeStyleData` 和 `ShapeStyleHDR`，以后者（HDR）为准

**边界情况**：
- 若 ShapeStyleData 和 ShapeStyleHDR 的字段有交集（都有 color），LIFO override 是否正确合并？
- 文档第 426 行说"字段语义取交集"，但实现中如何操作？需要补充伪码。

**风险**：✅ 低（设计清晰，但实现需仔细）

---

### 场景 5：未知 Tag 在不同位置（Layer 头、Property 列表、附加信息）

**现状**：✅ 清晰定义

**证据**：
- §4.4 规则 1 按粒度分层 skip（第 266-270 行）
- Layer 级 Property（6 个）整体迁入 LayerTransform sub-Tag（v2.19 落地）→ seek 到 layerTransformTagEnd
- VectorElement Property → seek 到 Element Tag 尾
- Payload 内 Property → seek 到 Payload Tag 尾

**具体例**：
- 新 encoding 在 Layer.visible Property → 旧 Reader seek 到 LayerTransform 末 → Layer children 仍可读 ✅
- 新 encoding 在 VectorElement.opacity → 旧 Reader seek 到 Element 末 → 同 Payload 内其他 Element 仍可读 ✅

**边界情况**：
- 若 Layer 内有嵌套 Payload（如 CompositionRef 内 animationData），seek 到何处？
- 文档需明确"嵌套 Tag 的 skip 层级"

**风险**：✅ 低（逻辑清晰）

---

### 场景 6：Tag 体长度字段异常（为 0 / 为负 / 超过文件尾）

**现状**：⚠️ 部分清晰

**证据**：
- §H.1 硬上限（第 6377-6451 行）定义 MAX_TOTAL_BODY_BYTES 等
- 第 6477 行："所有 `stream.setPosition(base + length)` 形式的 seek 必须用 `uint64_t` 中间结果并前置校验 `base + length ≤ stream.size()`"

**具体防护**：
- Tag length = 0 → TagBody 为空（可能合法，如空 VectorPayload）
- Tag length < 0 → 不可能（length 是 u16 或 u32）
- Tag length 超文件尾 → DecodeStream 读取时会越界 → TruncatedData error（第 303 错误码，见 §G.6）

**边界情况**：
- 若多个 Tag 的 length 都合法但相加超出文件尾，应在何处检测？
- 现有代码中 Decoder 循环读 Tag 直到 End（第 559-581 行），若中途越界会如何？
- 答：SubStream 的 slice 会返回剩余字节，Read 侧会检查是否足够（见 §D.3）

**风险**：✅ 低（防护清晰，需单测覆盖）

---

### 场景 7：新字段的默认值跨版本一致性

**现状**：✅ 清晰定义

**证据**：
- §4.3 "defaultValue 的权威声明"（第 237-256 行）
- `WriteProperty` / `ReadProperty` 模板使用同一常量

**具体例**：
```cpp
// v2.0：Layer.visible 默认 true
struct LayerTransform {
  Property<bool> visible = MakeProp(true);  // 默认声明
};

// v2.0 Writer 写：
WriteProperty(stream, data.visible, /*defaultValue=*/ true);

// v2.2 新增：Layer.visibilityMode 默认 Auto
// v2.0 Reader 读 v2.2 文件时读不到 visibilityMode，按默认 Auto 处理
// ⚠️ 问题：v2.0 Reader 并不知道 visibilityMode 概念，所以"默认 Auto"实际上是"v2.2 的隐式行为"

// 新 Reader（v2.2）读：
data.visible = ReadProperty(stream, ctx, /*defaultValue=*/ true, tagEnd);
data.visibilityMode = ReadProperty(stream, ctx, /*defaultValue=*/ Auto, tagEnd);  // v2.0 文件读不到，补 Auto
```

**边界情况**：
- 若默认值的*含义*在版本间变化怎么办？
- 例：v2.0 的 colorMode 默认"sRGB"（意思是"标准 sRGB 色彩空间"）
- 例：v2.2 的 colorMode 默认"Auto"（意思是"自动检测源文件色彩空间"）
- 两个默认值数值相同但语义不同 → 兼容性问题

**建议**：
- 禁止改变默认值的**数值**（一旦定为 0，永久为 0）
- 若语义需要变化，新增新字段+新枚举值，不改旧字段

**风险**：⚠️ 中（需要编码规范保护）

---

## 七、给主决策者的核心建议

### 必修（关键性）

1. **澄清三层兼容机制的触发场景**（第 408-444 行）
   - 补充详细的"何时用①②③"决策树
   - 给 ② Tag 版本化标注为"罕用，仅当新 Tag 不可行时考虑"
   - 给 ③ FORMAT_VERSION 明确触发条件为"旧 Reader 按现规则解析会产出错误结果，而非优雅降级"

2. **补充"发布前设计冻结清单"**（v2.0 尚未发布）
   - 逐个评估预见的未来需求（动画、3D、HDR）是否真的不需升版本号
   - 若有风险，提前规划版本号升级策略（而非承诺"5 年不升"后又被打脸）

3. **完整的"Tag 废弃流程"**
   - 哪些 Tag 当前可以考虑废弃的列表（如果有的话）
   - 何时完全停写、何时旧 Reader 应 skip、降级语义定义

4. **编码规范强化**
   - 每个新增 Tag 必须标注"降级语义"
   - 每个新增字段必须标注"默认值含义"和"旧 Reader 行为"
   - Code review 清单里加"兼容性检查项"

### 建议砍掉的"未来保险"

1. **propHeader bit 6-7 的"永久冻结"约束**（第 226, 228-233 行）
   - v2 尚未发布，删除这两位的冻结约束
   - 改为"保留未用，未来若需要升 FORMAT_VERSION"
   - **收益**：设计更灵活，无代价（v2 未发布）

2. **"Tag 版本化必须双写"的强制性**（第 426 行）
   - 改为"建议双写，新 Reader 须能读旧 Tag fallback"
   - 允许某些版本化 Tag 仅写新版本（适用于完全不兼容的情况）
   - **收益**：减少文件体积，简化编码逻辑

3. **版本化变体段 300-599 的 300 个预留槽**（第 375 行）
   - 改为 300-399（100 槽足够）
   - 多出的 200 槽回收到实验段或保留
   - **收益**：TagCode 空间结构更紧凑，减少"为可能过度的②路径服务"的浪费

### 建议加强的真实演进场景

1. **为"新增 LayerType / VectorElement / ColorSource / Filter / Style"预留充足槽位**
   - 当前预留数量已足（80 + 20 + 20 + 20 = 140+ 槽），无需加强
   - 但要监控"预留消耗速度"，若某个版本快速消耗超过 50%，提前规划新段

2. **为"Property encoding 值"预留充足槽位**
   - 当前 0-4 已用，5-15 预留 11 个
   - 动画期需求评审时，需列出"预期新增多少 encoding 值"
   - 若预留 11 个不足，需改设计（如新增 Property 字段而非新 encoding 值）

3. **明确"gradual deprecation"路径**
   - 若某个 Tag 需要淘汰（如 Shape payload 替代为 Vector payload），如何演进？
   - Writer 优先写新路径，Reader 同时接受旧路径（降级）
   - 经过 N 个版本（3-5 年）后，Writer 停写旧路径，仅 Reader 兼容
   - 文档中补充"该 Tag 已 deprecated，建议使用 XXX 替代"

---

## 八、测试覆盖建议补充

### 现有覆盖

§6.5 测试要求（第 446-448 行）已明确：
- 每次用 ② 新增版本化 Tag 时，RoundTripTest 加"新 Writer → 旧 Reader"用例
- 每次用 ① 追加字段时，RoundTripTest 加"旧 Writer → 新 Reader"用例

### 建议补充

1. **跨版本默认值一致性测试**
   - 构造 v2.0 文件（无某新字段），v2.2 Reader 读后补默认值，验证语义正确

2. **TagCode 预留消耗监控**
   - 单测中统计当前已用槽位，与预期阈值比对
   - 若消耗超过 70%，test fail 并打印警告

3. **格式冻结检查**
   - 在发布前运行一次"格式冻结测试"，对比"当前 TagCode 分配"vs"初始设计分配"，确保无漂移

4. **兼容性边界场景**
   - 构造"v1.Reader-未知 encoding"文件，v2.Reader 读后验证不崩溃、不错位
   - 构造"中途 End Tag"（第 567-573 行 PrematureEndTag），验证 warn + graceful 继续

---

## 九、对比业界参照的结论

| 方案 | 兼容机制 | 复杂度 | 预留策略 | 适用场景 |
|-----|--------|-------|--------|--------|
| **PNG** | 仅 length skip + ancillary chunk type | 低 | 无（按需新增 chunk） | 简单、稳定的格式 |
| **Protobuf** | field ID 无上限 + reserved ranges | 低 | 无（无需预留，varint 编码） | 演进频繁的业务协议 |
| **MessagePack** | ext 固定格式 + type ID | 低 | 无扩展机制（设计阶段穷尽） | 高性能序列化 |
| **本方案（PAG v2）** | 3 层（字段追加 + Tag 版本化 + 文件版本） | 中高 | 多段预留 50%（300+ 槽完全预留） | 长期维护的媒体格式 |

**总体结论**：本方案比 PNG / Protobuf / MessagePack **更复杂**（3 层 vs 1-2 层），但**预留策略更激进**（为可能的未来付出现时代价）。

**是否值得**：对于"5 年生命周期、兼容性关键"的媒体格式，中等复杂度是**合理的**。但建议：
- 删除"bit 6-7 永久冻结"等过度防守
- 标注②为"罕用"，简化决策树
- 监控预留槽位消耗，定期评审是否"预留过度"

---

## 十、最终评分与建议

### 最终评分

| 维度 | 评分 | 说明 |
|-----|------|-----|
| 向前兼容机制的完整性 | ★★★★★ | 三层设计覆盖全部场景 |
| 向前兼容机制的清晰度 | ★★★☆☆ | 触发条件模糊，需补充决策树 |
| TagCode 空间规划 | ★★★★☆ | 预留充足但看似过度（版本化段 300 槽完全未用） |
| 字段演进路径清晰度 | ★★★★☆ | 框架清晰，边界情况需补充 |
| 过度超前设计的控制 | ★★★☆☆ | 存在 3-4 处"为防止将来犯错"的过度防守 |
| 真实兼容性风险覆盖 | ★★★★☆ | 大多数风险已识别，但"Tag 废弃流程"缺失 |
| 测试覆盖 | ★★★★☆ | 已覆盖主要路径，边界场景需补充 |
| **整体** | ★★★★☆ | **4/5 — 设计合理，执行细节需打磨** |

### 最终建议

**发布前必做**：
1. 补充"三层兼容机制的决策树"伪码
2. 完整的"Tag 废弃流程"文档
3. "发布冻结检查"：逐个评估未来 5 年的预见需求，确认 FORMAT_VERSION 不需升级
4. "编码规范补充"：每个新增 Tag/字段的"兼容性 checklist"

**砍掉的过度设计**：
1. propHeader bit 6-7 的"永久冻结"约束（改为"保留，未来升版本号"）
2. 版本化变体段从 300 个槽缩为 100 个（回收浪费空间）

**加强的真实需求**：
1. 动画期需求评审（明确是否真的不升版本号）
2. 预留槽位消耗监控（测试中统计已用比例）
3. 跨版本兼容性边界 case 补充

**发布后持续关注**：
1. Monitor v2.x 版本间的实际兼容性问题（收集 field reports）
2. 每个小版本发布前，评审是否"预留用尽"或"预见需求变化"
3. 若 3 年内预留消耗超过 70%，提前规划 FORMAT_VERSION=0x03 的设计

---

## 附录：评审方法论

本评审采用以下方法：

1. **对比业界参照**：PNG chunk、Protobuf field tag、MessagePack ext（3 个代表方案）
2. **真实场景驱动**：列举 6 个典型演进场景，评估方案是否清晰
3. **YAGNI 原则**：对每个"为未来预留"的设计点，问"有真实用例吗"
4. **防守深度分析**：对每个防线（红线、约束、冻结），评估"防什么、代价是什么"
5. **边界情况穷举**：7 个关键边界场景覆盖检查

---

**评审完成时间**：2026-04-30  
**评审员**：协议演进专家  
**建议后续行动**：team-lead 收集反馈，发起设计评审会议

