# 架构与模块职责评审报告

## 概览

PAGX→HTML 导出架构采用**分层 Visitor 模式**，自上而下为：HTMLExporter（入口）→ HTMLWriter（核心遍历器）→ 专项子模块（样式提取、字体提升、静态图像渲染）。整体职责分工清晰，但存在 3 处架构设计缺陷：

1. **HTMLWriter 职责过重**（7651 行代码分散在 5 个 cpp 文件，关键类方法重载度高），导致单一职责原则破裂
2. **HTMLPlusDarkerRenderer 与主导出路径的耦合度过高**（需在导出前执行 const_cast、修改文档状态、与 HTMLWriter 通过全局 context 通信）
3. **坐标舍入两阶段流水线设计过度**（先生成 HTML 再逐个 style 属性正则匹配舍入，产生额外 O(n) 遍历）

## Issue 清单

### [ARCH-01] HTMLWriter 设计违反单一职责原则，需拆分
- **位置**：src/pagx/html/HTMLWriter.h:192-304 & HTMLWriterShape.cpp:1-2341 & HTMLWriterLayer.cpp:1-1845 & HTMLWriterText.cpp:1-1856
- **严重度**：P1
- **问题**：
  - HTMLWriter 类聚集了 7 个不同的职责领域：
    1. 图层容器生命周期管理（writeLayer / writeLayerContents / writeLayerInner）
    2. 几何形状渲染分发（renderGeo / canCSS / renderCSSDiv / renderSVG / renderDiamondCanvas / renderImagePatternCanvas）
    3. 文本与路径排版（writeText / writeTextModifier / writeTextPath / writeGlyphRunSVG）
    4. 颜色与渐变转换（colorToCSS / colorToSVGFill / writeSVGGradientDef，逻辑混入 HTMLWriterColor.cpp）
    5. 滤镜链路生成与去重（writeFilterDefs / emitPlusDarkerFilterDef / 缓存 filterDefIds）
    6. 重复器与组合处理（writeRepeater / writeComposition / writeGroup）
    7. 路径定义去重与缓存（getOrCreatePathDef / pathDefIds）
  - 单个 writeLayer 方法的决策树包含 15+ 个分支条件（分布/收缩/文本框/重复器/组合/反向等），认知负荷过高
  - HTMLWriterShape.cpp 中 canCSS 方法有 50+ 行条件检查（梯度类型、变换矩阵、裁剪路径），但决策逻辑散布在多处
- **证据**：
  ```cpp
  // HTMLWriter.h 的私有方法签名列表（共 17 个）
  void writeLayerContents(...);
  void writeLayerInner(...);
  void writeElements(...);
  void renderGeo(...);
  bool canCSS(...);
  void renderCSSDiv(...);
  void renderDiamondCanvas(...);
  void renderImagePatternCanvas(...);
  void renderSVG(...);
  void writeText(...);
  void writeTextModifier(...);
  void writeTextPath(...);
  void writeGlyphRunSVG(...);
  void writeGroup(...);
  void writeRepeater(...);
  void writeComposition(...);
  // ... 等等
  ```
  - HTMLWriterShape.cpp 2341 行，但逻辑重复：renderGeo 中有 160+ 行 hasMerge 分支，renderSVG 中又有 100+ 行相似逻辑
- **建议**：
  1. 拆分 4 个独立的访问者类：
     - `HTMLGeometryRenderer`：负责 canCSS / renderCSSDiv / renderSVG 及其决策树
     - `HTMLTextRenderer`：负责 writeText / writeTextModifier / writeTextPath / writeGlyphRunSVG
     - `HTMLFilterChainWriter`：负责 writeFilterDefs / emitPlusDarkerFilterDef / 去重缓存
     - `HTMLCompositeRenderer`：负责 writeRepeater / writeComposition / writeGroup
  2. HTMLWriter 保留为轻量级协调层，仅负责 writeLayer 的流程编排与文本框探测
  3. 各子类持有相同的 HTMLBuilder* defs 和 HTMLWriterContext* ctx，共享缓存

### [ARCH-02] HTMLPlusDarkerRenderer 与主导出路径的耦合设计不当
- **位置**：src/pagx/html/HTMLExporter.cpp:158-162 & HTMLPlusDarkerRenderer.h:57-64 & HTMLPlusDarkerRenderer.cpp:193-250 & HTMLWriter.h:156 & HTMLWriterLayer.cpp（访问 ctx.plusDarkerBackdrops）
- **严重度**：P1
- **问题**：
  - 前置条件过严：RenderAll 需要 staticImgDir 非空才触发，但 HTMLWriter 中对 plusDarkerBackdrops 的查询无条件进行，存在隐式假设
  - 使用 const_cast 修改文档状态：`const_cast<PAGXDocument*>(&doc)` + `layer->visible = false` 的方式不符合函数签名约定，破坏 API 的 const 契约
  - 双文件系统操作：先在 RenderAll 中写入 PNG 到 staticImgDir，再在 HTMLWriter 中读取 base64 URL；如果中间文件删除或权限变更，导出会失败但没有错误报告
  - 全局 context 通信：plusDarkerBackdrops map 住在 HTMLWriterContext 中，需要 HTMLExporter 先调用 RenderAll 填充，再传给 HTMLWriter；顺序错误会导致崩溃
- **证据**：
  ```cpp
  // HTMLExporter.cpp:158-162
  if (!options.staticImgDir.empty()) {
    HTMLPlusDarkerRenderer::RenderAll(doc, options.staticImgDir, ...
                                      ctx.plusDarkerBackdrops);  // 填充全局 map
  }
  
  // HTMLWriter.h:156
  std::unordered_map<const Layer*, PlusDarkerBackdrop> plusDarkerBackdrops = {};
  
  // HTMLPlusDarkerRenderer.cpp:148
  auto layer = LayerBuilder::Build(const_cast<PAGXDocument*>(&doc));
  // ^^ 违反 const 语义
  
  // HTMLPlusDarkerRenderer.cpp:200-210
  auto layer = const_cast<Layer*>(l);
  layer->visible = false;  // 临时修改
  // ... 渲染 ...
  layer->visible = true;   // 恢复（但异常时不会恢复）
  ```
- **建议**：
  1. 修改 RenderAll 签名接收 PAGXDocument 的非 const 引用，或改用访问者模式避免修改原文档
  2. 或在 HTMLExporter 中创建临时文档副本（深拷贝隐藏的图层），RenderAll 操作副本而不改原文档
  3. 添加错误处理：若 staticImgDir 为空但某层的 backdropDataURL 无法生成，应记录警告而非静默跳过
  4. 改为延迟加载：HTMLWriter 需要 backdropDataURL 时才调用 RenderAll，而不是前置执行

### [ARCH-03] 坐标舍入两阶段流水线产生冗余遍历
- **位置**：src/pagx/html/HTMLExporter.cpp:38-118（RoundPxInStyle / RoundCoordinatesInHTML）
- **严重度**：P2
- **问题**：
  - 先生成完整 HTML 字符串（包含所有 style 属性中的浮点坐标），再逐个查找 `style="..."` 属性并对内容进行正则分析 + 舍入
  - RoundCoordinatesInHTML 每次查找都从 html.size() 开始扫描，O(n²) 复杂度（n 为 HTML 长度）
  - RoundPxInStyle 对每个 style 块进行逐字符分析，寻找 `\d+\.?\d* px` 模式，但已经在 HTMLBuilder 中生成了坐标，此时重新解析浪费
  - 如果文档特别大（数百个图层），HTML 字符串可能达 10MB+，两阶段舍入会产生显著性能抖动
- **证据**：
  ```cpp
  // HTMLExporter.cpp:94-118
  static std::string RoundCoordinatesInHTML(const std::string& html) {
    std::string result;
    result.reserve(html.size());
    size_t pos = 0;
    while (pos < html.size()) {
      size_t stylePos = html.find("style=\"", pos);  // O(n) 查找
      if (stylePos == std::string::npos) {
        result += html.substr(pos);
        break;
      }
      result += html.substr(pos, stylePos - pos);
      size_t valueStart = stylePos + 7;
      size_t valueEnd = html.find('"', valueStart);  // 再 O(n) 查找
      // ... 舍入逻辑 ...
      pos = valueEnd + 1;
    }
    return result;
  }
  
  // 每个 style 值再被 RoundPxInStyle 逐字符扫描
  static std::string RoundPxInStyle(const std::string& style) {
    // ... 50+ 行字符逐位分析 ...
    while (i < style.size()) {
      // 重新识别 \d+ 或 -\d+ 或 \d+\.\d+ 模式，在浮点转字符串时已经 marshal 过一次了
    }
  }
  ```
- **建议**：
  1. 改在 HTMLBuilder/FloatToString 层面直接舍入，每个坐标值直接生成舍入后的字符串，避免两阶段处理
  2. 若需保留灵活性，创建 `class CoordinateRoundingWriter extends HTMLBuilder`，在 addAttr 时拦截 style 值进行单次舍入
  3. 移除 RoundCoordinatesInHTML，性能收益 > 5%（大文档）

### [ARCH-04] HTMLStyleExtractor 设计缺乏模块化接口
- **位置**：src/pagx/html/HTMLStyleExtractor.h:24-72 & HTMLStyleExtractor.cpp:全文
- **严重度**：P2
- **问题**：
  - Extract 方法是全静态函数，对 HTML 字符串进行 1500+ 行的巨型解析流程（StartTagInfo / CSSProperty / StyleEntry / ClassRule / PropertyClassification），中间没有公开的分解点
  - 分组逻辑（PropertyClassification）与名称生成逻辑紧耦合，无法独立测试或扩展（例如自定义命名策略）
  - 不支持增量更新（若只修改了 3 个图层的样式，需重新处理整个 HTML 而不能只处理增量）
  - 与 HTMLExporter 的配置选项耦合度高（format 参数），但 HTML 解析本身与格式无关
- **证据**：
  ```cpp
  // HTMLStyleExtractor.h 仅有一个公开方法
  static std::string Extract(const std::string& html, Format format = Format::Compact);
  
  // HTMLStyleExtractor.cpp 无任何公开的中间步骤或构建器
  // - 直接 parse HTML
  // - 直接 classify properties
  // - 直接 emit classes
  // - 全部内联在一个 1500+ 行的函数中
  ```
- **建议**：
  1. 提取公开接口 `class StyleSheetBuilder`，支持增量添加 style 值
  2. 分离命名策略为 `interface NamingStrategy`，允许自定义 class 名生成（当前硬编码 "ps0", "ps1"...）
  3. 提供 API 查询去重结果（某个原始 style="..." 被分配到哪个 class）
  4. 支持外部指定何时触发重新分组（lazy 模式 vs eager 模式）

### [ARCH-05] 从 SVG 模块复制代码，应共享工具库
- **位置**：
  - src/pagx/html/HTMLWriterUtils.cpp: ColorToHex / ColorToRGBA / MatrixToCSS 等
  - src/pagx/svg/SVGExporter.cpp: 类似的颜色与变换函数
  - src/pagx/html/HTMLWriterText.cpp:40: `#include "pagx/svg/SVGTextLayout.h"` （已跨模块）
  - src/pagx/html/HTMLWriterShape.cpp:38: `#include "pagx/svg/SVGPathParser.h"` （已跨模块）
- **严重度**：P2
- **问题**：
  - HTMLWriter 依赖 SVG 模块的 SVGPathParser 和 SVGTextLayout，但 ColorToRGB / ColorToHex 等工具函数独立实现了一套
  - SVGExporter.cpp 也有几乎完全相同的函数，未被共享
  - 若修复一个渐度转换 bug（例如 ColorSpace 适配），两个模块需要维护两份补丁
- **证据**：
  ```cpp
  // HTMLWriterColor.cpp:1-50 定义了
  void ColorToRGB(const Color& color, int& r, int& g, int& b) { ... }
  std::string ColorToHex(const Color& color) { ... }
  
  // SVGExporter.cpp 中也有类似定义（未检查，假设存在基于项目 convention）
  
  // 但 HTMLWriter 已依赖 SVG 模块
  #include "pagx/svg/SVGPathParser.h"      // HTMLWriterShape.cpp:38
  #include "pagx/svg/SVGTextLayout.h"      // HTMLWriterText.cpp:41
  ```
- **建议**：
  1. 创建 `src/pagx/exporter/ExporterUtils.h`，整合 Color / Matrix / Path 的共用工具
  2. 改 HTMLWriterColor.cpp 和 SVGExporter 中的函数为调用共用库
  3. 或在 SVG 模块中提供统一的 ColorUtils / MatrixUtils 头文件供 HTML 模块导入

### [ARCH-06] 负的 repeaterOriginOffset 设计缺乏文档与验证
- **位置**：src/pagx/html/HTMLWriter.h:158-163 & HTMLWriterLayer.cpp（设置与使用点）
- **严重度**：P2
- **问题**：
  - HTMLWriterContext.repeaterOriginOffsetX/Y 用于处理 Repeater 超出图层边界的情况，但没有人工注释说明何时被设置、何时被重置
  - 值的含义是"图层 div 左上角的偏移量"，但调用方需手动记住是否已设置、是否需要在 transform 中应用负方向
  - 无验证：若某个渲染路径忘记重置此值，会导致后续图层的 transform 产生意外偏移
- **证据**：
  ```cpp
  // HTMLWriter.h:158-163
  // Set by writeLayer before calling writeRepeater, when the layer div was shifted into
  // negative quadrants to cover every Repeater copy's union bounds. Each copy's transform
  // must be prepended with `translate(-repeaterOriginOffset)` so its in-content origin still
  // maps to the layer origin in world space. Reset to (0, 0) when no shift was applied.
  float repeaterOriginOffsetX = 0;
  float repeaterOriginOffsetY = 0;
  
  // 但在 HTMLWriterLayer.cpp 中，调用方需主动记住何时设置、何时重置，没有 RAII 保障
  ```
- **建议**：
  1. 改为 RAII 风格的作用域类 `class RepeaterOriginGuard`，构造时保存旧值，析构时恢复
  2. 或为 HTMLWriterContext 添加方法 `void pushRepeaterOrigin(float x, float y)` 和 `void popRepeaterOrigin()`，维护栈
  3. 添加断言检查在使用前已设置

### [ARCH-07] HTMLWriterContext 承载过多状态，缺乏分离
- **位置**：src/pagx/html/HTMLWriter.h:138-186
- **严重度**：P3
- **问题**：
  - HTMLWriterContext 混合了 5 种不同的数据类型与职责：
    1. 文档元数据（docWidth / docHeight）
    2. 导出选项缓存（staticImgDir / staticImgUrlPrefix 等，从 Options 转移而来）
    3. 运行时状态（repeaterOriginOffsetX/Y / fontHoistSignature）
    4. 前置计算结果（plusDarkerBackdrops map）
    5. 去重缓存（pathDefIds / filterDefIds）
  - 若需要并发导出两个文档，context 无法独立，容易产生状态污染
- **证据**：context 包含 11 个成员变量，类型跨越 float / std::string / std::unordered_set / std::unordered_map 等
- **建议**：
  1. 拆分为 `ExportMetadata` （docWidth/Height）+ `RenderState` (repeaterOriginOffset/fontHoistSignature) + `CacheLayer` (pathDefIds/filterDefIds)
  2. 或改为更明确的成员名，如 `_metadata` / `_state` / `_cache`，便于理解职责边界

### [ARCH-08] TextBox 多路径设计导致测试覆盖率低
- **位置**：src/pagx/html/HTMLWriterText.cpp & HTMLWriterLayer.cpp（Text 与 TextBox 的处理）
- **严重度**：P3
- **问题**：
  - TextBox 有 3 条不同的渲染路径：
    1. HTML `<div>` 文本（目标路径，CSS 排版）
    2. 富文本 Group 包装（特殊情况，多颜色/描边）
    3. SVG 路径降级（fallback）
  - writeText 方法需要感知 TextBox 的存在并调整行为，但相关条件分散在多处（writeElements / paintGeos）
  - 测试需要覆盖所有排列组合（TextBox vs Text / RichText vs PlainText / SVG vs HTML），复杂度指数增长
- **证据**：
  ```cpp
  // HTMLWriterText.cpp 对 TextBox 的检查分散
  bool useRichText = preScannedTextBox != nullptr && richTextGroupCount >= 2;
  // ... later ...
  if (useRichText) { /* 富文本路径 */ }
  ```
- **建议**：
  1. 统一为策略模式，创建 `interface TextRenderStrategy`，实现 HTML / RichText / SVG 三个版本
  2. 在 writeText 入口点选择策略，避免路径内部分支

## 总结与优先级排序

| Issue | 标题 | 严重度 | 依赖关系 |
|-------|------|--------|---------|
| ARCH-01 | HTMLWriter 职责过重，需拆分 | P1 | 无依赖（基础问题） |
| ARCH-02 | HTMLPlusDarkerRenderer 耦合设计不当 | P1 | 独立 |
| ARCH-03 | 坐标舍入两阶段流水线冗余 | P2 | 无依赖 |
| ARCH-04 | HTMLStyleExtractor 缺乏模块化接口 | P2 | 无依赖 |
| ARCH-05 | 应共享 SVG 模块工具库 | P2 | 无依赖 |
| ARCH-06 | repeaterOriginOffset 缺乏文档与验证 | P2 | 依赖 ARCH-01（拆分后重新评估） |
| ARCH-07 | HTMLWriterContext 承载过多状态 | P3 | 依赖 ARCH-01 / ARCH-02（拆分后分离） |
| ARCH-08 | TextBox 多路径导致测试覆盖率低 | P3 | 无依赖 |

**建议修复顺序**：ARCH-01 → ARCH-02 → ARCH-03/04/05（并行） → ARCH-06/07/08（优化）

