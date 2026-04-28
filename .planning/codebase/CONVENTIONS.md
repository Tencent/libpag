# Coding Conventions

**Analysis Date:** 2025-05-27

## Naming Conventions

**Files:**
- Source files: `PascalCase.cpp` / `PascalCase.h` (e.g., `PAGPlayer.cpp`, `RenderCache.h`)
- Test files: `PascalCaseTest.cpp` (e.g., `PAGFileTest.cpp`, `PAGFilterTest.cpp`)
- Utility/helper files: `camelCase.cpp` / `camelCase.h` in `utils/` subdirectories
- PAGX type files in `include/pagx/types/`: `PascalCase.h` per type (e.g., `ScaleMode.h`, `TileMode.h`)

**Classes and Structs:**
- `PascalCase` — all class and struct names (e.g., `PAGPlayer`, `RenderCache`, `PAGAnimator`)
- Prefix `PAG` for public-facing runtime classes, prefix `PAGX` for XML-format classes

**Functions and Methods:**
- Global functions and static class methods: `PascalCase` (e.g., `LoadPAGFile()`, `PAGImage::FromPath()`)
- Member methods: `camelCase` (e.g., `pagPlayer->setSurface()`, `pagFile->setCurrentTime()`)
- Getters do not use `get` prefix when name is self-evident: `width()`, `height()`, `duration()`

**Variables:**
- Local variables: `camelCase` (e.g., `pagFile`, `pagSurface`, `textLayer`)
- Member variables: `camelCase` (e.g., `baselineVersionPath`, `currentVersion`)
- Static constants: `ALL_CAPS_UNDERSCORE` (e.g., `OUT_ROOT`, `PAG_COMPLEX_FILE_PATH`)

**Macros:**
- `ALL_CAPS_UNDERSCORE` (e.g., `PAG_TEST`, `PAG_SETUP`, `PAG_API`, `GL_VER`)
- No `k` prefix on constants — use `ALL_CAPS_UNDERSCORE` instead

**Enums:**
- Enum type: `PascalCase`; enum values: `PascalCase` (e.g., `LayerType::PreCompose`, `EncodedFormat::WEBP`)

## Code Style

**Formatter:** clang-format, Google style base, configured in `.clang-format`
- Run before every build: `./codeformat.sh 2>/dev/null; true`
- Column limit: 100 characters
- Indent: 2 spaces, no tabs
- Pointer alignment: left (`int* ptr`)
- Brace style: same-line open brace (K&R variant)
- Max empty lines: 1

**Language Standard:** C++17

**Casting:**
- Use `static_cast<T>()` and `reinterpret_cast<T>()` as needed
- `dynamic_cast` is **forbidden**
- C-style casts are **forbidden**
- Prefer `std::static_pointer_cast<T>()` for shared_ptr downcasts

**Error Handling:**
- C++ exceptions (`throw`/`try`/`catch`) are **forbidden**
- Return `nullptr`, empty container, or `false` to signal failure
- Callers check return values; no exception propagation

**Lambda Expressions:**
- Avoid in production code — use explicit named methods or free functions instead
- Short lambdas are allowed inline by clang-format config but should not appear in new code

**Mutable Members:**
- Avoid `mutable` member variables
- If state must be modified in a conceptually-const context, prefer making the method non-const

**Smart Pointers:**
- Use `std::shared_ptr<T>` for shared ownership
- Use `std::unique_ptr<T>` for exclusive ownership
- Factory functions return `std::shared_ptr<T>` (e.g., `PAGFile::Load()`, `PAGImage::FromPath()`)

## File Organization

**Header Guards:** `#pragma once` in all headers (no traditional include guards)

**Namespace:** All code lives in namespace `pag`; TGFX types in `tgfx`. Test files use `using namespace tgfx;` locally.

**Include Order (clang-format merges and sorts includes alphabetically within a block):**
1. Own module header (for `.cpp` files)
2. Internal headers (project-relative paths, no angle brackets)
3. Third-party headers
4. Standard library headers

Example from `PAGPlayer.cpp`:
```cpp
#include "base/utils/TGFXCast.h"
#include "base/utils/TimeUtil.h"
#include "pag/file.h"
#include "rendering/FileReporter.h"
#include "rendering/caches/RenderCache.h"
#include "tgfx/core/Clock.h"
```

**Directory Structure:**
- `include/pag/` — public API headers (detailed doc comments required)
- `include/pagx/` — PAGX format public API headers
- `src/base/` — data model (no rendering dependencies)
- `src/rendering/` — rendering pipeline
- `src/codec/` — binary format codec
- `src/pagx/` — XML format implementation
- `src/platform/{platform}/` — platform-specific implementations
- `src/cli/` — CLI tool
- `test/src/` — test cases
- `test/src/base/` — test framework infrastructure
- `test/src/utils/` — test helpers

## Patterns to Avoid

| Pattern | Reason |
|---------|--------|
| `dynamic_cast` | Forbidden — causes RTTI overhead and fragile downcasts |
| `throw` / `try` / `catch` | Forbidden — no exception support in codebase |
| Lambda expressions | Forbidden in production code — reduces readability |
| `mutable` member variables | Avoid — prefer non-const method instead |
| C-style casts `(Type)value` | Use `static_cast<Type>(value)` |
| Backward-compatibility shims | Remove all affected code paths when changing API |
| `k` prefix on constants | Use `ALL_CAPS_UNDERSCORE` instead |

## Comment Standards

**File Headers:** Every `.cpp` and `.h` file begins with the Tencent Apache 2.0 license banner (97 slashes wide). Copyright year for new files must be the current year (e.g., `Copyright (C) 2026 Tencent`); do not change the year in existing files.

**Public API Headers (`include/`):** All public methods must have detailed doc comments including parameter descriptions:
```cpp
/**
 * Creates a PAGImage object from an array of pixel data, return null if it's not valid pixels.
 * @param pixels The pixel data to copy from.
 * @param width The width of the pixel data.
 * @param height The height of the pixel data.
 * @param rowBytes The number of bytes between subsequent rows of the pixel data.
 * @param colorType Describes how to interpret the components of a pixel.
 * @param alphaType Describes how to interpret the alpha component of a pixel.
 */
static std::shared_ptr<PAGImage> FromPixels(const void* pixels, ...);
```

**Other Public Methods (non-private, non-public-API):** One-sentence description of main purpose:
```cpp
/**
 * PAGAnimator provides a simple timing engine for running animations.
 */
class PAGAnimator { ... };
```

**Private Methods:** No comments.

**Inline Code Comments:** No line-by-line comments inside function bodies. Only add comments to explain:
- Non-obvious algorithm choices
- Special boundary conditions
- Workarounds for known bugs (include the reason)

Example of acceptable inline comment:
```cpp
// TODO(kevingpqi): We temporarily disable texture generation through NativeBuffer, as enabling
// asynchronous hardware... [reason for workaround]
```

**Test Case Comments:** Test cases have a Chinese description comment directly above the `PAG_TEST` macro:
```cpp
/**
 * 用例描述: PAGFile基础信息获取
 */
PAG_TEST(PAGFileTest, TestPAGFileBase) { ... }
```

---

*Convention analysis: 2025-05-27*
