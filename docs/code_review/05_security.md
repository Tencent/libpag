# 安全性评审报告

## 威胁模型

PAGX→HTML 转换器处理来自不可信来源的 PAGX 文档，输出 HTML/SVG/CSS，供用户在浏览器查看。典型攻击者将恶意 PAGX 嵌入客户端应用或服务器导出流程，目标是：

- **XSS 注入**：逃逸 HTML/CSS 执行任意脚本
- **资源耗尽**：导出过程内存/CPU 爆炸
- **路径遍历**：通过文件名参数写入非预期位置
- **外部资源加载**：字体/图片 URL 允许 file:// 或 javascript: 协议

## Issue 清单

### [SEC-01] CSS style 属性内 font-family 未转义，XSS 注入

**位置**：
- `src/pagx/html/HTMLWriterLayer.cpp:309` - 第一处
- `src/pagx/html/HTMLWriterLayer.cpp:418` - 第二处

**严重度**：P0 (XSS)

**威胁**：
攻击者构造 PAGX 文档，在 Text 元素的 fontFamily 字段设置包含引号和 CSS 转义序列的恶意字符串，如 `';}</style><script>alert(1)</script><style>'`。当该字符串被直接拼接到 style 属性值中，可以逃逸出 CSS 上下文并执行任意 JavaScript。

```cpp
// 受影响代码示例 (line 309):
spanStyle += "font-family:'" + span.text->fontFamily + "'";

// 攻击载荷：fontFamily = ";}</style><script>alert(1)</script><style>"
// 生成的 HTML：
// style="font-family:';}</style><script>alert(1)</script><style>'"
// 浏览器解析：font-family 被终止，style 块也被闭合，然后执行 script 标签
```

**证据**：
- `HTMLWriterLayer.cpp:309` - 在 TextBox 路径中，fontFamily 被直接字符串连接（无转义）
- `HTMLWriterLayer.cpp:418` - 在 RichText 路径中，同样问题
- `HTMLBuilder.h:escapeAttr()` 仅转义 `&"'<>`，不足以防止 CSS 上下文逃逸

**PoC 思路**：
```
构造 PAGX 文件，使某个 Text 元素的 fontFamily="Arial';}</style><script>alert('XSS')</script><style>'"
导出为 HTML
在浏览器打开 HTML，脚本执行
```

**建议**：
1. `FontHoist.cpp:105-112` 已正确使用单引号并对 `'` 进行转义（`\'`），但这仅用于 hoisted 字体
2. 未 hoist 的字体在 `HTMLWriterLayer.cpp` 直接拼接，需要应用相同转义逻辑
3. 改为调用一个安全的 EscapeCSSIdentifier 函数或直接使用 FontSignatureToCss 的转义方法

---

### [SEC-02] font-family 字段二阶 CSS 注入（多路径未覆盖）

**位置**：
- `src/pagx/html/HTMLWriterText.cpp` - 其他文本渲染路径

**严重度**：P0 (XSS)

**威胁**：
除了 HTMLWriterLayer 中的两个直接注入点，还需检查 HTMLWriterText.cpp 中的文本渲染逻辑。

**证据**：
Grep 结果显示在 HTMLWriterText.cpp 中多处引用 `text->fontFamily`，但未看到转义调用

**建议**：
对所有涉及 fontFamily 的代码路径进行审计，统一使用转义函数

---

### [SEC-03] SVG data: URI 中的 HTML/XML 注入

**位置**：`src/pagx/html/HTMLWriterFilter.cpp:633`

**严重度**：P1 (条件 XSS，仅 HTML 外壳易受影响)

**威胁**：
在 `writeClipDef` 中，SVG 内容被百分比编码并嵌入 data: URI。虽然只编码了 `< > # "`，但如果 SVG 内容本身未被充分清理（例如在 SVG 文本内容、属性值中的用户输入），仍可能逃逸：

```cpp
// 位置: HTMLWriterFilter.cpp:633
std::string dataURI = "url('data:image/svg+xml," + encoded + "')";
```

攻击面：SVG 属性值（如 filter id、mask id）若来自用户输入，可能包含未转义的引号或 URL 特殊字符。

**证据**：
- HTMLBuilder 对属性值进行了转义（escapeAttr），但 `_defs->addAttr("id", id)` 中的 id 基于 `_ctx->nextId()` 生成，目前是安全的
- 但如果将来允许用户自定义 ID 或从 PAGX 获取，则风险存在

**建议**：
1. SVG 属性值已通过 HTMLBuilder.escapeAttr 保护（转义 `&"'<>`）
2. SVG 文本内容应转义 `&<>`（已通过 HTMLBuilder.addTextContent 处理）
3. 继续禁止用户直接指定 SVG 元素 ID，仅用内部生成的序号

---

### [SEC-04] image->filePath 允许 file:// 或任意 URL scheme

**位置**：`src/pagx/html/HTMLWriterUtils.cpp:120`

**严重度**：P1 (取决于浏览器沙箱)

**威胁**：
Image 元素的 filePath 被直接通过 `EscapeCSSUrl()` 转义后用于 `url(...)` CSS 函数，不检查 scheme。恶意 PAGX 可设置 filePath 为 `file:///etc/passwd` 或 `javascript:alert(1)`，浏览器可能尝试加载这些资源：

```cpp
// 位置: HTMLWriterUtils.cpp:114-121
std::string GetImageSrc(const Image* image) {
  if (image->data) {
    // 内联 data: URI，安全
    return "data:" + std::string(mime) + ";base64," + Base64Encode(...);
  }
  return EscapeCSSUrl(image->filePath);  // 无 scheme 检查！
}
```

**PoC 思路**：
```
PAGX 中创建 Image，设置 filePath="file:///etc/hosts"
导出 HTML，渲染时浏览器可能试图读取本地文件
某些旧浏览器或特定扩展配置可能成功
```

**证据**：
- `EscapeCSSUrl()` 仅转义 CSS 特殊字符 `()'"\ `，不处理 URL scheme
- 无对 filePath 的 scheme 验证或白名单

**建议**：
1. 检查 filePath 是否以 `http://` 或 `https://` 开头
2. 若不是则视为相对 URL，需确保相对路径不逃逸（不包含 `..` 或绝对路径标记）
3. 考虑配置选项 `allowExternalImages` 控制是否允许 filePath，默认禁用

---

### [SEC-05] staticImgUrlPrefix 未验证，可能指向恶意源

**位置**：`src/pagx/html/HTMLExportOptions` 和相关使用

**严重度**：P1 (SSRF 风险)

**威胁**：
导出选项 `staticImgUrlPrefix` 由调用者控制。若设置为恶意 URL 如 `https://attacker.com/steal?id=`，生成的 HTML 会从该源加载静态图片，导致：
- SSRF（如果内部网络可达）
- 信息泄露（通过 URL 参数）

```cpp
// 位置: HTMLWriterShape.cpp:2259
out.addAttr("src", _ctx->staticImgUrlPrefix + fileName);
// 若 staticImgUrlPrefix = "https://attacker.com/?id="，则 src 变为外部 URL
```

**证据**：
- `HTMLExportOptions` 无对 `staticImgUrlPrefix` 的格式检查
- 调用者可任意指定该值

**建议**：
1. 文档明确说明 `staticImgUrlPrefix` 应为相对路径（如 `"static-img/"` 或 `"../assets/"`）
2. 在 HTMLExporter::ToHTML 中，验证 staticImgUrlPrefix 不包含 `://` 或不是绝对 URL
3. 若需要跨域，要求显式 CORS/CSP 设置

---

### [SEC-06] Base64 编解码边界检查充分，但结果信任风险

**位置**：`src/pagx/utils/Base64.cpp`

**严重度**：P2 (信息完整性)

**威胁**：
Base64 编码/解码实现本身较为健壮（检查了 padding、长度等），但 MIME 类型检测仅基于文件头：

```cpp
// 位置: HTMLWriterUtils.cpp:86-99
const char* DetectImageMime(const uint8_t* bytes, size_t size) {
  if (size >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
    return "image/png";
  }
  // ... 其他检查
  return "application/octet-stream";  // 默认值
}
```

若文件头伪造（例如 PNG 魔数后跟嵌入的 JavaScript），浏览器可能在特定上下文解析为不同类型。

**证据**：
- MIME 检测基于前 8 字节
- 无深层文件验证或结构校验
- 若未来允许 SVG 嵌入，SVG+XML MIME 与 XML/HTML 解析风险接近

**建议**：
1. 当前仅检测 PNG/JPEG/WebP/GIF，已是图像类型，风险较低
2. 若允许更多类型，考虑使用 tgfx::ImageCodec 进行更严格的格式验证
3. 对于 SVG，使用 `image/svg+xml` 需谨慎，考虑是否应禁用或沙箱化

---

### [SEC-07] 深度嵌套 Composition 可导致栈溢出

**位置**：`src/pagx/html/HTMLWriter.h:142` 及相关遍历

**严重度**：P2 (DoS)

**威胁**：
虽然 `visitedCompositions` 集合防止了无限循环，但深度嵌套（Composition A 包含 Composition B 包含 C...）可导致递归栈溢出。攻击者构造嵌套深度为 10000+ 的 PAGX，调用栈逐层增长。

**证据**：
- `HTMLWriterContext::visitedCompositions` 只防止循环，不限制深度
- 每层 Composition 调用 `writeComposition` → `writeElements` → `writeLayer` → 递归

**建议**：
1. 添加最大递归深度限制（如 256 或 512）
2. 在 `writeComposition` 入口检查深度，超过限制则返回或以占位符替代
3. 使用线程本地或上下文变量追踪当前深度

---

### [SEC-08] 巨大梯度停止点导致 O(n²) 字符串构造

**位置**：`src/pagx/html/HTMLWriterUtils.cpp:140-160` 及 `HTMLWriterColor.cpp:80-147`

**严重度**：P2 (DoS/内存)

**威胁**：
Gradient 的 colorStops 数组每个停止点都被转换为字符串并拼接。若恶意 PAGX 包含数万个颜色停止点，拼接操作导致内存占用和处理时间指数增长：

```cpp
// 位置: HTMLWriterUtils.cpp:140-160
std::string CSSStops(const std::vector<ColorStop*>& stops) {
  // ...
  for (size_t i = 0; i < stops.size(); i++) {
    // 每次迭代都拼接，共 O(n²) 字符复制
    r += ColorToRGBA(stops[i]->color) + ' ' + FloatToString(...) + '%';
  }
  return r;
}
```

**证据**：
- 无对 stops.size() 的上限检查
- 字符串拼接使用 `+=`，每次创建新字符串副本

**建议**：
1. 在 PAGX 解析阶段或 HTMLWriter 入口限制 colorStops 数量（如 <= 1024）
2. 改用 reserve + append 优化拼接
3. 若超限，截断或以占位符替代

---

### [SEC-09] 文件路径写入无验证，潜在目录遍历

**位置**：`src/pagx/html/HTMLExporter.cpp:213-226` 和 `HTMLWriterShape.cpp:2226, 2300`

**严重度**：P1 (任意文件写入)

**威胁**：
`ToFile()` 和静态图片写入使用用户提供的路径进行 `create_directories()` 和 `ofstream`：

```cpp
// 位置: HTMLExporter.cpp:207-226
bool HTMLExporter::ToFile(const PAGXDocument& document, const std::string& filePath, ...) {
  auto dirPath = std::filesystem::path(filePath).parent_path();
  if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);  // 无验证！
  }
  std::ofstream file(filePath, std::ios::binary);
  // ...
}

// 位置: HTMLWriterShape.cpp:2226
std::string absPath = _ctx->staticImgDir;
// ...
std::filesystem::create_directories(_ctx->staticImgDir);  // 无验证
```

攻击者若能控制 `filePath` 或 `staticImgDir`，可：
- 创建目录遍历路径如 `../../etc/passwd`
- 写入应用根目录外的位置
- 覆盖系统关键文件

**证据**：
- `std::filesystem::path` 本身支持 `..` 和绝对路径
- 无 canonicalize 或白名单检查

**建议**：
1. 对 filePath 进行 canonicalize（`std::filesystem::weakly_canonical`）
2. 验证结果仍在预期的基目录内
3. 禁止绝对路径，仅允许相对路径（`path.is_absolute()` 检查）
4. 对 staticImgDir 同样处理

示例伪代码：
```cpp
auto canonical = std::filesystem::weakly_canonical(filePath);
auto baseDir = std::filesystem::weakly_canonical(options.outputBaseDir);
if (!isSubpath(canonical, baseDir)) {
  return false;  // 拒绝越界路径
}
```

---

### [SEC-10] 未验证 MIME 类型白名单，可能加载脚本

**位置**：`src/pagx/html/HTMLWriterUtils.cpp:114-121`

**严重度**：P1 (XSS via data: URI)

**威胁**：
`DetectImageMime()` 返回默认 `"application/octet-stream"` 若无法识别。若攻击者上传伪装的图片（实际是 HTML 或 SVG），内联 data: URI 可能被浏览器解释为 HTML/XML 并执行脚本：

```
假设上传的 "image" 内容实际是:
  <html><script>alert(1)</script></html>
  
若 MIME 检测失败，返回 "application/octet-stream"，
浏览器在某些上下文（如 <embed> 或 <object>）可能自行推断类型并执行。
```

**证据**：
- `DetectImageMime()` 返回 `"application/octet-stream"` 作为后备
- HTMLBuilder 会生成 `<img src="data:application/octet-stream;base64,...">`
- 现代浏览器通常尊重 MIME 类型，但某些扩展或旧版本可能有漏洞

**建议**：
1. 在 `GetImageSrc()` 对 MIME 类型做白名单验证，仅允许 `image/png, image/jpeg, image/webp, image/gif`
2. 对于无法识别的数据，不生成 data: URI，而是返回空或占位符

```cpp
std::string GetImageSrc(const Image* image) {
  if (image->data) {
    auto mime = DetectImageMime(image->data->bytes(), image->data->size());
    // 白名单检查
    if (mime != "image/png" && mime != "image/jpeg" && 
        mime != "image/webp" && mime != "image/gif") {
      return "";  // 或返回占位符
    }
    return "data:" + std::string(mime) + ";base64," + Base64Encode(...);
  }
  return EscapeCSSUrl(image->filePath);
}
```

---

## 总结与优先级排序

### P0 - 高危 (立即修复)
1. **SEC-01** - CSS style 中 font-family XSS (HTMLWriterLayer:309, 418)
2. **SEC-02** - 其他文本路径中的 font-family 未覆盖
3. **SEC-04** - image->filePath 无 scheme 检查，允许 file:// SSRF
4. **SEC-09** - 文件写入无路径验证，可能目录遍历

### P1 - 中危 (尽快修复)
5. **SEC-05** - staticImgUrlPrefix 未验证，SSRF 风险
6. **SEC-10** - MIME 类型无白名单，可能加载脚本
7. **SEC-03** - SVG data: URI 中的注入风险（条件 XSS）

### P2 - 低危 (后续改进)
8. **SEC-07** - 深嵌套 Composition 栈溢出 DoS
9. **SEC-08** - 巨大梯度停止点导致 O(n²) 字符串拼接
10. **SEC-06** - Base64 MIME 检测基于文件头，易欺骗（信息完整性风险）

### 修复建议汇总

| Issue | 修复方式 |
|-------|---------|
| SEC-01, SEC-02 | 所有 fontFamily 使用都应通过 FontSignatureToCss 逻辑或 EscapeCSSIdentifier 转义 |
| SEC-04 | Image::filePath 限制为 http/https 或相对路径，或禁用 filePath 默认值 |
| SEC-05 | 验证 staticImgUrlPrefix 为相对路径，禁止 `://` 和绝对路径 |
| SEC-09 | 对所有文件路径进行 canonicalize 和子路径检查 |
| SEC-10 | MIME 类型白名单，仅允许已知图像类型 |
| SEC-07 | 添加最大递归深度限制（建议 256） |
| SEC-08 | 限制 colorStops 数量（建议 <= 1024） |

