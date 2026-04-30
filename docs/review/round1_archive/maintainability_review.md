# 可维护性评审报告

**评审员角色**：可维护性守门员  
**文档版本**：v2.19  
**评审日期**：2026-04-30  
**文档行数**：7086  
**状态**：发现 P0 结构问题 + P1 多处同步风险

---

## 一、整体评分

| 维度 | 评分（★1-5） | 关键问题 |
|-----|-----|-----|
| 内部一致性 | ★2 | Property<T> 三处重复、术语定义位置混乱、ErrorCode与阶段表割裂 |
| 命名一致性 | ★3 | 格式版本号说明混散、Context 命名不透明 |
| 伪代码可执行性 | ★3 | 部分伪代码缺关键函数定义、缺少错误路径明确化 |
| 多处同步风险 | ★2 | 无"单一真相源"、7 处需同步修改的位置无关联注记 |
| 文档可读性 | ★2 | 补丁章节（bis/ter/quater）零散、目录结构需重构、体量过大 |

**综合评分**：★2.4 / 5.0  
**发布风险**：**高风险**（多处一致性问题，易引入 AI 落代码偏差）

---

## 二、自相矛盾/不一致之处（P0）

### 不一致 #1：Property<T> 编码规则的三处重复定义

**位置 A**：§4.3 第 205-258 行  
说法 A：propHeader 位域 8 bit，bit[0-3]=encoding (4 bit)，bit 4=isDefault，bit 5=hasExt，bit 6-7=reserved

**位置 B**：§5.2 第 301-309 行  
说法 B："所有**可动画字段**以 `Property<T>` 包装，承载 `encoding` 标记位 + 常量 `value`（本期 encoding 恒为 Constant）"

**位置 C**：§13.1 第 1551-1558 行  
说法 C："Property<T> 壳：每个可动画字段已是 Property<T>，字节流前缀 1 byte `propHeader` 位域"

**矛盾点**：  
- 三处都在讲 Property<T> 编码，但粒度不同
- §4.3 是字节布局细节（bit 级）
- §5.2 是内存模型抽象
- §13.1 是动画扩展保留点
- **缺少"单一权威源"**：修改 propHeader 格式时，要同步 3 处

**建议**：
1. 在 §4.3 后加 **"§4.3.1 小节：propHeader 权威定义"**，明确为唯一真相源
2. §5.2 改为："Property<T> 是 §4.3 中 propHeader + value 的 C++ 包装"（引用 §4.3）
3. §13.1 改为："Property<T> 未来扩展入口见 §4.3 的 encoding 位"（引用 §4.3）
4. 在 §4.3 标题旁加红字："⚠ 唯一权威定义，修改需同步至 §5.2 / §13.1"

---

### 不一致 #2：EncodeSession 的定义时机错误

**位置 A**：§3.1 第 105 行  
说法 A："EncodeSession 新增（v2.19 P0-D：DiagnosticCollector* + StreamContext* 2 指针聚合）"  
（此处假设 EncodeSession 已定义）

**位置 B**：§3.2 第 127 行  
说法 B："3 Context + 1 EncodeSession（v2.19 P0-D 钉死；...）Encode 阶段直接用 EncodeSession 2 指针聚合"

**位置 C**：§8.5 第 701 行  
定义位置 C："**EncodeSession 聚合体**（v2.19 P0-D 钉死）...  
```cpp  
struct EncodeSession { DiagnosticCollector* diag; StreamContext* sc; };  
```"

**矛盾点**：  
- §3.2 的表格说"EncodeSession 用于 Encode"，但未在此表右侧标注"定义在 §8.5"
- 术语索引 §3.3 的表格（L135-165）中**漏掉了 EncodeSession 行**！
  - 表中有 `BakeContext`（L151）、`DecodeContext`（L152）、`InflaterContext`（L153）、`DiagnosticCollector`（L154）
  - **唯独无 EncodeSession**
- 新读者读 §3.2 后想找 EncodeSession 的权威定义，查 §3.3 术语表找不到

**建议**：
1. §3.3 术语表中加一行：`EncodeSession | Codec Encode 阶段轻量聚合（DiagnosticCollector + StreamContext） | §8.5`
2. §3.2 表格说"定义在 §8.5"处加超链接或 ↑ 箭头指向 §8.5

---

### 不一致 #3：错误码分段与实现阶段表无对应关系

**位置 A**：§G.2 第 5833 行  
错误码分段：
```
Baker: 100-199 (fatal) + 200-299 (warn)
Codec: 300-399 (fatal) + 400-499 (warn)
Inflater: 500-599 (fatal) + 600-699 (warn)
```

**位置 B**：§G.6 第 6317 行  
测试断言与覆盖矩阵：按阶段列表，如 "Phase 4 只覆盖 300/301/302/303/305/306/307（Codec fatal）"

**位置 C**：§19 阶段表  
阶段 0-13 的实现顺序，每阶段涉及不同 Tag 和对应错误码

**矛盾点**：  
- 想快速回答"ErrorCode::CompositionCycleDepth 属于哪个阶段"需要：
  1. 查 §G.2 发现它是 100-199（Baker fatal）
  2. 再查 §G.6 对应的 Baker 行
  3. 再查 §19 查 Baker 在哪个阶段
  - **三层关联，无直接映射表**
- 新人不知道改一个错误码会导致"需要更新 §G.2、§G.6、§19 三处"

**建议**：
1. 在 §G.2 后追加 **"§G.2.1 错误码→阶段映射快速查表"**：
   ```
   | ErrorCode | 码段 | 触发点 | 最早实现阶段 |
   | CompositionCycleDepth | 114 | Baker | Phase 2 |
   | ... |
   ```
2. §G.6 表格加一列 "错误码范围"（如 "100-107, 114, 121-199"）
3. §19 每个阶段加脚注：本阶段需测试覆盖的错误码范围（如"Phase 2: Baker 100-107, 114"）

---

### 不一致 #4：FORMAT_VERSION 与 KnownVersion 的版本号管理未集中说明

**位置 A**：§4.1 第 186-187 行
```cpp
constexpr uint8_t pagx::pag::FORMAT_VERSION = 0x02;
constexpr uint8_t pagx::pag::KnownVersion = 0x02;
```

**位置 B**：§4.1 第 189-190 行  
"v1 播放器改动：... 本期**不修改 v1 播放器**"

**位置 C**：§4.1 第 187 行末尾  
"升级周期中可能出现'Reader 识别 0x03 但仍写 0x02 保持后向兼容'的过渡期"

**矛盾点**：  
- 三句分散在不同位置，没有"版本管理决策表"
- 新开发者改 FORMAT_VERSION 到 0x03 时，需要同时：
  1. 修改两处常量（§4.1）
  2. 检查 v1 播放器是否需要改（§4.1 L189）
  3. 理解"Reader 识别策略"（§4.1 L187）
  4. 修改 Decoder 的版本检查逻辑（§8.3）
  - **没有 checklist**

**建议**：
1. 在 §4.1 容器头小节后加 **"§4.1.1 版本管理策略"** 小节（4-5 行）
2. 内容：
   ```
   FORMAT_VERSION 与 KnownVersion 同时升级。升级时改动清单：
   - (1) src/pagx/pag/TagCode.h：FORMAT_VERSION 和 KnownVersion ++
   - (2) src/codec/Codec.cpp：ReadBodyBytes 的 KnownVersion 检查（确保 v1 playback graceful reject）
   - (3) 文档 §4.1 本段两处常量值
   - (4) 若改为 0x03，所有 Reader 按 "version > KnownVersion" reject，不尝试解码
   ```

---

### 不一致 #5：LayerBuilder.cpp 行号映射缺失完整性检查

**位置 A**：§3.2 第 131 行  
"Baker 与 LayerInflater **以 `LayerBuilder.cpp` 为共同蓝本**"

**位置 B**：§20 附录 A "Baker / Inflater / LayerBuilder 行号映射索引"  
（标题暗示三者有行号映射表，但文档中未给出具体内容）

**位置 C**：§2.1 最后  
"已知已修复点：§D.2.1 `checkCoord`、§D.2.2 `popPt`、..."（引用伪码位置而非 .cpp 行号）

**矛盾点**：  
- 附录 A 的标题承诺"行号映射"，但搜索整文档无该附录的具体展开
- §2.1 最后的"已知已修复点"只给伪码位置（§D.2.1），不给 LayerBuilder.cpp 实际行号
- 维护时无法快速验证"Baker 伪码与 LayerBuilder 实现的对应关系"

**建议**：
1. 补充 §20（附录 A）的完整内容：
   ```
   ### 附录 A：Baker / Inflater / LayerBuilder 行号映射索引
   
   本附录列举核心映射算法的三处实现对应关系。修改时需同步三处。
   
   | 算法 | LayerBuilder.cpp | Baker 伪码位置 | Inflater 伪码位置 |
   | convertPath 量化 | L 341-370 | §D.2.2 popPt | §9.2 Pass 1 |
   | Color 量化（带 Precision） | L 401-425 | §D.2.2 readIntX | §9.2 Pass 1 applyColor |
   | Layer matrix 叠加 | L 456-480 | 无（Baker 按原始值） | §9.3 Apply matrix |
   | ... |
   ```
2. §2.1 最后补充：
   "**维护纪律**：若修改 LayerBuilder.cpp 的上述函数，须同步检查 Baker.cpp / LayerInflater.cpp 对应伪码，并跑 §18.3 InflaterParityTest 全绿"

---

### 不一致 #6：DiagnosticCollector 三层继承体系在不同章节说法不一

**位置 A**：§3.2 第 127 行  
"3 Context 全部继承 `DiagnosticCollector` 基类（P0-3 v2.18 抽取 + P0-C v2.19 钉死契约）"

**位置 B**：§8.5 第 698 行  
"抽取为 **`struct DiagnosticCollector`** 基类，**只暴露 protected helper**"

**位置 C**：§8.5 第 700 行  
"**`hasError()` 归子类**：InflaterContext 规定'无 fatal'——物理上把 `errors` 字段 + `hasError()` 方法声明在 BakeContext / DecodeContext 侧，**不下沉基类**"

**矛盾点**：  
- B 说 DiagnosticCollector 是"结构体"（struct），但 C 说"有虚 override"（virtual pushError）
- InflaterContext 不拥有 `errors` 字段，那么继承关系是什么？§8.5 伪码中 InflaterContext 继承了 DiagnosticCollector，后者有 `warnings` 字段，那 InflaterContext 何时警告不会意外调到 `errors`？
- 没有给出明确的继承树图

**建议**：
在 §8.5 DiagnosticCollector 定义前加一个**继承树小节**（3-5 行 + 1 图）：
```
架构图：
        DiagnosticCollector （warnings vector only）
              /        \
             /          \
        BakeContext   DecodeContext     InflaterContext（同样继承 DC，无 errors）
       (errors field)  (errors field)    (无 errors field，delete errors 方法以防误用)
```

---

## 三、命名问题

### 问题 #1：`hasError()` vs `hasFatal()` 命名不对称

**位置**：§8.5 第 770 行和第 892 行

BakeContext / DecodeContext 使用：
```cpp
bool hasError() const { return !errors.empty(); }  // DecodeContext L770
bool hasFatal() const { return !errors.empty(); }  // BakeContext L892
```

**问题**：
- 同一个语义（是否有 fatal），不同的方法名
- API 用户记不住哪个用 hasError、哪个用 hasFatal
- 混用时容易出现编译不过或语义错误

**建议**：
统一为 `hasError()`，在代码注释中说明"返回 true 表示有 fatal"（ErrorCode 100-199/300-399 段为 fatal）

---

### 问题 #2：`encoding` vs `propHeader.encoding` 的名称混淆

**位置**：§4.3 和 §D.1 多处

文档有时说"encoding 位"，有时说"propHeader.encoding"，有时直接用"encoding"。

**问题**：
- 伪代码中有 `if (encoding == Constant)` （§4.4 L261），容易被理解为全局变量
- 应该明确是 `propHeader.encoding` 的子字段

**建议**：
在 §4.3 第一次提到时用完整名 `propHeader.encoding`，后续可简写为 `encoding`。但伪码中保持完整名以避免混淆。

---

## 四、伪代码问题

### 问题 #1：§D.2.2 `popPt` 函数未定义，伪码引用不存在的工具函数

**位置**：§D.2.2 L5413 附近（根据文本片段推测）

伪码引用 `popPt(...)` 但全文档未见定义。

**问题**：
- 不清楚 popPt 的签名、返回值、边界条件
- AI 实现时可能猜错逻辑

**建议**：
补充 popPt 的完整伪码定义：
```cpp
static std::pair<int32_t, int32_t> popPt(const Path& path, uint32_t idx, uint32_t precision) {
  // 从 path 的第 idx 个点读出坐标，按 precision 反量化
  // 返回 (x, y) 坐标对
}
```

---

### 问题 #2：错误路径处理不明确

**位置**：§8.2 Encode 流程伪码、§8.3 Decode 流程伪码

伪码中有 `ctx->error(...)` 调用，但缺少对应的"返回值决策"说明。

**问题**：
- 不清楚 error 调用后是否立即 return？是否继续处理下一个 Tag？
- Baker 的伪码中没有体现"error 后 return 防止下游阶段看到坏数据"的模式

**建议**：
每个 Read/Write 函数的伪码结尾加：
```cpp
// 错误处理纪律：
// - ctx->error(...) 后不 return，继续读取下一字段（允许多个 error 累积）
// - ctx->hasError() 返回 true 时，调用侧负责中止当前处理链（参见 §8.3 主循环）
```

---

## 五、单一真相源缺失点

### 缺失点 #1：ImageAsset / FontAsset 的"生命周期金标准"在哪里？

改 `ImageAsset::data` 的内存来源时，需要同步修改：
1. §C.6 `ImageAsset` 结构体定义
2. §11.1 资源管理章节的说明
3. §18 测试方案中 ImageBytesReleasedAfterInflate 的断言逻辑
4. Baker 代码中的资源索引构造（§7.2）
5. Inflater 代码中的 MakeFromEncoded 后 data.reset() 逻辑（§11.1）

**建议**：
新增 **§11.0 "资源管理全局决策表"** 一节：

| 资源类型 | 所有权 | 内存形式 | 生命周期 |
|---------|------|---------|--------|
| ImageAsset::data | PAGDocument 独占 | `shared_ptr<const tgfx::Data>` | Decoder 至 Inflater MakeFromEncoded 成功后 release |
| FontAsset::data | PAGDocument 独占 | `shared_ptr<const tgfx::Data>` | 同上 |
| ... | | | |

所有引用资源管理的章节都指向本表。

---

### 缺失点 #2：Tag 编号分配未集中管理

改 Tag 编号或加新 Tag 时，需要同步：
1. §D.3 TagCode 枚举（行号 N）
2. §6.1 TagCode 分段说明（行号 M）
3. 具体 Tag 的字节布局定义（§D.5-§D.13 多处）
4. Decoder Tag dispatch 表（实现代码）
5. Encoder Tag write 函数列表（实现代码）

**建议**：
新增 **§6.0 "Tag 分配表（权威来源）"** 一节，列出所有 Tag 的编号、名称、格式版本：

```
| TagCode | Name | Value | 引入版本 | Format 定义位置 |
| FileHeader | 1 | 0x01 | v2.0 | §D.5 |
| ... | ... | ... | ... | ... |
```

所有提到具体 Tag 的地方都引用本表行号，而不是硬编码数字。

---

## 六、文档结构优化建议

### 问题 #1：bis/ter/quater 补丁章节破坏文档结构

当前结构：
```
8. Codec 设计
  8.1 对外（内部头）接口
  8.2 Encode 流程
  8.3 Decode 流程
  8.3bis 错误路径内存管理
  8.3ter Baker fatal → Codec 字节流污染预警
  8.4 防御性
  8.5 DiagnosticCollector / DecodeContext / ...
```

问题：
- 二级标题非线性（8.1 → 8.2 → 8.3 → 8.3bis ← 非标准 ← 8.3ter ← 同理 ← 8.4）
- 阅读者无法通过标题快速预判内容顺序
- 目录生成工具无法正常处理

**建议**：
在稳定版发布前，将文档重构为标准三级嵌套：
```
8. Codec 设计
  8.1 接口与流程
    8.1.1 对外（内部头）接口
    8.1.2 Encode 流程
    8.1.3 Decode 流程
  8.2 错误处理与防御
    8.2.1 错误路径内存管理
    8.2.2 Baker fatal 污染预警
    8.2.3 防御性编程
  8.3 Context 设计
    8.3.1 DiagnosticCollector 基类
    8.3.2 DecodeContext
    8.3.3 BakeContext
    8.3.4 EncodeSession
```

目前先加**补丁说明**（文档顶部）：
```
## 读者须知（临时）
本文档因评审批次众多，使用 §N.Nbis/ter/quater 补丁标记非标准章节。
完整章节列表请用 `grep -n "^### " docs/pagx_to_pag_v2_design.md` 生成。
```

---

### 问题 #2：7086 行的体量难以维护

按附录占比：
- 主文档（§1-§19）约 3000 行
- 附录（§20-§28）约 4086 行（占 58%）

问题：
- 附录肥大化，导致主文档与细节无法快速定位对应关系
- 修改主要逻辑时需要反复跳转附录

**建议**（不阻塞当前版本）：
- 短期：为每个附录生成"索引超链接"，主文档中引用附录时改为"见 [附录 C](#appendix-c)"
- 中期：把 §20 "附录 A 行号映射" 拆为独立文件 `.github/docs/line_mappings.md`
- 长期：考虑拆分为"设计文档（§1-§19）+ 参考手册（附录）"两个文档

---

### 问题 #3：新读者快速上手路径不清晰

**建议**：
在文档开头加"**快速导航**"（第一屏）：

```markdown
## 快速导航

**我想...**

- ...理解整体架构 → 先读 **§3.2（4 态 3 映射）** 再读 **§3.3（术语索引）**
- ...实现 Codec → 读 **§8**，细节看 **§D**
- ...实现 Baker → 读 **§7**，细节看 **附录 C**
- ...理解错误处理 → 读 **§8.5**（Context 设计）+ **§G**（错误码）
- ...新增一个 Tag → 按 **§6.0 Tag 分配表**（新增）改四处
- ...修改 Property<T> 编码 → 按 **§4.3**（权威源）同步 §5.2 / §13.1

**完整阅读顺序**（AI 从零开始）：
1. §1（背景）→ §2（设计原则）→ §3（架构）
2. 选择你的角色：
   - **Baker 实现者**：§7 + §C（数据模型）+ §D.8-11（Tag 布局）
   - **Codec 实现者**：§8 + §D（字节布局）+ §G（错误码）
   - **Inflater 实现者**：§9 + §10-13（特殊场景）+ §15.3（对外接口）
   - **测试设计**：§18 + §G.6（测试矩阵）
3. 遇到 Context / Property / Tag 定义时，查 §3.3 快速定位权威章节

**关键符号**：
- ⚠ = 维护陷阱（改这里需同步其他地方）
- § = 内部交叉引用
- P0/P1/P2 = 优先级
- bis/ter/quater = 补丁章节（与前一章同级）
```

---

## 七、项目代码交叉验证结果

| 文档引用 | 文档位置 | 实际代码位置 | 验证结果 |
|---------|---------|-----------|---------|
| StreamContext.errorMessages | §8.5 L788-790 | src/codec/utils/StreamContext.h:46 | ✅ 存在 |
| LayerBuilder.cpp 929 行 | §20 | src/renderer/LayerBuilder.cpp:929 | ✅ 行数匹配（检查时） |
| /src/pagx/pag/Baker.cpp | §3.3 L148 | **❌ 不存在** | 这是待建设计文档 |
| /src/pagx/pag/Codec.cpp | §3.3 L149 | **❌ 不存在** | 这是待建设计文档 |
| /src/pagx/pag/LayerInflater.cpp | §3.3 L150 | **❌ 不存在** | 这是待建设计文档 |
| /src/pagx/pag/TagCode.h | §4.1 L186 | **❌ 不存在** | 待建 |
| /src/pagx/pag/ValueCodec.h | §4.3 L237-246 | **❌ 不存在** | 待建 |
| /src/pagx/pag/ErrorCode.h | §15.1 L1641 | **❌ 不存在** | 待建 |
| tgfx::Layer | §3.2 L126 | ../tgfx/include/tgfx/layers/Layer.h | ✅ 存在（libpag 依赖） |
| tgfx::DisplayList | §3.1 L117 | ../tgfx/include/tgfx/gpu/DisplayList.h | ✅ 存在 |
| DiagnosticCollector 伪码 | §8.5 L710-731 | 待实现（phase 0） | ⏳ 占位符 |
| EncodeSession 伪码 | §8.5 L735-740 | 待实现（phase 0） | ⏳ 占位符 |
| DecodeContext 伪码 | §8.5 L744-802 | 待实现（phase 4） | ⏳ 占位符 |

**结论**：
- 文档是"设计蓝本"，不是"现有代码的文档化"
- 所有 `src/pagx/pag/` 下的文件待建，这是正常的
- 但需要在文档顶部明确声明"本文档是 v2.0 新增设计，实现未开工"
- 所有交叉引用（tgfx）都验证通过 ✅

---

## 八、给主决策者的核心建议

### ✅ 保持现状的优点
1. **架构清晰**：三态映射、四大 Context 的设计原理完整
2. **安全考虑充分**：P0-S1 至 P1-S5 的安全加固（18 个 CVE 防护点）完整列举
3. **测试驱动**：§18-§19 的阶段切分与测试矩阵细致（110 条测试、12 个测试文件）
4. **向前兼容**：§4.4 规则 1-3、§6.5 三级兼容机制设计前瞻性强

### 🔴 P0 必修改进（发布前必做）
1. **加"文档快速导航"**（第一屏，5-10 行）
   - 为不同角色（Baker/Codec/Inflater 实现者）指引阅读路径
   - 加"关键符号解释"（⚠ / § / bis/ter/quater）
   
2. **补三个"权威源表"**（新增 §4.1.1 / §6.0 / §11.0）
   - 版本管理决策表（FORMAT_VERSION / KnownVersion 修改 checklist）
   - Tag 分配表（TagCode 枚举权威源）
   - 资源生命周期表（ImageAsset / FontAsset 改动清单）
   
3. **修复 §3.3 术语表**（加 EncodeSession 行）
   
4. **补附录 A 具体内容**（Baker/Inflater/LayerBuilder 行号映射）

5. **加文档顶部声明**
   ```markdown
   ## 文档性质声明（必读）
   
   本文档是 **PAGX → PAG v2 转换模块的设计蓝本**，包含：
   - ✅ 确定的架构决策和字节布局规范（§1-§6）
   - ✅ 伪代码算法规范（§7-§13）
   - ✅ 测试/阶段计划（§18-§19）
   - ⏳ 待建设计（所有 `src/pagx/pag/` 下的源代码实现尚未开工）
   
   **维护纪律**：本文档每 PR 需同步更新附录 A（行号映射），phase 0 交付前需补三个权威源表。
   ```

### 🟠 P1 建议改进（发布前可选、近期需改）
1. **补伪代码关键函数定义**（popPt、checkCoord 等具体实现）
2. **补"修改清单"注记**：每个多处同步的位置旁加 ⚠ 符号 + 超链接指向关联位置
3. **整理 bis/ter/quater 补丁**：为稳定版制作"标准三级嵌套版本"（§8 → §8.1/8.2/8.3 等）
4. **补错误路径说明**：伪码中体现"error 调用后的返回策略"

### 🟢 P2 长期改进（下个版本考虑）
1. 拆分为"设计文档 + 参考手册"
2. 将行号映射表独立为 `.github/docs/line_mappings.json`（支持自动化验证）
3. 生成交互式"改动影响分析工具"（改 Property<T> 时自动列出影响的 7 处位置）

---

## 九、如何使用本评审结果

### 对 tech lead
- **即刻行动**：按 P0 清单补"文档快速导航"和三个权威源表（1-2 工作日）
- **code review 要点**：审查 PR 时检查"修改附录 A 行号映射"和"同步三个权威源表"
- **验收标准**：Phase 1 exit gate 前需跑"文档一致性检查脚本"（见下）

### 对 AI 实现者
- **阅读路径**：按"快速导航"选择你的角色并跳转对应章节
- **修改时**：遇到 ⚠ 符号的位置，查附录确认需要同步哪些地方
- **不确定时**：查 §3.3 术语表或 §6.0/§11.0 权威源表，而非按个人理解实现

### 对下一轮评审者
- 新增"维护记录表"（文档末尾），记录每个 PR 涉及的同步修改点
- 改 TAG / Property / Context 后需一键检查"影响分析"

---

## 附：自动化检查脚本建议

**脚本名**：`.github/scripts/doc_consistency_check.sh`

```bash
#!/bin/bash
# 文档一致性检查（Phase 1 exit gate）

# 1. Property<T> 多处一致性检查
if ! diff <(grep -n "propHeader" docs/pagx_to_pag_v2_design.md | grep "§4.3") \
          <(grep -n "propHeader" docs/pagx_to_pag_v2_design.md | grep "§5.2"); then
    echo "❌ Property<T> 定义不一致（§4.3 vs §5.2）"
    exit 1
fi

# 2. §3.3 术语表完整性检查
for term in "BakeContext" "DecodeContext" "InflaterContext" "EncodeSession"; do
    if ! grep -q "$term" docs/pagx_to_pag_v2_design.md | grep "§3.3"; then
        echo "❌ 术语表缺 $term（§3.3）"
        exit 1
    fi
done

# 3. 附录 A 行号有效性检查
grep "^LayerBuilder.cpp:" docs/pagx_to_pag_v2_design.md | while read -r line; do
    lineno=$(echo "$line" | cut -d: -f2)
    if [ "$lineno" -gt 929 ]; then
        echo "❌ 附录 A 行号超限（L$lineno > 929）"
        exit 1
    fi
done

echo "✅ 文档一致性检查全绿"
```

---

## 总结

本文档经过 19 个版本的迭代评审，架构决策和编码规范已相当完整。但**一致性缺陷**和**维护风险**的集中度较高，主要原因是：
1. 多轮评审的补丁积累（bis/ter/quater）导致结构零散
2. 缺少"权威源表"和"改动清单"的集中管理
3. 术语索引、错误码映射等关键查表不完整

**立即执行的改进建议**（预计 1-2 工作日）：
- 补"快速导航"和三个权威源表（§4.1.1 / §6.0 / §11.0）
- 修复 §3.3 术语表（加 EncodeSession）
- 加文档顶部"性质声明"
- 补附录 A 具体内容

这些改进将**显著降低 AI 实现时的偏差风险**，使代码更新维护时能快速定位同步位置。

**文档维护友好性最终评分改进**：★2.4 → ★3.8（待改进项完成后）

---

**评审完成时间**：2026-04-30 23:58  
**下次定期评审**：Phase 1 exit gate（建议加"文档一致性检查"作为 exit gate 的一部分）

