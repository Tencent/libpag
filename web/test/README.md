# libpag Web 测试车道

在浏览器中运行原生 `test/src` 的 Google Test 测试套件，渲染后端为 **WebGL** 或 **WebGPU**，
沿用 tgfx `web/test` 的模式：测试二进制经 Emscripten 编译为 WebAssembly，由 Puppeteer 驱动；
截图输出与基线 MD5 缓存通过 CDP 下载从虚拟文件系统导出。

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

1. `emcmake cmake` 构建 `pag-test`（Emscripten 可执行程序，入口为
   `src/WebTestRunner.cpp`，导出 `RunAllTests`/`RunTest`）。
2. `run_test.js` 启动 http-server，通过 Puppeteer 拉起 headless Chrome
   （无 GPU 的 Linux 上附加 `--use-webgpu-adapter=swiftshader`），打开 `index.html`。
3. `index.html` 实例化模块（WebGL：`#pag-test-canvas` 上的默认 GL 上下文；
   WebGPU：`preinitializedWebGPUDevice`），调用 `ccall('RunAllTests')`，随后把
   `baseline-out/`、`test/out/` 与 md5 缓存打包成 zip，由测试框架解回仓库。
4. 基线比较遵循原生语义：将仓库的 `test/baseline/version.json` 与各后端快照
   `test/baseline/.cache/{webgl,webgpu}/version.json` 比对；版本一致时用
   `.cache/{backend}/md5.json` 做 MD5 比较，版本不一致时跳过比较。

未生成基线缓存时，所有截图比较会静默通过（配置阶段会输出 CMake WARNING）。
请务必先运行 `update_baseline.sh`。

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
  webgl 车道。`FromTexture`/`GLRestorer` 在 webgpu 车道与 native 构建保留（它们在那里
  通过，基线受保护）；`PAGSurfaceWrapRejectsMismatchedDevice` 因 `TGFX_USE_OPENGL`
  未定义本就不参与 webgpu 构建，由 native GL 构建保留。
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
- seed 应使用 `--skip-images` 模式：带图模式（GENERATE_BASELINE_IMAGES）下
  `saveData` 把页内 `_base` 图集 rename 进 `/test/out`，其中 `PAGCompareFrameTest`
  逐帧比对会产出 8000+ 张 `_base` webp，页面 JSZip 打包时内存不足导致页面崩溃
  （detached Frame），导出通道全断。`--skip-images` 下 `/test/out` 只有数百个
  运行时文件，打包可承受；宿主 `_base` 图不更新不影响判定（web 车道的真基线
  是 `.cache/{backend}/md5.json`，`_base` 图仅供可视化参考）。
- 车道产物传输不依赖 headless Chrome 的 CDP 下载（多次实测不可靠，下载可能
  不落盘）：页面把 zip 的 base64 挂到 `window.__testZips`，`run_test.js` 用
  `page.evaluate` 取回写盘；CDP 下载作为兼容通道并存。`cache-web.zip` 从页内
  `/test/baseline/.cache` 整树打包（entry 带后端前缀），宿主提取到
  `test/baseline/.cache` 本身而非 `.cache/{backend}`，避免双层嵌套。
