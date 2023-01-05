# CHANGELOG

## 4.1.35

### BugFixes

- Fix SoftwareDecoder type checking error. (#643)
- Fix font family declarations is invalid on Wechat. (#649)
- Fix empty video frame when playable state on Safari. (#655)

## 4.1.33

### Feature

- Refactor video reader on Web. (#640)

### BugFixes

- Use texSubImage2D instead of texImage2D on Web. (#619)
- Refactor PAGView listener type checker. (#624)
- Fix replace image failed on WeChat- mini program. (#630)

## 4.1.29

### BugFixes

- Fix video play error when called load before on Chrome. (#613)
- Fix PAGView duration get error. (#614)
- Restore to default value of `UNPACK_PREMULTIPLY_ALPHA_WEBGL`. (#616)

## 4.1.20

### Feature

- Multiple readback operations using getImageData are faster. (#580)

### BugFixes

- Fix static time ranges pause error in video reader. (#573)
- Fix play video error when document hidden.

## 4.1.15

### Feature

- Add support for running on WeChat miniprogram. (#533)

### BugFixes

- Fix getLayerType crash when there has another async task. (#465)
- Fix destroy view clash when multiple views use the same canvas. (#478)
- Disable video playback rate when video sequence resolution is larger than 4k. (#548)
- Fix video decoder output frame error on the Android WeChat mini-program. (#559)

## 4.0.5.26

### Feature

- Add BMP sequence prepare.
- Add playbackRate support in decoder.

### BugFixes

- Fix video decoding failure when src is blobUrl on iOS Safari.
- Fix displacementMapLayer not decode.

## 4.0.5.17

### Breaking Changes

- Replace `FromCanvas` to `fromCanvas` , `FromTexture` to `fromTexture` , `FromRenderTarget` to `fromRenderTarget` on `PAGSurface`.
- Replace `Make` to `make` on `PAGComposition`.
- Replace `Make` to `make` on `PAGSolidLayer`.
- Replace `Make` to `make` on `PAGImageLayer`.

### Feature

- Add `backendContext`.
- Add onAnimationUpdate event.
- Add `version` on `libpag`.
- Add `setComposition` , `matrix` , `setMatrix` , `getLayersUnderPoint` on `PAGView`.
- Support unregister software decoder.
- Return TypeLayer when get PAGLayer.

## 4.0.5.11

### BugFixes

- Fix text rendering error when font-family has punctuation.

## 4.0.5.7

### Feature

- Add font style render in WebMask.
- Add fallback fontFamily.

### BugFixes

- Fix OffscreenCanvas assert error on Safari.
- Disable antialiasing when creating WebGL context from canvas.

## 4.0.5.5

### Feature

- Publish PAG Web SDK release version🎉
- Jump 4.0.5.5 to sync with libpag version

### Bug Fixes

- Fix video ready state assess error.

## 0.1.8

### Feature

- Support init `PAGView` from `offscreenCanvas`.
- Add `readPixels` function on `PAGSurface`.

### BugFixes

- Fix render cache validate fail.

## 0.1.7

### Feature

- Add software decoder registration function.
- Create MP4 container on wasm side.
- Add decorator to verify wasm status.
- PAGImage can be made from TexImageSource.

### BugFixes

- Fix the repeat event dispatch error.
- Optimize wasm queue.
- Fix some enum binding error.

## 0.1.6

### Breaking Changes

- Remove `fullBox` parameter in `PAGViewOptions`, add `useScale` and `firstFrame`.
- Remove flush action in `PAGView.setProgress`.

### Feature

- Add `imageBytes` on `PAGTextLayer`

### BugFixes

- Fix scale error when canvas hasn't mounted on DOM tree or `display:none`.

## 0.1.5

### Feature

- Add `initOptions` parameter what include `fullBox` and `useCanvas2D` on `PAGView.init()`.
- Add `setRenderCanvasSize` on `PAGView`.

### Bug Fixes

- Fix `PAGView.updateSize()` update canvas size error。
- Fix replace text when including space error.

## 0.1.4

### Bug Fixes

- Fix decoding the next frame error in Web env.
- Fix the frame align error when is in the static time range.

## 0.1.3

### Bug Fixes

- Fix WebGL context release error by destroy.

## 0.1.2

### Bug Fixes

- Fix video sequence reader decode frame error.
- Fix vector function async error.
- Fix font family format error.
- Fix the performance issue of a PAGSurface made from texture.

## 0.1.1

### Bug Fixes

- Fix canvas rescaling error when repeated call init function.
- Fix render end frame error when repeat count is 1.

## 0.1.0

### Breaking Changes

- Edit returns from `PAGFile.getLayersByEditableIndex` be `Vector<PAGLayer>`.
- Replace C++ enum `LayerType` to Js enum `LayerType`, replace `PAG.LayerType` to `libpag.types.LayerType`.

### Features

- Add warn message when canvas size is more than 2560 on Web env.
- Make `PAGFile.load` support data what's type is `Blob` or `ArrayBuffer` ，and add `maxSupportedTagLevel`, `tagLevel`, `copyOriginal` on `PAGFile`.
- Add `freeCache` on `PAGView`.
- ADD `clearAll` on `PAGSurface.`
- Add `getSurface` , `matrix`, `setMatrix` , `nextFrame` , `preFrame` , `autoClear` , `setAutoClear`, `getBounds,` `getLayersUnderPoint`, `hitTestPoint`, `renderingTime` , `imageDecodingTime`, `presentingTime` , `graphicsMemory` on `PAGPlayer`.
- Add `scaleMode` , `setScaleMode` , `matrix` , `setMatrix` on `PAGImage`.
- Add `matrix`, `setMatrix`, `resetMatrix`, `getTotalMatrix`, `parent`, `markers`, `setStartTime`, `currentTime`, `setCurrentTime`, `getProgress`, `setProgress`, `preFrame` , `nextFrame`, `getBounds`, `trackMatteLayer`, `excludedFromTimeline`, `setExcludedFromTimeline`, `isPAGFile`, `isDelete` on `PAGLayer`.
- Add `Make`, `removeLayer`, `getLayersUnderPoint` on `PAGComposition`.
- Add `create`, `fontFamily`, `fontStyle` on `PAGFont`.
- Add `PAGTextLayer`, `PAGImageLayer` Class.

## 0.0.5

### Bug Fixes

- Fix `PAGImage.fromSource` miss rewind `Asyncify` currData.

## 0.0.4

### Bug Fixes

- Fix some static functions that eliminate the side effects caused by the `Asyncify` module in emscripten miss in the Typescript decorator.

## 0.0.3

### Breaking Changes

- Eliminate the side effects caused by the `Asyncify` module in emscripten, change most of the methods that interact with wasm to `await` methods, and only keep `PAGPlayer.flush()` as an `async` method.
- Replace _`module_.\_PAGSurface` to *`module\*.PAGSurface`.

### Features

- Add `numChildren`，`setContentSize`，`getLayerAt`，`getLayersByName`，`getLayerIndex`，`swapLayer`，`swapLayerAt`，`contains`，`addLayer`，`addLayerAt`，`audioStartTime`，`audioMarkers`，`audioBytes`，`removeLayerAt`，`removeAllLayers` on `PAGComposition`.
- Add `onAnimationPla`，`onAnimationPause` ，`onAnimationFlushed` on `PAGViewListenerEvent`.

### Bug Fixes

- Fix font family rending error.
- Fix video sequence can not play on Wechat.

## 0.0.2

### Breaking Changes

- Change property `duration` to be function `duration()` on PAGView. Like: `PAGView.duration` ⇒ `PAGView.duration()`

### Features

- Support create pag view from the canvas element.
- Support measure text in low version browser, support Chrome 69+.
- Add `setDuration` ， `timeStretchMode` ，and `setTimeStretchMode` on `PAGFile`.
- Add `uniqueID` ， `layerType` ， `layerName` ， `opacity` ， `setOpacity` ， `visible` ， `setVisible` ， `editableIndex` ， `frameRate` ， `localTimeToGlobal` ，and `globalToLocalTime` on `PAGLayer`.

### Bug Fixes

- Fix `duration` on `PAGView` calculate error.

## 0.0.1

### Features

- Publish PAG Web SDK
- Support Chrome 87+ and Safari 11.1+ Browser
