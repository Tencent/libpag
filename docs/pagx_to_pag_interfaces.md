# PAGX → PAG 公开接口草案（M0-5）

> 依据：`docs/pagx_to_pag_design.md` §2/§4/§8/§13.3、`spec/pag_binary_spec.md` v1.0-draft1。
> 本文是接口草案（签名与语义），一期编码时按此落到 `include/pagx/` 头文件。

## 1. PAGExporter（include/pagx/PAGExporter.h）

与 `SVGExporter`、`HTMLExporter`、`PPTExporter` 同一命名方式；实现放 `src/pagx/pag/PAGExporter.cpp`，
只编入 CLI 与测试（`PAG_BUILD_CLI` / `PAG_BUILD_TESTS`）。

```cpp
namespace pagx {

// 警告码：不改变导出结果的完整性信息；--strict 时任何警告都导致失败。
enum class PAGExportWarning {
  MissingFont,            // 导出期找不到字体（列出字体名与节点）
  MissingImage,           // 外部图片缺失或解码失败，该 Image 无数据
  MissingSubDocument,     // 外部子 PAGX 缺失或解析失败，该图层输出为空
  UnsupportedURL,         // 子文档/图片路径是 URL（含 ://），不支持
  AnimatedContentSkipped, // 一期：动效内容只导出静态画面（§12.3，列出被忽略元素与文档）
  LayoutChannelDropped,   // 指向布局专用通道的动画/DataBind 被删除（XML 下同样不生效）
  MixedEmbeddedText,      // TextBox 内嵌入/未嵌入混合，整组运行时排版
  EmbeddedTextBound,      // 嵌入字形文本被 DataBind/动画驱动，修改后依赖宿主字体
  InvalidAnimationTiming, // duration<=0 或 frameRate<=0，原样保留，运行时时长为 0
  NonFiniteValue,         // Inf 等非有限值导出报错的明细
  ImageScaledDown,        // 图片按最大显示缩放缩小
};

struct PAGExportIssue {
  PAGExportWarning code;
  std::string message;    // 人读信息
  std::string scope;      // 所在文档（主文档或子文档诊断名）
  std::string nodePath;   // 节点定位描述，可空
};

struct PAGExportResult {
  bool success = false;
  std::vector<PAGExportIssue> warnings;
  size_t byteSize = 0;    // 产出 .pag 字节数
};

struct PAGExportOptions {
  bool strict = false;                     // 任何 warning 都失败
  std::string reportPath;                  // 非空则写 report.json
  std::vector<std::string> fallbackFonts;  // --fallback <path|name>，与 pagx embed/render 同名同义
  // 图片处理参数固定为默认值（§4.5），不提供调节。
};

class PAGExporter {
 public:
  // 发布流水线（§4.1）：递归加载外部子 PAGX 与图片 → 注册导出期字体 → applyLayout
  // → 布局扁平化 → 剥离 → 图片处理 → 编码。导出前克隆文档，不修改调用方的文档。
  static PAGExportResult ExportToPAG(std::shared_ptr<PAGXDocument> document,
                                     const std::string& filePath,
                                     const PAGExportOptions& options = {});
};

}  // namespace pagx
```

要点：
- 导出前克隆文档（扁平化/剥离是破坏性的）。
- 字体注册顺序：文档内 Font 的 `file` 自动注册 → `fallbackFonts` 补充（§4.3）。
- 结果确定性：同一输入、同一工具版本逐字节相同（T3）；报告不含时间戳等机器可变信息。

## 2. PAGImporter（include/pagx/PAGImporter.h）

```cpp
namespace pagx {

enum class PAGFormat { PAGLegacy, PAGv4, PAGX, Unknown };

struct PAGImportResult {
  bool success = false;
  PAGFormat format = PAGFormat::Unknown;
  std::string error;      // 失败原因（版本过高、截断、超上限、引用环…）
  std::vector<PAGExportIssue> warnings;  // 复用警告结构（tagLevel 提示、超限明细等）
  uint16_t tagLevel = 0;  // 解码统计（§3.2）；> MaxSupportedTagLevel 表示有被跳过的 Tag
};

class PAGImporter {
 public:
  // 只读前 16 字节识别格式（§3.6），供 PAGFile.Load、播放视图路由。
  static PAGFormat DetectFormat(const std::string& filePath);
  static PAGFormat DetectFormat(const void* bytes, size_t length);

  // v4 → PAGXDocument（渲染子集，布局已扁平化）。运行时与工具链都编入。
  // 失败返回 nullptr + result.error；解码不抛异常（项目禁异常）。
  static std::shared_ptr<PAGXDocument> LoadFromPAG(const std::string& filePath,
                                                   PAGImportResult* result = nullptr);
  static std::shared_ptr<PAGXDocument> LoadFromPAG(std::shared_ptr<pagx::Data> bytes,
                                                   PAGImportResult* result = nullptr);

  // 旧版本（v0–v3）→ PAGXDocument：经 Codec::Decode → pag::File → 转换。
  // 只编入工具链（PAG_BUILD_CLI/PAG_BUILD_TESTS）；运行时遇旧版本返回错误码提示走旧接口。
  static std::shared_ptr<PAGXDocument> LoadFromLegacyPAG(const std::string& filePath,
                                                         PAGImportResult* result = nullptr);
};

}  // namespace pagx
```

要点：
- 解码产物是 `PAGXDocument`，交 `PAGScene::Make` 走 `applyLayout()`（§4.2 扁平化加载语义）。
- 资源上限（规范 §11）超限时拒绝加载并给出 `error`，不崩溃。
- `PAGFile.Load` 遇 v4 返回明确错误码提示改用新接口（§3.7 对象模型分离）。

## 3. 全局字体注册（include/pagx/PAGFont.h 扩展）

对应旧 `PAGFont::RegisterFont` / `SetFallbackFontNames`，底层接 PAGX 的 `FontConfig`（§4.3）：

```cpp
namespace pagx {
class PAGFont {
 public:
  // 按路径或内存注册字体；内部全局缓存，线程安全。与旧 API 同名同义，宿主迁移成本低。
  static bool RegisterFont(const std::string& fontPath);
  static bool RegisterFont(const void* data, size_t length);
  // 回退字体族名列表，按序尝试；对应旧 SetFallbackFontNames。
  static void SetFallbackFontNames(const std::vector<std::string>& names);
};
}  // namespace pagx
```

文档需要的字体经 `PAGXDocument::getRequiredFonts()` 查询；`pagx info` 列出同样清单。

## 4. report.json 结构（--report）

```json
{
  "input": "input.pagx",
  "output": "output.pag",
  "success": true,
  "byteSize": 40960,
  "xmlSize": 153600,
  "compressionRatio": 0.267,
  "documents": [
    { "name": "main", "layers": 12, "nodes": 240, "images": 3, "subDocuments": 1 },
    { "name": "sub.pagx", "layers": 4, "nodes": 60, "images": 1, "subDocuments": 0 }
  ],
  "warnings": [
    { "code": "MissingFont", "scope": "main",
      "nodePath": "Layer[3]/Text[0]", "message": "font 'Foo' not found" }
  ],
  "fonts": [{ "family": "PingFang SC", "style": "Regular", "source": "embedded" }],
  "skipped": []
}
```

- 不含时间戳等机器可变信息；两次导出的报告逐字节相同。
- `compressionRatio` = byteSize / xmlSize，为 T7 基准观测字段。

## 5. 共用的本地外部文件加载（src/pagx/PAGXExternalFiles.h，内部接口）

CLI（export/render/verify/info）与 PAGViewer 共用，保证对比页第 1 列也显示子文档内容（§4.8）：

```cpp
namespace pagx {
// 按规范路径规则（相对发起文件所在目录；URL 不支持）递归读取外部子 PAGX 与图片，
// 逐个调用 doc->loadFileData(path, data)。缺失写入 issues，不中断其余文件。
bool LoadLocalExternalFiles(PAGXDocument* doc, const std::string& baseDir,
                            std::vector<PAGExportIssue>* issues);
}
```

一期实现静态子文档；子文档内的动画/VM 由运行时按现有语义跳过（二期生效）。
