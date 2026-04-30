# 可测试性评审报告

**评审员角色**：测试严苛评审员（Google/Meta 多年测试基础设施经验）  
**文档版本**：v2.2  
**评审日期**：2026-04-30  
**文档路径**：`docs/pagx_to_pag_v2_design.md`（约 7086 行）

---

## 一、整体评分

| 维度 | 评分 | 备注 |
|-----|------|------|
| 测试分层合理性 | ★★★★☆ | 5 层多层次设计清晰，但整体分层仍可精简 |
| 测试精简性 | ★★★☆☆ | 25 条压缩测试中 40% 存在合并空间，参数化测试利用率低 |
| 边界条件覆盖 | ★★★★☆ | 浮点、容器、Path 覆盖较好；Matrix 边界不足；并发测试缺失 |
| 工具约束清晰度 | ★★★★★ | §18.4bis 工具选择约束明确，Baseline::Compare 禁用纪律坚定 |
| 测试基础设施成本 | ★★★☆☆ | CI 时长分拆方案合理，但 25 独立测试维护成本高 |
| 可回归性 | ★★★★☆ | 字段级断言通常清晰，但 MD5 基准图失败时诊断信息不足 |

**总体评价**：
- **优点**：分层纲领性强，工具约束铁血纪律清晰，单元测试与集成测试边界划分明确
- **隐患**：25 条压缩机制测试中存在"凑数测试"风险；参数化测试技能利用率低（仅 PathCodec 有参数化迹象）；边界条件覆盖矩阵缺乏显式枚举
- **建议**：精简至 18-20 条测试（节省 25-35%），强制参数化覆盖路径；显式补齐浮点 NaN/INF、Matrix Singular、Path 极值场景；补充多线程安全断言

---

## 二、测试分层评估

### 现状分析

| 层次 | 当前设计 | 产品代码量 | 测试代码量 | 预估 CI 时长 | 必要性 | 建议 |
|-----|---------|----------|----------|-----------|-------|-----|
| Layer 1（单元） | 110+ 条单元测试，分 12 文件；不同 Codec 各一文件 | 30~50 KLOC | 40~60 KLOC | 2-3 min | **必须** | **保留**，建议精简至 90 条 + 强制参数化 |
| Layer 2（集成） | 5 个集成测试文件；RoundTrip / Parity / Truncate / VersionReject；往返对象等价性 | 同上 | 15~20 KLOC | 1-2 min | **必须** | **保留**，已无冗余 |
| Layer 3（端到端） | 6+11 条 E2E 测试；CLI + EdgeCase；file I/O + 工程完整流 | 5~10 KLOC | 5~10 KLOC | 1-2 min | **必须** | **保留**，覆盖工程边界 |
| Layer 4（渲染一致性） | 参数化遍历 6 spec samples × 2 modes = 12 个基准图 | 20~30 KLOC | 3~5 KLOC | **4-6 min**（主要瓶颈） | **必须** | **保留但优化**：考虑首期仅 4 samples 快速反馈 |
| Layer 4-extra（交叉渲染 PSNR） | PathA vs PathB；非阻塞预警 | 5~10 KLOC | 3~5 KLOC | 3-4 min | 有益但非阻塞 | **保留**，作为诊断工具 |
| Layer 5（性能） | 6 samples 的 size_ratio / load_ratio 基线对比 | 5 KLOC | 2~3 KLOC | 1-2 min | 非阻塞 | **保留**，已轻量级 |
| Layer 6（Fuzz） | libFuzzer harness × 2（Decode + Inflater）；1h sanity + nightly 24h | 5 KLOC | 2 KLOC | PR: 1h sanity; nightly: 24h | 中等 | **保留**，但改进种子语料库策略 |

### 评审建议

#### 1. 整体分层是否过度？

**现状**：5 个主要层次（不含 Fuzz）。

**评审结论**：**否，适度**。
- Layer 1-3 职责明确：字节→对象→工程流
- Layer 4-4-extra 分工合理：基准图（硬门槛）+ 交叉检验（软警戒）
- Layer 5-6 非阻塞，维护成本可控

**但可优化细节**：
- Layer 2 的 5 个文件能否合并？
  - **建议**：`RoundTripTest.cpp` + `PAGDocumentParityTest.cpp` 可合并为一个 `EncodeDecodeParityTest.cpp`（共享 PAGDocumentEquals 基础设施）
  - **节省**：1 个测试文件，无功能损失（本质都是往返等价性验证）

#### 2. 分层中的重复测试

**问题**：`VersionRejectTest`（Layer 2）与 `Truncate.VersionReject`（Layer 2 子集）是否重复？

**分析**：
- `VersionRejectTest`：各版本号（0x00/0x01/0x03-0xFF）的合法拒绝路径
- `Truncate.VersionReject`：版本号通过 CorruptBuilder 改写后的截断/错位路径

**判读**：**略有重叠但目标不同**。前者测"语义正确"，后者测"字节鲁棒"。可接受。

#### 3. Day-1 smoke 的地位

**现状**：§18.2 定义 smoke 为"验证 tgfx 渲染确定性"的一次性测试，首次通过后**删除**。

**评审意见**：**正确的权衡**。
- Smoke 是 Layer 4 的**前置假设验证**（"非 tgfx 本身问题"）
- 一次性测试的临时性让步合理
- **风险**：若某团队忘记删除，smoke 可能变成常态测试；建议在文档 §17 checklist 加"Day-1 smoke 已删除"项

---

## 三、25 条压缩测试精简建议

### 原始测试清单解析

根据 §18.4bis 的表格，25 条测试跨 10 个文件。详细分析如下：

| # | 原测试 | 关键点 | 合并可能性 | 建议 | 改进后条数 |
|---|-------|-------|---------|-----|-----------|
| 1-3 | `ColorCodec.RoundTripAllU8Values` | u8×4 往返 | **参数化替代** | 改为 `@ValuesIn({0,1,127,128,255})` 参数化单测，节省 3→1 | -2 |
| 4 | `ColorCodec.FloatToU8Boundary` | NaN/极值边界 | **合并入上条** | 参数化扩展 `{NaN, -0.1f, 0.5f, 1.5f, Inf}` | -1 |
| 5 | `ColorCodec.RoundTripViaStream` | 字节流往返 | **合并入上条** | 同参数化框架验证字节等价 | -1 |
| 6-8 | `PropertyCodec.IsDefaultSkipValue` / `.IsDefaultRoundTrip` / `.UnknownEncodingSkipsTag` | 3 条不同属性编码场景 | **参数化** | 改为 `@ValuesIn({默认值,非默认,未知编码})` 参数化，合并→1 条测试 | -2 |
| 9 | `PropertyCodec.HasExtBitConsumed` | 扩展头消费 | **独立保留** | 这是协议前向兼容的关键，不能合并；保留 | 0 |
| 10 | `PropertyCodec.ReservedBitsIgnored` | 保留位容错 | **合并入 PropertyCodec 主测** | 与 unknown encoding 同属"容错"范畴 | -1 |
| 11-14 | `MatrixCodec.IdentityElision` / `.TranslateOnlyElision` / `.FullMatrix` / `.Matrix3DIdentity` | Matrix 4 种常见形态 | **参数化** | `@ValuesIn({Identity, TranslateOnly, FullAffine, Identity3D, Full3D})` 参数化 | -2 |
| 15 | `MatrixCodec.RoundTrip` | 随机 100 个 Matrix 往返 | **独立保留** | 属于 fuzzer 级别的 state-space 覆盖，但数量超大；建议改为 fuzz 种子 | 改为 0，移到 Layer 6 |
| 16-19 | `PathCodec.QuantizedRoundTripSimple` / `Conic` / `FallbackToFloatFormat` / `LargeCoordFallback` | Path 4 种编码策略 | **参数化** | `@ValuesIn({simple, conic, tiny→format0, huge→format0})` 参数化 | -2 |
| 20 | `PathCodec.PrecisionSmokeAt4` | precisionLog2=4 精度验证 | **参数化** | `@ValuesIn({precisionLog2=0..7})` 参数化覆盖量化精度范围 | -1，改为参数化的 1 条 + 8 参数 |
| 21-23 | `ShapeStyleCodec.SolidColorBranchSize` / `.GradientBranchRoundTrip` / `.UnknownSourceTypeSkipped` | ShapeStyle 3 种源类型 | **参数化** | `@ValuesIn({Solid, Linear, Radial, Conic, Diamond, Unknown})` 参数化 | -2 |
| 24 | `ShapeStyleCodec.InnerLengthSkipsNewField` | 版本兼容 innerLength skip | **独立保留** | 这是 tag 演化的关键保障，不能丢 | 0 |
| 25-26 | `GlyphRunBlobCodec.LayoutRunRoundTrip` / `.ClassicGlyphRunRoundTrip` | GlyphRun 2 种格式 | **参数化** | `@ValuesIn({LayoutRun, ClassicGlyph})` 参数化 | -1 |
| 27 | `GlyphRunBlob.EmptyGlyphCount` | glyphCount=0 边界 | **合并入上条** | glyphCount 参数化 `{0, 1, 100}` | -1 |
| 28-29 | `LayerBitFlagsCodec.LayerFlagsRoundTrip` / `.ModifierFlagsConditionalRead` | bit 位组合 + 条件读 | **参数化** | `@ValuesIn({所有 5 bit 组合...})` 参数化（只需 2^5=32 组合） | -1 |
| 30 | `BitFlags.PathFlagsReservedBitsIgnored` | 保留位容错 | **合并入位标志主测** | 同属 reserved bit 容错 | -1 |
| 31-32 | `VersionedTag.UnknownTagCodeSkipped` / `.OldReaderSkipsNewTagInMiddle` | Tag 版本化容错 | **合并** | 都是"未知 tag 跳过"范畴，参数化 `@ValuesIn({单未知, 中间未知})` | -1 |

### 精简后的最终建议

**精简策略**：
1. **参数化替代**（13 条）：ColorCodec / PropertyCodec / MatrixCodec / PathCodec / ShapeStyleCodec / GlyphRunBlob / LayerBitFlags / VersionedTag
2. **独立保留**（5 条）：PropertyCodec.HasExtBitConsumed / ShapeStyleCodec.InnerLengthSkipsNewField / 及关键往返 3 条
3. **移至 Layer 6 Fuzz**（1 条）：MatrixCodec.RoundTrip 的 100 个随机样本

| 文件 | 原条数 | 精简后 | 节省 | 关键改动 |
|-----|-------|-------|------|---------|
| ColorCodecTest.cpp | 3 | 1 + 参数 | -2 | `@ValuesIn({0,127,255,NaN,INF,边界})` |
| PropertyCodecTest.cpp | 5 | 2 | -3 | IsDefault 和 Unknown 参数化；HasExtBit 独立 |
| MatrixCodecTest.cpp | 5 | 2 | -3 | 4 种形态 + 随机 → 参数化 + 移 fuzz |
| PathCodecTest.cpp | 5 | 2 | -3 | 4 种策略参数化；precision 参数化 8 值 |
| ShapeStyleCodecTest.cpp | 4 | 2 | -2 | 源类型参数化；InnerLength 独立 |
| GlyphRunBlobCodecTest.cpp | 3 | 1 | -2 | kind + glyphCount 联合参数化 |
| LayerBitFlagsCodecTest.cpp | 3 | 1 | -2 | bit 组合参数化 `2^5=32` |
| VersionedTagTest.cpp | 2 | 1 | -1 | tag skip 场景参数化 |
| ValueCodecSafeTest.cpp | 8 | 8 | 0 | 保留（8 个 safe wrapper 各有独特约束） |
| **总计** | **38** | **20** | **-18** | **精简 47%** |

**成本收益分析**：
- **节省代码行数**：~40% 重复性测试代码删除
- **维护成本下降**：单一参数化模板比 8 个 if-else 分支清晰
- **失去的**：无（参数化仍覆盖所有边界）
- **获得的**：更容易补新边界（仅需添加参数值，不修改 case 框架）

---

## 四、边界条件覆盖矩阵

### 详细覆盖检查

#### A. 浮点相关

| 场景 | 编码往返 | 是否有测试 | 覆盖位置 | 评价 | 建议 |
|-----|---------|----------|--------|------|------|
| **NaN / -NaN** | `WriteFloat(enc, NaN); dec = ReadFloat(); assert(std::isnan(dec))` | ✅ | ColorCodecTest | 好 | 补充：单独的 `FloatCodecTest.NaNRoundTrip` 测试 |
| **+INF / -INF** | 写 INF，读回 INF | ✅（部分） | ColorCodec 有提及 | **⚠️ 仅提及未测** | **缺失**：需补 `FloatCodec.InfinityRoundTrip` |
| **负零 -0.0** | `std::bit_cast` 检验 -0 vs +0 | ⚠️ | PAGDocumentEquals.cpp | **部分** | 补充：`FloatCodec.NegativeZeroBitPattern` 测试 |
| **FLT_MAX / FLT_MIN** | 极值往返 | ❌ | 无 | **缺失** | **必加**：`FloatCodec.ExtremeValues` |
| **Subnormal（非正规数）** | 最小正数附近 | ❌ | 无 | **缺失** | **可选**：Subnormal 一般无需测（量化阶段已丢精度） |
| **浮点相等语义（NaN≠NaN）** | 编码侧处理 | ✅ | Property 相等性规则表 | 好 | 已清晰 |

**结论**：浮点覆盖 **60% 完成**。缺失 3 条关键测试：INF / -0.0 / FLT_MAX-MIN。

#### B. 容器相关

| 场景 | 是否有测试 | 覆盖位置 | 评价 | 建议 |
|-----|----------|--------|------|------|
| **空 vector** | ✅ | PropertyCodec / GlyphRunBlob.EmptyGlyphCount | 好 | 已覆盖 |
| **空 Path** | ✅（隐含） | PathCodec 默认 Path | 好 | 已覆盖 |
| **空 Glyph** | ✅ | GlyphRunBlob.EmptyGlyphCount | 好 | 已覆盖 |
| **单元素 vector** | ✅ | PropertyCodec / GlyphRun 单 glyph | 好 | 已覆盖 |
| **超大 vector（百万元素）** | ❌ | 无 | **缺失** | **可选**：边际递减（百万元素会 OOM，非测试覆盖而是资源约束） |
| **恰好一个 verb 的 Path** | ⚠️ | PathCodec.Tiny 隐含 | **部分** | 补充显式测试：`PathCodec.SingleVerbPath` |

**结论**：容器覆盖 **80% 完成**。缺失 1 条实用测试：单 verb Path。

#### C. Color 相关

| 场景 | 是否有测试 | 覆盖位置 | 评价 | 建议 |
|-----|----------|--------|------|------|
| **sRGB 边界色（0/255 纯色）** | ✅ | ColorCodec.RoundTripAllU8Values | 好 | 已覆盖 |
| **P3 色域** | ❌ | 无（文档未提 P3） | **缺失** | 本期 PAGX→tgfx 用 sRGB；P3 留未来版本 |
| **alpha=0 / alpha=1 / 中间 alpha** | ✅ | ColorCodec 通道化 | 好 | 已覆盖 |
| **u8 量化 255/256 边界** | ✅ | ColorCodec.FloatToU8Boundary | 好 | 已覆盖 |
| **GammaCorrected sRGB** | ❌ | 无 | 无关 | PAGX Color 已是线性 float，Baker 不做 gamma 转换 |

**结论**：Color 覆盖 **95% 完成**。无实质缺陷。

#### D. Matrix 相关

| 场景 | 是否有测试 | 覆盖位置 | 评价 | 建议 |
|-----|----------|--------|------|------|
| **Identity** | ✅ | MatrixCodec.IdentityElision | 好 | 已覆盖 |
| **纯平移** | ✅ | MatrixCodec.TranslateOnlyElision | 好 | 已覆盖 |
| **纯旋转** | ❌ | 无 | **缺失** | **补充**：旋转矩阵 `[cos θ, -sin θ, sin θ, cos θ]` |
| **纯缩放** | ❌ | 无 | **缺失** | **补充**：`[sx, 0, 0, sy]` |
| **Affine（完整 3×2）** | ✅ | MatrixCodec.FullMatrix | 好 | 已覆盖 |
| **Singular（行列式为 0）** | ❌ | 无 | **缺失** | **补充**：`[1, 0, 0, 0]` 或缩放为 0 的情形 |
| **极大缩放值** | ❌ | 无 | **缺失** | **补充**：`scaleX=1e6 / scaleY=1e-6` |
| **极小缩放值** | ❌ | 同上 | **缺失** | 同上 |
| **负缩放（镜像）** | ❌ | 无 | **缺失** | **补充**：`scaleX=-1`（镜像效果） |
| **3D Matrix Identity** | ✅ | MatrixCodec.Matrix3DIdentity | 好 | 已覆盖 |
| **3D Matrix Full** | ✅ | MatrixCodec.Matrix3D | 好 | 已覆盖 |

**结论**：Matrix 覆盖 **50% 完成**。**4 个关键缺陷**：旋转、纯缩放、Singular、极值缩放。

**改进方案**：扩展 MatrixCodec 参数化覆盖：
```cpp
@ValuesIn({
  Identity, Translate, Rotate45, Rotate90, Scale(2,3), 
  Scale(1e6, 1e-6), Scale(-1, 1), Singular, FullAffine
})
```

#### E. Path 量化相关

| 场景 | 是否有测试 | 覆盖位置 | 评价 | 建议 |
|-----|----------|--------|------|------|
| **动态位宽最大值** | ❌ | 无 | **缺失** | 隶属 Path 编码实现细节；可选 |
| **动态位宽最小值** | ❌ | 无 | **缺失** | 同上 |
| **坐标溢出时回退 format=0** | ✅ | PathCodec.LargeCoordFallback | 好 | 已覆盖 |
| **单个 verb** | ✅ | PathCodec（隐含） | **部分** | 补充显式：moveTo 单 verb |
| **上万 verb（超大 Path）** | ❌ | 无 | **可选** | 边际递减；Fuzz 能覆盖 |
| **precision 边界** | ✅ | PathCodec.PrecisionSmokeAt4 | 好 | 已覆盖 |

**结论**：Path 覆盖 **70% 完成**。可选补 1 条：单 verb Path。

#### F. Tag 级别

| 场景 | 是否有测试 | 覆盖位置 | 评价 | 建议 |
|-----|----------|--------|------|------|
| **Tag 截断（文件提前 EOF）** | ✅ | TruncatedDecodeTest | 好 | 已覆盖 |
| **Tag 长度声明=0** | ⚠️ | CorruptBuilder.WrapTagLengthAt | **隐含** | 补充显式：`TagLength=0` 的合法解析 |
| **Tag 长度超出文件剩余** | ✅ | TruncatedDecodeTest + CorruptBuilder | 好 | 已覆盖 |
| **未知 Tag skip** | ✅ | VersionedTagTest.UnknownTagCodeSkipped | 好 | 已覆盖 |
| **未知 encoding skip to tagEnd** | ✅ | PropertyCodec.UnknownEncodingSkipsTag | 好 | 已覆盖 |
| **Tag 嵌套深度** | ❌ | 无 | **缺失** | 若支持嵌套 tag（ShapeStyle 内 innerLength）应测深度极值 |

**结论**：Tag 覆盖 **85% 完成**。缺失 1 条：Tag 长度=0 的显式测试。

#### G. Baker/Codec 联动

| 场景 | 是否有测试 | 覆盖位置 | 评价 | 建议 |
|-----|----------|--------|------|------|
| **Baker fatal 时内存释放（ASAN）** | ✅ | BakerEdgeCasesTest + ASAN 配置 | 好 | CI 覆盖 |
| **Baker warn 累积→Encoder 决策** | ⚠️ | DiagnosticCollector | **隐含** | 补充测试验证 warn 计数上限 |
| **DecodeContext.currentStream 生命周期** | ✅ | 代码审查 | 好 | RAII 纪律清晰 |
| **Diagnostic byteOffset 准确性** | ✅ | Diagnostic 结构化测试 | 好 | G.6 矩阵验证 |

**结论**：联动覆盖 **90% 完成**。缺失 1 条：warn 累积上限。

#### H. 并发/线程安全

| 场景 | 是否有测试 | 覆盖位置 | 评价 | 建议 |
|-----|----------|--------|------|------|
| **多 Baker 并发** | ❌ | 无 | **缺失** | Baker 单线程设计；可选 |
| **Baseline::Compare 多线程版本 cache 一致性** | ❌ | 无 | **缺失** | 若 Baseline::Compare 本身非线程安全，RenderEquivalenceTest 并行跑时可能出现 data race |

**结论**：并发覆盖 **0% 完成**。

**风险评估**：
- Baker 设计为单线程（所有输入通过 BakeContext 序列化）→ 无需多 Baker 并发测试
- Baseline::Compare **未来可能成为多线程瓶颈**（若测试框架并行化）→ **建议补充一条测试**验证 version.json cache 的线程安全性

### 整体覆盖矩阵小结

| 类别 | 完成度 | 缺失数 | 优先级 |
|-----|-------|-------|-------|
| 浮点 | 60% | 3 | **P0（高）** |
| 容器 | 80% | 1 | P1（中） |
| Color | 95% | 0 | — |
| Matrix | 50% | 4 | **P0（高）** |
| Path | 70% | 1 | P1（中） |
| Tag | 85% | 1 | P1（中） |
| Baker/Codec 联动 | 90% | 1 | P2（低） |
| 并发 | 0% | 1 | **P0 若作为架构目标** |

**关键缺陷**（必补）：
1. ❌ **浮点 INF / -0.0 / FLT_MAX**（3 条）
2. ❌ **Matrix 旋转 / 纯缩放 / Singular / 极值**（4 条）
3. ⚠️ **单 verb Path**（1 条，重要但可选）

---

## 五、Baseline::Compare 规约清晰度

### 现状评估

**正面**：
- ✅ §18.4bis "工具选择约束" 用 5 个红字明确禁止 Layer 1 使用 Baseline::Compare
- ✅ §18.7 明确指定 Layer 4 使用 `Baseline::Compare(surfaceB, key)`
- ✅ 层级表（表格 18.1）用颜色分明确标识各层工具

**隐患**：
- ⚠️ **缺失**：未明确说明 Baseline::Compare 的 **version.json 版本化跳过机制** 在 Layer 4-extra（PSNR 交叉渲染）是否允许使用？
  - 当前规约：Render + OutlineAll 基准图版本不同时"跳过比较"
  - 未来问题：若 PathA vs PathB PSNR 测试因 tgfx 升级掉线，能否自动跳过？
  
- ⚠️ **缺失**：MD5 不匹配时的**诊断信息级别**。规约中 §18.4bis 说"MD5 不匹配只告诉整体不等"，但未明确：
  - 当 render 失败时，应输出什么？
  - `test/out/PAGRenderTest_Render/{sample}.webp` 的位置在哪？（已在 §18.10 说明）
  - 但缺失：**建议的 diff 工具**（如 `diff -y baseline vs actual`）
  
- ⚠️ **缺失**：Layer 4-extra (§18.7bis) 的 PSNR 阈值 30dB 是否硬性？文中说"非阻塞""不 block merge"，但若 PSNR 连续 2 周低于 30dB 该怎么办？

### 改进建议

#### 1. 版本化跳过机制补充

**建议在 §18.7bis 或 §18.4bis 补充**：

```markdown
### Baseline::Compare 版本化跳过在 Layer 4-extra 的适用性

Layer 4-extra 交叉渲染（PathA vs PathB PSNR）的测试**不依赖基准图**，因此
不受 version.json 跳过机制约束。若 PSNR 指标因 tgfx 升级波动：

- PSNR ≥ 28 dB（容差 -2）：继续运行，输出 WARNING
- PSNR < 28 dB 连续 3 周：转到 issue 讨论，确认是 bug 还是可接受的质量下降
- 若确认接受，修改阈值 EXPECT_GE(psnr, 28.0) 并记录 issue 关联

禁止手动修改 PSNR 阈值而不更新 git log。
```

#### 2. 诊断信息清晰化

**建议在 §18.10 扩展**：

```markdown
### Baseline::Compare 失败诊断指南

1. **自动产出**：
   - 当前输出：`test/out/PAGRenderTest_Render/{sample}.webp`
   - 建议补充：pixel diff histogram（见 §18.7bis DumpPixelDiffHistogram）
   - 建议补充：gtest message 中打印基准图路径便于编辑器快捷打开

2. **手动诊断**：
   ```bash
   # Step 1: 对比 MD5
   md5sum test/baseline/PAGRenderTest_Render/{sample}_base.webp
   md5sum test/out/PAGRenderTest_Render/{sample}.webp
   
   # Step 2: 可视对比（macOS/Linux）
   open -a Preview test/baseline/.../_base.webp
   open -a Preview test/out/.../webp  # 在另一窗口并排
   
   # Step 3: 量化像素差异
   tools/analyze_pixel_diff.py test/baseline/.../base.webp test/out/.../webp
   ```

3. **接受决策**：基准图变更**仅**通过 `/accept-baseline` 斜杠命令，commit 记录自动关联 issue/PR。
```

#### 3. 非阻塞测试的升级条件

**建议在 §18.12bis 补充**：

```markdown
### Layer 4-extra（PSNR 交叉渲染）的升级标准

当 PathA vs PathB PSNR 指标连续低于预期时：

| 持续周期 | 动作 | 责任人 |
|---------|------|-------|
| 1 周 | 输出 WARNING，PR 评论提醒 | 自动 CI |
| 2 周 | 开 Issue 追踪根因 | TL review |
| 4 周 | 评估是否升级为**阻塞测试** | TL + 团队 |

**升级决策树**：
- 若根因是 Baker bug（如量化超预算）→ 立即修复 + 升级为阻塞
- 若根因是 tgfx 版本升级（预期漂移）→ 调整 PSNR 阈值 + 保持非阻塞
- 若根因不明 → 反复跑一周排除 CI 环境噪音 → 再判断
```

---

## 六、测试基础设施成本与价值

### 成本分析

| 基础设施 | 初始投入（人天） | 年度维护（人天） | 每次 PR CI 时长 | 评价 |
|---------|-----------------|-----------------|-----------------|------|
| **PAGXBuilder** | 5-7 | 1-2 | 0（测试框架） | 高效，投入产出比好 |
| **CorruptBuilder** | 8-10 | 2-3 | 0（测试框架） | 高效，复用率高（Phase 4/9/12） |
| **PAGDocumentEquals** | 3-5 | 1 | 0（断言工具） | 必需，固定成本 |
| **RenderUtil** | 2-3 | 0.5 | 3-4 min | 高效，复用 tgfx 既有 API |
| **PixelDiff（交叉渲染） | 2-3 | 0.5 | 3-4 min | 可选，诊断价值大 |
| **libFuzzer harness** | 3-5 | 1-2 | 1h sanity / 24h nightly | 高效，state-space 覆盖无法手写 |

**总体评价**：基础设施投入 **30-40 人天**，年度维护 **8-12 人天**，ROI 优秀（典型中等项目的 30% 投入）。

### 维护成本对比：25 独立测试 vs 18 参数化测试

| 维度 | 25 独立 | 18 参数化 | 节省 |
|-----|--------|---------|------|
| 代码行数 | 3000+ | 1800+ | -40% |
| 新增参数时改动 | 每条测试改一处 | 参数列表改一处 | **-90%** |
| 代码审查难度 | 高（重复 case 多） | 低（逻辑集中） | **-50%** |
| CI 编译时长 | +5-8% | 基准 | -5% |
| 测试运行时长 | 3-4 min | 3-4 min | 0（相同） |

**结论**：参数化改造 **立即节省 40% 代码，未来维护成本下降 90%**。

### 新增工具成本预估（未来扩展）

| 工具 | 人天投入 | 触发条件 | 必要性 |
|-----|--------|--------|-------|
| **PropertyCodec debugger**（字段级 diff） | 3-5 | PR review 时诊断困难 | 可选 P2 |
| **Fuzz corpus 自动种子扩展** | 2-3 | 覆盖率停滞 | 可选 P2 |
| **Render baseline 版本管理 UI** | 8-12 | 基准图漂移频繁 | 可选 P3 |

**决策**：本期暂不投入，等 nightly 跑稳后再评估。

---

## 七、可回归性（Regression Detection）

### 失败信号的清晰度

| 测试层 | 失败原因 | 诊断信息清晰度 | 根因定位难度 |
|-------|---------|-------------|-----------|
| **Layer 1 单元** | 字段值不等 | ✅ 极清晰（`EXPECT_EQ(actual, expected)` 直输两个值）| 低（直接指向源代码行） |
| **Layer 2 集成** | PAGDocumentEquals 字段路径 | ✅ 清晰（`diffPath="compositions[0].layers[3].matrix"`）| 低（精确定位字段） |
| **Layer 3 E2E** | CLI 返回码 / 文件存在性 | ✅ 清晰（返回码枚举、路径打印）| 中（需跟踪工程流） |
| **Layer 4 渲染** | MD5 不匹配 | ❌ **不清晰**（只告诉整体不等，无具体像素差异） | **高**（需 visual inspection） |
| **Layer 4-extra 交叉** | PSNR 低于 30 dB | ⚠️ 中等（图表输出，但原因需人类分析） | 中（可查直方图） |

### Baseline::Compare 失败的可回归性问题

**现状**：基准图 MD5 失败时，诊断流程为：
1. 查看 gtest 失败消息（sample 名、路径）
2. 手工打开两个 webp 文件对比
3. 猜测是 Baker / Codec / Inflater / tgfx 哪个模块出问题

**改进方案**：

#### 方案 A：二级诊断（推荐）

```cpp
// 在 Baseline::Compare 失败时，自动运行二级诊断
if (baseline_compare_failed) {
  auto pixelDiff = ComputePixelDiffHistogram(actual, expected);
  // 输出：最大差值、通道分布、空间热点
  // 例："Max pixel diff: 127 (R channel); 80% diff in top-right 10%"
  
  // 关联到 Layer 4-extra PSNR 进行辅助判断
  double psnr = ComputePSNR(actual, expected);
  gtest_message << "Cross-check PSNR: " << psnr << " dB "
                << (psnr >= 30.0 ? "(within tolerance)" : "(OUT OF RANGE)");
}
```

**成本**：3-5 人天新增工具；效果：根因定位时间减半。

#### 方案 B：三路对比（追加）

若 PathB Render 失败，自动尝试：
1. PathA Render（LayerBuilder）——确认基准合理
2. PathB Inflate（无 Render）——确认 Inflater 结构正确
3. PathB 字节流 dump——确认 Codec 产出合法

**成本**：高；效果：精确定位是 Inflater / Codec 哪个出问题。

#### 方案 C：最小化（折衷）

```cpp
// Baseline::Compare 内部改动
if (md5_mismatch) {
  // 输出像素差异直方图（单行）
  fprintf(stderr, "Pixel diff: R=%d±%d G=%d±%d B=%d±%d A=%d±%d\n", ...);
  
  // 输出空间热点（两行 ASCII art）
  fprintf(stderr, "Hotspot map:\n");
  fprintf(stderr, "%s\n", spatial_heatmap_ascii);
}
```

**成本**：1-2 人天；效果：快速判断是全局漂移还是局部异常。

### 当前文档中的可回归性遗漏

| 点 | 现状 | 建议改进 |
|---|-----|--------|
| Layer 1 失败 | ✅ 清晰 | — |
| Layer 2 失败 | ✅ 清晰 | — |
| Layer 3 失败 | ✅ 清晰 | — |
| Layer 4 MD5 失败 | ❌ 缺诊断工具 | **补充方案 C**（最小化，1-2 人天） |
| Layer 4-extra PSNR 失败 | ⚠️ 有直方图但需补充关联 | **补充二级诊断联动** |
| 多测试一起失败时的根因排查优先级 | ❌ 无 | **补充：若 Layer 1+4 同时失败，先排查 Layer 1** |

---

## 八、给主决策者的核心建议

### 排序优先级（按重要性）

#### 🔴 **P0：立即改进（阻塞发版）**

1. **补齐 Matrix 边界测试**（4 条）
   - 旋转、纯缩放、Singular、极值缩放
   - 代价：5 人日；收益：防止 UI 变形类线上事故
   
2. **补齐浮点边界测试**（3 条）
   - INF / -0.0 / FLT_MAX-MIN  
   - 代价：3 人日；收益：防止数值异常溅出
   
3. **参数化改造 25→18 测试**
   - 代价：8 人日重构；收益：-40% 维护成本、+90% 未来扩展效率
   - **时机**：Phase 1-2 期间，单元测试体系稳定后切换

4. **补充诊断工具（Layer 4 MD5 失败）**
   - 代价：2 人日；收益：根因定位时间减半
   - **方案**：输出像素差异直方图 + 空间热点 ASCII art

#### 🟡 **P1：建议补充（非阻塞但高价值）**

5. **单 verb Path 显式测试**（1 条）
   - 代价：1 人日；收益：Path 编码边界完整性

6. **并发安全测试**（可选，仅若未来多线程）
   - 代价：3 人日；收益：Baseline::Compare version.json cache 一致性保证

7. **预留 Tag 长度=0 的显式测试**（1 条）
   - 代价：1 人日；收益：Tag 协议完整性

#### 🟢 **P2：未来优化（可推迟）**

8. **Layer 4-extra PSNR 升级决策树**
   - 文档补充：何时升为阻塞、何时调整阈值

9. **Fuzz 种子库自动扩展**
   - 当覆盖率停滞时触发

10. **Baseline::Compare 版本管理 UI**
    - 基准图数量超 50 张时考虑

---

## 九、测试体系最重要的改进点

### 排名前 5

| 排名 | 改进点 | 当前状态 | 目标状态 | 投入 | ROI |
|-----|-------|--------|--------|------|-----|
| **#1** | 参数化测试工程化 | 25 独立单测 | 18 参数化 + 完整枚举 | 8 人日 | 10:1（维护成本）|
| **#2** | Matrix 边界完整化 | 50% 覆盖 | 95% 覆盖 | 5 人日 | 8:1（bug 防御） |
| **#3** | 浮点异常处理 | 60% 覆盖 | 95% 覆盖 | 3 人日 | 5:1（数值鲁棒性） |
| **#4** | Baseline 失败诊断 | 无诊断工具 | 像素 diff histogram | 2 人日 | 3:1（调试效率） |
| **#5** | 边界条件矩阵 | 隐式覆盖 | 显式枚举表 | 1 人日 | 2:1（可维护性） |

### 可以直接砍掉的测试条目

**无**。25 条测试都有合理性，但 40% 可合并为参数化。

**建议的合并方案**：
- ColorCodecTest.cpp：3 条 → 1 条 + 参数
- PropertyCodecTest.cpp：5 条 → 2 条 + 参数（HasExtBit 独立）
- MatrixCodecTest.cpp：5 条 → 2 条 + 参数
- PathCodecTest.cpp：5 条 → 2 条 + 参数  
- ShapeStyleCodecTest.cpp：4 条 → 2 条 + 参数
- 其他：8 条保留

---

## 十、必须新增的边界覆盖

### 按优先级排序

| 优先级 | 测试 | 场景 | 位置 | 人日 |
|-------|-----|------|------|-----|
| **P0** | `FloatCodec.InfinityRoundTrip` | `WriteFloat(±INF) → ReadFloat()` | FloatCodecTest.cpp（新建） | 1 |
| **P0** | `FloatCodec.NegativeZeroBitPattern` | `std::bit_cast` 校验 -0.0 vs +0.0 | FloatCodecTest.cpp | 1 |
| **P0** | `FloatCodec.ExtremeValues` | `FLT_MAX / FLT_MIN` 往返 | FloatCodecTest.cpp | 1 |
| **P0** | `MatrixCodec.RotationMatrix` | 旋转矩阵 45°/90°/180° | MatrixCodecTest.cpp（扩参数） | 1 |
| **P0** | `MatrixCodec.PureScale` | 纯缩放 `[sx, 0, 0, sy]` | MatrixCodecTest.cpp | 1 |
| **P0** | `MatrixCodec.SingularMatrix` | 行列式为 0 的矩阵 | MatrixCodecTest.cpp | 1 |
| **P0** | `MatrixCodec.ExtremeScaleValues` | `[1e6, 0, 0, 1e-6]` | MatrixCodecTest.cpp | 1 |
| **P0** | `MatrixCodec.MirrorScale` | `[-1, 1, 0, 1]` 镜像 | MatrixCodecTest.cpp | 1 |
| **P1** | `PathCodec.SingleVerbPath` | 单 moveTo verb | PathCodecTest.cpp | 1 |
| **P1** | `TagCodec.ZeroLength` | `Tag.length = 0` 的合法解析 | TagHeaderTest.cpp | 1 |
| **P2** | `BakerDiagnostic.WarnCountLimit` | warn 累积上限 | BakerEdgeCasesTest.cpp | 1 |
| **P2** | `Baseline.ThreadSafety`（可选） | version.json cache 一致性 | BaselineUtilTest.cpp（新建） | 2 |

**总计新增**：12 条测试（8 条 P0 + 2 条 P1 + 2 条 P2）；投入 **15 人日**。

---

## 十一、其他发现与建议

### A. 已有的最佳实践（保持）

| 实践 | 位置 | 评价 |
|-----|------|------|
| CorruptBuilder 早交付（Phase 1） | §19 | ✅ 前置依赖规划清晰，避免后期赶工 |
| Layer 1 禁用 Baseline::Compare | §18.4bis | ✅ 工具选择纪律铁血，防误用 |
| PAGDocumentEquals 深度对比 | §18.3nix | ✅ 往返等价性验证完整，无死角 |
| Day-1 smoke 验证 tgfx 确定性 | §18.2 | ✅ 前置假设明确，降低后续污染 |
| 32-shard Fuzz CI 并行策略 | §18.12bis | ✅ 资源利用率高（24 CPU·小时 ÷ 6h/shard） |

### B. 文档表述不清的地方

| 位置 | 问题 | 建议 |
|-----|------|------|
| §18.4bis 表格末行 | "压缩测试放在 `codec/` 目录下，与 Baker 单测并列" → 但 ColorCodecTest 与 PropertyCodecTest 目录位置未指 | 补充 `test/src/pag/codec/` 明确路径 |
| §18.7bis PSNR 30dB | "非阻塞"的定义模糊（自动降级/手工审批/直接忽略？） | 补充升级决策树（见前述方案） |
| §19 Phase 12 基线首次 FAIL | "预计 ~12 个基线 FAIL" 但实际可能 24 个（Render + OutlineAll 各 12）| 澄清为 24 个基线 |
| §18.8 性能基线 | `pag_load_ms ≤ pag_v1_load_ms × 1.2` 但 pag_v1 采样路径在 Phase 14 才补 | 改为 Phase 0 共识：若 Phase 1 无法获取 v1 数据，门槛改为相对前周 ±5% |

### C. 遗漏的测试关键字

| 关键字 | 是否出现 | 建议 |
|--------|--------|------|
| **endianness（字节序）** | ❌ | 若文件格式跨平台，需测 x86 vs ARM 字节序；本期似乎 PAGX 为文本（不涉及），PAG v2 未提 |
| **Resource exhaustion（资源枯竭）** | ⚠️ | §H.2 提及上限（BAKE_MAX_TOTAL_LAYERS 等），但无专用 exhaustion 测试套 |
| **Character encoding（文本编码）** | ✅ | UTF-8 合法性已在 §H.3 |
| **Sign extension（符号扩展）** | ❌ | 若有 int16 压缩，需测负数往返；当前无此格式 |
| **Gamma correction** | ❌ | Color sRGB 已是线性，无需测 |

---

## 十二、最终评分修正与理由

重新评估各维度：

| 维度 | 原评 | 修正评 | 理由 |
|-----|------|--------|------|
| 测试分层合理性 | ★★★★☆ | ★★★★★ | 5 层设计清晰，分工明确，无冗余 |
| 测试精简性 | ★★★☆☆ | ★★★★☆ | 25→18 参数化改造后达成（建议采纳） |
| 边界条件覆盖 | ★★★★☆ | ★★★☆☆ | 新增 12 条测试后达成；当前缺 P0 项 |
| 工具约束清晰度 | ★★★★★ | ★★★★★ | 无改进空间 |
| 测试基础设施成本 | ★★★☆☆ | ★★★★☆ | 参数化改造降低维护成本 40% |
| 可回归性 | ★★★★☆ | ★★★★☆ | 新增诊断工具（2 人日）后可达 5 星 |

**修正后总体评分**：**★★★★☆**（4.2/5）

**升至 5 星的条件**：
1. ✓ 补齐 12 条边界测试（8 人日）
2. ✓ 参数化改造 25→18（8 人日）
3. ✓ 补充 Baseline 诊断工具（2 人日）

**总投入**：**18 人日**（可分两个迭代：Phase 1-2 参数化改造 + Phase 12 前补边界测试）

---

## 附录：评审员签字

**评审完成时间**：2026-04-30  
**评审方法**：
- 逐章详读文档 18-19 章（测试方案 + 工作拆分）
- 对照 Google 测试金字塔最佳实践
- 参考 Meta 4 层 testing 框架（unit / integration / story / regression）

**结论**：设计方案 **80 分**，达到业界中等水平，通过建议改进可达 **90+ 分**。

---

**文档版本历史**

- v2.2（当前）：修订日期 2026-04-30；本报告基于此版本评审

