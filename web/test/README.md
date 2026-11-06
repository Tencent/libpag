# libpag Web 测试车道

在浏览器中运行原生 `test/src` 的 Google Test 测试套件，渲染后端为 **WebGL** 或 **WebGPU**，
测试二进制经 Emscripten 编译为 WebAssembly，由 Puppeteer 驱动。运行器校验实际后端、
构建模式和成套文件指纹，通过分块读取 MEMFS 回传结果，不依赖 CDN、ZIP 或浏览器下载。
`web/test` 覆盖 C++ 测试；发布 SDK 和非 Asyncify 浏览器集成由 `web/cypress` 单独覆盖。

## 前置条件

- Node.js 与 `web/emsdk-versions.txt` 钉版的 emsdk：

  ```bash
  node web/script/setup.emsdk.js
  source web/emsdk/emsdk_env.sh
  cd web/test && npm install
  ```

- tgfx JS binding（构建一次即可；wasm 侧通过这些 binding 在页面加载时注入的
  `Module.tgfx` / `Module.ScalerContext` 读取浏览器专属能力）：

  ```bash
  cd third_party/tgfx/web && npm install && npm run build:tgfx
  ```

  `copy:*` 脚本会自动从该处拷贝 `tgfx.esm.js`。

## 用法

```bash
# 在项目根目录执行：从 main 生成基线缓存（首次运行一次，之后每次接受基线变更后再运行）：
./update_baseline.sh USE_WEBGL           # 或 USE_WEBGPU
./update_baseline.sh USE_WEBGL --skip-images   # CI 模式：仅生成 md5 缓存

# 在当前分支上运行测试：
./autotest.sh USE_WEBGL                  # 或 USE_WEBGPU
# 或直接在 web/test 下执行：
npm run test:webgl                       # 构建 + 运行
npm run autotest:webgl                   # 仅运行（复用 build-webgl）
```

## 工作原理

1. 普通构建使用 `build-webgl` / `build-webgpu`，显式关闭 `PAG_UPDATE_BASELINE`。
   生成缓存每次使用 `build-baseline-{backend}` 下的新目录，不继承 TGFX 路径或旧合包。
2. `artifacts.js` 将 JS、WASM、DATA、TGFX binding 和页面发布到
   `dist/{backend}-{compare,update}`。每次运行先复制完整工件到独立快照，再校验 SHA-256；
   HTTP 仅服务该快照，重新构建或发布不会替换运行中的文件。
3. `run_test.js` 仅在 `127.0.0.1` 的随机端口提供本次产物，通过 Puppeteer 启动 Chrome。
   页面检查 WASM 导出的 `GetBuildInfo`，再注入只读缓存并调用 `RunAllTests` / `RunTest`。
   默认保留浏览器沙箱；确有需要的隔离 CI 环境可显式传递 `--no-sandbox`。
4. `GetRunResult` 返回执行、失败、跳过数量，以及实际 MD5 比较、版本跳过、缺少基线数量。
   无测试执行、缺少基线、后端/模式不符、浏览器错误或必需报告丢失均返回非零。
5. 关闭浏览器前按 256 KiB 分块读取输出，写入独立的 `results/{backend}-{mode}-{id}`。
   包含 `run-result.json`、GoogleTest `report.json` 和失败图等产物。
   `--output` 可指定仓库内尚不存在的输出目录；根 `autotest.sh` 将本轮结果直接放入 `result/`。

普通测试只读 `.cache/{backend}`，不自动刷新或合并期待值。只有基线与缓存 key 都存在且版本
不同时才保留版本跳过语义；缺失版本或 MD5 是失败，不是接受变更。普通测试不生成 `_base` 图。
缓存刷新会把本地改动（含未跟踪文件）自动 stash，切到 `origin/main` 构建并在结束后恢复现场；
要求 `origin/main` 已包含本协议，同时验证 `third_party/tgfx` 为 main 钉版的干净源码。
不回退到待测分支；仅完整成功后整体替换缓存，并记录来源及文件摘要。
普通比较拒绝缺失 `provenance.json`、来源不可信或内容不一致的旧缓存，不自动补写来源。
缓存源提交可不同于当前候选提交，但必须属于本地 `origin/main` 历史，且依赖、版本与该来源一致。
接受仓库截图基线仍须用户执行 `/accept-baseline`。迁移期间未建立可信缓存前，不应将运行器
夹具测试或编译成功当成截图回归通过，也不应提前接入 C++ 全量 CI 门禁。

运行 `npm run test:harness` 验证运行器逻辑；`npm run test:harness:browser` 使用真实 Chromium
和模拟 WASM/GPU 验证页面协议，均不使用真实基线。GPU 实际渲染仍需后端完整测试。
测试和产物回传共用 30 分钟期限；浏览器与 HTTP 清理各有独立的 10 秒兜底。

## Web 端排除项（第一阶段）

| 文件 | 原因 |
|---|---|
| `MultiThreadCase.cpp`、`PAGANRRegressionTest.cpp`、`PAGDiskCacheTest.cpp` | 依赖 pthreads（单线程构建）/ MEMFS 磁盘缓存语义 |
| `AsyncDecodeTest.cpp` | 断言依赖原生 500ms 预取窗口驱动的序列/图片缓存计数；web 构建在 `RenderCache::prepareLayers` 中全时间线预取，计数与原生语义永久不一致 |
| `PAGXCliTest.cpp` | CLI 源码不编入 web 测试 |
| `PAGXHTMLImporterTest.cpp` | 使用 `dynamic_cast`；tgfx 的 embind typeinfo 是 no-rtti 编译，无法开启 RTTI。由原生车道覆盖 |
| `*.mm`（Metal/Cocoa） | 平台测试在 Emscripten 下永远无法编译 |

`PAGTextLayerTest`/`PAGImageLayerTest` 中的 race 段通过 `__EMSCRIPTEN__` guard 跳过；
`PAGFrameCompareTest` 串行运行其语料。`PAGSequenceTest.Sequence_Multiple_References` 以相同
方式 guard：其缓存计数断言依赖原生 500ms 预取窗口（`DECODING_VISIBLE_DISTANCE`），而 web
构建在 `RenderCache::prepareLayers()` 中全时间线预取，计数与原生语义永久不一致（与
`AsyncDecodeTest.cpp` 被排除的原因相同）。

## 已知限制

- 整个 web 构建保持关闭 RTTI：tgfx 的 emscripten binding 依赖合成的 no-rtti typeinfo，
  在同一 embind 注册表中任何位置开启 RTTI 都会破坏所有 JS 到 C++ 的调用
  （BindingError: unknown type）。这就是基于 dynamic_cast 的 `PAGXHTMLImporterTest`
  无法在 web 上运行的原因。
- 视频序列解码由本目录下的 ffavc 软件解码器提供（`ffavc.min.js` + `ffavc.wasm`，
  `index.html` 中通过 `Module._registerSoftwareDecoderFactory` 注册，流程与官方 demo 一致）。
  需要升级解码器时从 `https://unpkg.com/ffavc@latest/lib/` 更新本地副本；缺少该文件时
  解码器工厂链失败，视频序列渲染为空。
- WebGL 后端运行在 Emscripten 的单一全局 GL 上下文上：不存在多上下文/多设备语义。
  `PAGSurfaceTest.FromTexture`（GL semaphore 语义）、`PAGSurfaceTest.GLRestorer`
  （跨上下文的 viewport/scissor/FBO 状态恢复）与
  `PAGXRuntimeTest.PAGSurfaceWrapRejectsMismatchedDevice`（`Devices::MakeDefault()` 返回
  同一单例，无法构造「设备不匹配」场景）断言的是桌面 GL 行为，已通过
  `PAG_WEBGL_ONLY` 宏（仅 webgl 构建定义，见 `web/test/CMakeLists.txt`）排除出
  webgl 车道。这些 GL 专用用例受 `TGFX_USE_OPENGL` 保护，不参与 WebGPU 构建，
  由 native GL 保留。旧运行器的通过数量不能证明进行了真实 MD5 比较；以严格协议报告中的
  `comparisons.compared`、`versionSkipped` 和 `missingBaseline` 为准。
- `resources/apitest/SVG` 永不打包，但 SVG 导入代码仍参与编译。
- WebGPU 构建开启 Asyncify，使 `Baseline::Compare` 使用的同步 `readPixels()`
  可以等待 buffer 映射完成。
- WebGPU 后端的 layer style 渲染曾存在不完整 BindGroup 缺陷。根因在 libpag 侧：
  `SolidStrokeFilter` 有单输入/双输入两个构造变体，但 `textureSamplers()`
  无条件声明 2 个 sampler，单输入变体（无 `originalImage`）draw 时
  `RuntimeFilter::onDraw` 只绑定 1 个纹理（2 个 entry），而 WebGPU 要求
  BindGroup 覆盖布局声明的全部 4 个 entry，浏览器报 Uncaptured Validation
  Error（Number of entries 2 vs 4）。GL 惰性绑定 + shader 用
  `isUseOriginalTexture` 分支守卫，不采样的槽位无副作用，故 webgl 车道不受
  影响。受影响用例：`PAGFilterTest.DropShadow/OuterGlow/Stroke`、
  `PAGTextLayerTest.Emoji_ID79762747`（同根，布局声明 2 sampler 实绑 1）。
  `PAGFilterTest.RadialBlur` 是另一类问题：uniform buffer 以 12 字节绑定，
  低于 WebGPU 要求的最小 16 字节（bound with size 12）。已在 tgfx WebGPU
  后端修复（tgfx 分支 `bugfix/billyjin_webgpu_bindgroup`，commit dd82d66 +
  f7dfe3a 补占位纹理 sampleCount/mipLevelCount/dimension 字段）：
  `updateBindGroup()` 对布局声明但未绑定的 texture/sampler 槽位填 1×1 占位
  纹理（占位永不被采样，语义等价 GL 惰性绑定）；uniform 绑定 size 向上取整
  到 16 的倍数并在 `WebGPUBuffer::Make` 对 UNIFORM 容量按 16 对齐。已验证：
  两类 validation error 清零，5 用例渲染恢复，重 seed 后全量真 md5 比对
  1617 例全绿（1615 过 + 2 skip，0 失败）。
- `PAGXSVGTest.SVGExport_WritingModeVerticalJustifyEmitsTextLength` 已排除出
  web（emscripten）构建：vertical justify 的 `textLength` 发射条件依赖 shaper
  列几何，浏览器字体度量与 native 不同导致断言不触发。native 构建保留该用例。
- web 车道的 md5 基线有整批性：`Baseline::saveData()` 在
  `PAGTest::HasFailure()` 为 true 时拒绝写任何 md5（保守设计，不固化失败
  态）。任何用例失败都会使整次 seed 的 md5 不落地，`update_baseline.sh` 会
  以 FAILED 退出。
- 生成缓存可使用 `--skip-images` 减少 MEMFS 占用；MD5 才是实际比较依据，
  `_base` 图仅供参考。回传采用分块读取，已移除整批 ZIP/base64 的额外内存副本。
- 当前 GitHub Actions 的 Web 运行测试为 Cypress，尚未接入本目录 C++ 全量测试。
  WebGPU 的同步截图使用 Asyncify，不能代替发布 SDK 的非 Asyncify 异步读回测试。
