# CHANGELOG

## 4.2.77 (2023年5月4日)

### BugFixes

- 修复 Safari 和 iOS 微信浏览器的用户代理检测问题 (#905)
- 修复兼容老浏览器 globalThis 的问题，例如 iOS 12.1 Safari (#898)
- 更新 ImageBitmap 兼容性检查，以适应 Safari 15 (#884)
- 修复 WebMask 纹理类型上传错误导致的微信崩溃问题 (#877)
- 为内置 iOS 16.4 的 AppleWebKit 浏览器添加 OffscreenCanvas 回退 (#875)

### Refactor 

- 重构微信小程序的 PAGView (#871)

## 4.2.56 (2023年4月13日)

### Feature

- 添加 isInstanceOf 方法来替代 instanceOf，在 Web 上防止未定义的全局类问题 (#861)
- VideoReader 支持从 HTMLVideoElement 上创建 (#850)
- 整理与 Web 上上传纹理相关的代码 (#826)
- 发布 Web Lite SDK 0.0.7 版本 (#809)
- 在 API 文档中添加了 Web Workers 的接口 (#806)
- 在 Web Lite SDK 上启用抗锯齿并使用线性过滤 (#803)
- 从 Matrix 类上移除了透视值 (#794)
- 使用 tgfx::ImageBuffer 替代 WebVideoBuffer (#750)
- 在 Web 平台上支持从 HTTP URL 制作 ImageCodec (#745)
- 在 Web 平台上实现了 NativeCodec::readPixels() 方法 (#742)
- 使用 emsdk 安装 emscripten (#733)
- 实现了 ByteBuffer (#727)
- 在 Web 平台上添加了创建图像的静态函数 (#728)
- 锁定了 emscripten 版本到 3.1.20 (#725)
- 在 Web 上异步实现图像解码器 (#720)

### BugFixes

- 修复 Safari 版本低于 16.4 时由于缺少全局变量 OffscreenCanvas 而导致的类型检查错误 (#860)
- 修复在 Web 上 iOS 16.4 的 OffscreenCanvas 和 WebGL 接口之间的冲突 (#831)
- 修复了 Web 上的帧准备错误 (#823)
- 修复了 Web 上内存扩展后的指针丢失问题 (#799)
- 修复了在 iOS Safari 上的 MP4 崩溃问题 (#786)
- 修复了 desp.sh (#731)
- 修复了 WeChat 上的用户代理获取失败问题 (#718)

## 4.1.43 (2023年2月2日)

### Feature

- 在 VideoSequenceReader 中实现了 decodeFrame 方法 (#705)
- 添加了 Web Worker 版本 (#675)
- 更新了 README 和 CHANGLOG (#673)
- 在 Wechat 上使用 Date.now() 替代 wx.getPerformance().now() (#669)
- 解除了 VideoReader 和 PAGPlayer 之间的关联 (#667)
- 在 Web 上通过 JS 数组返回 PAGImageLayer.getVideoRanges (#661)
- 发布了 libpag-lite 0.0

### BugFixes

- 修复 Web 上的 onAnimationStart 事件（#688）
- 在 Safari 浏览器上修复可播放状态时的空视频帧。（#655）

## 4.1.35

### BugFixes

- 修复软解码器类型检查错误。(#643)
- 修复微信小程序上字体声明验证错误。(#649)
- 修复 Safari 上视频可播放状态时出现空帧的问题。(#655)

## 4.1.33

### Feature

- 重构 Web 上视频解码器。(#640)

### BugFixes

- Web 上使用 texSubImage2D 替代 texImage2D。(#619)
- 重构 PAGView 监听器类型检查。(#624)
- 修复微信小程序上替换图片失败的问题。(#630)

## 4.1.29

### BugFixes

- 修复 Chrome 上视频播放前调用 load() 产生错误的问题。(#613)
- 修复 PAGView duration 获取错误的问题。(#614)
- 重置 `UNPACK_PREMULTIPLY_ALPHA_WEBGL` 的默认值。(#616)

## 4.1.20

### Feature

- 优化多次读取 Canvas2D 的性能。(#580)

### BugFixes

- 修复视频解码器在 Web 平台静态区间暂停错误的问题。(#573)
- 修复页面不可见时视频解码器播放错误。

## 4.1.15

### Feature

- 发布支持微信小程序的版本。(#533)

### BugFixes

- 修复在有其他异步任务时 getLayerType 崩溃的问题。(#465)
- 修复多个视图使用同一个 Canvas 时销毁视图崩溃的问题。(#478)
- 当 BMP 预合成大于 4K 分辨率时，禁止视频解码器变速。(#548)
- 修复在 Android 微信小程序上视频解码器输出帧数据错误的问题。(#559)

## 4.0.5.26

### Feature

- 增加 BMP 序列帧预加载解码
- 增加解码倍速支持

### BugFixes

- 修复 iOS Safari 上 src 使用 blobUrl 时解码错误的问题
- 修复 displacementMapLayer 没有解码的问题

## 4.0.5.17

### Breaking Changes

- PAGSurface 上 `fromCanvas` 替代 `FromCanvas` , `fromTexture` 替代 `FromTexture` , `fromRenderTarget` 替代 `FromRenderTarget`
- PAGComposition 上 `make` 替代 `Make`.
- PAGSolidLayer 上 `make` 替代 `Make`.
- PAGImageLayer 上 `make` 替代 `Make`.

### Feature

- 增加 `backendContext` 类
- libpag 上增加 `version`
- PAGView 上增加 `onAnimationUpdate` 事件，增加 `setComposition` , `matrix` , `setMatrix` , `getLayersUnderPoint` 函数接口
- 支持注销软件解码器
- 当获取 PAGLayer 会返回对应类型的 TypeLayer

## 4.0.5.11

### BugFixes

- 修复字体名称存在标点符号时渲染错误

## 4.0.5.7

### Feature

- 增加字体字形渲染
- 增加兜底字体

### BugFixes

- 修复 OffscreenCanvas 在 Safari 上不存在导致断言失败
- 关闭在 iOS Safari 15.4+ WebGL 上默认开启的矢量设置

## 4.0.5.5

### Feature

- 发布 PAG Web SDK 正式版 🎉
- 更新到 4.0.5.5 与 libpag 的版本号同步

### Bug Fixes

- 修复视频序列帧状态检查错误，上屏失败

## 0.1.8

### Feature

- 支持从 `offscreenCanvas` 创建 `PAGView`
- `PAGSurface` 上增加 `readPixels` 接口

### BugFixes

- 修复缓存没有生效的问题

## 0.1.7

### Feature

- 增加软件解码器注册接口
- 将 MP4 合成逻辑放到 wasm 中执行
- 增加装饰器检查 wasm 实例状态
- 能从 `TexImageSource` 创建 PAGImage

### BugFixes

- 修复循环播放事件错误
- 优化 wasm 队列
- 修复一些 enum 类型绑定错误

## 0.1.6

### Breaking Changes

- `PAGViewOptions` 接口中移除 `fullBox` 字段，增加 `useScale` 和 `firstFrame` 字段
- 移除 `PAGView.setProgress` 中的 flush 动作

### Feature

- `PAGTextLayer` 上增加 `imageBytes` 方法

### BugFixes

- 修复 `canvas` 未挂载到 DOM 树上时和 `display:none` 时缩放错误的问题

## 0.1.5

### Feature

- `PAGView.init()` 上增加 `initOptions` 参数，包含 `fullBox` 和 `useCanvas2D` 方法
- `PAGView` 上增加 `setRenderCanvasSize` 方法

### Bug Fixes

- 修复 `PAGView.updateSize()` 更新 Canvas 尺寸逻辑
- 修复替换带空格文字的文本时渲染错误

## 0.1.4

### Bug Fixes

- 修复预测下一帧导致闪屏的问题
- 修复静态帧区间 Video 对齐问题

## 0.1.3

### Bug Fixes

- 修复 `destroy` 没有正常释放 `WebGL context` 的问题

## 0.1.2

### Bug Fixes

- 修复视频序列帧解码过程渲染错误
- 修复 `vector` 结构上的方法同步错误
- 修复 `fontFamily` 格式化错误
- 修复一个从 `texture` 创建 `PAGSurface` 的性能问题

## 0.1.1

### Bug Fixes

- 修复 `PAGView` 重复调用 `init` 会多次缩放 `Canvas` 的问题
- 修复 `PAGView` 的 `repeatCount` 为 1 时结束帧不是最后一帧的问题

## 0.1.0

### Breaking Changes

- `PAGFile.getLayersByEditableIndex` 返回值改为 `Vector<PAGLayer>`
- 消除 C++ enum `LayerType` 替换为 Js enum `LayerType`, `PAG.LayerType` 替换为 `PAG.types.LayerType`

### Features

- Add warn message when canvas size is more than 2560 on Web env. 当 canvas 渲染尺寸大于 2560px 的时候，提示警告
- `PAGFile` 的 `load` 方法支持 `Blob` 和 `ArrayBuffer` 类型的数据，并且 `PAGFile` 上增加 `maxSupportedTagLevel`，`tagLevel`，`copyOriginal` 方法。
- `PAGView` 上增加 `freeCache` 方法
- `PAGSurface` 上增加 `clearAll`
- `PAGPlayer` 上增加 `getSurface` ，`matrix` ，`setMatrix` ，`nextFrame` ， `preFrame` ， `autoClear` ， `setAutoClear` ， `getBounds` ， `getLayersUnderPoint` ， `hitTestPoint` ， `renderingTime` ，`imageDecodingTime` ， `presentingTime` ， `graphicsMemory` 方法
- `PAGImage` 上增加 `scaleMode` ， `setScaleMode` ， `matrix` ， `setMatrix` 方法
- `PAGLayer` 上增加 `matrix` ， `setMatrix` ， `resetMatrix` ， `getTotalMatrix` ， `parent` ， `markers` ， `setStartTime` ， `currentTime` ， `setCurrentTime` ， `getProgress` ， `setProgress` ， `preFrame` ， `nextFrame` ， `getBounds` ， `trackMatteLayer` ， `excludedFromTimeline` ， `setExcludedFromTimeline` ， `isPAGFile` ， `isDelete` 方法
- `PAGComposition` 上增加 `Make` ， `removeLayer` ， `getLayersUnderPoint` 方法
- `PAGFont` 上增加 `create` 方法和 `fontFamily` ， `fontStyle` 属性
- 增加 `PAGTextLayer` ， `PAGImageLayer` 类

## 0.0.5

### Bug Fixes

- 修复 `PAGImage` 的 `fromSource` 方法没有回溯 `Asyncify` 的状态

## 0.0.4

### Bug Fixes

- 修复消除 emscripten 中 `Asyncify` 模块带来副作用的 Typescript 装饰器遗漏了静态方法

## 0.0.3

### Breaking Changes

- 消除 emscripten 中 `Asyncify` 模块带来的副作用，将大部分与 wasm 交互的接口改为同步接口，仅保留 `PAGPlayer.flush()` 为 `async` 方法
- 替换 `module._PAGSurface` 为 `module.PAGSurface`

### Features

- `PAGComposition` 上增加 `numChildren`，`setContentSize`，`getLayerAt`，`getLayersByName`，`getLayerIndex`，`swapLayer`，`swapLayerAt`，`contains`，`addLayer`，`addLayerAt`，`audioStartTime`，`audioMarkers`，`audioBytes`，`removeLayerAt`，`removeAllLayers` 方法
- `PAGViewListenerEvent` 上增加 `onAnimationPla`，`onAnimationPause` ，`onAnimationFlushed`

### Bug Fixes

- 修复 `FontFamily` 渲染错误
- 修复微信平台下视频序列帧无法播放

## 0.0.2

### Breaking Changes

- 修改 `PAGView` 上的属性 `duration` 为方法 `duration()` 。使用：`PAGView.duration` ⇒ `PAGView.duration()`

### Features

- 支持从 `CanvasElement` 对象创建 `PAGView` 实例
- 兼容低版本浏览器文字测量，支持 `Chrome 69+` 浏览器
- `PAGFile` 上增加 `setDuration` ， `timeStretchMode` ， `setTimeStretchMode` 方法
- `PAGLayer` 上增加 `uniqueID` ， `layerType` ， `layerName` ， `opacity` ， `setOpacity` ， `visible` ， `setVisible` ， `editableIndex` ， `frameRate` ， `localTimeToGlobal` ， `globalToLocalTime` 方法

### Bug Fixes

- 修复 `PAGView` 的 `duration` 计算错误

## 0.0.1

### Features

- 发布 `Web` 平台 `libpag` 的 `wasm` 版本，支持全特性 `PAG`
- 支持 `Chrome 87+` ，`Safar 11.1+` 浏览器
