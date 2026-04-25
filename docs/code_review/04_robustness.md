# 健壮性与防御性编程评审报告

## 概览

PAGX→HTML 导出模块包含 13 个核心源文件和 1 个公开 API 头文件。本评审覆盖 `src/pagx/html/**/*.{h,cpp}` 及 `include/pagx/HTMLExporter.h`，按照空指针解引用、边界值处理、数值转换、容器访问、字符串转义、递归栈溢出、外部资源降级、异常传播和静默错误等九个维度进行全面检查。

共发现 **8 个问题**，其中 **4 个 P0 阻塞问题**（空指针崩溃和转义缺失）、**3 个 P1 重要问题**（防御不足、无限递归风险）、**1 个 P2 改进建议**（文档完善）。

---

## Issue 清单

### [ROBUST-01] LinearGradient 空 colorStops 导致 front()/back() 崩溃

- **位置**：`src/pagx/html/HTMLWriterColor.cpp:105-106, 135, 147`
- **严重度**：P0
- **问题**：
  - 第 80 行检查 `!g->colorStops.empty()` 后，第 105 行直接调用 `g->colorStops.front()` 和 `g->colorStops.back()`，且第 135 行对 `g->colorStops.back()` 取指针解引用
  - 第 147 行无条件调用 `CSSStops(g->colorStops)`，而 `CSSStops()` 函数内部需要依赖 colorStops 非空
  - 若梯度定义中不含任何颜色停止点，容器为空时发生 undefined behavior 或崩溃

- **代码证据**：
```cpp
if (boxWidth > 0 && boxHeight > 0 && !g->colorStops.empty()) {
  // ... 线性变换计算 ...
  if (std::abs(endT - startT) > 1e-4f) {
    auto* firstStop = g->colorStops.front();  // [L105] 安全
    auto* lastStop = g->colorStops.back();    // [L106] 安全
    // ...
    if (endT < 1.0f) {
      float mappedLast = startT + g->colorStops.back()->offset * (endT - startT);  // [L135] 解引用指针前未检查
```

- **触发场景**：
  - PAGX 文档包含空梯度定义（colorStops 为空）且梯度作为颜色源
  - fallback path（第 147 行）传入 front()/back() 依赖的空容器

- **建议**：
  - 在第 105-106 行之前显式检查 `if (g->colorStops.empty()) return ...;`
  - 确保 `CSSStops()` 对空容器返回合理的默认值（如 "transparent"）或提前返回
  - 第 135 行访问指针前再次验证 colorStops 非空

---

### [ROBUST-02] RadialGradient/ConicGradient 同样的空 colorStops 解引用

- **位置**：`src/pagx/html/HTMLWriterColor.cpp:181, 184, 203-204`（RadialGradient）; 类似的 ConicGradient 路径
- **严重度**：P0
- **问题**：
  - 第 150-184 行处理 RadialGradient 时，直接调用 `CSSStops(g->colorStops)` 而未在函数入口检查容器非空性
  - 若梯度的 colorStops 为空，CSSStops() 内部的 front()/back() 调用崩溃
  - ConicGradient 分支（第 200+）同样风险

- **代码证据**：
```cpp
case NodeType::RadialGradient: {
  auto g = static_cast<const RadialGradient*>(src);
  // 无 colorStops.empty() 检查
  // ...
  return "radial-gradient(ellipse " + FloatToString(r) + "px " + FloatToString(ry) +
         "px at " + FloatToString(c.x) + "px " + FloatToString(c.y) + "px," +
         CSSStops(g->colorStops) + ")";  // [L181] 直接访问可能为空的容器
```

- **触发场景**：
  - 任何空梯度定义被转换为 CSS 形式

- **建议**：
  - 在所有梯度处理分支中统一添加 `if (g->colorStops.empty()) return "transparent";`
  - 或在 CSSStops() 中实现防御性检查，为空容器返回 "transparent"

---

### [ROBUST-03] HTMLBuilder::_tags 栈为空时解引用

- **位置**：`src/pagx/html/HTMLBuilder.h:60, 76, 86`
- **严重度**：P0
- **问题**：
  - `closeTagSelfClosing()` 调用 `_tags.back()` 而未检查容器非空
  - `closeTag()` 和 `closeTagWithText()` 同样访问 `_tags.back()` 未先验证
  - 若 openTag/closeTag 配对错误或调用顺序异常（如多余的 closeTag），`_tags` 空时崩溃

- **代码证据**：
```cpp
void closeTagSelfClosing() {
  auto& tag = _tags.back();  // [L60] 若 _tags 为空则 UB/崩溃
  if (std::strcmp(tag, "div") == 0 || std::strcmp(tag, "span") == 0) {
    // ...
  }
  _tags.pop_back();
}

void closeTag() {
  _level--;
  indent();
  _buf += "</";
  _buf += _tags.back();  // [L76] 若 _tags 为空则 UB/崩溃
  _buf += '>';
  // ...
  _tags.pop_back();
}
```

- **触发场景**：
  - 异常路径中提前调用 closeTag()（如 error handling）
  - 多个线程并发访问同一个 HTMLBuilder（虽然代码单线程，但若有重构需防守）

- **建议**：
  - 在 closeTag* 系列方法中添加 `if (_tags.empty()) return;` 或 `assert(!_tags.empty());`
  - 考虑提供调试模式验证 tag 栈的完整性

---

### [ROBUST-04] HTMLWriter::colorToCSS 解引用空指针未检查

- **位置**：`src/pagx/html/HTMLWriterColor.cpp:36-43`
- **严重度**：P0
- **问题**：
  - 函数第 36 行接受 `const ColorSource* src` 参数
  - 第 38-42 行检查 `if (!src)` 并返回 "transparent"
  - 但之后所有 `static_cast<const XxxGradient*>(src)` 调用时，若 src 为非空但类型不匹配或后续代码修改时疏漏检查，仍可能解引用不安全指针
  - **关键风险**：若新增梯度类型或 node type 转换错误时，cast 后的指针使用未验证

- **代码证据**：
```cpp
std::string colorToCSS(const ColorSource* src, float* outAlpha, ...) {
  if (!src) {  // [L38] 检查 src 本身
    if (outAlpha) {
      *outAlpha = 1.0f;
    }
    return "transparent";
  }
  switch (src->nodeType()) {
    case NodeType::SolidColor: {
      auto sc = static_cast<const SolidColor*>(src);  // [L46] 假定类型正确
      // sc->color 解引用未验证 sc 非空
```

- **触发场景**：
  - ColorSource 继承链有新增子类但 colorToCSS 的 switch 分支不完整
  - outAlpha 为 nullptr 时第 40 行的解引用

- **建议**：
  - 确保 switch 语句覆盖所有 NodeType 或有 default 分支处理意外类型
  - 为 outAlpha 的间接访问添加显式检查（第 48, 54-55 等行已有，但需全覆盖）
  - 添加断言或运行时验证 static_cast 的结果（如 DCHECK）

---

### [ROBUST-05] 字符串转义不完整：SVG 属性值转义缺失

- **位置**：`src/pagx/html/HTMLWriterShape.cpp` 和 `HTMLWriterFilter.cpp`（SVG 属性设置处）
- **严重度**：P1
- **问题**：
  - HTMLBuilder 提供 `escapeAttr()` 和 `escapeHTML()`，但它们仅转义基本的 5 个字符 (`&`, `<`, `>`, `"`, `'`)
  - SVG 属性值（如 `<path d="...">` 的 d 属性、`<feComposite values="...">` 的 values 属性）通过 `addAttr()` 放入引号属性中，内容使用 `escapeAttr()`
  - **缺陷**：SVG path 数据、transform 矩阵、filter 参数中可能包含单引号、双引号、反斜杠等特殊字符，**未能可靠转义会导致属性值提前闭合或注入 SVG 代码**

- **代码证据**：
```cpp
// HTMLWriterFilter.cpp, line 243
_defs->addAttr("values", "0 0 0 0 " + FloatToString(s->color.red) + " ...");
// FloatToString() 输出的浮点数一般安全，但复杂的颜色或矩阵值若包含引号则风险

// HTMLWriterShape.cpp - SVG path d 属性
renderSVG(...) {
  // 构造 d 属性值包含 FloatToString() 结果和符号，一般安全但无防御
}
```

- **触发场景**：
  - 自定义字体名或图像 URL 包含双引号
  - 极端浮点值的字符串表示包含特殊字符（理论上浮点不会，但防守应做）

- **建议**：
  - 在 HTMLBuilder 中添加 `escapeSVGAttr()` 函数，转义 SVG 标准要求的字符集
  - 或对所有外部输入（字体名、URL、文本内容）再应用一层 URL 编码或 Unicode escape
  - 特别是对 `_defs->addAttr()` 的调用进行审计

---

### [ROBUST-06] HTMLStyleExtractor 对格式错误的 HTML 无防守

- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:154-202`（Tokenize 函数）
- **严重度**：P1
- **问题**：
  - `Tokenize()` 解析 HTML 属性时，在循环中多次访问 `html[pos]`、`html[attrPos]` 等下标，**未在每次访问前检查边界**
  - 第 162 行 `if (pos + 1 >= html.size()) break;` 检查，但随后大量 `html[pos+N]` 访问未完全保护
  - 例如第 165 行 `html[pos + 3]` 在 `pos + 3 < html.size()` 检查后访问，但第 198-200 行 `html[nameEnd]` 则进行了循环条件但无额外边界检查

- **代码证据**：
```cpp
while (attrPos < html.size()) {
  while (attrPos < html.size() && (html[attrPos] == ' ' || ...)) {
    attrPos++;
  }
  if (attrPos >= html.size() || html[attrPos] == '>' || ...) break;  // 防守
  // ...
  while (attrPos < html.size() && html[attrPos] != '=' && ...) {
    attrPos++;  // 循环保证 attrPos < html.size()
  }
  std::string attrName = ToLower(html.substr(attrNameStart, attrPos - attrNameStart));
  // attrPos 更新后再次进入循环，可能到达末尾
  if (attrPos >= html.size() || html[attrPos] != '=') continue;  // 防守不足
}
```

- **触发场景**：
  - 格式错误或截断的 HTML 文档（缺少闭合 `>`）
  - 极长属性值未闭合
  - 恶意输入

- **建议**：
  - 为每个 `html[index]` 访问前添加显式 `if (index < html.size())` 或使用 safe accessor
  - 或在循环条件中综合所有依赖的边界

---

### [ROBUST-07] 递归调用无深度限制：层级嵌套栈溢出风险

- **位置**：`src/pagx/html/HTMLWriterLayer.cpp`（writeLayer → writeElements → writeLayer 的递归链），`HTMLWriterText.cpp`（writeRepeater、writeTextPath 递归）
- **严重度**：P1
- **问题**：
  - `writeLayer()` 处理 Layer 的 children 时，调用 `writeElements()`
  - `writeElements()` 遍历 elements，对每个 Group 递归调用 `writeGroup()`
  - `writeGroup()` 再次调用 `writeElements()`，最终回到 `writeLayer()`
  - **无深度计数或递归限制**，若 PAGX 文档包含很深的嵌套层级（如 200+ 层），可能导致栈溢出

- **代码证据**：
```cpp
// HTMLWriterLayer.cpp, writeElements()
for (auto* element : elements) {
  // ... handle geometry, fill, stroke ...
  case NodeType::Group: {
    auto group = static_cast<const Group*>(element);
    writeGroup(out, group, alpha, distribute);  // 递归进 writeGroup
    break;
  }
}

// HTMLWriterText.cpp, writeGroup()
void HTMLWriter::writeGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute) {
  // ...
  writeElements(out, group->elements, 1.0f, false, LayerPlacement::Background);  // 递归进 writeElements
  // ...
}

// writeComposition() 中
for (auto* layer : comp->layers) {
  writeLayer(out, layer, alpha, distribute);  // 递归进 writeLayer
}
```

- **触发场景**：
  - 深层嵌套的 Group → Layer → Group → ... 结构
  - Composition 内多层级 Layer 堆积

- **建议**：
  - 在 HTMLWriterContext 中添加 `int recursionDepth` 计数器，每层递归时递增
  - 在 writeLayer/writeGroup/writeElements 入口检查 `if (_ctx->recursionDepth > MAX_DEPTH) { log warning; return; }`
  - 或扁平化遍历转换为迭代式处理

---

### [ROBUST-08] 浮点数 NaN/Inf 未防守导致格式字符串异常

- **位置**：`src/pagx/html/HTMLExporter.cpp:38-88`（RoundPxInStyle），`HTMLWriterUtils.cpp` 等数值格式化处
- **严重度**：P2
- **问题**：
  - FloatToString() 函数未展示，但推测使用 std::atof 或类似转换
  - 若 Layer/Element 的 position、size、transform 等属性包含 NaN 或 Inf，格式化后可能产生非法的 CSS 值（"NaN px"、"Infinity px"）
  - 浏览器会忽略或报错，导致样式无效但**不会立即崩溃**（P2 降低）

- **代码证例**：
```cpp
// HTMLExporter.cpp, line 80
float value = static_cast<float>(std::atof(numStr.c_str()));
auto rounded = CoordToString(value);  // 若 value 是 NaN，输出是什么？
```

- **触发场景**：
  - PAGX 文档由外部工具生成并包含无效数值
  - Matrix 计算产生 NaN（如向量 normalize 时除以零）

- **建议**：
  - 在 FloatToString() 中检查 `std::isnan(value)` 和 `std::isinf(value)`，返回 "0" 或日志警告
  - 在 Layer/Element 加载时验证所有浮点属性

---

## 总结与优先级排序

| ID | 标题 | 严重度 | 类型 | 行数 |
|---|---|---|---|---|
| ROBUST-01 | LinearGradient 空 colorStops 崩溃 | P0 | 空指针 | 105-147 |
| ROBUST-02 | RadialGradient/ConicGradient 空 colorStops | P0 | 空指针 | 181-204 |
| ROBUST-03 | HTMLBuilder _tags 栈为空解引用 | P0 | 容器访问 | 60,76,86 |
| ROBUST-04 | colorToCSS outAlpha 空指针解引用 | P0 | 指针解引用 | 36-48 |
| ROBUST-05 | SVG 属性值转义缺失 | P1 | 字符串转义 | 多处 |
| ROBUST-06 | HTMLStyleExtractor 边界检查不足 | P1 | 边界值 | 154-240 |
| ROBUST-07 | 递归深度无限制栈溢出风险 | P1 | 递归栈 | 多个文件 |
| ROBUST-08 | NaN/Inf 浮点值格式化 | P2 | 数值转换 | 多处 |

### 修复优先级

**必须立即修复（P0）**：
1. **ROBUST-01/02** - 空梯度导致程序崩溃，影响任何包含梯度的文档
2. **ROBUST-03** - HTMLBuilder 基础崩溃，影响所有 HTML 生成
3. **ROBUST-04** - colorToCSS 空指针是 P0 中风险最高的

**应立即修复（P1）**：
4. **ROBUST-05** - 转义缺失可能被注入攻击利用
5. **ROBUST-06** - 格式错误的 HTML 导致越界访问
6. **ROBUST-07** - 深层嵌套文档导致栈溢出

**可后续改进（P2）**：
7. **ROBUST-08** - NaN/Inf 处理仅影响美观性，不崩溃

### 建议补充防守

- 在 HTMLWriterContext 中增加 **错误聚集器** 以记录运行时警告而非崩溃
- 添加 **单元测试** 覆盖空容器、空指针、深层嵌套、格式错误输入的场景
- 考虑在关键路径添加 **断言**（debug 模式）或 **DCHECK** 宏（编译时可控）
- 为模块提供 **sanitizer 友好的构建选项**（如 ASAN）以在 CI 中自动检测
