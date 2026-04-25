// Copyright (C) 2026 Tencent. All rights reserved.

# PAGX → HTML 模块代码评审总报告

**评审范围**：`src/pagx/html/`（10 159 行）+ `include/pagx/HTMLExporter.h`（138 行），共 ~10 300 行。
**评审方式**：7 名评审员并行覆盖 7 个维度。详细分册见同目录 `01_*.md` ~ `07_*.md`。

## 1. 全局统计

| 维度 | 报告 | 总 Issue | P0 | P1 | P2 | P3 |
|------|------|---------:|---:|---:|---:|---:|
| 架构与模块职责 | [01_architecture.md](01_architecture.md) | 8 | 0 | 2 | 4 | 2 |
| 对外 API | [02_api.md](02_api.md) | 8 | 1 | 3 | 3 | 1 |
| 可维护性与可读性 | [03_maintainability.md](03_maintainability.md) | 18 | 0 | 2 | 5 | 11 |
| 健壮性与防御性编程 | [04_robustness.md](04_robustness.md) | 8 | 4 | 3 | 1 | 0 |
| 安全性 | [05_security.md](05_security.md) | 10 | 4 | 3 | 3 | 0 |
| 性能与可扩展性 | [06_performance.md](06_performance.md) | 15 | 0\* | 4\* | 8 | 3 |
| 代码规范合规性 | [07_style.md](07_style.md) | 15 | 0 | 0 | 15 | 0 |
| **合计** | | **82** | **9** | **17** | **39** | **17** |

\* 性能报告未显式使用 P0/P1 分档，此处按「关键/重要」归类。

## 2. P0 阻塞级 Issue（9 项，建议立即修复）

| 编号 | 类别 | 标题 | 位置 |
|------|------|------|------|
| ROBUST-01 | 健壮性 | LinearGradient `colorStops` 空容器 `front()/back()` 崩溃 | HTMLWriterColor.cpp |
| ROBUST-02 | 健壮性 | RadialGradient / ConicGradient 同类空容器崩溃 | HTMLWriterColor.cpp |
| ROBUST-03 | 健壮性 | `HTMLBuilder::_tags` 栈空时 `closeTag()` 解引用崩溃 | HTMLBuilder.h |
| ROBUST-04 | 健壮性 | `colorToCSS` `outAlpha` 为 nullptr 时解引用崩溃 | HTMLWriterColor.cpp |
| SEC-01 | 安全 | `font-family` 未转义导致 XSS（可逃逸 `</style>`） | HTMLWriterLayer.cpp:309 |
| SEC-02 | 安全 | 同上，另一处遗漏 | HTMLWriterLayer.cpp:418 |
| SEC-04 | 安全 | `image->filePath` 无 scheme 校验，允许 `file://` / `javascript:` | 图像路径处理 |
| SEC-09 | 安全 | 文件写入路径未校验，可 `../` 目录遍历 | `ToFile()` 及静态图写入 |
| API-XX (P0) | API | `ToHTML()` 返回值语义模糊（空串 / 部分内容无法区分失败） | HTMLExporter.h / .cpp |

## 3. P1 重要级 Issue（17 项，建议本迭代内处理）

**架构**（2）：HTMLWriter 聚集 7 个职责（ARCH-01）、HTMLPlusDarkerRenderer 通过 `const_cast` 和全局 context 破坏 API 契约（ARCH-02）。
**API**（3）：`indent` 字段在 Minify 下被静默忽略（文档不完整）、Compact/Pretty 价值边界不清、参数约定未覆盖线程安全/所有权。
**可维护性**（2）：SVG 边界框计算重复 5 处（MAINT-01，~500 行冗余）、魔法数字无命名常量（MAINT-02）。
**健壮性**（3）：SVG 属性值转义不完整、`HTMLStyleExtractor` 对错误 HTML 边界检查不足、递归无深度上限可致栈溢出。
**安全**（3）：`staticImgUrlPrefix` 调用者控制无校验（SSRF 风险）、MIME 无白名单、SVG `data:` URI 属性值转义需持续复核。
**性能**（4）：Pass 3 重复 `ParseStyleProperties`（~20–30% 浪费）、位图回退无缓存（~30–60% 浪费）、字符串拼接未 `reserve()`（潜在 O(n²)）、三次 AST 遍历未合并。

## 4. P2/P3（56 项）

详见各子报告。P2 聚焦于：
- 架构：坐标舍入两阶段处理产生二次开销、`HTMLStyleExtractor` 缺模块化、HTML/SVG 导出器代码重复、回退渲染器无统一框架、新增节点要改 8+ 个文件。
- 可维护性：`renderSVG` 597 行、`writeElements` 216 行且嵌套 5–6 层、缺 CSS 样式构造器、HTMLBuilder 硬编码字符串比较、TextPath 极端嵌套。
- 规范：2 处 `k` 前缀常量、13 处 lambda（`std::sort` 回调 3 / 闭包 2 / `pathData.forEach` 8）应改具名函数。

## 5. 建议的修复路线（三阶段）

### 阶段 A：稳定性与安全（P0，9 项）
一次性修复：容器判空（ROBUST-01/02/03/04）、转义与 URL 校验（SEC-01/02/04/09）、`ToHTML` 返回值语义（API P0）。
**预期**：消除崩溃与 XSS/路径遍历风险。不涉及架构改动。

### 阶段 B：重要修复（P1，17 项）
- API：补齐文档（indent 忽略条件、线程安全、所有权、错误传递约定）、讨论 Compact/Pretty 合并可行性。
- 健壮性：SVG 属性转义统一函数、`HTMLStyleExtractor` 边界、递归深度阈值。
- 安全：URL 白名单、MIME 白名单。
- 性能：Pass 3 缓存已解析 props、位图回退按 Layer hash 缓存、HTMLBuilder 预估容量 `reserve()`。
- 架构：`HTMLPlusDarkerRenderer` 去 `const_cast`。

### 阶段 C：结构化重构（P2 架构/可维护性，选项）
- HTMLWriter 拆分为 Layer/Shape/Text/Filter 四个访问者。
- SVG 边界框/颜色/矩阵计算抽到 `ExporterUtils`（HTML 与 SVG 共享）。
- 魔法数字具名；长函数（>200 行）拆分。
- CSS 样式构造器统一拼接。
**预期**：代码量降 ~15%，新增节点类型改动从 8+ 文件降到 2 文件。

### 阶段 D：规范清理（P2 规范，15 项）
- `kOrder` / `kWidths` → `ORDER` / `WIDTHS`。
- 13 处 lambda 改为具名方法或自由函数。
可与阶段 C 合并提交。

## 6. 待你确认的决策点

下列决策需要你拍板后我们再动手：

1. **先修 P0 吗？** 是否在本会话中直接修复 9 项 P0（范围明确、无架构改动），单独提一个 commit？
2. **`HTMLFormat` 三档精简？** api-reviewer 指出 Compact 与 Pretty 仅格式差异、业务价值重叠。是否合并为两档（默认 + Minify），或保留三档？
3. **`ToHTML` 错误语义：** 空串代表失败还是成功输出空？倾向方案：
   - 方案 A：保留 `std::string`，增加 `bool* outSuccess` / `Status` 结构；
   - 方案 B：改 `std::optional<std::string>` 返回；
   - 方案 C：改为 `bool ToHTML(doc, opts, std::string* out)` 风格。
4. **架构拆分（ARCH-01）是否纳入本轮？** HTMLWriter → 4 个访问者是一次较大的改动，影响 7000+ 行。可延后至独立分支。
5. **P3 处理策略：** 是否一律延后至「有变更顺手改」，仅跟踪不专项修？
6. **性能专项（PERF-02/06）：** 是否值得做缓存优化？当前 ~800ms 对用户体感是否已经够用？是否有更硬的指标？
7. **规范违规（lambda 13 处）：** 是否一次性清理？部分 `pathData.forEach(lambda)` 改为具名回调会牺牲局部可读性，是否酌情保留？

请针对以上 7 个问题给出方向（可整体回答 "全部做"、"只做 P0"，也可逐条指示）。我会依据你的决定生成后续修复任务并分派给团队。
