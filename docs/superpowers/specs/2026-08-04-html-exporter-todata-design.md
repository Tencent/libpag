# HTMLExporter::ToData 内存 HTML ZIP 导出设计

> 状态：Draft
>
> 目标：在 libpag 的 `pagx::HTMLExporter` 中新增纯内存导出接口 `ToData()`，返回包含 `index.html` + `assets/**` 的自闭合 ZIP buffer（产物以内存 buffer 返回，不落盘）。对照 PPTX 双路径（`PPTExporter::ToData`，libpag commit `b711563bf`）的已验证模式实现。

## 1. 背景

当前 `HTMLExporter::ToHTML()` 的 `resourceDir` 语义是文件系统目录，导出时 libpag 向该目录写入 WOFF2 字体、栅格 PNG、PlusDarker backdrop 和外部图片副本。需要内存完整产物的上层调用方只能走"临时目录 → ToHTML → 读回 → 删目录"的中转，使磁盘成为纯内存导出的介质。

业务侧（ardot）期望与 PPTX 一致：host 提前下载图片字节喂入 `PAGXDocument`，libpag 直接返回自闭合的 HTML ZIP buffer，前端拿到单 buffer 直接存盘，不解析 manifest、不下载图片、不拼 zip。

## 2. 目标

- 新增 `HTMLExporter::ToData()`，返回标准 ZIP buffer（`index.html` + `assets/**`）；
- 产物不落盘，直接以内存 ZIP buffer 返回（与 `PPTExporter::ToData` 同模式）；
- 外部图片字节来源与 PPTX 一致：优先用 `image->data`（业务经 `loadFileDataMap()` 喂入），否则从本地 `filePath` 读取（libpag 可 IO，参考 PPTX `GetImageData`）；
- 保持现有 `ToHTML()`、`ToFile()` 的接口与行为逐字节不变；
- 复用 HTMLWriter 渲染逻辑，不复制约 1 万行 HTML 生成代码。

## 3. 非目标

- 不改变 `ToHTML()` 的 `resourceDir` 契约；
- 不改变 `ToFile()` 的 `.html + 同名资源目录` 行为；
- 不在 libpag 中增加网络下载能力；
- 不在本次改动中改变 HTML 视觉表现；
- 不新增 BMP / SVG 外部图片格式支持（现有 magic 检测覆盖 PNG/JPEG/WebP/GIF）；
- 不在本次改动中统一所有 exporter 的公共接口。

## 4. 公共接口

```cpp
// include/pagx/HTMLExporter.h
/**
 * Exports a PAGXDocument to an in-memory ZIP archive containing a full HTML
 * document at index.html and all auxiliary resources under assets/. The
 * archive is returned as a single buffer without writing any output file;
 * callers decide whether to save it, upload it, or hand it to another layer.
 *
 * Image bytes follow the same rules as PPTExporter: Image::data takes
 * precedence (populated via PAGXDocument::loadFileDataMap()), and images still
 * referenced only by a local filePath are read from disk. References that
 * resolve to neither (e.g. a "hash:" URI whose download is owned by the
 * caller) degrade gracefully: the image is omitted from the archive and the
 * archive stays valid.
 *
 * Returns nullptr when the archive cannot be produced. If errorMsg is non-null,
 * a human-readable description is written to *errorMsg.
 */
static std::shared_ptr<Data> ToData(PAGXDocument& document,
                                    const Options& options = {},
                                    std::string* errorMsg = nullptr);
```

- 固定 `HTMLOutputMode::FullDocument`，不暴露 mode 参数；
- `Data` 为 `pagx::types::Data`（`MakeWithCopy`），不是裸 HTML 字符串，语义在头文件注释中明确；
- 失败返回 `nullptr`；缺图**不失败**（降级，见第 8 节）；
- `errorMsg` 可选，失败时写入可定位资源的描述，不含资源字节。

## 5. 组件架构

```
ToHTML(): ctx(urlPrefix = resourceDir basename, resourceWriter = nullptr)
ToData(): ctx(urlPrefix = "assets/",          resourceWriter = &zipWriter)
              │
              ▼
        BuildHTML(doc, mode, options, ctx, err)   ← 共享主体（从 ToHTML 抽出）
              │
              ▼
        HTMLWriter 遍历（共享约 1 万行渲染逻辑）
              │
              │ 资源落地点调 ctx->writeResource(relativePath, bytes, size, err)
              ▼
   resourceWriter == nullptr ──→ 原写盘逻辑（行为逐字节不变）
   resourceWriter == zipWriter ─→ HTMLZipResourceWriter::write() → minizip 内存 ZIP
```

### 5.1 共享内存 ZIP 后端（抽取自 PPTExporter）

- 将 `PPTExporter.cpp:934-1030` 匿名 namespace 中的 `MemZipBuffer` + `MakeMemZipFileFunc` 抽到 `src/pagx/utils/MemZip.h/.cpp`（纯移动，行为不变）；
- `PPTExporter.cpp` 改为 `#include` 共享版本，删除本地副本；
- PPTX 行为由 `PAGXPPTTest` 兜底。

### 5.2 HTMLResourceWriter 抽象

```cpp
// src/pagx/html/HTMLResourceWriter.h
class HTMLResourceWriter {
 public:
  virtual ~HTMLResourceWriter() = default;
  virtual bool write(const std::string& relativePath, const void* bytes, size_t size,
                     std::string* errorMsg) = 0;
};

// HTMLZipResourceWriter：write() 调 minizip 加 entry；finish() 返回连续 Data buffer。
// entry 名即相对路径（ZIP 根），统一 '/' 分隔，无绝对路径、无 ".."。
```

`relativePath` 全部由 exporter 的 `ctx->nextId()` 生成（`dgc0.png`、`font_f0.woff2`、`img0.png` 等），**无任何外部可控字节进入 entry 名**，因此不需要针对路径穿越/绝对路径/重复 entry 的防御性校验——冲突由 `nextId()` 单调递增天然避免，非法字符不存在。

### 5.3 HTMLWriterContext 注入

`HTMLWriter.h:170` 的 `HTMLWriterContext` 新增：

```cpp
HTMLResourceWriter* resourceWriter = nullptr;

bool writeResource(const std::string& relativePath, const void* bytes, size_t size,
                   std::string* errorMsg) {
  if (resourceWriter) {
    // ToData：entry 名须与 HTML 引用 URL 逐字节一致，故加 staticImgUrlPrefix（"assets/"）；
    // ToHTML 下 resourceWriter 为 null，走下方原写盘逻辑，行为不变。
    return resourceWriter->write(staticImgUrlPrefix + relativePath, bytes, size, errorMsg);
  }
  // 原写盘逻辑原样保留：create_directories(staticImgDir + dirname) + ofstream 写文件
}

bool hasResourceOutput() const {
  return resourceWriter != nullptr || !staticImgDir.empty();
}
```

所有现有 `staticImgDir.empty()` 守卫（如 `HTMLWriterShape.cpp:2418`）改为 `!hasResourceOutput()`，否则 ToData 时（`staticImgDir` 为空）静态图会被跳过。**注意 `HTMLWriterShape.cpp:2329` 的 Conic 调度条件**（`!_ctx->staticImgDir.empty()`）同样必须改为 `_ctx->hasResourceOutput()`——它是"是否路由到 `renderConicCanvas`"的分支判断，不改则 ToData 下 Conic 永远走 CSS 回退路径。

### 5.4 共享 BuildHTML

从 `HTMLExporter.cpp:168-291` 抽出主体为内部函数 `BuildHTML(doc, mode, options, ctx, errorMsg)`，包含：`applyLayout`、PlusDarker pre-pass、字体 pre-pass、writer 遍历、样式抽取、FullDocument 包装。`ToHTML()` 组装 ctx（`urlPrefix` = resourceDir basename）后调用；`ToData()` 组装 ctx（`urlPrefix = "assets/"`、`resourceWriter = &zipWriter`）后调用。抽取为纯移动，`ToHTML()` 行为不变。

## 6. 数据流

```cpp
std::shared_ptr<Data> HTMLExporter::ToData(PAGXDocument& document, const Options& options,
                                           std::string* errorMsg) {
  if (!document.isLayoutApplied()) {
    document.applyLayout();
  }
  HTMLZipResourceWriter zipWriter;
  HTMLWriterContext ctx;
  ctx.docWidth = document.width;
  ctx.docHeight = document.height;
  ctx.resourceWriter = &zipWriter;
  ctx.staticImgUrlPrefix = "assets/";
  ctx.rasterScale = std::clamp(options.rasterScale, 0.01f, 4.0f);

  auto html = BuildHTML(document, HTMLOutputMode::FullDocument, options, ctx, errorMsg);
  if (html.empty()) {
    return nullptr;
  }
  if (!zipWriter.write("index.html", html.data(), html.size(), errorMsg)) {
    return nullptr;
  }
  return zipWriter.finish(errorMsg);
}
```

资源在 HTMLWriter 遍历过程中即时写入 ZIP stream，编码临时 buffer 写完后即可释放（`finish()` 返回前 zipWriter 持有全部已写 entry）。

## 7. 6 处资源落地点改造

| # | 资源 | 现位置 | 改造 |
|---|---|---|---|
| 1 | WOFF2 字体 | `HTMLExporter.cpp:220-232`（pre-pass） | `BuildWoff2FromFont` 已返回内存 bytes；写盘改 `ctx->writeResource("fonts/font_"+id+".woff2", ...)` |
| 2 | Diamond PNG | `HTMLWriterShape.cpp:2377-2391` | `RenderDiamondToPng` 改为返回 `std::shared_ptr<tgfx::Data>`（不再接收 outputPath）；调用点 `ctx->writeResource(id+".png", ...)` |
| 3 | Conic PNG | `HTMLWriterShape.cpp:2430-2438` | 同上 |
| 4 | ImagePattern PNG | `HTMLWriterShape.cpp:2534` | 同上 |
| 5 | PlusDarker | `HTMLPlusDarkerRenderer.cpp:212` | HTML 引用 `backdropDataURL`（base64，`HTMLWriterLayer.cpp:2719`），`pd_N.png` 写盘是**死输出**；`RenderAll` 增加 ctx 参数，ToData 时跳过写盘 |
| 6 | 外部图片 | `HTMLWriterUtils.cpp:214,224-231` | 见 7.1 |

### 7.1 外部图片（GetImageSrc）

字节获取语义与 PPTX `GetImageData`（`ImageFormatUtils.cpp:270-278`）完全一致：`image->data` 优先（业务经 `loadFileDataMap()` 喂入），否则从本地 `filePath` 读取；两者都取不到（如 `hash:` 引用，下载归调用方）则降级。`GetImageSrc` 在 ToData 模式（`ctx->resourceWriter` 非空）下独立走 ZIP asset 路径，ToHTML 原逻辑不动：

```cpp
std::string GetImageSrc(const Image* image, HTMLWriterContext* ctx) {
  if (ctx->resourceWriter) {
    // ToData：取字节（data 优先，否则读 filePath）→ magic 检测 → 写 ZIP asset
    auto bytes = GetImageBytes(image);   // 辅助函数，语义同 PPTX GetImageData
    if (!bytes.empty()) {
      auto mime = DetectImageMime(bytes.data(), bytes.size());
      if (mime) {
        std::string filename = ctx->nextId("img") + "." + MimeToExt(mime);
        ctx->writeResource(filename, bytes.data(), bytes.size(), err);
        ctx->externalImageAssets[image] = filename;   // 去重：同 Image 只写一次
        return ctx->staticImgUrlPrefix + filename;    // "assets/img0.png"
      }
    }
    return {};                                        // 读不到/格式未知 → 空 src 降级
  }
  // ← 原 ToHTML 逻辑（data → base64；filePath → copy_file / ifstream / EscapeCSSUrl），逐字节不变
}
```

`GetImageBytes` 内部即 `image->data` 的字节视图，或 `tgfx::Data::MakeFromFile(filePath)`（读不到返回空），libpag 可自由 IO。返回的 `"assets/img0.png"` 与 ZIP entry 名（`assets/img0.png`）逐字节一致——entry 名前缀由 5.3 的 `writeResource` 统一加 `staticImgUrlPrefix`，此处 `filename` 仅为 `img0.png`。

## 8. 错误处理

`ToData()` 返回 `nullptr` 表示整体失败。失败条件仅限 ZIP 层：

- ZIP writer 写入 entry 失败；
- `finish()` 失败；
- 结果 buffer 为空；
- HTML 生成失败（`BuildHTML` 返回空串，与 `ToHTML` 现有失败语义一致）。

**不失败**的场景（降级，与 PPTX `PPTWriterContext.h:63-64` 一致）：

- 外部图片取不到字节（`hash:` 引用未喂入，或本地文件读取失败）：返回空 src，ZIP 仍有效返回；
- 图片格式无法识别（`DetectImageMime` 返回 null）：src 缺失，与现有 `GetImageSrc` 行为一致。

第一版不提供"忽略缺图继续导出"之外的开关，也不提供"缺图整体失败"选项（与业务侧 60s 下载超时兜底继续导出的约束一致）。

## 9. ZIP 布局

```
index.html
assets/
  fonts/
    font_f0.woff2          ← 内嵌矢量字体
  img0.png                 ← 生成栅格 PNG（Diamond/Conic/ImagePattern，第一版扁平放 assets/ 根）
  img1.png|jpeg|webp|gif   ← 外部图片（magic 检测扩展名）
```

- 第一版不做 `assets/images/` / `assets/generated/` 三层目录（需在 6 个落地点区分类别，收益低）；
- 全部 entry 使用 `/` 分隔符；
- entry 名由 `nextId()` 生成，天然满足唯一、无绝对路径、无 `..`；
- `assets/` 前缀由 5.3 的 `writeResource` 统一添加（`staticImgUrlPrefix`），各落地点仅传相对路径（`img0.png`、`fonts/font_f0.woff2`）；
- `index.html` 为保留路径，与资源名无冲突可能。

## 10. CMake 改动

HTMLExporter 源码与 `src/pagx/utils/*` 均随 `PAG_BUILD_PAGX` 编译（`CMakeLists.txt:273-277` 的 HTML exporter glob 无 `PAG_BUILD_HTML` 门控），而 `PAG_BUILD_HTML`/`PAG_BUILD_PPT`/`PAG_BUILD_CLI`/`PAG_BUILD_TESTS` 都强制 `PAG_BUILD_PAGX`，故 minizip 依赖的实际锚点是 PAGX：

- `CMakeLists.txt:265-271`：minizip 源码（`zip.c`、`ioapi.c`）+ `MINIZIP_DIR` include 从 `PAG_BUILD_PPT` 分支移到独立的 `if (PAG_BUILD_PAGX)` 块；`PAG_BUILD_PPT` 分支只保留 `PAGX_PPT_SOURCES` glob；
- `CMakeLists.txt:380-388`：`if (PAG_BUILD_PAGX)` 块内追加 `list(APPEND PAG_INCLUDES ${TGFX_DIR}/third_party/out/zlib/${INCLUDE_ENTRY})`；
- `CMakeLists.txt:398-401`：PPT 块删除 zlib include 行，仅保留 `list(APPEND PAG_DEFINES PAG_BUILD_PPT)`；
- `src/pagx/utils/MemZip.h/.cpp` 加入 `PAGX_UTILS_SOURCES`（`CMakeLists.txt:210` 已 glob `utils/*.*`，自动纳入）。

`pagx/wechat/CMakeLists.txt` 是独立 CMake 项目（不读主 CMakeLists，编译宏仅 `PAG_BUILD_PAGX` + `PAG_USE_HARFBUZZ`），其 `file(GLOB_RECURSE PAGX_UTILS_SOURCES .../src/pagx/utils/*.cpp)` 会自动编译 MemZip.cpp 但该构建无 minizip 资源，需在 utils glob 后加 FILTER 排除（仿现有 `Woff2FontGenerator.cpp` 先例）：

```cmake
list(FILTER PAGX_UTILS_SOURCES EXCLUDE REGEX "MemZip\\.cpp$")
```

## 11. 测试计划（新增 test/src/PAGXHTMLDataTest.cpp）

ZIP 验证方式照抄 `PAGXPPTTest`（`HasZipMagic` + buffer 内搜 entry 名，不引 unzip 库）：

### 11.1 接口与 ZIP
- 有效 document 返回非空 `Data`，`HasZipMagic` 通过；
- buffer 中含 `index.html`、`assets/fonts/font_*.woff2`、`assets/img*.png` entry 名；
- **仅断言 entry 名，不搜 entry 内容**：ZIP entry 内容经 DEFLATE 压缩，明文（如 `<!DOCTYPE html>`）不会出现在 buffer 中，只有 entry 名在 local file header / central directory 中明文可见。验证 HTML 内容（DOCTYPE、URL-entry 对应关系）需用 minizip `unz*` 解压 index.html entry，本测试套件不引 unzip 库，故不验证。

### 11.2 资源
- WOFF2 字体进入 `assets/fonts/`；
- Diamond / Conic / ImagePattern 栅格 PNG 进入 ZIP；
- PlusDarker 场景正常导出，backdrop 以 base64 内联（ZIP 中**无** `pd_*.png` entry）；
- 通过 `loadFileDataMap()` 注入的 PNG/JPEG/WebP 图片进入 ZIP，扩展名由 magic 决定；
- 相同 Image 被多处引用时只产生一个 entry。

### 11.3 错误与降级
- 图片取不到字节（`hash:` 未喂入 / 本地文件读取失败）：`ToData()` 仍返回非空 ZIP（不失败），HTML 中对应 src 为空，不崩溃；
- 本地 `filePath` 图片可被读取并写入 ZIP asset（与 PPTX `GetImageData` 一致）。

### 11.4 产物不落盘
- `ToData()` 不接受 `resourceDir`，返回的 `Data` 即为完整 ZIP buffer；
- 测试执行前后，工作目录无新增资源目录或临时文件（产物以 buffer 返回，不写盘）；

### 11.5 回归
- 现有 `PAGXHtmlTest`（BatchConvertAll + 截图比较 + 字符串断言）全部通过，证明 `ToHTML` 行为不变；
- `PAGXPPTTest` 全部通过，证明 MemZip 抽取后 PPTX 行为不变；
- **不做** PPTX 式的 ToData/ToFile 逐字节比对（HTML 的 URL 前缀 ToFile 为 basename、ToData 为 `assets/`，天然不同）；改为验证解压后的渲染结果与 ToFile 视觉一致（复用现有 `HtmlScreenshotCompare` 能力评估）。

## 12. 兼容性

| 接口 | 返回值 | 输出资源 | 文件系统行为 |
|---|---|---|---|
| `ToHTML()` | HTML string | `resourceDir` | 不变 |
| `ToFile()` | bool | HTML 文件 + 资源目录 | 不变 |
| `ToData()` | ZIP `Data` | ZIP 内 `index.html + assets/**` | 产物不落盘（libpag 可自由 IO，参考 PPTX） |

新增声明遵循 `include/pagx/` 现有头文件的 ABI/导出宏约定。`MemZip` 抽取为纯移动，PPTX 对外行为不变。

## 13. 性能与内存

- 资源生成后立即写入 ZIP stream，编码临时 buffer 尽快释放；
- 外部图片去重避免重复写入；
- 避免无必要的 `Data` 全量复制（`finish()` 直接返回 zipWriter 持有的 buffer）；
- 压缩级别沿用 minizip 默认配置（`Z_DEFAULT_COMPRESSION`），与 PPTX 一致。

## 14. 线程安全

- `ToData()` 为无状态静态方法，不保留 document 指针；
- 并发调用必须使用不同 `PAGXDocument`；
- 每次调用使用独立 ZIP writer 和 HTML writer context；
- 无共享临时目录，不存在输出路径竞争。

## 15. 实施步骤

1. 抽取 `MemZipBuffer`/`MakeMemZipFileFunc` 到 `src/pagx/utils/MemZip.h/.cpp`，PPTX 迁移引用，跑 `PAGXPPTTest`；
2. 新增 `HTMLResourceWriter.h` + `HTMLZipResourceWriter`；
3. `HTMLWriterContext` 新增 `resourceWriter` 字段、`writeResource()`、`hasResourceOutput()`；
4. 6 处资源落地点改造（渲染函数改返回 bytes + 调用点改 `writeResource`）；
5. 抽取 `BuildHTML` 共享主体，`ToHTML` 保持行为不变；
6. 新增 `HTMLExporter::ToData()`；
7. CMake 放宽 minizip/zlib 挂载条件；
8. 新增 `PAGXHTMLDataTest` 测试；
9. 跑全量 `PAGFullTest`（含 `PAGXHtmlTest`、`PAGXPPTTest` 回归）。

## 16. 待讨论项

1. ZIP 内生成资源保持 `dgc*`/`cgc*`/`img*` 现有命名还是统一按内容摘要命名（第一版维持现状）；
2. 外部图片进 ZIP 后，解压出的 HTML 在本地文件协议下的跨域限制是否影响业务（超出本接口范围，由业务侧评估）。
