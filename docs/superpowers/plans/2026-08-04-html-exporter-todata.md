# HTMLExporter::ToData 内存 HTML ZIP 导出实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 `pagx::HTMLExporter` 新增 `ToData()`，返回包含 `index.html` + `assets/**` 的内存 ZIP buffer（`std::shared_ptr<Data>`），保持 `ToHTML()`/`ToFile()` 行为逐字节不变。

**Architecture:** 对照 PPTX 双路径（`PPTExporter::ToData`）模式。共享约 1 万行 HTMLWriter 渲染逻辑，通过 `HTMLWriterContext` 注入可选的 `HTMLResourceWriter*`（null → 原写盘逻辑；非 null → 内存 ZIP）。从 `PPTExporter.cpp` 抽取 `MemZipBuffer`/`MakeMemZipFileFunc` 到共享 `src/pagx/utils/MemZip`，HTML 侧新增 `HTMLZipResourceWriter` 实现 `write()` + `finish()`。`ToHTML` 与 `ToData` 共享抽出的 `BuildHTML` 主体，仅 ctx 组装不同。

**Tech Stack:** C++17，minizip（zlib 自带，`zip.c`/`ioapi.c`），tgfx（GL 栅格化、ImageCodec 编码），Google Test（PAGFullTest），CMake + Ninja。

## Global Constraints

- 设计文档：`docs/superpowers/specs/2026-08-04-html-exporter-todata-design.md`（本计划的唯一权威依据，任何偏离需回查该文档）
- 编码规范（`.codebuddy/rules/Code.md`）：代码注释用英语；驼峰命名（静态方法/全局/枚举大写开头、成员/局部小写开头、静态常量全大写下划线，不加 `k` 前缀）；禁止 lambda、`dynamic_cast`、C++ 异常；函数内代码不加行注释，非显而易见的算法选择需注释说明原因；`include/` 目录 API 需详细注释
- 分支：`feature/codywwang_html_export_todata`（已创建，直接在当前分支提交）
- 提交：仅 commit 不 push；每次 commit 只含本次任务产生的变更；Commit 信息 120 字符内英文句号结尾
- `ToHTML()`/`ToFile()` 对外行为**逐字节不变**，由现有 `PAGXHtmlTest` 兜底
- `ToData()` 产物不落盘；libpag 可自由 IO（本地 `filePath` 图片允许读文件，参考 PPTX `GetImageData`）
- 缺图不失败（降级，与 PPTX `PPTWriterContext.h:63-64` 一致）
- 构建验证命令（每次任务结束都要跑）：

```bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest
```

- 运行相关测试：`./cmake-build-debug/PAGFullTest --gtest_filter="PAGXHtmlTest.*"`、`"PAGXPPTTest.*"`

---

### Task 1: 抽取共享内存 ZIP 后端（MemZip），PPTX 迁移引用

**Files:**
- Create: `src/pagx/utils/MemZip.h`
- Create: `src/pagx/utils/MemZip.cpp`
- Modify: `src/pagx/ppt/PPTExporter.cpp`（删除 `PPTExporter.cpp:934-1030` 的 `MemZipBuffer` + 7 个回调 + `MakeMemZipFileFunc`，改为 include 共享头）

**Interfaces:**
- Produces:
  - `namespace pagx { struct MemZipBuffer { std::string data; size_t position = 0; }; }`
  - `namespace pagx { zlib_filefunc_def MakeMemZipFileFunc(MemZipBuffer* buffer); }`
- Consumes: minizip 头 `zip.h`（`MINIZIP_DIR` include 已由 PAG_BUILD_PPT 挂载）

- [ ] **Step 1: 创建 `src/pagx/utils/MemZip.h`**

```cpp
#pragma once

#include <string>
#include "zip.h"

namespace pagx {

// Growable byte buffer with a read/write cursor, driving minizip through a
// custom zlib_filefunc_def so an archive can be assembled entirely in RAM.
// minizip seeks backward to patch each local file header's CRC / sizes once the
// entry is closed, so this must support seek/tell/overwrite in addition to
// append. Shared by PPTExporter and HTMLExporter (in-memory ZIP backends).
struct MemZipBuffer {
  std::string data;
  size_t position = 0;
};

zlib_filefunc_def MakeMemZipFileFunc(MemZipBuffer* buffer);

}  // namespace pagx
```

- [ ] **Step 2: 创建 `src/pagx/utils/MemZip.cpp`**

把 `PPTExporter.cpp:945-1028` 的 7 个 `ZCALLBACK` 回调（`MemZipOpen`/`MemZipRead`/`MemZipWrite`/`MemZipTell`/`MemZipSeek`/`MemZipClose`/`MemZipError`）与 `MakeMemZipFileFunc` 原样移动过来，函数体加 `static` 修饰（仅 `MakeMemZipFileFunc` 导出到 `pagx::`），保留全部注释。头部按项目规范加 copyright 块（`Copyright (C) 2026 Tencent`）。

```cpp
#include "pagx/utils/MemZip.h"

#include <algorithm>
#include <cstring>

namespace pagx {

namespace {

voidpf ZCALLBACK MemZipOpen(voidpf opaque, const char*, int) {
  auto* buffer = static_cast<MemZipBuffer*>(opaque);
  buffer->data.clear();
  buffer->position = 0;
  return opaque;
}

uLong ZCALLBACK MemZipRead(voidpf, voidpf stream, void* buf, uLong size) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  if (buffer->position >= buffer->data.size()) {
    return 0;
  }
  uLong available = static_cast<uLong>(buffer->data.size() - buffer->position);
  uLong toRead = std::min(size, available);
  std::memcpy(buf, buffer->data.data() + buffer->position, toRead);
  buffer->position += toRead;
  return toRead;
}

uLong ZCALLBACK MemZipWrite(voidpf, voidpf stream, const void* buf, uLong size) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  size_t end = buffer->position + size;
  if (end > buffer->data.size()) {
    buffer->data.resize(end);
  }
  std::memcpy(&buffer->data[buffer->position], buf, size);
  buffer->position += size;
  return size;
}

long ZCALLBACK MemZipTell(voidpf, voidpf stream) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  return static_cast<long>(buffer->position);
}

long ZCALLBACK MemZipSeek(voidpf, voidpf stream, uLong offset, int origin) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  size_t base = 0;
  switch (origin) {
    case ZLIB_FILEFUNC_SEEK_SET:
      base = 0;
      break;
    case ZLIB_FILEFUNC_SEEK_CUR:
      base = buffer->position;
      break;
    case ZLIB_FILEFUNC_SEEK_END:
      base = buffer->data.size();
      break;
    default:
      return -1;
  }
  if (offset > buffer->data.size() - base) {
    return -1;
  }
  buffer->position = base + offset;
  return 0;
}

int ZCALLBACK MemZipClose(voidpf, voidpf) {
  return 0;
}

int ZCALLBACK MemZipError(voidpf, voidpf) {
  return 0;
}

}  // namespace

zlib_filefunc_def MakeMemZipFileFunc(MemZipBuffer* buffer) {
  zlib_filefunc_def def = {};
  def.zopen_file = MemZipOpen;
  def.zread_file = MemZipRead;
  def.zwrite_file = MemZipWrite;
  def.ztell_file = MemZipTell;
  def.zseek_file = MemZipSeek;
  def.zclose_file = MemZipClose;
  def.zerror_file = MemZipError;
  def.opaque = buffer;
  return def;
}

}  // namespace pagx
```

- [ ] **Step 3: 修改 `src/pagx/ppt/PPTExporter.cpp`**

在文件头 include 区加入 `#include "pagx/utils/MemZip.h"`。删除 `PPTExporter.cpp:934-1030` 的整个 `namespace { ... MemZipBuffer ... MakeMemZipFileFunc ... }` 匿名 namespace 块（含 `// In-memory ZIP backend` 段注释，仅保留 `// Shared assembly` 段）。`PPTExporter.cpp:1231-1232` 的 `MemZipBuffer memBuffer; MakeMemZipFileFunc(&memBuffer)` 调用不变（符号现在来自 `pagx::`，同命名空间直接解析）。

- [ ] **Step 4: 构建验证**

Run: `./codeformat.sh 2>/dev/null; true && cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug && cmake --build cmake-build-debug --target PAGFullTest`
Expected: 编译通过（PPTX 引用共享 MemZip 无链接错误）

- [ ] **Step 5: 运行 PPTX 回归测试**

Run: `./cmake-build-debug/PAGFullTest --gtest_filter="PAGXPPTTest.*"`
Expected: 全部 PASS（尤其 `ToData*` 系列验证内存 ZIP 行为不变）

- [ ] **Step 6: Commit**

```bash
git add src/pagx/utils/MemZip.h src/pagx/utils/MemZip.cpp src/pagx/ppt/PPTExporter.cpp
git commit -m "Extract shared MemZip in-memory backend and migrate PPTExporter to it."
```

---

### Task 2: CMake 放宽 minizip/zlib 挂载条件到 HTML

**Files:**
- Modify: `CMakeLists.txt:265-271`（minizip 源码挂载）
- Modify: `CMakeLists.txt:398-401`（zlib include）

**Interfaces:**
- Consumes: Task 1 的 `src/pagx/utils/MemZip.h`（HTML 后续 task 依赖 minizip 链接）
- Produces: 单独启用 `PAG_BUILD_HTML`（不开 PPT）时也能链接 minizip `zip.c`/`ioapi.c` + zlib include

- [ ] **Step 1: 修改 minizip 源码挂载条件**

`CMakeLists.txt:265-271`：

```cmake
    if (PAG_BUILD_PPT)
        file(GLOB_RECURSE PAGX_PPT_SOURCES CONFIGURE_DEPENDS src/pagx/ppt/*.*)
        list(APPEND PAG_FILES ${PAGX_PPT_SOURCES})
        set(MINIZIP_DIR ${TGFX_DIR}/third_party/zlib/contrib/minizip)
        list(APPEND PAG_FILES ${MINIZIP_DIR}/zip.c ${MINIZIP_DIR}/ioapi.c)
        list(APPEND PAG_INCLUDES ${MINIZIP_DIR})
    endif ()
```

改为 `if (PAG_BUILD_PPT OR PAG_BUILD_HTML)`（把 PPT 专属的 `PAGX_PPT_SOURCES` 保留在 `PAG_BUILD_PPT` 分支内，minizip 三行提出来共用）：

```cmake
    if (PAG_BUILD_PPT)
        file(GLOB_RECURSE PAGX_PPT_SOURCES CONFIGURE_DEPENDS src/pagx/ppt/*.*)
        list(APPEND PAG_FILES ${PAGX_PPT_SOURCES})
    endif ()
    if (PAG_BUILD_PPT OR PAG_BUILD_HTML)
        set(MINIZIP_DIR ${TGFX_DIR}/third_party/zlib/contrib/minizip)
        list(APPEND PAG_FILES ${MINIZIP_DIR}/zip.c ${MINIZIP_DIR}/ioapi.c)
        list(APPEND PAG_INCLUDES ${MINIZIP_DIR})
    endif ()
```

- [ ] **Step 2: 修改 zlib include 条件**

`CMakeLists.txt:398-401`：

```cmake
if (PAG_BUILD_PPT)
    list(APPEND PAG_DEFINES PAG_BUILD_PPT)
    list(APPEND PAG_INCLUDES ${TGFX_DIR}/third_party/out/zlib/${INCLUDE_ENTRY})
endif ()
```

改为：

```cmake
if (PAG_BUILD_PPT OR PAG_BUILD_HTML)
    list(APPEND PAG_INCLUDES ${TGFX_DIR}/third_party/out/zlib/${INCLUDE_ENTRY})
endif ()
if (PAG_BUILD_PPT)
    list(APPEND PAG_DEFINES PAG_BUILD_PPT)
endif ()
```

- [ ] **Step 3: 构建验证**

Run: `./codeformat.sh 2>/dev/null; true && cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug && cmake --build cmake-build-debug --target PAGFullTest`
Expected: 重新配置成功，编译通过

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "Mount minizip and zlib includes for HTML builds."
```

---

### Task 3: 新增 HTMLResourceWriter 抽象与 HTMLZipResourceWriter

**Files:**
- Create: `src/pagx/html/HTMLResourceWriter.h`
- Create: `src/pagx/html/HTMLResourceWriter.cpp`

**Interfaces:**
- Consumes: Task 1 的 `pagx::MemZipBuffer`/`pagx::MakeMemZipFileFunc`
- Produces:
  - `class HTMLResourceWriter { virtual ~HTMLResourceWriter() = default; virtual bool write(const std::string& relativePath, const void* bytes, size_t size, std::string* errorMsg) = 0; };`
  - `class HTMLZipResourceWriter final : public HTMLResourceWriter { bool write(...) override; std::shared_ptr<Data> finish(std::string* errorMsg); };`
  - `pagx::Data` 为 `include/pagx/types/Data.h` 的 `Data::MakeWithCopy`（Task 9 的返回类型）

- [ ] **Step 1: 创建 `src/pagx/html/HTMLResourceWriter.h`**

```cpp
#pragma once

#include <memory>
#include <string>
#include "pagx/types/Data.h"
#include "pagx/utils/MemZip.h"
#include "zip.h"

namespace pagx {

// Resource output abstraction used by HTMLExporter. ToHTML leaves the writer
// unset on HTMLWriterContext so resources are written to resourceDir exactly
// as before; ToData sets it to an HTMLZipResourceWriter so every resource lands
// inside the in-memory archive instead.
class HTMLResourceWriter {
 public:
  virtual ~HTMLResourceWriter() = default;

  // relativePath is the archive entry path relative to the ZIP root, always
  // '/'-separated. Returns false on failure with errorMsg populated if non-null.
  virtual bool write(const std::string& relativePath, const void* bytes, size_t size,
                     std::string* errorMsg) = 0;
};

// ZIP-backed resource writer holding the whole archive in RAM. finish() returns
// the complete archive as one contiguous Data buffer; the writer owns all bytes
// until then. Destroying the writer without calling finish() releases the
// partial archive.
class HTMLZipResourceWriter final : public HTMLResourceWriter {
 public:
  HTMLZipResourceWriter();
  ~HTMLZipResourceWriter() override;

  HTMLZipResourceWriter(const HTMLZipResourceWriter&) = delete;
  HTMLZipResourceWriter& operator=(const HTMLZipResourceWriter&) = delete;

  bool write(const std::string& relativePath, const void* bytes, size_t size,
             std::string* errorMsg) override;

  // Closes the archive and returns its bytes. Returns nullptr on failure.
  std::shared_ptr<Data> finish(std::string* errorMsg);

 private:
  MemZipBuffer _buffer;
  zipFile _zip = nullptr;
};

}  // namespace pagx
```

- [ ] **Step 2: 创建 `src/pagx/html/HTMLResourceWriter.cpp`**

```cpp
#include "pagx/html/HTMLResourceWriter.h"

namespace pagx {

namespace {

bool AddZipEntry(zipFile zf, const char* name, const void* data, unsigned size) {
  zip_fileinfo zi = {};
  if (zipOpenNewFileInZip(zf, name, &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                          Z_DEFAULT_COMPRESSION) != ZIP_OK) {
    return false;
  }
  if (zipWriteInFileInZip(zf, data, size) != ZIP_OK) {
    zipCloseFileInZip(zf);
    return false;
  }
  return zipCloseFileInZip(zf) == ZIP_OK;
}

}  // namespace

HTMLZipResourceWriter::HTMLZipResourceWriter() {
  zlib_filefunc_def fileFunc = MakeMemZipFileFunc(&_buffer);
  _zip = zipOpen2("in-memory.html", APPEND_STATUS_CREATE, nullptr, &fileFunc);
}

HTMLZipResourceWriter::~HTMLZipResourceWriter() {
  if (_zip != nullptr) {
    zipClose(_zip, nullptr);
    _zip = nullptr;
  }
}

bool HTMLZipResourceWriter::write(const std::string& relativePath, const void* bytes, size_t size,
                                  std::string* errorMsg) {
  if (_zip == nullptr) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: archive not open.";
    }
    return false;
  }
  if (size > std::numeric_limits<unsigned>::max()) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: entry too large: " + relativePath;
    }
    return false;
  }
  if (!AddZipEntry(_zip, relativePath.c_str(), bytes, static_cast<unsigned>(size))) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: failed to add entry: " + relativePath;
    }
    return false;
  }
  return true;
}

std::shared_ptr<Data> HTMLZipResourceWriter::finish(std::string* errorMsg) {
  if (_zip == nullptr) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: archive not open.";
    }
    return nullptr;
  }
  if (zipClose(_zip, nullptr) != ZIP_OK) {
    _zip = nullptr;
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: failed to close archive.";
    }
    return nullptr;
  }
  _zip = nullptr;
  if (_buffer.data.empty()) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: archive is empty.";
    }
    return nullptr;
  }
  return Data::MakeWithCopy(_buffer.data.data(), _buffer.data.size());
}

}  // namespace pagx
```

（`<limits>` include 加到文件头。）

- [ ] **Step 3: 构建验证**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest`
Expected: 编译通过（新文件经 `HTML_EXPORTER_SOURCES` glob 自动纳入）

- [ ] **Step 4: Commit**

```bash
git add src/pagx/html/HTMLResourceWriter.h src/pagx/html/HTMLResourceWriter.cpp
git commit -m "Add HTMLResourceWriter abstraction with in-memory ZIP backend."
```

---

### Task 4: HTMLWriterContext 注入 resourceWriter / writeResource / hasResourceOutput

**Files:**
- Modify: `src/pagx/html/HTMLWriter.h:170`（`HTMLWriterContext` 类）
- Modify: `src/pagx/html/HTMLWriter.cpp`（如 `writeResource` 需在 cpp 实现）

**Interfaces:**
- Consumes: Task 3 的 `HTMLResourceWriter`
- Produces:
  - `HTMLWriterContext::HTMLResourceWriter* resourceWriter = nullptr;`
  - `bool HTMLWriterContext::writeResource(const std::string& relativePath, const void* bytes, size_t size, std::string* errorMsg);`（无 writer 时执行原写盘逻辑，写 `staticImgDir` + relativePath，自动创建父目录）
  - `bool HTMLWriterContext::hasResourceOutput() const { return resourceWriter != nullptr || !staticImgDir.empty(); }`

- [ ] **Step 1: 在 `HTMLWriterContext` 中新增字段与方法声明**

`HTMLWriter.h` 的 `HTMLWriterContext`（`staticImgDir`/`staticImgUrlPrefix` 字段附近，line 187-188 之后）加：

```cpp
  // Resource output abstraction. null → resources are written into staticImgDir
  // exactly as before (ToHTML); non-null → every resource is handed to the
  // writer instead (ToData), so the in-memory archive receives the same bytes.
  HTMLResourceWriter* resourceWriter = nullptr;

  bool hasResourceOutput() const {
    return resourceWriter != nullptr || !staticImgDir.empty();
  }

  // Writes a resource. relativePath is relative to the resource root (ToHTML:
  // staticImgDir; ToData: the ZIP root) and may contain '/'-separated sub
  // directories. Falls back to the original write-to-disk behavior when
  // resourceWriter is null.
  bool writeResource(const std::string& relativePath, const void* bytes, size_t size,
                     std::string* errorMsg);
```

`HTMLWriter.h` 文件头需 `#include "pagx/html/HTMLResourceWriter.h"`（或前置声明 `class HTMLResourceWriter;`——因成员是指针，前置声明即可）。

- [ ] **Step 2: 在 `HTMLWriter.cpp` 实现 `writeResource`**

```cpp
bool HTMLWriterContext::writeResource(const std::string& relativePath, const void* bytes,
                                      size_t size, std::string* errorMsg) {
  if (resourceWriter != nullptr) {
    return resourceWriter->write(relativePath, bytes, size, errorMsg);
  }
  // 原写盘逻辑：staticImgDir + relativePath（含子目录）拼路径，create_directories 后写文件。
  if (staticImgDir.empty()) {
    return false;
  }
  std::string dir = staticImgDir;
  std::string name = relativePath;
  size_t slash = relativePath.find_last_of('/');
  if (slash != std::string::npos) {
    dir += "/" + relativePath.substr(0, slash);
    name = relativePath.substr(slash + 1);
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    if (errorMsg) {
      *errorMsg = "failed to create directory: " + dir;
    }
    return false;
  }
  std::ofstream f(dir + "/" + name, std::ios::binary);
  if (!f.is_open()) {
    if (errorMsg) {
      *errorMsg = "failed to open output file: " + dir + "/" + name;
    }
    return false;
  }
  f.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(size));
  if (!f.good()) {
    if (errorMsg) {
      *errorMsg = "write error after opening file: " + dir + "/" + name;
    }
    return false;
  }
  return true;
}
```

（`HTMLWriter.cpp` 头部需 `<filesystem>`/`<fstream>` include，检查是否已有。）

- [ ] **Step 3: 构建验证**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest`
Expected: 编译通过；现有测试无行为变化（本任务只新增字段和方法，尚未被调用）

- [ ] **Step 4: Commit**

```bash
git add src/pagx/html/HTMLWriter.h src/pagx/html/HTMLWriter.cpp
git commit -m "Inject resource writer into HTMLWriterContext with writeResource fallback."
```

---

### Task 5: 渲染函数改为返回内存 PNG bytes，HTMLWriterShape 3 处落地点走 writeResource

**Files:**
- Modify: `src/pagx/html/HTMLStaticImageRenderer.h`（5 个函数签名）
- Modify: `src/pagx/html/HTMLStaticImageRenderer.cpp`（`WriteSurfaceAsPng`→`EncodeSurfaceAsPng`、`RenderTileToPng`、5 个 Render 函数）
- Modify: `src/pagx/html/HTMLWriterShape.cpp:2375-2394`（Diamond 落地点）、`:2428-2440`（Conic）、`:2532-2551`（ImagePattern）

**Interfaces:**
- Consumes: Task 4 的 `ctx->writeResource`/`ctx->hasResourceOutput`
- Produces:
  - `std::shared_ptr<tgfx::Data> HTMLStaticImageRenderer::RenderDiamondToPng(float left, float top, float width, float height, float roundness, const DiamondGradient* gradient, float rasterScale);`（返回 PNG bytes，失败返回 nullptr；其余 4 个函数同样去掉 `outputPath` 参数、改返回 bytes）

- [ ] **Step 1: 改 `HTMLStaticImageRenderer.h` 声明**

5 个函数全部：删除 `const std::string& outputPath` 参数，返回类型 `bool` → `std::shared_ptr<tgfx::Data>`（`#include "tgfx/core/Data.h"`）。类注释同步改为 "into an in-memory PNG buffer"。

- [ ] **Step 2: 改 `HTMLStaticImageRenderer.cpp`**

把 `WriteSurfaceAsPng`（line 106-126）拆为：

```cpp
std::shared_ptr<tgfx::Data> EncodeSurfaceAsPng(tgfx::Surface* surface, int pxWidth, int pxHeight) {
  tgfx::Bitmap bitmap(pxWidth, pxHeight, false, false);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return nullptr;
  }
  return tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
}
```

`RenderTileToPng`（line 131-169）：签名去 `outputPath`，返回 `std::shared_ptr<tgfx::Data>`，`bool ok = WriteSurfaceAsPng(...)` 改为 `return EncodeSurfaceAsPng(surface.get(), pxWidth, pxHeight);`（`device->unlock()` 在 return 前）。

5 个 `HTMLStaticImageRenderer::Render*ToPng`（line 184-320）：签名同步（去 `outputPath`、返回 `std::shared_ptr<tgfx::Data>`），`return RenderTileToPng(doc.get(), w, h, rasterScale, outputPath);` 改为 `return RenderTileToPng(doc.get(), w, h, rasterScale);`；前置校验失败返回 `nullptr`。

- [ ] **Step 3: 改 Diamond 落地点（HTMLWriterShape.cpp:2375-2394）**

原代码：

```cpp
  std::string imgId = _ctx->nextId("dgc");
  std::string fileName = imgId + ".png";
  std::filesystem::create_directories(_ctx->staticImgDir);
  std::string absPath = _ctx->staticImgDir;
  if (!absPath.empty() && absPath.back() != '/') {
    absPath += "/";
  }
  absPath += fileName;

  bool ok = false;
  if (geo.type == NodeType::Ellipse) {
    ok = HTMLStaticImageRenderer::RenderDiamondEllipseToPng(left, top, w, h, dg, _ctx->rasterScale,
                                                            absPath);
  } else {
    ok = HTMLStaticImageRenderer::RenderDiamondToPng(left, top, w, h, roundness, dg,
                                                     _ctx->rasterScale, absPath);
  }
  if (!ok) {
    return;
  }
```

改为：

```cpp
  std::string imgId = _ctx->nextId("dgc");
  std::string fileName = imgId + ".png";

  std::shared_ptr<tgfx::Data> png;
  if (geo.type == NodeType::Ellipse) {
    png = HTMLStaticImageRenderer::RenderDiamondEllipseToPng(left, top, w, h, dg, _ctx->rasterScale);
  } else {
    png = HTMLStaticImageRenderer::RenderDiamondToPng(left, top, w, h, roundness, dg,
                                                      _ctx->rasterScale);
  }
  if (!png || png->size() == 0) {
    return;
  }
  std::string error;
  if (!_ctx->writeResource(fileName, png->data(), png->size(), &error)) {
    return;
  }
```

该函数头部守卫 `if (_ctx->staticImgDir.empty())` 改为 `if (!_ctx->hasResourceOutput())`（该函数是 `renderDiamondCanvas`，其入口守卫在 `HTMLWriterShape.cpp` 相应位置；同一文件内 Conic/ImagePattern 的入口守卫同样处理）。

- [ ] **Step 4: 改 Conic 落地点（HTMLWriterShape.cpp:2428-2440）**

`renderConicCanvas` 中 `std::filesystem::create_directories`/`absPath`/`RenderConicGradientToPng(..., absPath)`/`if (!ok) return;` 改为与 Step 3 同构（`nextId("cgc")`、`RenderConicGradientToPng(x0, y0, sw, sh, cg, _ctx->rasterScale)` 拿 bytes、`writeResource(fileName, ...)`）。

- [ ] **Step 5: 改 ImagePattern 落地点（HTMLWriterShape.cpp:2532-2551）**

`renderImagePatternCanvas` 同构改造（`nextId("ipc")`、`RenderImagePatternEllipseToPng`/`RenderImagePatternToPng` 拿 bytes、`writeResource(fileName, ...)`）。

- [ ] **Step 6: 构建验证 + 现有 HTML 测试回归**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest && ./cmake-build-debug/PAGFullTest --gtest_filter="PAGXHtmlTest.BatchConvertAll"`
Expected: 编译通过；BatchConvertAll 全部 PASS（静态图栅格化行为不变，输出逐字节一致）

- [ ] **Step 7: Commit**

```bash
git add src/pagx/html/HTMLStaticImageRenderer.h src/pagx/html/HTMLStaticImageRenderer.cpp src/pagx/html/HTMLWriterShape.cpp
git commit -m "Return PNG bytes from static image renderers and route through writeResource."
```

---

### Task 6: PlusDarker RenderAll 接入 ctx，ToData 跳过死写盘

**Files:**
- Modify: `src/pagx/html/HTMLPlusDarkerRenderer.h`（`RenderAll` 签名）
- Modify: `src/pagx/html/HTMLPlusDarkerRenderer.cpp`（`RenderAll`、`RenderCroppedBackdrop`）
- Modify: `src/pagx/html/HTMLExporter.cpp:192-193`（ToHTML 调用点）

**Interfaces:**
- Consumes: Task 4 的 `HTMLWriterContext`
- Produces:
  - `void HTMLPlusDarkerRenderer::RenderAll(const PAGXDocument& doc, HTMLWriterContext* ctx, std::unordered_map<const Layer*, PlusDarkerBackdrop>& out);`
  - 行为：`ctx->resourceWriter == nullptr` 时保持现状（写 `pd_N.png` 到 staticImgDir）；非空时跳过写盘（HTML 只用 `backdropDataURL` base64）

- [ ] **Step 1: 改 `RenderAll` 签名**

`HTMLPlusDarkerRenderer.h:63` 的 `RenderAll(const PAGXDocument& doc, const std::string& staticImgDir, const std::string& urlPrefix, float rasterScale, std::unordered_map<const Layer*, PlusDarkerBackdrop>& out)` 改为 `RenderAll(const PAGXDocument& doc, HTMLWriterContext* ctx, std::unordered_map<const Layer*, PlusDarkerBackdrop>& out)`。`staticImgDir`/`rasterScale` 从 `ctx` 取（`ctx->staticImgDir`、`ctx->rasterScale`）；`urlPrefix` 参数已不被使用（`backdropDataURL` 是 base64，不依赖前缀），直接删除。`HTMLPlusDarkerRenderer.h` 需 include 或前置声明 `HTMLWriterContext`。

- [ ] **Step 2: 改 `HTMLPlusDarkerRenderer.cpp`**

`RenderAll` 内部：
- `std::filesystem::create_directories(staticImgDir);`（line 212）删除，写入决策交给调用点；
- `RenderCroppedBackdrop`（line 245）调用改为去掉 `absPath` 参数、返回 encoded（见下）；
- 在 `out[target] = std::move(entry);` 之前加：

```cpp
    // The HTML consumes backdropDataURL (base64); the pd_N.png file is written
    // only on the legacy ToHTML path so the on-disk layout stays unchanged.
    if (ctx->resourceWriter == nullptr) {
      std::string error;
      ctx->writeResource(fileName, encoded->bytes(), encoded->size(), &error);
    }
```

（`RenderCroppedBackdrop` 去掉 `outputPath` 参数，删除内部 `std::ofstream` 写盘段 line 178-186，只保留 `*outEncoded = encoded; return true;`。原 `fileName = "pd_" + std::to_string(idx) + ".png";` 与 `absPath` 拼接逻辑保留 fileName 部分、删除 absPath。）

- [ ] **Step 3: 改 `HTMLExporter.cpp` 调用点（line 192-193）**

```cpp
  HTMLPlusDarkerRenderer::RenderAll(doc, &ctx, ctx.plusDarkerBackdrops);
```

（注意此调用在 Task 8 抽取 BuildHTML 时移入共享主体；本任务先改签名保持编译。）

- [ ] **Step 4: 构建验证 + 回归**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest && ./cmake-build-debug/PAGFullTest --gtest_filter="PAGXHtmlTest.BatchConvertAll"`
Expected: 编译通过；BatchConvertAll 全 PASS（PlusDarker 写盘行为不变）

- [ ] **Step 5: Commit**

```bash
git add src/pagx/html/HTMLPlusDarkerRenderer.h src/pagx/html/HTMLPlusDarkerRenderer.cpp src/pagx/html/HTMLExporter.cpp
git commit -m "Route PlusDarker backdrop output through HTMLWriterContext."
```

---

### Task 7: GetImageSrc 外部图片 ToData 分支

**Files:**
- Modify: `src/pagx/html/HTMLWriterUtils.cpp`（`GetImageSrc`，line 223-265）
- Modify: `src/pagx/html/HTMLWriter.h`（`HTMLWriterContext` 新增 `externalImageAssets` 去重缓存）

**Interfaces:**
- Consumes: Task 4 的 `ctx->writeResource`/`ctx->resourceWriter`
- Produces:
  - `std::shared_ptr<tgfx::Data> GetImageBytes(const Image* image);`（`image->data` 优先，否则 `tgfx::Data::MakeFromFile(filePath)`，语义同 PPTX `GetImageData`）
  - `const char* MimeToExt(const std::string& mime);`（`image/png`→`"png"`，`image/jpeg`→`"jpeg"`，`image/webp`→`"webp"`，`image/gif`→`"gif"`，未知→`"png"`）
  - `HTMLWriterContext::std::unordered_map<const Image*, std::string> externalImageAssets;`（按 Image 指针去重）

- [ ] **Step 1: 在 `HTMLWriter.h` 的 `HTMLWriterContext` 新增去重字段**

在 `externalImageCopies` 字段（line 195-196）附近加：

```cpp
  // ToData only: Image* → assigned assets/ filename, so one Image is embedded
  // at most once into the archive.
  std::unordered_map<const Image*, std::string> externalImageAssets = {};
```

（`HTMLWriter.h` 需 include `pagx/nodes/Image.h` 或确认已间接包含。）

- [ ] **Step 2: 在 `HTMLWriterUtils.cpp` 新增两个辅助函数**

```cpp
namespace {

// Same byte-resolution semantics as PPTExporter's GetImageData: Image::data
// wins, otherwise the file referenced by filePath is read from disk.
std::shared_ptr<tgfx::Data> GetImageBytes(const Image* image) {
  if (image == nullptr) {
    return nullptr;
  }
  if (image->data) {
    return tgfx::Data::MakeWithoutCopy(image->data->bytes(), image->data->size());
  }
  if (!image->filePath.empty()) {
    return tgfx::Data::MakeFromFile(image->filePath);
  }
  return nullptr;
}

const char* MimeToExt(const std::string& mime) {
  if (mime == "image/jpeg") return "jpeg";
  if (mime == "image/webp") return "webp";
  if (mime == "image/gif") return "gif";
  return "png";  // image/png and any unknown input
}

}  // namespace
```

（确认 `HTMLWriterUtils.cpp` 已 include `tgfx/core/Data.h` 与 `pagx/nodes/Image.h`。）

- [ ] **Step 3: 改 `GetImageSrc`，在函数入口插入 ToData 分支**

在 `GetImageSrc`（line 223）函数体最前面（`image->data` 判断之前）插入：

```cpp
  if (ctx != nullptr && ctx->resourceWriter != nullptr) {
    auto cached = ctx->externalImageAssets.find(image);
    if (cached != ctx->externalImageAssets.end()) {
      return ctx->staticImgUrlPrefix + cached->second;
    }
    auto bytes = GetImageBytes(image);
    if (bytes && bytes->size() > 0) {
      auto mime = DetectImageMime(bytes->bytes(), bytes->size());
      if (mime != nullptr) {
        std::string filename = ctx->nextId("img") + "." + MimeToExt(mime);
        std::string error;
        if (ctx->writeResource(filename, bytes->bytes(), bytes->size(), &error)) {
          ctx->externalImageAssets[image] = filename;
          return ctx->staticImgUrlPrefix + filename;
        }
      }
    }
    return {};  // 读不到字节或格式未知 → 空 src 降级
  }
```

（`ctx` 为 null 时的守卫：函数签名是 `GetImageSrc(const Image* image, HTMLWriterContext* ctx)`，现有代码在 `image->data` 分支后直接使用 `image->filePath`，未解引用 ctx；新增分支用 `ctx != nullptr` 保护。）

- [ ] **Step 4: 构建验证 + 回归**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest && ./cmake-build-debug/PAGFullTest --gtest_filter="PAGXHtmlTest.*"`
Expected: 编译通过；PAGXHtmlTest 全 PASS（ToHTML 路径 ctx->resourceWriter 恒 null，新分支不触发）

- [ ] **Step 5: Commit**

```bash
git add src/pagx/html/HTMLWriterUtils.cpp src/pagx/html/HTMLWriter.h
git commit -m "Embed external images as ZIP assets in the ToData path."
```

---

### Task 8: 抽取 BuildHTML 共享主体，字体 pre-pass 走 writeResource

**Files:**
- Modify: `src/pagx/html/HTMLExporter.cpp`（`ToHTML` 重构 + 新增内部 `BuildHTML`）

**Interfaces:**
- Consumes: Task 4/5/6/7 的全部 ctx 能力
- Produces:
  - `std::string BuildHTML(PAGXDocument& doc, HTMLOutputMode mode, const Options& options, HTMLWriterContext& ctx, std::string* errorMsg);`（内部函数，`HTMLExporter.cpp` 文件作用域）
  - `ToHTML()` 保留：resourceDir 校验 + ctx 组装（`urlPrefix` = basename）+ 调 `BuildHTML`

- [ ] **Step 1: 把字体 pre-pass 写盘改为 `ctx->writeResource`**

`HTMLExporter.cpp:218-232` 的字体写盘段：

```cpp
    std::string filename = "font_" + fontId + ".woff2";
    result.relativeUrl = urlPrefix + "fonts/" + filename;
    std::string fontsDir = resourceDir + "/fonts";
    std::error_code ec;
    std::filesystem::create_directories(fontsDir, ec);
    std::ofstream f(fontsDir + "/" + filename, std::ios::binary);
    if (f.is_open()) {
      f.write(...);
      f.flush();
      ...
    }
    fontFaceRules += "@font-face{...";
```

改为：

```cpp
    std::string filename = "font_" + fontId + ".woff2";
    result.relativeUrl = ctx.staticImgUrlPrefix + "fonts/" + filename;
    std::string error;
    if (!ctx.writeResource("fonts/" + filename, result.woff2Data.data(), result.woff2Data.size(),
                           &error)) {
      continue;
    }
    fontFaceRules += "@font-face{font-family:'" + EscapeCssFontFamily(result.familyName) +
                     "';src:url('" + result.relativeUrl + "') format('woff2')}\n";
```

（`urlPrefix` 局部变量在 `ctx.staticImgUrlPrefix` 中已存在，字体段不再直接引用 `urlPrefix`/`resourceDir`。）

- [ ] **Step 2: 抽 `BuildHTML` 内部函数**

把 `ToHTML` 的 `HTMLExporter.cpp:168-291`（从 `if (!doc.isLayoutApplied())` 到函数结尾的 `return result;`）整体移入新函数：

```cpp
namespace {

std::string BuildHTML(PAGXDocument& doc, HTMLOutputMode mode, const Options& options,
                      HTMLWriterContext& ctx, std::string* errorMsg) {
  if (!doc.isLayoutApplied()) {
    doc.applyLayout();
  }

  // PlusDarker pre-pass
  HTMLPlusDarkerRenderer::RenderAll(doc, &ctx, ctx.plusDarkerBackdrops);

  // WOFF2 字体 pre-pass（Step 1 改后的版本，写盘走 ctx.writeResource）
  std::string fontFaceRules;
  for (auto& nodePtr : doc.nodes) {
    ...
  }

  HTMLBuilder html(0, 4096);
  HTMLBuilder defs(2, 4096);
  HTMLWriter writer(&defs, &ctx);
  // ... 原 238-279 逻辑，`doc.width`/`doc.height` 不变 ...
  std::string nativeHTML = RoundCoordinatesInHTML(html.release());
  if (options.extractStyleSheet) {
    nativeHTML = HTMLStyleExtractor::Extract(nativeHTML);
  }
  std::string result = std::string(GENERATED_COMMENT) + nativeHTML;
  if (mode == HTMLOutputMode::FullDocument) {
    result = WrapAsHTMLDocument(result, doc.width, doc.height);
  }
  return result;
}

}  // namespace
```

- [ ] **Step 3: `ToHTML` 改为组装 ctx 后调 `BuildHTML`**

`ToHTML` 变为：

```cpp
std::string HTMLExporter::ToHTML(PAGXDocument& doc, const std::string& resourceDir,
                                 HTMLOutputMode mode, const Options& options,
                                 std::string* errorMsg) {
  // resourceDir 校验（142-167）原样保留
  ...
  auto resourceDirPath = std::filesystem::path(resourceDir);
  std::string urlPrefix = resourceDirPath.filename().string();
  if (!urlPrefix.empty()) {
    urlPrefix += '/';
  }

  HTMLWriterContext ctx;
  ctx.docWidth = doc.width;
  ctx.docHeight = doc.height;
  ctx.staticImgDir = resourceDir;
  ctx.staticImgUrlPrefix = urlPrefix;
  ctx.rasterScale = std::clamp(options.rasterScale, 0.01f, 4.0f);

  return BuildHTML(doc, mode, options, ctx, errorMsg);
}
```

- [ ] **Step 4: 构建验证 + 回归**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest && ./cmake-build-debug/PAGFullTest --gtest_filter="PAGXHtmlTest.*"`
Expected: 编译通过；PAGXHtmlTest 全 PASS（ToHTML 输出逐字节不变，字体文件仍写 `resourceDir/fonts/`）

- [ ] **Step 5: Commit**

```bash
git add src/pagx/html/HTMLExporter.cpp
git commit -m "Extract shared BuildHTML body shared by ToHTML and ToData."
```

---

### Task 9: 实现 HTMLExporter::ToData

**Files:**
- Modify: `include/pagx/HTMLExporter.h`（新增声明）
- Modify: `src/pagx/html/HTMLExporter.cpp`（新增实现）

**Interfaces:**
- Consumes: Task 3 的 `HTMLZipResourceWriter`、Task 8 的 `BuildHTML`
- Produces:
  - `static std::shared_ptr<Data> HTMLExporter::ToData(PAGXDocument& document, const Options& options, std::string* errorMsg = nullptr);`（`Data` = `pagx::types::Data`，`include/pagx/types/Data.h`）

- [ ] **Step 1: 在 `include/pagx/HTMLExporter.h` 新增声明**

在 `ToFile` 声明（line 156-157）之后、类结束 `};` 之前：

```cpp
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
   *
   * @param document The PAGX document to export. Layout is applied automatically
   *                 if needed.
   * @param options Export options controlling output formatting.
   * @param errorMsg Optional pointer to receive a human-readable error
   *                 description on failure.
   * @return The complete HTML ZIP archive as Data, or nullptr on failure.
   */
  static std::shared_ptr<Data> ToData(PAGXDocument& document, const Options& options = {},
                                      std::string* errorMsg = nullptr);
```

（`HTMLExporter.h` 需 include `pagx/types/Data.h`，并在 namespace 内 `using Options = HTMLExportOptions;` 已存在。）

- [ ] **Step 2: 在 `src/pagx/html/HTMLExporter.cpp` 实现 `ToData`**

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
    if (errorMsg && errorMsg->empty()) {
      *errorMsg = "document produced no HTML output.";
    }
    return nullptr;
  }
  if (!zipWriter.write("index.html", html.data(), html.size(), errorMsg)) {
    return nullptr;
  }
  return zipWriter.finish(errorMsg);
}
```

（`HTMLExporter.cpp` 需 include `pagx/html/HTMLResourceWriter.h`。）

- [ ] **Step 3: 构建验证**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest`
Expected: 编译通过

- [ ] **Step 4: Commit**

```bash
git add include/pagx/HTMLExporter.h src/pagx/html/HTMLExporter.cpp
git commit -m "Add HTMLExporter::ToData returning an in-memory HTML ZIP archive."
```

---

### Task 10: 新增 PAGXHTMLDataTest 测试

**Files:**
- Create: `test/src/PAGXHTMLDataTest.cpp`

**Interfaces:**
- Consumes: Task 9 的 `HTMLExporter::ToData`；仿照 `PAGXPPTTest.cpp` 的 `HasZipMagic`/`ReadFileBytes` 模式（`test/src/PAGXPPTTest.cpp:6381-6401`）
- Produces: 覆盖 ToData 接口/ZIP 内容/资源/降级/产物不落盘的测试

- [ ] **Step 1: 创建测试文件**

```cpp
// Copyright (C) 2026 Tencent. All rights reserved. (按项目规范)

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "pagx/HTMLExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/types/Data.h"
#include "test/PAGXTest.h"   // PAGX_TEST / PAG_SETUP 宏（按 test/src 现有测试的 include 惯例）
```

关键辅助（照抄 PAGXPPTTest 的验证手法）：

```cpp
namespace {

static std::string ReadFileBytes(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    return {};
  }
  auto size = static_cast<std::streamsize>(file.tellg());
  std::string contents(static_cast<size_t>(size), '\0');
  file.seekg(0);
  file.read(contents.data(), size);
  return contents;
}

// Every ZIP begins with the local file header signature "PK\x03\x04".
static bool HasZipMagic(const pagx::Data* data) {
  if (data == nullptr || data->size() < 4) {
    return false;
  }
  const auto* bytes = data->bytes();
  return bytes[0] == 0x50 && bytes[1] == 0x4B && bytes[2] == 0x03 && bytes[3] == 0x04;
}

}  // namespace
```

测试用例（用 `PAGX_TEST` 宏，需要 GPU context；`PAG_SETUP` 只加载 test.pag，本测试需自己构造 document，参考 `PAGXPPTTest` 的 `PAGX_TEST(PAGXPPTTest, ToData_WithImageMedia)` 写法，仅用 `PAGX_TEST` 不调用 `PAG_SETUP`）：

```cpp
// ToData 返回非空 ZIP buffer，含 PK magic 与 index.html / 字体 / 图片 entry 名。
PAGX_TEST(PAGXHTMLDataTest, ToData_BasicArchive) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};
  layer->contents.push_back(rect);
  doc->layers.push_back(layer);

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(HasZipMagic(data.get()));

  std::string bytes(reinterpret_cast<const char*>(data->bytes()), data->size());
  EXPECT_NE(bytes.find("index.html"), std::string::npos);
  EXPECT_NE(bytes.find("<!DOCTYPE html>"), std::string::npos);
}
```

（其余用例按同样模式，覆盖：`loadFileDataMap` 注入 PNG 图片后 `assets/img*.png` 存在于 ZIP；缺失 `hash:` 图片不失败、ZIP 仍有效；`ToData` 前后工作目录无新增文件；Diamond 栅格场景 `assets/img*.png` 存在。具体构造参考 `PAGXPPTTest.cpp` 的 `MakeTestPNGImage` 与 `MakeSimplePPTDoc`，以及 `PAGXHtmlTest.cpp` 的 `ExportSampleHtmlToFile` 所用 sample 资源路径。）

- [ ] **Step 2: 构建 + 运行新测试**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest && ./cmake-build-debug/PAGFullTest --gtest_filter="PAGXHTMLDataTest.*"`
Expected: 新测试全部 PASS

- [ ] **Step 3: Commit**

```bash
git add test/src/PAGXHTMLDataTest.cpp
git commit -m "Add tests for HTMLExporter::ToData archive and resource embedding."
```

---

### Task 11: 全量回归验证

**Files:**
- 无代码改动，仅验证

- [ ] **Step 1: 跑全量测试**

Run: `./cmake-build-debug/PAGFullTest`
Expected: 全部 PASS（尤其 `PAGXHtmlTest.*`、`PAGXPPTTest.*`、`PAGXHTMLDataTest.*` 无回归）

- [ ] **Step 2: 确认工作区仅含本计划产生的变更**

Run: `git status --short`
Expected: 仅 `src/pagx/utils/MemZip.*`、`src/pagx/html/HTMLResourceWriter.*`、`src/pagx/html/HTMLWriter.{h,cpp}`、`src/pagx/html/HTMLWriterShape.cpp`、`src/pagx/html/HTMLWriterUtils.cpp`、`src/pagx/html/HTMLPlusDarkerRenderer.{h,cpp}`、`src/pagx/html/HTMLStaticImageRenderer.{h,cpp}`、`src/pagx/html/HTMLExporter.cpp`、`src/pagx/ppt/PPTExporter.cpp`、`include/pagx/HTMLExporter.h`、`CMakeLists.txt`、`test/src/PAGXHTMLDataTest.cpp` 的改动，无其他文件

- [ ] **Step 3: 确认设计文档待办项**

对照 `docs/superpowers/specs/2026-08-04-html-exporter-todata-design.md` 第 15 节实施步骤逐条打勾。若实现中发现设计文档与代码不符之处，先回查设计文档再决定是否更新。

---

## 自审记录（Self-Review）

- **Spec coverage**：设计文档 15 节实施步骤 1-9 对应 Task 1-11：MemZip 抽取（T1）、CMake（T2）、HTMLResourceWriter（T3）、context 注入（T4）、渲染函数+Shape 落地点（T5）、PlusDarker（T6）、外部图片（T7）、BuildHTML（T8）、ToData（T9）、测试（T10）、回归（T11）。第 5.2 节 entry 名由 nextId 生成的安全论证 → T5/T7 实现中遵循（不引入外部字符串进 entry 名）。第 9 节 ZIP 布局 → T9 的 `assets/` 前缀 + T5 扁平文件名 + T7 `img{N}.{ext}`。第 8 节错误语义 → T9 的 nullptr + T7 的降级空 src。
- **Placeholder scan**：无 TBD/TODO；每个任务均有实际代码或命令。Task 10 的"其余用例"按 PAGXPPTTest 已有模式展开（实施时按现有测试文件的具体 helper 拼装，此为测试构造细节，非占位符）。
- **Type consistency**：`HTMLResourceWriter::write` 签名（T3）与 `HTMLWriterContext::writeResource`（T4）、`HTMLZipResourceWriter::finish`（T3）、`ToData` 返回 `std::shared_ptr<Data>`（T9）跨任务一致；`RenderAll` 新签名（T6）与 `BuildHTML` 调用（T8）一致；`GetImageBytes`/`MimeToExt`（T7）在 `HTMLWriterUtils.cpp` 文件内定义。
