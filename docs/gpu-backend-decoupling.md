# libpag GPU 后端解耦设计方案

## 版本信息

- 文档版本：v1.0
- 日期：2026-08-25
- 分支：`feature/thunderllei_gpu_backend`
- 状态：**待评审**

---

## 一、背景与目标

### 1.1 现状

libpag 的公开 API 保留了 `MOCK / OPENGL / METAL / VULKAN` 四种 `Backend` 枚举（`include/pag/gpu.h`），但**实际渲染管线只能跑在 OpenGL 之上**。tgfx 底层已经在 v2.1.1 中把架构升级到「统一 GPU 抽象（`GPU` / `CommandEncoder` / `RenderPipeline` / `RuntimeEffect`）」并支持 6 种后端（OpenGL / Metal / Vulkan / D3D12 / WebGPU / Mock），但 libpag 侧的胶水层未跟进——存在大量对 `tgfx::GLDevice`、`GLTextureInfo`、`GLFrameBufferInfo`、EAGL/CGL/EGL/WGL/WebGL Window 的硬绑定。

### 1.2 tgfx 编译期约束

tgfx 的后端是**编译期强互斥**的（参见 `third_party/tgfx/CMakeLists.txt` 中 `TGFX_USE_*` 的互斥/校验块，`FATAL_ERROR` 处）：

- `TGFX_USE_OPENGL / METAL / VULKAN / D3D12 / WEBGPU` 五选一（选一个自动关掉其它）
- 只有选中的后端源码会被编入 `libtgfx.a`
- 未选中的后端头文件里的类静态方法（如 `tgfx::MetalDevice::Make()`）在链接时会找不到符号

**这决定了 libpag 侧的后端选择也必须是编译期的**——通过 `#ifdef` 分派，而不是运行时 switch。

### 1.3 本次改造目标

**只做架构准备，不引入新后端**。核心目标：

1. 把 libpag 里约 26 处对 `tgfx::GLDevice` 的直接调用、以及所有 `#include "tgfx/gpu/opengl/*.h"` **收敛到 1 个新文件** `Devices.cpp`（平台特化白名单除外，见 §5.4）
2. 通过编译期宏 `TGFX_USE_OPENGL/METAL/VULKAN/D3D12/WEBGPU` 分派
3. 其他所有渲染代码只见 `tgfx::Device` 基类
4. 引入 `PAG_USE_METAL/VULKAN/D3D12/WEBGPU` CMake 开关，透传到 tgfx，与 `PAG_USE_OPENGL` 互斥
5. **对外 API 完全不变**（`include/pag/*.h`、平台层 Java/OC/JS API 全部保留签名和行为）

**本次不做**：

- 不实际引入 Metal/Vulkan/D3D12/WebGPU 任何一个新后端
- 不改 tgfx 头文件（`VulkanImageInfo` 无 `VkDevice` 字段等历史遗留延后）
- 不扩展 `BackendSemaphore` ABI（`initMetal/Vulkan/...` 等延后到具体后端接入时）
- 不动 iOS `+FromCVPixelBuffer:context:(EAGLContext*)` 等对外 GL 特化 API

### 1.4 收益

- 未来接入 Metal/Vulkan/D3D12/WebGPU 只需在 `Devices.cpp` 加 `#elif` 分支
- 核心渲染代码变得"后端无关"，可读性提升
- 无破坏性变更，可以独立合入 main
- 为公司内部潜在的 Metal 后端 PoC 铺路

---

## 二、GL 依赖盘点

按依赖形态分类的完整清单（约 26 处调用点，逐一对应 §5.3 修改文件表；平台特化白名单见 §5.4）：

### 类别 A：只需要"某个 Device"（易解耦）

对 Device 类型没有硬性要求，只是当年只有 GL 后端所以写死了 `GLDevice`：

| 文件 | 依赖形态 |
|---|---|
| `src/rendering/drawables/OffscreenDrawable.cpp` | `GLDevice::MakeWithFallback` |
| `src/rendering/drawables/HardwareBufferDrawable.cpp` | `GLDevice::MakeWithFallback` |
| `src/rendering/drawables/BitmapDrawable.h/.cpp` | 签名含 `std::shared_ptr<GLDevice>` |
| `src/rendering/drawables/DoubleBufferedDrawable.cpp` | `#include GLDevice.h`（未实际使用类型） |
| `src/rendering/CompositionReader.h/.cpp` | 签名含 `std::shared_ptr<GLDevice>` |
| `src/pagx/runtime/OffscreenDrawable.cpp` | `GLDevice::Make()` |
| `src/pagx/PAGImage.cpp` | `GLDevice::Current()` |
| `src/pagx/PAGSurface.cpp` | 2 处 `GLDevice::Current()` |
| `src/pagx/utils/RasterUtils.cpp` | `GLDevice::Make()` |
| `src/pagx/html/HTMLStaticImageRenderer.cpp` | `GLDevice::Make()` |
| `src/pagx/html/HTMLPlusDarkerRenderer.cpp` | `GLDevice::Make()` |
| `src/cli/CommandRender.cpp` | `GLDevice::Make()` |

### 类别 B：GL 独有的"当前上下文"语义

需要特别设计，Metal/VK 无对应概念：

| 文件 | 依赖形态 |
|---|---|
| `src/rendering/PAGSurfaceFactory.cpp` | `GLDevice::Current` / `GLDevice::Make(sharedContext)` |
| `src/rendering/PAGDecoder.h/.cpp` | 构造函数 + 成员含 `std::shared_ptr<GLDevice>`；`GLDevice::CurrentNativeHandle` |
| `src/rendering/editing/StillImage.cpp` | `GLDevice::CurrentNativeHandle`（在 `PAGImage::FromTexture`） |
| `src/rendering/graphics/Picture.cpp` | `GLDevice::CurrentNativeHandle`；`static_cast<GLDevice*>` + `sharableWith` |
| `src/c/ext/pag_surface_ext.cpp` | `GLDevice::Make(sharedContext)` |

### 类别 C：GL 特有类型（本次不动，延后）

- `include/pag/gpu.h` + `src/rendering/GPUBackend.cpp`：`BackendSemaphore` 只有 `initGL/glSync`
- `src/base/utils/TGFXCast.cpp`：`ToTGFX/ToPAG` 只覆盖 GL 分支
- `src/c/pag_backend_texture.cpp`、`src/c/pag_backend_semaphore.cpp`
- `src/platform/android/JPAG*.cpp`、`src/platform/web/PAGWasmBindings.cpp`

### 类别 D：真正的 GL API 调用

- `src/rendering/utils/GLRestorer.cpp` —— 保存/恢复 GL 全局状态（15 个状态项）
- 各平台 `src/platform/{ios,mac,android,ohos,qt,win,web}/GPUDrawable.*` —— 使用 tgfx 各平台 GL Window

### 类别 E：滤镜 shader（已抽象，零改动）

`src/rendering/filters/*` 里所有 GLSL 都通过 `tgfx::RuntimeEffect + CommandEncoder + RenderPipeline` 抽象，tgfx 负责编译到目标后端。

---

## 三、设计方案

### 3.1 核心思路

新增一个 libpag 内部胶水层 `pag::Devices`（静态工具类），封装所有 tgfx 后端具体调用。整个 libpag 只有 **1 个 `Devices.cpp`** 会 `#include "tgfx/gpu/opengl/*.h"`（以及未来 `metal/*.h`、`vulkan/*.h`）；所有其他代码只见 `tgfx::Device` 基类。

### 3.2 `Devices` 接口设计

```cpp
// src/rendering/gpu/Devices.h
#pragma once

#include <memory>
#include "tgfx/gpu/Backend.h"
#include "tgfx/gpu/Device.h"

namespace pag {

// 抽象基类：外部 GPU 状态守护。save()/restore() 分离，对象由调用方（PAGSurface）
// 持有并跨帧复用，避免每帧堆分配。GL 后端下真正实现；其他后端不创建（返回 nullptr）。
class ExternalStateGuard {
 public:
  virtual ~ExternalStateGuard() = default;
  virtual void save(tgfx::Context* context) = 0;  // 保存外部 GPU 全局状态
  virtual void restore() = 0;                      // 恢复到 save 时的状态
};

// 外部 GPU 资源身份标记（用于校验 device 归属）
// 各后端子类持有对应资源的强引用，避免悬空。
class ExternalDeviceRef {
 public:
  virtual ~ExternalDeviceRef() = default;
};

/**
 * libpag 后端胶水层。所有对具体 tgfx 后端（GL/Metal/Vulkan/D3D12/WebGPU）的调用都收敛到这里，
 * 通过编译期宏 TGFX_USE_OPENGL / TGFX_USE_METAL / ... 分派。
 * 其他 libpag 代码只应该看到 tgfx::Device 基类。
 */
class Devices {
 public:
  // ─── A: 通用工厂 ─────────────────────────────────────────

  /**
   * 创建一个默认 device。用于离屏渲染、CLI、pagx 等无外部资源约束的场景。
   *   GL: GLDevice::MakeWithFallback()
   *   Metal: MetalDevice::Make()
   *   D3D12: D3D12Device::Make() 或 MakeWarp()
   *   Vulkan: VulkanDevice::Make()
   *   WebGPU: WebGPUDevice::Make()
   */
  static std::shared_ptr<tgfx::Device> MakeDefault();

  /**
   * 为异步线程创建独立 device。
   *   GL: 从当前线程 GL context 派生共享 context（GLDevice::Make(currentHandle)）
   *   其他后端: 等同 MakeDefault
   * 用途：PAGDecoder 后台线程 / PAGSurface::MakeFrom(BackendTexture, forAsyncThread=true)
   */
  static std::shared_ptr<tgfx::Device> MakeForAsyncThread();

  // ─── B: 与外部 GPU 资源关联 ───────────────────────────────

  struct AdoptedDevice {
    std::shared_ptr<tgfx::Device> device;
    bool externalContext = false;  // 是否需要 GL 状态保护
  };

  /**
   * 复用宿主线程"当前 GPU 上下文"作为 device。仅 GL 后端有意义
   * （返回 externalContext=true，需要走 ExternalStateGuard）。
   * 其他后端返回 {nullptr, false}。
   * 用途：PAGSurface::MakeFrom(BackendRenderTarget) / PAGSurface::MakeFrom(BackendTexture, false)
   */
  static AdoptedDevice AdoptCurrent();

  /**
   * 为「采样指定外部纹理」推断并创建 device。
   *   GL: 忽略 texture 参数，直接返回当前线程 context 对应的 device（GLDevice::Current()）。
   *       GL 语义下无法从 texture id 反查 context，沿用既有约定：外部 texture 必须由当前
   *       context 创建/共享。
   *   Metal: 从 MTLTexture.device 反查
   *   D3D12: 从 ID3D12Resource::GetDevice() 反查
   *   Vulkan/WebGPU: 返回 MakeDefault()（约定外部 texture 必须由 libpag device 创建）
   * 命名说明：GL 下 texture 参数实际未被使用，保留参数是为统一各后端签名。
   * 用途：PAGImage::FromTexture(BackendTexture) 的隐式 device 推断
   */
  static std::shared_ptr<tgfx::Device> MakeForTexture(const tgfx::BackendTexture& texture);

  // ─── C: 身份捕获与校验 ──────────────────────────────────

  /**
   * 捕获调用线程当前"宿主 GPU 上下文"的身份，用于将来在渲染时校验 device 匹配。
   *   GL: 记录 CurrentNativeHandle
   *   Metal: 记录当前"隐式关联的 MTLDevice"（若无 TLS 概念则返回 nullptr）
   *   D3D12: 类似 Metal
   *   Vulkan/WebGPU: 返回 nullptr（无 TLS 概念）
   * 用途：Picture::BackendTextureProxy 保存外部 device 身份
   */
  static std::shared_ptr<ExternalDeviceRef> CaptureCurrent();

  /**
   * 判断 context 是否能采样一个持有 deviceRef 的资源。
   *   GL: GLDevice::sharableWith(nativeHandle)
   *   Metal: 比较 MTLDevice 指针
   *   D3D12: 比较 ID3D12Device 指针
   *   Vulkan/WebGPU: deviceRef 通常为 nullptr，总返回 true
   */
  static bool CanSampleFrom(tgfx::Context* context, const ExternalDeviceRef* deviceRef);

  // ─── D: GL 外部状态保护 ────────────────────────────────

  /**
   * 创建一个外部 GPU 状态守护对象，供 PAGSurface 在其生命周期内复用
   * （每帧 lockContext 调 save(context)、unlockContext 调 restore()）。
   *   GL: 返回 GLRestorer（保存/恢复 viewport / scissor / program / FBO / VAO / VBO / blend...）。
   *   非 GL 后端返回 nullptr（Metal/VK/D3D12/WebGPU 是无状态命令录制，不存在污染）。
   * 用途：PAGSurface 构造时创建一次；仅 externalContext=true 时需要。
   */
  static std::unique_ptr<ExternalStateGuard> MakeExternalStateGuard();

  // ─── E: 用户外部 device 注入 ─────────────────────────────
  // 本次不引入 SetSharedDevice。理由见 §3.8：它是一个语义随后端而异、且本次无法验证的
  // 进程级可变全局状态，属于 Vulkan/WebGPU 接入时才需要的能力。等真正接入这些后端时，
  // 连同其生命周期/线程安全语义一起在对应 PR 中定稿，避免留下无测试覆盖的死接口。
};

}  // namespace pag
```

### 3.3 各后端映射表

| 方法 | GL | Metal | Vulkan | D3D12 | WebGPU |
|---|---|---|---|---|---|
| `MakeDefault()` | `GLDevice::MakeWithFallback()` | `MetalDevice::Make()` | `VulkanDevice::Make()` | `D3D12Device::Make()` | `WebGPUDevice::Make()` |
| `MakeForAsyncThread()` | `GLDevice::Make(currentHandle)` | 同 MakeDefault | 同 MakeDefault | 同 MakeDefault | 同 MakeDefault |
| `AdoptCurrent()` | `{GLDevice::Current(), true}` | `{nullptr, false}` | `{nullptr, false}` | `{nullptr, false}` | `{nullptr, false}` |
| `MakeForTexture(tex)` | `GLDevice::Current()`（忽略 tex） | 反查 `MTLTexture.device` | `MakeDefault()`（信任） | 反查 `ID3D12Resource::GetDevice` | `MakeDefault()`（信任） |
| `CaptureCurrent()` | 存 `nativeHandle` | 无 TLS，`nullptr` | `nullptr` | 无 TLS，`nullptr` | `nullptr` |
| `CanSampleFrom(ctx, ref)` | `sharableWith(handle)` | 比较 `MTLDevice*` | ref 为空，返回 true | 比较 `ID3D12Device*` | ref 为空，返回 true |
| `MakeExternalStateGuard()` | `GLRestorer`（save/restore） | `nullptr`（no-op） | `nullptr` | `nullptr` | `nullptr` |

### 3.4 已知的后端可用性差异

以下是**对未来接入各后端的定性预判**（本次不引入任何非 GL 后端，无实测数据，仅用于评估架构预留是否足够）：

| 后端 | 预判 | 主要限制 |
|---|---|---|
| **GL** | 完全可用 | 无变化，行为完全兼容 |
| **Metal** | 基本可用 | 多 GPU + 主动选非默认 GPU 场景需未来的 device 注入 API |
| **D3D12** | 基本可用 | 同 Metal |
| **Vulkan** | 需强依赖 device 注入 | 未注入时外部资源通路禁用（`VulkanImageInfo` 无法反查 `VkDevice`）；桌面无 HardwareBuffer |
| **WebGPU** | 需强依赖 device 注入 | 未注入时外部资源通路禁用；`WebGPUDevice::Make()` 是异步的，libpag 内建有阻塞风险 |

Vulkan / WebGPU 后端**未来接入时**必须依赖 device 注入 API（本次不引入，理由见 §3.8）。

### 3.5 GLRestorer 改造

现有 `PAGSurface` 里持有 `void* glRestorer`：

```cpp
PAGSurface::PAGSurface(std::shared_ptr<Drawable> drawable, bool externalContext) {
#if !defined(PAG_BUILD_FOR_WEB) && !defined(_WIN32)
  if (externalContext) {
    glRestorer = new GLRestorer();
  }
#endif
}
```

改造后。注意保留现有 `GLRestorer` 的**对象复用**特性——现状是 `PAGSurface` 构造时 `new` 一次，
每帧 `lockContext` 里 `save()` / `unlockContext` 里 `restore()`，**不是每帧重新分配**。若改成每帧
`Devices::MakeExternalStateGuard()`（早期设计曾考虑的 RAII 版本）现造一个带虚表的 guard 并析构，会在每帧渲染热路径上多一次堆分配 +
虚析构，对高帧率场景是无谓开销。因此 `ExternalStateGuard` 采用 **save/restore 分离**、对象在
`PAGSurface` 生命周期内复用的模型：

```cpp
class PAGSurface {
 private:
  std::unique_ptr<ExternalStateGuard> stateGuard;  // 只用前置声明；构造时创建一次
  // ... externalContext 保留供 lockContext 判断
};

PAGSurface::PAGSurface(std::shared_ptr<Drawable> drawable, bool externalContext)
    : drawable(std::move(drawable)), externalContext(externalContext) {
  rootLocker = std::make_shared<std::mutex>();
  if (externalContext) {
    // GL 后端返回具体 guard；其他后端返回 nullptr。创建一次，全生命周期复用。
    stateGuard = Devices::MakeExternalStateGuard();
  }
}

tgfx::Context* PAGSurface::lockContext() {
  auto device = drawable->getDevice();
  if (device == nullptr) return nullptr;
  auto context = device->lockContext();
  if (context != nullptr && stateGuard != nullptr) {
    stateGuard->save(context);  // GL: 保存全局状态；其他后端不会走到这里（guard 为 nullptr）
  }
  return context;
}

void PAGSurface::unlockContext() {
  if (stateGuard != nullptr) {
    stateGuard->restore();  // GL: 恢复
  }
  drawable->getDevice()->unlock();
}
```

对应 `ExternalStateGuard` 基类改为显式 save/restore（而非 RAII 析构恢复）：

```cpp
class ExternalStateGuard {
 public:
  virtual ~ExternalStateGuard() = default;
  virtual void save(tgfx::Context* context) = 0;
  virtual void restore() = 0;
};
```

`GLRestorer` 从 `src/rendering/utils/` 挪到 `src/rendering/gpu/`，改造为 `ExternalStateGuard` 的子类，只在 `TGFX_USE_OPENGL && !PAG_BUILD_FOR_WEB && !_WIN32` 分支下编译。

Windows 排除现有 GLRestorer 是既有行为（`PAGSurface.cpp` 现有 `#if !defined(_WIN32)`），本次不改。

### 3.6 Picture::BackendTextureProxy 改造

现有：

```cpp
class BackendTextureProxy {
 private:
  void* sharedContext = nullptr;  // 悬空隐患

  bool checkContext(tgfx::Context* context) const {
    auto glDevice = static_cast<tgfx::GLDevice*>(context->device());
    return glDevice->sharableWith(sharedContext);
  }
};
```

改造后：

```cpp
class BackendTextureProxy {
 private:
  std::shared_ptr<ExternalDeviceRef> deviceRef;  // 各后端子类持强引用

  bool checkContext(tgfx::Context* context) const {
    if (!Devices::CanSampleFrom(context, deviceRef.get())) {
      LOGE("A Graphic made from a texture can not be drawn on to a PAGSurface"
           " if its GPU context is not a share context to the PAGSurface.");
      return false;
    }
    return true;
  }
};
```

构造时用 `Devices::CaptureCurrent()` 获取 `deviceRef`。

### 3.7 头文件签名变更

三个内部头文件里的 `std::shared_ptr<tgfx::GLDevice>` 参数改为 `std::shared_ptr<tgfx::Device>`：

```cpp
// src/rendering/drawables/BitmapDrawable.h
class BitmapDrawable : public Drawable {
 public:
  static std::shared_ptr<BitmapDrawable> Make(
      int width, int height,
      std::shared_ptr<tgfx::Device> sharedDevice = nullptr);  // was GLDevice
  // ...
};

// src/rendering/CompositionReader.h
class CompositionReader {
 public:
  static std::shared_ptr<CompositionReader> Make(
      int width, int height,
      std::shared_ptr<tgfx::Device> sharedDevice = nullptr);  // was GLDevice
  // ...
};

// src/rendering/PAGDecoder.h
class PAGDecoder {
 private:
  PAGDecoder(std::shared_ptr<PAGComposition>, int, int, int, float, float,
             std::shared_ptr<tgfx::Device> sharedDevice);  // was GLDevice
  std::shared_ptr<tgfx::Device> sharedDevice;  // was GLDevice
};
```

这三处都是**内部头文件**，不算破坏性变更。改后这些头文件不再需要 `#include "tgfx/gpu/opengl/GLDevice.h"`。

### 3.8 关于 `SetSharedDevice`：本次不引入

§3.2 接口里刻意**没有** `SetSharedDevice`。原因：

1. 它是一个**进程级可变全局状态**，与 `Devices` 作为无状态静态工具类的定位相冲突（生命周期归属、传 nullptr 清空后已派生 device 的处理、与并发 `Make*` 的顺序都需明确定义）。
2. 它的语义**随后端而异**：GL 下是"基于注入 device 派生"，其他后端是"直接复用"，同一 API 两种行为，调用方无法预知拿到的是共享还是派生。
3. 本次不引入任何非 GL 后端，这个 API **无法被任何测试覆盖**，引入即成死代码。

因此把它推迟到 Vulkan/WebGPU 真正接入的 PR，届时连同其生命周期与线程安全语义一起定稿。本次改造对它的唯一"预留"是：`Devices` 的所有工厂方法都是静态入口，未来加注入点不需要改调用方——这一点已经满足。

---

## 四、CMake 改造

### 4.1 现有 CMake 结构（改造前必读）

顶层 `CMakeLists.txt` 里与后端相关的逻辑不是一处，改造必须与它们合并，而不是新增一段浮空代码：

1. **选项定义**（L27-L30）：`PAG_USE_OPENGL`（默认 ON）、`PAG_USE_SWIFTSHADER`、`PAG_USE_ANGLE`、`PAG_USE_QT`。注意 **QT/SWIFTSHADER/ANGLE 都是 GL 的具体实现变体，不是独立后端**。
2. **既有互斥**（L90-L95）：`PAG_USE_QT` 优先，其次 `PAG_USE_SWIFTSHADER`，会关掉其它 GL 变体。
3. **既有强制开 GL**（L119-L121）：`if (PAG_USE_QT OR PAG_USE_SWIFTSHADER OR PAG_USE_ANGLE) set(PAG_USE_OPENGL ON)`。
4. **透传到 tgfx 有三条路径**（L641-L720）：
   - `TGFX_LIB + TGFX_INCLUDE`：直接用外部预编译 `.a`，**不透传任何 `TGFX_USE_*`**（后端由那个 `.a` 编译时决定）。
   - `HAS_CUSTOM_TGFX_DIR`（`-DTGFX_DIR`）：`add_subdirectory` 源码构建，通过 `set(TGFX_USE_* ...)`（L652-）透传。
   - 内建：`TGFX_CACHE_DIR` 存在时走 `node build_tgfx` 缓存构建，通过 `TGFX_OPTIONS` 列表拼 `-DTGFX_USE_*`（L676-）；否则回退到 `add_subdirectory`（L710-）透传。

**这意味着新增后端开关必须在上述 (2)(3) 之间插入新的互斥，并在 (4) 的三条透传路径里同步加 4 个 `TGFX_USE_*`。**

### 4.2 新增开关（与现有逻辑合并后的完整形态）

选项定义处（L27-L30 附近）追加：

```cmake
option(PAG_USE_OPENGL "Allow use of OpenGL as GPU backend" ON)     # 已有
option(PAG_USE_METAL "Use Metal as the GPU backend on Apple" OFF)  # 新增
option(PAG_USE_VULKAN "Use Vulkan as the GPU backend" OFF)         # 新增
option(PAG_USE_D3D12 "Use D3D12 as the GPU backend on Windows" OFF) # 新增
option(PAG_USE_WEBGPU "Use WebGPU as the GPU backend on Web" OFF)  # 新增
```

互斥逻辑：新块必须放在**现有 L90-L95 之前**，先由"非 GL 后端"关掉所有 GL 变体，再让现有
L119-L121 的"强制开 GL"在 `PAG_USE_OPENGL` 已被关的前提下不再触发（因为 QT/SWIFTSHADER/ANGLE
此时也都已被关）：

```cmake
# ── 新增：非 GL 后端优先，选中后关闭全部 GL 相关变体 ──
# 必须在既有 "if (PAG_USE_QT OR ...)" 互斥块之前
if (PAG_USE_METAL OR PAG_USE_VULKAN OR PAG_USE_D3D12 OR PAG_USE_WEBGPU)
    set(PAG_USE_OPENGL OFF)
    set(PAG_USE_QT OFF)
    set(PAG_USE_SWIFTSHADER OFF)
    set(PAG_USE_ANGLE OFF)
endif ()

# ── 既有块保持不动（L90-L95） ──
if (PAG_USE_QT)
    set(PAG_USE_SWIFTSHADER OFF)
    set(PAG_USE_ANGLE OFF)
elseif (PAG_USE_SWIFTSHADER)
    set(PAG_USE_ANGLE OFF)
endif ()

# ...（L119-L121 既有块保持不动）...
# if (PAG_USE_QT OR PAG_USE_SWIFTSHADER OR PAG_USE_ANGLE) set(PAG_USE_OPENGL ON) endif ()

# ── 新增：至少开一个后端 ──
if (NOT PAG_USE_OPENGL AND NOT PAG_USE_METAL AND NOT PAG_USE_VULKAN
    AND NOT PAG_USE_D3D12 AND NOT PAG_USE_WEBGPU)
    message(FATAL_ERROR
        "At least one PAG_USE_* backend must be enabled "
        "(PAG_USE_OPENGL / METAL / VULKAN / D3D12 / WEBGPU)")
endif ()
```

透传：三条 tgfx 构建路径都要补 4 个 `TGFX_USE_*`。

- **`HAS_CUSTOM_TGFX_DIR` 源码路径**（L652 附近）与**内建 `add_subdirectory` 回退**（L710 附近）追加：

  ```cmake
  set(TGFX_USE_METAL ${PAG_USE_METAL})
  set(TGFX_USE_VULKAN ${PAG_USE_VULKAN})
  set(TGFX_USE_D3D12 ${PAG_USE_D3D12})
  set(TGFX_USE_WEBGPU ${PAG_USE_WEBGPU})
  ```

- **内建缓存路径**（L676 的 `TGFX_OPTIONS` 附近）追加：

  ```cmake
  list(APPEND TGFX_OPTIONS "-DTGFX_USE_METAL=${PAG_USE_METAL}")
  list(APPEND TGFX_OPTIONS "-DTGFX_USE_VULKAN=${PAG_USE_VULKAN}")
  list(APPEND TGFX_OPTIONS "-DTGFX_USE_D3D12=${PAG_USE_D3D12}")
  list(APPEND TGFX_OPTIONS "-DTGFX_USE_WEBGPU=${PAG_USE_WEBGPU}")
  ```

- **`TGFX_LIB + TGFX_INCLUDE` 预编译路径**：**不透传**（后端已固化在外部 `.a` 里）。若外部 `.a`
  与 `PAG_USE_*` 声明的后端不一致，会在链接期暴露为符号缺失——本方案不额外校验，由使用方保证一致。

> **缓存一致性风险**：内建缓存路径下，切换后端会导致 `node build_tgfx` 以不同 `TGFX_USE_*`
> 产出不同的 `tgfx.a`，缓存 key（`.tgfx.<arch>.md5`）需能区分后端，否则会命中错误的旧缓存。
> **本次改造仍以 GL（默认）为唯一实际构建目标，切换非 GL 后端仅在 `-DTGFX_DIR` 源码路径下验证**；
> 缓存路径的多后端产物管理留到具体后端接入 PR 再处理。

具体平台约束（Metal 只能 Apple、D3D12 只能 Windows 等）依赖 tgfx 自己报错（见
`third_party/tgfx/CMakeLists.txt` 中 `TGFX_USE_*` 平台校验块，会 `FATAL_ERROR`），libpag 不重复校验。

### 4.3 编译宏

`Devices.cpp` 使用编译期宏进行分派：

```cpp
#if defined(TGFX_USE_OPENGL)
  #include "tgfx/gpu/opengl/GLDevice.h"
  // GL 分支实现
#elif defined(TGFX_USE_METAL)
  #include "tgfx/gpu/metal/MetalDevice.h"
  // Metal 分支实现
#elif defined(TGFX_USE_VULKAN)
  #include "tgfx/gpu/vulkan/VulkanDevice.h"
  // Vulkan 分支实现
#elif defined(TGFX_USE_D3D12)
  #include "tgfx/gpu/d3d12/D3D12Device.h"
  // D3D12 分支实现
#elif defined(TGFX_USE_WEBGPU)
  #include "tgfx/gpu/webgpu/WebGPUDevice.h"
  // WebGPU 分支实现
#else
  #error "No GPU backend selected"
#endif
```

**注意**：tgfx 的 `TGFX_USE_*` 宏在 tgfx CMakeLists.txt 里是 `target_compile_definitions(tgfx PRIVATE ...)` 定义的。libpag 需要拿到这些宏，两种方案：

- **方案 A**：改 tgfx CMake 把这些 defines 从 `PRIVATE` 改成 `PUBLIC`（更规范，需要 tgfx 侧配合）
- **方案 B**：libpag 自己在 CMake 里根据 `PAG_USE_*` 定义一套等价宏 `target_compile_definitions(pag PRIVATE TGFX_USE_OPENGL=1 ...)`

**采用方案 B**，避免对 tgfx 的耦合。这样即使 tgfx 未来调整可见性策略，libpag 也不受影响。

---

## 五、文件清单

### 5.1 新增文件

```
src/rendering/gpu/
├── Devices.h            # 接口定义（本文档 §3.2）
├── Devices.cpp          # 5 个 #if defined() 分支实现
├── GLRestorer.h         # 从 src/rendering/utils/ 挪过来
└── GLRestorer.cpp       # 改造为 ExternalStateGuard 子类
```

### 5.2 删除文件

```
src/rendering/utils/GLRestorer.h    # 挪走
src/rendering/utils/GLRestorer.cpp  # 挪走
```

### 5.3 修改文件

**核心渲染层**（10 个文件）：

| 文件 | 改动 |
|---|---|
| `src/rendering/PAGSurfaceFactory.cpp` | 4 处 `GLDevice::` → `Devices::` |
| `src/rendering/PAGDecoder.h/.cpp` | 3 处调用 + 构造函数签名 + 成员类型 |
| `src/rendering/PAGSurface.h/.cpp` | `void* glRestorer` → `std::unique_ptr<ExternalStateGuard>` |
| `src/rendering/CompositionReader.h/.cpp` | 签名 `GLDevice` → `Device` |
| `src/rendering/graphics/Picture.cpp` | 2 处 + `BackendTextureProxy::sharedContext` → `deviceRef` + `checkContext` |
| `src/rendering/editing/StillImage.cpp` | 1 处 `CurrentNativeHandle` |
| `src/rendering/drawables/OffscreenDrawable.cpp` | 1 处 |
| `src/rendering/drawables/HardwareBufferDrawable.cpp` | 1 处 |
| `src/rendering/drawables/BitmapDrawable.h/.cpp` | 1 处 + 签名 |
| `src/rendering/drawables/DoubleBufferedDrawable.cpp` | 移除多余 include |

**C API**（1 个文件）：

| 文件 | 改动 |
|---|---|
| `src/c/ext/pag_surface_ext.cpp` | 1 处 `GLDevice::Make` |

**pagx**（9 个文件）：

| 文件 | 改动 |
|---|---|
| `src/pagx/PAGImage.cpp` | 1 处 |
| `src/pagx/PAGSurface.cpp` | 2 处 |
| `src/pagx/utils/RasterUtils.cpp` | 1 处 |
| `src/pagx/html/HTMLStaticImageRenderer.cpp` | 1 处 |
| `src/pagx/html/HTMLPlusDarkerRenderer.cpp` | 1 处 |
| `src/pagx/runtime/OffscreenDrawable.cpp` | 1 处 |
| `src/pagx/runtime/Drawable.h` | include 清理 + getDevice 默认实现改走 Devices::AdoptCurrent |
| `src/pagx/runtime/TextureDrawable.h` | 注释更新（只是注释） |
| `src/pagx/runtime/RenderTargetDrawable.h` | 注释更新（只是注释） |

**CLI**（1 个文件）：

| 文件 | 改动 |
|---|---|
| `src/cli/CommandRender.cpp` | 1 处 |

**CMake**（1 个文件）：

| 文件 | 改动 |
|---|---|
| `CMakeLists.txt` | 新增 4 个选项 + 互斥约束 + 透传 tgfx + 新增 `TGFX_USE_*` 编译宏 + Devices.cpp 加入 PAG_FILES |

### 5.4 明确不改的文件

以下文件**保留 GL 特化**，作为对外 API 例外：

- `src/platform/ios/private/PAGSurfaceImpl.mm` —— `EAGLDevice::MakeFrom(EAGLContext*)`（对外 API 是 GL 特化）
- `src/platform/android/JPAGSurface.cpp` `JPAGImage.cpp` —— `GLTextureInfo` 构造（对外 API 是 GL texture ID）
- `src/platform/web/PAGWasmBindings.cpp` —— 同上
- `src/c/ext/pag_surface_ext.cpp` —— `pag_surface_make_offscreen_double_buffered(..., void* sharedContext)`
  的 `sharedContext` 参数就是 GL context native handle，对外 C API GL 特化，内部直接用
  `tgfx::GLDevice::Make(sharedContext)`
- 各平台 `GPUDrawable.*`（`ios/mac/android/ohos/qt/win/web` + `web/pagx/`，共 8 处 include `tgfx/gpu/opengl`）—— GL 平台窗口特化，不属于本次改造范围
- `src/c/ext/egl/pag_egl_globals.cpp` —— `#include "tgfx/gpu/opengl/egl/EGLGlobals.h"`，EGL 全局配置。仅在 `PAG_USE_C AND (ANDROID OR (WIN32 AND PAG_USE_ANGLE))` 条件下编入（见 `CMakeLists.txt` L634-L637），本就不进非 GL 后端构建
- `src/base/utils/TGFXCast.cpp` —— `ToTGFX(BackendTexture/RenderTarget/Semaphore)` 只覆盖 GL（Metal/VK 分支后续再补）
- `include/pag/gpu.h` + `src/rendering/GPUBackend.cpp` —— `BackendSemaphore` ABI 保留 `initGL/glSync`（其它后端 ABI 扩展延后）
- `src/c/pag_backend_texture.cpp`、`pag_backend_semaphore.cpp` —— 保留 GL 特化
- `test/src/base/PAGTest.h`、`test/src/utils/DevicePool.{h,cpp}`、`test/src/PAGDiskCacheTest.cpp`
  —— 这些测试代码直接测试 GL 特定行为（`sharableWith` / `MakeWithFallback` fallback 语义），
  保留 `tgfx::GLDevice` 直接调用是合理的

---

## 六、改造顺序（实施步骤）

按依赖倒序、风险从低到高：

### Step 1：基础设施（新增 Devices）

1. 创建 `src/rendering/gpu/Devices.h`（接口）
2. 创建 `src/rendering/gpu/Devices.cpp`（**只填 GL 分支实现**，`#if defined(TGFX_USE_OPENGL) / #else #error / #endif` 的最小分派；其余 `#elif defined(TGFX_USE_METAL/...)` 分支及其 `#error` 桩留到 Step 4 与 CMake 后端开关一起加入）
3. `src/rendering/utils/GLRestorer.{h,cpp}` 挪到 `src/rendering/gpu/`，改造为 `ExternalStateGuard` 子类（save 签名带上 `tgfx::Context*` 参数）
4. CMake 添加 `Devices.cpp`、`GLRestorer.cpp` 到 `PAG_FILES`；**同步在 CMake 里给 libpag 定义 `TGFX_USE_OPENGL` 编译宏**（`if (PAG_USE_OPENGL) list(APPEND PAG_DEFINES TGFX_USE_OPENGL) endif ()`）——否则 `Devices.cpp` 里 `#if defined(TGFX_USE_OPENGL)` 走不到 GL 分支
5. **编译通过**（未替换任何调用点，只是新增）

### Step 2：核心渲染层去 GL 化

替换约 26 处调用点里的**核心 libpag 部分**（不含 pagx/cli）：

1. `PAGSurfaceFactory.cpp` —— `MakeFrom(BackendRenderTarget)` 与 `MakeFrom(BackendTexture, false)` 用 `Devices::AdoptCurrent()`；`MakeFrom(BackendTexture, true)` 用 `Devices::MakeForAsyncThread()`
2. `PAGSurface.h/.cpp` —— `void* glRestorer` → `std::unique_ptr<ExternalStateGuard>`（构造时 `Devices::MakeExternalStateGuard()` 创建一次，lockContext/unlockContext 调 save/restore）
3. `PAGDecoder.h/.cpp` —— `GLDevice` 签名改 `Device`，`CurrentNativeHandle` 逻辑内嵌到 `Devices::MakeForAsyncThread()`
4. `Picture.cpp` `BackendTextureProxy` —— `sharedContext` 改 `deviceRef`（`Devices::CaptureCurrent()` + `CanSampleFrom()`）
5. `StillImage.cpp` —— 用 `Devices::CaptureCurrent()`；`PAGImage::FromTexture` 隐式推断用 `Devices::MakeForTexture()`
6. Drawable 层（`Offscreen/HardwareBuffer/Bitmap/CompositionReader`）—— 签名和实现改用 `Devices::MakeDefault()`

**编译 + 运行 `PAGFullTest_OpenGL` 全通过**才能进 Step 3。

### Step 3：pagx / cli 去 GL 化

替换剩余的 8 + 1 处调用点。

**编译 + 运行 `PAGFullTest_OpenGL` + `HTMLTest` 全通过**。

### Step 4：CMake 后端开关

1. 新增 `PAG_USE_METAL/VULKAN/D3D12/WEBGPU` 选项
2. 互斥约束
3. 透传到 tgfx
4. 定义 `TGFX_USE_*` 编译宏给 libpag 用

此时 `PAG_USE_OPENGL=OFF` `PAG_USE_METAL=ON` 会因 `Devices.cpp` 的 Metal 分支是 `#error` 桩而**编译失败**——这是预期的，Step 5 之后再实际支持其它后端。

### Step 5（可选，本次不做）：Metal 分支实现

未来单独 PR，只需要动 `Devices.cpp` 一个文件的 `#elif defined(TGFX_USE_METAL)` 分支。

---

## 七、验证策略

### 7.1 编译验证

```bash
./codeformat.sh 2>/dev/null; true
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest_OpenGL
```

### 7.2 测试验证

- 运行 `PAGFullTest_OpenGL`（Metal 后端则运行 `PAGFullTest_Metal`），全部用例通过
- 运行 `HTMLTest`，均值 SSIM / 像素差 / RGB 偏差不退化
- 视觉回归测试：任何截图基准变更都不应该发生（本次是纯重构）

### 7.3 iOS/Android/OHOS 平台验证

- iOS demo / Android demo / OHOS demo 三端运行，PAG 动画渲染正常
- 特别关注 `PAGImage::FromTexture` + `PAGSurface::MakeFrom(BackendTexture)` 的场景（外部 GL 纹理路径）

### 7.4 手动 code review 检查点

改造完成后，`grep -rl "tgfx/gpu/opengl" src/` 的期望命中集合（GL 默认构建下）应**精确等于**以下白名单，多一个或少一个都要排查：

```
src/rendering/gpu/Devices.cpp                       # 胶水层 GL 分支
src/rendering/gpu/GLRestorer.cpp                    # ExternalStateGuard GL 实现
src/c/ext/egl/pag_egl_globals.cpp                   # EGL 全局配置（条件编译）
src/platform/ios/private/PAGSurfaceImpl.mm          # EAGLDevice 对外 API
src/platform/ios/private/GPUDrawable.h              # 平台窗口
src/platform/mac/private/GPUDrawable.h/.mm          # 平台窗口
src/platform/android/GPUDrawable.h                  # 平台窗口
src/platform/ohos/GPUDrawable.h                     # 平台窗口
src/platform/qt/GPUDrawable.cpp                     # 平台窗口
src/platform/win/GPUDrawable.cpp                    # 平台窗口
src/platform/web/GPUDrawable.h                      # 平台窗口
src/platform/web/pagx/GPUDrawable.h                 # 平台窗口
```

- [ ] `grep -rl "tgfx/gpu/opengl" src/` 结果 == 上述白名单
- [ ] `grep -rl "tgfx::GLDevice" src/` 结果是上述白名单的子集（`Devices.cpp`、`GLRestorer.cpp`、`PAGSurfaceImpl.mm` 及使用 `GLTextureInfo`/`GLFrameBufferInfo` 的平台桥接文件；`pag_egl_globals.cpp` 用的是 `EGLGlobals` 不含 `GLDevice`）
- [ ] `include/pag/*.h` 无任何变更（`git diff --stat include/pag/` 为空）
- [ ] 所有平台层公开 API（iOS `+FromCVPixelBuffer:context:`、Android `SetupFromTexture`、Web `_FromTexture`）签名不变

---

## 八、已知限制与后续工作

### 8.1 遗留项（Metal 接入后核对状态）

以下为改造时的遗留项，标注 Metal 后端接入后的实际状态（`[完成]` / `[待完成]` / `[不适用]`）：

1. **[完成] BackendSemaphore ABI 扩展**：`initMetal` / `mtlEvent` / `mtlValue` 已加入 `include/pag/gpu.h`。
2. **[完成] iOS `+FromCVPixelBuffer:context:(EAGLContext*)` 的非 GL 版本**：已新增 `+FromCVPixelBuffer:device:(id<MTLDevice>)`。
3. **[不适用] Android/Web 外部纹理 API 的非 GL 版本**：Android/Web 无 Metal 后端。
4. **[不适用] VulkanImageInfo 无 VkDevice 字段**：Vulkan 后端相关，非 Metal。
5. **[不适用] WebGPUDevice::Make 阻塞异步 Promise**：WebGPU 后端相关，非 Metal。
6. **[完成] 各平台 GPUDrawable 的非 GL 版本**：已新增 `MetalGPUDrawable`（iOS/mac 共用）。
7. **[不适用] Windows 排除 GLRestorer 的历史原因**：与 Metal 无关。
8. **[待完成] Metal/D3D12 多 GPU 场景**：默认走系统默认 device，用户主动选非默认 GPU 时需要 device 注入 API（见第 9 项）。
9. **[待完成] device 注入 API（`SetSharedDevice` 或等价形态）本身**：随首个需要它的后端 PR 定稿其生命周期与线程安全语义。Metal 的多 GPU 场景同样依赖它，优先级低于 Vulkan/WebGPU。
10. **[完成] PAGView 的 Metal 版本**：iOS/mac 的 `PAGView`（UIView/NSView 高层封装）已支持 Metal 后端——复用 `[PAGSurface FromMetalLayer:]` + `PAGAnimator` 动画循环，`layerClass` / `makeBackingLayer` 按后端返回 `CAMetalLayer`，并在 `initPAGSurface` 显式设置 `drawableSize`（bounds × scale）与 Metal device 启动期重试。mac 端额外需 `wantsLayer = YES` 启用 layer backing。已通过 mac / iOS 模拟器 / iOS 真机三端验证（动画循环 + Retina 缩放 + 内容上屏）。

### 8.2 具体后端接入 PR 建议

后续每个后端一个独立 PR，各自的工作量：

| 后端 | 工作量 | 关键改动 |
|---|---|---|
| Metal | 核心已完成，仅剩 device 注入（多 GPU，低优先级） | `Devices.cpp` Metal 分支 + `MetalGPUDrawable` + iOS/mac 平台 API 扩展 + PAGView Metal 版（均已完成）；device 注入 API 待做（见 §8.1 第 8/9 项） |
| Vulkan | 大 | 引入 device 注入 API 并强制要求 + 各平台 Vulkan Drawable + `TGFXCast` VK 分支 |
| D3D12 | 中 | `Devices.cpp` D3D12 分支 + Windows Drawable + `TGFXCast` D3D12 分支 |
| WebGPU | 大 | 引入并强制 device 注入 API + Web 侧 API + 异步初始化处理 |

### 8.3 Metal 剩余待办优先级

| 待办 | 工作量 | 设计复杂度 | 优先级 | 建议 |
|---|---|---|---|---|
| device 注入 API + 多 GPU（§8.1 第 8/9 项） | 中 | **高** | 低 | 延后：进程级全局状态 + 生命周期 + 线程安全语义需精确定稿，且仅多 GPU 边缘场景需要；建议随 Vulkan/WebGPU 一起定稿，避免单独为 Metal 定语义后再改 |

Metal 的其余功能（含 PAGView Metal 版）均已完成，当前唯一剩余待办是 device 注入 API。

---

## 九、决策记录

以下是本方案定稿前的关键决策（供未来查阅）：

| 决策项 | 结论 | 理由 |
|---|---|---|
| 类名 | `pag::Devices` | 简洁；表达"静态工具类"语义，避免与 tgfx::Device 混淆 |
| `AdoptCurrent` 返回值 | 返回 `AdoptedDevice` 结构体 | 比 out 参数清晰 |
| `ExternalDeviceRef` 类型 | 抽象基类 + 各后端强引用子类 | 避免 Metal/D3D12 悬空崩溃 |
| `ExternalStateGuard` 模型 | save/restore 分离、PAGSurface 持有复用 | 避免每帧堆分配 + 虚析构开销 |
| pagx 是否本次改 | 一并改 | 彻底清理 tgfx GL 头 include |
| device 注入 API（`SetSharedDevice`） | 本次不引入，随首个需要的后端 PR 定稿 | 语义随后端而异、进程级全局状态、本次无测试覆盖，避免死接口 |
| `MakeForTexture` 命名 | 从 `MakeCompatibleWith` 改名并注明 GL 忽略参数 | 命名与 GL 实际行为对齐，避免误导 |
| PAGDecoder 非 GL 后端 async 语义 | 退化为 `MakeDefault` | 覆盖 99% 场景 |
| GLRestorer 位置 | 抽象为 `ExternalStateGuard` 子类 | 保持 Drawable / PAGSurface 后端无关 |
| tgfx 编译宏可见性 | libpag 自己定义 `TGFX_USE_*`（方案 B） | 不耦合 tgfx CMake |
| 外部资源跨后端桥接 | 不支持 | 平台系统 SDK 职责，不是 libpag 职责 |
| 编译期后端选择 | 单一后端互斥 | tgfx 已经是这个模型，libpag 对齐 |

---

## 十、参考

- `third_party/tgfx/CMakeLists.txt` —— tgfx 后端 `TGFX_USE_*` 互斥/平台校验逻辑（`FATAL_ERROR` 处）
- `third_party/tgfx/include/tgfx/gpu/Backend.h` —— 通用 `BackendTexture/RenderTarget/Semaphore`
- `third_party/tgfx/include/tgfx/gpu/{opengl,metal,vulkan,d3d12,webgpu}/*.h` —— 各后端 Device 声明
- `include/pag/gpu.h` —— libpag 对外 `Backend/BackendTexture/BackendRenderTarget/BackendSemaphore`
- `src/base/utils/TGFXCast.{h,cpp}` —— libpag ↔ tgfx 类型映射
