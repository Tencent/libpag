# libpag D3D12 后端适配设计方案

## 版本信息

- 文档版本：v1.1（评审修订）
- 日期：2026-11-17
- 基线：`main` @ `9c968baea`
- 分支：`feature/thunderllei_d3d12`
- 状态：**评审问题已纳入方案，关键接口待确认，尚未实现**
- 参考实现：Metal 适配提交 `ba9952640`（#3717）、`c0b63f058`（#3781）、`8a63822b0`（#3798）、`c52be7304`（#3804）、`e0815e9ca`（#3824）、`eb4412aa5`（#3828）

本版不把设备归属、资源状态、队列同步或首次基线建立当作可延后的限制。正文中的新增接口、目标和脚本行为均为拟实施设计，不代表当前代码已支持；v1.0 的评审问题与修订位置见附录 C。

---

## 一、背景与目标

### 1.1 现状

- 固定依赖 `third_party/tgfx` @ `a3349d9a5` 已提供 D3D12 设备、外部资源导入、Win32 交换链和 WARP；tgfx 有独立的 `win-d3d12-warp` CI lane。
- libpag 的 Metal / WebGPU 后端已接入 `src/rendering/gpu/{opengl,metal,webgpu}/`。D3D12 仍被 `CMakeLists.txt:192` 的实现白名单和测试目标选择拒绝，没有对应公共资源类型、后端胶水或窗口实现。
- 这些是对固定代码的静态核查结果。除评审时的 clang-cl 参数语法探针外，尚未做 D3D12 编译、运行或截图验证。

### 1.2 目标

对照 Metal 的职责边界、公共资源包装、窗口接入和测试覆盖落地 D3D12；涉及 D3D12 特有语义时明确设计，不机械复制 Metal 的限制：

1. C++ 支持 D3D12 外部纹理与渲染目标，C API 增加纹理创建/取值及完成等待入口。
2. 默认设备硬件优先、失败回退 WARP；提供固定 WARP 构建用于截图门禁。
3. 同一原生设备上的 libpag 工厂复用同一个 tgfx Device，并解决 PAGX 纹理图像的强生命周期和 contextID 归属。
4. 明确外部资源的合法描述、COM 所有权、COMMON 状态交接、独占区间和双向 GPU 完成顺序。
5. 接通 `GPUDrawable`、Win32Demo 的实际 D3D12 构建与播放路径。
6. 前置 Windows 全模块测试编译能力、基线接受支持，再完成后端测试与 CI；不靠跳过测试或接受空帧取得通过。

### 1.3 非目标与约束

- 不引入 Vulkan，不修改或升级固定 tgfx；本方案的设备身份查询和 CPU 等待使用已有公开接口。
- 首期不扩展 `pag::BackendSemaphore` 的 D3D12 fence ABI；必须提供 §4.5 的正常 CPU completion 路径，不能以此省略跨队列同步。
- 不新增 Windows 硬件视频解码器；复用已有内置 libavc 和外部解码器注册机制。
- 不把宿主显式提供的 tgfx Device 替换成另一个同适配器 Device；该集成路径仍由宿主保持精确的 contextID 和所有权。
- 不自动运行基线生成/接受目标，不手改版本台账。首次接受和缓存制品的来源按 §6.3 处理。

---

## 二、现状盘点

### 2.1 tgfx 能力与四类独立契约

| 能力 | 已有接口 | 对 libpag 的影响 |
|---|---|---|
| 硬件 / WARP | `D3D12Device::Make()` / `MakeWarp()` | 默认设备策略可直接复用，WARP 不是外部资源设备推断的回退 |
| 包装原生设备 | `D3D12Device::MakeFrom(void*)` | 内部持有 COM 引用，但每次创建新的 tgfx Device，没有 D3D12 去重 |
| 外部资源 | `tgfx::D3D12TextureInfo` | 包含 `ID3D12Resource` 和 DXGI 视图格式；公共 PAG 描述的支持范围见 §4.3 |
| 窗口 | `D3D12Window::MakeForHwnd()` / `MakeForComposition()` | 首期接入不透明 HWND 交换链，尺寸来自客户区 |
| 查询目标设备身份 | `Context::gpu()` → `GPU::createTexture()` → `Texture::getBackendTexture()` → `GetDevice()` | 可在已锁定目标 context 中探测一次并缓存，无需新增 tgfx getter |
| 队列完成等待 | `GPU::queue()` → `CommandQueue::waitUntilCompleted()` | 可实现 libpag 的显式 CPU 等待；底层接口返回 void，不能直接当作成功布尔值 |

必须区分以下四件事：

1. **原生设备同一性**决定资源是否能被目标设备使用。比较 canonical `IUnknown` 身份，不能只比较适配器或 LUID。
2. **tgfx Device / contextID 归属与寿命**决定 `TextureImage` 是否能绘制和持有资源。`TextureImage.cpp:83-88` 拒绝不同 contextID；D3D12 Device 析构会释放自身 GPU 资源。
3. **资源状态**决定 barrier 的 StateBefore/StateAfter。固定 tgfx 的 `D3D12Texture.h:66-68` 要求外部交接时处于 COMMON，不能从 `GetDesc()` 查询当前状态。
4. **队列完成顺序**决定读写先后。`flush()`、资源状态正确或同一原生设备都不等于跨队列工作已完成。

### 2.2 libpag 缺口与修改边界

| 编号 | 模块 | 必须补齐的能力 |
|---|---|---|
| G1 | CMake | D3D12 白名单、Windows 校验、系统库、WARP、全模块测试目标 |
| G2 | `include/pag/gpu.h`、`GPUBackend.cpp` | 资源类型、构造/复制/取值、合法性与所有权说明 |
| G3 | C API | D3D12 纹理入口、完成等待、真实 C 消费者的头文件语法检查 |
| G4 | `TGFXCast.cpp`、`cast/d3d12/` | 后端标签转换与资源格式查询；非本后端构建不解引用外来指针 |
| G5 | `rendering/gpu/d3d12/` | 原生身份注册、精确 Device 复用、强生命周期、上下文身份验证 |
| G6 | `Devices.cpp` / `Devices.h` | 显式资源用途验证、外部设备失败不回退到默认设备 |
| G7 | `pagx/PAGImage.*` | 立即导入的纹理图像保活原 Device，并能与目标 Surface 共用同一个 contextID |
| G8 | `PAGSurface.*`、`pagx/PAGSurface.*` | COMMON 交接文档与 CPU completion API |
| G9 | `platform/win/`、Win32Demo 工程 | 正确的编译宏、客户区尺寸、后端参数、库依赖和可观测运行确认 |
| G10 | Windows 测试构建 | Ninja + clang-cl、正确参数转发、统一 Debug/CRT 与子构建工具链 |
| G11 | `Baseline.cpp` | 窄范围引入 tgfx MD5 私有头及匹配静态符号，不扩大公开 include |
| G12 | `TestEnvironment.cpp` | 条件注册 ffavc，保留 Windows 内置 libavc 与 mock 测试 |
| G13 | 脚本、CI、接受流程 | 首次基线可建立，CI 只使用已审核基线，不在待测代码上自建期望值 |

### 2.3 Metal 参照实现

| Metal 实现 | D3D12 对应职责 |
|---|---|
| `gpu/metal/Devices.mm` | `gpu/d3d12/Devices.cpp`；注册、推断、身份校验均在后端胶水内 |
| `cast/MetalTexture.{h,cpp}` + `cast/metal/MetalTexture.mm` | D3D12 格式查询声明、非本后端桩、真实现 |
| `TGFXCast.cpp` / `GPUBackend.cpp` | 同样的资源包装与转换，不把原生 API 散落在共享渲染代码里 |
| `cocoa/private/metal/` | Windows `GPUDrawable` 与 Win32Demo 的后端接入 |
| Metal surface / texture / blend / PAGX 用例 | D3D12 等价行为测试，并增加 COMMON、completion、设备不匹配与图像寿命测试 |
| Metal 缓存隔离和 CI | D3D12 / 固定 WARP 缓存隔离；首次接受不能依赖尚未支持 D3D12 的 main |

---

## 三、总体设计

### 3.1 职责边界与数据流

```text
PAG resource description
  -> TGFXCast (backend tag and native description)
  -> Devices::ResolveForExternalResource (usage validation and device registry)
  -> Drawable / PAGX texture image (strong device ownership)
  -> Context recording / submission
  -> PAGSurface::waitUntilCompleted (CPU completion boundary)

HWND
  -> GPUDrawable
  -> Devices::MakeDefault (same registry)
  -> D3D12Window
  -> Context recording / automatic presentation
```

核心层负责渲染与锁边界，D3D12 胶水负责原生描述/COM 身份/设备注册；窗口层只负责 HWND、尺寸和交换链。PAGX 图像的 Device 保活由图像包装负责，不能仅在设备工厂补一个默认缓存。

### 3.2 后端选择

- `PAG_USE_D3D12` 是 CMake 选项；C++ 源码使用 CMake 已生成的 **`TGFX_USE_D3D12`**，不假定存在同名 `PAG_USE_D3D12` 宏。
- 非 GL 后端互斥的现有规则保留，D3D12 仅允许 Windows。
- `PAG_D3D12_USE_WARP=ON` 固定默认设备为 WARP；OFF 为硬件优先并允许回退。外部资源始终按自己的原生设备推断，推断失败必须返回失败。
- 自动选择模式需要记录实际硬件/WARP 选择；截图门禁与首次接受统一使用固定 WARP，不能将运行时回退产生的像素冒充硬件基线。

### 3.3 必须满足的不变量

1. 所有 libpag 管理的 D3D12 工厂按 canonical `IUnknown` 复用同一个 tgfx Device；该对象及原生身份随实际使用者强引用存活。
2. PAGX 立即导入的纹理图像与目标 Surface 必须具有相同 contextID；相同原生设备不足以替代这一条件。
3. `CaptureFromTexture()` 必须持有有效原生身份；身份获取失败或与目标 context 不匹配时拒绝使用，不能信任空引用。
4. 每次资源交接前，宿主已完成自己的相关 GPU 工作并将所有涉及的 subresource 置为 COMMON；libpag 使用期间不得由另一队列读写或改变状态。
5. `flush()` 仅提交。宿主必须在显式完成等待成功后复用资源；完成后的交接状态为 COMMON，不是交入前的任意状态。
6. 不持有 Device/context 锁调用会内部加锁的 libpag 方法；注册表锁不跨越 GPU 创建、context 加锁、等待或析构。
7. 第一阶段就必须能以 `PAG_BUILD_TESTS=ON` 构建完整测试目标。缺失基线保持失败，不用禁用测试或跳过缺失 key 代替验证。

---

## 四、接口设计

### 4.1 公共 C++ 资源 API

在 `include/pag/gpu.h` 的 `Backend` 末尾追加 `D3D12`，已有枚举值保持不变。资源描述继续采用已确认的指针形态：

```cpp
/**
 * Non-owning description of an external typed 2D D3D12 texture. The caller must keep the
 * resource valid until every object using it is released and its submitted GPU work completes.
 */
struct D3D12TextureInfo {
  /**
   * Pointer to a live ID3D12Resource. Null resources are invalid.
   */
  void* resource = nullptr;
};
```

`BackendTexture` 和 `BackendRenderTarget` 分别增加以下接口和对应 union 成员；构造、复制和取值均不转移原生资源所有权：

```cpp
/**
 * Creates a D3D12 backend description without adopting the native resource.
 * @param d3d12Info the external texture description.
 * @param width the logical width in pixels, greater than zero and no larger than the resource.
 * @param height the logical height in pixels, greater than zero and no larger than the resource.
 */
BackendTexture(const D3D12TextureInfo& d3d12Info, int width, int height);

/**
 * Copies the D3D12 description when this is a valid D3D12 backend texture.
 * @param d3d12TextureInfo non-null destination for the description.
 * @return true on success; false for a null destination or a different backend.
 */
bool getD3D12TextureInfo(D3D12TextureInfo* d3d12TextureInfo) const;

BackendRenderTarget(const D3D12TextureInfo& d3d12Info, int width, int height);
bool getD3D12TextureInfo(D3D12TextureInfo* d3d12TextureInfo) const;
```

渲染目标接口在实际公共头中补齐同等级的参数/返回说明。描述的正尺寸不代表原生资源可用，工厂还必须验证 §4.3 的全部条件；失败返回空对象，不延后到绘制时才发现不可用。

### 4.2 C API 与消费者约束

新增声明分别放在 `pag_types.h`、`pag_backend_texture.h` 和 `pag_surface.h`，实现使用既有 C 包装类，不另建兼容层：

```c
typedef struct pag_d3d12_texture_info {
  void* resource;
} pag_d3d12_texture_info;

PAG_EXPORT pag_backend_texture* pag_backend_texture_create_from_d3d12_texture_info(
    pag_d3d12_texture_info textureInfo, int width, int height);

PAG_EXPORT bool pag_backend_texture_get_d3d12_texture_info(
    pag_backend_texture* texture, pag_d3d12_texture_info* textureInfo);

PAG_EXPORT bool pag_surface_wait_until_completed(pag_surface* surface);
```

- 创建的描述包装通过 `pag_release` 释放；不对调用方传入的资源执行所有权转移。取值函数对空参数、错误后端返回 false。
- 完成等待与 §4.5 的 C++ 方法同义，对空 surface 返回 false。
- C 示例使用 `pag_d3d12_texture_info info = {0};`，不能在结构声明中使用 `nullptr` 或 C++ 默认成员初始化。
- 既有 `pag_types.h` 含 C++ include 和默认初始化，真实 C 消费者检查时需在该公共头源头修复 C/C++ 声明兼容性；不得把消费者改用 C++ 编译来伪装 C API 可用。

### 4.3 资源支持范围与用途验证

资源只存指针意味着**首期仅支持可直接确定视图格式的 typed 资源**。`GetDesc().Format` 不是 typeless 资源的视图选择接口，不能猜测 UNORM/sRGB，也不能提供默认 RGBA 格式绕过失败。

| 项目 | 首期约束 |
|---|---|
| 形状 | `D3D12_RESOURCE_DIMENSION_TEXTURE2D`，`DepthOrArraySize == 1`，单平面 |
| 格式 | R8 UNORM、RGBA8/BGRA8 UNORM 及其 sRGB 变体；具体用途还须受原生格式能力检查约束 |
| 不支持 | typeless、数组、立方体、多平面 YUV、压缩和 HDR 格式；MSAA 资源首期拒绝 |
| 尺寸 | 正整数逻辑尺寸不大于原生宽高，并通过 tgfx 设备上限检查；保留既有逻辑子区域行为 |
| 采样用途 | 不能设置 `D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE`；格式可采样 |
| 渲染用途 | 必须具备 `D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET` 且格式可作为颜色目标 |
| 同时渲染/采样 | 同时满足上述两项；不能把只支持渲染的目标冒充可采样纹理 |
| 状态与寿命 | 按 §4.5 每次以 COMMON 交接并等待完成；原生指针有效，资源不被另一队列并发重用 |

内部格式查询声明改为与公共可变 COM 指针一致，避免通过 const 原生对象调用非 const 的 `GetDesc()`：

```cpp
/**
 * Reads the DXGI format without adopting the resource.
 * @param resource pointer to a live ID3D12Resource, or null.
 * @return its format, or zero for null or a non-D3D12 build.
 */
unsigned GetD3D12TexturePixelFormat(void* resource);
```

`cast/D3D12Texture.cpp` 为非本后端桩，返回零且不解引用资源；`cast/d3d12/D3D12Texture.cpp` 使用 `ID3D12Resource*` 查询，CMake 只选一个实现。tgfx 的 `const void*` 仅表示不通过该描述写入内存，COM 查询需要的指针边界转换应集中在后端内，不向调用方传播不正确的 const 接口。

共享设备接口增加显式用途，所有调用点一次更新，不保留旧签名或多余默认参数：

```cpp
enum class ExternalResourceUsage { Sampled, RenderTarget, SampledRenderTarget };

static bool ValidateExternalResource(const tgfx::BackendTexture& texture,
                                     ExternalResourceUsage usage);
static bool ValidateExternalResource(const tgfx::BackendRenderTarget& renderTarget);
static AdoptedDevice ResolveForExternalResource(const tgfx::BackendTexture& texture,
                                                bool forAsyncThread,
                                                ExternalResourceUsage usage);
static bool IsDeviceHealthy(tgfx::Context* context);
```

PAG/PAGX 纹理 Surface 传 `SampledRenderTarget`，PAGX 纹理图像和 `Picture::BackendTextureProxy` 使用 `Sampled`，render-target 工厂使用 `RenderTarget`。共享代码先校验后端和用途，再解析设备；D3D12 原生检查在后端胶水实现。其他已实现后端补齐接口，保持其原有有效资源行为。

### 4.4 设备注册、身份与生命周期

#### 注册表

在 D3D12 胶水内建立进程共享、受 mutex 保护的弱注册表：

- 原生主键为 `QueryInterface(IUnknown)` 得到的 canonical 身份，不能直接拿任意接口指针或适配器编号作主键。
- `DeviceEntry` 强持有该 COM 身份和一个 `tgfx::D3D12Device`。按原生身份与 contextID 建立弱索引，默认设备槽也为弱引用。
- 返回给 Surface、PAGDecoder 和 PAGX 图像的 `shared_ptr<tgfx::Device>` 使用 entry 的共享所有权，例如 aliasing shared_ptr；只返回 entry 内部的独立 device 指针会破坏注册项寿命。
- 最后一个使用者释放后，GPU 图像/Surface 先释放，再销毁 tgfx Device，最后释放原生身份；过期弱项可清理，不能让注册表永久保活所有外部设备。
- 候选 Device 和身份探针在注册表锁外创建，随后二次检查并唯一发布。竞争中未采用的候选在锁外销毁，禁止注册表锁与 Device/context 锁形成反向等待。

#### 设备工厂伪代码

```text
MakeDefault():
  reuse the live default entry if present
  create a hardware candidate, or WARP after hardware failure
  use MakeWarp directly when PAG_D3D12_USE_WARP is enabled
  probe the candidate's native identity once
  get or publish the canonical DeviceEntry and cache its weak default reference
  return shared ownership of that entry's exact tgfx Device

MakeForTexture(texture or renderTarget):
  query and validate the native resource description
  GetDevice(ID3D12Device), then QueryInterface(IUnknown)
  reuse the entry with that canonical identity
  otherwise create D3D12Device::MakeFrom outside the registry lock and publish once
  return null on any failure; never select another default adapter

MakeForAsyncThread():
  return MakeDefault()

MakeForAsyncThread(texture):
  return MakeForTexture(texture)

AdoptCurrent():
  return an empty AdoptedDevice

MakeForSharedContext(sharedContext):
  return MakeDefault() only when sharedContext is null
  reject non-null GL-shaped context handles

CaptureFromTexture(texture):
  validate Sampled usage and strongly capture canonical native device identity
  return null on failure

RequiresCapturedIdentity():
  return true

CanSampleFrom(context, deviceRef):
  reject null context, missing identity, or a different backend
  compare the captured native identity with the context's cached native identity
  reject mismatches or identity lookup failure
```

默认回退用项目已有 `LOGI` 记录实际选择，最终创建失败用 `LOGE`；不使用不存在的 `LOGW`。共享 `ResolveForExternalResource()` 移除统一的异步失败默认回退；OpenGL 纹理 worker 的既有默认回退移入其后端实现，Metal/WebGPU 保留自身既有策略，D3D12 则严格传播失败。

#### 目标 context 的身份探测

注册表管理的 context 直接复用 entry 身份。宿主显式传入的 tgfx Device 在接入边界登记其所有权/身份元数据，不被替换成注册表中的另一个 Device。必要时在**已经锁定的目标 context** 内通过公开 `GPU::createTexture()` 创建一次 1×1 RGBA8 可采样纹理，再从该资源的 `GetDevice()` 得到身份。

探针不新建 context，不调用会隐式提交的 `Surface::getBackendTexture()`；身份随 Device 生命周期缓存并强持有 COM 引用，不每帧分配探针。无法建立安全所有权或查询失败的集成必须显式失败，而不是接受空身份。

#### PAGX 图像

`pagx::PAGImage::MakeFromTexture()` 保留立即导入方式，但新增强 Device 所有权成员，使用注册表返回的同一共享 owner。成员声明顺序保证 `_tgfxImage` 在 Device owner 之前析构。目标 PAGX Surface 的外部工厂使用同一 entry，从而满足 `TextureImage` 的 contextID 比较。

仅保活 image 或仅缓存默认设备都不够。宿主自管 tgfx Image 的 `MakeFrom(image)` 路径仍要求宿主保活其原 Device，并在匹配 context 上使用；不得承诺该图像能自动迁移到任意同适配器 context。

### 4.5 COMMON 交接与显式 CPU completion

推荐新增 `pag::PAGSurface::waitUntilCompleted()`、`pagx::PAGSurface::waitUntilCompleted()` 和 §4.2 的 C 转发，保持已有 `flush()` / scene 绘制的异步提交语义。新接口需确认后再编码：

```cpp
/**
 * Submits pending commands and blocks until this surface's device queue finishes prior work.
 * Does not advance playback or draw another frame. Manages the device lock internally.
 * @return true after completion and a successful device-health check; false if the device
 * cannot be locked, is lost, or completion cannot be established.
 */
bool waitUntilCompleted();
```

实现复用 `Context::flushAndSubmit()` 和 `CommandQueue::waitUntilCompleted()`，在 Surface 的锁边界内保活 Device。**底层等待返回 void，不能直接返回 true**：在仍已锁定的 context 上调用后端胶水 `Devices::IsDeviceHealthy(context)`，D3D12 通过登记的原生设备检查 `GetDeviceRemovedReason()`；随后解锁，在同线程重新锁定 Device，检查 tgfx 是否已标记 context lost，再释放锁。两项检查任一失败都返回 false，不允许宿主按正常 COMMON 资源继续使用。健康接口对空 context、身份不可解析或后端不匹配也返回 false，其他后端补齐自己的实现；不直接访问 D3D12 私有 GPU 字段。

交接顺序必须同时满足状态与完成两类条件：

```text
Host producer:
  finish producer writes and transition all used subresources to COMMON
  signal and wait for the host producer's own queue completion

PAG consumer / producer:
  construct or reuse the validated resource wrapper
  flush the player or draw the scene
  call the surface's waitUntilCompleted()
  do not let host or another queue access the resource before successful completion

Host next consumer:
  start from COMMON after successful PAG completion
  transition to its required state and submit its work
  complete that work and return to COMMON before the next PAG handoff
```

同一注册 Device 的 libpag 提交可复用其队列顺序，但宿主独立队列仍需要上述边界。资源销毁、再次交给另一 Surface 和 readback 也必须遵守此顺序。`readPixels()` 的隐式阻塞或析构等待不作为普通 `flush()` 已完成的证据。

等待 API 只保证该设备队列此前提交的工作完成，不等待宿主尚未提交的命令，也不保证一帧视觉内容正确。调用方不能持有同一 Device/context 锁再调用该方法。宿主使用自有 Device + `pagx::Record()` 的路径可以自行同步提交，但不能替代普通外部纹理工厂的完成 API。

### 4.6 Windows 窗口接口

`GPUDrawable` 成员统一为 `shared_ptr<tgfx::Window>`；D3D12 编译分支使用 **`TGFX_USE_D3D12`**，从注册表获得默认 Device 后交给 `D3D12Window::MakeForHwnd()`。`static_pointer_cast` 仅基于已验证的编译期后端不变量，不用于绕过类型错误。

D3D12 尺寸使用 `GetClientRect()`，零尺寸/最小化不创建目标，恢复与缩放后使 Surface 尺寸缓存失效并重新获取 backbuffer。`present()` 保持由 tgfx 提交自动驱动。

Win32Demo 的 `WinPAGDemo.vcxproj` 与 `.sln` 必须一起纳入：后端参数传给预构建命令，按后端隔离库输出，显式链接 D3D12 系统库，仅 GL/ANGLE 配置复制 ANGLE 文件。启动日志和测试记录明确确认实际为 D3D12，不能以默认 OpenGL 播放成功作为验收。

### 4.7 构建选项

| 选项 | 默认 | 作用 |
|---|---|---|
| `PAG_USE_D3D12` | OFF | 选择唯一 D3D12 GPU 后端，仅允许 Windows |
| `PAG_D3D12_USE_WARP` | OFF | ON 固定默认 Device 为 WARP；OFF 硬件优先、失败回退 |
| `PAG_BUILD_TESTS` | 按现有规则 | D3D12 验证必须 ON，启用所有关联模块 |
| `PAG_USE_C` | 按现有规则 | C API 验证配置必须 ON，不能遗漏生产者/消费者覆盖 |

---

## 五、实现细节

### 5.1 文件变更清单

以下是后续编码的范围，本次文档修订不修改这些代码或脚本。测试框架和接受流程不是核心实现之后的可选附件，应随首个可验证实现一起落地。

#### 核心、资源与完成边界

| 文件 | 变更 |
|---|---|
| `include/pag/gpu.h`、`src/rendering/GPUBackend.cpp` | D3D12 枚举/POD、构造、复制、取值和完整资源契约 |
| `include/pag/pag.h`、`src/rendering/PAGSurface.cpp` | 显式完成等待、context 锁和设备健康检查；更新外部资源接口文档 |
| `include/pag/c/pag_types.h`、`pag_backend_texture.h`、`pag_surface.h` | C 资源类型、取值/创建、完成等待及消费者声明兼容性 |
| `src/c/pag_backend_texture.cpp`、`src/c/pag_surface.cpp` | 转发既有包装类，不把原生资源释放权交给 POD |
| `src/base/utils/TGFXCast.cpp` | D3D12 双向转换，不在通用转换里替代使用方的用途验证 |
| `src/base/utils/cast/D3D12Texture.{h,cpp}`、`cast/d3d12/D3D12Texture.cpp` | 格式声明、非本后端安全桩与真实查询 |
| `src/rendering/gpu/d3d12/Devices.cpp` | DeviceEntry、弱注册表、原生身份、用途校验、设备健康、默认策略 |
| `src/rendering/gpu/Devices.{h,cpp}` | 显式用途接口、统一验证边界、移除统一异步失败回退 |
| `src/rendering/gpu/opengl/Devices.cpp`、`metal/Devices.mm`、`webgpu/Devices.cpp` | 补齐共享接口；OpenGL 自行保留既有纹理 worker 默认回退 |
| `src/rendering/PAGSurfaceFactory.cpp`、`src/rendering/graphics/Picture.cpp` | 所有 PAG 外部 Surface/图像按用途验证，不能遗漏延迟导入路径 |
| `include/pagx/PAGImage.h`、`src/pagx/PAGImage.cpp` | 纹理图像强 Device owner；同 contextID 与图像先析构规则 |
| `include/pagx/PAGSurface.h`、`src/pagx/PAGSurface.cpp` | 用途解析、完成等待、外部资源说明 |
| `src/pagx/runtime/Drawable.cpp`、`include/pagx/tgfx.h` | 宿主 Device 接入时登记所有权元数据，探测在已锁定 context 上进行，不隐式嵌套加锁 |
| `CMakeLists.txt` | 白名单、平台/选项校验、链接、源码选择、工具链与完整测试目标 |

#### Windows 窗口与 Demo

| 文件 | 变更 |
|---|---|
| `src/platform/win/GPUDrawable.{h,cpp}` | Window 基类、正确宏、注册 Device、客户区尺寸与尺寸缓存失效 |
| `win/Win32Demo/WinPAGDemo.vcxproj`、`win/Win32Demo.sln` | 明确后端配置、预构建参数、独立库输出和系统依赖 |
| `win/Win32Demo/pag_engine/PAGEngineImp.cpp` | 冒烟确认所选后端、窗口缩放与资源释放顺序；复用既有播放器流程 |

#### 测试、脚本与集成

| 文件 | 变更 |
|---|---|
| `test/src/utils/Baseline.cpp` | 窄范围使用 tgfx MD5 私有头，不改比较结果或接受策略 |
| `test/src/base/TestEnvironment.cpp` | 按实际产物条件注册外部 ffavc，保留内置 libavc |
| `test/src/GPUBackendTest.cpp`、`PAGXRuntimeTest.cpp`、`PAGDiskCacheTest.cpp` | 资源往返、PAGX 设备归属/寿命、异步和真实视频回归 |
| `test/src/PAGD3D12SurfaceTest.cpp`、`PAGD3D12TextureTest.cpp` | 状态交接、completion、格式/用途/设备错误和图像渲染 |
| `test/src/PAGBlendTest.cpp` | D3D12 等价 blend 像素用例 |
| `VideoSequenceFallbackTest.cpp`、`PAGANRRegressionTest.cpp` | 保留现有 mock 覆盖，不因 Windows 或缺 ffavc 整块跳过 |
| `autotest.sh`、`update_baseline.sh`、`accept_baseline.sh` | 后端参数、统一 Windows 工具链、可信缓存导入与受控接受路径 |
| `.github/workflows/{build,autotest}.yml` | 编译与运行分开记录，固定 WARP 完整套件读取已审核基线 |
| `.codebuddy/skills/accept-baseline/SKILL.md`、`.codebuddy/rules/Test.md` | Windows 参数、首次接受/缓存制品和后端规则；不解除接受限制 |
| `win/README.md`、`README.md` | 实际构建命令、限制、硬件/WARP 与宿主交接示例 |

### 5.2 CMake 与工具链

后端白名单追加 D3D12，并在配置阶段拒绝非 Windows 或脱离 D3D12 的 WARP 选项。`TGFX_USE_D3D12` 继续由现有后端枚举生成，不额外定义同名 `PAG_USE_D3D12` C++ 宏。

```cmake
set(PAG_IMPLEMENTED_GPU_BACKENDS OPENGL METAL WEBGPU D3D12)

if (PAG_USE_D3D12 AND NOT WIN32)
    message(FATAL_ERROR "PAG_USE_D3D12 is only supported on Windows.")
endif ()

option(PAG_D3D12_USE_WARP "Force the D3D12 backend onto WARP" OFF)
if (PAG_D3D12_USE_WARP)
    if (NOT PAG_USE_D3D12)
        message(FATAL_ERROR "PAG_D3D12_USE_WARP requires PAG_USE_D3D12=ON.")
    endif ()
    list(APPEND PAG_DEFINES PAG_D3D12_USE_WARP)
endif ()
```

GPU 后端依赖不能只挂在原生窗口条件内：D3D12 离屏设备和预编译 tgfx 模式也需要正确链接。D3D12 配置不再进入 `opengl32` / ANGLE 分支：

```cmake
if (PAG_USE_D3D12)
    find_library(D3D12_LIB d3d12 REQUIRED)
    find_library(DXGI_LIB dxgi REQUIRED)
    find_library(D3DCOMPILER_LIB d3dcompiler REQUIRED)
    find_library(DCOMP_LIB dcomp REQUIRED)
    list(APPEND PAG_SHARED_LIBS ${D3D12_LIB} ${DXGI_LIB} ${D3DCOMPILER_LIB} ${DCOMP_LIB})
    list(FILTER SRC_FILES EXCLUDE REGEX "/base/utils/cast/D3D12Texture\\.cpp$")
endif ()
```

测试使用 **x64 Windows SDK 环境 + clang-cl + Ninja + 单配置 Debug**。vendor/tgfx 子构建也须继承同一工具链、build type 和 CRT；独立构建目录与缓存键包含这些配置，不混用已生成的 VS 工程或 Release vendor 产物。

评审探针已确认 clang-cl 忽略裸 `-fno-access-control`。修改现有参数追加位置，而非重复追加一个有效参数保留旧错误项：

```cmake
list(APPEND PAG_TEST_COMPILE_OPTIONS ${PAG_COMPILE_OPTIONS})
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang"
        AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    list(APPEND PAG_TEST_COMPILE_OPTIONS /clang:-fno-access-control)
elseif (MSVC)
    message(FATAL_ERROR "Windows private-access tests require the clang-cl frontend.")
else ()
    list(APPEND PAG_TEST_COMPILE_OPTIONS -fno-access-control)
endif ()
```

所有 GNU 专用 coverage/诊断参数同样按 frontend 审核，不只看 compiler ID；不得为了编译通过删掉测试断言或改变私有访问对象。统一使用 `cmake --build ... --parallel`，不向 MSBuild 透传 `-j`。编译后必须执行测试程序，不能仅以 target 构建成功宣称测试通过。

### 5.3 MD5 与软件解码依赖

- `tgfx::MD5::Calculate` 的声明在 `third_party/tgfx/src/core/utils/MD5.h`，是私有头。只给 `Baseline.cpp` 或测试辅助对象添加 `${TGFX_DIR}/src`，不把 tgfx 私有目录加入全库公开 include。
- 固定 tgfx 静态库已有 MD5 实现，Windows 通过 wincrypt 使用已链接的 Advapi32；不重复实现 MD5。`TGFX_LIB + TGFX_INCLUDE` 的测试模式须额外校验匹配源码私有头和静态符号，缺失则明确失败。
- `TestEnvironment` 以独立测试 capability 标识外部 ffavc 产物是否可注册，不能使用是否启用 `PAG_USE_FFAVC` 推断所有软件解码能力。
- Windows 已有 `PAG_USE_LIBAVC=ON` 与 `SoftAVCDecoder` 回退，WARP lane 显式保证该选项。mock 解码测试全部保留，真实视频/缓存测试增加首帧非空与预期像素断言，禁止接受缺失视频的空帧。

### 5.4 资源转换和共享行为

`TGFXCast` 按后端标签复制原生描述，真实 D3D12 构建查询 typed 格式；非 D3D12 构建保留标签但不访问外来资源，由消费者拒绝不匹配。`ToPAG` 不丢失 D3D12 标签/尺寸/指针，零格式和非法用途不能悄悄映射到默认颜色。

所有 Surface 工厂先通过 `ValidateExternalResource`，图像路径按采样用途验证。外部资源设备解析失败不得由共享代码补一个默认 Device；这一修正同时覆盖 PAG、PAGX、异步线程和延迟图像导入。

### 5.5 窗口与异步线程

D3D12 不存在 thread-local current context。PAGDecoder 和外部纹理 worker 使用注册表的同一 Device，其 mutex 序列化 context 操作；不为追求并发创建新的 tgfx context 后继续共享立即导入图像。

`GetClientRect()` 与交换链尺寸一致；GL 路径不借此次修改改变现有窗口行为。窗口缩放、最小化恢复、关闭和 Device 错误路径必须独立验证；不得将 WGL 创建失败替代 D3D12 支持。

### 5.6 后端命名与缓存隔离

| 模式 | `PAG_BACKEND_NAME` | 目标后缀 | 缓存目录 |
|---|---|---|---|
| 默认自动选择，可能回退 WARP | `d3d12` | `D3D12` | `test/baseline/.cache/d3d12` |
| 固定 WARP，截图门禁模式 | `d3d12-warp` | `D3D12` | `test/baseline/.cache/d3d12-warp` |

版本台账和截图输出仍共享。缓存/制品记录实际设备模式、参考提交、tgfx 版本和工具链；不在同一目录并行运行不同后端，也不把缓存命中等同于版本已接受。

---

## 六、测试与验证

### 6.1 前置条件与验证边界

1. Windows 测试环境明确使用 x64 Windows SDK/MSVC 标准库环境、PATH 中的 clang-cl/Ninja；configure、vendor、tgfx 和接受脚本采用同一单配置 Debug。不把 Ninja 与已有 VS build 目录混用。
2. 第一阶段启用 `PAG_BUILD_TESTS=ON`，构建 PAGX/CLI/SVG/HTML/PPT/VideoExporter 全部关联模块；C API 验证额外启用 `PAG_USE_C=ON`。不能仅编译核心库宣称完整适配。
3. MD5 私有 include/静态符号和外部 ffavc capability 按 §5.3 前置解决，Windows 内置 libavc 保持 ON。已有 mock 用例不跳过。
4. Device 复用后必须验证文字 atlas 与测试顺序稳定性：独立像素测试、完整套件及不同顺序的输出应一致。若不一致，先定位并确认复用现有隔离机制的方案；禁止靠接受抖动基线、关闭断言或照抄 Metal 私有 hook 绕过。
5. 所有数值测试尽量使用整数；像素比较使用 `PAG_PIXEL_TEST` / `PAGX_PIXEL_TEST` / `CLI_PIXEL_TEST`。缺失基线或行为失败如实报告，不重复运行同一失败命令。
6. “成功构建”“行为测试通过”“截图基线已接受”“完整套件通过”分别记录，不能相互替代。本文件中的命令需在实现完成后执行，本次修订不执行它们。

### 6.2 必需用例

| 用例组 | 验收条件 |
|---|---|
| `GPUBackendTest` D3D12 往返 | 标签/尺寸/指针复制正确，空参数拒绝；非 D3D12 构建不解引用外来句柄 |
| 资源验证 | typed RGBA8/BGRA8/R8 与 sRGB 的合法用途通过；typeless、错误形状、非法尺寸、MSAA、缺用途标志和后端不匹配拒绝 |
| Device 注册 | 同原生身份返回同一 Device/contextID，多线程竞争不发布重复对象，最后 owner 释放后可回收 |
| 原生身份 | 同设备允许；不同 canonical 身份拒绝，不能因同适配器而放行。逻辑负例必须覆盖，硬件/WARP 双设备集成仅在设备实际可用时执行并记录覆盖范围 |
| PAGX 纹理图像寿命 | 原 Surface/临时 owner 释放后图像仍可在匹配 Surface 绘制；保活与 contextID 两个条件都被断言 |
| `PAGD3D12SurfaceTest` | 外部纹理/渲染目标、TopLeft/BottomLeft、像素回读；不要求 render-target-only 描述一定可导出可采样纹理 |
| 前后纹理 | 在可采样纹理或双缓冲路径验证 `getFrontTexture` 往返，不用错误用途掩盖失败 |
| COMMON 双向交接 | 宿主写入并转 COMMON、完成后 PAG 使用；PAG 完成等待后宿主从 COMMON 转入读取状态，下一轮再以 COMMON 交入 |
| 独立队列 completion | 不借 `readPixels()` 隐式等待证明正确性；使用新等待接口后，再由宿主独立队列读/写并校验内容 |
| 设备失败 | 已知设备丢失/无法锁定时完成等待返回 false，外部解析失败不切换默认适配器 |
| 异步线程 | `forAsyncThread=true` 仍匹配原生设备和注册 Device，不以创建另一个 context 冒充共享 |
| `PAGBlendD3D12Test` | 对应 Metal 的 CopyDstTexture / TextureBottomLeft / BothBottomLeft |
| `PAGXRuntimeTest` | 外部 texture/render target、图像使用、host-owned Device 的精确归属 |
| 视频/磁盘缓存 | libavc 真实首帧与后续帧内容、PAGDecoder 和缓存；保留 mock fallback/ANR 回归 |
| C 消费者 | 新声明按 C 编译，创建/取值/等待调用按正确 ABI 链接；不把 C++ consumer 当作 C 验证 |
| Win32Demo | 明确 D3D12 日志、播放、缩放、最小化恢复、关闭/释放，GL 后端单独验证 |

COMMON 是调用方交接契约，不能通过 `GetDesc()` 静态检查当前状态。合法交接测试配合 D3D12 调试层确认无 barrier 错误；不能在生产工厂里假装能读取任意外部资源的实时状态。

### 6.3 首次基线接受与可信缓存

v1.0 推荐先调用 `update_baseline.sh` 的流程撤销：当前脚本固定切 `origin/main`，该 main 没有 D3D12 目标；而直接或间接运行 `UpdateBaseline_*` 也不能绕过项目的接受限制。

首期推荐流程如下，缓存发布方式需按 §10 确认：

1. 在同一实现分支完成测试目标、Windows 工具链、D3D12 接受参数和缓存制品支持。先做全模块编译/行为验证，再运行完整测试观察实际截图；新 key 未接受时保持失败，不自动跳过。
2. 用户审核实现及截图，并**主动执行 `/accept-baseline`**，明确选择固定 WARP。该受控流程在当前已审核分支构建/运行，不要求先切到不支持该后端的 main。
3. 受控流程按现有完整套件检查与版本合并规则完成接受，产出与已接受台账匹配的 MD5/cache 制品。助手不独立执行底层接受/生成脚本，不手改或覆盖任何 `version.json`。
4. 制品记录审核过的参考实现提交、台账校验值、后端、tgfx/vendor 版本、工具链/CRT、测试字体资源和设备模式。发布到用户确认的可信存储；不能从任意待测 PR 自动接收期望 MD5。
5. CI 从可信制品或匹配缓存导入期望值，检查元数据后运行待测代码的完整套件。缓存缺失、不匹配或首次制品尚未提供时明确失败/待建立，不在待测 HEAD 上生成自己的期望结果。
6. main 合入后仍区分“生成已审核参考的本地缓存”与“接受新的像素变化”。新的接受必须再次由用户主动发起；共享台账和截图目录按后端串行操作。

D3D12 的 `update_baseline.sh` 分支只检查或导入已审核缓存，不自动 checkout/stash main、构建或运行生成目标。`autotest.sh` 不再无条件进入旧的 main 更新路径。首次审核/制品和测试支持均不能晚于依赖它们的 CI 门禁。

### 6.4 待实施的编译与运行命令

以下在仓库根目录执行。先在 Git Bash 按项目规则运行格式化；仅文档变更无需运行代码格式化：

```bash
./codeformat.sh 2>/dev/null; true
```

随后在已初始化 x64 SDK、INCLUDE/LIB 和 clang-cl/Ninja PATH 的开发者 PowerShell 中执行。明确使用当前固定 tgfx 源码，避免首轮验证误用另一工具链的预编译缓存：

```powershell
$env:CC = 'clang-cl'
$env:CXX = 'clang-cl'
cmake -S . -B cmake-build-d3d12-warp -G Ninja `
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl `
    -DCMAKE_BUILD_TYPE=Debug -DPAG_USE_D3D12=ON -DPAG_D3D12_USE_WARP=ON `
    -DPAG_BUILD_TESTS=ON -DPAG_USE_C=ON -DPAG_USE_LIBAVC=ON `
    -DTGFX_DIR=third_party/tgfx
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build cmake-build-d3d12-warp --target PAGFullTest_D3D12 pagx-cli --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

完整运行前，按 §6.3 提供匹配的已审核 WARP 缓存；首次未接受时允许观察失败，但必须明确报告缺失 key 而非声称通过：

```powershell
& './cmake-build-d3d12-warp/PAGFullTest_D3D12.exe' `
    --gtest_output=json:cmake-build-d3d12-warp/PAGFullTest.json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

### 6.5 回归矩阵

| 检查项 | 通过标准 |
|---|---|
| D3D12 固定 WARP | 上述完整编译、行为/像素/视频测试全部通过，报告实际 backend 和基线来源 |
| 默认设备策略 | 非固定 WARP 独立目录验证硬件优先与回退日志；不能将该模式产物写入固定 WARP 缓存 |
| CLI | 构建 `pagx-cli`，运行图形/图片/字体/动画样例，输出实际内容正确，不仅检查文件存在 |
| Win32Demo | 完成 §4.6 的工程适配后实际构建/运行，检查后端日志；不能用旧 OpenGL 预构建证明通过 |
| OpenGL | 独立目录 `PAG_BUILD_TESTS=ON` 的完整编译和受影响测试；异步默认回退保持原行为 |
| Metal | 共享接口、MD5 和图像所有权有变更，必须走 mac Metal 完整测试，不声称 Apple 不受影响 |
| 预编译 tgfx | 检查公共和测试私有头版本、静态符号与系统库；缺依赖不静默排除测试模块 |

---

## 七、CI、脚本与文档

- 三条脚本统一 Windows x64 环境、clang-cl、Ninja、Debug、后端独立 build 目录和 `.exe` 路径；沿用 tgfx Windows lane 的环境准备思路，不照搬 macOS shell 的 GNU coverage/MSBuild 参数。
- `USE_D3D12` 表示默认自动选择，`USE_D3D12_WARP` 表示固定 WARP；接受和像素门禁以固定 WARP 为准。构建脚本参数与 CMake、缓存命名一致。
- `accept_baseline.sh` 的 Windows 后端支持只由用户主动 `/accept-baseline` 的受控流程使用；无直接执行或另一脚本间接生成的捷径。
- D3D12 `update_baseline.sh` / autotest 路径按 §6.3 导入可信缓存，冷缓存不能自动采用待测代码或缺该后端的 main 来建立期望值。
- `win_d3d12_warp` 正式运行完整套件并上传诊断/截图，报告检查到的参考版本与环境。编译任务和运行任务分开；正式门禁不得因基线未建立自动绿灯。
- Windows coverage 单独按 LLVM 工具链配置；不复制旧 `-fprofile-arcs`/gcov 命令，不以 coverage 暂未接入为理由跳过测试。
- `.github/workflows/build.yml` 增加 D3D12 全模块编译与 Demo 产物验证；后端测试源码在不支持的平台过滤，但通用 mock/软件解码测试不能按 Windows 整体过滤。
- `win/README.md` 说明构建环境、actual backend、CPU completion、COMMON、所有权和支持范围。项目测试规则补充 D3D12/固定 WARP；Metal 过渡说明只在核实已接受 key 和 CI 行为后更正，不解除基线接受限制。

---

## 八、实施顺序与合入门禁

推荐在一个完整 feature PR 内按三阶段实施，而不是把测试/接受脚本留到核心合入之后。以下是实施阶段，不自动代表三个可独立合入的 PR：

| 阶段 | 内容 | 阶段验收 |
|---|---|---|
| I：可编译的完整框架 | 设备注册/强寿命、用途验证、COMMON/完成 API、公共类型、Windows 窗口接入；同时修复测试工具链/MD5/解码 capability，提供 D3D12 测试目标和接受入口 | `PAG_BUILD_TESTS=ON` 全模块编译，资源/错误路径行为验证；基线尚未建立时明确报告，不能声称完整测试通过 |
| II：行为与像素覆盖 | PAG/PAGX/异步/C API、注册与图像寿命、独立队列交接、typed 资源错误、视频与 blend；Win32Demo 后端/库路径适配 | 必需行为用例通过，实际窗口确认为 D3D12；完整测试输出可供用户逐项审核 |
| III：受控接受与正式 CI | 用户 `/accept-baseline`、可信 WARP 缓存制品、CI 导入与完整运行、文档/规则同步 | 已接受基线的完整 WARP 套件通过，OpenGL/Metal 受影响路径不回归，实际编译和运行报告均可审查 |

若必须拆为多个 PR，首个暴露 D3D12 的 PR 仍须包含可编译的完整测试框架、必要的资源正确性和接受支持；任何新像素变化在其合入前完成用户审核。不能用“后续 PR 补同步/测试/基线”满足本次完成度，也不能让接受流程依赖尚未合入的下一 PR。

---

## 九、明确限制与剩余验证风险

### 9.1 CPU 等待与性能

首期 completion 是设备队列范围的 CPU 阻塞，不是细粒度跨队列 fence，也不代表屏幕已经完成呈现。窗口交换链清理由 tgfx 的窗口生命周期处理，不能用一次资源 completion 代替窗口的呈现/销毁规则。若实际性能要求必须使用 GPU semaphore，应先重新确认公共 fence 接口方案，不静默删去等待。

### 9.2 设备、并发和丢失

同原生设备共享 tgfx Device 是正确性要求，其 context mutex 会序列化工作。注册表、别名 owner、宿主元数据和析构顺序需通过竞争/寿命测试。设备丢失后拒绝继续使用旧资源或旧 PAGX 图像，不自动换一个 Device/contextID 后假装仍可共享。

### 9.3 资源描述与调用方责任

首期不支持 typeless/数组/MSAA 等未列入契约的资源，不能通过猜格式或改尺寸绕过。调用方必须传入有效 COM 资源并遵守 COMMON/独占/完成顺序；静态描述检查无法验证任意地址有效性或外部队列的实时状态。Device 同一性、contextID 和同步缺失均不再是可容忍限制。

### 9.4 Windows 硬件缓冲

Windows `HardwareBufferRef` 是 `ID3D11Texture2D*`。固定 tgfx 有 D3D11 共享纹理的采样导入，但 `D3D12HardwareTexture.cpp:59-63` 拒绝导入纹理作为 RENDER_ATTACHMENT。普通 D3D12 渲染 Surface 不会因此获得可导出的 D3D11 硬件缓冲；不能把采样互操作等同于 Surface 支持该渲染入口，更不能强转成 `ID3D12Resource*`。

首期对此明确报告能力限制。若目标包含 D3D11 硬件缓冲渲染/导出，需要先确认单独的 tgfx 能力变更；该要求不能靠返回一个实际无法绘制的 Surface 满足。

### 9.5 环境与基线

固定 WARP 降低硬件差异，但不保证不同 Windows/SDK/字体/编译环境的像素必然一致。接受制品与 CI 环境必须匹配，元数据检查不通过时不能重建“更适合当前结果”的期望值。制品的可信发布来源和首个 runner 环境仍需明确。

WARP 的完整套件可能很慢；可以优化缓存和工作流耗时，但正式后端门禁保留完整测试，不能仅编译、缩小用例或跳过缺失 key。

### 9.6 固定依赖与实施验证

当前方案基于固定 tgfx 的公开能力和两处明确的测试私有依赖（MD5、既有 Metal 测试支持），不新增 D3D12 私有 GPU 访问。如果编译、atlas 稳定性或错误观察发现现有能力不够，应停下说明根因并重新确认范围；不擅自升级 `DEPS`、做强制类型转换或削减覆盖。

---

## 十、编码前需确认的设计选择

| 项目 | 修订建议 / 已确认边界 |
|---|---|
| 公共纹理描述 | 保留用户选择的 `void* resource`，typed 支持范围与错误拒绝规则按 §4.3 |
| 默认设备 | 保留用户选择的硬件优先 + WARP 回退；固定 WARP 采用编译期开关用于门禁 |
| 注册与图像寿命 | 采用 canonical 身份弱注册表、共享 entry owner、PAGX 图像强 Device owner；编码前确认 |
| completion API | 推荐两类 `PAGSurface::waitUntilCompleted()` 及 C 转发，首期不扩展 fence ABI；接口与 CPU 阻塞语义需确认 |
| 测试工具链 | 推荐 Windows x64 + Ninja + clang-cl + Debug，正确转发参数；完整测试框架不得延期 |
| 首次基线制品 | 按 §6.3 由用户受控接受产出；需要指定与 CI 匹配的产出环境、可信存储和发布方式，不假定这些设施已经存在 |
| 合入拆分 | 推荐完整单 PR、三阶段实施；如需拆分，逐个满足 §8 的前置依赖与合入门禁 |
| 硬件缓冲范围 | 首期不增加 D3D11 硬件缓冲的 D3D12 渲染/导出；若属必需能力，先确认独立 tgfx 变更 |

这些是修订后的推荐设计，不是代码实现已获确认或测试已经通过。本轮只修订本文件；确认后再进行编码，不自动执行截图接受。

---

## 附录 A：Metal ↔ D3D12 对照

| 概念 | main 的 Metal | 拟接入的 D3D12 |
|---|---|---|
| 公共纹理描述 | `MtlTextureInfo` | `D3D12TextureInfo`，只存资源指针且限定 typed 支持范围 |
| 格式 | `MTLTexture.pixelFormat` | 验证后的 `GetDesc().Format`，不猜 typeless 视图 |
| 默认设备 | `MetalDevice::Make()` | 硬件优先 + WARP 回退，固定 WARP 用于门禁 |
| 设备复用 | tgfx 原生 MTLDevice 注册 | libpag canonical COM 身份注册与共享 entry owner |
| 原生身份 | 比较 MTLDevice | COM 身份捕获与目标 context 一次探测，不信任空引用 |
| PAGX 图像 | 依赖精确 tgfx contextID | 同一注册 Device + 图像强 owner，不能只比较原生适配器 |
| 资源交接 | 没有 D3D12 式显式状态 | 每次 COMMON 交接与独占，完成后再由宿主切换状态 |
| 信号量/完成 | pag 公共 API 未转发 Metal semaphore | 首期不扩 fence，新增明确 CPU completion，而非省略同步 |
| 窗口 | CAMetalLayer 与 MetalLayerDrawable | D3D12Window 与已适配的 Win32Demo |
| 测试 | Metal 后端完整套件 | 完整 D3D12 套件，固定 WARP 与可信缓存 |
| 缓存 | `.cache/metal` | `.cache/d3d12` 与 `.cache/d3d12-warp` 分离 |

## 附录 B：静态证据与参考资料

- Metal 适配与测试提交：`ba9952640`、`c0b63f058`、`c52be7304`、`8a63822b0`；CI/解码修复：`eb4412aa5`、`e0815e9ca`。
- PAGX 图像寿命/归属：`src/pagx/PAGImage.cpp:65-81`、`third_party/tgfx/src/core/images/TextureImage.cpp:83-88`、`third_party/tgfx/src/gpu/d3d12/D3D12Device.cpp:151-153`。
- COMMON 与独立队列：`third_party/tgfx/src/gpu/d3d12/D3D12Texture.h:66-68`、`D3D12RenderPass.cpp:580-610`、`src/rendering/PAGSurface.cpp:375-382`。
- 公开身份/完成能力：`third_party/tgfx/include/tgfx/gpu/Context.h:78-83`、`GPU.h:84-89`、`Texture.h:156-160`、`CommandQueue.h:78-82`。
- 宏和编译参数：`CMakeLists.txt:204`、`:1273`；评审的无文件 clang-cl 探针证明裸参数被忽略、`/clang:-fno-access-control` 生效。
- Windows Demo 预构建：`win/Win32Demo/WinPAGDemo.vcxproj:167-173`、`:209-214`。
- MD5 私有头：`third_party/tgfx/src/core/utils/MD5.h`；测试 include：`CMakeLists.txt:1255-1263`。
- 内置软件解码和 mock：`CMakeLists.txt:82-85`、`src/rendering/video/VideoDecoderFactory.cpp:82-92`、`test/src/VideoSequenceFallbackTest.cpp:148-225`、`PAGANRRegressionTest.cpp:643-690`。
- 旧缓存脚本的 main 依赖：`update_baseline.sh:59-61`、`:108-139`；基线接受限制：`.codebuddy/rules/Test.md:34-36`。
- tgfx Windows CI 环境：`third_party/tgfx/.github/workflows/autotest.yml` 的 `win-d3d12-warp`。

## 附录 C：评审问题落实表

| 评审要点 | 本版修订位置 |
|---|---|
| Device/contextID 不同与 PAGX 图像短命 | §3.3、§4.4、§6.2：精确复用、强 owner、析构顺序和寿命测试 |
| COMMON 入口/出口遗漏 | §4.3、§4.5：交接状态、独占和双向测试 |
| 只提交没有正常 completion 出口 | §4.5：新增等待接口、健康检查和队列顺序，不用回读掩盖 |
| 缺 getter 不等于无法验证原生设备 | §2.1、§4.4：公开 API 一次探测与 COM 身份匹配 |
| Windows 使用未生成的宏 | §3.2、§4.6：统一 `TGFX_USE_D3D12` |
| ClangCL 参数仍会被忽略 | §5.2：按 frontend 转发，旧裸参数被替换 |
| VS 多配置、vendor、脚本路径不一致 | §5.2、§6.4、§7：统一单配置工具链、Debug/CRT、路径和并行参数 |
| 首次基线/main/PR 依赖死锁 | §6.3、§7、§8：当前已审核分支接受，可信制品，测试和流程前置 |
| Win32Demo 仍构建 OpenGL | §4.6、§5.1、§6.5：工程参数、独立产物、系统库与运行日志 |
| ffavc 缺失误判软件解码能力 | §5.3、§6.2：复用 libavc，保留 mock 与真实视频覆盖 |
| MD5 私有 include 未传递 | §5.3：文件/辅助对象级私有路径与预编译符号契约 |
| 指针不能表达任意视图格式 | §4.3：typed 白名单、形状/用途/尺寸拒绝，不猜格式 |
| const GetDesc、不存在的 LOGW、C 示例语法 | §4.2-4.4：正确 COM 指针接口、已有日志和合法 C 声明 |
