# PAGX 动效导出 MP4：方案与实现

本文记录 `pagx` 动效导出为 MP4 的技术方案、已完成的渲染层实现、实测数据，以及编码层与 ardot 接入的待决事项。

- 范围：输入为已带动画轨道的 `pagx::PAGXDocument` 或 `.pagx` 文件，输出为 MP4。
- 不涉及：「设计稿动效 → PAGX」的导出实现（由 exporter 侧负责），详见 §2.3。
- 落点：`pagx` 独立命令行（native）与 ardot 编辑器（wasm / CEF）。

---

## 1. 背景

### 1.1 需求

ardot 的数据流是「设计稿节点树 → `pagx::PAGXDocument` → 其他格式」，HTML / PPTX / PDF / SVG 已经按这条链路实现。视频导出是缺口：动效面板（`animation-export.tsx`）目前是禁用占位。

同时对标对象 Figma 已经提供该能力：Figma Motion（Config 2026 发布，Open Beta）支持把顶层 Frame 的动效导出为 MP4 / WebM / GIF / SVG / Lottie，导出参数只有 `fps` / quality preset / size constraint 三项，分辨率 >1920×1080 或帧率 >30fps 需付费计划。

### 1.2 两个落点的差异

| | CLI（native） | ardot（wasm / CEF） |
|---|---|---|
| 运行环境 | 进程内，原生 GPU 后端 | 浏览器沙箱，WebGL/WebGPU |
| 编码器 | 需自备（无 WebCodecs） | JS 侧有 WebCodecs（硬件编码） |
| 设备归属 | 可自建 `pag::Devices::MakeDefault()` | 必须复用画布 device，wasm 下建 device 受线程限制 |
| 输出 | 落盘 `.mp4` | 内存 `Uint8Array` → 前端 Blob 下载 |

这个差异决定了架构：**渲染编排共享，编码与封装按宿主分开**。

---

## 2. 现状分析

### 2.1 pagx 现有导出链路

CLI 入口在 `src/cli/main.cpp`，导出命令分发在 `src/cli/CommandExport.cpp:263-283`：

| 路径 | 实现 | 本质 | GPU 渲染 |
|---|---|---|---|
| `export --format svg` | `SVGExporter::ToFile` | 节点树 → SVG 矢量序列化 | 仅 bake 不支持的特性时（`bakeUnsupported` 默认开启） |
| `export --format html` | `HTMLExporter::ToFile` | 节点树 → HTML/CSS（ZIP 打包） | 同上 |
| `export --format pptx` | `PPTExporter::ToFile` | 节点树 → OOXML | 同上 |
| `render` | `LayerBuilder::Build` → `tgfx::DisplayList` → `readPixels` → `ImageCodec::Encode` | 静态光栅化单帧 | 始终 |
| `render --time <s>` | `PAGScene::Make` → `setCurrentTime` + `apply` → `advanceAndApply` → `draw` | 动画某一时刻的单帧 | 始终 |

前三条是**静态快照序列化，没有时序能力**；只有 `render --time` 具备时间轴能力，但它每次调用都重建 scene，不适合批量出帧。

**注意：这三条 export 并非完全不碰 GPU。** `bakeUnsupported` 开启时（SVG/HTML/PPT 的默认值），凡目标格式无法表达的效果都会通过 `pagx::GPUContext` 懒创建一个 device 做离屏光栅化，结果以 base64 PNG 内嵌到产物中（典型场景：`ImagePattern` 的 tiling、带 mask 的图层、依赖 backdrop 的混合模式）。这一机制的用途与设计意图写在 `src/pagx/utils/RasterUtils.h:37-43`：

> Manages a lazily-created GPU device and context for off-screen rendering. Reuse a single instance across multiple render calls to avoid repeated GL context creation (each `Devices::MakeDefault()` on the GL backend builds a brand-new GL context, which would make per-pattern bakes in SVG/PPT export repeatedly recompile shaders).

也就是说「导出需要 device」在 pagx 里已有先例，MP4 不是第一次引入这个依赖；`GPUContext` 的「懒创建 + 全程复用一个实例」正是 MP4 逐帧渲染要照搬的模式。

### 2.2 ardot 的调用链路

```
TS 前端  workspace/src/features/export/service.ts
   │      Schema.ExportImageRequest / ExportDocumentRequest（kiwi schema）
   ▼
bridge   bridge/src/api/handlers/export_handler.cc:28 / :77
   │      make_export_callback() → Schema.ExportDataCallback → emitEvent(ShortLived)
   ▼
ui       ui/src/export/image_exporter.cc（节点级，按格式策略表分发）
         ui/src/export/document_exporter.cc（文档级，多 frame / 多 slide 合并）
   │
   ▼
pagx 公开 API：HTMLExporter::ToData / PPTExporter::ToData / PAGXExporter::ToXML / tgfx::SVGExporter
```

几条必须遵守的既有约定：

1. **只用 `ToData` 不用 `ToFile`** —— 编辑器全程内存态，落盘/下载由前端 Blob 完成。CLI 才用 `ToFile`。
2. **一次完成回调**，链路中没有进度上报。
3. **字体在 import 阶段注入** `document->fontConfig`，导出侧复用。
4. **图片资源通过 `collect_resource_paths` → `ExportResourceFetchManager::fetch` → `document->loadFileDataMap()`** 注入。
5. **GPU 锁是硬约束**：导出器头文件明确要求调用方不得持有同一 device 的 context lock，`DeviceLockScope` 不可重入。
6. **导出器自建设备**：`GPUContext` 懒创建 device（内部 `pag::Devices::MakeDefault()`），并在一次导出内全程复用该实例。`MakeDefault()` 本身**不是单例**，每次调用都会新建 device 与 context，复用的责任在调用方。

### 2.3 两套「快照 / 动画」语义

exporter 的 `ExportOptions::snapshot_current_frame` 决定两件事（`exporter/src/pagx/exporter.cc:1219-1224`）：

- `true`：用编辑器**当前帧**几何求值（`MotionGeometryScope`），且**不写 Animation 轨道**
- `false`（默认）：静态几何（`StaticGeometryScope`）+ 完整动画轨道

ardot 现有两条路径正好相反：

| 路径 | 位置 | snapshot | 动画轨道 |
|---|---|---|---|
| 节点级（PNG/JPEG/WEBP/SVG/PDF/HTML/PPTX 单节点） | `ui/src/export/image_exporter.cc:982` | `true` | 空 |
| 文档级（PDF/PPTX/HTML 多页、多 frame） | `ui/src/export/document_exporter.cc:237-268` | `false` | 保留 |

**因此 MP4 不能复用节点级现有的 `export_request.document`** —— 那份 document 的 `animations` 为空，逐帧渲染只会得到 N 张相同的图。MP4 必须走 `snapshot_current_frame = false` 的构造路径。

ardot 侧「动效 → PAGX」的引擎代码已完整：`exporter/src/pagx/utils/motion_animation_builder.cc`（2216 行），在 `exportNode` / `exportNodes`（`exporter.cc:540/644/759`）中于 `snapshot_current_frame == false` 时被调用，把编辑器 k 帧绑定写成 `<Animation>/<Object>/<Channel>/<Key>`。它只生成 `Animation`，不生成 `StateMachine`。缺的是产品通道（导出面板、schema 类型、策略登记）。

---

## 3. 总体架构

```
输入：带动画轨道的 PAGXDocument / .pagx
   │
   ▼
① 帧渲染编排（本文已实现，跨平台，native + wasm 共用）
   VideoOptions { fps, scale, background, duration }
   一次 PAGScene + 一次 PAGSurface，逐帧：时间采样 → 求值 → 绘制 → 读回
   │  交付 RGBA
   ▼
② 编码 + 封装（按宿主实现）
   CLI（native）    ：内置 H.264 编码器 + 复用 pag::MP4BoxHelper 封装
   ardot（wasm）    ：JS WebCodecs VideoEncoder + mp4-muxer 封装
   ▼
MP4
```

共用的只有 ①。这样 CLI 不会引入 wasm 用不上的编码器，ardot 也不会被迫使用 wasm 内低效的软件编码器。

---

## 4. 渲染层：设计与实现

### 4.1 时间轴模型

PAGX 的动效有三种驱动形态，渲染时必须全部覆盖：

| 形态 | 说明 | 驱动方式 |
|---|---|---|
| top-level `Animation` | `<Animations>` 下的 `<Animation>` 节点 | `scene->getAnimation(id)->setCurrentTime(us)` + `apply()` |
| Layer timeline driver | Layer 上的 `animation="@xxx_anim"` | `PAGComposition` 为其 spawn 一个独立实例，由 `scene->advanceAndApply(delta)` 驱动 |
| `StateMachine` | 状态机调度多个 Animation | 若被 Layer 引用则同上；未被引用时不影响渲染结果 |

关键细节：`PAGScene::getAnimation()` 返回的是一个**独立实例**（`src/pagx/PAGScene.cpp:431-441` 的 `instantiatedTimelines`），而 `PAGComposition::spawnTimelines`（`src/pagx/runtime/PAGComposition.cpp:162-172`）为 Layer driver **另外 new** 一个实例。两者不是同一个对象，各自维护时序。

所以必须**两种驱动都做**：

- 只做显式 seek：被 Layer driver 驱动的动画可能被 composition 内实例覆盖
- 只做 `advanceAndApply`：未被任何 Layer 引用的顶层 Animation 不会生效

### 4.2 逐帧推进策略

```cpp
auto scene = PAGScene::Make(document);             // 一次
auto surface = PAGSurface::MakeOffscreen(w, h);    // 一次
int64_t previousUs = 0;
for (int64_t frame = 0; frame < frameCount; frame++) {
  int64_t timeUs = llround(frame * 1000000.0 / frameRate);
  for (auto& animation : animations) {
    animation->setCurrentTime(timeUs);             // 绝对 seek
    animation->apply(1.0f);
  }
  scene->advanceAndApply(timeUs - previousUs);     // 嵌套 timeline 只能增量
  previousUs = timeUs;
  scene->draw(surface);
  surface->readPixels(pixmap.writablePixels(), pixmap.rowBytes());
  // 编码 + 落盘
}
```

为什么是混合策略：

- 顶层动画支持绝对 seek（`setCurrentTime` 设置 raw elapsedUs，`apply` 时才按 Loop/PingPong 折叠），所以多圈循环只要 t 持续增长
- `PAGComposition` 的 timeline 只提供 `advance(delta)`，没有绝对定位接口，必须用帧间增量累加
- 两者时间基准一致（整数微秒），因此结果等价

### 4.3 设备与上下文

| 场景 | 推荐路径 | 原因 |
|---|---|---|
| CLI | `PAGSurface::MakeOffscreen(w, h)` | 进程独占，可自建 device；但必须**只建一次**，见下方说明 |
| ardot | `pagx::MakeFrom(device, tgfx::Surface)` + `pagx::Record(context, scene, surface, autoClear)` | wasm 下 device 必须在主 runtime 线程创建，且应与画布 device 同源 |

`PAGSurface::MakeOffscreen` 的 device 来源是 `OffscreenDrawable::Make` → `pag::Devices::MakeDefault()`（`src/pagx/runtime/OffscreenDrawable.cpp:28`）。**`MakeDefault()` 不是进程级单例**：OpenGL 后端走 `tgfx::GLDevice::MakeWithFallback()`、WebGPU 走 `WebGPUDevice::Make()`、Metal 走 `MetalDevice::Make()`，每次调用都会新建 device 及其 context。因此「每帧新建 scene + 新建 surface」的旧路径实际每帧都在重建 device/context/texture，这是旧路径 400ms 单帧的主要浪费之一；复用 scene + surface 后这部分只付一次。

`pagx::MakeFrom` / `pagx::Record` 见 `include/pagx/tgfx.h:50-67`，目前只有测试在用，正是为「宿主已有 device、想批量提交」设计的。

**禁止嵌套 context lock**：`PAGScene::draw` 与 `PAGSurface::readPixels` 内部都会自行加锁，调用方不能已持有同一 device 的锁。

**WebGPU 的额外限制**：`pag::Devices::MakeDefault()` 在 WebGPU 后端只能由主 runtime 线程调用（`src/rendering/gpu/webgpu/Devices.cpp:26-28`，tgfx 在非主线程直接报错返回 nullptr）。ardot 属于这一情形，`MakeOffscreen` 在子线程里不可用。

### 4.4 CLI 实现

已在 `src/cli/CommandRender.cpp` 落地（commit `9b5a1262c`），新增三个参数：

```
--frames-dir <dir>     把动效渲染为图片序列写入目录，全程复用同一个 scene
--fps <n>              采样帧率，默认取文档 Animation.frameRate（缺省 60）
--duration <seconds>   序列时长，默认取最长动画的一个完整周期
```

约束：

- `--frames-dir` 与 `--time` / `--id` / `--xpath` / `--crop` / `--output` 互斥
- `--fps` / `--duration` 必须配合 `--frames-dir`
- 帧文件命名 `f_0000.png`，格式由 `--format` 决定（png / jpg / webp）

用法：

```bash
pagx render --frames-dir /tmp/frames --fps 60 --duration 8 input.pagx
ffmpeg -framerate 60 -i /tmp/frames/f_%04d.png -c:v libx264 -pix_fmt yuv420p out.mp4
```

### 4.5 实测数据

样例：`a9ad04e7….pagx`（1200×738，13 个 Animation，duration 49~480 帧，`loop="loop"`，无 `frameRate` 属性 → 默认 60fps）。

**性能**（PNG 输出，1200×738）：

| 路径 | 60 帧 | 单帧 | 480 帧（8s） |
|---|---|---|---|
| 每帧重建进程（旧方式） | 24s | 400ms | ~192s（推算） |
| 复用 scene + PNG | 8s | 133ms | **60s（实测）** |
| 复用 scene + JPEG q90 | 3s | 50ms | ~24s（推算） |

单帧 133ms 的构成：PNG 编码约 83ms（62%），渲染 + 读回约 50ms。**瓶颈在中间图片编码，不在渲染**。

**一致性验证**（复用 scene 增量推进 vs 每帧新建 scene 绝对 seek，fps=100 使时间点可精确表达）：

| 帧 | 差异像素占比 | 最大通道差 | 平均差 |
|---|---|---|---|
| 0 / 1 | 0 | 0 | 0（字节完全一致） |
| 2 | 2.80% | 6/255 | 0.028 |
| 10 | 2.37% | 6/255 | 0.024 |
| 20 | 2.27% | 2/255 | 0.023 |
| 29 | 1.92% | 6/255 | 0.019 |

差异不随帧号放大，属于渲染缓存/浮点噪声，H.264 有损压缩后不可见。**结论：增量推进与绝对 seek 视觉等价，核心假设成立。**

**产物质量**：480 帧 → `1200x738 / 60fps / 8.000s / 1.36MB / 1.36Mbps`，相邻帧差异 0.104~0.784（平滑），重复帧 0。

**循环接缝**：首尾帧差异 6.50（不闭合）。该文件的 13 个动画周期互不相同，不存在有意义的整体循环周期，无缝循环需要产品层面另行定义规则。

---

## 5. 编码层：方案（待决策）

### 5.1 候选路线

仓库目前**没有任何视频编码能力**：`third_party` 只有解码类库（`libavc` 的 target 名为 `libavcdec`），平台原生编码 API（`AVAssetWriter` / `VTCompressionSession` / `IMFTransform` / Android `MediaCodec`）零使用，`exporter/` 的 `OfflineVideoEncoder` 是 AE 插件专用（`QProcess` 调外部 x264），且 **x264 为 GPL-2.0，不能进 Apache-2.0 的 libpag 主库**。

| 路线 | 优点 | 代价 |
|---|---|---|
| A. 外部 ffmpeg 进程 | 零新依赖、无许可证问题、最快落地 | 破坏 npm CLI 开箱即用；三平台行为不一致 |
| B. 内置 openh264（BSD-2） | 三平台一致、单份代码 | 新增 DEPS + vendor target + 6 个平台/架构构建矩阵；专利授权需法务确认 |
| C. 平台硬编 | 质量与速度最好、mac/win 零新增依赖 | 三份平台代码；Linux 无 GPU 时仍需软件兜底 |

建议：先以 A 打通端到端（`--frames-dir` 已具备，只差一次进程调用），再按产品节奏评估 B/C。

### 5.2 MP4 封装

`src/codec/mp4/MP4BoxHelper::CovertToMP4` 已在主库，输入为 `pag::VideoSequence`：

- `headers`：codec specific data，Annex-B（前 4 字节起始码，写包时跳过），至少 SPS + PPS
- `frames`：每帧 `VideoFrame`，含 `frame`（pts）、`isKeyframe`、`fileBytes`（Annex-B NALU）
- `width` / `height` / `frameRate`：取自 `Sequence` 的 public 字段，需自行填充（不从 SPS 解析）

`headers` / `frames` / `width` / `height` 均为 public，可直接构造实例后调用，无需改动现有代码。openh264 / x264 默认输出即 Annex-B，天然匹配。

注意 `MP4BoxHelper` 产出的是 **fragmented MP4**（`ftyp+moov+moof+mdat`）。微信、QuickTime、部分剪辑软件对 fMP4 兼容性参差，若目标场景要求普通 MP4（`moov` 前置、单 `mdat`），需要改造或另写 muxer。

### 5.3 色彩与 alpha

- H.264 不支持 alpha，MP4 输出恒为不透明。RGBA 需先 unpremultiply，再与 `--background` 指定颜色合成，最后转 I420（BT.709 有限范围）
- 若产品必须支持透明动效，只能走 WebM（VP9 alpha）或 MOV（ProRes 4444），与 MP4 是两条链路

---

## 6. ardot 接入设计（未实现）

### 6.1 策略与任务

`ExportStrategy` 现有四种（`ui/src/export/image_exporter.cc:60-66`），MP4 是 `kUnsupported`（`:101-107`）。MP4 既需要 GPU 渲染（像 `kDeviceRender`），又耗时长不能主线程当场产出（像 `kAsyncSerialize`），**必须新增一种策略**（暂名 `kAsyncRender`）。

改动点：

| 位置 | 改动 |
|---|---|
| `ui/src/export/image_exporter.cc:88` `traits_of` | `ImageType::MP4` 登记新策略 |
| `ui/src/export/image_exporter.cc:696` `exportSerializedItem` | 新增分支或独立 `exportVideoItem`，提交 `VideoExportTask` |
| `ui/src/export/image_exporter.cc:982` | MP4 必须用 `snapshot_current_frame = false`，不能复用静态快照 document |
| `ui/src/tasks/export/` | 新增 `video_export_task.{h,cc}` |
| `packages/common/schema/entity.kiwi:1517` `ExportSettings` | 追加 `fps`、`duration` / `loop`（`quality`、`constraint` 可复用） |
| `workspace/.../Export/animation-export.tsx` | 占位 UI 转正（fps 12/24/30/60、0.5x/1x/2x、1920w~2160h、LOW/MEDIUM/HIGH 已就绪） |
| `workspace/src/features/export/service.ts` | 新增 `ext: 'mp4'` / `mimeType: 'video/mp4'` |

`ImageType::MP4` 枚举值已存在（`entity.kiwi:441-447`），不需要动枚举。

### 6.2 编码分工

wasm 内没有 H.264 编码器，把 openh264 编进 wasm 可行但性能不可接受（1080p 单/双线程个位数 fps）。因此 ardot 侧：

```
C++ 逐帧渲染 → RGBA → bridge 回调 → JS WebCodecs VideoEncoder(avc1) → mp4-muxer → Blob
```

两个必须评估的点：

1. **跨语言拷贝**：1080p RGBA 约 8MB/帧，300 帧约 2.4GB 内存带宽，必要时用「C++ 分批渲染 + 每批一次回调」摊薄
2. **兼容性探测**：`VideoEncoder.isConfigSupported()` 先行，失败时提示降级（CEF/Chrome 无此问题，Web 端需留意 Firefox/Safari）

### 6.3 设备与锁

ardot 导出时必须使用 `pagx::MakeFrom(device, tgfx::Surface)` + `pagx::Record(...)`，不能使用 `PAGSurface::MakeOffscreen`（后者内部 `Devices::MakeDefault()`，wasm 下可能与画布 device 不同源，WebGPU 后端还限制只能在主 runtime 线程建设备）。

同时注意与现有导出流程的锁生命周期错开：`bakeImagePaintFilters` 是 RAII 持锁、函数返回即释放，MP4 任务应在之后提交。

---

## 7. 风险与限制

| 风险 | 影响 | 对策 |
|---|---|---|
| H.264 无 alpha | 透明动效无法导出 | 明确不支持，或另开 WebM/MOV 链路 |
| PNG 中间编码占单帧成本 62% | 出片慢 | 接编码器时直接 RGBA → YUV，跳过图片编码 |
| fMP4 兼容性 | 部分播放器/剪辑软件打不开 | 需要时改造为普通 MP4 |
| 循环不闭合 | 首尾衔接跳变 | 产品定义时长规则（按最短周期截断 / 强制对齐） |
| 多动画周期不一致 | 无法自然定长 | 默认取最长动画一个周期，允许 `--duration` 覆盖 |
| 状态机驱动的动效 | 交互逻辑无法离线复现 | 只渲染时间轴，不模拟状态转换（当前样例的 StateMachine 未被引用，无影响） |
| wasm 跨语言传帧 | 内存带宽开销 | 分批回调 / 必要时降采样 |

---

## 8. 后续计划

| 阶段 | 内容 | 状态 |
|---|---|---|
| P0 | `pagx render --frames-dir` 复用 scene 逐帧渲染 + 一致性验证 | ✅ 已完成（`9b5a1262c`） |
| P1 | CLI 接入编码器（ffmpeg 或 openh264），一条命令直接产 MP4 | 待决策 |
| P2 | ardot 接入：新策略 + `VideoExportTask` + WebCodecs + 进度通道 | 待上游动效 PAGX 通道打通 |
| P3 | 性能优化：跳过图片编码、异步读回、多线程编码 | 待 P1/P2 之后 |
| P4 | 质量档位与码率映射（对齐 Figma 的 LOW/MEDIUM/HIGH） | 待 P1 |

---

## 9. 附录：验证命令

```bash
# 构建
cmake -G Ninja -DPAG_BUILD_CLI=ON -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target pagx-cli

# 单帧抽查（动画是否被驱动）
./cmake-build-debug/pagx render --time 0 -o /tmp/t0.png input.pagx
./cmake-build-debug/pagx render --time 2 -o /tmp/t2.png input.pagx

# 帧序列（默认时长 = 最长动画一个周期）
./cmake-build-debug/pagx render --frames-dir /tmp/frames --fps 60 input.pagx

# 合成 MP4
ffmpeg -y -framerate 60 -i /tmp/frames/f_%04d.png -c:v libx264 -pix_fmt yuv420p -crf 20 \
  -movflags +faststart out.mp4
```

一致性对比时注意时间点必须精确对齐：`--time` 接收秒（float），`--frames-dir` 内部按 `llround(frame * 1e6 / fps)` 计算微秒，`fps` 取 100/50/25 这类可被十进制精确表达的值，否则会引入几个微秒的偏差，导致像素对比出现假阳性。
