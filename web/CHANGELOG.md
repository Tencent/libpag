# CHANGELOG

## 0.0.1

### Features

- Publish PAG Web SDK
- Support Chrome 87+ and Safari 11.1+ Browser

## 0.0.2

### Breaking Changes

- Change property `duration` to be function `duration()` on PAGView. Like: `PAGView.duration` ⇒ `PAGView.duration()`

### Features

- Support create pag view from the canvas element.
- Support measure text in low version browser, support Chrome 69+.
- Add `setDuration` ， `timeStretchMode` ，and `setTimeStretchMode` on `PAGFile`.
- Add `uniqueID` ， `layerType` ， `layerName` ， `opacity` ， `setOpacity` ， `visible` ， `setVisible`  ， `editableIndex` ， `frameRate` ， `localTimeToGlobal` ，and `globalToLocalTime` on `PAGLayer`.

### Bug Fixes

- Fix `duration` on `PAGView` calculate error.

## 0.0.3

### Breaking Changes

- Eliminate the side effects caused by the `Asyncify` module in emscripten, change most of the methods that interact with wasm to `await` methods, and only keep `PAGPlayer.flush()` as an `async` method.
- Replace  *`module*._PAGSurface`  to  *`module*.PAGSurface`.

### Features

- Add  `numChildren`，`setContentSize`，`getLayerAt`，`getLayersByName`，`getLayerIndex`，`swapLayer`，`swapLayerAt`，`contains`，`addLayer`，`addLayerAt`，`audioStartTime`，`audioMarkers`，`audioBytes`，`removeLayerAt`，`removeAllLayers`  on `PAGComposition`.
- Add  `onAnimationPla`，`onAnimationPause` ，`onAnimationFlushed` on `PAGViewListenerEvent`.

### Bug Fixes

- Fix font family rending error.
- Fix video sequence can not play on Wechat.

## 0.0.4

### Bug Fixes

- Fix some static functions that eliminate the side effects caused by the `Asyncify` module in emscripten miss in the Typescript decorator.

## 0.0.5

### Bug Fixes

- Fix `PAGImage.fromSource` miss rewind `Asyncify` currData.

## 0.1.0

### Breaking Changes

- Edit returns from `PAGFile.getLayersByEditableIndex` be `Vector<PAGLayer>`.
- Replace C++ enum `LayerType` to Js enum `LayerType`,  replace `PAG.LayerType` to `libpag.types.LayerType`.

### Features

- Add warn message when canvas size is more than 2560 on Web env.
- Make `PAGFile.load` support data what's type is  `Blob` or `ArrayBuffer` ，and add `maxSupportedTagLevel`, `tagLevel`, `copyOriginal` on `PAGFile`.
- Add `freeCache` on  `PAGView`.
- ADD `clearAll` on `PAGSurface.`
- Add  `getSurface` , `matrix`, `setMatrix` , `nextFrame` ,  `preFrame` ,  `autoClear` , `setAutoClear`, `getBounds,` `getLayersUnderPoint`,  `hitTestPoint`,  `renderingTime` , `imageDecodingTime`,  `presentingTime` ,  `graphicsMemory` on `PAGPlayer`.
- Add  `scaleMode`  ,  `setScaleMode` , `matrix` , `setMatrix` on `PAGImage`.
- Add `matrix`, `setMatrix`, `resetMatrix`, `getTotalMatrix`, `parent`, `markers`, `setStartTime`, `currentTime`, `setCurrentTime`, `getProgress`, `setProgress`, `preFrame` , `nextFrame`, `getBounds`, `trackMatteLayer`, `excludedFromTimeline`,  `setExcludedFromTimeline`, `isPAGFile`, `isDelete` on `PAGLayer`.
- Add `Make`, `removeLayer`, `getLayersUnderPoint` on `PAGComposition`.
- Add `create`, `fontFamily`, `fontStyle` on `PAGFont`.
- Add `PAGTextLayer`, `PAGImageLayer` Class.