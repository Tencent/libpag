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
| `render --frames-dir` | 一次 `PAGScene` + 一次 `PAGSurface`，逐帧 seek + `advanceAndApply` + `draw` + `readPixels`（§4.2） | 动效全帧序列 | 始终 |

前三条是**静态快照序列化，没有时序能力**。时间轴能力来自后两条：`render --time` 每次调用都重建 scene（`PAGScene::Make` + `PAGSurface::MakeOffscreen`），不适合批量出帧；`render --frames-dir` 是本项目的产物，复用同一 scene/surface 输出整段序列，即 §4 的渲染层实现。

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
   CLI（native）    ：S1 外部 ffmpeg 进程 + rawvideo 管道，编码与封装一次完成（见 §5.1）
                      备选 S3 内置 openh264 + pag::MP4BoxHelper 封装（当前产出 fMP4，见 §5.2）
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
| ardot | 当前线程内 `PAGScene::draw` + `PAGSurface::readPixels`（§6.3 模型 A）；只有跨线程时才用 `pagx::MakeFrom(device, surface)` + `pagx::Record(context, scene, surface, autoClear)` | wasm 下无法新建 device，只能使用当前线程已有的那个 |

`PAGSurface::MakeOffscreen` 的 device 来源是 `OffscreenDrawable::Make` → `pag::Devices::MakeDefault()`（`src/pagx/runtime/OffscreenDrawable.cpp:28`）。**`MakeDefault()` 不是进程级单例**：native 后端每次调用都会新建 device 及其 context（OpenGL 走 `tgfx::GLDevice::MakeWithFallback()`、WebGPU 走 `WebGPUDevice::Make()`、Metal 走 `MetalDevice::Make()`）。因此「每帧新建 scene + 新建 surface」的旧路径实际每帧都在重建 device/context/texture，这是旧路径 400ms 单帧的主要浪费之一；复用 scene + surface 后这部分只付一次。

`pagx::MakeFrom` / `pagx::Record` 见 `include/pagx/tgfx.h:50-67`，目前只有测试在用，正是为「宿主已有 device、想批量提交」设计的。

**禁止嵌套 context lock**：`PAGScene::draw` 与 `PAGSurface::readPixels` 内部都会自行加锁，调用方不能已持有同一 device 的锁。

**wasm 下 `MakeDefault()` 的语义完全不同**：它不创建 device，只返回「当前线程已绑定的 device」，其他线程一律 nullptr。WebGL 与 WebGPU 两个后端都是如此（细节与线程模型见 §6.3）。

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

## 5. 编码层：方案与选型

### 5.1 候选路线全清单

仓库目前**没有任何视频编码能力**：`third_party` 只有解码类库（`libavc` 的 target 名为 `libavcdec`），平台原生编码 API（`AVAssetWriter` / `VTCompressionSession` / `IMFTransform` / Android `MediaCodec`）零使用，`exporter/` 的 `OfflineVideoEncoder` 是 AE 插件专用（`QProcess` 调外部 x264），且 **x264 为 GPL-2.0，不能进 Apache-2.0 的 libpag 主库**。

按「编码器从哪来」和「跑在谁里面」两个维度拆开，可选方案共 7 条：

| 编号 | 方案 | 编码器来源 | 运行形态 | 许可证 | 直接产出 |
|---|---|---|---|---|---|
| S1 | 外部 ffmpeg（用户环境已有） | 用户安装的 ffmpeg（通常带 libx264） | 子进程 + rawvideo 管道 | 不随包分发，主库干净 | 标准 MP4（含 faststart） |
| S2 | 随包分发 ffmpeg（LGPL 构建） | 随包的 ffmpeg | 子进程 | LGPL-2.1，需声明且允许替换 | 标准 MP4 |
| S3 | 内置 openh264 | 链进主库 | 进程内 | BSD-2，兼容 Apache-2.0；专利需法务 | Annex-B 裸流 |
| S4 | 平台硬编 | VideoToolbox / Media Foundation / VAAPI | 进程内 | 系统 API，专利由 OS 承担 | Annex-B 或平台样本缓冲 |
| S5 | 随包分发 H264EncoderTools | 独立 GPL 可执行文件（x264） | 子进程 + 文件握手 | GPL-2.0，须提供该工具源码 | Annex-B 裸流 |
| S6 | 链接 / dlopen 系统 x264 | 系统 `libx264` | 进程内 | GPL-2.0 与 Apache-2.0 不兼容 | Annex-B 裸流 |
| S7 | 只出帧序列（当前 `--frames-dir`） | 无 | 无 | 干净 | PNG / JPEG / WEBP 序列 |

**S1 外部 ffmpeg（推荐）**：`ffmpeg -f rawvideo -pixel_format rgba -video_size WxH -framerate N -i pipe:0 -c:v libx264 -pix_fmt yuv420p -crf Q -movflags +faststart out.mp4`，C++ 侧把 `readPixels` 的结果直接写进子进程 stdin，跳过中间图片编码（实测 PNG 编码占单帧 62%），同时省掉自研 muxer。代价是 npm 包不能无条件开箱即用，需检测 `ffmpeg` 是否存在并给出安装指引。注意三点：背景色在 CLI 侧先清屏合成（tgfx 输出是预乘 alpha，交给 ffmpeg 做 unpremultiply 容易错）、`write` 阻塞天然形成背压故单线程即可、结束必须 `waitpid` 校验退出码否则「文件损坏也报成功」。

**S2 随包分发 ffmpeg（排除）**：体积增加数十 MB，且 LGPL 构建的 ffmpeg 不能带 `libx264`（GPL），H.264 编码只剩 `h264_videotoolbox` / `h264_vaapi` / `h264_openh264` 等 wrapper —— 等于绕回 S4 或 S3，还额外背一条「动态链接 + 允许替换」的 LGPL 义务。

**S3 内置 openh264（备选）**：唯一能同时满足「Apache-2.0 兼容」和「开箱即用」的软件编码器。代价是新增 DEPS + vendor target + 6 个平台/架构构建矩阵；码率控制只有 CBR/VBR，没有 x264 的 CRF 恒质量模式；Cisco 的 H.264 专利补偿只覆盖其官方预编译二进制，自行编译需法务单独确认。**前提：产品明确要求零外部依赖。**

**S4 平台硬编（不推荐）**：mac 一套 `VTCompressionSession`、win 一套 Media Foundation MFT（COM + 异步事件），Linux 还要第四套兜底。pagx 的产物是独立 MP4，通用播放器本就不支持 alpha，所以这里不存在 exporter 那种「硬编给不了 alpha」的死结；否决理由纯粹是工程量与收益不匹配 —— 离线导出对编码速度不敏感，而同码率画质不如 x264 的 CRF。

**S5 随包分发 H264EncoderTools（不推荐，见 §5.1.1）**：编码器现成、参数可控（quality 0-100 映射 x264 CRF/VBV/keyint，工具协议与产物见 §5.4），但必须自备 muxer，且要履行 GPL 源码提供义务。

**S6 链接 / dlopen 系统 x264（否决）**：GPL-2.0 没有「系统库例外」（那是 GPLv3 的条款），动态链接与 `dlopen` 都不能豁免；更硬的一条是 Apache-2.0 与 GPL-2.0 单向不兼容，主库不可能为此改许可证。

**S7 只出帧序列（兜底）**：即当前的 `--frames-dir`，由用户自行 `ffmpeg -i f_%04d.png` 合成。零依赖、零许可证风险，但多一步手工操作且要多付一次图片编解码。

### 5.1.1 H264EncoderTools 属于哪一套

**它属于 S5，不是 S1。** 两者的分界线是「谁提供那个二进制、谁承担 GPL 义务」：

- S1 的编码器由用户环境提供，我们不随包分发、不链接，因此不承担 x264 的分发义务。
- S5 是我们随产品投递一个 GPL 可执行文件，必须履行 GPL-2.0 义务（提供该工具的完整对应源码或书面 offer）。进程边界只能把义务限制在工具自身、不污染 CLI 本体，不能免除义务。exporter 现在就是这么做的：工具放在 `~/Library/Application Support/H264EncoderTools`（mac）与 `%APPDATA%\H264EncoderTools`（win），不进 plugin bundle。

**在 CLI 上照搬 S5 的成本比 exporter 高**，原因有三：

1. exporter 只要 Annex-B 裸流，直接塞进 `.pag` 的 `VideoSequence` 即可；CLI 要的是独立 MP4，裸流必须自己 mux，而 `MP4BoxHelper` 只产 fMP4（§5.2），所以 S5 天然绑定「改造 muxer（300~500 行）或强制 remux」。
2. `OfflineVideoEncoder` 依赖 Qt 的 `QProcess` / `QFile` / `QJsonDocument`，CLI 不能引 Qt，需改 `posix_spawn` / `CreateProcess` 并自带 JSON 解析。
3. 文件握手（`YUV-<n>.yuv` + `InFrameInfo-<n>.txt` ↔ `H264-<n>.264` + `OutFrameInfo-<n>.txt`，1ms 轮询）在 CLI 上应改成 stdin/stdout 管道，否则每帧都要多一次写盘。

结论：S5 只在「必须开箱即用」且「法务接受随包分发 GPL 工具」两个条件同时成立时才选，且必须同步改造 muxer。相比之下 S1 是同样的「外部进程 + 裸帧」，但把编码器来源换成用户环境，许可证与 muxer 两个成本同时消失。

### 5.1.2 与 exporter 的选型差异

exporter 选 S5、CLI 选 S1，不是标准不一致，而是产物和播放方不同：

| 维度 | exporter（AE 插件） | pagx CLI |
|---|---|---|
| 产物 | `.pag` 里内嵌的 H.264 | 独立 `.mp4` 文件 |
| 播放方 | PAG runtime（Android / iOS / Web / Windows / mac） | 通用播放器、浏览器、剪辑软件 |
| 格式约束 | 所有端解码能力的交集 = H.264 + 自带 alpha 灰度区 | H.264 yuv420p 最稳；alpha 不支持 |
| 平台硬编能否满足 | 不能：Windows / Web 只有 H.264 软解，mac 硬解输出固定 BGRA 无 alpha 平面 | 能满足（MP4 本就不带 alpha），但工程量与收益不匹配 |
| alpha 方案 | 画布扩 2 倍 + alpha 灰度区（`(a*219>>8)+16`） | 不支持；透明动效需另开 WebM / MOV 链路 |
| 最终选择 | S5（x264 进程隔离，唯一能出带 alpha 的 H.264 的路） | S1（外部 ffmpeg，零依赖零许可证风险） |

### 5.1.3 各方案优缺点对照

| 方案 | 优点 | 缺点 |
|---|---|---|
| S1 外部 ffmpeg（用户环境） | 零新增依赖与包体；不随包分发、不链接，主库无许可证义务；编码与封装一次完成，直接产标准 MP4（faststart、色彩矩阵、B 帧都由 ffmpeg 负责）；rawvideo 管道跳过中间图片编码，省掉实测占单帧 62% 的成本；实现量最小，只差一次进程调用 | 未安装 ffmpeg 时无法出片，破坏开箱即用；用户环境的 ffmpeg 版本与构建不一致，同参数输出可能不同；需自行处理管道背压、子进程退出码与写入失败；跨平台进程创建要各写一遍；用户若装的是 GPL 构建，合规责任在用户侧，但容易被误认为由我们分发 |
| S2 随包分发 ffmpeg（LGPL 构建） | 开箱即用；直接产标准 MP4；参数与行为统一可控；无需用户额外安装 | 包体增加数十 MB 且要覆盖多平台；LGPL 构建不能带 libx264，H.264 编码只剩 `h264_videotoolbox` / `h264_vaapi` / `h264_openh264` 等 wrapper，等于退回 S3 或 S4；额外承担 LGPL 声明与可替换义务；mac 签名公证、Linux glibc 基线、Windows 杀软误报都要处理；第三方预编译二进制带来供应链风险 |
| S3 内置 openh264 | BSD-2 与 Apache-2.0 兼容，可静态链接进主库；体积仅几 MB；一套代码三平台一致；无运行时外部依赖；用 Cisco 官方二进制可享其专利补偿 | 只有 CBR/VBR + bitrate，没有 CRF 恒质量，质量档位需重新设计；画质与压缩率不如 x264；新增 DEPS + vendor target + 6 个平台/架构构建矩阵；自行编译拿不到 Cisco 专利补偿，需法务确认；仍需自备 muxer（复用 `MP4BoxHelper` 只能出 fMP4） |
| S4 平台硬编 | 编码速度最快、CPU 占用最低；H.264 专利由 OS / 芯片厂商承担；mac 与 win 不引入额外二进制；可选用硬件支持的高 profile 与 10bit | 三套平台代码（VideoToolbox / Media Foundation / VAAPI）；Linux 无 GPU 时仍需软件兜底；参数模型弱（无 CRF），同码率画质通常低于 x264；异步回调 + B 帧重排接入复杂；输出仍需自备 muxer；不同 GPU 与驱动输出不一致 |
| S5 随包分发 H264EncoderTools | 编码器现成且经生产验证（AE 插件长期使用）；参数精细（quality 0-100 映射 CRF/VBV/keyint）；mac / win 已有工程与打包脚本；画质与 x264 一致 | GPL-2.0，须提供该工具的完整对应源码或书面 offer；只能进程隔离，文件握手 1ms 轮询 + 每帧写盘；只吐 Annex-B，必须自备 muxer；依赖 Qt，CLI 需替换掉；vendored x264 冻结在 0.164.x，构建不可复现；mac 产物已签名，替换二进制会破坏签名 |
| S6 链接 / dlopen 系统 x264 | 画质与参数控制最好；不新增编码器代码 | GPL-2.0 与 Apache-2.0 单向不兼容，动态链接与 `dlopen` 都无法豁免（GPLv2 没有系统库例外）；链入即要求主库整体按 GPL 分发；用户环境不一定有 `libx264`；版本与 ABI 不可控；法务风险最高 |
| S7 只出帧序列（`--frames-dir`） | 零依赖、零许可证风险；已实现可直接使用；用户可自选编码器、容器与参数；逐帧文件便于排查问题 | 不能一条命令出片；帧落盘带来 IO 与磁盘占用；PNG 编码占单帧成本 62%，是全部方案里最慢的；没有进度与错误汇总 |

按落地成本与风险从低到高排列：**S7 ≈ S1 < S5 < S4 < S3 < S2 < S6（S6 不可行）**。

### 5.1.4 选型结论

1. **默认 S1**：CLI 直出 MP4 走外部 ffmpeg + rawvideo 管道，`--frames-dir` 保留为无 ffmpeg 时的降级路径（S7）。
2. **S2、S6 排除**；**S5 不推荐**（GPL 义务 + 自备 muxer + Qt 依赖替换）；**S4 仅在明确要求零外部依赖且接受三套平台代码时评估**。
3. **S3 作为 S1 的产品化备选**：只有当「npm 装完必须能直接出片」是硬需求时才启动，且法务须先给专利口径。
4. 无论选哪条，编码器输出的都是 Annex-B，与 §5.2 的 `VideoSequence` 结构天然匹配；S1/S2 由 ffmpeg 顺带解决封装，S3/S4/S5 必须自带 muxer。

### 5.2 MP4 封装：fMP4 与普通 MP4

`src/codec/mp4/MP4BoxHelper::CovertToMP4` 已在主库，输入为 `pag::VideoSequence`：

- `headers`：codec specific data，Annex-B（前 4 字节起始码，写包时跳过），至少 SPS + PPS
- `frames`：每帧 `VideoFrame`，含 `frame`（pts）、`isKeyframe`、`fileBytes`（Annex-B NALU）
- `width` / `height` / `frameRate`：取自 `Sequence` 的 public 字段，需自行填充（不从 SPS 解析）

以上字段均为 public，可直接构造实例后调用，无需改动现有代码。x264 / openh264 默认输出即 Annex-B，天然匹配。

**但它产出的是 fragmented MP4**。两种容器的差异：

| | 普通 MP4 | fMP4（当前实现） |
|---|---|---|
| 结构 | `ftyp + moov(含完整 stbl) + mdat` | `ftyp + moov(mvex/trex) + (moof + mdat)*` |
| sample 索引 | `moov.stbl` 的 `stts/stss/stsc/stsz/stco` | 每段的 `moof.traf.trun` |
| 写入时机 | 需知道全部 sample 布局后写，或回填 `stco`/`stsz` | 边写边生成，无需回填 |
| 随机 seek | `stco` + `stss` 直接定位 | 依赖逐段解析，通常还需 `sidx` |
| 典型用途 | 文件分发、剪辑软件 | 直播、录制、DASH/HLS/CMAF |

代码实证：

```82:92:src/codec/mp4/MP4Generator.cpp
int MP4Generator::moov(EncodeStream* stream, bool write) {
  ...
  PushInWriteFun(mvex);      // 有 mvex 即分片 MP4
```

```321:335:src/codec/mp4/MP4Generator.cpp
int MP4Generator::stsc(EncodeStream* stream, bool write) {
  ...
    stream->writeInt32(0);
    stream->writeInt32(0);   // entry_count = 0，空表
```

`stsc` / `stsz` / `stco` 全部写空表，sample 信息都在 `moof` 里。

**兼容性风险**：现代浏览器、QuickTime Player、VLC 都能播 fMP4；但微信内置播放器（老 X5 内核）可能进度条不可拖或只播首段，Premiere / Final Cut 等剪辑软件可能拒绝导入或时长识别错误，Android 老播放器 / SmartTV 可能不认 `moof`。

**三条出路**：

| 方案 | 做法 | 成本 |
|---|---|---|
| A. 改造 muxer | 两遍写：`box(write=false)` 先算 `stbl` 大小（机制已存在），写 `mdat` 时记录每个 sample 的 offset/size，最后回填 `stbl` | 约 300~500 行 |
| B. remux | 先出 fMP4，再 `ffmpeg -i in.mp4 -c copy -movflags +faststart out.mp4` | 零编码成本，需 ffmpeg |
| C. 换 muxer | 用 ffmovie 的 `FFMediaMuxer`（内部 `movflags=faststart`） | 见 §5.4 |

### 5.3 色彩与 alpha

- H.264 本身不带 alpha，默认输出不透明。RGBA 需先 unpremultiply，再与 `--background` 指定颜色合成，最后转 I420（BT.709 有限范围）
- 若需要透明动效，PAG 生态已有成熟方案：**画布扩展 + alpha 灰度区**（见 §5.5 的 `PAGEncoder`），把 alpha 量化后贴在主画面旁边，编码面积 ×2，播放侧按 `alphaStartXY` 裁切还原。不需要 WebM/ProRes
- 该方案对 mp4 同样成立（因为本质是普通 H.264），代价是分辨率与码率翻倍
- **色彩矩阵必须与编码路径一致**：`MP4BoxHelper` 不写 `colr`/`nclx` box（`src/codec/mp4/` 内无相关代码），播放器按容器缺省规则猜。若改用 ffmovie 的 `FFVideoEncoder`，其 `libyuv::ABGRToI420` 固定 BT.601 有限范围，与上面约定的 BT.709 不符，肤色与橙色会明显偏移 —— 要么自己做 709 转换，要么把约定改成 601 并接受播放器的猜测行为

### 5.4 现成资产盘点：ffmovie / ffaudio / H264EncoderTools

libpag 主库没有视频编码能力，但生态里有三个相关仓库。源码级核实后的结论：

| | ffmovie | ffaudio | H264EncoderTools |
|---|---|---|---|
| 定位 | FFmpeg 封装的**多媒体库**（解封装 / 解码 / 编码 / muxer） | FFmpeg 封装的**音频编码库** | 独立进程的 **x264 编码命令行工具** |
| 产物 | `libffmovie`（LGPL-2.1） | `libffaudio`（LGPL-2.1） | 可执行文件（GPL-2.0） |
| 视频编码输入 | `FFVideoEncoder::onSendData(rgba, w, h, rowBytes, pts)`，内部 `libyuv::ABGRToI420` | 无 | 裸 YUV420p 文件 |
| 封装 | `FFMediaMuxer`，`movflags=faststart` → 普通 MP4 | 音频 muxer | 无（只吐 Annex-B 裸流） |
| 质量控制 | 仅 `videoBitrate` + profile | bitrate | CRF（Quality 0-100）+ VBV + keyint |
| 进程模型 | 库内调用 | 库内调用 | 独立进程 + 文件握手 |
| 平台 | android/ios/mac/win/linux/web 构建脚本齐全 | 同 | mac arm64/x86_64 + VS2019 预编译 x264 |

三个判定：

1. **ffaudio 与本需求无关** —— PAGX 规范里没有音频节点，也不需要音轨。
2. **H264EncoderTools 是唯一现成的编码器**，且输出协议（`Header-0.dat` = SPS、`Header-1.dat` = PPS、`H264-<n>.264` = 每帧 Annex-B）与 `pag::VideoSequence` 的 `headers`/`frames` 完全一致，与 `MP4BoxHelper` 零摩擦对接。代价：GPL 必须进程隔离、只收 YUV、每帧一次文件读写。
3. **ffmovie 当前配置下视频编码不可用**。它走 `avcodec_find_encoder(AV_CODEC_ID_H264)`，而各平台 `scripts/build_ffmpeg_*.sh` 只写了 `--enable-encoder=h264,mpeg4`，没有任何 H.264 encoder wrapper（`libx264` / `libopenh264` / `h264_videotoolbox`）。FFmpeg 不存在名为 `h264` 的 encoder，因此 `find_encoder` 会返回 nullptr，`initCodec()` 失败。**它唯一能立即复用的是 `FFMediaMuxer`（普通 MP4 + faststart）。**

**ffmovie 的引入方式：预编译二进制 + Git LFS**，不走 `DEPS` / `vendor.json`：

```
viewer/vendor/ffmovie/
├── include/            ffmovie/movie.h + pag/decoder.h + ffavc.h
├── mac/x64/libffmovie.dylib      3.4 MB   (Git LFS)
├── mac/arm64/libffmovie.dylib    3.4 MB   (Git LFS)
└── win/x64/ffmovie.dll + .lib    7.2 MB   (Git LFS)
```

exporter 与 viewer 各自手写一份引用（`exporter/CMakeLists.txt:149`、`:170-173`，`viewer/CMakeLists.txt:196-218`）：mac 直接链 `.dylib`，win 链导入库；mac 部署时 POST_BUILD 拷进 `Contents/Frameworks` 并 `install_name_tool -add_rpath '@loader_path/../Frameworks'`；win 的 `deployqt.bat` 只处理 Qt，`ffmovie.dll` 由发布流程另行处理。仓库内没有构建 ffmovie 的脚本，升级靠人工替换二进制，无版本锁定与 hash 校验。

若要把 ffmovie 引入 CLI，成本是：先跑它自己的 `build_ffmpeg.sh` 生成 FFmpeg 静态库 → 各平台产出动态库 → npm 包体 +3.4MB(mac) / +7.2MB(win) → 处理 rpath 与 LGPL 声明（LGPL 要求动态链接，静态链接会触发重新链接义务）。

### 5.5 exporter 的视频导出链路（可复用性分析）

`exporter/` 是 AE 插件，**没有「导出 mp4 文件」的功能**：它把 AE 里的视频素材逐帧编码成 H.264 后**内嵌进 `.pag` 的 `VideoComposition`**。完整链路：

```
AE footage
  │ GetVideoSequence()      exporter/src/export/sequence/VideoSequence.cpp:240-390
  ├─ AE 渲染 GetRenderFrame() → RGBA（:308）
  ├─ 可见性裁剪 / 尺寸对齐 / alpha 探测 / 静止帧判定
  ▼
PAGEncoder::encodeRGBA()    exporter/src/export/encode/VideoEncoder.cpp:106-124
  ├─ RGBAToYUV420
  └─ hasAlpha 时画布扩 2 倍，alpha 灰度写进扩展区
  ▼
OfflineVideoEncoder         exporter/src/export/encode/OfflineVideoEncoder.cpp:77-135
  └─ QProcess 启动 H264EncoderTools，逐帧写 YUV 文件、轮询读 .264 文件
  ▼
PAGEncodeThread（QThread）  encode/PAGEncodeThread.cpp
  └─ 回调 → pag::VideoFrame{fileBytes, isKeyframe, frame}
  ▼
MP4BoxHelper::WriteMP4Header(sequence)   VideoSequence.cpp:386
composition->sequences.push_back(sequence)  ← 进 .pag，不落 mp4
```

值得借鉴的机制：

| 机制 | 说明 | 对本项目的意义 |
|---|---|---|
| alpha 画布扩展 | 主画面 + 旁边一块 alpha 灰度图（`(a*219>>8)+16`），记录 `alphaStartXY` | 若 mp4 需要透明，这是 PAG 生态已验证的方案 |
| 静止帧合并 | `ImageIsStatic` 比较相邻帧，静止段降级为 P 帧并合并成静态区间（`staticTimeRange`） | 长静止 + 短动效的稿件可显著减体积 |
| 渲染/编码流水线 | 编码在独立线程，主线程渲染下一帧，1ms 轮询节流 | CLI 侧可用 `std::thread` + 环形缓冲复刻 |
| 外部进程编码 | GPL 的 x264 隔离在独立进程 | 与 §5.4 的结论一致 |

**不可复用的部分**：渲染源是 AE 的 `GetRenderFrame`（需换成 `PAGScene::draw`）；Qt 依赖（`QProcess` / `QThread` / `QApplication::processEvents`）在 CLI 里必须替换；最后一步没有 `avformat` mux，产出的是 PAG 内部结构而非独立文件。

另注：exporter 里 ffmovie 只用于**音频**（`AudioSequence.cpp:45-111` 把 AE 音频采样经 `FFAudioEncoder` + `FFMediaMuxer` 写成临时 mp4，再读进 `composition->audioBytes`），完全不参与视频编码 —— 与上面「视频编码器配置缺失」的判断一致。

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

**结论先行：wasm 下不存在「在子线程自己建设备」这条路。**

`GLDevice::Make()` 在 WebGL 构建里是空实现：

```76:78:third_party/tgfx/src/gpu/opengl/webgl/WebGLDevice.cpp
std::shared_ptr<GLDevice> GLDevice::Make(void*) {
  return nullptr;
}
```

因此 `Devices::MakeDefault()` → `GLDevice::MakeWithFallback()` 在 wasm 上会退化为 `GLDevice::Current()`：

```34:38:third_party/tgfx/include/tgfx/gpu/opengl/GLDevice.h
  /**
   * Returns an GLDevice associated with current OpenGL context. Returns nullptr if there is no
   * current OpenGL context on the calling thread.
   */
  static std::shared_ptr<GLDevice> Current();
```

也就是说：**只有已经绑定 GL context 的线程才能拿到 device，其他线程一律 nullptr**。`PAGSurface::MakeOffscreen` 因此在没有 GL context 的 worker 线程里会失败（`OffscreenDrawable::Make` 中 device 为空即返回 nullptr）。

ardot 导出任务的线程由 `foundation::SubmitTask` 决定：它把任务投递到 `TaskKind::Concurrent` 通道。wasm 线上预算是 `BudgetForWasmMt`（`write=1`、`compute=side-1`，其中 `side = min(2, (cores-1)/2)`）：

| hardwareConcurrency | side | compute_workers | 导出任务实际线程 |
|---|---|---|---|
| ≤4 | 1 | 0 | **inline，主线程** |
| ≥6 | 2 | 1 | **compute worker** |

桌面端 `compute = max(1, hw-1)`，导出确实在子线程；但桌面走 EGL/WGL，`GLDevice::Make()` 能正常新建 pbuffer context，所以子线程 bake 可行。**wasm 没有这个能力**。

两个后果：

1. **现有 bake 路径在 wasm 上可能是降级状态**。`bakeUnsupported` 开启时（SVG/HTML/PPT 默认），exporter 内部的 `GPUContext` 若拿不到 device，bake 只会打 warning 并降级（丢失 `ImagePattern` tiling、带 mask 图层、backdrop 混合等效果）。6 核以上的机器上，这类导出任务会落到 compute worker，存在静默降级的风险，需要实测确认产物完整度。
2. **MP4 渲染必须保证在有 GL context 的线程执行**，可选模型：

| 模型 | 做法 | 代价 |
|---|---|---|
| A（推荐） | 主线程 inline 渲染，每帧结束后 `Submit` 下一帧，让 UI 有喘息 | 8 秒 1080p 需 20~60 秒，期间主导航仍可交互 |
| B | 主线程建设备与 surface，worker 用 `pagx::MakeFrom(device, surface)` + `Record` | wasm GL context 不跨线程共享，纹理可见性需验证 |
| C | worker 建 OffscreenCanvas + 独立 GL context | 依赖 `-sOFFSCREENCANVAS_SUPPORT`（ardot 仅在 hop 构建打开），且像素要 readPixels 回传 |

推荐 A：改动最小，不依赖 OffscreenCanvas 编译开关，也不涉及跨线程纹理共享。

锁生命周期仍需错开：`bakeImagePaintFilters` 是 RAII 持锁、函数返回即释放，MP4 任务应在之后提交。

---

## 7. 风险与限制

| 风险 | 影响 | 对策 |
|---|---|---|
| 编码器许可证 | 误链或误 dlopen GPL 的 x264，会迫使整个主库按 GPL 分发 | 只走外部进程（S1），不链接不 dlopen；随包分发 GPL 工具时须履行源码提供义务（§5.1.1） |
| CLI 依赖外部 ffmpeg | 用户环境未安装时无法出片 | 启动时探测并给出安装指引，降级到 `--frames-dir`（S7） |
| H.264 无 alpha | 透明动效无法导出 | 明确不支持，或另开 WebM/MOV 链路 |
| PNG 中间编码占单帧成本 62% | 出片慢 | 接编码器时直接 RGBA → YUV，跳过图片编码 |
| fMP4 兼容性 | 部分播放器/剪辑软件打不开 | 需要时改造为普通 MP4 |
| 循环不闭合 | 首尾衔接跳变 | 产品定义时长规则（按最短周期截断 / 强制对齐） |
| 多动画周期不一致 | 无法自然定长 | 默认取最长动画一个周期，允许 `--duration` 覆盖 |
| 状态机驱动的动效 | 交互逻辑无法离线复现 | 只渲染时间轴，不模拟状态转换（当前样例的 StateMachine 未被引用，无影响） |
| wasm 跨语言传帧 | 内存带宽开销 | 分批回调 / 必要时降采样 |
| wasm 无 GL context 的线程拿不到 device | 渲染任务落错线程会直接失败 | 渲染固定在主线程（分帧让出），或由主线程传 device（见 §6.3） |
| wasm 6 核以上导出任务可能进 compute worker | 现有 SVG/HTML/PPT bake 可能静默降级 | 实测确认产物完整度，必要时把 bake 拉回主线程 |

---

## 8. 后续计划

| 阶段 | 内容 | 状态 |
|---|---|---|
| P0 | `pagx render --frames-dir` 复用 scene 逐帧渲染 + 一致性验证 | ✅ 已完成（`9b5a1262c`） |
| P1 | 按 §5.1 落地 S1：CLI 接外部 ffmpeg + rawvideo 管道直出 MP4（`--frames-dir` 降级保留），编码与封装一并解决 | 方案已定，待实施 |
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
