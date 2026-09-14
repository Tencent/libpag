# PAGImageView 主线程解码 ANR 排查与修复方案

## 版本信息

- 日期：2026-09-11
- 分支：`release/4.5`
- Crash ID：`dfcedaf3-b066-4477-b2ad-2c7926a4c97f`（Bugly 附件）
- 影响方：腾讯地图 `com.tencent.map` v11.6.0.5994（自带 PAG 引擎 tag `Engine-v4.37.8`）
- 状态：已完成代码与方案审查，待确认后编码

---

## 一、审查结论

### 1.1 总体结论

ANR 主链路成立：**初次布局时，`PAGImageView` 在主线程同步执行 `PAGDecoder.readFrame()`；视频硬解在系统高负载下持续无输出，主线程在输入超时窗口内长时间无法处理事件，最终触发 5 秒输入分发超时。**

初版方案中的方向并非全部合理，需要调整：

| 项目 | 审查结论 |
|---|---|
| `VideoReader` 增加解码预算 | 方向合理，但只能提供协作式软预算，不能承诺严格的 2 秒上界 |
| 通过 `Performance` 上抛失败 | 不合理；性能统计与当前帧正确性没有可靠的一一对应关系 |
| 把 `PAGImageView.flush()` 整体改为异步 | 不合理；破坏公开 API 的同步语义，并引入生命周期、丢唤醒和缓冲区竞态 |
| 优先修复 `PAGAnimator.update()` 的异步语义 | 更符合现有 API 约定，且能直接覆盖本次 `onSizeChanged` 触发路径 |
| 解码失败与帧请求绑定后上抛 | 合理；可以避免失败帧进入内存状态和磁盘缓存 |

### 1.2 证据等级

#### 已确认

1. 主线程栈明确停在 `PAGDecoder.readFrame()`，上层为 `PAGImageView.onSizeChanged()` 触发的 `PAGAnimator.update()`。
2. `PAGAnimator::doUpdate()` 在 `_isSync == true` 或 `_isRunning == false` 时同步调用 `onFlush()`。
3. `PAGImageView.onAnimationUpdate()` 同步调用 `flush()`，继而进入 `handleFrame()` 和 native `readFrame()`。
4. `AsyncDataSource::getData()` 使用无超时的 `task->wait()`；若任务仍在排队，等待线程甚至会直接执行该任务。
5. 本次日志中，硬解在 tid 3779 上启动，约 4.7 秒后触发连续 `TryAgainLater` 次数上限。
6. `VideoReader` 首次失败后还有同 decoder 重试和下一 decoder factory fallback，可能继续放大阻塞。

#### 高概率但无法由附件单独证明

1. 系统 CPU 和内存压力导致 MediaCodec 服务长时间无法输出视频帧，是本次硬解停滞的主要诱因。
2. 主线程在该实例中等待 tid 3779 上的 tgfx 解码任务。日志线程与 Java 主线程栈吻合，但附件没有 native 全线程 trace。

#### 已删除或降级的推断

1. 不能根据 4.7 秒总耗时推导“每次 Binder 调用约 47ms”；该区间还包括建 codec、排队、输入输出 dequeue 和其他渲染工作。
2. 尺寸变化不会必然让“两层缓存全部失效”：磁盘缓存 key 对特定 PAGFile 包含尺寸，但 Java `bitmapCache` 在 `onSizeChanged()` 中没有被清空。
3. 后续重试不是固定“三倍耗时”；是否进入、耗时多少取决于 decoder 状态和可用 factory。
4. 解码时间预算无法覆盖 mutex 等待、任务排队或单次卡住的系统调用，因此不能承诺严格的最大耗时。

---

## 二、问题分析

### 2.1 现象

设备为 WIKO GAR-AN60（Android 11，EMUI 14，8GB 内存），场景为车机导航页 `MapStateCarNav`。ANR 原因：

```text
ANR Input dispatching timed out (a3e98a9 com.tencent.map/com.tencent.map.AppIcon (server)
is not responding. Waited 5000ms for MotionEvent)
```

主线程栈：

```text
org.libpag.PAGDecoder.readFrame(Native Method)
org.libpag.c$a.a(SourceFile:3)
org.libpag.PAGImageView.a(SourceFile:86)
org.libpag.PAGImageView.flush(SourceFile:12)
org.libpag.PAGImageView.onAnimationUpdate(SourceFile:8)
org.libpag.PAGAnimator.onAnimationUpdate(SourceFile:3)
org.libpag.PAGAnimator.update(Native Method)
org.libpag.PAGImageView.c(SourceFile:10)
org.libpag.PAGImageView.onSizeChanged(SourceFile:10)
android.view.View.sizeChange(View.java:23667)
...
android.view.ViewRootImpl.performTraversals(ViewRootImpl.java:3697)
```

关键日志：

```text
21:39:37.742  555 3779 E tgfx: HardwareDecoder: START Hardware Decoder.
21:39:41.883  555  684 E eup : #++++++++++Record By Bugly++++++++++#
21:39:42.449  555 3779 E tgfx: VideoDecoder: try decoding frame count reach limit 100.
```

### 2.2 准确触发条件

`PAGImageView.onSizeChanged()` 总会执行 `decoderInfo.reset()`、更新尺寸并调用 `checkVisible()`，但 `checkVisible()` 仅在可见状态发生变化时调用 `animator.update()`。

因此本次更准确的触发条件是：

> `PAGImageView` 初次布局、attach 后首次具备有效尺寸，或尺寸变化同时使可见状态从 false 变为 true。

普通的“已经可见时再次 resize”不会仅因 `checkVisible()` 直接调用 `animator.update()`，但仍可能通过其他刷新路径触发解码。

### 2.3 完整调用链

1. 初次布局进入 `PAGImageView.onSizeChanged()`。
2. `checkVisible()` 判断 View 首次可见，设置 animator duration 后调用 `animator.update()`。
3. animator 尚未播放，`PAGAnimator::doUpdate()` 命中 `!isRunning`，同步调用 `onFlush()`。
4. JNI 在当前主线程回调 `PAGImageView.onAnimationUpdate()`。
5. `onAnimationUpdate()` 同步调用 `flush()` → `handleFrame()` → `DecoderInfo.readFrame()`。
6. `DecoderInfo.readFrame()` 持 Java `ReentrantLock` 调用 native；`PAGDecoder::readFrame()` 同时持 native `PAGDecoder::locker`，直到读取缓存、渲染和写缓存结束。
7. 视频帧由 `SequenceFrameGenerator` 包装为 tgfx `AsyncDataSource`。GPU 上传资源时调用 `getData()`，内部无超时 `task->wait()`。
8. 本次任务已在 tid 3779 执行，主线程等待；如果任务仍处于 Queueing，`Task::wait()` 也可能直接在主线程执行解码。
9. 硬解持续返回 `TryAgainLater`，约 4.7 秒后才到达次数上限；这段等待与同一输入超时窗口内的其他主线程工作共同耗尽 5 秒响应期限，触发 ANR。

### 2.4 缓存行为

- `PAGDecoder` 磁盘缓存 key 对未修改的 PAGFile 包含目标宽高，尺寸变化可能切换到新的缓存文件。
- 非 PAGFile 或 `ContentVersion > 0` 时 key 为空，不能简单归因于尺寸 key。
- `PAGImageView.onSizeChanged()` 没有清空 `bitmapCache`；开启 `cacheAllFramesInMemory` 时还存在复用旧尺寸 Bitmap 的风险，应在实现阶段一并修正。
- 当前 `DecoderInfo.reset()` 使用 `tryLock()`：空闲时立即释放 decoder，繁忙时只做逻辑失效，native decoder 延迟回收。因此不能描述为“尺寸变化立即丢弃 decoder”。

### 2.5 系统负载

ANR 时间窗口内：

- `com.tencent.map` 约 98% CPU；
- `system_server` 约 97% CPU；
- 多个微信小程序进程、dex2oat 同时占用 CPU；
- PSI memory some avg10 为 27.65%，kswapd0 占用约 22%；
- 全系统 CPU 接近 100%。

这些数据支持“系统资源争用加剧 MediaCodec 停滞”，但没有逐次 codec 调用耗时或 binder trace，不能进一步量化单次调用延迟。

### 2.6 main 分支现状

main 已有以下相关修复，`release/4.5` 也已包含：

| 提交 | 修复内容 | 对本次问题的覆盖 |
|---|---|---|
| `c0535940b` #3377 | 磁盘缓存 IO 移到后台线程 | 排除一类主线程 IO，但不解决解码等待 |
| `322c33e88` #3554 | 等待 pending animator task 增加 500ms 超时 | 不覆盖本次同步 `onFlush()` 路径 |
| `3b50fff0f` #3570 | `DecoderInfo.reset()` 使用 tryLock | 不覆盖正在执行的 `readFrame()` |
| `decca43cc` #3647 | composition 常驻，避免重建时重新加载文件 | 降低加载阻塞，不解决视频硬解停滞 |

另外，#3554 仍有一个边界：`Task::wait(timeout)` 在任务尚处于 Queueing 时会直接在调用线程执行任务，timeout 不生效。因此它不是严格的主线程保护。

---

## 三、初版方案审查

### 3.1 初版 Fix 1：VideoReader 时间预算

方向合理，但初版设计有四个问题：

1. `TryAgainLater` 次数耗尽被归为 `Failed`，会继续同步重试和 fallback，违背“停滞时快速返回”的目标。连续 `TryAgainLater` 应统一归为 `Stalled`。
2. 当前代码在切换目标帧后立即清空 `lastBuffer`，所以超时后 `return lastBuffer` 实际通常返回 null。
3. 即使保留上一帧，也不能把旧 buffer 当作目标帧返回，否则 `SequenceImageQueue` 会把旧画面记录为新 `currentFrame`，暂停后可能永久不再重试该目标帧。
4. 时间检查无法中断 mutex、任务排队、单次 MediaCodec 调用或 codec stop/delete，因此只能称为软预算。

结论：**保留方向，但必须修改状态分类、返回值和表述。**

### 3.2 初版 Fix 2：通过 Performance 上抛失败

不可采用，原因如下：

1. `RenderCache::detachFromContext()` 当前先 `prepareNextFrame()`，再 `recordPerformance()`；下一帧预解码结果可能污染当前帧。
2. `recordPerformance()` 遍历全部 `sequenceCaches`，而不是仅处理当前 flush 实际使用的 queue。
3. `SequenceReader::reportPerformance()` 只有 `decodingTime > 0` 才调用上报，正确性信号不应依赖性能计时。
4. 解码线程写失败标志、渲染线程读失败标志，普通 bool 存在数据竞争。
5. `PAGSurface::draw()` 在返回前已经 flush、submit、present；事后修改 `PAGPlayer::flush()` 返回值不能保证窗口保留上一帧。

结论：**删除该方案。解码正确性必须与具体 frame/request generation 绑定，不能复用 Performance。**

### 3.3 初版 Fix 3：整体异步化 PAGImageView.flush()

不可直接采用，原因如下：

1. `flush()` 的公开契约是“立即渲染并返回内容是否变化”，改成“是否调度成功”属于 API 语义变更。
2. `AtomicBoolean running + volatile boolean queued` 存在明确的丢唤醒竞态。
3. `NativeTask.Run()` 最终使用 `tgfx::Task::Run()`；入队失败时会在调用线程执行，无法保证不在主线程解码。
4. `releaseBitmap()` 若等待 `decodeLock`，主线程仍可能被在途解码阻塞；软预算也不能保证严格小于 2 秒。
5. 缺少 composition、尺寸、attach 状态的 generation 校验，旧任务可能在 resize、detach 或换资源后发布结果。
6. listener 会在真实帧完成前收到 `onAnimationUpdate()`，回调中读取 `currentImage()` 得到旧帧。
7. 双缓冲在失败时仍翻转 `useFirst`，异步化会增加写入当前展示 Bitmap 的概率。

结论：**不改变现有同步 `flush()`；优先修正自动更新的调度语义。若新增异步公开接口，应单独设计 `flushAsync()`。**

---

## 四、修订后的方案

修订方案分为三个必要改动和一个可选增强：

| 优先级 | 改动 | 目标 |
|---|---|---|
| P0 | 修正 `PAGAnimator.update()` 的异步语义 | 直接移除本次初始 layout 的主线程同步解码 |
| P0 | `VideoReader` 增加停滞软预算 | 系统过载时尽快结束本次解码，不继续同步 fallback |
| P0 | 按 frame/request 传播失败 | 失败帧不进入成功缓存，不写入 PAGDecoder 磁盘缓存，并允许后续重试 |
| P1 | 新增明确的 `PAGImageView.flushAsync()` | 给业务提供不阻塞调用线程的主动刷新接口，保留 `flush()` 原契约 |

### 4.1 P0-A：修正 PAGAnimator.update() 的异步语义

Java API 已声明：`isSync=false` 时，手动 `update()` 不阻塞调用线程。当前 C++ 因 `!isRunning` 强制同步，和接口契约不一致。

修改目标：

- 手动调用 `PAGAnimator::update()` 时，只由 `_isSync` 决定同步或异步，不再因为暂停状态强制同步。
- 播放推进 `advance()` 的最终帧暂时维持现有顺序，避免打乱 `onAnimationUpdate` → `onAnimationEnd` 以及回调线程约定。
- 异步提交必须保证不会在调用线程内联执行。
- 手动 update 到达时若已有 task 正在执行，记录一个 pending generation；当前 task 完成后再处理最新请求，不能沿用现有“直接丢弃 update”的行为，否则 resize/setPath 使旧 decoder 失效后可能没有新帧补上。

建议内部接口：

```cpp
enum class UpdateSource {
  Manual,
  Playback,
};

void PAGAnimator::update() {
  doUpdate(false, UpdateSource::Manual);
}

void PAGAnimator::doUpdate(bool setStartTime, UpdateSource source) {
  auto shouldRunAsync = !isSync && (isRunning || source == UpdateSource::Manual);
  if (shouldRunAsync) {
    submitFlushTask(setStartTime);
  } else {
    onFlush(setStartTime);
  }
}
```

必须同时解决任务内联问题，二选一：

1. tgfx 新增“只尝试入队、绝不调用者内联”的提交接口，提交失败时丢弃该次自动刷新并等待后续 update；或
2. 为 animator 使用保证异步的专用执行器。

不能继续直接依赖当前 `Task::Run()`，因为 `TaskGroup::pushTask()` 失败时会调用 `task->execute()`。提交成功后，task 完成路径需要在锁内检查 pending generation，并在锁外继续提交最新一次 update，形成无丢唤醒的串行 drain loop。

#3554 的等待也应调整：带 timeout 的 wait 不应执行 Queueing 任务。建议新增 `waitOnly(timeout)`，或将 `wait(timeout > 0)` 改为只等待。这样主线程取消/切换 sync 状态时不会把排队中的重任务拉回主线程执行。

本改动直接覆盖本次：

```text
onSizeChanged → checkVisible → animator.update()
```

`update()` 返回后，真实 `onAnimationUpdate → PAGImageView.flush → readFrame` 在后台执行。

已知边界：播放结束的最后一帧目前仍为同步更新，以保持 update/end 顺序。如要彻底消除该路径，需要增加“异步完成后再在 UI 线程发送 onAnimationEnd”的跨平台事件调度，单独设计，不与本次补丁混合。

### 4.2 P0-B：VideoReader 停滞软预算

状态分类：

```cpp
enum class DecodeStatus {
  Success,
  Error,
  Stalled,
};
```

规则：

- `DecodingResult::Error`、发送输入失败等确定错误 → `Error`，允许现有 decoder reset/fallback。
- 连续 `TryAgainLater` 次数或无进展时间超限 → `Stalled`，立即结束本次请求，不执行同 decoder 重试和 software fallback。
- 总时间预算在首次尝试、重试和 fallback 之间共享。
- 超时返回 null，不返回上一帧冒充目标帧。

伪代码：

```cpp
std::shared_ptr<tgfx::ImageBuffer> VideoReader::onMakeBuffer(Frame targetFrame) {
  auto startTime = tgfx::Clock::Now();
  std::lock_guard<std::mutex> autoLock(locker);
  auto deadline = startTime + MAX_TOTAL_DECODE_TIME_US;
  auto status = decodeFrame(sampleTime, deadline);
  if (status == DecodeStatus::Error && tgfx::Clock::Now() < deadline) {
    resetParams();
    status = decodeFrame(sampleTime, deadline);
  }
  if (status == DecodeStatus::Error && tgfx::Clock::Now() < deadline) {
    destroyVideoDecoder();
    factoryIndex++;
    if (checkVideoDecoder()) {
      status = decodeFrame(sampleTime, deadline);
    }
  }
  if (status != DecodeStatus::Success) {
    return nullptr;
  }
  return videoDecoder->onRenderFrame();
}

DecodeStatus VideoReader::decodeFrame(int64_t sampleTime, int64_t deadline) {
  auto lastProgressTime = tgfx::Clock::Now();
  int tryDecodeCount = 0;
  while (currentDecodedTime < sampleTime) {
    if (tgfx::Clock::Now() >= deadline) {
      return DecodeStatus::Stalled;
    }
    if (!sendSampleData()) {
      return DecodeStatus::Error;
    }
    auto result = videoDecoder->onDecodeFrame();
    if (result == DecodingResult::Error) {
      return DecodeStatus::Error;
    }
    if (result == DecodingResult::Success) {
      tryDecodeCount = 0;
      lastProgressTime = tgfx::Clock::Now();
      currentDecodedTime = videoDecoder->presentationTime();
      continue;
    }
    if (result == DecodingResult::EndOfStream) {
      outputEndOfStream = true;
      return DecodeStatus::Success;
    }
    tryDecodeCount++;
    auto now = tgfx::Clock::Now();
    if (tryDecodeCount >= MAX_TRY_DECODE_COUNT ||
        now - lastProgressTime >= MAX_STALL_TIME_US || now >= deadline) {
      return DecodeStatus::Stalled;
    }
  }
  return DecodeStatus::Success;
}
```

建议初始阈值：

- 连续无进展：500ms；
- 单次 `readBuffer()` 总预算：2000ms。

阈值必须通过低端 Android 设备数据校准。文档只承诺：

> 对能够周期性返回控制权的 decoder，预算可显著缩短停滞等待；它不能中断 mutex、任务排队或一次卡住的系统 API，因此不是硬实时上限。

### 4.3 P0-C：按 frame/request 传播解码结果

目标：

1. 视频解码返回 null 时，目标帧不能被 `SequenceImageQueue` 记录为成功。
2. 后续再次请求同一目标帧时必须能够重试。
3. `PAGDecoder` 不得把缺失视频层的画面写入 SequenceFile。
4. 下一帧预解码失败不能污染当前帧。

不使用 `Performance`。新增内部请求结果：

```cpp
enum class SequenceReadStatus {
  Pending,
  Success,
  Failed,
};

struct SequenceReadResult {
  uint64_t requestID = 0;
  Frame targetFrame = -1;
  SequenceReadStatus status = SequenceReadStatus::Pending;
};
```

建议数据流：

1. `SequenceImageQueue` 每次创建 `SequenceFrameGenerator` 时生成递增 `requestID`，并将 `requestID + targetFrame` 传给 reader。
2. `SequenceReader::readBuffer()` 在任务完成时，以线程安全方式记录对应请求的 Success/Failed。
3. 当前 draw 的资源任务执行完成后，`RenderCache` 只检查 `usedSequences` 中本帧实际使用的 queue 和 requestID。
4. 检查顺序放在 `prepareNextFrame()` 之前，避免预取结果串入当前帧。
5. 失败时清除该 queue 对应的 `currentImage/currentFrame` 或 `preparedImage/preparedFrame`，使下次请求能够重新解码。
6. `RenderCache` 保存 `lastFrameHasSequenceDecodeFailure`，但不放入 Performance。
7. `CompositionReader::renderFrame()` 在 `pagPlayer->flush()` 后读取该状态；若失败则返回 false。`PAGDecoder::readFrameInternal()` 已会因此跳过 `sequenceFile->writeFrame()`。

这里不修改普通 `PAGPlayer` 的窗口播放语义：窗口可能已经完成本帧提交；本状态首先用于保证 `PAGDecoder` 候选 Bitmap 不发布、不落盘。不能宣称所有渲染目标都会自动保留上一帧。

### 4.4 P1：新增 PAGImageView.flushAsync()

保留现有：

```java
public boolean flush(); // Synchronous, unchanged.
```

新增异步接口时应明确回调语义：

```java
public void flushAsync(FlushListener listener);
```

完整实现必须满足：

- 使用保证不在调用线程内联的执行器，不能直接复用当前 `NativeTask.Run()`；
- 使用单调 generation 或互斥保护的 drain loop 实现 latest-wins，不能使用 `AtomicBoolean + volatile boolean`；
- 所有异步刷新进入同一串行执行器；
- 任务捕获 composition、尺寸、frame、attach generation 快照；完成后只发布仍匹配的结果；
- resize、detach、换资源只增加 generation，不在主线程等待 decode lock；
- worker 写入未发布的独立 back buffer，成功后在 UI 线程交换，不能写当前展示的 front buffer；
- listener 在结果提交后触发，异常路径使用 finally 恢复运行状态；
- `cacheAllFramesInMemory` 在尺寸变化时清理旧尺寸缓存。

该接口是增强项，不是本次 ANR 修复的前置条件。

---

## 五、实施顺序

1. **先落地 P0-A**：修正手动 `PAGAnimator.update()` 在 `isSync=false` 时的异步语义，并确保任务不会在调用线程内联。本次初始布局 ANR 的调用链在这里被切断。
2. **再落地 P0-B**：为硬解停滞增加软预算，避免后台任务长时间占用 decoder 和线程池。
3. **同步落地 P0-C**：保证 stalled/failed frame 可重试且不会污染磁盘缓存。P0-B 不应脱离 P0-C 单独上线。
4. **按需求提供 P1**：新增 `flushAsync()`，不改变现有 `flush()` 契约。

---

## 六、测试计划

### 6.1 PAGAnimator

1. paused + `isSync=false` + `update()`：调用线程立即返回，更新发生在 worker。
2. paused + `isSync=true` + `update()`：保持同步。
3. task 执行期间连续调用 update/resize/setPath：合并为最新请求，完成后必定补一次刷新，不丢唤醒。
4. 异步任务仍 Queueing 时调用 `cancel()` / `setSync()`：主线程不会内联执行任务。
5. 任务提交失败：不在调用线程 fallback，后续 update 可恢复。
6. 播放最终帧：`onAnimationUpdate` 与 `onAnimationEnd` 顺序、线程与改动前一致。

### 6.2 VideoReader

1. decoder 永远返回 `TryAgainLater`：约 500ms 后返回 Stalled，不进入同步 fallback。
2. `TryAgainLater` 快速达到次数上限：仍归类 Stalled。
3. decoder 返回确定 Error：仍可在共享总预算内 fallback。
4. decoder 间歇成功但总体很慢：达到总预算后返回 Stalled。
5. 单次 codec API 自身阻塞超过预算：记录并验证软预算的边界，不做错误的硬上限断言。
6. 连续请求同一 targetFrame：首次失败后第二次确实重新解码。

### 6.3 帧结果与缓存

1. stalled frame 不更新 `currentFrame/preparedFrame` 成功状态。
2. stalled frame 不写入 PAGDecoder SequenceFile。
3. 当前帧失败和下一帧预取并发时，结果不串帧。
4. 多视频层中仅一个失败时，当前 PAGDecoder render 返回 false。
5. `cacheAllFramesInMemory=true` 后 resize，不复用旧尺寸 Bitmap。

### 6.4 Android 场景

1. 初次布局、attach、setPath、setComposition、setCurrentFrame 均验证主线程无同步 readFrame。
2. 重复 resize、detach/reattach、切换 composition，不发布过期结果。
3. 在低端设备和 CPU/内存压力下复现硬解停滞，Perfetto 验证主线程无 >5s 阻塞。
4. 回归 `PAGImageViewListener.onAnimationUpdate()` 的线程与完成时序。

---

## 七、风险与范围

- `PAGAnimator` 是跨平台公共组件，P0-A 需要回归 Android、iOS、OHOS 的 paused/manual update 行为。
- 播放最终帧的同步更新是独立风险；本次为保持 callback 顺序不一并修改。
- OHOS `JPAGImageView` 和其他同步 `PAGDecoder` 调用方仍依赖 P0-B 的软预算，不能获得严格的不阻塞保证。
- 腾讯地图使用的 `Engine-v4.37.8` 未包含 main/release 现有缓解；最终还需业务升级 SDK。
- 不建议直接给 `AsyncDataSource::getData()` 增加超时后返回，因为任务可能继续写数据，且通用 tgfx 调用方未必具备失败重试和资源生命周期处理。

## 八、预计改动范围

| 改动 | 主要文件 |
|---|---|
| P0-A animator 手动更新异步化 | `src/rendering/PAGAnimator.*`、tgfx task 提交/等待接口、相关测试 |
| P0-B 解码软预算 | `src/rendering/sequences/VideoReader.*`、mock decoder 测试 |
| P0-C 请求结果传播 | `SequenceReader.*`、`SequenceInfo.*`、`SequenceImageQueue.*`、`RenderCache.*`、`CompositionReader.cpp` |
| P1 异步公开接口 | Android `PAGImageView.java` 及生命周期/并发测试 |
