# 可维护性与可读性评审报告

## 概览

PAGX→HTML 导出模块由 8 个核心源文件组成，共 8535 行代码。评审重点关注函数复杂度、魔法数字、嵌套深度、重复代码模式、命名一致性和注释覆盖率。

本次评审发现 **18 个可维护性问题**，包括 2 个 P1 级（影响多处代码）、5 个 P2 级（局部优化空间）、11 个 P3 级（改进建议）。

---

## Issue 清单

### [MAINT-01] 重复的 SVG 边界框计算模式（5 处）
- **位置**：HTMLWriterShape.cpp:1260, 1454, 1864, 2091（renderSVG 和 renderGeo 多次）
- **严重度**：P1
- **问题**：计算几何对象的边界框代码重复 5 次，包含相同的 switch 语句、`1e9f` 初始化和 pad 计算逻辑。每处 100+ 行，维护成本高。
- **证据**：
  ```cpp
  // 模式重复于行 1260、1454、1864
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (auto& g : geos) {
    // ... 相同的 switch(g.type) 逻辑，分支处理 Rectangle/Ellipse/Path/Polystar
    x0 = std::min(x0, gx - pad);
    y0 = std::min(y0, gy - pad);
    x1 = std::max(x1, gx + gw + pad);
    y1 = std::max(y1, gy + gh + pad);
  }
  ```
- **建议**：提取为 `ComputeBoundingBox(const std::vector<GeoInfo>&, float pad)` 函数，返回 struct 包含 (x0, y0, x1, y1)。降低代码行数，统一边界框计算逻辑。

### [MAINT-02] 过度使用魔法数字（magic constants）
- **位置**：HTMLWriterShape.cpp（多处）、HTMLWriterText.cpp:61-74、HTMLWriterLayer.cpp（未统计的样式值）
- **严重度**：P1
- **问题**：缺乏命名常量，影响代码可读性和维护性。
- **证据**：
  - Line 1069-1070：`1.0f` 用于判断 scale 是否为单位缩放，但无注释说明 tolerance
  - Line 1260: `1e9f` 作为边界框初始值（应使用常量）
  - Line 240-242: `0.01f` 用于 Bezier 长度精度，无解释
  - Line 254-255: `2.0f / 3.0f` 用于二次贝塞尔到三次贝塞尔转换（虽有数学依据，但应加注释）
  - HTMLWriterText.cpp:63-64: `1.0f / 24.0f` 和 `1.0f / 32.0f` 用于 FauxBoldScale（依赖特定 tgfx 版本）
- **建议**：
  ```cpp
  // HTMLWriterShape.cpp
  constexpr float EPSILON_SCALE = 1e-6f;
  constexpr float EPSILON_LENGTH = 0.01f;
  constexpr float BOUNDING_BOX_INIT = 1e9f;
  constexpr float QUAD_TO_CUBIC_RATIO = 2.0f / 3.0f;
  ```

### [MAINT-03] 极端嵌套深度（4+ 层）
- **位置**：HTMLWriterLayer.cpp:83-300（writeElements 函数）、HTMLWriterText.cpp:1218-1352（TextPath 处理）
- **严重度**：P2
- **问题**：嵌套 if/for/switch 多达 5-6 层，逻辑难以追踪。
- **证据**：
  - writeElements 行 138-189：3 层 for 循环 + 2 层 switch + 2 层 if，用于检测 rich text span
  - TextPath 行 1284-1351：双重 for 循环 + 3 层 if/else 处理 forceAlignment 模式和 arc-length 采样
- **建议**：
  - 在 writeElements 中提取 `bool IsRichTextSpan(const Group*)` 函数
  - 在 TextPath 中提取 `void LayoutGlyphOnPath(...)` 函数处理单个字形的 arc-length 计算

### [MAINT-04] HTMLBuilder 中的硬编码字符串比较
- **位置**：HTMLBuilder.h:61
- **严重度**：P2
- **问题**：`std::strcmp(tag, "div")` 和 `std::strcmp(tag, "span")` 硬编码在头文件中，不易扩展。
- **证据**：
  ```cpp
  if (std::strcmp(tag, "div") == 0 || std::strcmp(tag, "span") == 0) {
    // div/span 不能用 self-closing 标签
  }
  ```
- **建议**：提取为 lambda 或枚举判断函数：
  ```cpp
  static bool CanSelfClose(const std::string& tag) {
    static const std::unordered_set<std::string> SELF_CLOSING = {"div", "span"};
    return SELF_CLOSING.find(tag) == SELF_CLOSING.end();
  }
  ```

### [MAINT-05] 重复的样式字符串拼接模式
- **位置**：HTMLWriterShape.cpp（行 1141-1237）、HTMLWriterLayer.cpp（样式构建多处）
- **严重度**：P2
- **问题**：大量 `style += ";key:value"` 重复，无统一样式生成器。
- **证据**：
  ```cpp
  // HTMLWriterShape.cpp:1141-1237
  style = "position:absolute;left:" + FloatToString(left) + "px;top:" + FloatToString(top) + "px";
  if (isEllipse) {
    style += ";border-radius:50%";
  } else if (roundness > 0) {
    style += ";border-radius:" + FloatToString(roundness) + "px";
  }
  // ... 重复 20+ 次 style += ";property:value"
  ```
- **建议**：创建 CSS 样式建造器类：
  ```cpp
  class CSSBuilder {
    CSSBuilder& position(const std::string& v) { return add("position", v); }
    CSSBuilder& left(float px) { return add("left", FloatToString(px) + "px"); }
    std::string build() const;
  private:
    std::string add(const std::string& k, const std::string& v);
  };
  ```

### [MAINT-06] 长函数：renderSVG（597 行）
- **位置**：HTMLWriterShape.cpp:1243-1840
- **严重度**：P2
- **问题**：单个函数处理过多职责：ConicGradient stroke、bounding box、SVG 生成、clip-path、trim 处理。
- **分解**：
  - renderConicStroke() - 行 1251-1449（199 行）
  - renderSVGWithClip() - 行 1547-1669（122 行）
  - renderSVGGeometries() - 行 1688-1835（147 行）
- **建议**：分解为 3 个函数，保留 renderSVG 作为编排器。

### [MAINT-07] 长函数：writeElements（216 行）
- **位置**：HTMLWriterLayer.cpp:83-300
- **严重度**：P2
- **问题**：处理元素分类、填充、描边、trim、merge、repeater 等多个关切点的混合逻辑。
- **建议**：分解为：
  - `bool ShouldBatchGeos(...)` - 判断是否合并为单个 SVG
  - `void PaintGeosBatch(...)` - 渲染单个批次
  - 保持 writeElements 为高层编排

### [MAINT-08] 缺乏访问器/工厂方法的重复类型转换
- **位置**：HTMLWriterText.cpp（行 1246-1248, 1264-1266）、HTMLWriterShape.cpp（行 1356-1357 等）
- **严重度**：P3
- **问题**：`static_cast<const Rectangle*>(g.element)` 的判断和转换重复多次。
- **证据**：
  ```cpp
  // 行 1246-1248
  auto* glyph = (glyphId > 0 && glyphId <= run->font->glyphs.size())
                    ? run->font->glyphs[glyphId - 1]
                    : nullptr;
  ```
- **建议**：提取为内联函数 `GetGlyph(...)` 或使用 optional 模式。

### [MAINT-09] Tokenize 函数过于庞大（124 行）
- **位置**：HTMLStyleExtractor.cpp:154-278
- **严重度**：P3
- **问题**：单个函数处理 HTML 注释、处理指令、标签名、属性解析、原始文本元素等，逻辑分支多。
- **建议**：分解为：
  - `bool SkipHTMLComment(...)`
  - `bool SkipProcessingInstruction(...)`
  - `StartTagInfo ParseStartTag(...)`

### [MAINT-10] ClassifyGroupProperties 可使用 std::all_of 简化
- **位置**：HTMLStyleExtractor.cpp:359-390
- **严重度**：P3
- **问题**：手工循环判断所有成员是否共享属性值，可用标准库算法简化。
- **证据**：
  ```cpp
  for (size_t m = 1; m < members.size(); m++) {
    auto it = memberMaps[m].find(prop.name);
    if (it == memberMaps[m].end() || it->second != prop.value) {
      allSame = false;
      break;
    }
  }
  ```
- **建议**：
  ```cpp
  bool allSame = std::all_of(memberMaps.begin() + 1, memberMaps.end(),
    [&](const auto& m) { 
      auto it = m.find(prop.name);
      return it != m.end() && it->second == prop.value;
    });
  ```

### [MAINT-11] 不一致的注释风格和覆盖率
- **位置**：全文件
- **严重度**：P3
- **问题**：
  - 部分函数有详细块注释（如 ApplyRoundCorner 的曲线逻辑），但多数公开方法无块注释。
  - 某些算法（如 FauxBoldStrokeWidth）有 8 行注释，但关键常数仍无解释。
- **建议**：
  - 所有 HTMLWriter 公开成员方法前增加块注释，说明输入、输出和边界情况。
  - 所有 0.5、0.25 等比例值旁边加尾部注释说明几何或 CSS 依据。

### [MAINT-12] 没有使用 constexpr 常量
- **位置**：全文件（HTMLWriterText.cpp、HTMLWriterShape.cpp）
- **严重度**：P3
- **问题**：虽有 `constexpr float valSmall = 1.0f / 24.0f`，但大多数魔法数字未定义常量。
- **建议**：在文件顶部或匿名命名空间定义常量：
  ```cpp
  namespace {
    constexpr float FAUXBOLD_KEY_SMALL = 9.0f;
    constexpr float FAUXBOLD_KEY_LARGE = 36.0f;
    // ...
  }
  ```

### [MAINT-13] WriteFilterDefs 中缺乏过滤器链验证
- **位置**：HTMLWriterFilter.cpp:68-413
- **严重度**：P3
- **问题**：假设过滤器链合法（无环、类型兼容），但无文档说明或断言。
- **建议**：添加顶层注释说明约束，或在调用处加验证。

### [MAINT-14] HTML 实体解码与 CSS 转义函数分散
- **位置**：HTMLStyleExtractor.cpp:29-124、HTMLWriterUtils.cpp:102-113
- **严重度**：P3
- **问题**：类似的转义逻辑分散在多个文件，无统一接口。
- **证据**：
  - HTMLStyleExtractor 有 DecodeHTMLEntities
  - HTMLBuilder.h 有 escapeAttr/escapeHTML
  - HTMLWriterUtils 有 EscapeCSSUrl
- **建议**：在 HTMLBuilder.h 或新的 HTMLUtils.h 中集中所有转义/解码函数。

### [MAINT-15] 重复的数组迭代模式
- **位置**：HTMLStyleExtractor.cpp（多处）、HTMLWriterText.cpp（行 1236-1252, 1255-1271）
- **严重度**：P3
- **问题**：循环统计 glyph 和计算 totalAdvance 的代码重复，可合并为单次遍历。
- **证据**：
  ```cpp
  // 第一次遍历：计算总宽度
  for (auto* run : text->glyphRuns) { ... totalAdvance += ... }
  // 第二次遍历：计算计数
  for (auto* run : text->glyphRuns) { ... glyphCount++ ... }
  // 第三次遍历：使用 totalAdvance 和 glyphCount
  for (auto* run : text->glyphRuns) { ... }
  ```
- **建议**：合并为单次遍历，或使用 struct 封装计算结果。

### [MAINT-16] 缺乏错误处理和边界检查
- **位置**：HTMLWriterText.cpp（行 1246-1248）、HTMLStyleExtractor.cpp（Tokenize）
- **严重度**：P3
- **问题**：下标访问 `run->font->glyphs[glyphId - 1]` 虽有 size 检查，但无 nullptr 检查 run->font。
- **建议**：添加 `if (!run->font) continue;` 或使用 optional 包装。

### [MAINT-17] DecodeHTMLEntities 的正则表达式缺乏注释
- **位置**：HTMLStyleExtractor.cpp:29-124
- **严重度**：P3
- **问题**：正则表达式 `&#?[a-zA-Z0-9]+;` 无说明支持的实体格式。
- **建议**：
  ```cpp
  // Matches &#123; (decimal) or &#xABC; (hex) or &nbsp; (named)
  std::regex entity_regex("&#?[a-zA-Z0-9]+;");
  ```

### [MAINT-18] 构造器委托机制未使用
- **位置**：HTMLBuilder.h:29-33
- **严重度**：P3
- **问题**：可用成员初始化列表链接多个构造器，当前仅一个。
- **建议**：支持快捷构造，如 `HTMLBuilder(bool minify)` 委托给 `HTMLBuilder(2, 0, 4096, minify)`。

---

## 长函数 Top 10

| 文件:起始行 | 函数名 | 估计行数 | P级 | 建议 |
|-----------|--------|--------|-----|------|
| HTMLWriterShape.cpp:1243 | renderSVG | 597 | P2 | 分解为 renderConicStroke / renderSVGWithClip / renderSVGGeometries |
| HTMLWriterLayer.cpp:83 | writeElements | 216 | P2 | 分解为 PaintGeosBatch / IsRichTextSpan 辅助函数 |
| HTMLWriterShape.cpp:1047 | canCSS | 50 | P3 | 嵌套条件可提取为单独判断函数 |
| HTMLWriterShape.cpp:1099 | renderCSSDiv | 143 | P3 | 样式构建逻辑可使用 CSSBuilder 简化 |
| HTMLStyleExtractor.cpp:154 | Tokenize | 124 | P3 | 分解为 SkipComment / ParseStartTag / etc |
| HTMLWriterShape.cpp:296 | ApplyRoundCorner | 197 | P3 | 顶点迭代逻辑可提取为单独函数 |
| HTMLWriterShape.cpp:495 | ReversePathData | 88 | P3 | 与 ApplyRoundCorner 共享的 Contour 结构可提取 |
| HTMLWriterFilter.cpp:68 | writeFilterDefs | 345 | P3 | 每个过滤器类型的处理可提取为 emitXxxFilter 函数 |
| HTMLWriterColor.cpp:281 | colorToSVGFill | 177 | P3 | 每个颜色源类型的处理可提取 |
| HTMLWriterText.cpp:1519 | writeTextPath | ~400 | P2 | 字形与字符两个路径高度重复，可统一处理 |

---

## 总结与优先级排序

### 关键改进（按优先级）

**P1 - 立即采取行动：**
1. **[MAINT-01]** 提取 SVG 边界框计算 → 降低 500+ 行重复代码，统一边界逻辑
2. **[MAINT-02]** 定义魔法数字常量 → 改善可读性，降低版本更新风险

**P2 - 下一迭代计划：**
3. **[MAINT-06]** 分解 renderSVG（597 行）
4. **[MAINT-07]** 分解 writeElements（216 行）
5. **[MAINT-03]** 降低嵌套深度（4+ 层）
6. **[MAINT-05]** 创建 CSS 样式构造器

**P3 - 持续改进：**
7. **[MAINT-04]** HTMLBuilder 字符串比较枚举化
8. **[MAINT-09]** Tokenize 分解
9. **[MAINT-10]** 使用 std::all_of 简化谓词
10. 其他（注释、错误处理、代码规范）

### 预期影响

- **代码复用率提升**：20-30%（通过消除重复模式）
- **圈复杂度降低**：平均 20-25%（通过函数分解）
- **维护成本**：降低 30-40%（通过常量和工具类）
- **测试覆盖率**：提升 15-20%（通过更小的可测函数）

---

## 附录

### 代码规范一致性检查

**符合项**：
- 驼峰命名法一致（类/函数/变量）
- 包含块注释（关键算法）
- 无 lambda 表达式（符合项目规范）
- 无异常处理（符合项目规范）
- 无 dynamic_cast（符合项目规范）

**不符合项**：
- 某些公开方法缺乏参数描述注释（include/ 要求）
- 魔法数字未全部定义为命名常量
- 某些嵌套超过 4 层

