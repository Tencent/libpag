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
- Modify: `pagx/wechat/CMakeLists.txt`（utils glob 后 FILTER 排除 MemZip.cpp，仿 Woff2FontGenerator 先例）

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

- [ ] **Step 3: wechat 独立构建排除 MemZip.cpp**

`pagx/wechat/CMakeLists.txt` 是独立 CMake 项目（编译宏仅 `PAG_BUILD_PAGX` + `PAG_USE_HARFBUZZ`），通过 `file(GLOB_RECURSE PAGX_UTILS_SOURCES .../src/pagx/utils/*.cpp)` 自动编译 utils 全部源文件，但该构建既无 minizip include 也无 `zip.c`/`ioapi.c` 源文件。`MemZip.cpp` 一经创建，wechat 构建就会编译它并因 `zip.h` 缺失而失败。在 utils glob 后、现有 `Woff2FontGenerator.cpp` FILTER 旁加同款排除：

```cmake
# MemZip depends on minizip (zip.c/ioapi.c) which is only needed for PPT/HTML export.
list(FILTER PAGX_UTILS_SOURCES EXCLUDE REGEX "MemZip\\.cpp$")
```

（wechat 构建依赖 Emscripten 工具链，不在主构建验证命令范围内；FILTER 语法与既有行一致，CMake 配置阶段即生效。）

- [ ] **Step 4: 修改 `src/pagx/ppt/PPTExporter.cpp`**

在文件头 include 区加入 `#include "pagx/utils/MemZip.h"`。删除 `PPTExporter.cpp:934-1030` 的整个 `namespace { ... MemZipBuffer ... MakeMemZipFileFunc ... }` 匿名 namespace 块（含 `// In-memory ZIP backend` 段注释，仅保留 `// Shared assembly` 段）。`PPTExporter.cpp:1231-1232` 的 `MemZipBuffer memBuffer; MakeMemZipFileFunc(&memBuffer)` 调用不变（符号现在来自 `pagx::`，同命名空间直接解析）。

- [ ] **Step 5: 构建验证**

Run: `./codeformat.sh 2>/dev/null; true && cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug && cmake --build cmake-build-debug --target PAGFullTest`
Expected: 编译通过（PPTX 引用共享 MemZip 无链接错误）

- [ ] **Step 6: 运行 PPTX 回归测试**

Run: `./cmake-build-debug/PAGFullTest --gtest_filter="PAGXPPTTest.*"`
Expected: 全部 PASS（尤其 `ToData*` 系列验证内存 ZIP 行为不变）

- [ ] **Step 7: Commit**

```bash
git add src/pagx/utils/MemZip.h src/pagx/utils/MemZip.cpp src/pagx/ppt/PPTExporter.cpp pagx/wechat/CMakeLists.txt
git commit -m "Extract shared MemZip in-memory backend, migrate PPTExporter, and exclude it from the WeChat build."
```

---

### Task 2: minizip/zlib 挂载到 PAGX，PPT 条件收窄

**Files:**
- Modify: `CMakeLists.txt:265-271`（minizip 源码挂载从 PPT 分支上移到 PAGX 块）
- Modify: `CMakeLists.txt:380-388`（PAGX 块内追加 zlib include）
- Modify: `CMakeLists.txt:398-401`（PPT 块收窄为仅保留编译定义）

**Interfaces:**
- Consumes: Task 1 的 `src/pagx/utils/MemZip.h`（HTML/PPT 共享内存 ZIP 后端）
- Produces: 所有 `PAG_BUILD_PAGX` 构建均可链接 minizip `zip.c`/`ioapi.c` + zlib include；`PAG_BUILD_PPT` 条件仅保留 PPT 专用源码与编译定义

- [ ] **Step 1: minizip 源码挂载到 PAGX 块**

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

改为（PPT 分支只保留 PPT 专用源码；minizip 三行挂到 `PAG_BUILD_PAGX`，因为 `src/pagx/utils/*` 与 HTML exporter 源码均随 PAGX 编译，PAGX 是 minizip 依赖的实际锚点；`PAG_BUILD_HTML`/`PAG_BUILD_PPT`/`PAG_BUILD_CLI`/`PAG_BUILD_TESTS` 都强制 PAGX，故条件天然覆盖全部场景）：

```cmake
    if (PAG_BUILD_PPT)
        file(GLOB_RECURSE PAGX_PPT_SOURCES CONFIGURE_DEPENDS src/pagx/ppt/*.*)
        list(APPEND PAG_FILES ${PAGX_PPT_SOURCES})
    endif ()
    if (PAG_BUILD_PAGX)
        set(MINIZIP_DIR ${TGFX_DIR}/third_party/zlib/contrib/minizip)
        list(APPEND PAG_FILES ${MINIZIP_DIR}/zip.c ${MINIZIP_DIR}/ioapi.c)
        list(APPEND PAG_INCLUDES ${MINIZIP_DIR})
    endif ()
```

- [ ] **Step 2: zlib include 挂到 PAGX 块，PPT 块收窄为仅编译定义**

在 `CMakeLists.txt:380-388` 的 `if (PAG_BUILD_PAGX)` 块内、`list(APPEND PAG_DEFINES PAG_BUILD_PAGX)` 之后追加一行：

```cmake
if (PAG_BUILD_PAGX)
    list(APPEND PAG_DEFINES PAG_BUILD_PAGX)
    list(APPEND PAG_STATIC_VENDORS expat SheenBidi)
    list(APPEND PAG_INCLUDES third_party/SheenBidi/Headers
            third_party/expat/expat/lib)
    list(APPEND PAG_INCLUDES ${TGFX_DIR}/third_party/out/zlib/${INCLUDE_ENTRY})
    if (WIN32)
        list(APPEND PAG_DEFINES XML_STATIC)
    endif ()
endif ()
```

`CMakeLists.txt:398-401` 的 PPT 块删除 zlib include 行，只保留编译定义：

```cmake
if (PAG_BUILD_PPT)
    list(APPEND PAG_DEFINES PAG_BUILD_PPT)
endif ()
```

（zlib include 目录是 tgfx 构建产物，`PAG_BUILD_PAGX` 强制依赖 tgfx，故该目录在 PAGX 构建下必然存在。）

- [ ] **Step 3: 构建验证**

Run: `./codeformat.sh 2>/dev/null; true && cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug && cmake --build cmake-build-debug --target PAGFullTest`
Expected: 重新配置成功，编译通过

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "Mount minizip and zlib includes for all PAGX builds."
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
- Modify: `src/pagx/html/HTMLWriter.h:170`（`HTMLWriterContext` 类；`HTMLWriter` 为 header-only，`writeResource` inline 实现于本头文件）

**Interfaces:**
- Consumes: Task 3 的 `HTMLResourceWriter`
- Produces:
  - `HTMLWriterContext::HTMLResourceWriter* resourceWriter = nullptr;`
  - `bool HTMLWriterContext::writeResource(const std::string& relativePath, const void* bytes, size_t size, std::string* errorMsg);`（无 writer 时执行原写盘逻辑，写 `staticImgDir` + relativePath，自动创建父目录；有 writer 时 entry 名前加 `staticImgUrlPrefix`，保证 ZIP entry 名与 HTML 引用 URL 逐字节对应）
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

`HTMLWriter.h` 文件头需 `#include "pagx/html/HTMLResourceWriter.h"`（`writeResource` 在本头文件 inline 实现，需完整类型调用其 `write()`，前置声明不够）。

同步更新 `HTMLWriter.h:182-186` 中 `staticImgDir`/`staticImgUrlPrefix` 字段上方注释：原 "The resourceDir is mandatory at the public API boundary, so both fields are non-empty when HTMLWriter runs." 在 ToData（`staticImgDir` 为空、`resourceWriter` 非空）下不再成立，补充说明 ToData 模式由 `resourceWriter` 承担资源输出。

- [ ] **Step 2: 在 `HTMLWriter.h` inline 实现 `writeResource`**

```cpp
bool HTMLWriterContext::writeResource(const std::string& relativePath, const void* bytes,
                                      size_t size, std::string* errorMsg) {
  if (resourceWriter != nullptr) {
    // ToData: the archive entry name must equal the URL the HTML references
    // ("assets/img0.png", "assets/fonts/font_f0.woff2" ...), so prepend
    // staticImgUrlPrefix ("assets/"). ToHTML never reaches here (resourceWriter
    // is null) and keeps writing to staticImgDir unchanged.
    return resourceWriter->write(staticImgUrlPrefix + relativePath, bytes, size, errorMsg);
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

（`HTMLWriter.h` 头部需新增 `<filesystem>`/`<fstream>` include。`HTMLResourceWriter` 为指针成员，Step 1 前置声明即可，但 inline 的 `writeResource` 需调用其 `write()`，故 Step 1 改为 include `pagx/html/HTMLResourceWriter.h`。）

- [ ] **Step 3: 构建验证**

Run: `./codeformat.sh 2>/dev/null; true && cmake --build cmake-build-debug --target PAGFullTest`
Expected: 编译通过；现有测试无行为变化（本任务只新增字段和方法，尚未被调用）

- [ ] **Step 4: Commit**

```bash
git add src/pagx/html/HTMLWriter.h
git commit -m "Inject resource writer into HTMLWriterContext with writeResource fallback."
```

---

### Task 5: 渲染函数改为返回内存 PNG bytes，HTMLWriterShape 3 处落地点走 writeResource

**Files:**
- Modify: `src/pagx/html/HTMLStaticImageRenderer.h`（5 个函数签名）
- Modify: `src/pagx/html/HTMLStaticImageRenderer.cpp`（`WriteSurfaceAsPng`→`EncodeSurfaceAsPng`、`RenderTileToPng`、5 个 Render 函数）
- Modify: `src/pagx/html/HTMLWriterShape.cpp:2375-2394`（Diamond 落地点）、`:2428-2440`（Conic）、`:2532-2551`（ImagePattern）、`:2328-2329`（Conic 调度条件）

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

**同时修改 Conic 的调度条件（HTMLWriterShape.cpp:2328-2329）**：当前调度分支是

```cpp
  if (fill && fill->color && fill->color->nodeType() == NodeType::ConicGradient &&
      !_ctx->staticImgDir.empty() && !stroke && !hasTrim) {
```

其中 `!_ctx->staticImgDir.empty()` 改为 `_ctx->hasResourceOutput()`，并同步更新该分支的注释。否则 ToData 下 `staticImgDir` 为空，Conic 永远进不了 `renderConicCanvas`，退化为 headless Chromium 下不可靠的 CSS conic-gradient 路径（见 :2330 注释），与 ToHTML 视觉不一致。（Diamond/ImagePattern 的调度点 :2311/:2316 不检查 `staticImgDir`，仅改函数入口守卫即可。）

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

`HTMLPlusDarkerRenderer.h:63` 的 `RenderAll(const PAGXDocument& doc, const std::string& staticImgDir, const std::string& urlPrefix, float rasterScale, std::unordered_map<const Layer*, PlusDarkerBackdrop>& out)` 改为 `RenderAll(const PAGXDocument& doc, HTMLWriterContext* ctx, std::unordered_map<const Layer*, PlusDarkerBackdrop>& out)`。`staticImgDir`/`rasterScale` 从 `ctx` 取（`ctx->staticImgDir`、`ctx->rasterScale`）；`urlPrefix` 参数已不被使用（`backdropDataURL` 是 base64，不依赖前缀），直接删除。

`HTMLPlusDarkerRenderer.h` **只能**前置声明 `class HTMLWriterContext;`——`HTMLWriter.h:28` 已 include 本头文件，再 include `HTMLWriter.h` 会构成循环依赖。

注意 `rasterScale` 语义：`ctx->rasterScale` 是 clamp 后的值（`HTMLExporter` 组装 ctx 时 `std::clamp(options.rasterScale, 0.01f, 4.0f)`），而现有 ToHTML 调用点传原始 `options.rasterScale`（HTMLExporter.cpp:192）。差异仅出现在选项超范围时；`HTMLExportOptions::rasterScale` 的契约（include/pagx/HTMLExporter.h:52）本身声明超范围会被 clamp，视为修正现有实现与文档契约的不一致，默认值 2.0 及正常输入下逐字节不变。

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
      if (!ctx->writeResource(fileName, encoded->bytes(), encoded->size(), &error)) {
        // Match the legacy behavior: a failed backdrop write drops this entry so
        // the caller falls back to the mix-blend-mode approximation.
        continue;
      }
    }
```

（`RenderCroppedBackdrop` 去掉 `outputPath` 参数，删除内部 `std::ofstream` 写盘段 line 178-186，只保留 `*outEncoded = encoded; return true;`。原 `fileName = "pd_" + std::to_string(idx) + ".png";` 与 `absPath` 拼接逻辑保留 fileName 部分、删除 absPath。`HTMLPlusDarkerRenderer.cpp` 头部需新增 `#include "pagx/html/HTMLWriter.h"`——当前只 include 自己的头（:19），访问 `ctx->staticImgDir`/`ctx->rasterScale`/`ctx->resourceWriter` 需要 `HTMLWriterContext` 完整定义。）

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

ToData 分支对 `hash:`/`http(s)` scheme 的 `filePath` 直接 `tgfx::Data::MakeFromFile`（无 `IsSafeImageUrl` 检查，读取失败即降级空 src）；与 ToHTML 下 `EscapeCSSUrl` 直引外部 URL 的行为不同——这是设计文档第 8 节的有意设计（ToData 是自闭合产物，ZIP 内不能引用外部 URL）。

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

（`urlPrefix` 局部变量在 `ctx.staticImgUrlPrefix` 中已存在，字体段不再直接引用 `urlPrefix`/`resourceDir`。`writeResource` 失败时 `continue` 丢弃该字体，与旧代码（文件写失败仍追加 `fontFaceRules` 并注册 `woff2Fonts`，HTML 可能引用不存在的文件）的差异仅存在于写盘失败路径；正常路径逐字节不变，且 ToData 下必须跳过未写入 ZIP entry 的字体，否则 HTML 会引用不存在的 entry。）

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
  // Only entry *names* appear verbatim in the buffer (in local file headers / central
  // directory); entry *content* is DEFLATE-compressed and cannot be searched as plain
  // text. Asserting on "<!DOCTYPE html>" here would fail by construction.
  EXPECT_NE(bytes.find("index.html"), std::string::npos);
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
Expected: 仅 `src/pagx/utils/MemZip.*`、`src/pagx/html/HTMLResourceWriter.*`、`src/pagx/html/HTMLWriter.h`、`src/pagx/html/HTMLWriterShape.cpp`、`src/pagx/html/HTMLWriterUtils.cpp`、`src/pagx/html/HTMLPlusDarkerRenderer.{h,cpp}`、`src/pagx/html/HTMLStaticImageRenderer.{h,cpp}`、`src/pagx/html/HTMLExporter.cpp`、`src/pagx/ppt/PPTExporter.cpp`、`include/pagx/HTMLExporter.h`、`CMakeLists.txt`、`pagx/wechat/CMakeLists.txt`、`test/src/PAGXHTMLDataTest.cpp` 的改动，无其他文件

- [ ] **Step 3: 确认设计文档待办项**

对照 `docs/superpowers/specs/2026-08-04-html-exporter-todata-design.md` 第 15 节实施步骤逐条打勾。若实现中发现设计文档与代码不符之处，先回查设计文档再决定是否更新。

---

## 自审记录（Self-Review）

- **Spec coverage**：设计文档 15 节实施步骤 1-9 对应 Task 1-11：MemZip 抽取（T1）、CMake（T2）、HTMLResourceWriter（T3）、context 注入（T4）、渲染函数+Shape 落地点（T5）、PlusDarker（T6）、外部图片（T7）、BuildHTML（T8）、ToData（T9）、测试（T10）、回归（T11）。第 5.2 节 entry 名由 nextId 生成的安全论证 → T5/T7 实现中遵循（不引入外部字符串进 entry 名）。第 9 节 ZIP 布局 → T9 的 `assets/` 前缀 + T5 扁平文件名 + T7 `img{N}.{ext}`。第 8 节错误语义 → T9 的 nullptr + T7 的降级空 src。
- **Placeholder scan**：无 TBD/TODO；每个任务均有实际代码或命令。Task 10 的"其余用例"按 PAGXPPTTest 已有模式展开（实施时按现有测试文件的具体 helper 拼装，此为测试构造细节，非占位符）。
- **Type consistency**：`HTMLResourceWriter::write` 签名（T3）与 `HTMLWriterContext::writeResource`（T4）、`HTMLZipResourceWriter::finish`（T3）、`ToData` 返回 `std::shared_ptr<Data>`（T9）跨任务一致；`RenderAll` 新签名（T6）与 `BuildHTML` 调用（T8）一致；`GetImageBytes`/`MimeToExt`（T7）在 `HTMLWriterUtils.cpp` 文件内定义。
- **CMake 依赖边界（B2 决策）**：minizip 源码、minizip include、zlib include 全部挂载到 `PAG_BUILD_PAGX`（HTMLExporter 源码与 `src/pagx/utils/*` 均随 PAGX 编译，PAGX 是 minizip 依赖的实际锚点；`PAG_BUILD_HTML`/`PAG_BUILD_PPT`/`PAG_BUILD_CLI`/`PAG_BUILD_TESTS` 均强制 PAGX，条件天然覆盖全部场景）；`PAG_BUILD_PPT` 仅保留 PPT 专用源码与编译定义。`pagx/wechat` 是独立 CMake 项目（不读主 CMakeLists，编译宏仅 `PAG_BUILD_PAGX` + `PAG_USE_HARFBUZZ`），其 utils glob 自动编译 MemZip.cpp 但无 minizip 资源，已在其中加 FILTER 排除（仿 `Woff2FontGenerator.cpp` 先例）。**与设计文档第 10 节（`PPT` → `HTML OR PPT`）偏离，以本计划为准；实施时同步更新设计文档第 10 节。**
- **审查发现修正（A/B/C 系列）**：
  - A1 ZIP entry 名与 HTML URL 一致性：`writeResource` 的 ZIP 分支在 entry 名前加 `staticImgUrlPrefix`（ToData = "assets/"），保证 `assets/img0.png`/`assets/fonts/font_f0.woff2` 等 entry 与 HTML 引用逐字节对应；ToHTML 下 `resourceWriter` 为 null 不受影响（T4）。与设计文档第 9 节布局一致，修正了设计文档 7.1/5.3 代码中缺失前缀的不一致。
  - A2 Conic 调度守卫：`HTMLWriterShape.cpp:2328-2329` 的 `!_ctx->staticImgDir.empty()` 改为 `_ctx->hasResourceOutput()`，否则 ToData 下 Conic 永不进入栅格化分支（T5 Step 4）。
  - A3 测试断言：ZIP entry 内容经 deflate 压缩不可明文搜索，删除 `<!DOCTYPE html>` 明文断言，仅断言 entry 名（T10）。**与设计文档 11.1 的"含 DOCTYPE"表述偏离，以本计划为准。**
  - B3 `HTMLPlusDarkerRenderer.h` 仅前置声明 `HTMLWriterContext`（`HTMLWriter.h:28` 已 include 它，include 会循环）；`HTMLPlusDarkerRenderer.cpp` 需新增 include `HTMLWriter.h`（T6）。
  - C1 PlusDarker 写盘失败时 `continue`（保持旧语义：失败跳过该 backdrop，回退 mix-blend-mode）。
  - C2 PlusDarker 的 `rasterScale` 改用 clamp 后的 `ctx->rasterScale`，超范围输入下与旧实现不同；options 契约本身声明 clamp，视为修正，正常输入逐字节不变。
  - C3 字体写失败时 `continue`，丢弃未写入的资源；与旧失败路径（HTML 引用可能不存在的字体文件）的差异仅存在于写盘失败时。
  - C4 更新 `staticImgDir` 注释，ToData 下 "both fields are non-empty" 假设不成立。
  - C5 ToData 分支对 `hash:`/`http(s)` filePath 降级空 src 为有意设计（设计文档第 8 节），与 ToHTML 的 `EscapeCSSUrl` 直引不同。
