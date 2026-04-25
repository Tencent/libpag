# 代码规范合规性评审报告

## 概览

对 PAGX→HTML 模块（`src/pagx/html/` + `include/pagx/HTMLExporter.h`）进行全面代码规范合规性评审。

**总体评分：P1-P2 级违规 15 处**

关键发现：
- **禁用特性合规**：未使用 `dynamic_cast`、异常机制（`throw`/`try`/`catch`）✅
- **版权声明**：所有文件年份均为 2026，符合新增文件要求 ✅  
- **头文件保护**：所有头文件使用 `#pragma once` ✅
- **命名空间**：头文件中未发现 `using namespace std;` ✅
- **代码格式**：`codeformat.sh` 通过，无格式问题 ✅
- **注释质量**：`include/pagx/HTMLExporter.h` API 文档完整 ✅

## 违规清单

### [STYLE-01] 命名违规：k 前缀 (Google 风格，禁用)

**严重度：P2**

静态常量不应使用 `k` 前缀。应改用 `全大写_下划线` 格式。

#### 违规 1.1：HTMLStyleExtractor.cpp:489
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:489`
- **代码**：
  ```cpp
  static const std::unordered_map<std::string, int> kOrder = {
  ```
- **违规项**：`kOrder`（Google 风格）
- **应为**：`CSS_PROPERTY_ORDER` 或 `ORDER`
- **上下文**：函数内静态常量，用于 CSS 属性排序

#### 违规 1.2：HTMLWriterText.cpp:104
- **位置**：`src/pagx/html/HTMLWriterText.cpp:104`
- **代码**：
  ```cpp
  static const float kWidths[94] = {
  ```
- **违规项**：`kWidths`（Google 风格）
- **应为**：`CHAR_WIDTHS` 或 `WIDTHS`
- **上下文**：函数内静态常量数组，存储 ASCII 字符宽度比例

---

### [STYLE-02] 禁止 lambda 表达式（应改用显式方法/函数）

**严重度：P2**

项目规范禁止 lambda 表达式，应提取为具名函数或成员方法以提升可读性与可维护性。

#### 违规 2.1：HTMLStyleExtractor.cpp:577（std::stable_sort）
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:577-579`
- **代码**：
  ```cpp
  std::stable_sort(sorted.begin(), sorted.end(), [](const CSSProperty& a, const CSSProperty& b) {
    return CSSPropertyOrder(a.name) < CSSPropertyOrder(b.name);
  });
  ```
- **应为**：提取为具名比较函数 `bool CompareCSSProperty(const CSSProperty& a, const CSSProperty& b)`

#### 违规 2.2：HTMLStyleExtractor.cpp:632（std::sort）
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:632-634`
- **代码**：
  ```cpp
  std::sort(sortedGroups.begin(), sortedGroups.end(), [](const GroupInfo& a, const GroupInfo& b) {
    return a.firstTagIndex < b.firstTagIndex;
  });
  ```
- **应为**：提取为具名比较函数 `bool CompareGroupInfo(const GroupInfo& a, const GroupInfo& b)`

#### 违规 2.3：HTMLWriterText.cpp:219（std::sort）
- **位置**：`src/pagx/html/HTMLWriterText.cpp:219-221`
- **代码**：
  ```cpp
  std::sort(randList.begin(), randList.end(),
            [](const std::pair<uint32_t, size_t>& a, const std::pair<uint32_t, size_t>& b) {
              return a.first < b.first;
  ```
- **应为**：提取为具名比较函数

#### 违规 2.4：HTMLWriterFilter.cpp:177（auto ensureAlpha）
- **位置**：`src/pagx/html/HTMLWriterFilter.cpp:177-188`
- **代码**：
  ```cpp
  auto ensureAlpha = [&]() -> std::string {
    if (!alphaDirty) {
      return alphaResult;
    }
    // ... 复杂逻辑
  };
  ```
- **应为**：提取为成员方法 `std::string EnsureAlpha()` 或私有函数
- **理由**：闭包方式隐蔽，难以追踪捕获变量的生命周期

#### 违规 2.5：HTMLWriterLayer.cpp:887（auto emitLeftTop）
- **位置**：`src/pagx/html/HTMLWriterLayer.cpp:887-889`
- **代码**：
  ```cpp
  auto emitLeftTop = [&](float x, float y) {
    style += ";left:" + FloatToString(x) + "px;top:" + FloatToString(y) + "px";
    positionSet = true;
  };
  ```
- **应为**：提取为方法或传递参数，避免闭包修改外层变量
- **理由**：修改捕获的 `style` 和 `positionSet` 变量，隐蔽且难以追踪

#### 违规 2.6-2.9：HTMLWriterShape.cpp forEach 回调（4处）
- **位置**：
  - `src/pagx/html/HTMLWriterShape.cpp:193`
  - `src/pagx/html/HTMLWriterShape.cpp:263`
  - `src/pagx/html/HTMLWriterShape.cpp:316`
  - `src/pagx/html/HTMLWriterShape.cpp:513`
  - `src/pagx/html/HTMLWriterShape.cpp:587`

- **代码示例**（193 行）：
  ```cpp
  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        // ...
    }
  });
  ```
- **应为**：提取为私有方法 `void ProcessPathVerb(PathVerb verb, const Point* pts)`，或创建内部 Visitor 类
- **理由**：5 处类似回调构成代码重复模式，应统一为方法

#### 违规 2.10：HTMLWriterShape.cpp:1174（auto repeatFor）
- **位置**：`src/pagx/html/HTMLWriterShape.cpp:1174-1176`
- **代码**：
  ```cpp
  auto repeatFor = [](TileMode m) -> const char* {
    return m == TileMode::Repeat ? "repeat" : "no-repeat";
  };
  ```
- **应为**：提取为全局或类静态方法 `static const char* TileModeToRepeatCSS(TileMode m)`

#### 违规 2.11-2.13：HTMLWriterText.cpp forEach 回调（2处）
- **位置**：
  - `src/pagx/html/HTMLWriterText.cpp:470`
  - `src/pagx/html/HTMLWriterText.cpp:1199`

- **应为**：同上，提取为私有方法或 Visitor 模式

---

## 通过项

### 禁止特性检查 ✅

- **`dynamic_cast`**：未发现 ✅
- **异常（`throw`/`try`/`catch`）**：未发现 ✅
- **`mutable` 成员**：未发现 ✅
- **`using namespace std;`（在 .h 中）**：未发现 ✅

### 头文件保护 ✅

所有头文件均使用 `#pragma once`：
- `include/pagx/HTMLExporter.h`
- `src/pagx/html/HTMLWriter.h`
- `src/pagx/html/HTMLBuilder.h`
- `src/pagx/html/HTMLStyleExtractor.h`
- `src/pagx/html/FontHoist.h`
- `src/pagx/html/HTMLPlusDarkerRenderer.h`
- `src/pagx/html/HTMLStaticImageRenderer.h`

### 版权年份 ✅

所有文件年份均为 2026（符合新增文件要求）：
- `include/pagx/HTMLExporter.h`：Copyright (C) 2026
- `src/pagx/html/HTMLStyleExtractor.cpp`：Copyright (C) 2026（创建于 2026-04-23）
- `src/pagx/html/HTMLWriterText.cpp`：Copyright (C) 2026（创建于 2026-04-07）
- 其他文件：Copyright (C) 2026

### 代码格式 ✅

`codeformat.sh` 执行完成，无格式问题。

### 注释规范 ✅

`include/pagx/HTMLExporter.h` 公开 API 文档完整（Doxygen 风格）：
- `HTMLExportOptions` 结构体：各成员有详细描述
- `HTMLExporter::ToHTML()`：参数 `@param`、返回值 `@return` 完整
- `HTMLExporter::ToFile()`：参数 `@param`、返回值 `@return` 完整

示例（良好）：
```cpp
/**
 * Exports a PAGXDocument to an HTML string. The output is a standalone HTML fragment containing
 * CSS styles and SVG elements that visually reproduce the PAGX content in modern browsers.
 * @param document The PAGX document to export.
 * @param options Export options controlling output formatting.
 * @return The generated HTML string, or an empty string if the document has no content.
 */
static std::string ToHTML(const PAGXDocument& document, const Options& options = {});
```

---

## 统计汇总

| 类别 | 违规数 | 严重度 |
|------|--------|--------|
| k 前缀命名 | 2 | P2 |
| lambda 表达式 | 13 | P2 |
| **总计** | **15** | **P2** |

---

## 总结

PAGX→HTML 模块在 **禁止特性、头文件保护、版权声明、代码格式** 方面全部合规。

主要违规集中在 **2 个命名问题** 和 **13 个 lambda 表达式**，均为 P2 级（代码风格/可维护性）。

### 建议修复优先级

1. **高优先**：替换 2 个 `k` 前缀常量为 `全大写_下划线` 格式（快速修复）
2. **中优先**：逐步重构 13 个 lambda 为具名函数/方法（可增量进行）
   - 先重构算法比较函数（stable_sort、sort）：3 处
   - 再提取状态捕获 lambda（ensureAlpha、emitLeftTop）：2 处  
   - 最后统一 pathData.forEach 的 Visitor 模式：8 处

### 对未来维护的影响

- **可读性**：lambda 风格隐蔽，审查者难以快速理解捕获变量修改
- **可维护性**：重复的 pathData.forEach 模式 (8 处) 应统一为 Visitor，便于后续扩展
- **一致性**：与项目其他模块的命名与函数风格保持一致
