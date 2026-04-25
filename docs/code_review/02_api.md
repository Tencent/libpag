# 对外 API 评审报告

## 概览

本报告对 `HTMLExporter` 公开 API 进行了系统评审，涵盖接口设计、参数约定、文档完整性、与其他 Exporter 的一致性等维度。评审范围包括 `include/pagx/HTMLExporter.h`、`src/pagx/html/HTMLExporter.cpp`，对比参考了 `SVGExporter`、`PAGXExporter`、`PAGXImporter` 的设计模式。

共发现 **8 个 Issue**：P0 级 1 个（阻塞性），P1 级 3 个（重要），P2 级 3 个（一般），P3 级 1 个（改进）。

---

## Issue 清单

### [API-01] ToHTML 返回值语义模糊：空串代表"无内容"还是"失败"？
- **位置**：`include/pagx/HTMLExporter.h:125`
- **严重度**：P0
- **问题**：
  - 文档注释说"返回空串如果文档无内容"，但空字符串作为失败信号容易导致调用者困惑。
  - `ToFile` 在第 210 行将 `html.empty()` 当作失败条件来处理，但 `ToHTML` 文档未明确说明空串等同于错误。
  - 与 `SVGExporter::ToSVG` 的约定不一致：后者无相关文档说明。
  - 一个尺寸为 0×0 但包含有效元素的文档可能生成空字符串，被误判为无内容。
- **证据**：
  - `HTMLExporter.cpp:210-211`：`if (html.empty()) { return false; }`
  - `HTMLExporter.h:123`：文档注释明确提及"空串"但语义模糊
  - `SVGExporter.h:60`：ToSVG 无相应说明
- **建议**：
  1. 明确界定 `ToHTML` 的返回值约定：a) 空串只代表"无可渲染内容"不代表错误，b) 永远返回有效 HTML（包括空文档的最小框架），c) 引入返回值或输出参数表示成功/失败。
  2. 建议选择方案 c)，采用输出参数或返回 `optional<string>` 来区分"内容为空但成功"与"解析失败"。
  3. 保持与 SVGExporter 的一致性。

---

### [API-02] HTMLExportOptions 中 indent 字段在 Minify 模式下完全被忽略但仍然存在
- **位置**：`include/pagx/HTMLExporter.h:62-65`、`HTMLExporter.cpp:140-141`
- **严重度**：P1
- **问题**：
  - 文档注释明确提及"在 Minify 下忽略"，但字段仍被序列化和传递。
  - API 使用者可能误以为该字段对所有 format 都有效，导致调优困惑。
  - 增加了选项结构的复杂性，违反单一职责原则。
- **证据**：
  - `HTMLExporter.h:62-65`：文档说"Ignored when format is Minify"
  - `HTMLExporter.cpp:140-141`：`int indent = minify ? 0 : options.indent;` 强制覆盖
- **建议**：
  1. **短期**：在文档中明确注明"仅在 Compact/Pretty 下生效"，鼓励调用者验证使用场景。
  2. **长期**：考虑使用类型系统隐藏条件字段（例如 C++ variant 或分离 Options 结构）以避免 API 表面积增加。

---

### [API-03] HTMLFormat 枚举中 Compact 与 Pretty 的功能边界模糊
- **位置**：`include/pagx/HTMLExporter.h:29-49`
- **严重度**：P1
- **问题**：
  - `Compact`：HTML 按 indent 缩进，CSS 每行一条规则但无声明缩进。
  - `Pretty`：HTML 缩进，每个 CSS 声明单独成行加 2 空格缩进。
  - 名称 "Compact" 暗示尺寸优化，实际只影响 CSS 格式，效果不明显。用户难以判断何时选择 Compact vs Pretty。
  - `Minify` 的定义清晰，但 Compact/Pretty 的区分价值不高。
- **证据**：
  - `HTMLExporter.h:30-41`：文档说明中 Compact 和 Pretty 的差异仅在 CSS 格式（"无声明缩进"vs"每个声明成行"）
  - 大多数场景（开发调试）选择 Pretty，生产环境选择 Minify；Compact 处于尴尬位置。
- **建议**：
  1. 在文档中新增性能数据对比（Compact vs Pretty 的输出尺寸差异百分比）。
  2. 考虑将 Compact 改名为 `Standard` 或 `Normal` 以消除误导。
  3. 长期评估是否可以合并 Compact 与 Pretty，通过单一 Pretty 加可选 minify 布尔值替代。

---

### [API-04] ToHTML 参数采用 const 引用而 ToFile 文档未说明文件写入失败场景
- **位置**：`include/pagx/HTMLExporter.h:125`、`HTMLExporter.cpp:207-227`
- **严重度**：P1
- **问题**：
  - `ToHTML(const PAGXDocument& document, ...)` 采用 const 引用，但文档未明确说明调用期间是否可修改 document。
  - `ToFile` 返回 bool，文档仅说"成功返回 true"，未列举失败原因（文件权限、磁盘满、路径无效等）。
  - 与 `PAGXImporter::FromFile` 的返回值约定一致（返回 nullptr 代表失败），但原因文档缺失。
  - 调用者难以进行错误恢复（例如是否应该重试、是否应该回滚）。
- **证据**：
  - `HTMLExporter.h:118-135`：两个方法的文档都未提及 document 的生命周期约束或修改限制。
  - `HTMLExporter.cpp:207-227`：`ToFile` 有多个失败点（文件创建、目录创建、写入），但都一视同仁返回 false。
- **建议**：
  1. 在 ToHTML 文档中补充：document 参数在调用期间必须保持有效，不应修改，导出完成后无副作用。
  2. 在 ToFile 文档中补充失败原因的详细列表及建议的错误处理方式。
  3. 考虑添加一个异步版本或返回详细错误码的重载。

---

### [API-05] HTMLExportOptions 缺少边界值约束文档和校验
- **位置**：`include/pagx/HTMLExporter.h:54-107`
- **严重度**：P2
- **问题**：
  - `indent` 类型为 int，无范围限制说明。调用者可能传递负数、极大值，导致未定义行为。
  - `staticImgPixelRatio` 类型为 float，无最小/最大值范围约束。可能传递 0、负数或极大值。
  - `staticImgDir` 和相关路径字符串无 null-safety 或长度限制的提示。
  - 文档未说明各参数是否有合理范围或应用的默认备选。
- **证据**：
  - `HTMLExporter.h:65`：`int indent = 2;` 无范围说明。
  - `HTMLExporter.h:92`：`float staticImgPixelRatio = 2.0f;` 无范围说明。
  - `HTMLExporter.cpp` 实现中未见显式边界检查。
- **建议**：
  1. 为所有数值参数添加合理范围文档（indent >= 0, staticImgPixelRatio > 0, 等）。
  2. 在 ToHTML/ToFile 入口处添加参数校验，非法值应清晰记录警告或返回错误。
  3. 为路径参数添加长度和字符集限制说明。

---

### [API-06] 头文件缺少线程安全性和内存所有权说明
- **位置**：`include/pagx/HTMLExporter.h` 整体
- **严重度**：P2
- **问题**：
  - API 文档未说明 static 方法是否线程安全。用户不知道是否可以从多个线程并发调用 `ToHTML`/`ToFile`。
  - 参数所有权约定不明确。`document` 采用 const 引用，但文档未说明导出期间是否可被其他线程修改或销毁。
  - 生成的 HTML 中包含对 `staticImgDir` 的相对/绝对路径引用，文档未说明路径的生命周期和所有权。
- **证据**：
  - `HTMLExporter.h` 无 thread-safe 相关说明，对标 `SVGExporter.h` 也无相应说明。
  - 其他 PAG API（如 PAGFile）通常会明确标注"非线程安全"或"线程安全"。
- **建议**：
  1. 补充明确的线程安全性声明（建议：非线程安全，同一文档不应被多线程并发导出）。
  2. 补充 document 参数的生命周期约束。
  3. 补充生成 HTML 中对外部资源（如 staticImgDir）的引用的有效期说明。

---

### [API-07] extractStyleSheet 选项的 DOM 结构变化未充分文档化
- **位置**：`include/pagx/HTMLExporter.h:94-106`
- **严重度**：P2
- **问题**：
  - `extractStyleSheet` 为 true 时会改变生成的 DOM 结构（inline style → stylesheet + class 引用）。
  - 文档说明了尺寸优化（10-25%），但未说明对应的 CSS 选择器命名规则（`.ps0`, `.ps1`, ...）。
  - 用户如果需要后处理 HTML（例如注入自定义 CSS 或进行代码分析），无法预测 class 名称的格式。
  - 无说明 class 名称是否稳定（多次导出相同文档是否生成相同的 class 序列）。
- **证据**：
  - `HTMLExporter.h:95-102`：文档提及 `.ps0`, `.ps1` 但无详细说明。
  - `HTMLExporter.cpp` 中的 HTMLStyleExtractor 负责生成这些名称，但 API 层无对应文档。
- **建议**：
  1. 补充选择器命名规则的详细说明（例如 `.ps{sequential_id}`）。
  2. 补充"稳定性保证"说明（例如同一文档导出结果中 class 名称是否确定）。
  3. 提供 CSS 提取模式下的 DOM 结构示例（before/after）。

---

### [API-08] 参数类型一致性：document 参数采用 const 引用，PAGXImporter 返回 shared_ptr
- **位置**：`include/pagx/HTMLExporter.h:125,134`、`PAGXImporter.h:36,42,48`
- **严重度**：P3
- **问题**：
  - `HTMLExporter::ToHTML/ToFile` 采用 `const PAGXDocument& document`。
  - `PAGXImporter::FromFile/FromXML` 返回 `std::shared_ptr<PAGXDocument>`。
  - 同为 Public API 的两个接口对内存管理的约定不一致。新用户使用 Importer 获得 shared_ptr，然后传递给 Exporter 时需要解引用。
  - 虽然函数式（Exporter 接收引用，Importer 返回指针）是合理的，但这种不对称性可能导致混淆。
- **证据**：
  - `HTMLExporter.h:125`：`const PAGXDocument&`
  - `PAGXImporter.h:36`：`std::shared_ptr<PAGXDocument>`
  - 对标 SVGExporter/PAGXExporter 也采用相同约定（const 引用），因此不是 bug，但值得一致性审视。
- **建议**：
  1. 在两个 API 的文档中明确说明设计决策（为何 Importer 返回 shared_ptr，为何 Exporter 接收 const 引用）。
  2. 可考虑在 Importer/Exporter 头文件顶部新增"API 约定"注释段落，统一说明参数/返回值的所有权和生命周期。
  3. 短期无需改动代码，长期可在 PAG 2.0 或主版本升级时统一为 shared_ptr 或实现 rvalue reference 重载。

---

## 总结与优先级排序

### 严重度分布

| 级别 | 数量 | 标题 |
|------|------|------|
| **P0** | 1 | 返回值语义模糊（API-01） |
| **P1** | 3 | indent 忽略（API-02）、Compact vs Pretty 边界模糊（API-03）、参数约定不清（API-04） |
| **P2** | 3 | 边界值校验缺失（API-05）、线程安全未说明（API-06）、DOM 变化未文档（API-07） |
| **P3** | 1 | 参数类型一致性（API-08） |

### 建议处理顺序

1. **立即处理（P0）**：
   - **[API-01]** 明确 ToHTML 返回值的语义。建议采纳建议方案 c)，引入错误信号机制。

2. **本周处理（P1）**：
   - **[API-02]** 补充 indent 字段的详细文档，明确其在各 format 下的行为。
   - **[API-03]** 新增 HTMLFormat 的对比表和选择指南。
   - **[API-04]** 补充 ToFile 的失败原因列表和参数约束说明。

3. **计划优化（P2）**：
   - **[API-05]** 为所有参数添加范围约束和校验逻辑。
   - **[API-06]** 补充线程安全性和内存所有权声明。
   - **[API-07]** 补充 extractStyleSheet 下的 DOM 结构和 class 命名规则说明。

4. **长期改进（P3）**：
   - **[API-08]** 审视参数类型的一致性，可考虑在主版本升级时统一。

---

## 附注

本评审基于代码版本日期 2026-04-25。所有建议均面向 API 可用性和明确性，未涉及业务逻辑或性能优化。文档和代码样例应由 HTMLExporter 维护团队根据实际情况调整。
