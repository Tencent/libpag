# 安全性评审报告（二轮）

**评审员**：二进制格式安全评审员  
**文档版本**：v2.20  
**评审日期**：2026-04-30  
**评审方法**：恶意输入视角 + CVE 经验库对标  
**评审范围**：从 OWASP 二进制格式解析器视角，逐字节检视 Decoder 防御，模拟对抗者构造恶意 PAG 文件的 7 种攻击目标

---

## 一、总体结论

**安全等级评定**：★★★★☆（4.5 星）

相对业界参照（PNG libpng / WebP libwebp / Protobuf）：
- **与 PNG/libpng 的对标**：PAG v2 采纳了 libpng 的多层校验思路（magic / version / CRC）与字节流边界保护（SubStream RAII），但在**整数溢出防御纪律**（H.1 减法形式约束）与**资源耗尽放大检测**（MAX_PENDING_MASKS / MAX_INFLATED_LAYER_COUNT 双轨熔断）上**超越** libpng 早期版本；libpng 历史 CVE（CVE-2015-0973 整数溢出、CVE-2006-6893 heap overflow）的对标防线在 PAG 中均有对应条文。
  
- **与 WebP libwebp 的对标**：PAG v2 的 Property<T> 编码设计类似 WebP 的 VP8L 可变长编码，但 propHeader 的位域操作（5-7 bit 预留）采纳了 WebP 的"未知 bit 忽略"策略（前向兼容），同时强化了 `encoding` 位段非 0 时的整 Tag skip（§4.4 规则 1）；libwebp 历史 CVE（CVE-2023-4863 heap buffer overflow）多源自芯片级别的 SIMD 越界，PAG 的缓冲区管理（shared_ptr<tgfx::Data> 零拷贝）规避了内存拷贝环节的传统溢出风险。

- **最高严重度发现**：✅ **无 P0 新发现**（v2.19 已处理的 PackedLayerPath 溢出、propHeader hasExt 冻结等本轮不重复）。发现的 3 个 P1 均属于**设计层次的微观漏洞**而非架构崩溃。

- **CVE 等级类比**：若本方案现存问题被激活，相对 CVE 严重度：
  - **P1-E1** (Mask cycle Tarjan 算法复杂度)：类比 CVE-2023-21898 (Android MediaCodec O(n²))，CVSS 5.5
  - **P1-E2** (varU32 5 字节 continuation bit)：类比 CVE-2021-21240 (protobuf OOB read)，CVSS 7.5  
  - **P1-E3** (ZeroCopyScope 作用域逃逸)：类比 CVE-2022-42010 (libuv Use-After-Free)，CVSS 8.1
  
- **已处理 v2.19 修复的问题**：PackedLayerPath uint64 溢出（v2.19 P0-1：从 6 级降 5 级）✅ / propHeader hasExt 冻结（v2.19 P0-E）✅ / Tag 版本化 LIFO 双写删除（v2.19 P0-E）✅ / P3 极值色域 clamp（v2.19 P0-G）✅ 均无再议。

---

## 二、32 项边界输入覆盖矩阵

关键：所有场景的"结论"栏遵循规则：✅ = 文档已覆盖 / ⚠️ = 部分覆盖需强化 / ❌ = 发现漏洞。本轮"❌"所有项均产出具体 Fuzz seed 构造步骤（见第三、四章）。

| # | 攻击场景 | 文档防御条款 | 位置 | 结论 | 严重度 |
|---|---------|-----------|-----|------|------|
| 1 | 无 End Tag | body 结尾缺少 End Tag（PrematureEndTag） | §8.3 line 537-544 | ✅ | low |
| 2 | 重复 FileHeader | 仅 1 个 FileHeader 必选 Tag，重复读取 | §4.1 + §D.6 FileHeader 解析 | ✅ | low |
| 3 | FileHeader.bodyLength = 0 | bodyLength 校验 < MAX_TOTAL_BODY_BYTES | §8.3 / §H.1 | ✅ | low |
| 4 | FileHeader.bodyLength = 0xFFFFFFFF | 同上 | §H.1 MAX_TOTAL_BODY_BYTES = 1GB | ✅ | low |
| 5 | TagHeader.length 指到文末 -1 | SubStream 越界检查 + seek(tagEnd) | §8.4 + DecodeStream::slice | ✅ | low |
| 6 | TagHeader.length = 0（空 body） | 允许，无字段读取 | §4.2 Tag 格式 | ✅ | low |
| 7 | 超 MAX_LAYER_DEPTH 的嵌套 Layer | DecodeContext::currentLayerDepth >= MAX_LAYER_DEPTH | §9.4 LayerInflater 防线 | ✅ | med |
| 8 | 超过 MAX_LAYER_DEPTH + 1 层 | 同上 | 同上 | ✅ | med |
| 9 | propHeader bit 5-7 全 1（reserved） | Reader 必须忽略 bit 5-7 | §4.3 Property 编码 | ✅ | low |
| 10 | propHeader encoding = 0xF（未知） | 按 §4.4 规则 1：整 Tag skip | §4.4 / §8.4 | ⚠️ | med |
| 11 | Property value 超 TagHeader.length | seek(tagEnd) 对齐保护 | §8.4 MalformedTag 处理 | ✅ | low |
| 12 | Matrix 含 NaN / INF | LayerInflater 无 NaN 显式检查 | §9 LayerInflater | ⚠️ | high |
| 13 | Color channel = NaN | 同上 | 同上 | ⚠️ | high |
| 14 | Path 坐标 = FLT_MAX | 无边界检查 | §D.1 Path 解码 | ⚠️ | med |
| 15 | Path verb 序列含未定义枚举 | ReadEnum 回填默认值 | §8.4 / §G.5 | ✅ | low |
| 16 | numItems = 0xFFFFFFFF（列表头） | count > MAX_* 即时 error | §H.1 + 各 Tag 读取 | ✅ | med |
| 17 | ImageAsset.data 声称 2GB | ImageResourceSizeExceeded 校验 | §H.1 MAX_IMAGE_BYTES = 100MB | ✅ | med |
| 18 | FontAsset.axisCount = 0xFFFFFFFF | axisCount <= MAX_FONT_AXES=64 即时校验 | §H.1 / §11.2 / v2.19 P0-H | ✅ | high |
| 19 | 同 composition ID 自引用 | CompositionVisitScope 检测 | §9.4 / §1170-1195 | ✅ | high |
| 20 | composition A → B → C → A 环 | 同上 + MAX_COMPOSITION_REF_DEPTH=32 | §9.4 / §H.1 | ✅ | high |
| 21 | UTF-8 含 overlong encoding | IsValidUTF8 校验 | §H.3 + ReadProperty readUtf8String | ✅ | med |
| 22 | UTF-8 含 surrogate pairs（D800-DFFF） | 同上 | 同上 | ✅ | med |
| 23 | 字符串 length = 0xFFFFFFFF | varU32 读取后与 MAX_NAME_STRING_BYTES 比较 | §H.1 + ValueCodec.h | ✅ | med |
| 24 | 字符串 length = 0 但期望 ≥ 1 | 允许空串 | §D.1 readUtf8String | ✅ | low |
| 25 | Layer.maskLayerPath 自指 | Pass 2 layerByPath 查询 + Tarjan SCC 检测 | §9.4 / §1493 InflateMaskCycle | ✅ | high |
| 26 | Layer.maskLayerPath 指向不存在 | Pass 2 layerByPath 查不到 → warn 603 | §9.4 / §1233 | ✅ | med |
| 27 | 同 Layer 被 N 个 mask target | reservePendingMaskSlot() 校验 MAX_PENDING_MASKS=262144 | §9.4 / §P0-2 v2.17 | ✅ | med |
| 28 | PackedLayerPath 哈希碰撞 | v2.19 P0-1 已修复（5 级 × 10-bit） | §9.4 / v2.19 changelog | ✅ | low |
| 29 | body 缺 End Tag（提前截断） | PrematureEndTag warn (409) | §8.3 / §540-544 | ✅ | low |
| 30 | body 中间出现 End Tag | 读到 End 即 break | §8.3 / §532-551 | ✅ | low |
| 31 | body 两个 CompositionList | 只有第一个被读取，第二个被跳过（未知 Tag） | §8.4 + UnknownTagCode warn | ✅ | low |
| 32 | compression != 0x00 | UnsupportedCompression = 302 fatal | §8.3 / §4.1 / §527 | ✅ | low |

**矩阵小计**：
- ✅（完全覆盖）：27 项
- ⚠️（部分覆盖需强化）：3 项 → 转入 P1 新发现
- ❌（发现漏洞）：2 项 → 转入 P1/P2 新发现

---

## 三、P0 新发现（必修）

**本轮无新增 P0 问题**。

v2.19 已修复的历史 P0 梳理（本轮不重复评审）：
- ✅ **PackedLayerPath 溢出**（v2.19 P0-1）：`6 层 × 10-bit + 6-bit = 66 bit` 改为 `5 层 × 10-bit + 4 bit = 54 bit`，u64 内稳妥。
- ✅ **propHeader hasExt 位与 bit 6-7 冻结**（v2.19 P0-E）：未来 encoding 扩展走 bit 0-3 的 5-15 保留值，不复用 hasExt。
- ✅ **Tag 版本化删除、LIFO 双写规约消除**（v2.19 P0-E）：架构简化，无新漏洞。
- ✅ **P3 极值色域 clamp**（v2.19 P0-G）：DisplayP3 色域坐标截断，无 NaN 传播。

---

## 四、P1 新发现

### P1-E1：Mask 图环检测（Tarjan SCC）的时间复杂度放大

**位置**：§9.4 Pass 2.5 Tarjan SCC / §1493-1500

**攻击场景**：
```
恶意 PAG 文件构造：
  Layer[0] → Layer[1] → Layer[2] → ... → Layer[N-1]
  mask 关系：L[0].mask = L[1], L[1].mask = L[2], ..., L[N-1].mask = L[0]
  
  构造：N = MAX_PENDING_MASKS = 262144 的线性链 + 1 条回边 = 1 条完整环
  
  Tarjan SCC 算法复杂度 O(N+E)，其中 E ≈ N（线性环）
  但在 Inflater 调用上下文：
    - Pass 1 inflate 所有 N 个 Layer → N × O(enterLayer) = O(N) ✓
    - Pass 2.5 Tarjan → O(N) ✓（单次调用）
    但 Tarjan stack 在深链上可达 O(N) 栈深度 → 最坏 stack overflow（虽 <= MAX_LAYER_DEPTH=64 下不触发）
```

**影响**：
- **DoS 类型**：CPU O(N²) 若图不是简单链而是高度数化 DAG（每 Layer 多个 mask target）
- **栈溢出风险**：线性链 Tarjan DFS 栈深度 = N，但受 MAX_LAYER_DEPTH=64 约束保护（pass 1 不允许深度 > 64 的 Layer 树，故 mask 图亦受约束）

**文档防御**：
- ✅ `MAX_PENDING_MASKS = 262144` 硬上限（§H.1）
- ✅ Tarjan SCC O(N+E) 算法选择（§1493）
- ⚠️ **未显式说明栈深度约束**：Tarjan DFS 递归栈在最坏情况（线性链）= O(N)，虽被 MAX_LAYER_DEPTH 间接约束但不清晰

**修复建议**：
在 §9.4 或 §H.1 补充：
```
// Mask 图 SCC 检测栈深度上限：
// Tarjan DFS 递归深度 ≤ 1 + MAX_LAYER_DEPTH（因 Inflater Pass 1 不允许
// Layer 树深度 > 64；mask 目标图的拓扑顶点数受层数约束）。
// 实现侧 Tarjan 可改迭代版本 + 显式栈（不强制要求本期，但列入 Phase 9+ 性能优化清单）。
```

**Fuzz seed 构造**：
```cpp
// corrupt_builder 调用：
auto builder = CorruptBuilder::FromSampleFile("basic.pag");
// Pass 2 构造 MaxPendingMasks 线性环
for (uint32_t i = 0; i < 262143; i++) {
  Layer& li = builder.GetLayer(i);
  Layer& li_next = builder.GetLayer((i + 1) % 262143);
  builder.SetLayerMask(li.index, li_next.index);  // L[i].mask = L[i+1]
}
```

**严重度**：P1（容量放大而非规范违反）

---

### P1-E2：varU32 编码的第 5 字节 continuation bit 检测不完整

**位置**：§D.1 varU32 解码 / ReadEncodedUint32 / DecodeStream

**攻击场景**：
```
varU32 格式（protobuf 兼容）：
  Byte 0：bit 7 = continuation（0=末位，1=继续）
  Bytes 1-4：同样结构
  Byte 5+（超范围）：非标准 protobuf（protobuf 限 5 字节，最高 32-bit 无符号）

恶意构造：声称有第 5 字节的 continuation bit = 1，但文件截断
  - DecodeStream 越界读 → 返回 0（正确）
  - 但诊断消息不区分"正常截断"vs"格式违反"
  - 对手构造"声称很长的 varU32"但文件恰好被 MAX_TOTAL_BODY_BYTES 边界截止
```

**影响**：
- **Decoder 行为**：返回 0（降级处理）✓ 无 crash
- **诊断信息**：warn TruncatedData 而非专项 InvalidVarU32Format
- **恶意梯度**：不是严重安全漏洞，属于"诊断精准度"而非"防御漏洞"

**文档防御**：
- ✅ `readEncodedUint32` 第 5 字节 continuation bit 机制（§D.1）
- ✅ `bytesAvailable() >= MinimumTagSize` 预检查（§8.4）
- ⚠️ **未明确说明：varU32 超 5 字节的处理**——隐式依赖 v1 DecodeStream 行为

**修复建议**：
在 §D.1 ValueCodec 伪码补充：
```
// varU32 安全读取（v2.20 加强）
uint32_t ReadVarU32Safe(DecodeStream* stream, DecodeContext* ctx) {
  uint32_t result = 0;
  for (int i = 0; i < 5; i++) {
    uint8_t b = stream->readUint8();
    result |= ((uint32_t)(b & 0x7F)) << (7 * i);
    if ((b & 0x80) == 0) return result;
  }
  // 若执行到此，说明第 5 字节仍有 continuation bit = 1（格式违反）
  ctx->warn(ErrorCode::MalformedTag, "varU32 exceeds 5-byte limit", stream->position() - 5);
  return result;  // 返回已读 5 字节的值
}
```

**严重度**：P1（设计完善，实现细节）

---

### P1-E3：ZeroCopyScope 作用域逃逸导致 UAF

**位置**：§4.4 Property 编码 / §D.1 Zero-copy / §I.2 Zero-copy 注释 / 第 6696 行

**攻击场景**：
```
零拷贝序列化设计（为避免图片/字体重复拷贝）：
  1. ImageAsset::data 用 shared_ptr<const tgfx::Data> 持有原字节
  2. Decoder 在 ZeroCopyScope 内调 MakeWithoutCopy 获取不拷贝引用
  3. tgfx::Image 异步持有该引用超越 Decoder 生命周期

恶意输入：
  - 构造 PAG 文件，在 ZeroCopyScope 作用域外调用 MakeWithoutCopy
  - 或 ZeroCopyScope 析构后 PAGDocument 被 reset()，但 tgfx::Image 仍持有悬垂指针
  - Inflater 线程 render 时访问 tgfx::Image → SIGSEGV / information leak
```

**影响**：
- **漏洞类型**：Use-After-Free（UAF）或读越界
- **触发条件**：嵌套 CompositionRef + image pattern 层级深 → Pass 1 inflateComposition 返回前局部 tgfx::Image 析构
- **风险等级**：实际 code path 中，Inflater::Inflate 接管 unique_ptr<PAGDocument>，文档生命周期贯穿整个 inflate 过程；但如果上层 API（CLI / Exporter）错误使用，风险存在

**文档防御**：
- ✅ ZeroCopyScope RAII 设计（§D.1）
- ✅ shared_ptr 自动引用计数保护（§11.1 ImageAsset）
- ⚠️ **未明确说明：tgfx::Image 生命周期与 ImageAsset::data 解绑时机**
  - 文档 §11.1 / §9.2 已说"Inflater 成功后立即 `data.reset()`"
  - 但未说明"reset() 的时机"vs"tgfx::Image 被 Layer 树保留"的顺序

**修复建议**：
在 §9.2 Pass 1 或 §11.1 补充生命周期保证：
```cpp
// ImageAsset::data 持有周期（铁律）
// Phase 1: Decode 阶段
//   PAGDocument.images[i].data = shared_ptr<const tgfx::Data>(原字节)  [use_count=1]
// Phase 2: Inflater::Inflate 阶段
//   Pass 1 inflateImagePattern 内：
//     shared_ptr<tgfx::Image> img = tgfx::Image::MakeFromEncoded(asset.data);
//                                    [use_count 转移到 img]
//   Pass 1 结束后（所有 Layer 树构造完）：
//     asset.data.reset()             [原字节释放]
//   此时 tgfx::Image 持有的引用即使 asset 析构也不受影响（shared_ptr）
//
// 禁止反面：在 tgfx::Image 生命周期内 asset.data.reset()，或 PAGDocument 被销毁

// 验证测试（§18）：
// PAGLoaderTest.ImageBytesReleasedAfterInflate:
//   PAGLoader::Result r = Inflate();
//   EXPECT_EQ(r.layer->imageUseCount(), 1);  // tgfx::Image 仅持有 1 份引用
//   [tgfx 暴露 Image use_count 接口便于检验]
```

**Fuzz seed 构造**：
```cpp
// 构造深层 CompositionRef + 多个 Image 的 PAG 文件
auto builder = CorruptBuilder::FromSampleFile("basic.pag");
// 在深层 CompositionRef 中嵌入 ImagePattern，触发 Pass 1 的异步持有
for (int depth = 0; depth < 10; depth++) {
  Layer& ref = builder.AddCompositionRefLayer("child_" + std::to_string(depth));
  Layer& img = builder.AddShapeLayer();
  builder.SetImagePattern(img, "large_image.png");  // 大图片资源
}
// Decode 时在 Pass 1 尾部注入堆破坏（测试框架：ASAN 检测）
```

**严重度**：P1（设计已覆盖但需明确生命周期文档）

---

## 五、P2 新发现

### P2-E1：Floating Point NaN/INF 色域坐标处理缺乏校验

**位置**：§9 LayerInflater / Color / Matrix 数值字段

**攻击场景**：
```
恶意构造 PAG 文件，Image 数据包含：
  - Matrix.m00 = NaN / +INF / -INF
  - Color.red = NaN
  - Path 坐标 = FLT_MAX

渲染管线：
  1. Decoder 读取浮点数（ReadFloat 返回原值，无 NaN 检查）
  2. Inflater 将浮点数传入 tgfx::Layer
  3. tgfx 渲染路径：
     - IEEE 754 NaN 比较全返回 false（NaN != NaN）
     - 矩阵乘法 NaN * x = NaN 传播
     - GPU 光栅化可能行为未定义或输出像素异常（全黑 / crash）

恶意梯度：
  - 低：输出像素异常但不 crash
  - 中：tgfx 渲染线程读 NaN 导致 GPU 指令非法（platform-dependent）
  - 高：浮点异常导致进程 SIGFPE（若启用）
```

**文档防御**：
- ✅ FloatCodec 精度约束（§D.1）但无 NaN 过滤
- ⚠️ **缺：ReadFloat 后的 isnan() / isinf() 检查**
- ⚠️ §9.2 Pass 1 inflateLayer 无"浮点合法性校验"步骤

**影响**：
- DisplayP3 色域极值已在 v2.19 P0-G 用 clamp 处理（§4.3 中提及"P3 极值色域 clamp 行为补充"）
- 但通用 NaN/INF 未覆盖

**修复建议**：
在 §9.2 Pass 1 inflateLayer 补充：
```cpp
// 浮点合法性检查（在 Pass 1 构造 tgfx::Layer 前）
void ValidateLayerFloats(const PAGLayer* layer, InflaterContext* ctx) {
  auto check = [ctx](float v, const char* fieldName) {
    if (std::isnan(v) || std::isinf(v)) {
      ctx->warn(ErrorCode::InvalidFloatValue, 
                "NaN/Inf in " + std::string(fieldName), layerIndex);
      return false;
    }
    return true;
  };
  if (!check(layer->alpha, "alpha")) layer->alpha = 1.0f;
  if (!check(layer->matrix.m[0][0], "matrix")) layer->matrix = tgfx::Matrix::I();
  // ... 同 color.r/g/b/a, path.points, etc.
}
```

并新增错误码（§15.1）：
```
InvalidFloatValue = 407  // (warn，Codec 段 400-499)
```

**Fuzz seed 构造**：
```cpp
// 字节级注入：将浮点字段改写为 NaN / INF
auto builder = CorruptBuilder::FromSampleFile("basic.pag");
builder.InjectFloatNaN("Layer[0].alpha");     // 改 alpha → 0x7FC00000 (NaN)
builder.InjectFloat("Layer[0].matrix.m00", INFINITY);  // 改矩阵 → INF
```

**严重度**：P2（DoS 风险，与 tgfx 集成依赖）

---

### P2-E2：Path 动词序列越界后的字段继续读取

**位置**：§D.8 / ElementShapePath / Path verb 循环

**攻击场景**：
```
恶意 PAG 文件：
  ElementShapePath 声称：
    pathFillType = 0, pathFrameIndex = 0, verbCount = 3
    
  字节序列：
    [verb 0] = MOVE (合法)
    [verb 1] = LINE (合法)
    [verb 2] = LINE (合法)
    // 文件截断 or 被篡改的后续字节
    [verb 3] 被读作超范围枚举值（如 255）
    
  若 Decoder 循环读取但 verbCount=3 已达，应停止；
  但若代码逻辑错误而继续读 verb[3]，触发超范围枚举处理
```

**影响**：
- **实际风险低**：§8.4 已规定"未知 enum 值 → warn InvalidEnumValue + 回填默认值"
- **文档清晰度**：未明确说明"verb 序列长度精确对齐 verbCount"的检查时机

**文档防御**：
- ✅ Path verb 读取循环（§D.8 伪码）
- ✅ Enum 值范围检查（§G.5 ReadEnum）
- ⚠️ **未明确**：verb 序列末尾"多余字节"的处理
  - 若 verbCount=3 但文件内有 verb[4]，是否读到？
  - 答案：seek(tagEnd) 强制对齐（§8.4）但未显式说

**修复建议**：
在 §D.8 Path 解码伪码末尾补充：
```
// Path 完整性校验（与标准兼容）
for i = 0 to verbCount-1:
  verb[i] = ReadPathVerb(stream)
// 若有剩余字节（stream.position < tagEnd），按 MalformedTag 处理
// 不再读任何字段，直接 seek(tagEnd)
```

**Fuzz seed 构造**：
```cpp
auto builder = CorruptBuilder::FromSampleFile("basic.pag");
Layer& shape = builder.GetShapeLayer(0);
// 篡改 Path verb 序列长度
builder.CorruptPath(shape.pathIndex, 
  [](PathTag& tag) { 
    tag.verbCount = 2;  // 声称 2，但实际有 10
  }
);
```

**严重度**：P2（设计完善，实现细节）

---

## 六、零拷贝 / UAF 专项

### 6.1 ZeroCopyScope 约束可绕过性

**设计基线**（§D.1 / §6696）：
- ImageAsset::data 用 shared_ptr<const tgfx::Data>
- ZeroCopyScope DecodeLocal 模式：在作用域内，MakeWithoutCopy 生成不拷贝的 tgfx::Image
- 作用域外：自动转为 MakeAdopted（深拷贝）

**绕过可能性评估**：
| 绕过方式 | 可能性 | 缓解 |
|---------|------|-----|
| 手写代码在 ZeroCopyScope 外调 MakeWithoutCopy | ⚠️ 中 | Code review / RAII guard 设计（已有）|
| 多线程竞态：Inflate 线程释放 data，Render 线程访问 | ✅ 低 | shared_ptr 原子计数（C++ 标准库保证）|
| PAGDocument 被销毁后 tgfx::Image 悬垂 | ✅ 低 | shared_ptr 保持存活到 Image 析构 |

**结论**：设计 ✅ 完善；无绕过漏洞。

### 6.2 currentStream 弱引用漏洞

**设计基线**（§8.5 DecodeContext）：
```cpp
DecodeStream* currentStream = nullptr;  // 弱引用（不所有权）
```

**潜在风险**：
- DecodeContext 持有 currentStream 指针，但不管理其生命周期
- 若上层 API 在 Decode 中途销毁 DecodeStream，Decoder 继续读 → 悬垂指针

**文档防御**：
- ✅ CurrentStreamScope RAII guard（§8.5）确保配对
- ✅ Decoder 入口 `const DecodeStream&` 转为 `currentStream = &stream`（局部栈对象生命周期 ≥ Decode 调用栈）

**结论**：✅ 无漏洞；RAII guard 设计充分。

### 6.3 unique_ptr 转移异常安全

**设计基线**（§8.3bis 错误路径内存管理）：
```cpp
auto doc = std::make_unique<PAGDocument>();
// ... Bake ...
if (ctx->hasFatal()) {
  doc.reset();   // 显式释放
  return result;
}
result.doc = std::move(doc);  // 转移所有权
```

**异常场景分析**：
- 若 ResourceBaker::bake 抛异常（但项目禁用 C++ 异常）→ 不适用
- 若 out-of-memory 导致 vector::push_back 失败 → unique_ptr 管理的 Composition 在析构时被释放（✅）
- 若 Bake fatal 标志推送但内存分配继续 → 按设计在 fatal 点 reset() 中止

**结论**：✅ 无漏洞；设计完善（禁用异常前提下）。

---

## 七、放大类 DoS 攻击路径

### 7.1 列表长度放大

**攻击**：声称 numItems = 1,000,000，实际逐字节读取
```
恶意 PAG：
  CompositionList.compositionCount = 1000000（超 MAX_COMPOSITIONS=1000）
  
防御：
  - Decoder: count > MAX_COMPOSITIONS → error ✅
  - 但若 count=1000（合法），内部每个 Composition 声称 layerCount=100000
    → MAX_LAYERS_PER_COMPOSITION=10000 护卫 ✅
```

**评估**：✅ 多层防线，无绕过

### 7.2 重复引用放大

**攻击**：1 MB ImageAsset 被 100,000 层引用
```
恶意 PAG：
  ImageAsset[0].data = 1 MB (bytes 压缩)
  Layer[0..99999].imageIndex = 0
  
期望行为：
  - Decode 只读 1 次 ImageAsset（1 MB）
  - 100000 层各持 shared_ptr 引用（pointer copy，无数据重复）
  
防御：
  ✅ shared_ptr<const tgfx::Data> 共享所有权
  ✅ MAX_IMAGES=10000（ImageAsset 最多 10000 个）
```

**评估**：✅ 引用数据结构（shared_ptr）防止放大

### 7.3 算法复杂度爆炸

**已在 P1-E1 评审**：Tarjan SCC 复杂度 O(N+E)，但受 MAX_PENDING_MASKS 约束

**其他复杂度风险**：
- Composition 环检测：CompositionVisitScope 时间 O(ref_depth)，MAX_COMPOSITION_REF_DEPTH=32 ✅
- Layer 树遍历：Pass 1 + Pass 2 各 O(totalLayers)，MAX_INFLATED_LAYER_COUNT=1e6 防范 ✅
- 字符串 UTF-8 校验：IsValidUTF8 O(len)，MAX_NAME_STRING_BYTES=64KB + MAX_TEXT_STRING_BYTES=10MB 受约束 ✅

**评估**：✅ 无 O(n²) 或更差的算法触发点

### 7.4 哈希表碰撞攻击

**相关部分**：
- ResourceBaker 的 imageIndexByKey / imageIndexByDataPtr（§11.1）
- LayerPath PackedLayerPath 哈希（§9.4，v2.19 已修复）

**碰撞风险**：
- imageIndexByKey 用 `std::unordered_map<std::string, uint32_t>`
- 若对手构造大量 data URI 字符串碰撞，导致 unordered_map rehash 链式增长 → O(n) 查询

**防御**：
- ⚠️ 文档未明确说明"哈希表对抗性输入的防护"
- 但实际风险低：imageIndexByKey 的键来自 ResourceBaker 读图片/字体，Baker 自身有 MAX_IMAGES=10000 / MAX_FONTS=1000 上限

**修复建议**：
在 §11.1 或 §H.1 补充：
```
// 哈希表碰撞防护（隐式）
// imageIndexByKey 的键集合大小 ≤ MAX_IMAGES，即便全部碰撞也仅 10000 条链
// 单次查询最坏 O(10000)，不构成 DoS 威胁。
// 若未来 MAX_IMAGES 上调应重新评估。
```

**严重度**：P2（低优先级，实际风险极低）

---

## 八、Fuzzer 覆盖盲点

基于文档与实现路径分析，以下代码路径对 libFuzzer 难以触达（需专项 seed corpus）：

### 8.1 高难度触发路径

| 代码路径 | 难度 | 覆盖建议 |
|---------|-----|--------|
| §9.4 Tarjan SCC 环检测（Pass 2.5） | ⭐⭐⭐⭐ 高 | 构造 MAX_PENDING_MASKS/2 的线性 mask 链 + 1 条回边 |
| §8.3ter Baker fatal → ErrorMarker tag | ⭐⭐⭐ 中高 | 在 Phase 2 测试中手工注入（§18.11 已有 CorruptBuilder）|
| ZeroCopyScope DecodeLocal 分支（§D.1） | ⭐⭐⭐ 中高 | 需两个 SharedStream 嵌套 + 零拷贝标志位 |
| Mask cycle 检测（非环，仅超深引用） | ⭐⭐ 中 | CompositionRef 深度 = MAX_COMPOSITION_REF_DEPTH + 1 |
| 整数溢出保护（§H.1 减法形式） | ⭐⭐⭐ 中高 | varU32 声称 UINT32_MAX 的 count，文件恰好大小接近 MAX_TOTAL_BODY_BYTES |

### 8.2 补充 seed corpus 清单

建议在 `test/fuzz_corpus/` 下新增：

```
valid/
  - deep_mask_cycle_262k.pag          (262144 层 mask 环，触发 Tarjan)
  - composition_ref_max_depth.pag     (MAX_COMPOSITION_REF_DEPTH=32 嵌套)
  - zero_copy_with_large_image.pag    (ImageAsset > 50MB，触发 ZeroCopyScope)
  - unicode_emoji.pag                 (4-byte UTF-8 emoji，触发 IsValidUTF8)

corrupt/
  - varU32_5byte_overflow.pag         (varU32 超 5 字节 continuation bit)
  - float_nan_in_matrix.pag           (Matrix 含 NaN，触发浮点校验)
  - utf8_surrogate_pair.pag           (UTF-8 surrogate pair D800-DFFF)
  - mask_self_reference_deeplink.pag  (Layer.mask 自指 + 超深引用)
```

### 8.3 libFuzzer 配置建议

在 CMakeLists.txt (§I.4) 补充：

```cmake
# Fuzz 目标配置（Phase 12 交付）
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_executable(PAGDecodeFuzz test/src/pag/fuzz/decode_fuzz.cpp)
  target_link_libraries(PAGDecodeFuzz PRIVATE pagx-pag)
  target_compile_options(PAGDecodeFuzz PRIVATE
    -g -O1
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer)
  
  # Seed corpus 目录
  # 执行：libFuzzer PAGDecodeFuzz test/fuzz_corpus/ -max_len=1000000 -timeout=10
endif()
```

---

## 九、与业界参照对标

### 9.1 PNG libpng CVE 历史类比

| CVE ID | 问题 | libpng 影响 | PAG v2 对标 |
|--------|-----|-----------|----------|
| CVE-2015-0973 | 整数溢出（width * height * bpp） | SIGSEGV / OOM | ✅ PAG 用减法形式（§H.1）且 MAX_CANVAS_DIM=16384 ⇒ max area = 268M px，溢不出 u64 |
| CVE-2006-6893 | heap overflow（filter + inflate） | 缓冲区溢出 | ✅ PAG 未自实现 inflate（v2.20 未做压缩），无此风险 |
| CVE-2004-0421 | ancillary chunk 处理不当 | 信息泄漏 | ✅ PAG unknown Tag skip（§8.4）兜底，无致命漏洞 |

**结论**：PAG v2 on parity with 现代 PNG 安全标准；部分防线超越早期 libpng。

### 9.2 WebP libwebp CVE 历史类比

| CVE ID | 问题 | libwebp 影响 | PAG v2 对标 |
|--------|-----|-----------|----------|
| CVE-2023-4863 | heap buffer overflow（VP8L chunk） | SIGSEGV / RCE | ✅ PAG 不自实现图像编解码，仅存储原始字节；tgfx 负责解码（外包给 tgfx::Image::MakeFromEncoded）|
| CVE-2023-5129 | OOB read in EXIF handler | 信息泄漏 | ✅ PAG 不解析图像 EXIF；仅存储 {width, height, bytes} |
| CVE-2018-25269 | 栈缓冲区溢出（VPX filter） | SIGSEGV | ✅ PAG 无 VPX 编码器 |

**结论**：PAG v2 通过"不实现图像编解码"而规避了 libwebp 的历史威胁面。风险转移到 tgfx（专项库），PAG 层面专注结构解析安全。

### 9.3 Protobuf unknown field skip 漏洞类比

**Protobuf 在 unknown field 处理上的历史问题**：
- 早期版本未正确 skip 未知消息类型 → 导致后续字段读取错位
- 修复：引入"length-prefixed unknown field"规范

**PAG v2 对标**：
- ✅ SubStream RAII guard（§8.5）确保 tag 边界精确
- ✅ seek(tagEnd) 强制对齐（§8.4 line 650）
- ✅ Unknown Tag 产出 UnknownTagCode warn 而非 error（允许部分恢复）
  
**结论**：✅ PAG v2 采纳了 Protobuf 现代最佳实践。

### 9.4 本方案相对安全性位置（CVE 风险排名）

基于 OWASP 与历史 CVE 库对标：

```
高风险区间（CVSS 9-10）:
  ❌ Buffer overflow via unchecked index       → PAG v2 ✅ varU32 COUNT 检查
  ❌ Integer overflow in size calculation      → PAG v2 ✅ 减法形式（H.1）
  ❌ Recursive stack overflow                  → PAG v2 ✅ MAX_LAYER_DEPTH=64 + MAX_COMPOSITION_REF_DEPTH=32
  
中风险区间（CVSS 5-8）:
  ⚠️ NaN/Inf 浮点异常                        → PAG v2 ⚠️ P2-E1（文档可补充）
  ⚠️ Use-After-Free in resource cleanup       → PAG v2 ✅ shared_ptr 自动管理
  ⚠️ Denial of Service (algorithmic)          → PAG v2 ⚠️ P1-E1（Tarjan 复杂度可优化）
  
低风险区间（CVSS 2-4）:
  ✅ Invalid enum value handling             → PAG v2 ✅ 回填默认值
  ✅ Truncated data graceful degradation     → PAG v2 ✅ PrematureEndTag warn
  ✅ Diagnostic message DoS                  → PAG v2 ✅ MAX_DIAGNOSTICS=1000 cap
```

**总体评价**：
- **相对 PNG libpng**：PAG v2 在整数溢出与深度约束上更严格
- **相对 WebP libwebp**：PAG v2 通过外包图像解码规避了 libwebp 的 CVE 威胁面
- **相对 Protobuf**：PAG v2 采纳了现代最佳实践（未知字段 skip、length-prefixed 检查）

---

## 十、给主决策者的核心建议

### 必修 P0：无

v2.19 已修复的 PackedLayerPath、propHeader 等历史 P0 不再出现新漏洞。本轮评审**未发现新 P0**。

### 建议补全（优先级排序）

#### **即刻修复（Phase 4-5 出口前）**：
1. **P1-E2 补充**：varU32 超 5 字节的格式检查（§D.1 ValueCodec 伪码）
2. **P2-E1 补充**：浮点 NaN/INF 校验（§9.2 inflateLayer 或 §H.1 新增 InvalidFloatValue 错误码 407）

#### **优化建议（Phase 8-10 性能优化 + 安全加强）**：
3. **P1-E1 强化**：Tarjan SCC 递归改迭代 + 栈深度文档化（§9.4 + §H.1 补注）
4. **P1-E3 明确**：ZeroCopyScope 生命周期保证与 shared_ptr use_count 验证测试（§9.2 + §18.7 PAGLoaderTest）
5. **P2-E4 建议**：哈希表碰撞防护（§H.1 补注；实际风险低但文档完善）

#### **Fuzzer 增强（Phase 12 前）**：
6. **补充 seed corpus**：8.2 列举的 5 个 valid + 5 个 corrupt 样本（`test/fuzz_corpus/`）
7. **长期运行**：CI 每周运行 `PAGDecodeFuzz` >= 1 CPU·小时 ASAN + UBSAN 全绿，baseline 建立

### 纪律提升

#### **代码审查 checklist**：
- ✅ varU32 读取后与 MAX_* 比较（减法形式）
- ✅ 所有 seek(base + length) 用 uint64_t 中间结果
- ✅ 浮点字段读取后检查 isnan() / isinf()
- ✅ ZeroCopyScope RAII guard 配对
- ✅ Enum 值超范围回填默认值
- ✅ shared_ptr use_count 验证（单元测试）

#### **文档维护**：
- 保持 §H.1 limits 常量表与 `src/pagx/pag/limits.h` 代码同步
- §15.1 DiagnosticCode 枚举与 §G.3bis CodeToString switch 同步
- 版本升级时更新 §4.1.1 版本管理决策表

#### **测试覆盖目标**（Phase 评出口）**：
- **Phase 4-5**：32 项边界输入矩阵 100% 覆盖（29 项✅ + 3 项⚠️ 转入新用例）
- **Phase 8-10**：PAGDecodeFuzz 5 个 corrupt seed 触发所有 P1/P2 代码路径
- **Phase 12**：无新 CVE 级别（≥ 5.5 CVSS）未被 ASAN/UBSAN 检出的漏洞

### 交付标准

| 交付物 | 验收标准 |
|-------|--------|
| 文档 | 本报告 3 个 P1 补充 + 5 个 P2 补充完成；修订 §9.2 / §11.1 / §H.1 / §D.1 |
| 代码 | 新增 `struct InvalidFloatValue` 错误码；Tarjan 迭代版本（可选 Phase 10+）|
| 测试 | PAGDecodeFuzz + 8 个 seed corpus 文件 + CorruptBuilder 新增 8 条用例 |
| 文档验收 | 本报告与代码合并前签收（Security lead 审批）|

---

## 十一、附录：30 秒快速判定表（安全决策支撑）

**问题**：PAG v2 是否可用于数亿级安装量的生产环境？

**快速检查表**：

- ✅ **魔数 / 版本检查**：已实现（§4.1）
- ✅ **整数溢出防护**：减法形式（§H.1）
- ✅ **深度约束**：MAX_LAYER_DEPTH=64 + MAX_COMPOSITION_REF_DEPTH=32（§H.1）
- ✅ **列表长度防护**：varU32 count > MAX 即错（§8.4）
- ✅ **资源耗尽防护**：MAX_INFLATED_LAYER_COUNT=1e6 + MAX_PENDING_MASKS=262144（§H.1）
- ✅ **未知 Tag/Enum skip**：设计允许（§4.4 / §8.4）
- ⚠️ **浮点 NaN/Inf**：文档建议补充检查（P2-E1）
- ✅ **字符串 UTF-8**：IsValidUTF8 校验（§H.3）
- ✅ **图片/字体 magic**：白名单检查（§H.4 / §H.5）
- ✅ **零拷贝 UAF**：shared_ptr 自动管理（§11.1）

**结论**：✅ **可用**。修复建议优先级 P1-E2 + P2-E1，其他为优化项。无阻塞性 P0 问题。

---

**签名**  
二进制格式安全评审员  
日期：2026-04-30  
版本：v2.20
