# libpag D3D12 后端适配设计方案

## 版本信息

- 文档版本：v1.0
- 日期：2026-11-17
- 基线：`main` @ `9c968baea`
- 分支：`feature/thunderllei_d3d12`
- 状态：**待评审**
- 参考实现：Metal 适配提交 `ba9952640`（#3717）、`c0b63f058`（#3781）、`8a63822b0`（#3798）、`c52be7304`（#3804）、`e0815e9ca`（#3824）、`eb4412aa5`（#3828）

---

## 一、背景与目标

### 1.1 现状

- tgfx（`third_party/tgfx`，锁定 `a3349d9a5`）已经具备完整的 D3D12 后端：设备创建、外部纹理导入、Win32 交换链窗口、WARP 软件光栅化，并有独立 CI lane `win-d3d12-warp`。
- libpag 在 `main` 上已经完成 Metal / WebGPU 后端的适配，后端胶水集中在 `src/rendering/gpu/{opengl,metal,webgpu}/`，公共代码只依赖 `tgfx::Device` 基类。
- libpag 的 D3D12 仍然只有开关和占位：`CMakeLists.txt` 在 `PAG_IMPLEMENTED_GPU_BACKENDS`（第 192 行）处拒绝 D3D12；公共 `pag::Backend` 枚举没有 `D3D12`；没有后端胶水、纹理转换和平台接入；测试目标只允许 OpenGL / Metal（第 1207 行）。

### 1.2 目标

完全对照 Metal 的落地方式，把 D3D12 接入到与 Metal 同等的完成度：

1. 公共 API（C++ 与 C）支持 D3D12 外部纹理与渲染目标。
2. 新增 D3D12 后端胶水，实现 `pag::Devices` 的全部接口。
3. Windows 平台层（`GPUDrawable`）支持基于 `tgfx::D3D12Window` 的窗口渲染。
4. 构建系统支持 `PAG_USE_D3D12`，并提供 WARP 模式用于无 GPU 的 CI/基线环境。
5. 补齐 Windows 测试基建与 D3D12 测试用例，接入基线体系。
6. 补齐脚本、CI lane 与文档。

### 1.3 非目标

- 不引入 Vulkan 后端。
- 不扩展 `pag::BackendSemaphore` 的 D3D12 信号量 ABI（Metal 在 `main` 上也未开放，见 §9.3）。
- 不修改 `third_party/tgfx`。如果实施过程中发现必须新增 tgfx 公开接口（例如 `ID3D12Device` 取值器），单独立项升级 tgfx，不夹带在本次改动中。
- 不提供 Windows 硬件解码器路径（Metal 侧对应的是 Apple `HardwareDecoder` 的 Metal 分支，Windows 无同类模块）。

---

## 二、现状盘点

### 2.1 tgfx 侧能力（`third_party/tgfx/include/tgfx/gpu/d3d12/`）

| 能力 | 接口 | 说明 |
|---|---|---|
| 硬件设备 | `D3D12Device::Make()` | 遍历 DXGI 适配器，跳过 `DXGI_ADAPTER_FLAG_SOFTWARE`；无可用硬件时返回 nullptr |
| WARP 设备 | `D3D12Device::MakeWarp()` | CPU 光栅化，功能完整（FL12_1）、性能很低，用于 CI 与无 GPU 环境 |
| 已有设备 | `D3D12Device::MakeFrom(void* device)` | 入参为 `ID3D12Device*`，内部 `QueryInterface` 并持有自己的引用 |
| 外部纹理 | `tgfx::D3D12TextureInfo { const void* resource; unsigned format; }` | `resource` 为 `ID3D12Resource*`，`format` 为 `DXGI_FORMAT` |
| 外部同步 | `tgfx::D3D12SyncInfo { const void* fence; uint64_t value; }` | 本次不接入 |
| 窗口 | `D3D12Window::MakeForHwnd / MakeForComposition` | 内部 `IDXGISwapChain3`，按 `GetClientRect` 尺寸重建 backbuffer |
| 测试模式 | 编译宏 `TGFX_D3D12_USE_WARP` | tgfx 自己的 CI 用它把测试固定到 WARP |

两个关键约束：

1. **tgfx 公共 `D3D12Device` 没有暴露 `ID3D12Device` 的取值接口**，libpag 只能从 `ID3D12Resource::GetDevice()` 反查原生设备，无法把渲染 context 的设备与外部资源做同一性比较。
2. **tgfx 不对 D3D12 设备做去重**：`D3D12Device::MakeFrom` 不调用 `Device::RegisterNative`，`DeviceKey`（`src/gpu/DeviceRegistry.h`）只有 Metal 分支。同一个 `ID3D12Device` 上可能同时存在多个 `tgfx::Device` 实例（各自的命令队列与缓存，但资源可互通）。

### 2.2 libpag 侧缺口

| 编号 | 位置 | 缺口 |
|---|---|---|
| G1 | `CMakeLists.txt:192` | `PAG_IMPLEMENTED_GPU_BACKENDS` 不含 D3D12，配置阶段直接失败 |
| G2 | `CMakeLists.txt:1207` | 测试目标只允许 OpenGL / Metal |
| G3 | `include/pag/gpu.h` | `pag::Backend` 无 `D3D12`；无 `D3D12TextureInfo`；`BackendTexture` / `BackendRenderTarget` 无 D3D12 构造与取值接口 |
| G4 | `include/pag/c/`、`src/c/pag_backend_texture.cpp` | C API 无 D3D12 纹理结构体与创建/取值函数 |
| G5 | `src/rendering/GPUBackend.cpp` | `BackendTexture` / `BackendRenderTarget` 的构造、拷贝、取值无 D3D12 分支 |
| G6 | `src/base/utils/TGFXCast.cpp` | `ToTGFX` / `ToPAG` 无 D3D12 分支 |
| G7 | `src/rendering/gpu/` | 没有 `d3d12/` 目录与 `Devices` 实现 |
| G8 | `src/base/utils/cast/` | 没有 D3D12 纹理格式查询（Metal 对应 `cast/MetalTexture.*`） |
| G9 | `src/platform/win/GPUDrawable.*` | 只支持 WGL / ANGLE 窗口，无 `D3D12Window` 路径 |
| G10 | `CMakeLists.txt` WIN32 分支 | `USE_NATIVE_PLATFORM` 下无条件链接 `opengl32`（或 ANGLE），D3D12 构建需要改为 D3D12 依赖 |
| G11 | `test/src/utils/Baseline.cpp:25` | MD5 依赖 `CommonCrypto`（Apple-only），Windows 无法编译 |
| G12 | `test/src/base/TestEnvironment.cpp` | 非 Emscripten 构建无条件注册 ffavc，但 `vendor/ffavc` 只有 `mac` 产物，Windows 链接失败 |
| G13 | `CMakeLists.txt:1273` | 测试依赖 `-fno-access-control`（GCC/Clang 选项），MSVC 不支持 |
| G14 | `autotest.sh` / `update_baseline.sh` / `accept_baseline.sh` | 只支持 OpenGL / OpenGL SwiftShader / Metal / Web 后端 |
| G15 | `.github/workflows/` | Metal 有完整 autotest lane，D3D12 只有 Windows 编译任务且未启用 D3D12 |
| G16 | 文档 | `win/` 无 README；`.codebuddy/rules/Test.md` 的后端说明只覆盖 OpenGL 与 Metal |

### 2.3 Metal 参照实现清单

| Metal 文件 | 作用 | D3D12 对应 |
|---|---|---|
| `src/rendering/gpu/metal/Devices.mm` | 后端胶水 | `src/rendering/gpu/d3d12/Devices.cpp` |
| `src/base/utils/cast/MetalTexture.{h,cpp}` + `cast/metal/MetalTexture.mm` | 纹理格式查询（桩 + 真实现） | `cast/D3D12Texture.{h,cpp}` + `cast/d3d12/D3D12Texture.cpp` |
| `src/base/utils/TGFXCast.cpp` 的 METAL 分支 | 公共类型与 tgfx 类型互转 | D3D12 分支 |
| `src/rendering/GPUBackend.cpp` 的 METAL 分支 | 公共资源包装 | D3D12 分支 |
| `src/platform/cocoa/private/metal/*`、`PAGSurface+Metal.h` | 平台窗口/图层接入 | `src/platform/win/GPUDrawable` |
| `test/src/PAGMetal*Test.mm`、`PAGBlendMetalTest` | 后端用例 | `PAGD3D12*Test.cpp`、`PAGBlendD3D12Test` |
| `.codebuddy/rules/Test.md`、脚本、CI | 后端命名与缓存隔离 | `d3d12` / `d3d12-warp` |

---

## 三、总体设计

### 3.1 分层

```text
公共 API (include/pag)
        │  pag::BackendTexture / BackendRenderTarget / BackendSemaphore
        ▼
渲染核心 (src/rendering)
        │  PAGSurface / PAGImage / PAGDecoder 只依赖 tgfx::Device
        ▼
后端胶水 (src/rendering/gpu/d3d12)
        │  Devices::MakeDefault / MakeForTexture / ResolveForExternalResource ...
        ▼
纹理转换 (src/base/utils/TGFXCast.cpp + cast/d3d12)
        │  pag::D3D12TextureInfo <-> tgfx::D3D12TextureInfo
        ▼
平台层 (src/platform/win)
        │  GPUDrawable -> tgfx::D3D12Window
        ▼
tgfx D3D12 (third_party/tgfx)
```

### 3.2 后端选择原则

- 后端在编译期唯一确定：`PAG_USE_D3D12` 与 `PAG_USE_OPENGL` / `PAG_USE_METAL` / `PAG_USE_VULKAN` / `PAG_USE_WEBGPU` 互斥，CMake 已经把非 GL 后端统一处理（`CMakeLists.txt:111-121`）。
- `PAG_USE_${PAG_GPU_BACKEND}` 已经生成 `TGFX_USE_D3D12` 宏（`CMakeLists.txt:204`），公共代码用 `TGFX_USE_D3D12` 区分平台路径。
- 新增 `PAG_D3D12_USE_WARP` 编译选项，透传 `PAG_D3D12_USE_WARP` 宏，只影响默认设备选择与测试后端命名。

### 3.3 关键不变量

1. D3D12 构建中 `Devices::MakeDefault()` 返回的实例一定是 `tgfx::D3D12Device`（平台层需要 `static_pointer_cast` 后交给 `D3D12Window`）。
2. `Devices::MakeForTexture()` 必须从调用方 `ID3D12Resource` 反查 `ID3D12Device`，再用 `D3D12Device::MakeFrom()` 包装；不能直接用 `MakeDefault()`，否则可能落在另一个适配器上。
3. 不引入 thread-local "current device" 概念：`AdoptCurrent()` 返回空。
4. 外部资源与渲染 context 的设备同一性不做强制校验（`CaptureFromTexture()` 返回 nullptr），退化为信任调用方，与 WebGPU 一致。

---

## 四、接口设计

### 4.1 公共 C++ API（`include/pag/gpu.h`）

```cpp
enum class Backend {
  MOCK,
  OPENGL,
  METAL,
  VULKAN,
  WEBGPU,
  D3D12,  // 追加在末尾，已有枚举值保持不变
};

/**
 * Types for interacting with D3D12 resources created externally to PAG. Holds the
 * ID3D12Resource as a void*.
 */
struct D3D12TextureInfo {
  /**
   * Pointer to an ID3D12Resource representing a texture.
   */
  void* resource = nullptr;
};

class PAG_API BackendTexture {
 public:
  BackendTexture(const D3D12TextureInfo& d3d12Info, int width, int height);
  bool getD3D12TextureInfo(D3D12TextureInfo* d3d12TextureInfo) const;

 private:
  union {
    // ...
    D3D12TextureInfo d3d12Info;
  };
};

class PAG_API BackendRenderTarget {
 public:
  BackendRenderTarget(const D3D12TextureInfo& d3d12Info, int width, int height);
  bool getD3D12TextureInfo(D3D12TextureInfo* d3d12TextureInfo) const;
};
```

设计取舍：

- 与 `MtlTextureInfo` 一致，只保存原生资源指针；DXGI 格式由 libpag 内部通过 `ID3D12Resource::GetDesc().Format` 查询，调用方不需要知道 `DXGI_FORMAT`。
- 不加 `D3D12SyncInfo` / `BackendSemaphore::initD3D12`，理由见 §9.3。

### 4.2 公共 C API

```c
// include/pag/c/pag_types.h
typedef struct pag_d3d12_texture_info {
  void* resource = nullptr;
} pag_d3d12_texture_info;

// include/pag/c/pag_backend_texture.h
PAG_EXPORT pag_backend_texture* pag_backend_texture_create_from_d3d12_texture_info(
    pag_d3d12_texture_info textureInfo, int width, int height);

PAG_EXPORT bool pag_backend_texture_get_d3d12_texture_info(
    pag_backend_texture* texture, pag_d3d12_texture_info* textureInfo);
```

实现放在 `src/c/pag_backend_texture.cpp`，与 Metal 的 `create_from_mtl_texture_info` 对称。

### 4.3 内部纹理格式查询

```cpp
// src/base/utils/cast/D3D12Texture.h
namespace pag {
/**
 * Returns the DXGI format of the given ID3D12Resource, or zero when the resource is null or the
 * selected backend is not D3D12.
 */
unsigned GetD3D12TexturePixelFormat(const void* resource);
}
```

- `cast/D3D12Texture.cpp`：非 D3D12 构建的桩，始终返回 0。
- `cast/d3d12/D3D12Texture.cpp`：`static_cast<const ID3D12Resource*>(resource)->GetDesc().Format`。
- CMake 在 `PAG_USE_D3D12` 时排除桩文件，与 Metal 的 `cast/MetalTexture.cpp` 处理方式一致。

### 4.4 后端胶水（`src/rendering/gpu/d3d12/Devices.cpp`）

```cpp
Devices::MakeDefault():
#ifdef PAG_D3D12_USE_WARP
  return D3D12Device::MakeWarp();
#else
  auto device = D3D12Device::Make();
  if (device == nullptr) {
    LOGW("Devices::MakeDefault() No D3D12 hardware adapter, falling back to WARP.");
    device = D3D12Device::MakeWarp();
  }
  return device;
#endif

Devices::SupportsBackend(backend):        return backend == tgfx::Backend::D3D12;

Devices::MakeForAsyncThread():            return MakeDefault();

Devices::MakeForSharedContext(void*):     return MakeDefault();

Devices::AdoptCurrent():                  return {};

Devices::MakeForAsyncThread(texture):
  auto device = MakeForTexture(texture);
  return device != nullptr ? device : MakeDefault();

Devices::MakeForTexture(texture):
  tgfx::D3D12TextureInfo info = {};
  if (!texture.getD3D12TextureInfo(&info)) return nullptr;
  return MakeDeviceFromResource(info.resource);

Devices::MakeForTexture(renderTarget):
  tgfx::D3D12TextureInfo info = {};
  if (!renderTarget.getD3D12TextureInfo(&info)) return nullptr;
  return MakeDeviceFromResource(info.resource);

Devices::CaptureFromTexture(texture):     return nullptr;

Devices::RequiresCapturedIdentity():      return false;

Devices::CanSampleFrom(context, ref):     return context != nullptr;
```

`MakeDeviceFromResource`（文件内静态函数）：

```cpp
static std::shared_ptr<tgfx::Device> MakeDeviceFromResource(const void* resource) {
  if (resource == nullptr) {
    return nullptr;
  }
  ID3D12Device* d3d12Device = nullptr;
  auto hr = static_cast<ID3D12Resource*>(const_cast<void*>(resource))
                ->GetDevice(IID_PPV_ARGS(&d3d12Device));
  if (FAILED(hr) || d3d12Device == nullptr) {
    return nullptr;
  }
  // D3D12Device::MakeFrom() QueryInterfaces and retains its own reference.
  auto device = tgfx::D3D12Device::MakeFrom(d3d12Device);
  d3d12Device->Release();
  return device;
}
```

### 4.5 Windows 平台接口

`src/platform/win/GPUDrawable` 保持现有“内部 Drawable”形态（Windows 没有等价的公开 `PAGSurface` category）：

```cpp
std::shared_ptr<tgfx::Device> GPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) return nullptr;
  if (window == nullptr) {
#if defined(PAG_USE_D3D12)
    auto device = std::static_pointer_cast<tgfx::D3D12Device>(Devices::MakeDefault());
    if (device == nullptr) return nullptr;
    window = tgfx::D3D12Window::MakeForHwnd(static_cast<HWND>(nativeWindow), device, nullptr, true);
#elif defined(PAG_USE_ANGLE)
    window = tgfx::EGLWindow::MakeFrom(...);
#else
    window = tgfx::WGLWindow::MakeFrom(...);
#endif
  }
  return window ? window->getDevice() : nullptr;
}
```

成员类型从 `std::shared_ptr<tgfx::WGLWindow>` / `std::shared_ptr<tgfx::EGLWindow>` 统一为 `std::shared_ptr<tgfx::Window>`。`present()` 保持为空（tgfx 在提交命令后自动 present）。

### 4.6 构建选项

| 选项 | 默认 | 作用 |
|---|---|---|
| `PAG_USE_D3D12` | OFF | 选择 D3D12 为唯一 GPU 后端，仅允许 Windows |
| `PAG_D3D12_USE_WARP` | OFF | 默认设备固定为 WARP；测试后端名变为 `d3d12-warp` |

---

## 五、实现细节

### 5.1 文件变更清单

#### 阶段 A：核心库

| 文件 | 类型 | 说明 |
|---|---|---|
| `include/pag/gpu.h` | 修改 | `Backend::D3D12`、`D3D12TextureInfo`、`BackendTexture` / `BackendRenderTarget` 接口与注释 |
| `include/pag/c/pag_types.h` | 修改 | `pag_d3d12_texture_info` |
| `include/pag/c/pag_backend_texture.h` | 修改 | 创建/取值声明 |
| `src/c/pag_backend_texture.cpp` | 修改 | 创建/取值实现 |
| `src/rendering/GPUBackend.cpp` | 修改 | 构造、赋值、union 拷贝、取值 |
| `src/base/utils/TGFXCast.cpp` | 修改 | `ToTGFX` / `ToPAG` 的 D3D12 分支 |
| `src/base/utils/cast/D3D12Texture.h` | 新增 | 格式查询声明 |
| `src/base/utils/cast/D3D12Texture.cpp` | 新增 | 非 D3D12 构建的桩 |
| `src/base/utils/cast/d3d12/D3D12Texture.cpp` | 新增 | 真实格式查询 |
| `src/rendering/gpu/d3d12/Devices.cpp` | 新增 | `pag::Devices` 全部实现 |
| `src/rendering/gpu/Devices.h` | 修改 | 删除 “D3D12 not implemented” 注释，补充 D3D12 行为说明 |
| `CMakeLists.txt` | 修改 | 后端白名单、平台校验、Windows 依赖、桩文件过滤、WARP 选项 |

#### 阶段 B：Windows 窗口渲染

| 文件 | 类型 | 说明 |
|---|---|---|
| `src/platform/win/GPUDrawable.h` | 修改 | 成员统一为 `std::shared_ptr<tgfx::Window>` |
| `src/platform/win/GPUDrawable.cpp` | 修改 | D3D12Window 创建；D3D12 分支尺寸用 `GetClientRect` |

#### 阶段 C：测试基建与用例

| 文件 | 类型 | 说明 |
|---|---|---|
| `CMakeLists.txt` | 修改 | `PAG_BACKEND_NAME` / `TEST_TARGET_SUFFIX` 映射 `d3d12`/`d3d12-warp`；源码过滤 |
| `test/src/utils/Baseline.cpp` | 修改 | 统一使用 `tgfx::MD5` |
| `test/src/base/TestEnvironment.cpp` | 修改 | Windows 不注册 ffavc |
| `test/src/VideoSequenceFallbackTest.cpp` | 修改 | 按 `PAG_USE_FFAVC` / 非 Windows 跳过 |
| `test/src/PAGANRRegressionTest.cpp` | 修改 | 同上 |
| `test/src/PAGD3D12SurfaceTest.cpp` | 新增 | 外部纹理/渲染目标、ImageOrigin、回读 |
| `test/src/PAGD3D12TextureTest.cpp` | 新增 | `PAGImage::FromTexture`、异步线程、格式变体 |
| `test/src/PAGBlendTest.cpp` | 修改 | D3D12 blend 用例 |
| `test/src/PAGSurfaceTest.cpp` | 修改 | D3D12 用例或 Gating |
| `test/src/PAGXRuntimeTest.cpp` | 修改 | D3D12 surface 用例 |
| `test/src/PAGDiskCacheTest.cpp` | 修改 | D3D12 设备/异步路径 |

#### 阶段 D：脚本、CI、文档

| 文件 | 类型 | 说明 |
|---|---|---|
| `autotest.sh` / `update_baseline.sh` / `accept_baseline.sh` | 修改 | `USE_D3D12` / `USE_D3D12_WARP` |
| `.github/workflows/autotest.yml` | 修改 | `win-d3d12-warp` lane |
| `.github/workflows/build.yml` | 修改 | win 编译任务增加 D3D12 |
| `win/README.md` | 新增 | D3D12 构建与运行说明 |
| `README.md` | 修改 | 后端列表补充 D3D12 |
| `.codebuddy/rules/Test.md` | 修改 | D3D12 后端说明；修正过时的 Metal 过渡描述 |
| `.codebuddy/skills/accept-baseline/SKILL.md` | 修改 | 后端参数补充 D3D12 |

### 5.2 CMake 变更

```cmake
# 1) 后端白名单
set(PAG_IMPLEMENTED_GPU_BACKENDS OPENGL METAL WEBGPU D3D12)

# 2) 平台校验（放在 Metal 校验附近）
if (PAG_USE_D3D12 AND NOT WIN32)
    message(FATAL_ERROR "PAG_USE_D3D12 is only supported on Windows.")
endif ()

# 3) WARP 选项
option(PAG_D3D12_USE_WARP "Force the D3D12 backend onto the WARP software rasterizer" OFF)
if (PAG_D3D12_USE_WARP)
    if (NOT PAG_USE_D3D12)
        message(FATAL_ERROR "PAG_D3D12_USE_WARP requires PAG_USE_D3D12=ON.")
    endif ()
    list(APPEND PAG_DEFINES PAG_D3D12_USE_WARP)
endif ()

# 4) 非 D3D12 构建使用桩文件
if (PAG_USE_D3D12)
    list(FILTER SRC_FILES EXCLUDE REGEX "/base/utils/cast/D3D12Texture\\.cpp$")
endif ()

# 5) Windows 平台依赖
elseif (WIN32)
    if (USE_NATIVE_PLATFORM)
        if (PAG_USE_D3D12)
            find_library(D3D12_LIB d3d12 REQUIRED)
            find_library(DXGI_LIB dxgi REQUIRED)
            find_library(D3DCOMPILER_LIB d3dcompiler REQUIRED)
            find_library(DCOMP_LIB dcomp REQUIRED)
            list(APPEND PAG_SHARED_LIBS ${D3D12_LIB} ${DXGI_LIB} ${D3DCOMPILER_LIB} ${DCOMP_LIB})
        elseif (PAG_USE_ANGLE)
            # 现有 ANGLE 分支
        else ()
            # 现有 opengl32 分支
        endif ()
    endif ()
```

说明：`TGFX_USE_D3D12` 已经由 `PAG_DEFINES TGFX_USE_${PAG_GPU_BACKEND}` 生成，不需要重复添加。tgfx 自身在 `add_subdirectory` 时会链接 `d3d12/dxgi/d3dcompiler/dcomp`，libpag 显式声明是为了预编译 tgfx 库（`TGFX_LIB` 模式）下也能正确链接。

### 5.3 转换逻辑

```cpp
tgfx::BackendTexture ToTGFX(const BackendTexture& texture) {
  switch (texture.backend()) {
    // ... OpenGL / Metal / WebGPU
    case Backend::D3D12: {
      D3D12TextureInfo d3d12Info = {};
      if (!texture.getD3D12TextureInfo(&d3d12Info)) {
        return {};
      }
      tgfx::D3D12TextureInfo sampler = {};
      sampler.resource = d3d12Info.resource;
      sampler.format = GetD3D12TexturePixelFormat(d3d12Info.resource);
      return tgfx::BackendTexture{sampler, texture.width(), texture.height()};
    }
    // ...
  }
}
```

`ToPAG` 反向转换同理；`BackendRenderTarget` 的 `ToTGFX` 使用相同逻辑。

### 5.4 设备生命周期与线程

- `Devices::MakeDefault()` 每次调用都会新建 `tgfx::D3D12Device`（tgfx 不去重）。`DevicePool` 在测试里按线程缓存，离屏 `PAGSurface` 每次创建自己的设备；同一 `ID3D12Device` 上的资源仍可互通。
- 异步线程（`PAGDecoder`）走 `MakeForAsyncThread()`，D3D12 命令录制没有 thread-local 上下文，直接使用默认设备。
- 外部纹理异步渲染（`PAGSurface::MakeFrom(texture, origin, true)`）走 `MakeForAsyncThread(texture)`，优先从纹理反查设备，保证与调用方在同一适配器上。

### 5.5 窗口尺寸

`GPUDrawable::updateSize()` 目前使用 `GetWindowRect`（含边框），而 `tgfx::D3D12Window` 与 `tgfx::WGLWindow` 都按 `GetClientRect` 创建 render target。D3D12 分支改用 `GetClientRect`，避免 PAGSurface 尺寸与交换链尺寸不一致；GL 分支保持现状，避免改变既有行为。

### 5.6 测试后端命名与缓存

| 模式 | `PAG_BACKEND_NAME` | `TEST_TARGET_SUFFIX` | 缓存目录 |
|---|---|---|---|
| D3D12 硬件 | `d3d12` | `D3D12` | `test/baseline/.cache/d3d12` |
| D3D12 WARP | `d3d12-warp` | `D3D12` | `test/baseline/.cache/d3d12-warp` |

与 tgfx 的 `d3d12-warp` 命名保持一致。`test/baseline/version.json` 仍然全局共享，见 §9.2。

---

## 六、测试与验证

### 6.1 测试基建前提

1. **编译器**：测试依赖 `-fno-access-control`（`CMakeLists.txt:1273`），MSVC 不支持。方案是 Windows 测试构建使用 clang-cl（VS 2022 自带 `VC/Tools/Llvm/x64/bin/clang-cl.exe`），通过 CMake `-T ClangCL` 启用。需要先验证 tgfx + libpag 在 clang-cl 下的完整编译。
2. **MD5**：`Baseline.cpp` 改为统一包含 tgfx 的 `core/utils/MD5.h` 并使用 `tgfx::MD5::Calculate`。tgfx 在 Windows 上使用 wincrypt（`advapi32`，libpag 已链接），Apple 上使用 CommonCrypto，行为一致。
3. **ffavc**：`vendor/ffavc` 只有 `mac` 产物。Windows 与 Emscripten 一样视为“无软件解码器”：`TestEnvironment` 跳过注册，`VideoSequenceFallbackTest`、`PAGANRRegressionTest` 中依赖 ffavc 的用例按宏跳过。
4. **连带模块**：`PAG_BUILD_TESTS=ON` 会强制打开 `PAG_BUILD_PAGX/CLI/SVG/HTML/PPT`，需要确认这些模块在 clang-cl 下可编译。
5. **glyph atlas**：Metal 的 `PAGMetalAtlasTest` 依赖 tgfx 私有 atlas 重置钩子；D3D12 先不引入，若截图出现不稳定再单独处理。

### 6.2 用例清单

| 用例 | 覆盖点 |
|---|---|
| `PAGD3D12SurfaceTest/BackendTexture` | `PAGSurface::MakeFrom(BackendTexture)`、TopLeft/BottomLeft、`readPixels` |
| `PAGD3D12SurfaceTest/BackendRenderTarget` | `PAGSurface::MakeFrom(BackendRenderTarget)`、`getFrontTexture` 往返 |
| `PAGD3D12TextureTest/ImageFromTexture` | `PAGImage::FromTexture`、渲染结果比对 |
| `PAGD3D12TextureTest/AsyncThread` | `forAsyncThread=true` 时从纹理反查设备 |
| `PAGD3D12TextureTest/PixelFormats` | RGBA8 / BGRA8 等 DXGI 格式 |
| `PAGBlendD3D12Test/*` | 对应 Metal 的 `CopyDstTexture` / `TextureBottomLeft` / `BothBottomLeft` |
| `PAGXRuntimeTest/*D3D12*` | `pagx::PAGSurface` 从外部 BackendTexture/RenderTarget 创建 |
| `PAGDiskCacheTest/*` | 默认设备、异步线程、外部资源路径 |

截图用例必须使用 `PAG_PIXEL_TEST` / `PAGX_PIXEL_TEST` / `CLI_PIXEL_TEST` 注册，与现有像素比较机制保持一致。

### 6.3 基线接受

- 新增截图用例产生的基线版本只能通过用户执行 `/accept-baseline` 流程接受；本方案不直接运行 `accept_baseline.sh`，不手改 `test/baseline/version.json`。
- 首次生成基线前需要先跑 `./update_baseline.sh USE_D3D12_WARP` 建立本地缓存。

### 6.4 验证矩阵

| 检查项 | 命令 | 通过标准 |
|---|---|---|
| 配置 | `cmake -S . -B build-d3d12 -G "Visual Studio 17 2022" -A x64 -DPAG_USE_D3D12=ON -DPAG_BUILD_PAGX=ON -DPAG_BUILD_CLI=ON -DPAG_BUILD_SHARED=OFF` | 无 FATAL，生成解决方案 |
| 库与 CLI | `cmake --build build-d3d12 --config Release --target pagx-cli` | 编译通过 |
| 离屏渲染 | `pagx render` 对样例文档输出 PNG | 输出非空且内容正确 |
| 窗口渲染 | `win/Win32Demo.sln` 以 D3D12 配置构建并运行 | 播放、缩放正常 |
| 单元测试（WARP） | `cmake -S . -B build-d3d12-test -G "Visual Studio 17 2022" -A x64 -T ClangCL -DPAG_USE_D3D12=ON -DPAG_D3D12_USE_WARP=ON -DPAG_BUILD_TESTS=ON` + `cmake --build ... --target PAGFullTest_D3D12` | 全量用例通过（基线已接受后） |
| OpenGL 回归 | Windows 下 `-DPAG_USE_OPENGL=ON` 编译 | 编译通过，行为不变 |
| Metal 回归 | macOS CI `mac_metal` lane | 通过（本次不改 Apple 平台代码） |

---

## 七、CI 与文档

- `autotest.sh`：新增 `USE_D3D12`、`USE_D3D12_WARP` 分支，后者追加 `-DPAG_D3D12_USE_WARP=ON`，后端名 `d3d12-warp`。
- `update_baseline.sh`：同上；WARP 模式的缓存写入 `test/baseline/.cache/d3d12-warp`。
- `accept_baseline.sh`：新增 `USE_D3D12` / `USE_D3D12_WARP`，目标后缀统一 `D3D12`。
- `.github/workflows/autotest.yml`：新增 `win_d3d12_warp` 任务（`windows-latest`，WARP，缓存 `test/baseline/.cache/d3d12-warp`），与 `mac_metal` 结构对齐。
- `.github/workflows/build.yml`：`win` 任务增加 D3D12 编译验证（可先只构建 `pagx-cli` 与 `Win32Demo`）。
- `win/README.md`：新增 D3D12 构建说明（`PAG_USE_D3D12=ON`、WARP、VS 版本要求）。
- `.codebuddy/rules/Test.md`：补充 D3D12 后端命名与缓存说明，并修正已过时的 Metal 描述（7 个 key 实际已在 `version.json`，mac Metal CI 已运行完整套件）。

---

## 八、交付计划

| 阶段 | 内容 | 验收 |
|---|---|---|
| PR 1 | 阶段 A + B：公共 API、后端胶水、纹理转换、Windows 窗口、CMake | Windows 下 D3D12 配置/编译通过；`pagx render` 与 `Win32Demo` 冒烟通过；OpenGL 构建不回归 |
| PR 2 | 阶段 C：Windows 测试基建、D3D12 用例、基线 | `PAGFullTest_D3D12`（WARP）在基线接受后全量通过 |
| PR 3 | 阶段 D：脚本、CI、文档 | `win_d3d12_warp` lane 跑通；文档与规则同步 |

如果评审希望一次性合入，三个阶段的改动也可以合并为单个 PR，但建议至少保留“核心+窗口”与“测试/CI”两次独立验证。

---

## 九、风险与限制

### 9.1 测试工具链（最大不确定项）

MSVC 不支持 `-fno-access-control`，而 libpag 测试大量直接访问私有成员（如 `diskCache->locker`、`PAGFile::composition`）。可选方案：

1. Windows 全量构建切 clang-cl（推荐，改动集中在 CMake，测试代码不变）。
2. 引入 `PAG_TEST_ACCESS_PRIVATE` 宏，改造所有私有访问点（工作量大，且与现有测试写法不一致）。
3. 暂不做测试目标，只交付核心与平台层。

### 9.2 基线硬件相关性

D3D12 截图随适配器/驱动变化，CI 只能使用 WARP。`test/baseline/version.json` 是全局共享的，硬件与 WARP 两个缓存交替接受时会互相触发缓存重建（现有机制可恢复，但需要约定“CI 只跑 WARP，硬件模式仅本地调试”）。

### 9.3 信号量

`pag::BackendSemaphore` 目前只有 GL sync。`PAGSurface::draw()` 对 `tgfx::Backend::D3D12` 走 `LOGE` 分支（`src/rendering/PAGSurface.cpp:376`），与 Metal 现状一致。跨队列同步需要调用方自行处理，后续如要支持，需要同时扩展公共 ABI 与 tgfx 的 `D3D12SyncInfo` 转发。

### 9.4 设备身份校验

`CaptureFromTexture()` 返回 nullptr，`CanSampleFrom()` 退化为“context 非空即通过”。跨设备误用不会在绘制前被拒绝，可能在命令提交阶段表现为未定义行为。根因是 tgfx 公共 `D3D12Device` 没有暴露 `ID3D12Device`。

### 9.5 设备去重

tgfx 不对 D3D12 设备去重，libpag 多次 `MakeDefault()` 会产生多个 `tgfx::Device`（各自缓存与队列）。资源可互通，但显存与 shader 缓存有冗余。若后续要收敛，需要 tgfx 提供原生设备注册或 libpag 侧缓存默认设备。

### 9.6 `getHardwareBuffer()`

Windows 的 `HardwareBufferRef` 是 `ID3D11Texture2D*`，D3D12 路径无法产生，返回 null。需要在 `win/README.md` 中说明。

### 9.7 CI 成本

WARP 是 CPU 光栅化，全量截图套件耗时明显高于 Metal lane；Windows runner 上还可能受并发与缓存命中影响。建议 `win_d3d12_warp` 只在 push 与定时任务运行。

### 9.8 tgfx 版本依赖

方案基于 `a3349d9a5`。如果实施中发现需要新的 tgfx 公开接口，必须单独升级 `DEPS` 并评估对 Metal/OpenGL 的影响，不在本 PR 内夹带。

---

## 十、待确认事项

1. 测试工具链：接受“Windows 测试构建切 clang-cl”，还是先不做 `PAGFullTest_D3D12`、把测试放到后续 PR？
2. WARP 开关：使用编译期 `PAG_D3D12_USE_WARP`（与 tgfx 一致），还是运行时环境变量？
3. 交付拆分：按 §8 的三个 PR 推进，还是一次性合入？

---

## 附录 A：Metal ↔ D3D12 对照

| 概念 | Metal | D3D12 |
|---|---|---|
| 公共纹理描述 | `MtlTextureInfo { void* texture; }` | `D3D12TextureInfo { void* resource; }` |
| 格式来源 | `MTLTexture.pixelFormat` | `ID3D12Resource::GetDesc().Format` |
| 默认设备 | `MetalDevice::Make()` | `D3D12Device::Make()` → `MakeWarp()` |
| 设备去重 | tgfx 按 `MTLDevice` 注册 | 无 |
| 外部纹理反查设备 | `MTLTexture.device` | `ID3D12Resource::GetDevice()` |
| 设备身份校验 | 比较 `MTLDevice` | 不支持（退化为信任调用方） |
| 窗口接入 | `CAMetalLayer` + `MetalLayerDrawable` | `tgfx::D3D12Window`（swap chain） |
| 平台公开入口 | `PAGSurface (Metal)` category | 内部 `GPUDrawable` |
| 信号量 | 未开放 | 未开放 |
| 测试缓存 | `test/baseline/.cache/metal` | `test/baseline/.cache/d3d12[-warp]` |

## 附录 B：参考资料

- Metal 后端：`ba9952640` (#3717) Add Metal backend support on Apple platforms
- Metal 测试与基线：`c0b63f058` (#3781)、`c52be7304` (#3804)、`8a63822b0` (#3798)
- Metal CI：`eb4412aa5` (#3828)、`e0815e9ca` (#3824)
- tgfx D3D12 接口：`third_party/tgfx/include/tgfx/gpu/d3d12/`
- tgfx D3D12 CI：`third_party/tgfx/.github/workflows/autotest.yml` 的 `win-d3d12-warp`
