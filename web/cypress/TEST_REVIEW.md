# Cypress 测试设计与覆盖评审

- 评审日期：2026-10-30
- 评审基准：`76f4dca18`，分支 `feature/billy_cypress_webgpu`
- 范围：`web/cypress`、直接相关的 SDK 实现、运行脚本及 CI 配置
- 方法：静态只读审查，未运行 Cypress，未生成或接受截图基线

本文记录上述版本的评审结论，不代表后续版本的实时测试状态。用例数量是源码中的测试声明数，不是执行数、通过数或代码覆盖率。整改项在本次评审中均未实施。

## 1. 总体结论

**需要补充用例，但首先应修正现有用例的有效性。** 当前 Cypress 主要提供单线程 SDK 的 API 冒烟保障，不能视为真实播放、像素正确性、异步失败和资源生命周期已得到充分验证。

优先顺序：

1. 修复与当前实现脱节的 WebGPU 断言。
2. 消除跨测试共享对象，正确等待异步操作并清理资源。
3. 修复不能识别功能退化的断言和本地测试入口的失败传播。
4. 增加少量确定性像素、播放生命周期、多 View 和真实视频场景。
5. 根据交付支持范围补充 worker、MT 和浏览器矩阵。

不建议优先增加大量 getter/setter 用例，也不建议把所有底层渲染基线重复搬到 Cypress。

## 2. 当前覆盖情况

### 2.1 静态用例盘点

共 16 个 spec、177 处测试声明：普通 `it()` 158 处、`itGL()` 7 处、`itWebGPU()` 12 处。

| 分类 | 文件 | 声明数 |
|---|---|---:|
| SDK 初始化 | `sdk-init.spec.ts` | 1 |
| 数学、容器、队列 | `matrix.spec.ts`、`vector.spec.ts`、`queue.spec.ts` | 30 |
| 内容、图层、图片 | `pag-file.spec.ts`、`pag-composition.spec.ts`、`pag-layer.spec.ts`、`pag-image.spec.ts`、`pag-image-layer.spec.ts`、`pag-solid-layer.spec.ts`、`pag-text-layer.spec.ts` | 78 |
| 播放、渲染 | `pag-player.spec.ts`、`pag-view.spec.ts`、`pag-surface.spec.ts` | 50 |
| WebGPU 专项 | `pag-webgpu.spec.ts` | 12 |
| 字体栅格化 | `scaler-context.spec.ts` | 6 |

按当前注册规则推导：WebGL 注册 165 条启用测试、12 条跳过测试；WebGPU 注册 170 条启用测试、7 条跳过测试。这不是实际运行结果。Queue 测试直接导入源码，也不是后端集成测试。

### 2.2 CI 和本地入口

| 入口 | 构建与运行范围 |
|---|---|
| CI WebGL | 单线程 SDK，Electron |
| CI WebGPU | 单线程 SDK，Chrome；Linux 使用 SwiftShader |
| `npm run cypress` | 仅默认 WebGL lane |
| `npm run cypress:webgpu` | 仅 WebGPU lane |
| `npm run UnitTest` | 启动服务器后调用默认 Cypress；不构建，也不运行 WebGPU lane |
| MT | 有构建和开发命令，但上述 Cypress CI 未覆盖 |

当前不是“WebGL × WebGPU × 多浏览器 × 单双线程”的完整矩阵。后续可先增加同一 Chrome 上的小型双后端对照，避免将浏览器差异与后端差异混淆，无需立即扩满组合。

依据：[工作流](../../.github/workflows/build.yml)、[npm 命令](../package.json)、[Cypress 配置](../cypress.config.ts)、[lane 选择器](e2e/lane.ts)。

### 2.3 已有有效覆盖

以下能力应保留，不应将现有测试全部视为无效：

- `sdk-init.spec.ts` 已在请求前注册拦截，等待初始化，并检查 WASM 请求路径、HTTP 200 和基本 SDK 可用性；两后端使用对应页面。
- WebGPU 专项验证宿主 GPUDevice 初始化、纹理包装及部分不支持接口的返回约定。
- WebGPU 不可用时专用测试会失败，而不是静默跳过。
- Canvas2D 合成测试检查存在非透明像素，能够识别完全空白输出。
- 多数属性往返和对象结构断言能够捕获基础 API 接线错误。

## 3. 应先修复的现有测试

### 3.1 两处 WebGPU 断言与实现脱节

**优先级：高，建议当前 PR 合入前修复。**

#### 无 ID canvas 测试依赖旧 DOM 副作用

`PAGView assigns an id to a canvas without one` 要求 canvas 的 ID 以 `pag-webgpu-canvas-` 开头。

当前 `PAGView.makeWebGPUSurface()` 直接将 canvas 对象传给 `PAGSurface.fromCanvas()`，不再以自动分配 ID 作为创建 Surface 的前提。测试不应继续约束旧实现的副作用。

建议验证：无 ID canvas 能正确渲染，销毁后可重新初始化；补充离 DOM canvas 的代表场景。

#### Surface 销毁测试读取不存在的 wrapper 字段

`PAGSurface destroy releases the WebGPU texture handle` 强转并读取 `pagSurface.webGPUTextureHandle`，期待销毁后等于 0。

当前句柄存储在 `wasmIns` 的非枚举 Symbol 属性中，而不是这个 wrapper 字段。按当前源码构建，测试读取的是 `undefined`，不能用于证明资源已经释放。

建议分层验证：

- Node：注册与释放配对、失败回滚、重复销毁不重复释放。
- Cypress：销毁包装器后，宿主拥有的纹理仍可正确使用或重新包装。

不应为了满足旧测试重新暴露可写句柄，也不应依赖 GC 发生时机。

依据：[WebGPU 专项](e2e/pag-webgpu.spec.ts)、[PAGView](../src/pag-view.ts)、[纹理包装](../src/core/webgpu-texture.ts)、[PAGSurface](../src/pag-surface.ts)。

### 3.2 跨测试共享对象与页面生命周期不一致

**优先级：高。**

Composition、File、Layer、Image、Player、SolidLayer 六套 spec 存在类似模式：`beforeEach` 重访页面并初始化 SDK，但被测对象由前面的 `it` 创建，后续 `it` 继续使用或修改。

例如 Composition 的尺寸修改依赖前一条加载出的 composition，层索引操作又依赖更早创建的 layer。

影响：

- 单独运行一条测试不可靠。
- 调整顺序、失败重试可能改变结果。
- 可能继续使用旧页面、旧 WASM 模块创建的对象。
- 前面失败会导致后面级联失败，难以定位根因。

Composition 还将部分 `destroy()` 写在 `describe` 注册阶段；此时测试对象尚未创建，不是测试结束后的清理。现有 spec 缺少统一的 `afterEach` 资源释放。

建议每条测试独立准备 fixture，并在 `afterEach` 释放对象、播放器、纹理和快照。需要连续验证的一组动作放在同一个测试中，而不是依赖多个 `it` 的顺序。

依据：[Composition](e2e/pag-composition.spec.ts)、[Player](e2e/pag-player.spec.ts)、[Image](e2e/pag-image.spec.ts)。

### 3.3 异步操作未纳入测试完成条件

**优先级：高。**

- Queue 三条测试都有末尾 `.then()` 未返回或等待；其中包含延迟任务。
- View 的 `prepare()`、`play()`、`stop()` 测试没有等待相应 Promise。
- Player 的部分 `prepare()` 调用也未等待。

可能造成晚到断言、错误归属不清、任务跨测试残留，或操作尚未完成测试就结束。

建议等待最终 Promise，随后检查结果和状态。不要只补 `await` 而保留“调用不抛异常即成功”的弱判断。

注意：`cy.window().then(async (...) => ...)` 会等待返回的 Promise；不能仅因 hook 没有显式 `return cy...` 就认定 Cypress 命令没有被等待。

依据：[Queue](e2e/queue.spec.ts)、[View](e2e/pag-view.spec.ts)、[Player](e2e/pag-player.spec.ts)。

### 3.4 部分断言不能识别目标功能退化

| 测试 | 问题 | 建议 |
|---|---|---|
| TextLayer 描边色 | 设置后断言本地变量，而不是 SDK 状态 | 再调用 `strokeColor()` 检查 |
| TextLayer reset | 每条测试使用原始新对象，reset 前没有修改 | 先修改文字、颜色等，再验证恢复 |
| View composition | 加载新对象后检查旧对象，替换后无结果断言 | 回读 composition，并验证输出变化 |
| View 播放和事件 | 监听器只打印；pause 前没 play；stop 未验证首帧 | 验证完整状态序列、进度和事件次数 |
| View resize | 没改变尺寸，仅调用 `updateSize()` | 改变实际尺寸，验证 Surface 和输出尺寸 |
| View listener 移除 | 未先添加监听器 | 添加、触发、移除、再次触发并核对计数 |
| Matrix pre/post | 部分输入为单位矩阵，乘法顺序写反也可能通过 | 使用非单位、非对称且含平移的矩阵 |
| ScalerContext | bounds/metrics 无断言，部分结果只检查类型 | 使用固定字体和明确的几何结果断言 |
| ImageLayer 替图 | 主要证明输入图片构造成功 | 检查替换后的实际图层或渲染结果 |

依据：[TextLayer](e2e/pag-text-layer.spec.ts)、[View](e2e/pag-view.spec.ts)、[Matrix](e2e/matrix.spec.ts)、[ScalerContext](e2e/scaler-context.spec.ts)、[ImageLayer](e2e/pag-image-layer.spec.ts)。

负向用例中的宽泛 `.to.throw()` 也应尽可能匹配预期错误原因，避免无关异常满足断言。

## 4. 视觉验证尚未形成有效门禁

当前 16 个 spec 没有调用 `compareData()`、`saveData()`，也没有调用 `cy.screenshot()`。存在 `baselines` 目录和读写 task，不等于实际执行了视觉回归。

现有检查的边界：

- Surface 读回主要检查非空和字节长度；全零、通道错误、上下翻转、行错位仍可能通过。
- `makeSnapshot()` 主要检查 `ImageBitmap` 类型，不能证明内容和当前帧正确。
- Canvas2D 的非透明像素检查能发现全空白，但不能证明颜色、位置和画面内容正确。

不能直接启用现有比较器而不修复它：

- `utils.ts` 的比较函数不先检查长度，空实际数组或等于基线前缀的短数组可能返回成功。
- `compareData()` 保留了 `cy.pause()`，没有返回可组合的命令链。
- 保存 task 使用同一基线目录，没有后端隔离或明确的比较/更新模式。

建议优先增加可计算期望值的简单像素测试；需要视觉基线时应遵守比较只读、更新显式、后端隔离、缺失即失败的原则。不要在缺基线时自动接受当前输出，也不要用 WebGPU 输出覆盖 WebGL 基线。

依据：[Surface](e2e/pag-surface.spec.ts)、[WebGPU 专项](e2e/pag-webgpu.spec.ts)、[比较工具](utils.ts)、[实际 task 配置](../cypress.config.ts)。

## 5. 建议新增的高价值用例

### 5.1 第一优先级：真实浏览器集成

| 场景 | 关键断言 |
|---|---|
| 确定性像素读回 | 使用非正方形、行字节数非 256 对齐的图案，例如 `65×3`；上下左右不同颜色；检查 RGBA、透明度、方向及行对齐 |
| 连续帧读回 | 渲染 A 再渲染 B，返回正确的新帧；旧返回数组在后续读回后内容不变 |
| 宿主纹理采样 | 上传已知图案，经 `PAGImage.fromTexture()` 渲染并核对像素，不仅验证包装器创建 |
| 播放状态序列 | `play → pause → resume → end`；暂停后不推进，stop 回首帧；检查事件顺序和次数 |
| 多 View 与 resize | 两个不同尺寸 View，修改或销毁其中一个，不影响另一个；验证透明区域清除 |
| Canvas 生命周期 | 无 ID、离 DOM canvas；同 canvas 销毁后重建；初始化失败后恢复 |
| 真实视频 | 明确含视频的 fixture，两时间点输出有效且不同；seek、暂停恢复、替换 composition 和销毁 |
| 初始化失败 | WASM 加载失败、显式 `locateFile`、无 adapter 或申请 device 失败；失败不悬挂，后续初始化可恢复 |

像素验证优先使用整数坐标和简单颜色，避免抗锯齿边缘；必要时明确容差。视频应检查有效内容、变化和时间窗口，不宜做跨浏览器全帧逐字节基线。

GPU 集成测试应收集意外 validation error 和设备丢失。预期负向错误在用例内局部收集，不应全局屏蔽。

### 5.2 第二优先级：Node 可控状态测试

以下场景应使用可控 session、时钟和资源代理，避免依靠真实 GPU 随机制造竞争或等待默认超时：

- `readPixelsAsync()` 的 Pending → Ready、Pending → Failed、超时和恢复。
- 等待期间销毁，尤其销毁与 Ready 在同一轮发生。
- 同一 Surface 并发读回及完成后再次调用。
- 无论成功、失败、超时还是销毁，session 都恰好释放一次。
- 纹理包装返回空或抛异常时，注册句柄回滚。
- 重复销毁不重复释放；宿主纹理所有权不被错误接管。
- Queue 同步抛错或异步 reject 后继续执行下一任务。
- 视频监听器、Object URL 和定时器的释放。

这些建议有实际代码风险作为依据，而不是机械追求分支覆盖：`readPixelsAsync()` 在轮询等待后进入 Ready 分支时没有再次检查销毁状态；纹理包装处理了 factory 返回空，但调用过程中抛异常的回滚尚需验证。此类竞争和泄漏风险应先写确定性回归测试，不将静态风险描述成已发生的线上故障。

依据：[异步读回](../src/pag-surface.ts)、[纹理包装](../src/core/webgpu-texture.ts)、[队列](../src/utils/queue.ts)、[视频读取](../src/core/video-reader.ts)。

### 5.3 第三优先级：交付形态和矩阵

根据支持承诺增加少量冒烟：

- 显式传入 OffscreenCanvas。
- Dedicated Worker 内运行 SDK 并渲染。
- MT 的隔离环境、worker 资源 URL、初始化、一次渲染及销毁。
- 同一 Chrome 上的小型 WebGL/WebGPU 对照。

Dedicated Worker 与 WASM pthread worker 是不同路径，不能相互代替。无视频 OffscreenCanvas 成功也不能推出 worker 视频支持。MT 测试需要正确的 COOP/COEP 环境，不能仅增加一个后端参数。

## 6. 测试入口与诊断问题

### 6.1 本地 UnitTest 失败传播

`web/script/UnitTest.js` 启动服务器后立即启动 Cypress，没有等待服务就绪。子进程退出时只打印退出码并关闭服务器，没有设置父进程退出码。

建议等待服务就绪，并可靠传播测试失败、启动失败和异常退出。该问题影响本地或使用此包装器的自动化；当前 CI 直接调用 `npm run cypress*`，不走这个入口。

依据：[本地入口](../script/UnitTest.js)。

### 6.2 后端参数与失败诊断

- `lane.ts` 只有 `BACKEND === 'webgpu'` 才选择 GPU，其他值包括拼写错误都回到 GL。应拒绝未知显式值。
- Cypress 配置关闭了录像和失败截图。CI 虽会上传文件，仍应让像素失败产出实际/期望小图、后端、浏览器和 GPU 错误信息。
- SDK 可能将播放异常转为日志；“没有未捕获 JS 异常”不能作为完整成功条件。
- 重试不能替代用例独立性，也不能掩盖异步任务跨测试残留。

## 7. 测试职责边界

| 层次 | 主要职责 |
|---|---|
| Node | 可控状态机、Promise 结算、队列恢复、资源注册/回滚、构建和退出码契约 |
| Cypress | 发布 SDK、真实 DOM、GPU 纹理、少量像素结果、浏览器播放和视频、worker/MT 冒烟 |
| C++ / C++ Web 测试 | 底层格式转换、rowBytes、origin、GPU 资源行为、大规模渲染基线 |

`web/test` 的 WebGPU 使用 Asyncify 支持同步式测试，不能替代发布 SDK 中 `readPixelsAsync()` 的事件循环、轮询和销毁竞争验证。

已存在的 Node 打包测试和 C++ Surface 测试应扩展或复用，不应为 Cypress 再复制全部底层测试。

## 8. 建议 PR 拆分与验收

### 当前 PR 合入前

- [ ] 修复两处过时 WebGPU 断言，不恢复旧实现副作用。
- [ ] 使用当前源码构建，确认现有 WebGL/WebGPU Cypress CI 结果。

### 独立测试质量 PR

- [ ] 消除六套 spec 的跨用例依赖。
- [ ] 补齐 Promise 等待与 `afterEach` 清理。
- [ ] 修复本地变量断言、无效 reset、播放/事件/resize 等弱断言。
- [ ] 修正 `UnitTest` 的服务就绪检查和退出码传播。
- [ ] 未知显式后端参数失败，不静默回落。
- [ ] 验证代表用例独立运行、整套运行和重试不依赖前序状态。

### 增强覆盖 PR

- [ ] 确定性像素、连续帧和真实纹理采样。
- [ ] 播放生命周期、多 View resize 和真实视频。
- [ ] Node 异步读回状态机、异常恢复及资源回滚。
- [ ] 按支持范围补 worker/MT 冒烟与同浏览器双后端对照。
- [ ] 失败能够输出足够的像素与运行环境诊断信息。

C++ Web 全量 CI 接入继续单独推进，不与上述全部整改捆绑。真实截图缓存建立、atlas 顺序对照与 C++ 全量回归仍按该任务自身的验收要求执行。

**最终判断：当前声明数量不等于可靠回归保障。完善方向应是让用例真正识别错误画面、未结算的异步操作、未释放的资源和未恢复的状态。**
