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
  同一单例，无法构造「设备不匹配」场景）断言的是桌面 GL 行为，在本车道上预期失败。
- `resources/apitest/SVG` 永不打包，但 SVG 导入代码仍参与编译。
- WebGPU 构建开启 Asyncify，使 `Baseline::Compare` 使用的同步 `readPixels()`
  可以等待 buffer 映射完成。
