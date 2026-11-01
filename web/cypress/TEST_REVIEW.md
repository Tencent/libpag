# Cypress 测试设计与覆盖评审

- 首次评审：2026-10-30，基准 `76f4dca18`
- 整改复核：2026-11-01，基准 `cac5f0dd5`，分支 `feature/billy_cypress_webgpu`
- 范围：`web/cypress`、直接相关的 SDK 实现、运行脚本及 CI 配置
- 方法：静态只读审查，未运行 Cypress，未生成或接受截图基线

本文初版记录了 2026-10-30 评审发现的问题；本版逐条标注整改结果。用例数量是源码中的测试声明数，不是执行数、通过数或代码覆盖率。

## 1. 总体结论

**初版评审指出的问题已全部闭环，当前 Cypress 双车道可以作为 PR 的质量门禁。** 用例有效性（跨测试共享、异步等待、弱断言）、入口可靠性（UnitTest、lane 选择、失败传播）、确定性像素验证和 WebGPU 生命周期覆盖均已落实；视觉基线死代码已彻底移除，由可计算期望值的像素断言替代。

遗留增强项（不影响本 PR，建议后续独立立项）：MT 与 Dedicated Worker 形态冒烟、初始化失败负向用例、更完整的真实视频场景、同浏览器双后端对照。

## 2. 当前覆盖情况

### 2.1 静态用例盘点

共 17 个 spec、183 处测试声明：普通 `it()` 163 处、`itGL()` 7 处、`itWebGPU()` 13 处。

| 分类 | 文件 | 声明数 |
|---|---|---:|
| SDK 初始化 | `sdk-init.spec.ts` | 1 |
| 数学、容器、队列 | `matrix.spec.ts`、`vector.spec.ts`、`queue.spec.ts` | 30 |
| 内容、图层、图片 | `pag-file.spec.ts`、`pag-composition.spec.ts`、`pag-layer.spec.ts`、`pag-image.spec.ts`、`pag-image-layer.spec.ts`、`pag-solid-layer.spec.ts`、`pag-text-layer.spec.ts` | 78 |
| 播放、渲染 | `pag-player.spec.ts`、`pag-view.spec.ts`、`pag-surface.spec.ts` | 55 |
| WebGPU 专项 | `pag-webgpu.spec.ts` | 13 |
| 字体栅格化 | `scaler-context.spec.ts` | 6 |

另有 Node 可控状态测试（`node --test`）：`UnitTest.test.cjs`（运行器入口）、`cypress-config.test.cjs`（配置门禁）、`video-prepare.test.cjs`、`video-reader.test.cjs`、`wechat-video-reader.test.cjs`（视频链路）、`bundle-format.test.cjs`（产物形态）。

### 2.2 CI 和本地入口

| 入口 | 范围 |
|---|---|
| CI `web-webgl` | 单线程 SDK 构建 → Node 套件（`test:cypress-runner` + `test:video`）→ Cypress 全量（Electron） |
| CI `web-webgpu` | 单线程 WebGPU SDK 构建 → Cypress 全量（Chrome + SwiftShader） |
| `npm run cypress` / `cypress:webgpu` | 本地对应车道 |
| `npm run UnitTest` | 本地包装入口，等待服务就绪并传播退出码 |
| `npm run test:bundle` | 随 `build:js:st` 在 CI 构建中执行 |
| MT | 有构建命令，Cypress 未覆盖（遗留） |

两个 Cypress job 均为 PR→main 的门禁步骤，失败上传 junit、summary.json 和截图 artifact（按后端分目录）。

## 3. 初版问题的整改情况

### 3.1 WebGPU 断言与实现脱节 — 已修复

- 无 ID canvas 测试：重写为离 DOM canvas 渲染、销毁后同 canvas 重建初始化，并做精确像素断言（`pag-webgpu.spec.ts`「Renders to a detached canvas without an id and reinitializes after destroy」）。
- Surface 销毁测试：改为对 `WebGPU.mgrTexture` 的 create/release 做 spy 配对验证——注册一次、销毁恰好释放一次、宿主纹理可重新包装并读出正确像素（同文件「Destroy releases the registration but preserves the host texture」）。

### 3.2 跨测试共享对象 — 已修复

Composition、File、Layer、Image、Player、SolidLayer、TextLayer、View、Surface、ImageLayer 全部改为：`beforeEach` 重建页面并独立准备 fixture，所有权统一登记，`afterEach` 逆序销毁（带 `isDestroyed` 守卫）。描述块注册期不再有 `destroy()` 调用，用例可独立运行。

### 3.3 异步操作未纳入完成条件 — 已修复

- Queue 三条测试全部 `await` 最终 Promise 并断言顺序与返回值。
- PAGView 播放链路用 stub 的 `performance.now`/`requestAnimationFrame` 驱动确定性帧推进，`play/pause/resume/stop` 全程验证进度、帧号、事件次序与调用次数。

### 3.4 弱断言 — 已修复

| 测试 | 整改 |
|---|---|
| TextLayer 描边色 | 设置后回读 `strokeColor()` 验证 SDK 状态 |
| TextLayer reset | 先修改字体/字号/颜色/文字，reset 后逐项验证恢复默认值 |
| View composition | 回读 `getComposition()` 校验 uniqueID 与时长，并 flush 确认输出变化 |
| View 播放与事件 | 完整状态序列 + spy 断言事件次序和次数 |
| View resize | 真实修改 canvas 尺寸，验证矩阵、flush 结果与快照尺寸 |
| View listener 移除 | 添加→触发→移除→再触发，核对计数 |
| Matrix pre/post | 改用非对称且含平移的基矩阵；复核时又修正了期望值与 `makeAll` 参数映射不一致的问题（按 `a=scaleX, b=skewY, c=skewX, d=scaleY`、pre=`M·X`、post=`X·M` 重算，已实测通过） |
| ScalerContext | bounds/fontMetrics/advance 接入文件顶部基线常量，generateImage 校验长度与非零 alpha |
| ImageLayer 替图 | 验证替换与 null 重置后的图层状态；并修复了由此暴露的 SDK 缺陷：`replaceImage(null)`/`setImage(null)` 在 web 端抛 TypeError（wrapper 未做空值安全，已改用 `getWasmIns()`） |

## 4. 视觉验证：死代码移除，由确定性像素断言替代

初版指出 `compareData()`/`saveData()` 无调用方、比较函数不查长度、残留 `cy.pause()`。整改选择删除而非修复该链路：`utils.ts`、config 的 readData/saveData task、Cypress 9 遗留的 `plugins/index.js`、`brotli` 依赖四层全部移除（npm 脚本中的 brotli CLI 使用系统二进制，不受影响）。

像素正确性改由可计算期望值的断言承担：

- WebGL：`pag-surface.spec.ts` 65×3 纹理/渲染目标读回，逐字节纯色断言、连续两帧读回互不污染、`clearAll` 后全零。
- WebGPU：`pag-webgpu.spec.ts` 非 256 对齐行宽的精确 RGBA（含半透明与裁剪区域）、ALPHA_8 读回、宿主 GPUTexture 采样（含 origin）、canvas2D 合成精确像素、多 View resize 隔离；`afterEach` 统一收集 validation/internal/out-of-memory error scope，意外 GPU 错误直接失败。

## 5. 增强覆盖的落实情况

### 已落地

- 确定性像素读回、连续帧读回、宿主纹理采样（见 §4）。
- 播放状态序列（play→pause→resume→stop 全链路事件与进度断言）。
- 多 View 与 resize 隔离（销毁其中一个不影响另一个）。
- Canvas 生命周期（无 ID、离 DOM、销毁重建）。
- 视频：非零时间帧准备不启动播放、准备失败时其他图层继续渲染且错误可读、恢复后错误清除（`pag-player.spec.ts`）；Node 侧视频准备/读取/微信小程序读取三套可控状态测试。
- 初始化：`sdk-init.spec.ts` 校验 WASM 请求路径与 HTTP 200，且不复制大响应体。
- 入口：`UnitTest.js` 重写（服务就绪等待、退出码传播、信号处理、`--backend` 校验）并有 22 例 Node 测试；已纳入 CI（`web-webgl` job，`cac5f0dd5`）。
- 诊断：`lane.ts` 拒绝未知后端；失败截图开启并按后端分目录；结果含 junit 与 summary.json。

### 未落地（建议后续独立 PR）

- MT 构建、Dedicated Worker、显式 OffscreenCanvas 的冒烟（三者是不同路径，不可互相替代）。
- 初始化失败负向：WASM 加载失败、显式 `locateFile`、无 adapter 或 device 申请失败后的恢复。
- 真实视频的 seek、暂停恢复、替换 composition 与销毁场景。
- 同一 Chrome 上的 WebGL/WebGPU 小型对照（隔离浏览器差异与后端差异）。
- `readPixelsAsync()` 的 Pending→Failed、超时、等待期销毁等状态机回归（适合 Node 可控测试）。

## 6. 测试职责边界（维持初版划分）

| 层次 | 主要职责 |
|---|---|
| Node | 可控状态机、Promise 结算、队列恢复、资源注册/回滚、运行器入口与配置门禁（已进 CI） |
| Cypress | 发布 SDK、真实 DOM、GPU 纹理、确定性像素结果、浏览器播放和视频、worker/MT 冒烟（待补） |
| C++ / C++ Web 测试 | 底层格式转换、rowBytes、origin、GPU 资源行为、大规模渲染基线 |

`web/test` 的 WebGPU 使用 Asyncify 支持同步式测试，不能替代发布 SDK 中 `readPixelsAsync()` 的事件循环、轮询和销毁竞争验证。

**最终判断：声明数量已对应有效的回归保障。本 PR 的 Cypress 整改完成；§5 未落地项按独立 PR 规划推进。**
