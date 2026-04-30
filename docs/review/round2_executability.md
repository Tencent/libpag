# 开工可行性评审报告

**评审员**：开工可行性评审员  
**文档版本**：v2.20  
**评审日期**：2026-04-30  
**核心问题**：明天开工，能否按期交付可工作的 v2 编解码？

---

## 一、总体判断

**可开工度**：★★★★☆ (4/5 — 高度可行，前置条件基本备齐，工程障碍清晰可解)

**预估交付准确性**：**合理** 
- 文档完整性：★★★★★ （6710行，细节充分，无重大歧义）
- 实施复杂度可估计：★★★★☆ （14阶段设计清晰，但涉及31个新文件、4个新编码格式）
- 前置依赖就绪度：★★★☆☆ （65% 可用，35% 需创建或补齐）

**前置条件完备度**：⚠️ **部分完备**
- ✅ 完备项：文档、设计、tgfx依赖、项目规范
- ❌ 缺失项：`src/pagx/pag/` 目录、CMake target、测试基建目录
- ⚠️ 需补齐项：GlyphRun序列化参考代码位置确认

**核心障碍 Top 3**（可解决，非阻塞）：

1. **Path双格式切换的工程复杂度** （§D.2 量化编码）
   - 涉及动态位宽、uint24打包、溢出防护（int64中间结果）
   - 选择策略有阈值（QUANTIZATION_MIN_VERBS=3、QUANTIZATION_MAX_COORD=2^23）
   - **建议**：阶段5.5额外拆分Path编码单测（ValueCodecTest::PathQuantization*）

2. **GlyphRun三种编码路径的排列组合测试空间** （§D.11 GlyphRunBlob）
   - kind=LayoutRun vs ClassicGlyphRun，位字段压缩，xform可选性
   - **建议**：单测参数化（GlyphRunBlobCodecTest 用 Cartesian product 编织参数组合）

3. **Mask两趟处理与循环检测的同步设计** （§9 Inflater + §12 Mask）
   - Pass 1（Layer树编码）与 Pass 2（Mask路径解析）需保持强一致性
   - 需防止 DAG 变图时的引用悬垂
   - **建议**：先落 LayerInflater 框架，Pass 2 单独 checkpoint（相当于 Phase 9 中阶段）

---

## 二、开工前置条件 checklist

| 前置项 | 当前状态 | 位置 | 建议 |
|-------|--------|-----|-----|
| **目录结构** | | | |
| src/pagx/pag/ | ❌ 不存在 | — | 开工前必创建；建议包含 10 个 subdir（见下表） |
| include/pagx/pag/ | ✅ 头文件dir存在 | src/pagx/ | 无需创建，内部头直接在 src/pagx/pag/*.h |
| test/src/pag/ | ⚠️ 树存在，为空 | test/src/pag/ | 需创建5个subdir（见§16） |
| **构建系统** | | | |
| CMakeLists 新增 target | ❌ 未配置 | CMakeLists.txt | 需新增 `pagx-pag` static lib target；见§16 CMake集成段 |
| `include/pagx/*.h` 导出 | ✅ 部分 | include/pagx/ | 现有 PAGXDocument.h 等，v2 需新增 Diagnostic.h/PAGExporter.h/PAGLoader.h 三个公共头 |
| tgfx 链接 PUBLIC | ⚠️ 需确认 | CMakeLists.txt | `pagx-pag` 库应依赖 `tgfx` 作 PUBLIC 依赖（PAGLoader.h 暴露 tgfx::Layer） |
| **测试基建** | | | |
| test/src/pag/support/ | ❌ 无 | — | 需创建；交付 RenderUtil.h/cpp、SampleEnumerator.h/cpp、PAGXBuilder.h/cpp、StructBuilders.h/cpp |
| test/src/pag/unit/ | ❌ 无 | — | 需创建；15+ 单测文件 |
| test/src/pag/codec/ | ❌ 无 | — | 需创建；8 个压缩机制单测文件 |
| test/src/pag/integration/ | ❌ 无 | — | 需创建；5 个集成测试文件 |
| test/src/pag/e2e/ | ❌ 无 | — | 需创建；CLI+端到端3文件 |
| test/src/pag/render/ | ❌ 无 | — | 需创建；RenderEquivalenceTest.cpp（基准图） |
| test/src/pag/perf/ | ❌ 无 | — | 需创建；PerformanceTest.cpp |
| **样本数据** | | | |
| spec/samples/*.pagx | ✅ 存在 | spec/samples/ | 已有~50个PAGX样本；v2不需新增，直接消费 |
| test/baseline/PAGRenderTest_Render/ | ❌ 无基准图 | test/baseline/ | Phase 12首跑会全FAIL；需用户手动 `/accept-baseline` 接受基准 |
| test/baseline/PAGRenderTest_OutlineAll/ | ❌ 无基准图 | test/baseline/ | 同上 |
| test/perf/baseline.json | ❌ 无 | test/perf/ | Phase 14生成 |
| **代码依赖** | | | |
| src/codec/utils/EncodeStream.h | ✅ 可用 | src/codec/utils/ | v1 已有；v2 复用（writeInt32List/writeFloatList/writeUBits） |
| src/codec/utils/DecodeStream.h | ✅ 可用 | src/codec/utils/ | v1 已有；v2 复用 |
| src/codec/TagHeader.cpp 格式 | ✅ 参考可用 | src/codec/TagHeader.cpp:23-48 | TagHeader 字节格式相同，v2独立实现但格式同源 |
| pag::Ratio / pag::Color / tgfx::* | ✅ 可用 | include/pag/file.h 等 | 所有底层类型均可复用 |
| src/renderer/LayerBuilder.cpp | ✅ 可用 | src/renderer/LayerBuilder.cpp:138-801 | Baker/Inflater 的蓝本；不改动，只读参考 |
| ToTGFX.cpp 枚举映射 | ✅ 可用 | src/renderer/ToTGFX.cpp | 所有 PAGX→tgfx 映射已存在，Baker直接调用 |
| test/src/utils/Baseline.h | ✅ 可用 | test/src/utils/ | 渲染测试基建；接受 tgfx::Surface 重载 |
| **工具链** | | | |
| clang-format | ✅ 可用 | .clang-format | 项目已有；`./codeformat.sh` 可用 |
| gtest | ✅ 可用 | CMakeLists.txt | 项目已集成；PAGFullTest target 存在 |
| cmake / ninja | ✅ 可用 | — | 编译验证脚本已定义 |
| **代码规范** | ✅ 完备 | .codebuddy/rules/ | Code.md / Git.md / Test.md 三规范齐备；见 memo |

### 开工第一天需完成的准备

**命令序列**（约 2 小时）：

```bash
# 1. 创建目录骨架
mkdir -p src/pagx/pag/layer_bakers src/pagx/pag/tags
mkdir -p test/src/pag/{support,unit,codec,integration,e2e,render,perf}

# 2. 初始化 Phase 0 必需文件（占位）
touch src/pagx/pag/ErrorCode.h  # Phase 0 alias
touch src/pagx/pag/DiagnosticCollector.h  # 3 Context 基类
touch src/pagx/pag/EncodeSession.h  # Encode 2 指针聚合

# 3. 验证编译环境
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
# 预期：CMakeLists 尚未配置 pagx-pag target，暂失败

# 4. 抽取 GlyphRun 参考位置
grep -n "class GlyphRun" include/pagx/nodes/GlyphRun.h
grep -n "struct.*Blob\|class.*Glyph" src/pagx/TextLayout.h
# → 确认 GlyphRunBlob 的源数据结构位置
```

---

## 三、Phase 拆分评估

### 总体结构

| 维度 | 评价 |
|-----|------|
| Phase 数量 | 15 个（0-14）+ 15(工具) = 结构清晰，量级适中 |
| 依赖关系清晰度 | ⭐⭐⭐⭐⭐ 明确且无环路（见§19 依赖矩阵） |
| 出口条件可验证性 | ⭐⭐⭐⭐ 每个 Phase 关键检查点：测试全绿+特定 grep 门槛 |
| **关键路径** | Phase 0 → 1→2→4（OR） → {5,9} → {10,10.5} → 12 |
| **并行化可能性** | **高**（见下文分析） |

### 关键路径深度分析

**串行必须路径**（最短路径，14 个工作日）：

```
Day 0: Phase 0 (Diagnostic ABI)              ← 所有模块的基础，无法并行
  ↓
Day 1-2: Phase 1 (ValueCodec/TagHeader)      ← Phase 0 blocker
  ↓
Day 3-4: Phase 2 (PAGDocument/BakeContext)   ← Phase 1 blocker  
  ↓
Day 5-7: Phase {3,4,5,6,7,8} (Baker子模块)   ← 可部分并行
  ├─ Phase 3 (LayerBaker)           [并行1]   
  ├─ Phase 4 (Codec基础Tag)         [并行2]
  ├─ Phase 5 (VectorBaker)  ← 需 Phase 3
  ├─ Phase 6-8 (Paint/Style/Text)   [并行3-5]
  ↓ （Phase 5 → 6/7/8）
Day 8-10: Phase 9 (LayerInflater)   ← Phase 4/5/8 blocker
  ↓
Day 11: Phase 10 (PAGExporter)      ← Phase 9 blocker
  ↓
Day 12: Phase 10.5 (PAGLoader)      ← Phase 10/9.5 blocker
  ↓
Day 13: Phase 11 (CLI)              ← Phase 10.5 blocker
  ↓
Day 14-16: Phase 12 (Render + Fuzz) ← Phase 10.5/9.5 blocker
  ↓
Day 17: Phase 13-15 (perf/coverage) ← Phase 12 blocker
```

**关键路径长度**：17 个工作日（对应 3.5 周，如果单人，或 2 周如果 2 人）

### 并行化示意

```
预期 3 人团队分工（假设 Phase 容量均衡）：
┌─────────────────────────────────────────────────────────────────┐
│ Engineer-A: Diagnostic → ValueCodec → PAGDocument → LayerBaker  │
│ (Phase 0, 1, 2, 3)                                              │
└────┬────────────────────────────────────────────────────────────┘
     ↓
┌────┬───────────────────────────────────────────────────────────┐
│ E-B: Codec Tags → ElementTags → VectorBaker                    │
│ (Phase 4, 5)                  ← 需 A 的 Phase 3               │
└────┼───────────────────────────────────────────────────────────┘
     │
┌────┼────────────────────────────────────────────────────────────┐
│ E-C: Paint/Style/TextBaker → LayerInflater                     │
│ (Phase 6, 7, 8, 9)           ← 需 B 的 Phase 5                │
└────┴────────────────────────────────────────────────────────────┘
     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Converge: API + CLI + Render + Perf (Phase 10-15)              │
│ (可选 1-2 人轮流处理，或全员 code review)                       │
└─────────────────────────────────────────────────────────────────┘
```

**并行收益**：
- 单人顺序 17 天 → 3人最优可并 8 天（Critical Path only）
- 实际可达 10-12 天（因为 review/debug 时间）

### Phase 门槛评估

| Phase | 出口条件清晰度 | 检查复杂度 | 建议 |
|-------|-------------|--------|------|
| 0 | ⭐⭐⭐⭐⭐ | 1 个 grep + CodeToString 遍历 | 自动化检查可行 |
| 1 | ⭐⭐⭐⭐ | `grep -n "std::vector<uint8_t>"` | P1 exit gate 自动化 |
| 2 | ⭐⭐⭐⭐ | data 类型迁移+builder fluent smoke | 需手工审查 |
| 3-8 | ⭐⭐⭐ | 测试全绿，无特殊 gate | 依赖 Phase 间顺序 |
| 9 | ⭐⭐⭐⭐ | RoundTrip 往返等价性 | 需覆盖 mask 循环场景 |
| 12 | ⭐⭐ | Render 基准图手工审 | **首跑失败不代表 bug**（需接受基准）|

### 隐藏的阶段依赖

- **Phase 9.5 (RenderUtil)** 虽标注不依赖 Phase 9，但需要"本地tgfx构建"，暗示必须 `DPAG_BUILD_TESTS=ON` 从 Phase 4 起生效
- **CorruptBuilder（Phase 1 产出）** 被 Phase 4/8/9/12 重度使用；Phase 1 必须通过 CorruptBuilderTest smoke 才能锁定 API
- **PAGXBuilder（Phase 2 产出）** 被所有 Baker 单测依赖；Phase 2 fluent API 不稳定会导致后续测试代码反复改写

---

## 四、目录结构完备性

### 需要创建的文件清单（按 Phase）

**Phase 0**（4 文件）：
```
include/pagx/
  └─ Diagnostic.h                         # 对外公共 Diagnostic + DiagnosticCode
src/pagx/
  ├─ Diagnostic.cpp
  └─ pag/
      ├─ ErrorCode.h                      # using alias
      ├─ DiagBuild.h                      # kAllDiagnosticCodes[] 初始化
      └─ DiagnosticCollector.h             # 3 Context 共用基类
```

**Phase 1**（4 文件 + 支撑库）：
```
src/pagx/pag/
  ├─ TagCode.h                            # TagCode 枚举 + FORMAT_VERSION 常量
  ├─ PropertyEncoding.h                   # propHeader 位域 + ReadProperty/WriteProperty 模板
  ├─ ValueCodec.h                         # Read/WriteValue<T> 特化（20+ 种 T）
  └─ EncodeSession.h                      # struct EncodeSession { diag*, streamCtx* }
test/src/pag/support/
  ├─ CorruptBuilder.h/cpp                 # 畸变构造器（为单测注入错误）
  └─ (占位) PAGXBuilder.h/cpp、StructBuilders.h/cpp（Phase 2 交付）
```

**Phase 2**（8 文件）：
```
src/pagx/pag/
  ├─ PAGDocument.h                        # 全量数据模型定义（C.1-C.9）
  ├─ BakeContext.h/cpp                    # 资源索引 + 错误收集上下文
  └─ layer_bakers/
      └─ ResourceBaker.cpp                # 图片/字体去重收集
test/src/pag/support/
  ├─ PAGXBuilder.h/cpp                    # 单测 DOM fluent builder
  ├─ StructBuilders.h/cpp                 # MakeDeepLayerStack 等构造
  └─ PAGDocumentEquals.h/cpp              # 深度字段等价性判断
```

**Phase 3-8**（20 文件）：
```
src/pagx/pag/
  ├─ Baker.h/cpp                          # 顶层编排
  ├─ layer_bakers/
  │   ├─ LayerBaker.cpp                   # 通用字段 + Composition
  │   ├─ VectorBaker.cpp                  # VectorLayer + 14 Element
  │   ├─ TextBaker.cpp                    # 双模式文本
  │   ├─ StyleFilterBaker.cpp             # 3 Style + 5 Filter
  │   └─ ResourceBaker.cpp 续              # (Phase 2 开始，3-8 补齐)
  ├─ Codec.h/cpp                          # Encode/Decode 主逻辑
  ├─ DecodeContext.h/cpp                  # Decode 阶段 Context
  └─ tags/
      ├─ FileHeaderTag.cpp
      ├─ CompositionTag.cpp
      ├─ LayerBlockTag.cpp
      ├─ VectorPayloadTag.cpp
      ├─ CompositionRefPayloadTag.cpp
      ├─ ElementTags.cpp                  # 14 种 element
      ├─ FilterTags.cpp                   # 5 filter
      ├─ StyleTags.cpp                    # 3 style
      ├─ ResourceTags.cpp                 # Image/FontAssetTable
      └─ (占位) ShapePayloadTag 等 4 个  # 本期 no-op，留位
```

**Phase 9**（2 文件）：
```
src/pagx/pag/
  ├─ LayerInflater.h/cpp                  # PAGDocument → tgfx::Layer
  └─ InflaterContext.h                    # Inflater 运行时状态
```

**Phase 9.5**（1 文件）：
```
test/src/pag/support/
  └─ RenderUtil.h/cpp                     # 渲染 helper
```

**Phase 10+**（7 文件）：
```
include/pagx/
  ├─ PAGExporter.h
  └─ PAGLoader.h
src/pagx/pag/
  ├─ PAGExporter.cpp
  ├─ PAGLoader.cpp
  └─ DecodeContext.cpp 续
src/cli/
  └─ CommandExport.cpp 续                  # --format pag 扩展
```

**测试文件** (45+ 文件)：见 §16 test 树完整清单

### 职责清晰度评价

| 文件 / 模块 | 职责明确度 | 关键约束 |
|-----------|----------|--------|
| PAGDocument.h | ⭐⭐⭐⭐⭐ | 完整定义在 §C，无歧义；Property<T> 型万级数据模型 |
| Baker.cpp | ⭐⭐⭐⭐ | 映射规则在 §7，但 double-pass mask 处理流程需确认（§12） |
| Codec.cpp | ⭐⭐⭐⭐ | Tag 字节格式清晰（§D），但 forward-compat 规则 4.4 需深读 |
| LayerInflater.cpp | ⭐⭐⭐ | 流程框架给出，但 mask cycle 检测(SCC 算法) 需自行设计 |
| ValueCodec.h | ⭐⭐⭐⭐⭐ | 20+ 种类型的编码特化规则逐一列举；量化编码细节在 D.2 |
| ElementTags.cpp | ⭐⭐⭐ | 14 种 element 字段表完整（D.11），但 inline 嵌套长度包裹易混淆 |

### 命名规范检查

✅ **符合项目规范**（见 Code.md）：
- 类命名大写开头（Baker, LayerInflater, etc）
- 函数名小写开头（bakeLayer, inflateColor, etc）
- 静态常量全大写带下划线（MAX_LAYER_DEPTH, FORMAT_VERSION）
- 无 `k` 前缀，无 lambda 表达式

⚠️ **易出错点**：
- §D.11 双重 length 包裹（外层 TagHeader.length vs 内层 innerLength）→ 变量命名必须区分 `tagLength` vs `styleLength`
- Property<T> 默认值常量 → 不得在 cpp 里硬编码，必须与结构体初始化保持一致

### Include 依赖方向检查

**基本依赖树**（无环）：

```
include/pagx/Diagnostic.h
  ↓
src/pagx/pag/ErrorCode.h (alias)
  ↓
{DiagnosticCollector.h, BakeContext.h, DecodeContext.h, InflaterContext.h}
  ↓
{Baker.h, Codec.h, LayerInflater.h}
  ↓
{PAGExporter.h, PAGLoader.h}
```

**关键约束**（必须遵守）：
1. `include/pagx/PAGLoader.h` 暴露 `tgfx::Layer` → 必须 `#include "tgfx/layers/Layer.h"`
2. `src/pagx/pag/*.h` **不得** include `include/pagx/nodes/XXX.h`（PAGX DOM） → Baker 仅通过 `const pagx::Layer*` 指针消费
3. `LayerInflater.h` **不得** include `Baker.h`（单向依赖 Codec）

---

## 五、接口签名完备性

逐项打分（满分 5）：

| 接口 | 签名 | 契约 | 错误码 | 线程安全 | 生命周期 | **总分** |
|-----|-----|-----|-------|---------|--------|---------|
| `Codec::Encode(const PAGDocument&)` | ✅ | ✅ | ✅ | ✅ | ⚠️ | 4.6/5 |
| `Codec::Decode(const uint8_t*, size_t)` | ✅ | ✅ | ✅ | ✅ | ✅ | 5/5 |
| `Baker::Bake(const PAGXDocument&, BakeContext*)` | ✅ | ✅ | ✅ | ✅ | ✅ | 5/5 |
| `LayerInflater::Inflate(const PAGDocument&, InflaterContext*)` | ✅ | ✅ | ✅ | ⚠️ | ✅ | 4.5/5 |
| `PAGLoader::LoadFromBytes/LoadFromFile` | ✅ | ⚠️ | ✅ | ✅ | ✅ | 4.5/5 |

### 详细评审

#### 1. `Codec::Encode(const PAGDocument& doc) → EncodeResult`

**签名**（§8.4）：
```cpp
struct EncodeResult { 
  std::shared_ptr<tgfx::Data> data;
  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
};
EncodeResult Codec::Encode(const PAGDocument& doc, DiagnosticCollector* diag);
```

**评价**：
- ✅ 签名清晰（输入1，输出1+错误）
- ✅ 契约明确：返回 `nullptr + error` 或 `non-null + warnings`（§15）
- ✅ 错误码段完备（300-399 = Codec fatal, 400-499 = warning，见§G）
- ⚠️ **生命周期问题**：§8.5 P0-D 提及 EncodeSession 聚合 `DiagnosticCollector* + StreamContext*`，但 PAGDocument 本身无所有权标记 → 调用方必须保证 `doc` 在 Encode 期间有效；文档应补充："不转移所有权；调用方负责生命周期"
- ✅ 线程安全：无全局状态修改（`EncodeSession` 栈对象）

**建议**：补充 Encode 签名的 `DiagnosticCollector*` 参数说明文档

#### 2. `Codec::Decode(const uint8_t* data, size_t length) → DecodeResult`

**签名**（§8.4）：
```cpp
struct DecodeResult {
  std::unique_ptr<PAGDocument> doc;
  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
};
DecodeResult Codec::Decode(const uint8_t* data, size_t length, DecodeContext* ctx);
```

**评价**：
- ✅ 完整度：5/5
- ✅ 错误码完备（300-399 分布明确）
- ✅ 安全性：uint64_t 中间结果防 32-bit 溢出（§D.3 repeated)
- ✅ Forward-compat：Unknown Tag Code 400 warn + skip（§4.4 规则 2）
- ✅ 生命周期：输出 `unique_ptr` 明确所有权转移

#### 3. `Baker::Bake(const PAGXDocument& pagx, BakeContext* ctx) → std::unique_ptr<PAGDocument>`

**签名**（§7）：
```cpp
struct BakeResult {
  std::unique_ptr<PAGDocument> doc;
  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
};
BakeResult Baker::Bake(const PAGXDocument& pagx, BakeContext* ctx);
```

**评价**：
- ✅ 完整度：5/5
- ✅ 三步先决条件明确（applyLayout, resolve imports, no cycle）
- ✅ 契约：若任一失败推 error 并返回 `nullptr`
- ✅ 错误码段（100-199 fatal, 200-299 warn）完整覆盖
- ✅ 转移所有权（unique_ptr）

#### 4. `LayerInflater::Inflate(const PAGDocument& doc, InflaterContext* ctx) → LayerInflater::Result`

**签名**（§9）：
```cpp
struct Result {
  std::shared_ptr<tgfx::Layer> layer;
  std::vector<Diagnostic> warnings;
  bool ok;  // true if !layer 且不推 error（空文件合法）
};
Result LayerInflater::Inflate(const PAGDocument& doc, InflaterContext* ctx);
```

**评价**：
- ✅ 返回 layer + warnings 语义清晰
- ✅ `ok` 字段处理空文件场景（§17 C1）
- ⚠️ **线程安全**：Inflater 构造 tgfx::Layer，tgfx 内部是否线程安全？文档应明确"Inflater 线程安全但 layer 共享需外部同步"
- ✅ 生命周期：`shared_ptr<tgfx::Layer>` 清晰

**建议**：补充 Inflater 的线程模型文档

#### 5. `PAGLoader::LoadFromBytes(const uint8_t*, size_t) / LoadFromFile(const char*) → LoadResult`

**签名**（§15）：
```cpp
struct LoadResult {
  std::shared_ptr<tgfx::Layer> layer;
  std::vector<Diagnostic> warnings;
  bool ok;
};
LoadResult PAGLoader::LoadFromBytes(const uint8_t* data, size_t length);
LoadResult PAGLoader::LoadFromFile(const char* filePath);
```

**评价**：
- ✅ 签名简洁（对标 PAGXImporter::FromFile）
- ⚠️ **契约不完整**：文档未明确 `ok = false + layer == nullptr` vs `ok = true + layer == nullptr`（见§15.3）；建议补充表格：

| 场景 | ok | layer | warnings |
|-----|----|----|----------|
| 成功渲染 | true | non-null | empty/partial |
| 空文件（合法） | true | nullptr | [InflaterEmptyDocument] |
| 解码错误 | false | nullptr | [errors+] |
| 加载失败（文件缺失） | false | nullptr | [FileReadFailed] |

- ✅ 错误码：307 (FileReadFailed) 独占，由 PAGLoader 推送
- ⚠️ **生命周期**：Diagnostic 中 `contextIndex` 若为 UINT32_MAX（跨Tag哨兵），调用方需 range-check；文档应补充示例代码（见§15 scenario 3）

---

## 六、TODO / 遗留事项清单

| 事项 | 位置 | 合理延后 vs 应现在决定 | 依据 |
|-----|-----|----------------------|------|
| **动画扩展点** | §13 全章 | ✅ **合理延后** | 本期 0% 实现，字节布局已留坑（AnimationData sub-Tag）；动画期无版本号升级阻力 |
| **ShapePayload / TextPayload / ImagePayload / SolidPayload** | §D.10 | ✅ **合理延后** | PAGX 无对应入口；Tag 枚举值占位已分配（20-23），Reader skip 路径有；未来接入无兼容风险 |
| **MeshPayload** | §D.10 | ✅ **合理延后** | 完全 no-op，Body 为空；标记占位即可 |
| **Variable Font（kind=2）** | §D.6 FontAsset | ✅ **合理延后** | Baker 本期恒写 0；Decoder 遇 kind==2 warn UnsupportedFeature + fallback Embedded；动画期可接入 |
| **OutlineAll 字体模式**（全字体栅格化）| §10 | ✅ **合理延后** | 设计完成（§10 双模式），实现依赖 tgfx TextLayer；Baseline 独立基准图已规划（§18.7） |
| **Path 双格式回退条件** | §D.2 QUANTIZATION_MAX_COORD = 2^23 | ❌ **应现在决定** | 常量值已定（int64 中间结果需 +0.0625 倍）；开工前应验证 PathBuilder::getVerbAt API（若 tgfx 无此则改 PathIterator）|
| **GlyphRun 最小字段集**| §D.11 | ⚠️ **已敲定** | 源码位置已列举（TextLayout.h / GlyphRun.h / GlyphRunRenderer.cpp）；v1.1 Appendix B 确认无需补齐 |
| **ColorMatrix 量化精度** | §D.12 FilterColorMatrix | ⚠️ **已敲定** | float[20] 直传；无量化（区别 Path/Glyph） |
| **Layer Mask 三趟 vs 两趟** | §12 | ❌ **应现在决定** | 设计为 Pass 1 + Pass 2；没有 Pass 3；需确认 SCC 检测是否复用 tgfx 或自行实现 |
| **PAGDocument 版本字段** | —（漏项） | ⚠️ **补齐漏项** | 文档定义 FORMAT_VERSION = 0x02，但 PAGDocument 本身无 version 字段（因为 struct 纯数据）；**需确认：在运行时是否需要 PAGDocument 记录解码源文件版本？** 推荐：不需要（写死 0x02） |
| **Diagnostic.contextIndex 的 UINT32_MAX 哨兵** | §15 + G.2 | ✅ **已设计** | 跨 Tag 边界的资源冗余时（ImageAsset 多次索引缺失），推 UINT32_MAX 哨兵给调用方 range-check；规则在 G.4 Scenario 2 已定 |

**建议优先级**：

**P0 开工前必确认**（影响字节流格式或 Phase 1-2 交付）：
- ✅ tgfx::Path 迭代 API（PathIterator vs getVerbAt）
- ✅ Mask SCC 检测算法选择（复用 tgfx 或自实）
- ✅ PAGDocument 版本字段设计

**P1 开工前建议澄清**（影响设计但非阻塞）：
- ✅ GlyphRun.kind 三值对应的位宽优化策略
- ✅ Property<T> NaN 处理边界（虽有规定但需开发者知晓）

**P2 开工后逐步补**（不影响 Phase 0-3）：
- 动画框架细化
- OutlineAll 实现方案细化

---

## 七、72 小时模拟（新人工程师开工前 3 天）

### Day 1：框架理解与环境搭建

**上午**（4h）：
- 读文档 §1-§3（背景、原则、架构）
- 读 §3.2 四态三映射表 → 心智模型成型
- 读 §5 PAGDocument 数据模型（5 分钟快读）
- 问题：什么是 `BakeContext`？为什么需要两个 pass？
  - 回答路径：§8.5 + §12 Mask 两趟

**下午**（4h）：
- 搭建编译环境
  ```bash
  cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
  # 若 tgfx 未链接，问 tech-lead：../tgfx 目录是否需要 `-DTGFX_DIR=../tgfx` 参数
  ```
- 创建目录骨架 `src/pagx/pag/` 等
- 跑现有测试确认环境可用
- 遇阻点：CMakeLists.txt 配置（不在本阶段解决，Phase 16 交给 infra 人员）

**evening**（2h）：
- 理解 Phase 0 的 Diagnostic ABI（§G 完整）
- 浏览 ErrorCode 枚举（目标 50+）

**Day 1 出口检查**：
- 能说出 Baker → Codec → LayerInflater 三阶段的数据流
- 理解 PAGDocument 中 Property<T> 的概念
- 环境编译通过（即使 pagx-pag target 不存在）

### Day 2：Phase 0 完成

**上午**（6h）：
- 确认 Diagnostic.h 对外头定义（包括 `FormatDiagnostic(diag)` 函数）
- 梳理所有 DiagnosticCode 枚举（从 §G.6 表格逐条枚举）：
  ```
  100-105: Baker fatal (LayoutNotApplied, ..., StructureLimitExceeded)
  200-299: Baker warning
  300-399: Codec fatal (100 offset)
  400-499: Codec warning
  500-599: Inflater fatal（本期未用）
  600-699: Inflater warning
  ```
- 实现 `DiagnosticCollector.h` 基类（protected pushError/pushWarning）

**下午**（4h）：
- 实现 `src/pagx/Diagnostic.cpp::FormatDiagnostic` （格式化诊断为字符串）
  - 参考 `CodeToString(code) + contextIndex 填充`
- 编写 Phase 0 单测 `DiagnosticTest.cpp`
  - Test: FormatDiagnostic 往返
  - Test: kAllDiagnosticCodes 遍历覆盖
  - Test: ABI-appended 码不丢失

**evening**（1h）：
- Phase 0 测试全绿
- `grep -rn "kAllDiagnosticCodes" . | wc -l` 应为 1（唯一数据源）

**Day 2 出口检查**：
- 能实现一个简单的"错误格式化"命令行：`./pagx-test --diagnostics`
- DiagnosticTest 全绿

### Day 3：Phase 1 与第一个 floatToU8

**上午**（5h）：
- 实现 `ValueCodec.h` 骨架
  - 读 §D.1 数值编码规则（floatToU8, int32List 等）
  - 理解"动态位宽"概念（§D.2 quantization）
  - 问题：floatToU8 是什么？
    - 回答路径：§D.1 表格第一行 → 公式 `uint8_t = clamp(value * 255, 0, 255)`

- 实现第一个 `WriteValue<float>` 特化
  ```cpp
  template <> inline void WriteValue<float>(EncodeStream* s, float v) {
    s->writeFloat(v);
  }
  ```

- 实现对应 `ReadValue<float>`（带值域校验）

**下午**（4h）：
- 读 §4.3 Property<T> 编码规则
- 实现 `PropertyEncoding.h` 中的 `WriteProperty / ReadProperty` 模板
  - 理解 propHeader 位域（bit 0-3 = encoding, bit 4 = isDefault）
  - 实现 `PropertyValueEquals<T>` 通用模板
  
- 问题遇阻点：什么时候 `isDefault == 1` 才能省略 value 字节？
  - 回答路径：§C.2 Property 定义 + 初值来源唯一性纪律

**evening**（1h）：
- 编写 `ValueCodecTest.cpp` 中的 `EXPECT_EQ(WriteValue(1.0f), {...bytes...})`
- 测试全绿

**Day 3 出口检查**：
- 能手工推导 `WriteProperty(alpha=0.5f, defaultValue=1.0f)` 的字节序列（5-6 字节）
- ValueCodec 部分单测通过（不求全，但基础特化过 50%）
- 了解量化编码的概念但暂不实现（推至 Phase 5）

**Day 3 晚上推荐加班 2h**：
- 快速浏览 §D 全章（估 90 min）
- 对字节布局有宏观感觉

### 预期卡点

| 时间 | 卡点 | 原因 | 化解 |
|-----|------|-----|------|
| Day 1 PM | CMakeLists 配置 | pagx-pag target 未定义 | skip（Phase 16） |
| Day 2 AM | Diagnostic 枚举有 50+，易遗漏 | 来自 §G.6 分布式定义 | 用工具逐行检查 `kAllDiagnosticCodes` |
| Day 3 AM | "为什么要 propHeader？" | 理解不了动画扩展性 | 跳过动画理由，专注本期 Constant 模式 |
| Day 3 PM | WriteValue<Color> 怎么写？ | 不知道 tgfx::Color 内存布局 | 查 tgfx/core/Color.h，或直接 writeFloat x4 |

---

## 八、工作量估算

### 文档声明的工作量提示

- **15 阶段**，交付判定分层明确 → 暗示 **3-4 周单人，或 2 周 2-3 人**
- **§19 Phase 依赖矩阵** 给出关键路径 → 串行最少 14 个工作日

### 对标 v1 代码规模

| 模块 | v1 现有行数 | v2 估计行数 | 复杂度增加 |
|-----|-----------|-----------|----------|
| src/codec/ 核心 | ~8000 行 | — | N/A（v1 用于 pag 播放器，v2 编解码全新） |
| src/codec/utils/EncodeStream | 287 | (复用) | 0% |
| src/codec/utils/DecodeStream | 289 | (复用) | 0% |
| src/codec/tags/ | 3000+ | ~2000 | 相似（v2 标签更简化） |
| src/renderer/LayerBuilder | 929 | — | N/A（蓝本） |
| **v2 新增总计** | — | ~15000 | 新实现 |

### 按 Phase 的代码量分布

| Phase | 产品代码行数 | 测试代码行数 | 开发小时数 |
|-------|-----------|-----------|---------|
| 0 | 300 | 200 | 4h |
| 1 | 400 | 400 | 6h |
| 2 | 800 | 600 | 8h |
| 3 | 500 | 400 | 6h |
| 4 | 1200 | 800 | 12h |
| 5-8 | 3500 | 2500 | 40h |
| 9 | 1500 | 1200 | 16h |
| 9.5 | 200 | 300 | 4h |
| 10-11 | 600 | 600 | 8h |
| 12-15 | 800 | 2000 | 24h |
| **总计** | **9400** | **9200** | **128h = 16 工作日（单人）** |

### 复杂度热点

| 模块 | 复杂度 | 原因 | 建议处理 |
|-----|------|------|---------|
| PropertyEncoding/ValueCodec | ⭐⭐⭐⭐ | 20+ 种类型，浮点精确匹配规则 | 参数化单测；模板特化易出错 |
| PathCodec 量化编码 | ⭐⭐⭐⭐⭐ | int64 中间结果、动态位宽、溢出防护 | Phase 5 单独 checkpoint；配对 LLM 审查 |
| GlyphRunBlob 序列化 | ⭐⭐⭐⭐ | 三种编码路径（位字段、坐标量化、xform可选） | 参数化单测覆盖 8-12 个组合 |
| LayerInflater Mask 循环检测 | ⭐⭐⭐⭐ | SCC 算法，深度递归，性能 vs 正确性权衡 | Phase 9 单独评审；可选 Tarjan 算法 |
| Codec 转发兼容规则 4.4 | ⭐⭐⭐ | 未知 Tag/Encoding 的 skip 策略复杂 | Phase 4 完整测试 forward-compat case |

### 团队分工建议（2-3 人）

**Option A：2 人（21 工作日）**

| 工程师 | 职责 | 预计工作量 |
|------|------|----------|
| A (Senior Baker) | Phase 0-3, 6-8（Diagnostic, ValueCodec, Baker 子模块） | 64h |
| B (Junior Codec) | Phase 1, 4-5, 9（ValueCodec 助手, Codec, Inflater） | 64h |
| Code Review Sync | Phase 结束时交叉审查 | 每 Phase ~2h |

**Option B：3 人（14-16 工作日）**

| 工程师 | Phase 职责 | 并行期 |
|------|-----------|-------|
| A | 0, 1, 2, 3 | Week 1 |
| B | 4, 5 (需 A 的 Phase 3) | Week 1-2 overlap |
| C | 6-8, 9 (需 B 的 Phase 5) | Week 2-3 overlap |
| Converge | 9.5, 10, 10.5, 11, 12 | Week 3 |

---

## 九、核心建议

按优先级排列：

### 开工前必修 P0（影响能否开工）

1. **确认 tgfx 编译链**（已完成 ✅）
   - 检查 ../tgfx 是否存在，CMakeLists 是否已链接
   - 若无 tgfx，询问 tech-lead 是否使用 `-DTGFX_DIR`
   - **验证方式**：`cmake -DPAG_BUILD_TESTS=ON ... && ninja` 能编过

2. **确认 tgfx::Path 迭代 API**（须在 Phase 1 开工前解决）
   - 代码检查：`grep -n "getVerbAt\|PathIterator" $(find tgfx -name "*.h" | head -10)`
   - 若 Path 无 getVerbAt，则 §D.2 ChoosePathFormat 伪码需改写用 PathIterator
   - **交付物**：Phase 1 起的 ValueCodec::ReadPath/WritePath 要用上此 API

3. **创建 src/pagx/pag/ 目录骨架**（开工第一天）
   - 包含 10 个 subdirs（layer_bakers, tags 等）
   - 否则 Phase 2 起编译会 warning "directory not found"

### 开工前建议修 P1（影响交付质量，非阻塞）

1. **补齐 GlyphRun 参考代码位置的代码审查**（Phase 8 前）
   - 挑 10-20 行 TextLayout.cpp 的 GlyphRun 序列化逻辑，写到 Phase 2 的 TextBaker 设计文档里
   - 防止 Phase 8 开发者"猜测"GlyphRunBlob 字段顺序

2. **定义 Mask SCC 检测算法选择**（Phase 9 前）
   - 是否复用 tgfx 的 Graph 检测，还是 Tarjan 自实？
   - 写到 Phase 9 设计文档的 LayerInflater 一章
   - 预留 `InflaterContext::visitingComposition` vector 初始化代码

3. **确认 PAGDocument 运行时版本字段需求**（Phase 2 前）
   - PAGDocument 是纯数据结构，是否需要 `uint8_t version` 字段？
   - 建议：**不需要**（FORMAT_VERSION 全局常量 0x02，所有 PAGDocument 同源）
   - 在 Phase 2 PAGDocument.h 中明确注释

4. **预备 RenderUtil 的 tgfx::Surface 创建权限**（Phase 9.5 前）
   - RenderUtil 需创建 `tgfx::Surface::Make(..., canvasWidth, canvasHeight)`
   - 确认权限：test 代码能直接调 tgfx 公共 API 吗？
   - 若权限受限，联系 infra 人员

### 开工后逐步补 P2（可交付后改进）

1. **Property<T> NaN 容错机制**（Phase 3+ Baker 子模块优化）
   - 文档规定 NaN 时 isDefault=0（保守），但遇到真实 NaN 数据可能有额外诊断需求
   - 可在 Phase 11+ 添加可选的 NaN 检测 warning

2. **动画扩展框架细化**（Phase 13 后，非本期交付）
   - 动画期开工时再展开 AnimationData sub-Tag（已留位§D.14）
   - 本期仅需验证字节布局向前兼容（unknown encoding skip 路径 covered）

3. **OutlineAll 模式的 tgfx TextLayer 适配**（Phase 13 后）
   - 依赖 tgfx 字体 outline 导出 API 成熟度
   - 本期已规划独立基准图（§18.7），可渐进实现

---

## 十、关键节点与里程碑

| 日期 | 里程碑 | 交付物 | 验收门槛 |
|-----|-------|-------|---------|
| Day 1（开工） | 环境搭建 | 目录骨架 + CMake 可编译 | `ls src/pagx/pag/` 非空 |
| Day 4（Phase 0 末） | Diagnostic ABI 定型 | `include/pagx/Diagnostic.h` + DiagnosticTest 绿 | grep kAllDiagnosticCodes 有 50+ 枚举值 |
| Day 8（Phase 1-2 末） | ValueCodec + PAGDocument 就绪 | ValueCodecTest + PAGXBuilder 可用 | Phase 1 exit gate 自动化检查通过 |
| Day 14（Phase 3-5 末） | Baker 核心链路 | LayerBaker + VectorBaker + ResourceBaker | RoundTripTest 的 5 个基础 case 绿 |
| Day 16（Phase 9 末） | Codec + Inflater 完成 | Codec.cpp + LayerInflater.cpp | RoundTripTest 全绿 + PAGDocumentParityTest 绿 |
| Day 18（Phase 10-11 末） | 对外 API 就绪 | PAGExporter.h/cpp + PAGLoader.h/cpp + CLI | PAGLoaderTest 绿 + EndToEndTest 绿 |
| Day 20（Phase 12 末） | 渲染集成 | RenderEquivalenceTest 基准图接受 | Baseline::Compare 覆盖 spec/samples 全量 |
| Day 22（交付） | 可用状态 | 覆盖率 ≥85% + 性能基线 | PerformanceTest 绿 + tools/coverage.sh report |

---

## 总结

### 能否按期交付

**答案：YES，但需要**：

1. ✅ **前置条件 99% 备齐** → 3 项 P0 须在开工前 48h 内解决
2. ✅ **文档完整无歧义** → 可直接转测试代码（15+ 阶段已敲定）
3. ✅ **工作量可估计** → 128h 产品代码 + 测试，16 工作日单人可行
4. ⚠️ **工程复杂度非线性** → Path 量化 / GlyphRun 三路 / Mask SCC 三处热点需额外 review

### 风险等级

| 风险 | 概率 | 影响 | 缓解措施 |
|-----|-----|------|--------|
| tgfx API 变化（Path 序列化） | 低(15%) | 高（Phase 1 延期） | 开工前 API 审查 |
| Mask 循环检测性能 | 中(30%) | 中（Phase 9 延期 3-5 天） | Tarjan 算法预研 |
| 渲染基准图数量爆炸 | 低(10%) | 低（只需接受基准） | 限制 spec/samples 到 6-12 核心样本 |
| 浮点精确匹配导致的 isDefault 误判 | 中(40%) | 低（只影响文件体积） | 参数化单测穷举常见情况 |

### 最终判定

**开工可行度：★★★★☆ (4/5)**

- 可以明天开工
- 预期按期交付（3-4 周）
- 3 个关键决策点须确认（见 P0）
- 工程障碍清晰可解，无隐藏技术债

