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
