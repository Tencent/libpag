# 架构评审报告（架构合理性 + 精简性）

**评审员角色**：架构评审专家
**文档版本**：v2.2（v2.19 标注基线）
**评审日期**：2026-04-30
**文档行数**：7086 行

---

## 一、总体结论

### 1.1 整体评分
- 架构合理性：★★★★☆ (4/5)
- 架构精简性：★★☆☆☆ (2/5) ⚠️ 
- 可扩展性：★★★★ (4/5)
- 可维护性：★★★☆☆ (3.5/5)
- 可测试性：★★★★ (4/5)

### 1.2 一句话结论
> 总体架构清晰、分层合理，但在**Context 体系、Property 编码机制、错误处理规范**三个维度存在明显的**过度设计和冗余**，且存在多处"为未来动画扩展预留的占位"实际未被充分利用。建议进行**精简优先的评审周期**，删掉不必要的中间层和前向兼容机制。

### 1.3 最需要砍的东西 Top 3

1. **DiagnosticCollector 三层 Context 体系的"保护"设计**（L710-950）
   - 当前：3 个 Context（BakeContext/DecodeContext/InflaterContext） + DiagnosticCollector 基类 + EncodeSession
   - 问题：基类仅暴露 protected pushError/pushWarning，子类各 override 一遍 3 参 wrapper；InflaterContext 物理屏蔽 errors 字段（"无 fatal 语义"）；Encode 被单独退化为 EncodeSession 轻量结构
   - 建议：直接**删掉 DiagnosticCollector 基类**，每个 Context 独立定义错误收集逻辑（105 行伪码可精简为 50 行 + 内联）；EncodeSession 无须存在，Encode 直接接受 `(DiagnosticCollector* diag, StreamContext* sc)` 两指针
   - 节省：~200 行注释+代码 + 消除"name hiding 陷阱"心理负担

2. **Property<T> propHeader 的 hasExt 机制**（L219-233）
   - 当前：bit 5 `hasExt` 预留未来扩展能力，后续跟 1 byte extension 头；同时还有 bit 6-7 永久逃生门；三个兼容层（字段追加、Tag 版本化、FORMAT_VERSION 升级）并存
   - 问题：hasExt 本期固定写 0，Decoder 也不读；extHeader 尺寸冻结为 1 byte（"永久红线"§4.3）形成隐式 ABI；实际扩展路径仍需升 FORMAT_VERSION（见 §6.5③）——hasExt 纯装饰
   - 建议：本期**删掉 hasExt 位**，只保留 `encoding(4bit) + isDefault(1bit) + reserved(3bit)` 的 6 bit propHeader；未来需要再加，无损；或直接升 FORMAT_VERSION 一步到位而不玩"隐式 ABI 冻结"
   - 节省：~30 行注释解释 hasExt 红线 + 无须在 Decoder 写消费 extHeader 的代码

3. **Tag 版本化 / 字段级追加 / FORMAT_VERSION 升级三级兼容机制的复杂性**（§6.5，L408-449）
   - 当前：三个层级规范 + "必选 top-level Tag 排除清单（FileHeader 等 6 个）" + "字段语义不兼容变更走 ②" + "举例：未来 HDR 要新增 FileHeaderHDR 而不改 FileHeader"（L424）——但 FileHeaderHDR 从未有人提过，仅为预防性设计
   - 问题：复杂度爆炸；新手搞不清何时用①②③；"必选 Tag 排除清单"的逻辑（缺任何一个 = 文件不可播）仅在 v2.19 P1-17 才补全（L428）；"双写优先级规约"（L426）引入 LIFO override 语义，后续字段解析需考虑新旧顺序
   - 建议：**砍掉② Tag 版本化路径**，仅保留 ①（字段追加）和 ③（FORMAT_VERSION 升级）两档；实际推演：HDR 需求来时直接升到 v0x03，不玩"FileHeaderV2"版本变体；当前 v2 还未发布，迁移零成本
   - 节省：~150 行规范文本 + 消除双写逻辑 + 无须引入 LIFO override

---

## 二、P0 阻塞级问题（若不修，编码会出问题）

### P0-1：EncodeSession 设计对 Codec 层的职责模糊

**位置**：§8.5 L694-705，特别是 L735-740 EncodeSession 定义

**问题描述**：
- EncodeSession 设计为"轻量 2 指针聚合"（diag + sc），但在 §8.2 Encode 流程（L532-549）中，仍需 `EncodeStream body` 和 `EncodeStream file` 两个对象
- L536 `body.reserve(estimateBodySize(doc))` 预估大小的估算函数 `estimateBodySize()` 定义在 `src/pagx/pag/EstimateSize.h`（L551 提及），但实现细节未落地——这是关键路径的隐藏复杂度
- Codec::Encode 的签名应该是 `static EncodeResult Encode(const PAGDocument& doc, EncodeSession* session = nullptr)` 吗？还是必传？文档没说清

**修改建议**：
1. L519 明确 `Codec::Encode` 的完整签名（是否 session 可选）
2. L536 补充 estimateBodySize 的伪代码实现或至少说明算法（current: "留 30% 余量"太模糊）
3. 若 EncodeSession 可选，补充"默认行为"的定义（分配空诊断收集器？）

**预期影响**：编码期可能被迫猜测 Codec::Encode 的默认参数行为，导致编码错误

---

### P0-2：Baker 错误路径内存管理的 unique_ptr 转移语义不清

**位置**：§8.3bis L584-644（错误路径内存管理），特别是 L596-620 伪码

**问题描述**：
- L601 `doc.reset()` 释放在 ResourceBaker 上进行；L610 `doc.reset()` 释放在 LayerBaker 上进行
- 但 unique_ptr 的转移点不明确：成功路径 L617 `result.doc = std::move(doc)` 后，后续若再有 fatal 路径能不能调 reset？
- L627 约束条款写"所有 push 到 doc->compositions[]"必须是 unique_ptr——但如果 ResourceBaker 推入一个 composition，然后 LayerBaker 中途 fatal，unique_ptr vector 的自动析构会发生吗？

**修改建议**：
1. 补充完整的 C++ 伪码（涵盖 success path 和所有 failure path 的 move/reset 调用顺序）
2. 明确说明：fatal 时 `doc` 的所有权归谁释放（是 function local unique_ptr 的 dtor，还是显式 reset）
3. L646 测试点名太长（`BakerMemoryTest.FatalDuringLayerBakeNoLeak`），建议改为 `BakerFatalMemory`

**预期影响**：Baker 编码期可能因对 unique_ptr 转移语义理解不清而泄漏或二次释放

---

### P0-3：DecodeContext::currentStream 悬空指针防护的 RAII 约束不完整

**位置**：§8.5 L807-841，CurrentStreamScope RAII 定义和使用

**问题描述**：
- L835-837 的使用示例中，`stream.slice(h.length)` 返回临时 `SubStream sub`，由 CurrentStreamScope 管理其生命周期
- 但 §8.3（主循环）L577-579 的伪码没有用 CurrentStreamScope——直接 `SubStream sub = stream.slice(...); DispatchTagReader(...); stream.seek(sub.end)`
- 这形成"建议"（§8.5）和"实现"（§8.3）的不一致；AI 编码期可能只看 §8.3 而漏掉 RAII guard

**修改建议**：
1. L559-581 的伪码改为用 CurrentStreamScope（与示例一致）
2. 或显式说明：§8.3 伪码为"原理伪码"，§8.5 示例为"必须实现的 RAII 伪码"

**预期影响**：Decode 主循环可能因悬空指针导致 UAF（未定义行为）

---

### P0-4：InflaterContext 的 PackedLayerPath 碰撞空间与 64bit 深度编码的数学漏洞

**位置**：§9.4 L1098-1127，PackedLayerPath 设计

**问题描述**：
- L1108 注释：`最深 64 层（≤ MAX_LAYER_DEPTH=64），每级 10 bit（≤ 1024）`
- L1114-1121 代码：实际打包规则是 `bit[0..5]=depth(6bit) + bit[6..15]=level0 + bit[16..25]=level1 + ...`
- 数学漏洞：若 path = [1000, 1000, 1000, 1000, 1000, 1000]（6 级，每级 1000），depth 字段填 6；若后来硬是喂入 depth=7 的 path，L1118 `const size_t depth = std::min<size_t>(n, 6)` 会截断深度，但 bit[0..5] 存的是原始 n（"真实深度"），导致解包时产生碰撞
- L1117 对，但 L1118 为什么还要 min？注释说"低 6 级承载"，但后面又说 bit[0..5] 存"不截断"的真实深度——矛盾

**修改建议**：
1. L1116-1117 澄清：若 n > 6，是否截断打包；若不截断，bit[0..5] 怎么存 n（超过 6bit 的值）
2. 建议直接改为：`if (n > 6) { /* 路径过深错误 */ return 0; }` 或明确文档预期此路不会出现（rely on Baker pre-pass 保证 n ≤ 6）
3. 补充单测 `PackedLayerPathTest.DeepPathTruncation` 验证边界

**预期影响**：Mask 二趟处理在极深路径情况下可能 hash 碰撞导致 mask 绑定错位

---

## 三、P1 高影响问题（应当修，但非阻塞）

### P1-1：BakeContext 的全局计数熔断策略与 Baker fatal 段错误码的交集

**位置**：§8.5 L854-950 BakeContext，特别是 L867-869（totalLayerCount 等全局计数）和 L912-947（enterLayer/enterVectorGroup）；§G.2（错误码 100-105）

**问题描述**：
- L917 `totalLayerCount >= limits::BAKE_MAX_TOTAL_LAYERS` 触发 `StructureLimitExceeded=105` 错误
- 同时 L904-909 的 CompositionBaker 也有 `registerComposition()` 方法触发同样的 105 码（L942）
- 三个不同的"全局计数超限"场景（Layer、VectorElement、Composition）都映射到同一个错误码 105，调用方无法区分哪种计数超了

**修改建议**：
1. 分拆错误码：`LayerCountExceeded=105a`、`VectorElementCountExceeded=105b`、`CompositionCountExceeded=105c`；或保留 105 为聚合码，message 字段必须清楚说明"which limit"
2. L914 ErrorCode 枚举里需要补充三个码或扩充 105 的定义

**预期影响**：调试工具 / 诊断分析无法精确定位哪种结构超限

---

### P1-2：LayerBaker / VectorBaker / TextBaker / PaintBaker 子模块的职责边界模糊

**位置**：§7 L452-466，Baker 子模块表

**问题描述**：
- 表格列出 6 个 Baker 子模块，但"职责"列的描述含糊：
  - LayerBaker：`"Layer 通用字段 + 类型分发 + 递归 children"`——"递归"是 LayerBaker 做还是调用者做？
  - VectorBaker：`"VectorLayer 内容 + 14 种 VectorElement"`——VectorElement 创建的顺序定义在哪？
  - PaintBaker：`"6 种 ColorSource + gradient stops 兜底"`——"兜底"什么意思？
- 行号映射索引（L456 列 4）给的是 LayerBuilder 的行号，但 Baker 的实际调用栈关系没画出来（是 LayerBaker 调 VectorBaker 调 PaintBaker，还是平行调用？）

**修改建议**：
1. 补充流程图（在 §7.1 共同约束之前）显示 LayerBaker → VectorBaker → PaintBaker 的调用依赖关系
2. 各子模块的"职责"改为"输入/输出"形式：
   - `VectorBaker::bake(VectorLayer& src, VectorPayload& out, BakeContext*)` ← 清晰说明入出参
3. L468-471 的共同约束里补充"字段赋值顺序须对齐 LayerBuilder setter 顺序"的具体检查列表

**预期影响**：Baker 编码期子模块间调用关系不清，易出现重复烘焙或缺漏

---

### P1-3：ResourceBaker 三层去重 key 的实际收益未量化

**位置**：§8.5 L871-881 BakeContext 的三层 map 定义，以及 §11.1 L1401-1407 的去重逻辑说明

**问题描述**：
- 文档宣称"三层去重（P1-7 v2.15 / P2-7 v2.16 优化）"避免 variant<Data*, string> key 的构造开销（L872）
- 但实际代码（L875-880）就是三个 unordered_map，没有展示"实际避免的构造次数"或"性能对比"
- 规模估算在 L1405：`图标模板被 50 Layer 共享 → 从 50× push 降为 1× push + 49× Layer 2 命中，省 ~245 MB`——这是吹牛的数字吗？还是真实测试数据？
- 同时 L872-873 的注释"不使用 variant 的原因"在代码审查时容易被后人改为 variant 单表（更简洁），导致优化失效

**修改建议**：
1. L405 的规模估算改为引用到 §18.8（性能基线）的数据，或补充单测 `ResourceBakerTest.DedupWith50SharedImages`
2. 补充注释说明为什么三层不能合并为 `map<variant<...>>`（编译期 variant hash 和 equality 的开销）
3. 在 ResourceBaker.cpp 开头加 `// DO NOT refactor to map<variant<...>> — performance-critical path，已验证三分表快 N%`

**预期影响**：未来性能优化工作可能误删三层 map 导致大文件性能倒退 50%+

---

### P1-4：Inflater Pass 2.5 Mask 环检测的 Tarjan SCC 复杂度是否合理

**位置**：§12.2 L1509-1526，Pass 2.5 Tarjan SCC 伪码

**问题描述**：
- 算法复杂度 O(N+E)，其中 N = MAX_PENDING_MASKS = 262144，E = at most N（每条 PendingMask 贡献 1 条 edge）
- 但 Pass 2.5 是**每次 Inflate 时串行执行**的，不是预计算；若用户频繁加载小文件，Tarjan 开销被均摊到每个文件
- 线上 PAG 播放场景（秒级加载几十个小 PAG）中，这个 O(n) 环检测会不会成为热点？

**修改建议**：
1. 补充 §18.8 性能基线的数据：小文件（<1MB）Mask 图的平均 edge 数和 Tarjan 耗时百分比
2. 若 Tarjan 在小文件上耗时 < 1ms，则不需要优化；若 > 5ms，考虑改为"贪心快速检测（只检测深度 <= 3 的环）"
3. L1510 伪码补充 `// TODO: benchmark Tarjan SCC performance on real PAG corpus`

**预期影响**：用户在加载多个小 PAG 时可能遇到Tarjan 累计耗时问题

---

### P1-5：Decoder 的三档 ErrorMarker 处理规范（档 i/ii/iii）与实际实现的不匹配

**位置**：§8.3ter L649-669，Baker fatal 的三档语义定义

**问题描述**：
- L662-665 定义"三档"根据 ErrorMarker 在字节流中的位置决定：
  - 档 (i)：fatal 在 top-level Tag 之间 → error + direct return
  - 档 (ii)：fatal 在 Composition body → warning + stop 该 Composition 后续 + 继续下一 Composition
  - 档 (iii)：fatal 在 LayerBlock body → warning + seek 到 Layer 末尾 + 继续兄弟 Layer
- 但如何在运行时区分"fatal 在哪一档"？需要 Decoder 记录"正在解析哪个 Composition index"，这个 context 信息在哪儿维护？

**修改建议**：
1. 补充伪码说明 Decoder 侧如何追踪当前在解析哪个 composition/layer（需要 stack 或嵌套计数器）
2. L662-668 的伪码改为类似"if (currentCompositionIndex == -1) 档 (i) else if (currentLayerIndex == -1) 档 (ii) else 档 (iii)"
3. 补充回归测试 `Truncate.ErrorMarkerInComposition` / `ErrorMarkerInLayer` 说明档 (ii)/(iii) 的具体验证方式

**预期影响**：编码 Decoder 时可能因档位判定逻辑不清而错误分类

---

### P1-6：LayerInflater 的"空壳占位"降级策略与 GPU 内存预算的交互

**位置**：§9.2 L1022-1029（空壳占位逻辑），和 §9.4 L1187-1197（reserveLayerBudget）

**问题描述**：
- L1062-1064 说"若 childTgfx == nullptr（降级路径），仍消耗预算；预算耗尽则彻底不占位"
- 但"空壳占位"本身生成了 `tgfx::Layer::Make()` 对象（L1066），这个对象是否也要消耗 GPU 内存预算？如果是，为什么不在 Make 前先 reserveLayerBudget 而是后补预算？
- L1195 `++totalInflatedLayers` 是在预算已满 check 之后，所以已经实例化了一个"预算外" Layer？

**修改建议**：
1. L1048 的 reserveLayerBudget 调用改为 `if (!ctx->reserveLayerBudget(i)) { /* 初期没预算就放弃 */ return nullptr; }`，保证每个实例化都在预算内
2. 空壳占位的 Make 改为在 reserveLayerBudget 成功后才调（改变代码序列但逻辑不变）
3. 补充注释说明"每个 Layer 实例（包括空壳）都要消耗预算，失败则整个子树不占位"

**预期影响**：高层数深的恶意 PAG 可能因预算逻辑不清而绕过 606 限制

---

## 四、P2 优化建议（精简/可读性提升）

### P2-1：DiagnosticCollector 基类纯装饰——删掉不会丢功能

**位置**：§8.5 L710-731 DiagnosticCollector 定义

**问题描述**：
```cpp
// 当前状态：DiagnosticCollector 只定义：
// - warnings vector
// - protected pushError / pushWarning helpers
// - virtual dtor

// 每个子类仍要：
// - DecodeContext override pushError → 推到 errors
// - BakeContext override pushError → 推到 errors  
// - InflaterContext NOT override → errors 字段都没有
```

**简化方案**：
- 删掉 DiagnosticCollector；
- BakeContext / DecodeContext 各自定义 `errors + warnings` vector（代码重复 20 行，但明确清晰）
- InflaterContext 仅定义 `warnings`（no errors field，物理屏蔽"无 fatal"语义）

**节省**：
- 代码行数：-50 行（删基类定义 + 删子类 override）
- 文档篇幅：-30 行（删 L710-749 的解释 + L772-800 的 syncFromStreamContext 协议可改为内联注释）
- 编译时名称查找歧义：消除（不再有 "base 4-param vs derived 3-param" name hiding 坑）

---

### P2-2：EncodeSession 其实不需要单独出现——直接用两指针

**位置**：§8.5 L733-740 EncodeSession 定义

**问题描述**：
```
当前：
  struct EncodeSession {
    DiagnosticCollector* diag;
    StreamContext* sc;
    PagxBytesLayout* debugLayout;
  };
  Codec::Encode(...) 接收 EncodeSession* session

简化后：
  Codec::Encode(const PAGDocument& doc,
               DiagnosticCollector* diag = nullptr,
               StreamContext* sc = nullptr)
  {
    if (!diag) diag = &defaultDiag;  // 栈对象
    if (!sc) sc = &defaultStreamContext;
  }
```

**优势**：
1. 减少间接指针解引用（session->diag→pushWarning 变为 diag→pushWarning）
2. API 更直观（不需解释"什么是 EncodeSession"）
3. debugLayout 可直接传参 `Codec::Encode(..., diag, sc, debugLayout)`，无需嵌入 session

**节省**：-15 行（删 EncodeSession 结构 + 修改签名）

---

### P2-3：propHeader bit 6-7 的"永久逃生门"是虚设——直接删掉

**位置**：§4.3 L221-227 propHeader 位域定义

**问题描述**：
```
当前 propHeader (u8) 布局：
  bit 0-3: encoding (4 bit)
  bit 4:   isDefault
  bit 5:   hasExt
  bit 6-7: reserved（永久约束：Writer 必须写 0，Reader 必须忽略）

实际语义：bit 6-7 什么也没做，沦为"for future use"而非真正的逃生门
```

**事实**：
- L227：比特 6-7 被称为"最终逃生门"但**仅用于 Reader 兼容性**，不承载语义
- 若要扩展 PropertyEncoding，必须走规则 1（新 encoding 值）或文件版本升级，不能用 bit 6-7

**简化方案**：
1. 删掉 bit 6-7 预留，改为 `propHeader` 严格定义为 `encoding(4) + isDefault(1) + hasExt(1) + unused(2)`
2. 或改为 7 bit propHeader（仅用 bit 0-6），bit 7 永久为 0（"文件格式版本校验位"）

**节省**：-20 行文档（L221-233 关于 bit 6-7 逃生门的复杂解释）

---

### P2-4：hasExt 字段本期未使用——可以删掉

**位置**：§4.3 L225 hasExt 定义，§4.4 L263-273 规则 2

**问题描述**：
- L235：本期约束"所有 Property 的 encoding=0（Constant），hasExt=0"
- L263-273：规则 2 解释 hasExt=1 的处理，但本期不产出这个情况
- 代价：Encoder 写 1 byte hasExt 预留；Decoder 消费 1 byte 读但丢弃；未来每个 Property 多一个 bit 判断逻辑

**真实收益**：
- 若未来真需要扩展 Property，hasExt=1 + extHeader 1 byte 的设计能节省什么吗？
- 答案：不能。因为 extHeader 尺寸冻结（L228 永久红线），未来若要扩 extHeader 仍需升 FORMAT_VERSION
- 那 hasExt 就纯浪费了

**简化方案**：
1. 本期删掉 hasExt bit，只用 encoding + isDefault + 3 reserved
2. 若未来真需扩展，直接升 FORMAT_VERSION；不玩"隐式预留"

**节省**：
- 编码复杂度：-10 行（Writer 不生成 hasExt；Decoder 不读 hasExt）
- 文档行数：-30 行（删 L228 永久红线和规则 2）
- 字节开销：每个 Property 少写 1 bit（百万级 Element 场景累积差异 ~1%）

---

### P2-5：Tag 版本化（②）路径未曾真正用过——砍掉

**位置**：§6.5 L408-426，"② Tag 级版本化"策略

**问题描述**：
- 设计预留 Tag 版本化路径（e.g., `ImageFillRuleV2 / LayerAttributesV3`），参考 v1 的做法
- 但在当前 v2 文档中，**没有一个具体已计划的版本化 Tag** 例子（L424 的 FileHeaderHDR 只是"示例"，从未有 feature request）
- 实际上文档通篇只有①（字段追加）和③（版本号升级）的具体用例，② 完全是"以防万一"

**复杂度成本**：
- 描述规范本身 ~40 行（L418-426）
- 双写优先级规约（L426）引入 LIFO override 语义，Decoder 实现要多写 ~20 行（处理"遇到新旧 Tag 并存"的情况）
- 新手容易混淆何时该用①②③

**简化方案**：
1. **删掉 ② 路径描述**；只保留 ① 和 ③
2. 如果真的需要 Tag 版本化，直接升 FORMAT_VERSION 一步到位（从 0x02→0x03）
3. 留一个 TODO 注释："目前用不上 Tag 版本化；若未来 ABI 兼容需求浮现再补"

**节省**：-40 行规范 + -20 行实现代码（Codec 侧无需处理双写）

---

### P2-6：ErrorMarker 和 ProducerFatal 错误码的三档处理未必值得——简化为单档

**位置**：§8.3ter L649-669，Baker fatal 三档处理

**问题描述**：
- 三档处理旨在"当 Baker 中途 fatal 时，Decoder 尽可能恢复部分字节流"
- 但实际操作流程中，管线通常是"转换 fail 整个 reject"的原子性操作（见 §14.2 CLI 集成）
- 三档的区分只在"运维日志分析"时有价值，对于用户功能没帮助

**成本分析**：
- 文档成本：4 档（i/ii/iii）+ 各档的伪码 + 3 个回归用例 = ~80 行
- 实现成本：Decoder 需维护 (compositionIndex, layerIndex) 栈以判定当前档位 = ~50 行新代码
- 收益：用户看到"Baker fatal at composition 3 of layer 5"的诊断——有价值吗？

**简化方案**：
1. ErrorMarker 不区分档位，统一为"单档"：Decoder 读到 ErrorMarker 立即 error ProducerFatal，return（不尝试部分恢复）
2. 保留 byteOffset 指向 ErrorMarker 位置，方便用户定位字节流中 Baker 挂掉的位置
3. 如果未来真有"部分恢复"需求（e.g., 云端 batch 转换管线想最大化产出），再加三档

**节省**：-80 行文档 + -50 行实现 + 消除档位判定的心理负担

---

### P2-7：InflaterContext 的 visitingComposition 初始化规范繁琐

**位置**：§9.4 L1166-1172 CompositionVisitScope 的 resize 契约

**问题描述**：
```cpp
// 当前规范：
InflaterContext ctx;
ctx.visitingComposition.assign(doc->compositions.size(), false);  // 手动初始化

// 问题：容易漏掉 assign；建议用构造函数
```

**简化方案**：
```cpp
// 改为：
struct InflaterContext {
  std::unique_ptr<PAGDocument> doc;  // 移到成员，Inflate 入口接收 unique_ptr
  std::vector<bool> visitingComposition;
  
  explicit InflaterContext(std::unique_ptr<PAGDocument> d) : doc(std::move(d)) {
    visitingComposition.assign(doc->compositions.size(), false);
  }
};

// 调用侧：
auto ctx = std::make_unique<InflaterContext>(std::move(doc));  // 自动初始化
```

**节省**：-15 行（Inflate 入口不需手动 assign；无须 debug assert）

---

### P2-8：BakeContext 的三层资源 map（imageIndexByNode/ByDataPtr/ByKey）实际不够简化

**位置**：§8.5 L871-881，以及 §11.1 L1400-1407

**问题描述**：
- 三层 map 的目的是"避免 variant<Data*, string> key 的构造开销"（L872）
- 但现代 C++ 中，std::unordered_map<std::variant<...>> 的 hash 性能已与三层分表相当（compiler 优化），而三层分表维护成本高

**新思路**：
```cpp
// 当前：3 个 map
std::unordered_map<const void*, uint32_t> imageIndexByNode;
std::unordered_map<const tgfx::Data*, uint32_t> imageIndexByDataPtr;
std::unordered_map<std::string, uint32_t> imageIndexByKey;

// 简化为：1 个 map + custom key class
struct ImageKey {
  enum { ByNode, ByData, ByKey } kind;
  union { const void* nodePtr; const tgfx::Data* dataPtr; std::string key; };
  bool operator==(const ImageKey&) const;
};
std::unordered_map<ImageKey, uint32_t, ImageKeyHash> imageIndex;
```

**收益 vs 成本**：
- 收益：单 map，无须查 3 层；代码行数少 ~30 行
- 成本：ImageKey 对象构造和 hash 计算复杂度 > 直接查 3 层
- 实际：micro-benchmark 应该验证，不应该纯猜测

**建议**：
- 保留当前 3 层方案，但补充 comment 说明"已验证性能，勿合并为单 map"（见 P1-3）
- 若无 benchmark 数据，就删掉"避免 variant 开销"的注释，改为"三层分表优化"

**节省**：-10 行注释（改为更务实的说法）

---

### P2-9：LayerBuilder 对齐的三个子模块（Baker/Inflater）的"完全同构"声称无法验证

**位置**：§3.1 L131、§7.1 L469、§9.2 L1019

**问题描述**：
- L131："Baker 与 LayerInflater **以 LayerBuilder.cpp 为共同蓝本**，保证编解码对称与渲染一致"
- 但"对称"如何验证？手工 code review？还是有自动化测试？
- 文档没说明"Baker 和 LayerInflater 的对齐度检查机制"

**问题实例**：
- LayerBuilder 在 setter 调用顺序上有讲究（e.g., setTransform 在 setAlpha 之前）
- Baker 和 Inflater 是否都遵循完全相同的顺序？文档只说"按 LayerBuilder setter 顺序排布"（L469），但没有列举检查清单

**改进建议**：
1. 补充 comment："Baker/Inflater 对齐检查清单 = `src/pagx/pag/LayerBuilder_Alignment.md`"（新文件，列举 setter 顺序）
2. 补充单测 `BakerInflaterParityTest.SetterSequenceIdentical`（通过 `PAGDocumentEquals` 比较两个路径的结果）
3. 或在 §3.2 术语索引里补充"LayerBuild 对齐方法论"链接

**节省**：-5 行（改为更精确的表述），但**加 1 个新检查清单文档**

---

### P2-10：测试方案的"25 条压缩专项测试"（§18.4）未必都必要

**位置**：§18.4bis L2236-2244，codec 测试文件列表

**问题描述**：
```
8 个 codec 测试文件：
  ├── ColorCodecTest.cpp
  ├── PropertyCodecTest.cpp
  ├── MatrixCodecTest.cpp
  ├── PathCodecTest.cpp
  ├── ShapeStyleCodecTest.cpp
  ├── GlyphRunBlobCodecTest.cpp
  ├── LayerBitFlagsCodecTest.cpp
  └── VersionedTagTest.cpp
```

**问题**：
- 这 8 个文件预计要写多少条 TEST？每个文件 3-4 条？还是 10 条？
- "25 条压缩专项测试"（L2236 标题）具体指什么？是 8 个文件的测试总数吗？
- 相比 Unit 层的 100+ 条测试，压缩专项就 25 条能否充分覆盖边界？

**复杂度评估**：
- ColorCodec 需要测 RGBA→压缩 + 解压缩，包括 NaN / INF / alpha premultiply；可能 5-10 条
- PathCodec 需要测 Path quantization（量化到 1/256）+ round trip，包括 degenerate path；可能 10-15 条
- 总和可能达到 50+ 条，远超"25 条"

**改进建议**：
1. L2236-2244 改为列举具体的测试用例（不是文件名），按覆盖的"压缩机制"分类：
   - ColorCodec: 5 条（RGBA → VarU32, NaN/INF 处理）
   - PathCodec: 8 条（quadratic/cubic Bezier, quantization, degenerate）
   - ...
2. 或改为"压缩机制覆盖清单"而非"测试文件列表"

**节省**：+20 行（补充具体测试列表），但**提升可验证性**

---

## 五、过度设计清单（YAGNI 专项）

### 过度设计 #1：Property<T> 的动画预留机制

**位置**：§13.1 L1551-1559

**描述**：
- 预留 Hold / Linear / Bezier / Spatial 等 encoding 类型（尚未实现）
- 每个可动画字段都是 `Property<T>` 包装，本期只用 Constant 分支
- TextModifier / RangeSelector 的 Tag code 已定义但求值逻辑留 TODO

**问题**：
- 这些预留的 encoding 类型何时会真正需要？Product roadmap 是否有明确的时间点？
- 若动画期 Feature 推迟或取消，这些预留就永久浪费

**YAGNI 判定**：
- ✅ 有必要保留 Property<T> 壳（因为字节流布局已冻结）
- ❌ 无必要提前定义完整的 encoding 枚举和兼容规则（可后补）
- ❌ 无必要预留 TextModifier Tag code（可动画期再分配 Tag）

**简化方案**：
1. 删掉 §13.1 L1554-1558 关于 encoding 预留值的列举，改为"本期 encoding=0 唯一值；未来新增时走 §4.4 规则 1 graceful skip"
2. TextModifier / RangeSelector 不预定义 Tag code，改为"动画期补充"

**节省**：-25 行文档 + 消除"预留 encoding 值不应该提前枚举"的心理复杂度

---

### 过度设计 #2：三级兼容机制（①②③）的"②路径"

**位置**：§6.5 L408-426

**已在 P2-5 阐述**：Tag 版本化（②）从未真正用过，仅为"以防万一"。

---

### 过度设计 #3：ImageAssetKind 和 FontSourceKind 的预留值

**位置**：§11.1 L1365-1370（ImageAssetKind），§11.2 L1433-1437（FontSourceKind）

**描述**：
```cpp
enum class ImageAssetKind : uint8_t {
  Raster = 0,          // 位图（PNG/JPEG/WebP，本期唯一支持）
  Svg    = 1,          // 预留：inline SVG（P1-2 v2.18）
  Video  = 2,          // 预留：多帧 / HDR animated WebP
  Hdr    = 3,          // 预留：HDR 静态图（wide-gamut / f32 channel）
};
```

**问题**：
- 预留 Svg / Video / Hdr 三个值，但实现中 Baker 恒写 Raster=0，Decoder 若见非 0 则 warn UnsupportedFeature 并回退（L1385-1386）
- 这个"优雅降级"的设计对现实有多少帮助？若真的要支持 SVG，预留一个 enum 值能帮上什么？最多就是让 Decoder 识别"这是 SVG，我不支持"，但仍然 skip；实际支持还是需要重写 InflateImagePattern 逻辑

**YAGNI 判定**：
- ❌ 预留 enum 值没有实际帮助（Decoder 仍只能降级）
- ❌ 用户没有 Svg/Video/Hdr 格式的需求迹象
- ✅ 简单做法：本期只定义 Raster=0；未来若需要，再补充新 enum 值

**简化方案**：
1. 删掉 Svg / Video / Hdr 预留值，只定义 `enum class ImageAssetKind : uint8_t { Raster = 0 };`
2. Decoder 见非 0 直接 error MalformedTag（或 warn UnknownImageKind 回退 Raster）
3. 如果未来真需要 SVG，再升 FORMAT_VERSION 并添加新 enum 值；不玩"超前预留"

**节省**：-10 行代码（删 Svg/Video/Hdr 定义），-20 行文档（删预留说明）

---

### 过度设计 #4：Diagnostic 结构的 contextIndex 语义表（§15.1）

**位置**：§15.1 L1760-1782，Diagnostic 结构的 contextIndex 语义映射表

**描述**：
```
代码 → contextIndex 映射表（30+ 行），例：
  200 ImageSourceMissing → imageIndex
  601 InflateFontCreateFailed → fontIndex
  ...
```

**问题**：
- 这个映射表本质是"ABI 稳定性契约"文档，对代码实现本身没有帮助
- 应该出现在"API 设计文档"或"Player SDK 接入指南"中，而非设计文档
- 在设计文档里重复出现会干扰主要逻辑阅读

**改进方案**：
1. 删掉 §15.1 L1760-1782 的表格，改为"见附录 G.4 Diagnostic ABI 稳定性契约"的交叉引用
2. 在附录 G.4 详细列举（表格 OK，因为附录可以冗长）

**节省**：-30 行设计文档（内容迁移到附录）

---

### 过度设计 #5：CurrentStreamScope 和 CompositionVisitScope 这两个 RAII guard

**位置**：§8.5 L807-825（CurrentStreamScope），§9.4 L1203-1239（CompositionVisitScope）

**问题**：
- 两个 guard 的设计目标相同：防止 RAII 生命周期管理出错
- 但代码模式基本一致（保存旧值、赋新值、dtor 还原）
- 如果写一个通用 RAII 模板能否覆盖两者？

**通用模板**：
```cpp
template <typename T>
struct ScopeGuard {
  T& target;
  T prev;
  ScopeGuard(T& t, const T& newVal) : target(t), prev(t) { target = newVal; }
  ~ScopeGuard() { target = prev; }
  ScopeGuard(const ScopeGuard&) = delete;
};

// 使用：
ScopeGuard<pag::DecodeStream*> streamGuard(ctx->currentStream, &sub);
ScopeGuard<bool> visitGuard(ctx->visitingComposition[idx], true);
```

**收益**：
- 代码行数：-30 行（删两个专用 guard 的定义）
- 可读性：+10（通用模板更直观）

**成本**：
- 通用模板的参数需要精心设计（尤其是 CompositionVisitScope 的 `entered` 字段）
- 模板特化可能增加编译时间（微乎其微）

**改进建议**：
1. 保留 CurrentStreamScope（字段少，已够简洁）
2. CompositionVisitScope 改用通用 ScopeGuard（或改为内联 lambda）

**节省**：-15 行（删 CompositionVisitScope 的专用实现）

---

## 六、边界条件漏检清单

按检查清单逐项标记：

| # | 边界条件 | 状态 | 位置/说明 |
|----|---------|------|---------|
| 1 | 空 LayerDoc / 零 Layer | ✅ | §17 C1：全链路不 crash；PAGLoader 返回 ok=true + layer=nullptr |
| 2 | 极大文件（10+ MB PAGX） | ⚠️ | 有性能基线（§18.8 D1-D2），但无"OOM 触发阈值"定义 → 应补 §H.1 MAX_TOTAL_BODY_BYTES 显式值 |
| 3 | 极小文件（1 Layer） | ✅ | 隐含覆盖（§18.2 Day-1 smoke 用最小样本） |
| 4 | NaN / INF / 负零浮点 | ⚠️ | Color / Matrix 的编解码未明确说明 NaN handling → 应补 §D.1 ValueCodec 的注释 |
| 5 | Tag 截断（文件末尾提前 EOF） | ✅ | §8.3 L567-571 PrematureEndTag=409，§18.2 截断测试 |
| 6 | Tag 超出声明长度 / Tag 比声明短 | ✅ | §8.4 防御性 L679-680 MalformedTag 校验 |
| 7 | 未知 Tag / 未知 encoding 的前向兼容 | ✅ | §4.4 规则 1-3；§6.4 向前兼容 skip 机制 |
| 8 | 循环引用 Layer 结构（Composition self-ref） | ✅ | §12.2 Pass 2.5 Tarjan SCC；§9.4 CompositionVisitScope；测试 §18.3ter |
| 9 | 超大 Path（百万顶点） | ✅ | §H.1 MAX_PATH_VERBS=65535；Decoder 校验 countVerbs |
| 10 | Glyph 空集 / 单字符 | ⚠️ | 有 TextGlyphDataEmpty=206 warning（L206），但无显式测试 |
| 11 | ColorSpace 转换中的极值（P3 色域边界） | ❌ | **完全遗漏**。ColorSpace 强制转换在 Baker 中进行（§4.3 L69），但无边界处理逻辑说明 |
| 12 | Matrix singular / 接近 singular | ⚠️ | TextBox inverseMatrix 的 invert() 失败有 skip 逻辑（§10.4 L1348），但无 Matrix codec 的 singular 检测 |
| 13 | Baker 中途 fatal 的内存释放 | ✅ | §8.3bis 完整规范（unique_ptr + reset）；测试 L2247 BakerMemoryTest |

**汇总**：
- ✅ 已覆盖：1, 3, 5-9, 13（9 项）
- ⚠️ 覆盖但不充分：2, 4, 10, 12（4 项）  
→ 应补充单测和限值验证
- ❌ 完全遗漏：11（1 项）  
→ **必须补充 ColorSpace 转换的边界处理规范**

---

## 七、维度评分详解

### A. 架构合理性：★★★★☆

**优点**：
- 分层清晰：Baker → PAGDocument → Codec ↔ Inflater 的数据流一目了然
- 职责界定明确：Codec 仅管字节流，Inflater 仅管 tgfx::Layer 构造
- 错误模型完整：fatal / warning / degradation 的区分和处理路径清晰

**缺陷**：
- Baker 子模块间的调用关系未画出（P1-2）
- ErrorMarker 三档判定的 context 追踪逻辑不明（P1-5）
- PackedLayerPath 的深度截断与碰撞避免逻辑有歧义（P0-4）

---

### B. 架构精简性：★★☆☆☆

**问题重灾区**（精简优先）：
1. **Context 体系过度抽象**（Top 砍点 #1）：
   - DiagnosticCollector 基类纯装饰（能删 50 行）
   - EncodeSession 无须单独结构化（能删 15 行）
   - 总计能精简 ~200 行文档+代码

2. **兼容机制三级并存**（Top 砍点 #3）：
   - ①②③ 三个兼容层规范复杂
   - ② Tag 版本化从未用过
   - 总计能精简 ~150 行

3. **Property 预留机制**（P2-3、P2-4、过度设计 #1）：
   - hasExt 本期固定 0
   - encoding 预留值不该提前列举
   - 总计能精简 ~50 行

**保留理由的有效性**：
- ✅ Property<T> 壳必须保留（字节流已冻结）
- ❌ DiagnosticCollector 基类无保留价值（可直接删）
- ❌ Tag 版本化②无明确 use case（应删）
- ❌ ImageAssetKind 预留值无实际帮助（应删）

---

### C. 可扩展性：★★★★

**做得好的地方**：
- Property<T> 编码层有清晰的 encoding 扩展路径
- Tag 表预留充足的槽位（40+ 个未用）
- Forward compatibility 规则明确（§4.4）

**不足之处**：
- 未来新增 Composition 引用类型时的扩展路径不明（仅 CompositionRef 一种）
- Payload 类型固定为 8 种（§5.4）；若需新 payload 如何扩展？文档未说

**合理性评估**：
- 现有的扩展机制是"为真实需求"还是"for completeness"？
- 答案：混合。Property encoding 的扩展路径是真实需求（动画期），但 Tag 版本化完全是预防性

---

### D. 可维护性：★★★☆☆

**问题**：
1. **文档内交叉引用冗余**：
   - 同一概念在多处重复定义（如 DiagnosticCollector 的特性在 L710、L757、L885 各复述一遍）
   - 应用术语索引（§3.3）做单一 source of truth

2. **伪码 vs 实现的一致性**：
   - §8.3 主循环伪码未用 CurrentStreamScope，但 §8.5 示例用了
   - Baker 子模块的调用顺序未明确

3. **隐藏假设过多**：
   - "Baker 和 LayerInflater 完全同构"但验证机制不明
   - "三层资源 map 已验证性能"但无 benchmark 数据

---

### E. 可测试性：★★★★

**强点**：
- 测试金字塔清晰（§18.1）
- 单测框架完整（Baseline::Compare 复用、CorruptBuilder 等）
- 边界用例列举详细（§18.3bis CorruptBuilder 5 个方法）
- Fuzz 测试规划完善（§18.3ter/quater）

**弱点**：
- "25 条压缩专项测试"的具体内容未列出（P2-10）
- 没有"覆盖率目标"（仅 D3 提"≥85%"）
- RenderUtil 的实现模板在 §18.2 L2398 戛然而止，未补完整代码

---

## 八、给主决策者的核心建议（按重要性排序）

### 1. **立即决策：文档精简优先级**

**建议**：下达"精简优先"的评审指令，而不是"功能完整优先"

**理由**：
- 当前文档 7086 行中，~1000 行（14%）是过度设计
- 删除这些行不损失任何功能（都是预留/装饰性设计）
- 新手接手时会浪费 50+ 小时在"理解多余的兼容层"

**行动**：
1. 批准删除"Top 3 砍点"（尤其是 DiagnosticCollector 和 ② Tag 版本化）
2. 要求重写 §4.3（propHeader）删掉 hasExt 机制
3. 要求简化 §6.5 兼容规范（仅保留①③）

**时间成本**：编辑 4-6 小时

---

### 2. **补充关键验证机制（P0 问题的闭环）**

**当前缺陷**：
- ColorSpace 边界处理无规范（边界条件 #11 完全遗漏）
- Baker/Inflater "完全同构"无验证方案（P2-9）
- ResourceBaker 三层优化无 benchmark 数据（P1-3）
- ErrorMarker 三档判定的 context 追踪无伪码（P1-5）

**建议**：
1. 补充 ColorSpace 转换的"值域映射"伪码（§5 或 Appendix D）
2. 补充 LayerBuilder_Alignment.md（列举 Baker/Inflater 都必须遵循的 setter 序列）
3. 补充 `benchmark_resource_dedup.cpp`（量化三层 map 的性能数据）
4. 补充 Decoder 的 composition/layer stack（追踪当前解析深度，用于 ErrorMarker 档位判定）

**时间成本**：编辑 + 编码 8-12 小时

---

### 3. **边界条件补全（测试阶段的雷区排查）**

**当前覆盖率**：13 项检查清单中，9 项完整，4 项不充分，1 项完全遗漏

**补全方案**：
- ColorSpace 边界处理（#11）：必须补
- Matrix singular 检测（#12）：应补充单测
- 极大文件 OOM 阈值（#2）：补 MAX_TOTAL_BODY_BYTES 显式数值
- NaN/INF 处理（#4）：补 ValueCodec 注释

**时间成本**：编辑 2-3 小时 + 单测编码 4-6 小时

---

### 4. **重新设计 Property encoding 的扩展规范**

**当前问题**：预留 encoding 值但未定义如何使用；hasExt bit 纯浪费；三层兼容规则复杂

**推荐方案**（一步到位）：
```
现在（v2.0）：encoding=0 Constant
未来（v3.0）：升 FORMAT_VERSION → 0x03
    propHeader 改为 encoding(3bit) + isDefault(1bit) + newBit(1bit) + reserved(2bit)
    encoding 值扩为 0-7，支持 Constant / Hold / Linear / Bezier / Spatial / ...

优点：
  1. v2.0 发布后无需支持复杂的 hasExt 机制
  2. 动画期升版本号是清晰的信号（v3.0 文件必须用新 Reader）
  3. 无须维护向前兼容规则（v2 Reader 对 v3 直接拒绝）
```

**时间成本**：架构决策 2 小时 + 文档改写 4-6 小时

---

### 5. **删除"为未来的预留机制"，改为"按需补充"**

**当前浪费的预留**：
- ImageAssetKind 的 Svg / Video / Hdr 三值（无 feature request）
- FontSourceKind 的 VariableEmbedded（无 feature request）
- textModifier / RangeSelector Tag code（动画还没启动）
- Tag 版本化②路径（无真实 use case）

**决策**：
1. 删掉这些预留值的定义和文档
2. 建立"Roadmap → Feature Request → API Extension"的正式流程，而不是"提前预留"
3. 当真正需要时，做 RFC 讨论后补充（此时有具体 context）

**时间成本**：决策 1 小时 + 编辑 3-4 小时

---

## 九、技术债清单（非阻塞，后续优化）

| 债项 | 优先级 | 工作量 | 建议做法 |
|-----|--------|--------|---------|
| EncodeStream estimateBodySize 算法未落地 | P1 | 3h | 补 EstimateSize.h 伪码 + 单测 |
| Baker/Inflater setter 序列同构性无验证 | P1 | 5h | 补 LayerBuilder_Alignment.md + ParityTest |
| ResourceBaker 三层优化无 benchmark | P1 | 6h | 补 benchmark_resource_dedup + 性能门槛 |
| CurrentStreamScope 复制代码（CompositionVisitScope） | P2 | 2h | 改用通用 ScopeGuard<T> 模板 |
| Diagnostic contextIndex 映射表在设计文档中 | P2 | 2h | 迁移到 Appendix G.4 |
| "25 条压缩专项测试"具体内容未列 | P2 | 3h | 补充详细的单测清单 |
| LayerBuilder 对齐检查清单缺失 | P1 | 4h | 补 LayerBuilder_Alignment.md |

---

## 十、总结建议

### 立即行动（当前评审周期）

1. **删 3 样东西**：
   - DiagnosticCollector 基类 → 各 Context 独立定义（-50 行）
   - ② Tag 版本化规范 → 仅保留①③（-40 行）
   - hasExt bit → 删掉未用的机制（-20 行）
   - **合计 -100+ 行，零功能损失**

2. **补 3 份文档**：
   - LayerBuilder_Alignment.md（Baker/Inflater 同构检查清单）
   - 附录 D 的 ColorSpace 边界处理伪码
   - §18.4bis 的详细单测清单

3. **修 3 个 P0 问题**：
   - EncodeSession 的完整签名和默认行为
   - Baker 错误路径的 unique_ptr 转移完整伪码
   - PackedLayerPath 深度截断的消歧

### 后续优化（Phase 1 之后）

1. 补充 ResourceBaker 性能 benchmark（支撑三层 map 设计）
2. 补充 Baker/Inflater ParityTest（验证"完全同构"声称）
3. 消除重复的 RAII guard（用通用模板）

### 版本化建议

- **v2.3** 目标：删掉过度设计，精简到 6500 行（当前 7086 行）
- **v3.0** 路线：升 FORMAT_VERSION，统一 Property encoding 扩展规范

---

