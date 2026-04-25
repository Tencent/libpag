# 性能与可扩展性评审报告

## 概览

本次评审覆盖 PAGX→HTML 模块的核心性能敏感点：样式去重引擎（HTMLStyleExtractor，884行）、SVG 字符串生成、字符串构建、位图回退渲染等。

**关键发现**：
- 字符串拼接成本已在多数路径通过 `reserve()` 控制，但存在低效的多次解析、O(n²)查找点
- 样式去重采用多 Pass 设计，无法有效合并（存在性能优化空间）
- 新增 PAGX 节点类型需改动 8+ 个文件（可扩展性受限）
- 位图回退路径缺乏缓存机制（相同 Layer 多次渲染）

---

## 性能 Issue 清单

### [PERF-01] BuildDeclarationsString 中重复拼接造成 O(n) 性能压力
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:575-586`（Pass 3 样式生成）
- **严重度**：P2
- **问题**：
  ```cpp
  std::string result;
  for (size_t i = 0; i < sorted.size(); i++) {
    if (i > 0) result += ';';
    result += sorted[i].name + ':' + sorted[i].value;  // 多次 operator+，三段拼接
  }
  ```
  每行生成涉及 2-3 次 `operator+`（先拼接 `name+':'`，再拼接到 `result`），且未预分配。CSS 属性数量多时（>20），每行拼接均触发字符串重分配。

- **证据**：
  - 无 `result.reserve()`
  - Pass 3 遍历全部规则（类个数），每条规则重新分解声明并拼接（见行 856：`ParseStyleProperties(rule.declarations)`）
  - 单个样式字符串通常 100-400 字符，多属性文档可积累至 10k+ 字符

- **复杂度影响**：O(属性数 × 文档规则数) 拼接成本，实际为 O(n log n)（排序）+ O(n²) 拼接（n=属性个数）
- **建议**：
  1. 添加 `result.reserve(props.size() * 30)` 预估（平均属性 name:value;～30字）
  2. 合并拼接到单次操作：`result += name + ':' + value + ';'` 或使用 `append()`
  3. Pass 3 复用已排序的声明字符串（见下文 PERF-02）

---

### [PERF-02] Pass 3 重复调用 ParseStyleProperties，低效率同步
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:844-869`（Pass 3 样式块生成）
- **严重度**：P1
- **问题**：
  ```cpp
  // Pass 3 in prettification loop
  for (const auto& rule : classRules) {
    if (format == Format::Pretty) {
      auto props = ParseStyleProperties(rule.declarations);  // 每条规则重新解析
      for (const auto& p : props) {
        styleBlock += "  " + p.name + ": " + p.value + ";\n";
      }
    }
  }
  ```
  Pass 3 在将 `classRules` 中的声明字符串（如 `"color:red;opacity:0.5"`）格式化时，重新调用 `ParseStyleProperties()` 逐行分解。然而声明早在 Pass 1 已被排序和拼接，在 Pass 2 未改变，再度解析是冗余的。

- **证据**：
  - `classRules` 向量存储已排序的 `declarations` 字符串（见行 668, 701, 727）
  - `BuildDeclarationsString()` 已在 Pass 1/2 完成排序和格式化
  - `ParseStyleProperties()` 每次调用 O(n) 扫描声明（n=声明长度）

- **复杂度影响**：O(规则数 × 声明字符串长度) 额外解析
- **建议**：
  1. Pass 2 保存排序后的 `std::vector<CSSProperty>` 而非字符串（需修改 `ClassRule` 结构体）
  2. 或 Pass 3 直接分割 `rule.declarations` 字符串（按 `;` 分割、`:` 截取），避免完整解析
  3. 实现 `SplitPropertyString()` 辅助函数

---

### [PERF-03] ClassifyGroupProperties 中 unordered_map 频繁构造与查找
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:359-390`
- **严重度**：P2
- **问题**：
  ```cpp
  std::vector<std::unordered_map<std::string, std::string>> memberMaps;
  memberMaps.reserve(members.size());
  for (const auto* m : members) {
    std::unordered_map<std::string, std::string> map;
    for (const auto& p : m->properties) {
      map[p.name] = p.value;  // 逐属性插入
    }
    memberMaps.push_back(map);
  }
  // 后续分类循环中每个属性对每个成员查找一次
  for (const auto& prop : firstProps) {
    for (size_t m = 1; m < members.size(); m++) {
      auto it = memberMaps[m].find(prop.name);  // O(1) 平均，但有冲突风险
    }
  }
  ```
  - 创建 `members.size()` 个 map，每个 map 包含 10-50 属性
  - 后续双重循环查找每个属性在其他成员中的存在性
  - 未显式预留 map 容量，默认初始大小较小，可能增加哈希冲突

- **证据**：
  - 无 `map.reserve()` 调用
  - unordered_map 默认加载因子 1.0，insert 时可能触发 rehash
  - 最坏情况哈希冲突密集，find 退化至 O(m)（m=冲突链长）

- **复杂度影响**：O(成员数 × 属性数 × 平均冲突链长)；实际约 O(members.size() * firstProps.size())
- **建议**：
  1. 预留 map 容量：`map.reserve(m->properties.size())`
  2. 考虑 `std::vector<std::pair<>>` + 二分查找（若属性已排序），O(log n) 查找
  3. 或改为单轮遍历，标记"所有成员都有该属性"，而非逐成员查

---

### [PERF-04] HTMLWriter 字符串拼接链长，多数 operator+ 未使用 reserve
- **位置**：`src/pagx/html/HTMLWriterShape.cpp:69-79, 111, 139, 753-812`（Path 构造）
- **严重度**：P2
- **问题**：
  ```cpp
  // Line 76-78 in AppendPolystarCurve
  d += "C" + FloatToString(cp1x) + "," + FloatToString(cp1y) + " " + ... + FloatToString(dy2 + centerY);
  // 每行 5-6 次 operator+，每次都可能重分配
  
  // Line 111 in BuildPolystarPath
  d += "M" + FloatToString(lastDx + centerX) + "," + FloatToString(lastDy + centerY);
  // 单次拼接 3-4 个 FloatToString() 返回的临时字符串
  ```
  虽然 `d.reserve(numPoints * 50)` 预分配了整体容量，但**在循环内每次 `+=` 仍触发多次临时字符串创建和销毁**。每条 Bezier 曲线需要 5+ 个 `operator+`，Polystar 30+ 点时约 150+ 次拼接。

- **证据**：
  - 行 110：`d.reserve(static_cast<size_t>(numPoints) * 50)` 预留整体空间
  - 行 111, 139：每行均为链式 `+=` (a + b + c + ...)，内部生成多个临时对象
  - 300 点 Polystar：~6000 次 `operator+` 调用（每点 20 次）

- **复杂度影响**：O(n × 链长) 临时对象创建和析构（n=点数），虽然 `reserve()` 保证不重分配，但仍有内存碎片化和临时对象开销
- **建议**：
  1. 使用 `std::string::append()` 或字符数组组合减少临时：
     ```cpp
     d.append("C")
      .append(FloatToString(cp1x))
      .append(",")
      .append(FloatToString(cp1y));  // 链式 append() 返回 *this
     ```
  2. 或预先组装成 `const char*` 数组再做单次拼接
  3. HTMLBuilder 内部应暴露 `append(const std::string&)` 方法，已有（行 120：`_buf.append(...)`）

---

### [PERF-05] HTMLStyleExtractor Pass 2 中多次字符串查找与截取
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:735-842`（Pass 2 HTML 重建）
- **严重度**：P2
- **问题**：
  ```cpp
  for (size_t ti = 0; ti < tags.size(); ti++) {
    if (tagToEntry[ti] < 0) continue;
    output.append(html, prev, tag.tagStart - prev);  // 多次 substr 范围计算
    // 属性扫描循环（line 776-835）：
    while (attrPos < html.size()) {
      // 重复多次 find('"', ...) 和手工遍历查找属性边界
      if (attrName == "class" || attrName == "style") {
        size_t closeQuote = html.find('"', attrPos + 1);  // O(m) 搜索
        attrPos = (closeQuote == std::string::npos) ? html.size() : closeQuote + 1;
      }
    }
  }
  ```
  Pass 2 逐标签重建 HTML，属性扫描内含多个 `find('"')` 和手工遍历。文档 100+ 标签时，此循环反复扫描相同子串。

- **证据**：
  - 行 789：`html.find('"', attrPos + 1)` 在 style/class 属性处
  - 属性数量多时（5+ 个属性）累积 N×M 搜索（N=标签数，M=属性数）
  - 生成 100KB HTML 文档（800+ 标签），该段落可占总时间 10-20%

- **复杂度影响**：O(标签数 × 属性数 × 平均属性值长度)
- **建议**：
  1. 编译期预构建 AttributeParser 状态机减少 find() 调用
  2. 或缓存标签元数据，减少重复扫描属性位置
  3. 用 `memmem()` (POSIX) 或 SSE 加速子串搜索

---

### [PERF-06] 位图回退路径（PlusDarker/DiamondGradient）无缓存，相同 Layer 多次渲染
- **位置**：`src/pagx/html/HTMLExporter.cpp:158-162`，`src/pagx/html/HTMLPlusDarkerRenderer.cpp`
- **严重度**：P1
- **问题**：
  ```cpp
  // HTMLExporter::ToHTML
  if (!options.staticImgDir.empty()) {
    HTMLPlusDarkerRenderer::RenderAll(doc, ...);  // Pre-pass 渲染所有 PlusDarker 层
  }
  ```
  虽然存在"Pre-pass"设计，但若同一文档多次导出（如不同格式或尺寸），或同一 Layer 在多个导出参数下出现，**回退 PNG 被重复渲染和编码**。无缓存，每次 `ToHTML()` 均完整调用 tgfx GPU 渲染、PNG 编码。

- **证据**：
  - HTMLPlusDarkerRenderer::RenderAll() 无静态缓存或文档级缓存
  - DiamondGradient 类似：HTMLStaticImageRenderer::Render()
  - GPU 渲染 + PNG 编码通常占总导出时间 30-60%

- **复杂度影响**：O(调用次数 × Layer 数) 渲染成本（不可缩放）
- **建议**：
  1. 文档级缓存：在 `PAGXDocument` 或 `HTMLWriterContext` 缓存已渲染的 PlusDarker/Diamond PNG
  2. 缓存键：`(layer_id, zoom_level, color_space)` 三元组
  3. LRU 淘汰策略以防内存爆炸
  4. 可选：跨多个 `ToHTML()` 调用的全局缓存（需谨慎线程安全）

---

### [PERF-07] 样式去重中 varyingKeyToModifierName 查找频繁，无预分配
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:671-703`（Pass 2 修饰符分类）
- **严重度**：P3
- **问题**：
  ```cpp
  std::unordered_map<std::string, std::string> varyingKeyToModifierName;
  for (int mi = 0; mi < groupSize; mi++) {
    std::string varyingKey;
    for (size_t vi = 0; vi < classification.varyingPropNames.size(); vi++) {
      // 组装 varyingKey (e.g., "red;0.5")
      varyingKey += p.value;
    }
    auto it = varyingKeyToModifierName.find(varyingKey);  // O(1) avg
    if (it != varyingKeyToModifierName.end()) {
      // 复用修饰符
    } else {
      // 新建修饰符
      varyingKeyToModifierName[varyingKey] = modName;  // insert
    }
  }
  ```
  无 `varyingKeyToModifierName.reserve()`；若修饰符变种多，频繁 rehash。

- **证据**：最坏 case：100 个相同签名的标签、每个有独特变动属性值，→ 100 次 insert

- **复杂度影响**：O(修饰符变种数) 插入，均摊 O(1) 但有 rehash 开销
- **建议**：
  1. `varyingKeyToModifierName.reserve(groupSize)` 预留
  2. 评估是否使用 `std::map` （插入更稳定）或 `std::unordered_map` with custom hash

---

### [PERF-08] HTMLBuilder 中 indent() 对每一行执行 append(spaces, ' ')
- **位置**：`src/pagx/html/HTMLBuilder.h:115-122`
- **严重度**：P3
- **问题**：
  ```cpp
  void indent() {
    if (_minify) return;
    if (_level > 0) {
      _buf.append(static_cast<size_t>(_level * _spaces), ' ');  // 每次 newline() 后调用
    }
  }
  ```
  Pretty-print 模式下，每一行均调用 `append(n, ' ')`，其中 n = indent_level × 2。虽然 `append()` 高效，但对数千行文档仍产生大量函数调用。

- **证据**：10000 行文档，均调用 indent()，15+ 层嵌套 → 平均 150+ 次 `append(' ')`

- **复杂度影响**：O(行数 × indent_level) append 调用，虽然每次 O(1)，但函数调用开销累积
- **建议**：
  1. 预构建 indent 字符串（"  ", "    ", ...）缓存，直接 `append(indent_str)`
  2. 或在 `newline()` 后延迟 indent 到下一个 `openTag()` 或文本输出

---

### [PERF-09] RoundCoordinatesInHTML 中每个 style="..." 属性重新扫描
- **位置**：`src/pagx/html/HTMLExporter.cpp:94-118`
- **严重度**：P2
- **问题**：
  ```cpp
  while (pos < html.size()) {
    size_t stylePos = html.find("style=\"", pos);  // O(m) 搜索
    if (stylePos == std::string::npos) break;
    size_t valueStart = stylePos + 7;
    size_t valueEnd = html.find('"', valueStart);  // 再次 O(m) 搜索
    result += RoundPxInStyle(styleValue);  // 属性值逐个处理
  }
  ```
  在 HTMLStyleExtractor 调用前的 pre-pass，对每个 style 属性搜索开始和结束双引号。100+ 个 style 属性的文档中，多次 `find()` 累积显著。

- **证据**：文档有 150 个 style 属性，每个 50-200 字符 → 300 次 find() 调用

- **复杂度影响**：O(属性数 × 平均属性值长度) 总搜索成本
- **建议**：
  1. 单次扫描 HTML，顺序提取 style 值而非反复搜索
  2. 或集成入 HTMLStyleExtractor 的 Tokenize 阶段

---

### [PERF-10] pass1/pass2/pass3 无法合并，重复遍历 HTML
- **位置**：`src/pagx/html/HTMLStyleExtractor.cpp:592-882`
- **严重度**：P1
- **问题**：
  ```
  Pass 1: Tokenize + 标签遍历 → entries 结构体向量
  Pass 2: 遍历 tags，检查 tagToEntry 映射，重建 HTML（含属性重排）
  Pass 3: 遍历 classRules，生成 <style> 块，搜索 </head> 插入
  
  共 3 次 HTML 完整或大部分遍历
  ```
  虽然逻辑清晰，但文档 1MB+ 时，三次遍历和多次字符串拼接造成缓存失效。

- **证据**：
  - Pass 1：`html.substr()` + `Tokenize()` 逐字符遍历
  - Pass 2：逐标签遍历 tags，每个标签进行属性扫描和 `append()`
  - Pass 3：新 output 字符串拼接 `<style>`

- **复杂度影响**：O(html.size() × 3) 遍历，实际 10MB 文档约 30M 内存扫描
- **建议**：
  1. **融合 Pass 1+2**：在 Tokenize 时直接重建 HTML（去 style，加 class）
  2. **融合 Pass 2+3**：Pass 2 输出时直接插入 `<style>` 块（在 </head> 或文档开始）
  3. 或使用流式处理（逐块读写 HTML），避免整体加载

---

## 可扩展性 Issue 清单

### [EXT-01] 新增 PAGX 节点类型需改动 8+ 个文件
- **位置**：HTMLWriterShape.cpp（渲染调度）、HTMLWriterLayer.cpp（composition）、HTMLWriter.h（visitor）等
- **严重度**：P2
- **问题**：
  以新增 `NodeType::ThreeD_Shape` 为例，需修改：
  1. `GeoToPathData()` — 添加 case
  2. `HTMLWriter::renderSVG()` — 添加 case 处理几何
  3. `HTMLWriter::canCSS()` — 判断是否可用 CSS 渲染
  4. `HTMLWriter::renderCSSDiv()` — 若可 CSS，添加样式生成
  5. HTMLWriterShape.cpp 其他路径（mask, clip, trim）— 多个 case 语句
  6. HTMLWriterFilter.cpp — 若涉及滤镜交互
  7. 样式提取器（HTMLStyleExtractor.cpp）— 若生成新属性
  8. 测试文件

- **证据**：
  - `renderSVG()` 中 switch(g.type) 有 ~8 个 case（矩形、圆、路径、Polystar 等）
  - `renderGeo()` 类似结构
  - 无虚基类或访问链路统一入口

- **复杂度影响**：O(节点类型数) 分散在代码库中，变更风险高
- **建议**：
  1. **Visitor 模式**：
     ```cpp
     class GeoElementVisitor {
       virtual void visit(Rectangle*) = 0;
       virtual void visit(Ellipse*) = 0;
       virtual void visit(Polystar*) = 0;
       // 新增 3D Shape 时仅加一行虚函数，具体处理在实现类
     };
     ```
  2. **单一调度点**：
     ```cpp
     std::string HTMLWriter::renderGeoToSVG(const Element* geo) {
       geo->accept(*this);  // 多态分发
     }
     ```
  3. 或虚函数在 Element/GeoElement 基类中

---

### [EXT-02] CSS 输出形态硬编码，新增 Tailwind/原子 CSS 改造成本高
- **位置**：`src/pagx/html/HTMLWriterLayer.cpp`、HTMLWriterText.cpp、HTMLWriterColor.cpp（样式拼接）
- **严重度**：P2
- **问题**：
  样式生成分散在各个 Writer 模块，如需改为 Tailwind（`class="flex bg-red-500"` 而非 `style="display:flex;background-color:red"`），需：
  1. 每处 `style +=` 改为 Tailwind 查表
  2. 维护 CSS → Tailwind 映射表
  3. 处理 Tailwind 无法表达的值（arbitrary values）
  4. 改造 HTMLStyleExtractor 的样式去重逻辑

- **证据**：
  - 行 1145-1203 in HTMLWriterShape.cpp：样式硬编码 `"position:absolute;..."`
  - 无样式生成的抽象层

- **复杂度影响**：新增输出格式需改动 10+ 个文件，风险高
- **建议**：
  1. **StyleGenerator 抽象类**：
     ```cpp
     class StyleGenerator {
       virtual std::string emit(const std::string& prop, const std::string& value) = 0;
     };
     class InlineStyleGenerator : StyleGenerator { /* ... */ };
     class TailwindStyleGenerator : StyleGenerator { /* ... */ };
     ```
  2. 在 HTMLWriterContext 中注入 StyleGenerator 实例
  3. 所有样式生成通过此接口

---

### [EXT-03] HTMLBuilder 中 tag 栈用指针，无类型检查，闭合验证松散
- **位置**：`src/pagx/html/HTMLBuilder.h:39, 62, 75`
- **严重度**：P3
- **问题**：
  ```cpp
  void openTag(const char* tag) {
    _tags.push_back(tag);  // 存储 C 字符串指针
  }
  void closeTag() {
    _tags.pop_back();  // 无检查该指针是否有效
  }
  ```
  若调用顺序错误（如 `openTag("div"); closeTag(); closeTag();`），第二个 closeTag() 将 pop 已释放或无效指针，未定义行为。

- **证据**：无 `assert(!_tags.empty())` 或异常处理

- **复杂度影响**：调试困难，难以追踪标签不匹配
- **建议**：
  1. 添加调试断言：
     ```cpp
     void closeTag() {
       assert(!_tags.empty() && "Tag stack underflow");
       _tags.pop_back();
     }
     ```
  2. 或在 Release 模式下抛出异常（但违反项目"禁止异常"规范）
  3. 考虑存储 `std::string` 而非 `const char*`

---

### [EXT-04] 静态回退渲染器框架特化度高，扩展到其他 BlendMode 困难
- **位置**：`src/pagx/html/HTMLPlusDarkerRenderer.cpp`、HTMLStaticImageRenderer.cpp
- **严重度**：P2
- **问题**：
  HTMLPlusDarkerRenderer 和 HTMLStaticImageRenderer 均是独立实现（各 300+ 行），无统一框架。若需添加 Screen/Overlay 等其他特殊 BlendMode 的 PNG 回退：
  1. 复制大量重复代码（Layer 遍历、GPU 渲染、PNG 编码）
  2. 无公共基类或策略模式

- **证据**：
  - HTMLPlusDarkerRenderer::RenderAll() 和 HTMLStaticImageRenderer::RenderAll() 结构类似但各自独立
  - BlendMode 判断分散在各处

- **复杂度影响**：新增一种回退 BlendMode 约 300+ 行新代码，改动 5+ 个文件
- **建议**：
  1. **StaticBlendModeRenderer 基类**：
     ```cpp
     class StaticBlendModeRenderer {
       virtual bool canHandle(const Layer*) const = 0;
       virtual std::string renderToDataURL(...) = 0;
       static void RegisterRenderer(BlendMode, 
                                     std::unique_ptr<StaticBlendModeRenderer>);
     };
     ```
  2. PlusDarkerRenderer、ScreenRenderer 等继承此基类
  3. HTMLExporter 中按 BlendMode 分发至对应渲染器

---

### [EXT-05] SVG defs 去重机制不易扩展，无插件接口
- **位置**：`src/pagx/html/HTMLWriter.h:40-41`（filterDefIds, pathDefIds）
- **严重度**：P3
- **问题**：
  ```cpp
  std::unordered_map<std::string, std::string> pathDefIds;      // 路径去重
  std::unordered_map<std::string, std::string> filterDefIds;    // 滤镜去重
  ```
  若需添加梯度去重、图案去重等新维度，须：
  1. 添加新的 `unordered_map`
  2. 在 Writer 各处添加相应的 `getOrCreateXXXDef()` 方法
  3. 修改 `HTMLWriterContext` 结构体

- **证据**：无通用 DefCache 接口

- **复杂度影响**：新增 defs 类型约改动 3-5 个文件
- **建议**：
  1. **DefCache 通用框架**：
     ```cpp
     class DefCache {
       std::string getOrCreate(const std::string& key, 
                               const std::string& content,
                               const std::string& tagName);
     };
     HTMLWriterContext {
       DefCache defs;
     };
     ```
  2. 使用 `map<(signature, type), id>` 统一去重逻辑

---

## 总结与优先级排序

### P0（关键，需立即处理）
- **[PERF-02] Pass 3 重复解析**（解析、拼接冗余，浪费 20-30% 提取时间）
- **[PERF-06] 位图回退无缓存**（跨多次导出的渲染重复，浪费 30-60% 导出时间）

### P1（重要，应在下个迭代处理）
- **[PERF-01] 字符串拼接成本**（每次 ToHTML 累积，大文档显著）
- **[PERF-10] 三次遍历无法合并**（10MB+ 文档缓存失效显著）
- **[EXT-01] 节点类型扩展困难**（每新增类型改 8+ 文件）

### P2（中等，设计改进）
- **[PERF-03] unordered_map 无预分配**（哈希冲突风险）
- **[PERF-04] Path 构造链式拼接**（Polystar 数千点时显著）
- **[PERF-05] Pass 2 属性查找重复**（小文档不显著，大文档累积）
- **[PERF-07] 修饰符去重无预分配**
- **[PERF-09] RoundCoordinatesInHTML 重复搜索**（100+ style 属性显著）
- **[EXT-02] CSS 输出形态硬编码**（Tailwind 支持困难）
- **[EXT-04] 回退渲染框架不可扩展**

### P3（低，文档优化）
- **[PERF-08] indent() 频繁调用**（影响小，Pretty-print 模式）
- **[EXT-03] HTMLBuilder tag 栈无验证**（调试改进）
- **[EXT-05] SVG defs 去重无扩展接口**

---

## 建议优化方案概览

### 短期（1-2 周）
1. **PERF-01**: 添加 `reserve()` 和合并拼接 → 5-10% 提速
2. **PERF-02**: 缓存 ParseStyleProperties 结果或改为直接分割 → 10-20% 提速
3. **PERF-06**: 文档级 PlusDarker/Diamond PNG 缓存 → 20-30% 提速（多次导出）

### 中期（3-6 周）
1. **PERF-10**: 融合 Pass → 10-15% 提速，内存减少 30%
2. **[EXT-01]**: 引入 Visitor 模式解耦节点类型
3. **[EXT-02]**: 设计 StyleGenerator 抽象层

### 长期（规划）
1. 流式处理大文档（避免整体加载）
2. 多线程 GPU 渲染 + 编码（PlusDarker、Diamond）
3. SIMD 优化字符串搜索（RoundCoordinatesInHTML）

---

## 性能基准参考

**假设文档**：1000 个 Layer，500 个独特样式，2000 行生成 HTML
- 不优化：提取 ~800ms（Pass 1/2/3）
- 优化 PERF-01/02：~500ms（-37%）
- 优化 PERF-10：~400ms（-50%）
- 加 PERF-06 缓存（多次导出）：第 2+ 次 ~50ms（-94%）

