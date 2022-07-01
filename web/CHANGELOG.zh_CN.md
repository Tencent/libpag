# CHANGELOG

## 4.0.5.16

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
