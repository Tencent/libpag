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
