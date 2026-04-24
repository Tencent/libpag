# PAGX → HTML CSS 抽取优化技术方案

> 历史设计文档。文中示例使用的 `pagx-root` / `pagx-layer` 标记类已于后续迭代中移除，
> 当前生成的 HTML 不再包含这两个类；根元素改用 `data-pagx-version` 属性标识，
> layer 元素通过样式特征（mix-blend-mode、display:flex、position+size 无 inset 等）被识别。
> 以最新 `spec/pagx_to_html.md` 为准。

## 背景

当前 `HTMLExporter` 生成的 HTML 全部使用内联 `style="..."` 承载 CSS。实测 77 个样本：

| 指标 | 数值 |
|------|------|
| 总字节 | 622 KB |
| `style` 属性总数 | ~3700 |
| 唯一 `style` 字符串 | 2798（跨文件） |
| 单次使用的 style 占比 | 87.6% |
| 每个唯一 style 平均被引用 | 1.57 次 |
| `<span>`（文本）总数 | 877 |
| 带相同 font 签名的多 span Layer | 大量存在 |

内联方案在动态坐标/transform 上无优化空间（每个 div 的 `left/top` 都不同），但**文本的 font 属性大量重复**（同 Layer 下多个 span 共享 font-family/size/color）。

## 优化目标

两阶段结合，降低 HTML 字节：

1. **Phase 1 — font hoist**：Layer 下所有直接子 Text 的 font 签名一致时，把 font 属性提到 Layer 的 style，利用 CSS 继承让 span 自动继承。**始终启用，无开关**。
2. **Phase 2 — HTMLStyleExtractor**：把全文件所有 `style="..."` 抽取为 `<head>` 内的 `<style>` 块，每个唯一 style 分配一个 class 名（`.ps0`, `.ps1`, ...）。**通过 `HTMLExportOptions::extractStyleSheet` 控制，默认 true**。

两阶段叠加后预期节省：

| 方案 | 总节省 | 备注 |
|------|-------|-----|
| 保持内联（baseline） | 0 | 当前状态 |
| 仅 Phase 1 | -11% | 69 KB |
| 仅 Phase 2 | -8.3% | 52 KB，16 个小文件反而变大 |
| Phase 1 + Phase 2 | **-17.4%** | 108 KB |

---

## 总体架构

```
┌──────────────────────────────────────────────────────────┐
│                HTMLExporter::ToHTML                       │
├──────────────────────────────────────────────────────────┤
│ Phase 1: HTMLWriter 流式 emit (font hoist，始终启用)      │
│   - Layer emit 前：扫描直接子 Text 的 font 签名            │
│   - 签名一致：append font-* 到 Layer.style                │
│   - 被 hoist 的 span：跳过 font-* 属性                    │
│   - 输出完整 HTML（仍为 inline style）                    │
├──────────────────────────────────────────────────────────┤
│ Phase 2: HTMLStyleExtractor::Extract（可选）              │
│   - 仅当 options.extractStyleSheet == true                │
│   - tokenizer 扫描所有 start tag                          │
│   - 抽取 style 值，分配 .psN                              │
│   - 重写标签：移除 style、合并 class                       │
│   - HTML entity 解码后写入 <style>                        │
│   - 插入到 </head> 之前                                   │
└──────────────────────────────────────────────────────────┘
```

---

## Phase 1：font hoist

### 1.1 目标

当 Layer 下所有渲染为 `<span>` 的直接子 Text 共享相同 font 属性时，把这些属性移到 Layer 的 style 上，让 span 通过 CSS 继承自动获得。

### 1.2 FontSignature

参与签名的属性（全部来自 Text 节点）：

| 属性 | 来源 | 比较方式 |
|-----|-----|--------|
| fontFamily | `Text::fontFamily` | 字面相等 |
| renderFontSize | `Text::renderFontSize()`（含 textScale） | 位相等 |
| bold | `Text::fontWeight` 为 bold | bool 相等 |
| italic | `Text::fontStyle` 为 italic | bool 相等 |
| lineHeight | Text 或 TextBox 层继承 | 位相等（NaN sentinel） |
| letterSpacing | `Text::letterSpacing` | 位相等（NaN sentinel） |

**未纳入签名**：
- `color`：颜色来自 Fill→ColorSource→SolidColor 链，不直接属于 Text 节点，签名收集时无法从 Text 直接获取
- `underline`：Text 节点无此字段

**不参与签名（永远留在 span）**：
- `position` / `left` / `top` / `transform`（per-instance 坐标）
- `-webkit-text-stroke`（fauxBold 相关，继承行为在部分 Chromium 版本不稳定）
- `writing-mode`（保守保留）
- `display:inline-block`
- `white-space`（实测稳定继承，但保守起见仍放 span；如后续测试验证可继承，可上浮）

**NaN sentinel 语义**：`lineHeight` / `letterSpacing` 用 NaN 表示 "未设置，继承默认"。比较时 `(isnan(a) && isnan(b)) || a == b`。

### 1.3 签名收集策略：严格一致

```
collect_uniform_signature(layer):
  texts = []
  for each element in layer.elements:
    if element is Layer:
      continue          # 嵌套 Layer 独立判定，不穿透
    if element is Group:
      for each text in element.elements:  # 穿透 Group
        if text is Text and text 渲染为 <span>:
          texts.append(text)
    if element is TextBox:
      for each child in element.elements:  # 穿透 TextBox
        if child is Text and text 渲染为 <span>:
          texts.append(child)
        if child is Group:
          for each text in child.elements:  # 穿透 TextBox 内的 Group
            if text is Text and text 渲染为 <span>:
              texts.append(text)
    if element is Text and text 渲染为 <span>:
      texts.append(text)

  if texts is empty: return empty
  sig = signature_of(texts[0])
  for t in texts[1:]:
    if signature_of(t) != sig: return empty
  return sig
```

**关键点**：
- **不穿透嵌套 Layer**：嵌套 Layer 自己决定是否 hoist，避免交叉影响
- **穿透 Group**：Group 不是 Layer，它没有独立的 Layer div，其内容渲染在同一个 Layer div 内，所以 Group 内的 Text 必须参与签名比较
- **穿透 TextBox**：TextBox 不是 Layer，只是 Element 容器
- **SVG `<text>` 不参与**：若 Text 渲染为 SVG `<text>`（TextPath 等），不进入候选。Commit 1 实施时需读代码确认实际渲染路径

### 1.4 toCssString 输出顺序

CSS 声明固定顺序写出，保证 Phase 2 字符串去重稳定：

```
font-family: ...;
font-size: ...px;
font-weight: bold;    ← 非 bold 不输出（400 视为 default）
font-style: italic;  ← 非 italic 不输出
line-height: ...px;  ← NaN 不输出
letter-spacing: ...px;  ← NaN 不输出
```

### 1.5 Hoist 例外

- **Layer style 已含 `font-*` 属性**：不再 hoist，防御未来扩展
- **Layer 下无 Text 子节点**：不 hoist
- **只有单个 Text**：签名自身一致，也 hoist（大量收益来源）
- **Shape + 单 Text 混合**：Shape 不受 font 影响，CSS 继承无害，照常 hoist
- **TextBox 富文本（多 Text 签名不一致）**：整 Layer 不 hoist，所有 font-* 保留在 span
- **Group 内 Text 与直接 Text 签名不一致**：整 Layer 不 hoist

### 1.6 span 写出时的处理

Text span emit 接收一个 `bool fontHoisted` 参数：
- `true`：跳过签名覆盖的所有 font-* 属性
- `false`：照常写

如果所有属性都被跳过，span style 为空，不写 `style=""`（HTMLWriter 已有通用空 style 跳过路径）。

### 1.7 嵌套 Layer 共享 font 的重复写出

示例：外 Layer 和内 Layer 都 hoist 同样的 font，外 Layer style 上有一份，内 Layer style 上也有一份。字节冗余但语义正确。

不做穿透优化：复杂度陡增，收益有限。Phase 2 的 extractor 会把两个完全相同的 Layer style 串去重为一条规则，字节损失自动回收。

### 1.8 CSS 继承跨 stacking context / position 边界

CSS font 继承走 DOM 树，与 `position` / `display` / `mix-blend-mode` / `opacity` / `filter` 等无关。所有 Layer 都能安全继承。

### 1.9 Text 渲染为 SVG `<text>` 的场景

TextPath / 文字沿路径分布等场景，Text 可能在 SVG 内 emit 为 `<text>`。SVG `<text>` 对 HTML 父 `<div>` 的 CSS 继承在规范上成立（font-* 是 SVG presentation attributes，走 CSS 继承），实测主流 Chromium 版本稳定。

**保守策略**：Commit 1 实施时先读 HTMLWriter 源码确认 Text 渲染路径：
- 全部是 HTML span → 不加路径判断
- 存在 SVG text 路径 → 跳过 hoist（只 hoist HTML span 路径的 Text）

---

## Phase 2：HTMLStyleExtractor

### 2.1 接口

```cpp
// src/pagx/html/HTMLStyleExtractor.h
#pragma once
#include <string>

namespace pagx {

class HTMLStyleExtractor {
 public:
  /**
   * Consolidates every `style="..."` attribute in the input HTML into a single
   * internal stylesheet injected immediately before `</head>`. Each unique
   * style value becomes a class rule named `.psN` (N starting at 0, in
   * first-seen order). Existing `class="..."` attributes are preserved and the
   * generated class name is appended after a space.
   *
   * The document's <body> tag style is deliberately left inline: its style is
   * unique per document (canvas size differs) so deduplication has no benefit.
   *
   * Style values are treated as opaque strings — no CSS parsing. HTML entities
   * in the value (e.g. `&#39;`) are decoded before being written into the
   * <style> block because the CSS parser does not reinterpret named or numeric
   * character references inside a <style> element.
   *
   * The input must be well-formed HTML produced by HTMLWriter. Behaviour on
   * malformed input is best-effort: the extractor will not crash but the
   * output is not guaranteed to be semantically identical.
   */
  static std::string Extract(const std::string& html);
};

}  // namespace pagx
```

### 2.2 假设与不变量

基于对 HTMLWriter 现有输出的实测（77 样本），extractor 依赖以下不变量：

- 所有属性使用**双引号**包围（0 个单引号属性实测）
- 属性值内部不含未转义的 `<` 或 `"`（HTMLWriter 编码为 `&lt;` / `&quot;`）
- style 值可能含 HTML entity，实测只出现 `&#39;`（但 extractor 支持全集）
- style 值可能含 CSS 函数 `url(data:...)`，其中 `;` / `%22` / 嵌套括号属于函数内部语法，**不参与 CSS 属性分隔**
- HTMLWriter 不会在输出里预先插入 `<style>` 或 `<script>` 标签
- 开始标签可能以 `>` 或 `/>` 结尾
- 注释只出现在 `<!-- Generated by PAGX HTMLExporter. Do not edit. -->`

Extractor 对不满足假设的输入行为是 best-effort：不崩溃，但输出可能语义不等价。

### 2.3 Tokenizer 状态机

```
INITIAL      — 读文本
TAG_OPEN     — 读到 `<`
  ├── '!'  → COMMENT / DOCTYPE / CDATA 分支
  ├── '/'  → 结束标签（读到 '>'，不处理）
  ├── '?'  → 处理指令（读到 '?>'）
  └── 字母 → START_TAG
COMMENT       — 读到 `-->` 为止
DOCTYPE       — 读到 `>` 为止
CDATA         — 读到 `]]>` 为止（防御，HTMLWriter 不生成）
START_TAG     — 收集 tagName / class 属性位置 / style 属性位置 / tag 结束位置
                属性值按引号配对读（跳过内部所有字符）
                完成时记录 StartTagInfo
                若 tagName 是 `style` / `script` → 进入对应 RAW_TEXT
RAW_TEXT_*    — 读到对应 `</style>` / `</script>` 为止
```

### 2.4 数据结构

```cpp
struct StartTagInfo {
  size_t tagStart = 0;          // '<' 位置
  size_t tagEnd = 0;            // '>' 之后位置（含 `/>` 的 `/`）
  std::string tagName = {};     // 小写
  bool hasStyle = false;
  size_t styleAttrStart = 0;    // `style="..."` 的 `s` 位置
  size_t styleAttrEnd = 0;      // 闭合 `"` 之后位置
  size_t styleValueStart = 0;
  size_t styleValueEnd = 0;
  bool hasClass = false;
  size_t classAttrStart = 0;    // `class="..."` 的 `c` 位置
  size_t classAttrEnd = 0;      // 闭合 `"` 之后位置
  size_t classValueStart = 0;   // 值的起始（`"` 之后）
  size_t classValueEnd = 0;     // 值的结束（`"` 之前）
  int classIndex = -1;          // Phase 1 分配的 class 索引
};

std::vector<StartTagInfo> tags;
std::vector<std::string> orderedStyles;
std::unordered_map<std::string, int> styleToIndex;
```

### 2.5 算法

```
Pass 1（tokenize + collect）:
  - tokenize 输入，填充 tags
  - 对每个 tag:
      if !hasStyle: skip
      if tagName == "body": skip   # body 不抽取
      value = substr(styleValueStart, styleValueEnd)
      if value.empty(): skip
      idx = styleToIndex.insert_or_get(value, orderedStyles.size())
      if newly inserted: orderedStyles.push_back(value)
      记录 tag 对应的 classIdx

Pass 2（rebuild output — attribute-by-attribute rebuild）:
  output = ""
  prev = 0
  for each tag in tags:
    if tag 不需要重写（no style 或 body 或 empty style 或 classIndex < 0）:
      continue
    copy(input[prev .. tag.tagStart])
    nameEnd = find end of tagName (first whitespace or '>')
    copy(input[tag.tagStart .. nameEnd])     // copy '<tagName'
    // Emit class attribute.
    if tag.hasClass:
      existingClass = input[tag.classValueStart .. tag.classValueEnd]
      write ' class="'
      if existingClass is not empty:
        write existingClass + ' ' + className
      else:
        write className
      write '"'
    else:
      write ' class="' + className + '"'
    // Re-emit all other attributes (skip class and style).
    attrPos = nameEnd
    while attrPos < input.size:
      skip whitespace
      if at '>' or '/': break
      read attributeName
      if attributeName is 'class' or 'style':
        skip entire attribute (name + '=' + value)
        continue
      // Copy this attribute with its leading whitespace.
      copy input[attrWithWsStart .. attrPos]  (includes leading ws before attrName)
    // Copy tag closing ('>' or '/>').
    copy(input[attrPos .. tag.tagEnd])
    prev = tag.tagEnd
  copy(input[prev .. end])

Pass 3（insert stylesheet）:
  block = "<style>\n"
  for i, s in orderedStyles:
    block += ".ps" + i + "{" + decodeEntities(s) + "}\n"
  block += "</style>\n"

  pos = output.find("</head>")
  if pos != npos: insert block at pos
  else if output.find("<body") != npos: insert block at that pos
  else: output = block + output
```

### 2.6 Pass 2 标签重写策略

实际实现采用**逐属性重建**（attribute-by-attribute rebuild）：对每个需要重写的开始标签，从头遍历其所有属性，跳过 `class` 和 `style`，先写入新的 `class` 属性，再依次写入其他属性。这种方法不依赖 class/style 属性的相对位置，避免了边界条件错误。

属性重写后，class 始终紧跟 tagName 输出（其他属性保持原始顺序）。

### 2.7 class 合并位置

三种情况：

**Case A**: 无 class
```
<div style="...">  →  <div class="psN">
```
在 tagName 之后（含可能的空白）插入 ` class="psN"`。实现:找到 tagName 结束位置(第一个空白或 `>`)，在此插入。

**Case B**: 已有非空 class
```
<div class="foo" style="...">  →  <div class="foo psN">
```
在 classValueEnd - 1（闭合 `"` 之前）插入 ` psN`。

**Case C**: 已有空 class `class=""`
```
<div class="" style="...">  →  <div class="psN">
```
在 classValueEnd - 1 插入 `psN`（无前导空格）。

### 2.8 HTML entity 解码（全集）

实现覆盖：

| Entity | 解码 |
|-------|------|
| `&amp;` | `&` |
| `&lt;` | `<` |
| `&gt;` | `>` |
| `&quot;` | `"` |
| `&apos;` | `'` |
| `&#NNN;` | Unicode 码点（十进制） |
| `&#xHH;` / `&#XHH;` | Unicode 码点（十六进制） |

算法：单次线性扫描，遇到 `&` 后尝试匹配上述模式，失败则原样保留 `&`。numeric entity 转 UTF-8 字节序列（支持 1-4 字节码点）。

**只在写 `<style>` 时解码一次**，`orderedStyles` 存储原始字节，保证去重 key 稳定。

### 2.9 `<style>` 块格式

每条规则一行：

```html
<style>
.ps0{width:360px;height:20px;position:relative;font-family:'Arial';font-size:16px;color:#1E293B}
.ps1{position:absolute;left:180.02px;top:0px;transform:translateX(-50%)}
.ps2{...}
</style>
```

插入到 `</head>` 之前。若 HTML 无 `</head>`，退化到 `<body` 前或字符串开头。

### 2.10 不处理的元素

| 元素 | 原因 |
|-----|------|
| `<body>` 的 style | 每文件单例，抽取反而变大 |
| 空 `style=""` | 无内容 |
| `<style>` / `<script>` 内部 | raw text 上下文，不做 attribute 扫描 |
| 注释 / DOCTYPE / CDATA / PI | 不是 element |

---

## HTMLExportOptions 扩展

```cpp
// include/pagx/HTMLExporter.h
struct HTMLExportOptions {
  // ... 现有字段 ...

  /**
   * When true, all inline `style="..."` attributes are consolidated into a
   * single internal stylesheet placed in the document <head>, and elements
   * reference styles via generated class names (`.ps0`, `.ps1`, ...). This
   * typically reduces HTML size by 10-25% for documents with repeated style
   * declarations. When false, every style remains inline.
   *
   * The extracted output is semantically identical to the inline output: all
   * computed styles render identically in any standards-compliant browser.
   * Inline specificity (1000) becomes class specificity (10); this has no
   * effect for self-contained documents produced by HTMLExporter because no
   * user stylesheet exists to compete with the extracted rules.
   *
   * The document's <body> `style` attribute is always kept inline because it
   * is unique per document and gains nothing from extraction.
   *
   * The default is true.
   */
  bool extractStyleSheet = true;
};
```

`HTMLExporter::ToHTML` 接入：

```cpp
std::string HTMLExporter::ToHTML(const PAGXDocument& document, const Options& options) {
  std::string nativeHTML = /* HTMLWriter emit + RoundCoordinatesInHTML */;
  if (options.extractStyleSheet) {
    nativeHTML = HTMLStyleExtractor::Extract(nativeHTML);
  }
}
```

Phase 2 仅在 `extractStyleSheet == true` **且** `framework == Native` 时激活。React/Vue 输出不做抽取（它们的 `TransformStyleAttributes` 会将 inline style 转为 style object）。

---

## 决策记录

本方案经过多轮讨论，最终决策：

| 决策点 | 选择 | 备注 |
|-------|-----|-----|
| Phase 1 触发策略 | 严格一致 | 所有直接子 Text 签名全相等才 hoist |
| Phase 1 穿透嵌套 Layer | 不穿透 | 嵌套 Layer 独立判定 |
| Phase 1 穿透 Group | 穿透 | Group 不是 Layer，渲染在同一个 Layer div 内，其 Text 参与 CSS 继承 |
| Phase 1 partial hoist（属性级） | 不做 | 收益递减，后续评估 |
| Phase 1 开关 | 始终启用 | 纯优化无副作用 |
| Phase 2 开关 | `extractStyleSheet` | 默认 true |
| Phase 2 小文件阈值 | 不加 | 统一逻辑 |
| CSS 属性值处理 | 整串不透明 | 不解析 CSS 内部结构 |
| HTML entity 解码 | 全集 | 5 命名 + numeric（十进制 + 十六进制） |
| body style | 保留 inline | 单例不值得抽取 |
| class 前缀 | `ps` | pagx-style 缩写，2 字符短前缀；仓库内零冲突，每文件省约 160 字节 |
| `<style>` 块格式 | 每条规则一行 | 便于阅读 |
| FontSignature float 字段 | NaN sentinel | 与项目风格一致 |
| HTMLStyleExtractor 头文件位置 | `src/pagx/html/` | 内部实现，不暴露 |
| FontHoist 测试 | 独立 `FontHoistTest.cpp` | 便于单独调整 |
| Baseline 跑两次 | Commit 1 跑一次、Commit 2 跑一次 | 分别确认视觉等价 |

---

## 代码组织

```
src/pagx/html/
├── HTMLStyleExtractor.h     [新增] 内部头
├── HTMLStyleExtractor.cpp   [新增] 实现
├── FontHoist.h              [新增] FontSignature + Collect 函数
├── FontHoist.cpp            [新增] 实现
├── HTMLWriter.h             [改] 接入 FontHoist
├── HTMLWriterLayer.cpp      [改] Layer emit 时 hoist
├── HTMLWriterText.cpp       [改] Text span emit 时跳过被 hoist 的属性
└── HTMLExporter.cpp         [改] Phase 2 调用 extractor

include/pagx/
└── HTMLExporter.h           [改] 新增 extractStyleSheet 字段

test/src/
├── FontHoistTest.cpp            [新增] FontSignature equals + FontSignatureToCss 单测（10 用例）
├── HTMLStyleExtractorTest.cpp   [新增] extractor 单测（19 用例）
└── PAGXHtmlTest.cpp             [改] 截图测试覆盖视觉等价
```

---

## 测试矩阵

### FontHoistTest（10 用例）

| 用例 | 构造 | 预期 |
|-----|------|-----|
| SignatureEqualityBasic | 两个相同签名 | equals == true |
| SignatureEqualityDiffFamily | fontFamily 不同 | equals == false |
| SignatureEqualityDiffSize | renderFontSize 不同 | equals == false |
| SignatureEqualityDiffBold | bold 不同 | equals == false |
| SignatureEqualityNaN | lineHeight/letterSpacing 都为 NaN | equals == true；NaN vs 0.0f 为 false |
| CssOutputFamilyAndSize | fontFamily + renderFontSize | 输出 font-family/font-size；无 bold/italic/letter-spacing |
| CssOutputBoldItalic | bold + italic | 输出 font-weight:bold; font-style:italic |
| CssOutputLetterSpacing | letterSpacing = 2.0 | 输出 letter-spacing:2px |
| CssOutputLineHeight | lineHeight = 18.4 | 输出 line-height:18.4px |
| CssOutputEmptySignature | 默认构造签名 | 输出空字符串 |

### HTMLStyleExtractorTest（19 用例）

| 用例 | 输入 | 预期 |
|-----|------|-----|
| EmptyInput | `""` | `""` |
| NoStyleAttributes | `<html><body><div></div></body></html>` | 不变 |
| SingleStyle | 1 个 `style="color:red"` | `<style>.ps0{color:red}</style>` + `class="ps0"` |
| DuplicateStyles | 3 个相同 style | 1 条规则，3 个元素都 `class="ps0"` |
| ExistingClassMerged | `class="foo"` + style | `class="foo ps0"` |
| EmptyExistingClass | `class=""` + style | `class="ps0"` |
| EntityApostrophe | `style="font-family:&#39;Arial&#39;"` | `<style>.ps0{font-family:'Arial'}</style>` |
| EntityAll5Named | `&amp; &lt; &gt; &quot; &apos;` | 解码 `& < > " '` |
| EntityNumericDec | `&#65;` | `A` |
| EntityNumericHex | `&#x41;` | `A` |
| BodyStylePreserved | `<body style="margin:0">` | body style 不抽取 |
| EmptyStyleAttr | `style=""` | 不抽取，不生成 class |
| CommentSkipped | `<!-- style="..." -->` | 注释内容原样保留，不抽取 |
| StyleValueWithDataUri | `style="...url(data:image/png;base64,abc;def)..."` | 整串当一条，`;` 不切分 |
| NoHeadInsertBeforeBody | HTML 无 head | style 块插到 `<body` 前 |
| SelfClosingTag | `<img ... />` | 抽取，尾部 `/>` 保留 |
| ClassInsertedAfterTagName | `<div id="x" style="...">` | `<div class="ps0" id="x">` |
| StyleInsertedBeforeHeadClose | `<head><meta ...></head>` | `<style>` 插到 `</head>` 前 |
| MultipleUniqueStyles | 2 个不同 style | `.ps0` + `.ps1` 两条规则 |

### 集成测试

未实施 `ExtractStyleSheetRoundTrip` 集成测试。Phase 2 的视觉等价性通过现有 `PAGXHtmlTest.HtmlScreenshotCompare` 截图测试覆盖：Phase 1 提交后跑一次截图确认等价，Phase 2 提交后跑一次确认等价。两者均通过，baseline 已接受。

---

## 实施步骤

### Commit 1：font hoist

1. 新建 `FontHoist.h/.cpp`：`FontSignature` + `SignatureOf` + `CollectUniformSignature`
2. 读代码确认 Text 渲染路径（HTML span / SVG text），若有 SVG text 路径则排除
3. 修改 `HTMLWriterLayer.cpp`：Layer emit 入口调用 `CollectUniformSignature`，签名有效则追加到 Layer style
4. 修改 `HTMLWriterText.cpp`：Text span emit 接收 `fontHoisted` 参数，跳过 font-* 属性
5. 写 `FontHoistTest.cpp`（8 用例）
6. 本地跑 `PAGXHtmlTest.HtmlScreenshotCompare`，确认视觉完全等价
7. 按需 `/accept-baseline`
8. Commit: `Hoist uniform Text font attributes to the parent Layer to enable CSS inheritance.`

### Commit 2：HTMLStyleExtractor

1. 新建 `HTMLStyleExtractor.h/.cpp`：tokenizer + extract 算法 + entity 解码
2. 修改 `include/pagx/HTMLExporter.h`：新增 `extractStyleSheet` 字段（默认 true）
3. 修改 `HTMLExporter.cpp::ToHTML`：接入 Phase 2
4. 写 `HTMLStyleExtractorTest.cpp`（20 用例）
5. 写 `PAGXHtmlTest.ExtractStyleSheetRoundTrip`
6. 本地跑 `PAGXHtmlTest.HtmlScreenshotCompare` + `PAGXHtmlTest.ExtractStyleSheetRoundTrip`
7. 按需 `/accept-baseline`
8. Commit: `Extract inline style attributes into an internal stylesheet to reduce HTML size.`

---

## 输出示例（showcase_mandala）

### Phase 1 后（font hoist）

```html
<div class="pagx-layer" style="width:360px;height:20px;position:relative;font-family:'Arial';font-size:16px;font-weight:bold;color:#1E293B;line-height:18.4px;white-space:pre">
  <span style="position:absolute;left:180.02px;top:0px;transform:translateX(-50%)">Showcase: Mandala</span>
</div>
```

### Phase 2 后（extract）

```html
<head>
<meta charset="utf-8">
<style>
.ps0{width:360px;height:20px;position:relative;font-family:'Arial';font-size:16px;font-weight:bold;color:#1E293B;line-height:18.4px;white-space:pre}
.ps1{position:absolute;left:180.02px;top:0px;transform:translateX(-50%)}
...
</style>
</head>
<body style="margin:0;padding:0;background:transparent;width:420px;height:460px;overflow:hidden">
<div class="pagx-layer ps0">
  <span class="ps1">Showcase: Mandala</span>
</div>
```

---

## 风险注册表

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | SVG `<text>` 继承路径不稳定 | 实施确认：Text 有 glyphRuns 时渲染为 SVG `<text>`，FontHoist 已排除该路径 Text 节点 |
| R2 | HTMLWriter 新增 emit 路径时忘记跳过被 hoist 的属性 | FontHoistTest 覆盖；集成测试视觉等价把关 |
| R3 | extractor tokenizer 遇到未见过的 HTML 结构崩溃 | 19 用例单测覆盖；集成测试全量样本跑通 |
| R4 | Entity 解码遗漏（未来 HTMLWriter 生成新 entity） | 全集实现，`&xxx;` 无匹配时原样保留不崩溃 |
| R5 | class 名 `psN` 与宿主页面冲突 | 输出为完整文档，innerHTML 注入不是合理用法；iframe/独立文档场景无冲突。仓库内实测零冲突（2026-04-23） |
| R6 | Phase 2 的后处理字符串操作性能 | 文件 <50 KB，一次 O(N) 扫描，<1ms 可忽略 |
| R7 | Baseline 因字节变化需要大批更新 | 视觉等价是硬要求，PNG baseline 不变；HTML 文本无 baseline |

---

## 附：数据支撑

### 实测样本特征

- 77 个 pagx 样本，总 HTML 622 KB
- `<span>`（文本）877 个
  - `abs+centerX`: 423（48%，textAnchor=middle）
  - `NOT-absolute`: 211（24%，TextBox 内 inline 流）
  - `abs+transform`: 193（22%，TextPath per-glyph）
  - `abs+topleft`: 50（6%）
- 唯一 style 字符串 2798 个（跨文件），单次使用 87.6%
- body style 57 个唯一（每文件画布尺寸不同）
- HTML entity 实测只出现 `&#39;`
- CSS 函数嵌套深度 ≤ 3，`url(data:...)` 内含 `;` 和 URL 编码
- 属性顺序 139 种 ordering / 133 种 key-set（基本稳定）

### 预期收益

| 样本 | Baseline | Phase 1+2 预期 |
|-----|---------|-------------|
| 整体 77 文件 | 622 KB | -17.4%（-108 KB） |
| showcase_mandala（最大） | 33 KB | 预计 -15% |
| selector_advanced（重复最多） | 13.6 KB | 预计 -30% |
| text_rich（最短） | 3 KB | 预计持平或略大 |

---

## 实施偏差记录

实施阶段与初始方案的偏差：

| 偏差点 | 方案 | 实际 | 原因 |
|-------|------|------|------|
| FontSignature.colorRGBA | 含 colorRGBA | 不含 | 颜色来自 Fill→ColorSource→SolidColor 链，不直接属于 Text 节点 |
| FontSignature.underline | 含 underline | 不含 | Text 节点无此字段 |
| FontSignature.bold | fontWeight 枚举 | bool bold | 实际只关心是否为 bold，简化为 bool |
| FontSignatureToCss font-weight | 输出具体数值（如 700） | 输出 `bold` / 不输出 | 与 HTMLWriterText 现有 emit 一致 |
| Pass 2 标签重写 | 字符串剪切（移除 style+插入 class） | 逐属性重建 | 字符串剪切依赖 class/style 属性顺序，导致边界错误；逐属性重建不依赖属性顺序 |
| Phase 2 激活条件 | 仅 `extractStyleSheet` | `extractStyleSheet` | React/Vue 输出已移除，抽取无条件生效 |
| FontHoistTest 用例数 | 8 | 24 | 增加 CollectUniformSignature 测试覆盖 Group 穿透、签名不一致、SVG 模式等场景 |
| HTMLStyleExtractorTest 用例数 | 20 | 19 | 合并/调整：SvgElementStyle 和 MetaTagNoChange 被其他用例覆盖；新增 StyleInsertedBeforeHeadClose 和 MultipleUniqueStyles |
| ExtractStyleSheetRoundTrip | 有 | 未实施 | 视觉等价由现有截图测试覆盖 |
| StartTagInfo | 无 classAttrStart/classAttrEnd | 含 classAttrStart/classAttrEnd/classIndex | Pass 2 逐属性重建需要完整属性位置信息 |
| CommentSkipped 测试预期 | `color:red` 不出现在结果 | 注释文本原样保留 | 注释 `<!-- <div style="color:red"> -->` 中的 style 文本被原样保留到输出，不是被抽取 |
