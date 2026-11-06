# PAGX 导出 MP4：实施方案

本文只写已确定的方案与实现约定，面向编码。选型论证过程不在本文范围内。

- 输入：已带动画轨道的 `pagx::PAGXDocument` 或 `.pagx` 文件
- 输出：MP4（H.264 + AAC）
- 落点：`pagx export --format mp4`（native CLI）与 ardot 编辑器（web 版与 CEF 桌面版共用 JS 侧 WebCodecs 编码层，见 §10）

## 1. 方案概要

| 项 | 结论 |
|---|---|
| 命令形态 | `pagx export --format mp4`（后续视频格式同走 `--format`） |
| 编码器形态 | 外部 ffmpeg **可执行文件** + 子进程 rawvideo 管道 |
| ffmpeg 来源 | 部署问题，可替换：用户环境 / 随包分发 / 内部构建，不影响代码结构 |
| 编码与封装位置 | 主库之外。pagx 不链接任何 H.264 编码器，只出帧 |
| 帧产出 | 库层 `VideoExporter`，pull 式一次一帧，native 与 wasm 共用 |
| native 编码 | ffmpeg 子进程（`libx264`，可按平台换 `h264_videotoolbox`） |
| web 编码 | JS WebCodecs `VideoEncoder` + mediabunny |
| 音频 | P1 不产出（`-an`），接口按 §5 预留 |
| 其他视频格式 | P1 只支持 mp4，webm/gif 不做占位；扩展点见 §4.3 |
| 编码器可替换 | `--encoder` 换编码器；无 CRF/preset 选项的编码器（如 `h264_videotoolbox`）配 `--no-crf` / `--no-preset` |
| 尺寸对齐 | `yuv420p` 等 4:2:0 格式要求偶数宽高，`ceil(size * scale)` 向上取偶 |

形态由三条硬约束决定：

1. H.264 编码器不能进 Apache-2.0 主库（许可证与平台差异都无法收编）。
2. wasm 无法 spawn 子进程，web 端编码只能在 JS 侧完成。
3. wasm 下 1080p 单帧渲染 + 回读约 50ms，整段一次跑完会阻塞主线程十几秒。

## 2. 架构

```
┌─ 装配层 ──────────────────────────────────────────────────┐
│ native CLI / 任意 C++ 宿主：VideoExporter + ffmpeg 子进程   │
│ wasm（ardot）：VideoExporter + JS WebCodecs + mediabunny    │
└───────────────────────────────────────────────────────────┘
┌─ 编码后端（宿主实现，pagx 不感知）─────────────────────────┐
│ native：ffmpeg stdin 管道（rawvideo rgba）                  │
│ web：   WebCodecs VideoEncoder(avc1) + muxer               │
└───────────────────────────────────────────────────────────┘
┌─ 帧产出（pagx 公开 API，native/wasm 完全一致）─────────────┐
│ include/pagx/VideoExporter.h + src/pagx/video/VideoExporter.cpp │
└───────────────────────────────────────────────────────────┘
```

视频文件由编码后端产出（ffmpeg 进程直接写盘 / muxer `finalize()` 出字节），装配层不产出任何视频数据，只负责参数决策、驱动循环与错误编排。

| 文件 | 职责 |
|---|---|
| `include/pagx/VideoExporter.h`（新增） | `VideoExportOptions` + `VideoExporter`，与 SVG/HTML/PPT 导出器同构 |
| `src/pagx/VideoExporter.cpp`（新增） | 逐帧时间轴循环（承载现 `CommandRender.cpp` 的逻辑）。放根目录而非 `video/` 子目录，因为 `src/pagx/*.cpp` 是非递归 GLOB，子目录需逐条登记 |
| `src/cli/VideoEncoder.h/.cpp`（新增） | ffmpeg 发现、格式表、命令行构建（纯函数）、子进程封装、帧写入 |
| `src/cli/CommandExport.cpp`（改） | `--format mp4` 分支、视频相关参数解析 |
| `src/cli/CommandRender.cpp`（改） | `--frames-dir` 改用 `VideoExporter` |

L1 必须放 `src/pagx`：ardot 的 `pagx.cmake` 只摘编 `src/pagx` + `src/renderer`。若该 cmake 是显式文件列表而非 GLOB，新增文件需同步登记。

## 3. 库层：VideoExporter

```cpp
namespace pagx {

/**
 * Export options for VideoExporter.
 *
 * ABI stability: this struct is part of the public API. New optional fields must be appended at
 * the end with sensible defaults so that callers compiled against an older header continue to
 * link and behave correctly.
 */
struct VideoExportOptions {
  /** Sampling frame rate; 0 means "use the document animation frame rate". */
  int frameRate = 0;
  /** Sequence length in seconds; <= 0 means "the longest animation's duration". */
  float duration = -1.0f;
  /** Output scale factor applied to the document size. */
  float scale = 1.0f;
  /** Background color drawn under the content. Whether it must be opaque is decided by the
   *  output container, not here: H.264 carries no alpha, VP9 does. */
  bool hasBackground = false;
  Color background = {};
  /** Round the output width and height up to even numbers; 4:2:0 codecs cannot take an odd one. */
  bool alignToEvenSize = false;
};

/**
 * Produces an RGBA frame sequence from a document's animations. The scene and surface are created
 * once and reused for every frame; frames are pulled one at a time so that hosts can interleave
 * other work (a UI thread, an encoder queue) between them.
 *
 * The exporter only renders. Encoding and muxing are the host's responsibility: ffmpeg on native,
 * WebCodecs on the web. No codec ships inside pagx.
 *
 * Frames are premultiplied RGBA. Containers that carry alpha need the host to unpremultiply;
 * opaque output (the mp4 path) can pass the pixels through unchanged.
 *
 * Constraints:
 * - Make() must run on a thread that owns a GPU context; on wasm that is the main thread.
 * - Callers must not hold the same device's context lock while calling nextFrame().
 * - PAGScene::Make() applies layout when the document has not applied it yet, so callers only
 *   need to register fonts on PAGXDocument::fontConfig() beforehand when it needs external fonts.
 *
 * The document is taken by shared_ptr because PAGScene retains it for the exporter's lifetime;
 * PAGXDocument does not derive from enable_shared_from_this.
 */
class VideoExporter {
 public:
  using Options = VideoExportOptions;

  static std::shared_ptr<VideoExporter> Make(std::shared_ptr<PAGXDocument> document,
                                             const Options& options = {},
                                             std::string* errorMsg = nullptr);

  int width() const;
  int height() const;
  int frameRate() const;
  int64_t frameCount() const;
  int64_t currentFrame() const;

  /** Renders the next frame. Returns false at the end of the sequence or on failure. */
  bool nextFrame();

  /** Premultiplied RGBA8888. Valid until the next nextFrame() call. */
  const uint8_t* pixels() const;
  size_t rowBytes() const;
};

}  // namespace pagx
```

四条使用约束：

1. **必须 pull**：宿主一次取一帧，中间可以 `SubmitTask` 让出主线程。禁止一次跑完全部帧。
2. **`Make()` 绑定 GL 线程**：wasm 下 `Devices::MakeDefault()` 只返回当前线程已绑定的 device，其他线程是 nullptr。
3. **是否要求不透明由输出格式决定**：mp4 要求不透明，`alpha=255` 时预乘等于直通，无需 unpremultiply；将来支持带 alpha 的容器时由宿主做 unpremultiply，库不参与该决策。
4. **`pixels()` 只到下一次 `nextFrame()`**：复用同一块 surface 内存，宿主必须当场消费。

宿主调用形态：

```cpp
// native: the CLI wiring, VideoEncoder lives in src/cli (not in the library)
auto exporter = VideoExporter::Make(document, videoOptions, &errorMsg);
auto encoder = VideoEncoder::Start(ffmpegPath, *FindVideoFormat("mp4"), encodeOptions, &errorMsg);
while (exporter->nextFrame()) {
  encoder->writeFrame(exporter->pixels(), exporter->rowBytes(), &errorMsg);
}
encoder->finish(&errorMsg);
```

```ts
// wasm / JS
while (exporter.nextFrame()) {
  const view = HEAPU8.subarray(ptr, ptr + rowBytes * height);   // 零拷贝视图
  const frame = new VideoFrame(view, {format: 'RGBA', codedWidth: width, codedHeight: height,
                                      timestamp: currentFrame * 1e6 / fps});
  encoder.encode(frame, {keyFrame: currentFrame % gop === 0});
  frame.close();
  if (encoder.encodeQueueSize > 8) await drainEncoded();
  await yieldToUI();
}
```

## 4. CLI 层

### 4.1 命令与参数

```
pagx export --format mp4 --input in.pagx --output out.mp4
            [--fps N] [--duration S] [--scale F] [--background #RRGGBB]
            [--crf 0-51 | --no-crf] [--preset <name> | --no-preset]
            [--encoder libx264] [--encoder-args "<args>"]
            [--font <path>] [--fallback <path|name>]
            [--ffmpeg <path>] [--dry-run]
```

`--format` 也可从 `--output` 扩展名推断（沿用现有机制）。多输入仅 pptx 允许，视频格式强制单输入。视频专用参数用在 svg/pptx/html 上会直接报错而不是静默忽略；`--font` / `--fallback` 是全局参数，但只有 mp4 分支会消费。

| 参数 | 默认 | 说明 |
|---|---|---|
| `--input <file>` | 必填 | 单输入（视频格式不允许重复） |
| `--output <file>` | `<input>.<format>` | 输出路径 |
| `--format mp4` | 从 `--output` 推断 | 目前只接受 `mp4`，其他值走现有 unsupported 分支 |
| `--fps <n>` | 文档动画帧率，缺省 60 | 采样帧率 |
| `--duration <s>` | 最长动画一个周期 | 序列时长 |
| `--scale <f>` | 1.0 | 缩放（`export` 此前无此参数，需新增） |
| `--background` | `#FFFFFF` 不透明 | 与 Figma 行为一致；mp4 要求 alpha=255，传入带 alpha 的颜色直接报错 |
| `--crf <0-51>` | 20 | 质量 |
| `--no-crf` | — | 不下发 `-crf`，供 `h264_videotoolbox` 等无 CRF 选项的编码器使用 |
| `--preset` | `medium` | x264 preset |
| `--no-preset` | — | 不下发 `-preset`，同上 |
| `--encoder` | 取格式表默认值 | 内部版可换 `libo264rt`，mac 可换 `h264_videotoolbox` |
| `--encoder-args` | 空 | 原样追加到命令行 |
| `--font` / `--fallback` | — | **必须新增**。`export` 现有路径不加载字体（靠文档内嵌 GlyphRun），而 mp4 是真实渲染，缺少时会 fallback 或渲染不出文本 |
| `--ffmpeg <path>` | — | 显式路径，优先级最高 |
| `--dry-run` | false | 打印最终命令行而不执行 |

ffmpeg 发现顺序：`--ffmpeg` > `FFMPEG_BIN` 环境变量 > PATH 探测。全部失败时报错并给出安装指引与 `--frames-dir` 降级提示。

### 4.2 ffmpeg 命令

```bash
ffmpeg -hide_banner -loglevel error -y \
  -f rawvideo -pixel_format rgba -video_size {W}x{H} -framerate {fps} -i pipe:0 \
  -an -c:v {encoder} -preset {preset} -crf {crf} -pix_fmt {pixFmt} \
  -vf scale=in_color_matrix=bt709:out_color_matrix=bt709:out_range=tv,format={pixFmt} \
  -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv \
  -movflags +faststart -f {container} {out}.part
```

四个必须处理的细节：

1. **rowBytes 对齐**：`rawvideo` 没有 stride 概念。若 `pixmap.rowBytes() != width * 4`，必须逐行打包再写，否则画面斜切。
2. **SIGPIPE**：ffmpeg 提前退出时 `write()` 触发 SIGPIPE 会直接杀掉 CLI。视频路径启动前 `signal(SIGPIPE, SIG_IGN)`，把 `EPIPE` 转换为「ffmpeg 提前退出（退出码 N）」。
3. **`-f {container}` 必须显式写**：临时文件后缀是 `.part`，ffmpeg 无法从后缀推断封装格式。
4. **临时文件 + rename**：先写 `{out}.part`，`waitpid` 返回 0 后才改名。顺序必须是「关闭 stdin → 等待退出码 → 改名」。

色彩必须双向指定：`-colorspace/-color_primaries` 只写容器元数据，真正决定 swscale 转换矩阵的是 `-vf scale=...in_color_matrix/out_color_matrix`。只写前者会在播放器里出现肤色偏移。

**实测补充（ffmpeg 7.1.1 / libx264）**：`-color_primaries bt709 -color_trc bt709` 实际**不会**写进 H.264 VUI，`ffprobe` 仍报 `color_primaries=unknown / color_transfer=unknown`，只有 `-colorspace` 生效。要写进 VUI 必须用 `-x264-params colorprim=bt709:transfer=bt709:colormatrix=bt709`，但那是 x264 专属、会破坏 `--encoder` 可替换性，因此**不下发**；需要严格元数据的用户用 `--encoder-args` 自行补。像素结果不受影响（矩阵由 `-vf` 决定）。

**尺寸**：`yuv420p` 等 4:2:0 像素格式要求宽高为偶数，CLI 会把 `ceil(size * scale)` 向上取偶（`VideoExportOptions::alignToEvenSize`），并在 stderr 提示实际尺寸。

### 4.3 格式扩展点

P1 只实现 mp4，但参数分派按一张格式表走，新增单遍编码格式（如 webm/VP9）只需追加一行：

```cpp
// src/cli/VideoEncoder.h
/** Describes one output container. P1 ships mp4 only; new single-pass formats append a row. */
struct VideoFormatSpec {
  std::string name;           // --format value, e.g. "mp4"
  std::string container;      // ffmpeg -f value
  std::string defaultEncoder; // e.g. "libx264"
  std::string pixelFormat;    // e.g. "yuv420p"
  bool requiresOpaque;        // mp4: true (H.264 has no alpha)
};

const VideoFormatSpec* FindVideoFormat(const std::string& format);  // nullptr => unsupported
```

```cpp
static const VideoFormatSpec kVideoFormats[] = {
    {"mp4", "mp4", "libx264", "yuv420p", true},
};
```

两点保留说明：

- **webm** 属"追加一行"类型（`libvpx-vp9` + `yuva420p` + `requiresOpaque=false`），但透明输出需要宿主先 unpremultiply。
- **gif 不能只靠加一行**：`palettegen` 需统计完整片段，`split` + `palettegen` 会把整段视频缓冲在滤镜图里，管道输入会 OOM。到时要么限制时长/尺寸，要么先出中间 mp4 再二次转码，是一个独立流程分支。

## 5. 音频

**P1 未实现**：当前实现固定下发 `-an`，产物没有音轨；`--audio` 参数不存在。以下为后续设计。音频**不进入 `VideoExporter`**，由宿主装配后交给编码器；将来走 `pagx export --format mp4 --audio <file>`：

| 层 | 音频职责 |
|---|---|
| `VideoExporter` | 不感知音频，只出帧 |
| CLI / 宿主装配 | 决定音频来源、起点、裁剪、循环 |
| ffmpeg（native） | 解码、重采样、AAC 编码、混流一次完成 |
| WebCodecs（web） | `AudioEncoder` + muxer 混流 |

音频是时间轴上的**带起止片段**，不是「有没有」：

```cpp
struct AudioClip {
  std::string filePath;   // 或内嵌资源引用（P2）
  int64_t startUs = 0;    // 在文档时间轴上的起点
  int64_t durationUs = 0; // 0 表示到素材结尾
  float volume = 1.0f;
};
```

单段（例：4s 视频，只有 2-3s 有声）：

```bash
ffmpeg ... -i clip.m4a -map 0:v:0 -map 1:a:0 \
  -af "atrim=0:1,asetpts=PTS-STARTPTS,adelay=2000:all=1,apad" \
  -c:a aac -b:a 128k -ar 48000 -ac 2 \
  -t 4 -movflags +faststart -f mp4 out.mp4.part
```

- `atrim` 裁剪素材，`asetpts=PTS-STARTPTS` 重置时间戳（裁完不重置会让 `adelay` 叠加出错）
- `adelay` 前置静音把音频摆到 2s，`apad` + `-t` 补齐尾部，产物是 0→4s 的连续音轨
- 不用 `-itsoffset` 制造「音轨从 2s 才开始」：那需要 edit list，老播放器与剪辑软件解释不一

多段用 `filter_complex` + `amix`，`normalize=0` 必须加，否则叠加区音量减半。

web 侧对应做法：补齐静音帧让音轨从 0 开始连续，不要把首段 timestamp 直接设成 `2e6`。AAC 编码前用 `AudioEncoder.isConfigSupported()` 探测，不支持则降级为无音轨，不要把 Opus 塞进 mp4。

AAC 的 encoder delay（约 20ms priming samples）由 ffmpeg 写 `elst` 补偿，这是相对自研 muxer 的隐性优势。

## 6. 编码器来源

P1 只依赖「一个能跑的 ffmpeg 可执行文件」，来源作为部署问题处理。已确认的边界：

| 来源 | 结论 |
|---|---|
| 用户环境 ffmpeg | 默认形态，零分发义务 |
| `ffmpeg-static` | 其 mac 产物 `ffmpeg -L` 自报 not legally redistributable（`--enable-nonfree`），且不提供源码与 configure 参数。仅可开发期自用，不可随包分发 |
| `O264RT`（内部，OpenH264 衍生） | BSD-2，可经 `--enable-libo264rt` 编进 ffmpeg；配 `--disable-gpl` 可得整体 LGPL 构建，S2b 因此重新可行。仅限腾讯自有产品分发，不可进开源仓库、不可转授第三方 |
| `txc_264_enc`（内部，x264 fork） | 受 x264 许可证与公司停止合作影响，不采用 |

内部版 ffmpeg 只需把 `--encoder` 换成 `libo264rt`（Win/Linux）或 `h264_videotoolbox`（mac），CLI 代码不变。

**已实现的发现顺序**（`FindFFmpeg()`）：`--ffmpeg` > `FFMPEG_BIN` > PATH。显式路径或 `FFMPEG_BIN` 指向非可执行文件时直接报错，不回退 PATH，避免掩盖配置错误；PATH 中的空条目（含义是当前目录）会被跳过。全部失败时报错并给出安装指引与 `pagx render --frames-dir` 降级提示。

**换编码器的实测注意**：`h264_videotoolbox` 不接受 `-preset` / `-crf`，必须配 `--no-preset` / `--no-crf`（默认会下发这两个选项，ffmpeg 会直接报未知选项）。已在 macOS 上用 `--encoder h264_videotoolbox --no-crf --no-preset` 实测出片成功。

## 7. 与 Figma 的对标

Figma 导出样例（808×332 / 30fps / 2.07s / h264 High）的实测特征，作为默认值依据：

| 项 | Figma 行为 | 我们的取值 |
|---|---|---|
| 背景 | `#FFFFFF` 不透明（原始 YUV `Y=235 / Cb=Cr=128`） | 默认白底不透明 |
| muxer | WebCodecs + mediabunny（`MediabunnyVideoHandler`） | ardot 侧同栈 |
| 色彩 | `yuv420p(tv, bt709/bt709/iec61966-2-1)` | bt709 matrix + tv range，`-color_trc` 用规范值 `bt709` |
| 尺寸 | 808×332，非 16 整数倍 | 不做对齐约束，按 scale 后取整 |
| 音轨 | AAC LC 48kHz stereo | 见 §5 |

## 8. 实施顺序

1. `include/pagx/VideoExporter.h` + `src/pagx/VideoExporter.cpp`，把 `CommandRender.cpp` 的时间轴循环搬进去。**已完成**
2. `--frames-dir` 改用 `VideoExporter`，验证帧序列输出与现状逐字节一致。**已完成**（20 帧样例，全部帧与 `--time` 路径逐字节一致）
3. `src/cli/VideoEncoder.*`：格式表 + ffmpeg 发现 + 命令行构建 + 子进程封装。**已完成**
4. `CommandExport::exportMp4()`：新增视频参数与 `--format mp4` 分支，先跑 `--dry-run` 验证命令行。**已完成**
5. 端到端：480 帧样例出片，与 `--frames-dir` + 手工 ffmpeg 的结果做时长与像素对比。**已完成**

第 5 步实测（1200×738 / 60fps / 8s / 480 帧，样例 `a9ad04e7…pagx`）：

- `pagx export` 产物与「`--frames-dir` 480 张 PNG + 同参数手工 ffmpeg」产物 **md5 相同**（`6d0b04ab…`），解码后的 RGBA 流逐字节相同
- 整片 PSNR 相对源帧序列：`y:47.59 u:48.71 v:49.99 average:48.09 dB`
- 出片耗时 14.3s（≈30ms/帧，含渲染 + 编码）

## 9. 验证命令

```bash
# 构建（新增源文件靠 GLOB 收集，必须重新 configure）
cmake -G Ninja -DPAG_BUILD_CLI=ON -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target pagx-cli

# 命令行预演：只打印 ffmpeg 命令行，不启动子进程、不落盘
./cmake-build-debug/pagx export --input input.pagx --output /tmp/out.mp4 --dry-run

# 出片
./cmake-build-debug/pagx export --input input.pagx --output /tmp/out.mp4 --fps 60 --crf 20

# 校验产物
ffprobe -hide_banner -v error -show_entries \
  stream=codec_name,profile,width,height,pix_fmt,r_frame_rate,nb_frames,color_space,color_range \
  -show_entries format=duration -of default=noprint_wrappers=1 /tmp/out.mp4

# 与帧序列路径比对（两条路径的 mp4 应逐字节相同）
./cmake-build-debug/pagx render --frames-dir /tmp/frames --fps 60 input.pagx
ffmpeg -hide_banner -loglevel error -y -framerate 60 -i /tmp/frames/f_%04d.png -an \
  -c:v libx264 -preset medium -crf 20 -pix_fmt yuv420p \
  -vf "scale=in_color_matrix=bt709:out_color_matrix=bt709:out_range=tv,format=yuv420p" \
  -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv \
  -movflags +faststart -f mp4 /tmp/manual.mp4
cmp /tmp/out.mp4 /tmp/manual.mp4 && echo identical

# 单元测试（ffmpeg 缺失时相关用例自动 SKIP）
cmake --build cmake-build-debug --target PAGFullTest_OpenGL
./cmake-build-debug/PAGFullTest_OpenGL --gtest_filter='PAGXCliTest.Video*:PAGXCliTest.Export*'
```

帧序列与 mp4 的比对前提是**背景不透明**（默认 `--background #FFFFFF`），此时预乘等于直通，PNG 与 rawvideo 送进编码器的字节完全一致；带透明像素时预乘值会被当作直通值，两条路径都不适用。

一致性对比时时间点必须精确对齐：`--time` 接收秒（float），帧序列内部按 `llround(frame * 1e6 / fps)` 计算微秒，`fps` 取 100/50/25 这类可被十进制精确表达的值，否则会引入几个微秒偏差，导致像素对比出现假阳性。

## 10. Web 端（WebCodecs）

### 10.1 已核实的前提

| 事实 | 依据 |
|---|---|
| CEF 桌面端基于 **Chromium 144**，WebCodecs 可用 | `third_party/cef/mac-arm64/include/cef_version.h`: `CHROME_VERSION_MAJOR 144` |
| ardot 桌面端的编辑器渲染核心**跑在 CEF 渲染进程内**，与 JS 同进程 | `packages/frontend/desktop/macos/CMakeLists.txt` 的 renderer helper 链入 `desktop/src/render_process/*`（`EditorCanvas` 自建 tgfx `GLDevice`） |
| ardot 的 `pagx` 库用非递归 GLOB 收集 `src/pagx/*.cpp`，`VideoExporter` 自动编入 | `packages/common/editor/pagx.cmake:42` |
| 仓库内 web SDK 的 wasm **不含 pagx** | `PAG_BUILD_PAGX` 默认 OFF（root `CMakeLists.txt:51`），`web/script/cmake.demo.js` 不传该开关 |
| libpag 仓库与 ardot 目前**都没有** WebCodecs / muxer 代码 | 全量 grep 无命中 |

因此 web 端编码层需要**新增**，且三段宿主（viewer / ardot web / ardot 桌面）共用同一份实现，只在“帧源适配器”上分叉。

### 10.2 宿主 × 后端 × 编码器

| 宿主 | 帧源 | 回读 | 编码 + 封箱 |
|---|---|---|---|
| libpag viewer（示例/回归） | wasm `VideoExporter` 或 canvas | 同步（WebGL） | WebCodecs + JS muxer |
| ardot web 版 | wasm（GL / WebGPU） | 同步 / **异步**（WebGPU 必须） | 同上（同一份代码） |
| ardot 桌面版（CEF） | native（渲染进程内 GL device） | 同步 | 同上（同一份代码） |
| pagx CLI / 无 CEF 的批处理 | 库内 `VideoExporter` | 同步 | ffmpeg 子进程（§4） |

同步/异步差异**属于渲染后端**而非宿主：WebGL/Metal/D3D12 可同步读回，WebGPU 必须 `mapAsync`（非 Asyncify 构建下同步 `Surface::readPixels` 不可用）。所以帧源接口一律设计成 async，同步实现用 `async` 包一层。

### 10.3 实测基线（ardot 桌面端，CEF / Chromium 144，1080×1920）

三轮探测，逐轮排除干扰：

| 轮次 | 内容 | 结果 |
|---|---|---|
| 1. 支持性 | `isConfigSupported` @1080×1920@60 | `avc1.42E01E`（Baseline **L3.1**）**false**；`avc1.4d0028`（Main L4.0）**true**；`vp09.00.10.08` **true** |
| 2. 平坦帧吞吐 | 全 0 帧 × 60 | `prefer-hardware` 6.87ms/帧，`prefer-software` 4.44ms/帧 |
| 3. 真实内容吞吐 | 渐变 + 20 个动矩形 + 文字 × 60 | `prefer-hardware` 11.65ms/帧（85.8fps），`prefer-software` 12.07ms/帧（82.8fps） |

结论：

1. **H.264 可用**（Main profile），说明该 CEF 构建带 proprietary codecs；桌面端可以走 WebCodecs。
2. **`avc1.42E01E` 失败不是缺编解码器**，是 level 与分辨率不匹配（L3.1 上限 ≈1280×720@30）。codec string 的 level 必须与输出尺寸/帧率匹配。
3. 第 2 轮数字无参考价值：平坦帧编码成本接近 0，测到的是帧拷贝开销。
4. 第 3 轮两路径只差 3.6%，远小于硬编/软编应有的 3–5 倍差距，推测**两条路都在软编**；但 `VideoFrame(RGBA)` → I420 的转换成本（1080p 约 4–8ms）会同时压住两条路径，需按 §10.7 进一步确认。
5. **无论硬编是否生效，编码都不是瓶颈**：12ms/帧 ≈ 83fps，高于 60fps 采集节奏；480 帧约 5.6s 纯编码，与 native 路径（14.3s 含渲染）同量级。因此 `hardwareAcceleration` 保持默认 `'no-preference'`，**不强制硬编**（第 2/3 轮里强制硬编均无收益甚至更慢）。

### 10.4 codec 选择策略

不维护“尺寸 → level”硬编码表，改为**从高到低探测 + 结果缓存**，且必须带真实的 `width/height/framerate`：

```text
avc1.640034 → avc1.640033 → avc1.4d002A → avc1.4d0028 → avc1.42E01F → vp09.00.10.08
```

逐个 `VideoEncoder.isConfigSupported()`，取第一个 `supported === true` 的；全部失败才向用户报“该浏览器不支持 H.264/VP9 编码”。其他约定：

- `latencyMode: 'quality'`，关键帧间隔 2s（`8000 * 2` 个微秒）
- 码率按像素数从 1080p 的 8–12 Mbps 线性缩放
- 不暴露 `hardwareAcceleration` 给上层（见 §10.3 结论 5）

### 10.5 帧源接口

```ts
interface VideoFrameSource {
  width: number; height: number; frameRate: number; frameCount: number;
  /** 第 index 帧的紧凑 RGBA（已去 stride），timestampUs = round(index * 1e6 / fps) */
  readFrame(index: number): Promise<{ rgba: Uint8Array; timestampUs: number }>;
}
encodeToMp4(source: VideoFrameSource, params: EncodeParams): Promise<Blob>
```

- `encodeToMp4` 是唯一的共享实现：编码配置 + `VideoFrame` 构造 + 时间戳 + muxer + 进度回调 + 取消。
- 三个宿主各写一个 adapter：viewer / ardot-web（wasm 堆）、ardot 桌面（native buffer，可零拷贝包成 external ArrayBuffer）。
- 时间戳必须与 native 侧一致（`llround(index * 1e6 / fps)`），否则无法做跨端产物对比。

### 10.6 已知坑

1. **canvas 抓帧必须同 task**：tgfx 的 WebGL canvas 没有开 `preserveDrawingBuffer`（`WebGLDevice.cpp` 只透传 attrs，pagx/viewer 侧未开启），`VideoFrame(canvas)` 必须紧跟 `_draw()` 在同一个任务里执行，跨 `await` 会拿到清屏后的空帧。
2. **WebGPU 必须异步**：非 Asyncify 构建下同步 `readPixels` 不可用，帧源 adapter 要 await 每帧回读完成。
3. **RGBA→I420 转换是主要成本**（1080p 约 4–8ms/帧），两条编码路径都要付。桌面端的优化方向是 native 直接出 I420/NV12（待确认 tgfx 的 `PixelFormat` 是否支持）。
4. **不能把 wasm/native 内存直接交给 `VideoFrame`**：构造后编码器会异步引用，必须拷贝或转移所有权（external ArrayBuffer + 释放回调）。
5. **渲染进程崩溃面**：CEF 桌面端编码器在渲染进程内，长序列导出要有内存上限与取消（`encoder.close()` + 丢弃 muxer）。
6. **产物不能跨端逐字节对齐**：viewer(WebGL) / ardot(WebGPU) / native(ffmpeg) 渲染与编码都不同，跨端只能用 PSNR/SSIM 门禁；逐字节一致只在同宿主同后端内成立。

### 10.7 待验证

- CEF 桌面端到底用的是硬编还是软编：看 `chrome://gpu` 的 **Video Encode** 行，或 `--enable-logging=stderr --v=1` 日志里的编码器实现名（`H264VideoToolboxEncoder` vs `OpenH264`）。
- tgfx / libpag 是否能直接产出 I420，以省掉 §10.6 第 3 条的转换成本。
- 真实 480 帧端到端耗时（渲染 + 编码）与 native 的 14.3s 对比，决定是否值得为桌面端引入 I420 直出优化。
