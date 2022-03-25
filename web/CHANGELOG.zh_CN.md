# CHANGELOG

## 0.0.1

### Features

- 发布 `Web` 平台 `libpag` 的 `wasm` 版本，支持全特性 `PAG`
- 支持 `Chrome 87+` ，`Safar 11.1+` 浏览器

## 0.0.2

### Breaking Changes

- 修改 `PAGView` 上的属性 `duration` 为方法 `duration()` 。使用：`PAGView.duration` ⇒ `PAGView.duration()`

### Features

- 支持从 `CanvasElement` 对象创建 `PAGView` 实例
- 兼容低版本浏览器文字测量，支持 `Chrome 69+` 浏览器
- `PAGFile` 上增加 `setDuration` ， `timeStretchMode` ， `setTimeStretchMode` 方法
- `PAGLayer` 上增加 `uniqueID` ， `layerType` ， `layerName` ， `opacity` ， `setOpacity` ， `visible` ， `setVisible`  ， `editableIndex` ， `frameRate` ， `localTimeToGlobal` ， `globalToLocalTime` 方法

### Bug Fixes

- 修复 `PAGView` 的 `duration` 计算错误

## 0.0.3

### Breaking Changes

- 消除 emscripten 中 `Asyncify` 模块带来的副作用，将大部分与 wasm 交互的接口改为同步接口，仅保留 `PAGPlayer.flush()` 为 `async` 方法
- 替换 `module._PAGSurface`  为 `module.PAGSurface`

### Features

- `PAGComposition` 上增加  `numChildren`，`setContentSize`，`getLayerAt`，`getLayersByName`，`getLayerIndex`，`swapLayer`，`swapLayerAt`，`contains`，`addLayer`，`addLayerAt`，`audioStartTime`，`audioMarkers`，`audioBytes`，`removeLayerAt`，`removeAllLayers` 方法
- `PAGViewListenerEvent` 上增加 `onAnimationPla`，`onAnimationPause` ，`onAnimationFlushed`

### Bug Fixes

- 修复 `FontFamily` 渲染错误
- 修复微信平台下视频序列帧无法播放

## 0.0.4

### Bug Fixes

- 修复消除 emscripten 中 `Asyncify` 模块带来副作用的Typescript 装饰器遗漏了静态方法

## 0.0.5

### Bug Fixes

- 修复 `PAGImage` 的 `fromSource` 方法没有回溯 `Asyncify` 的状态

## 0.1.0

### Breaking Changes

- `PAGFile.getLayersByEditableIndex` 返回值改为 `Vector<PAGLayer>`
- 消除 C++ enum `LayerType` 替换为 Js enum `LayerType`,  `PAG.LayerType` 替换为 `PAG.types.LayerType`

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

## 0.1.1

### Bug Fixes

- 修复 `PAGView` 重复调用 `init` 会多次缩放 `Canvas` 的问题
- 修复 `PAGView` 的 `repeatCount` 为1时结束帧不是最后一帧的问题
