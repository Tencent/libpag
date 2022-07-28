/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>  // for windows
#include <unordered_map>
#include "pag/decoder.h"
#include "pag/gpu.h"
#include "pag/types.h"

namespace tgfx {
struct Rect;
class Context;
class Surface;
}  // namespace tgfx

namespace pag {
class Recorder;

class RenderCache;

class Drawable;

class Content {
 public:
  virtual ~Content() = default;

 protected:
  virtual void measureBounds(tgfx::Rect* bounds) = 0;

  virtual void draw(Recorder* recorder) = 0;

  friend class FilterRenderer;

  friend class LayerRenderer;

  friend class MemoryCalculator;

  friend class TrackMatteRenderer;

  friend class RenderCache;

  friend class PAGLayer;

  friend class PAGComposition;

  friend class PAGTextLayer;
};

class Graphic;

class PAGLayer;

/**
 * An image used to replace the contents of PAGImageLayers in a PAGFile.
 */
class PAG_API PAGImage {
 public:
  /**
   * Creates a PAGImage object from a path of a image file, return null if the file does not exist
   * or it's not a valid image file.
   */
  static std::shared_ptr<PAGImage> FromPath(const std::string& filePath);

  /**
   * Creates a PAGImage object from the specified file bytes, return null if the bytes is empty or
   * it's not a valid image file.
   */
  static std::shared_ptr<PAGImage> FromBytes(const void* bytes, size_t length);

  /**
   * Creates a PAGImage object from an array of pixel data, return null if it's not valid pixels.
   * @param pixels The pixel data to copy from.
   * @param width The width of the pixel data.
   * @param height The height of the pixel data.
   * @param rowBytes The number of bytes between subsequent rows of the pixel data.
   * @param colorType Describes how to interpret the components of a pixel.
   * @param alphaType Describes how to interpret the alpha component of a pixel.
   */
  static std::shared_ptr<PAGImage> FromPixels(const void* pixels, int width, int height,
                                              size_t rowBytes, ColorType colorType,
                                              AlphaType alphaType);

  /**
   * Creates a PAGImage object from the specified backend texture, return null if the texture is
   * invalid.
   */
  static std::shared_ptr<PAGImage> FromTexture(const BackendTexture& texture, ImageOrigin origin);

  virtual ~PAGImage() = default;

  /**
   * Returns a globally unique id for this object.
   */
  ID uniqueID() const {
    return _uniqueID;
  }

  /**
   * Returns the width in pixels.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height in pixels.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the current scale mode. The default value is PAGScaleMode::LetterBox.
   */
  int scaleMode() const;

  /**
   * Specifies the rule of how to scale the content to fit the image layer's original size.
   * The matrix changes when this method is called.
   */
  void setScaleMode(int mode);

  /**
   * Returns a copy of current matrix.
   */
  Matrix matrix() const;

  /**
   * Set the transformation which will be applied to the content.
   * The scaleMode property will be set to PAGScaleMode::None when this method is called.
   */
  void setMatrix(const Matrix& matrix);

 protected:
  PAGImage(int width, int height);

  virtual std::shared_ptr<Graphic> getGraphic() = 0;

  virtual bool isStill() const = 0;

  virtual bool setContentTime(int64_t time) = 0;

 private:
  mutable std::mutex locker = {};
  ID _uniqueID = 0;
  int _width = 0;
  int _height = 0;
  int _scaleMode = PAGScaleMode::LetterBox;
  Matrix _matrix = Matrix::I();
  bool hasSetScaleMode = false;
  PAGLayer* _owner = nullptr;

  Matrix getContentMatrix(int defaultScaleMode, int contentWidth, int contentHeight);

  void setOwner(PAGLayer* owner);

  PAGLayer* getOwner() const;

  friend class ImageReplacement;

  friend class PAGImageLayer;

  friend class PAGFile;

  friend class RenderCache;

  friend class AudioClip;
};

class PAGComposition;

class PAGImageLayer;

class PAGStage;

class PAG_API PAGFont {
 public:
  /**
   * Registers a font required by the text layers in pag files, so that they can be rendered as
   * designed.
   */
  static PAGFont RegisterFont(const std::string& fontPath, int ttcIndex,
                              const std::string& fontFamily = "",
                              const std::string& fontStyle = "");
  /**
   * Registers a font required by the text layers in pag files, so that they can be rendered as
   * designed.
   */
  static PAGFont RegisterFont(const void* data, size_t length, int ttcIndex,
                              const std::string& fontFamily = "",
                              const std::string& fontStyle = "");

  /**
   * Unregister a font.
   */
  static void UnregisterFont(const PAGFont& font);

  /**
   * Resets the fallback font names. It should be called only once when the application is being
   * initialized.
   */
  static void SetFallbackFontNames(const std::vector<std::string>& fontNames);

  /**
   * Resets the fallback font paths. It should be called only once when the application is being
   * initialized.
   */
  static void SetFallbackFontPaths(const std::vector<std::string>& fontPaths,
                                   const std::vector<int>& ttcIndices);

  PAGFont(std::string fontFamily, std::string fontStyle)
      : fontFamily(std::move(fontFamily)), fontStyle(std::move(fontStyle)) {
  }

  /**
   * A string with the name of the font family.
   **/
  const std::string fontFamily;
  /**
   * A string with the style information — e.g., “bold”, “italic”.
   **/
  const std::string fontStyle;
};

class File;

class Layer;

class LayerCache;

class Content;

class Transform;

class PAGFile;

class PAG_API PAGLayer : public Content {
 public:
  PAGLayer(std::shared_ptr<File> file, Layer* layer);

  ~PAGLayer() override;

  /**
   * Returns a globally unique id for this object.
   */
  ID uniqueID() const;

  /**
   * Returns the type of layer.
   */
  LayerType layerType() const;

  /**
   * Returns the name of the layer.
   */
  std::string layerName() const;

  /**
   * A matrix object containing values that alter the scaling, rotation, and translation of the
   * layer. Altering it does not change the animation matrix, and it will be concatenated to current
   * animation matrix for displaying.
   */
  Matrix matrix() const;

  void setMatrix(const Matrix& value);

  /**
   * Resets the matrix to its default value.
   */
  void resetMatrix();

  /**
   * The final matrix for displaying, it is the combination of the matrix property and current
   * matrix from animation.
   */
  Matrix getTotalMatrix();

  /**
   * Returns the current alpha of the layer if previously set.
   */
  float alpha() const;

  /**
   * Set the alpha of the layer, which will be concatenated to the current animation opacity for
   * displaying.
   */
  void setAlpha(float value);

  /**
   * Whether or not the layer is visible.
   */
  bool visible() const;

  /**
   * Set the visible of the layer.
   */
  void setVisible(bool value);

  /**
   * Ranges from 0 to PAGFile.numTexts - 1 if the layer type is text, or from 0 to PAGFile.numImages
   * -1 if the layer type is image, otherwise returns -1.
   */
  int editableIndex() const;

  /**
   * Returns the parent PAGComposition of current PAGLayer.
   */

  std::shared_ptr<PAGComposition> parent() const;

  /**
   * Returns the markers of this layer.
   */
  std::vector<const Marker*> markers() const;

  /**
   * Converts the time from the PAGLayer's (local) timeline to the PAGSurface (global) timeline. The
   * time is in microseconds.
   */
  int64_t localTimeToGlobal(int64_t localTime) const;

  /**
   * Converts the time from the PAGSurface (global) to the PAGLayer's (local) timeline timeline. The
   * time is in microseconds.
   */
  int64_t globalToLocalTime(int64_t globalTime) const;

  /**
   * The duration of the layer in microseconds, indicates the length of the visible range.
   */
  int64_t duration() const;

  /**
   * Returns the frame rate of this layer.
   */
  float frameRate() const;

  /**
   * The start time of the layer in microseconds, indicates the start position of the visible range
   * in parent composition. It could be negative value.
   */
  int64_t startTime() const;

  /**
   * Set the start time of the layer, in microseconds.
   */
  void setStartTime(int64_t time);

  /**
   * The current time of the layer in microseconds, the layer is invisible if currentTime is not in
   * the visible range (startTime <= currentTime < startTime + duration).
   */
  int64_t currentTime() const;

  /**
   * Set the current time of the layer, in microseconds.
   */
  void setCurrentTime(int64_t time);

  /**
   * Returns the current progress of play position, the value ranges from 0.0 to 1.0.
   */
  double getProgress();

  /**
   * Set the progress of play position, the value ranges from 0.0 to 1.0. A value of 0.0 represents
   * the frame at startTime. A value of 1.0 represents the frame at the end of duration.
   */
  void setProgress(double percent);

  /**
   * Set the progress of play position to the previous frame.
   */
  void preFrame();

  /**
   * Set the progress of play position to the next frame.
   */
  void nextFrame();

  /**
   * Returns a rectangle that defines the original area of the layer, which is not transformed by
   * the matrix.
   */
  Rect getBounds();

  /**
   * Returns trackMatte layer of this layer.
   */
  std::shared_ptr<PAGLayer> trackMatteLayer() const;

  /**
   * Indicate whether this layer is excluded from parent's timeline. If set to true, this layer's
   * current time will not change when parent's current time changes.
   */
  bool excludedFromTimeline() const;

  /**
   * Set the excludedFromTimeline flag of this layer.
   */
  void setExcludedFromTimeline(bool value);

  /**
   * Returns true if this layer is a PAGFile.
   */
  virtual bool isPAGFile() const;

  // internal use only.
  void* externalHandle = nullptr;
  std::shared_ptr<File> getFile() const;

 protected:
  std::shared_ptr<std::mutex> rootLocker = nullptr;
  Layer* layer = nullptr;
  LayerCache* layerCache = nullptr;
  PAGStage* stage = nullptr;
  PAGComposition* _parent = nullptr;
  Frame startFrame = 0;
  Frame contentFrame = 0;
  // It could be nullptr when the layer is created by PAGComposition(width, height),
  // check before using it!
  std::shared_ptr<File> file = nullptr;
  PAGFile* rootFile = nullptr;
  std::weak_ptr<PAGLayer> weakThis;
  Matrix layerMatrix = {};
  float layerAlpha = 1.0f;
  PAGLayer* trackMatteOwner = nullptr;

  const Layer* getLayer() const;
  const PAGStage* getStage() const;
  Frame localFrameToGlobal(Frame localFrame) const;
  Frame globalToLocalFrame(Frame globalFrame) const;
  Point globalToLocalPoint(float stageX, float stageY);
  void draw(Recorder* recorder) override;
  void measureBounds(tgfx::Rect* bounds) override;
  Matrix getTotalMatrixInternal();
  virtual void setMatrixInternal(const Matrix& matrix);
  virtual float frameRateInternal() const;
  double getProgressInternal();
  void setProgressInternal(double percent);
  void preFrameInternal();
  void nextFrameInternal();
  virtual bool gotoTime(int64_t layerTime);
  virtual Frame childFrameToLocal(Frame childFrame, float childFrameRate) const;
  virtual Frame localFrameToChild(Frame localFrame, float childFrameRate) const;

  /**
   * Marks the content of parent (and parent's parent...) changed. It also marks this layer's
   * content changed if you pass true in the contentChanged parameter.
   */
  void notifyModified(bool contentChanged = false);
  virtual bool contentModified() const;
  virtual bool cacheFilters() const;
  virtual Frame frameDuration() const;
  virtual Frame stretchedFrameDuration() const;
  virtual Frame stretchedContentFrame() const;
  virtual int64_t durationInternal() const;
  virtual int64_t startTimeInternal() const;
  virtual Content* getContent();
  virtual void invalidateCacheScale();
  virtual void onAddToStage(PAGStage* pagStage);
  virtual void onRemoveFromStage();
  virtual void onAddToRootFile(PAGFile* pagFile);
  virtual void onRemoveFromRootFile();
  virtual void onTimelineChanged();
  virtual void updateRootLocker(std::shared_ptr<std::mutex> locker);

 private:
  ID _uniqueID = 0;
  bool layerVisible = true;
  bool _excludedFromTimeline = false;
  std::shared_ptr<PAGLayer> _trackMatteLayer = nullptr;
  int _editableIndex = -1;
  uint32_t contentVersion = 0;

  void setVisibleInternal(bool value);
  void setStartTimeInternal(int64_t time);
  int64_t currentTimeInternal() const;
  Frame currentFrameInternal() const;
  bool setCurrentTimeInternal(int64_t time);
  bool frameVisible() const;
  void removeFromParentOrOwner();
  void attachToTree(std::shared_ptr<std::mutex> newLocker, PAGStage* newStage);
  void detachFromTree();
  PAGLayer* getTimelineOwner() const;
  PAGLayer* getParentOrOwner() const;
  bool getTransform(Transform* transform);
  bool gotoTimeAndNotifyChanged(int64_t targetTime);

  friend class PAGComposition;

  friend class PAGFile;

  friend class TrackMatteRenderer;

  friend class FilterModifier;

  friend class RenderCache;

  friend class PAGStage;

  friend class PAGPlayer;

  friend class FileReporter;

  friend class PAGImageLayer;
};

class SolidLayer;

class PAG_API PAGSolidLayer : public PAGLayer {
 public:
  static std::shared_ptr<PAGSolidLayer> Make(int64_t duration, int32_t width, int32_t height,
                                             Color solidColor, Opacity opacity = 255);

  PAGSolidLayer(std::shared_ptr<File> file, SolidLayer* layer);

  ~PAGSolidLayer() override;

  /**
   * Returns the layer's solid color.
   */
  Color solidColor();

  /**
   * Set the the layer's solid color.
   */
  void setSolidColor(const Color& value);

 protected:
  Content* getContent() override;
  bool contentModified() const override;

 private:
  SolidLayer* emptySolidLayer = nullptr;
  Content* replacement = nullptr;
  Color _solidColor = White;
};

class TextLayer;

class TextReplacement;

class PAG_API PAGTextLayer : public PAGLayer {
 public:
  static std::shared_ptr<PAGTextLayer> Make(int64_t duration, std::string text, float fontSize = 24,
                                            std::string fontFamily = "",
                                            std::string fontStyle = "");

  static std::shared_ptr<PAGTextLayer> Make(int64_t duration,
                                            std::shared_ptr<TextDocument> textDocumentHandle);

  PAGTextLayer(std::shared_ptr<File> file, TextLayer* layer);
  ~PAGTextLayer() override;

  /**
   * Returns the text layer’s fill color.
   */
  Color fillColor() const;

  /**
   * Set the text layer’s fill color.
   */
  void setFillColor(const Color& value);

  /**
   * Returns the text layer's font.
   */
  PAGFont font() const;

  /**
   * Set the text layer's font.
   */
  void setFont(const PAGFont& font);

  /**
   * Returns the text layer's font size.
   */
  float fontSize() const;

  /**
   * Set the text layer's font size.
   */
  void setFontSize(float size);

  /**
   * Returns the text layer's stroke color.
   */
  Color strokeColor() const;

  /**
   * Set the text layer's stroke color.
   */
  void setStrokeColor(const Color& color);

  /**
   * Returns the text layer's text.
   */
  std::string text() const;

  /**
   * Set the text layer's text.
   */
  void setText(const std::string& text);

  /**
   * Reset the text layer to its default text data.
   */
  void reset();

 protected:
  void replaceTextInternal(std::shared_ptr<TextDocument> textData);
  void setMatrixInternal(const Matrix& matrix) override;
  Content* getContent() override;
  bool contentModified() const override;

 private:
  TextLayer* emptyTextLayer = nullptr;

  TextReplacement* replacement = nullptr;

  const TextDocument* textDocumentForRead() const;

  TextDocument* textDocumentForWrite();

  friend class PAGFile;

  friend class TextReplacement;
};

class ShapeLayer;

class PAG_API PAGShapeLayer : public PAGLayer {
 public:
  PAGShapeLayer(std::shared_ptr<File> file, ShapeLayer* layer);
};

/**
 * Represents a time range from the content of PAGImageLayer.
 */
class PAG_API PAGVideoRange {
 public:
  PAGVideoRange(int64_t startTime, int64_t endTime, int64_t playDuration)
      : _startTime(startTime), _endTime(endTime), _playDuration(playDuration) {
  }

  /**
   * The start time of the source video, in microseconds.
   */
  int64_t startTime() const {
    return _startTime < _endTime ? _startTime : _endTime;
  }

  /**
   * The end time of the source video (not included), in microseconds.
   */
  int64_t endTime() const {
    return _startTime < _endTime ? _endTime : _startTime;
  }

  /**
   * The duration for playing after applying speed.
   */
  int64_t playDuration() const {
    return _playDuration;
  }

  /**
   * Indicates whether the video should play backward.
   */
  bool reversed() const {
    return _startTime > _endTime;
  }

 private:
  int64_t _startTime;
  int64_t _endTime;
  int64_t _playDuration;
};

class ImageLayer;

class ImageReplacement;

template <typename T>
class Property;

template <typename T>
class AnimatableProperty;

class PAG_API PAGImageLayer : public PAGLayer {
 public:
  /**
   * Make a PAGImageLayer with with, height and duration(in microseconds).
   */
  static std::shared_ptr<PAGImageLayer> Make(int width, int height, int64_t duration);

  PAGImageLayer(std::shared_ptr<File> file, ImageLayer* layer);

  ~PAGImageLayer() override;

  /**
   * Returns the content duration in microseconds, which indicates the minimal length required for
   * replacement.
   */
  int64_t contentDuration();

  /**
   * Returns the time ranges of the source video for replacement.
   */
  std::vector<PAGVideoRange> getVideoRanges() const;

  /**
   * [Deprecated]
   * Replace the original image content with the specified PAGImage object.
   * Passing in null for the image parameter resets the layer to its default image content.
   * The replaceImage() method modifies all associated PAGImageLayers that have the same
   * editableIndex to this layer.
   *
   * @param image The PAGImage object to replace with.
   */
  void replaceImage(std::shared_ptr<PAGImage> image);

  /**
   * Replace the original image content with the specified PAGImage object.
   * Passing in null for the image parameter resets the layer to its default image content.
   * The setImage() method only modifies the content of the calling PAGImageLayer.
   *
   * @param image The PAGImage object to replace with.
   */
  void setImage(std::shared_ptr<PAGImage> image);

  /**
   * Converts the time from the PAGImageLayer's timeline to the replacement content's timeline. The
   * time is in microseconds.
   */
  int64_t layerTimeToContent(int64_t layerTime);

  /**
   * Converts the time from the replacement content's timeline to the PAGLayer's timeline. The time
   * is in microseconds.
   */
  int64_t contentTimeToLayer(int64_t contentTime);

  /**
   * The default image data of this layer, which is webp format.
   */
  ByteData* imageBytes() const;

 protected:
  bool gotoTime(int64_t layerTime) override;
  void setImageInternal(std::shared_ptr<PAGImage> image);
  int64_t getCurrentContentTime(int64_t layerTime);
  Property<float>* getContentTimeRemap();
  bool contentVisible();
  Content* getContent() override;
  bool contentModified() const override;
  bool cacheFilters() const override;
  void onRemoveFromRootFile() override;
  void onTimelineChanged() override;
  int64_t localFrameToFileFrame(int64_t localFrame) const;
  int64_t fileFrameToLocalFrame(int64_t fileFrame) const;

 private:
  ImageLayer* emptyImageLayer = nullptr;
  ImageReplacement* replacement = nullptr;
  std::unique_ptr<Property<float>> contentTimeRemap;

  PAGImageLayer(int width, int height, int64_t duration);
  bool hasPAGImage() const;
  std::shared_ptr<PAGImage> getPAGImage() const;
  std::unique_ptr<AnimatableProperty<float>> copyContentTimeRemap();
  TimeRange getVisibleRangeInFile();
  static void BuildContentTimeRemap(AnimatableProperty<float>* property, PAGFile* fileOwner,
                                    const TimeRange& visibleRange, double frameScale);
  static void ExpandPropertyByRepeat(AnimatableProperty<float>* property, PAGFile* fileOwner,
                                     Frame contentDuration);
  static Frame ScaleTimeRemap(AnimatableProperty<float>* property, const TimeRange& visibleRange,
                              double frameScale, Frame fileEndFrame);
  Frame getFrameFromTimeRemap(Frame value);
  void measureBounds(tgfx::Rect* bounds) override;

  friend class RenderCache;

  friend class PAGStage;

  friend class PAGFile;

  friend class AudioClip;
};

class PreComposeLayer;

class VectorComposition;

class PAG_API PAGComposition : public PAGLayer {
 public:
  /**
   * Make a empty PAGComposition with specified size.
   */
  static std::shared_ptr<PAGComposition> Make(int width, int height);

  PAGComposition(std::shared_ptr<File> file, PreComposeLayer* layer);

  ~PAGComposition() override;

  /**
   * Returns the width of the Composition.
   */
  int width() const;

  /**
   * Returns the height of the Composition.
   */
  int height() const;

  /**
   * Set the width and height of the Composition.
   */
  void setContentSize(int width, int height);

  /**
   * Returns the number of child layers of this composition.
   */
  int numChildren() const;

  /**
   * Returns the child layer that exists at the specified index.
   * @param index The index position of the child layer.
   * @returns The child layer at the specified index position.
   */
  std::shared_ptr<PAGLayer> getLayerAt(int index) const;

  /**
   * Returns the index position of a child layer.
   * @param pagLayer The layer instance to identify.
   * @returns The index position of the child layer to identify.
   */
  int getLayerIndex(std::shared_ptr<PAGLayer> pagLayer) const;

  /**
   * Changes the position of an existing child layer in the composition. This affects the layering
   * of child layers.
   * @param pagLayer The child layer for which you want to change the index number.
   * @param index The resulting index number for the child layer.
   */
  void setLayerIndex(std::shared_ptr<PAGLayer> pagLayer, int index);

  /**
   * Add a PAGLayer to current PAGComposition at the top. If you add a layer that already has a
   * different PAGComposition object as a parent, the layer is removed from the other PAGComposition
   * object.
   */
  bool addLayer(std::shared_ptr<PAGLayer> pagLayer);

  /**
   * Add a PAGLayer to current PAGComposition at the specified index. If you add a layer that
   * already has a different PAGComposition object as a parent, the layer is removed from the other
   * PAGComposition object.
   */
  bool addLayerAt(std::shared_ptr<PAGLayer> pagLayer, int index);

  /**
   * Check whether current PAGComposition contains the specified pagLayer.
   */
  bool contains(std::shared_ptr<PAGLayer> pagLayer) const;

  /**
   * Remove the specified PAGLayer from current PAGComposition.
   */
  std::shared_ptr<PAGLayer> removeLayer(std::shared_ptr<PAGLayer> pagLayer);

  /**
   * Remove the PAGLayer at specified index from current PAGComposition.
   */
  std::shared_ptr<PAGLayer> removeLayerAt(int index);

  /**
   * Remove all PAGLayers from current PAGComposition.
   */
  void removeAllLayers();

  /**
   * Swap the layers.
   */
  void swapLayer(std::shared_ptr<PAGLayer> pagLayer1, std::shared_ptr<PAGLayer> pagLayer2);

  /**
   * Swap the layers at the specified index.
   */
  void swapLayerAt(int index1, int index2);

  /**
   * The audio data of this composition, which is an AAC audio in an MPEG-4 container.
   */
  ByteData* audioBytes() const;

  /**
   * Returns the audio markers of this composition.
   */
  std::vector<const Marker*> audioMarkers() const;

  /**
   * Indicates when the first frame of the audio plays in the composition's timeline.
   */
  int64_t audioStartTime() const;

  /**
   * Returns an array of layers that match the specified layer name.
   */
  std::vector<std::shared_ptr<PAGLayer>> getLayersByName(const std::string& layerName);

  /**
   * Returns an array of layers that lie under the specified point. The point is in pixels and from
   * this PAGComposition's local coordinates.
   */
  std::vector<std::shared_ptr<PAGLayer>> getLayersUnderPoint(float localX, float localY);

 protected:
  int _width = 0;
  int _height = 0;
  int64_t _frameDuration = 1;
  float _frameRate = 60;
  std::vector<std::shared_ptr<PAGLayer>> layers;

  PAGComposition(int width, int height);
  std::vector<std::shared_ptr<PAGLayer>> getLayersBy(
      std::function<bool(PAGLayer* pagLayer)> filterFunc);
  float frameRateInternal() const override;
  bool gotoTime(int64_t layerTime) override;
  Frame childFrameToLocal(Frame childFrame, float childFrameRate) const override;
  Frame localFrameToChild(Frame localFrame, float childFrameRate) const override;
  int widthInternal() const;
  int heightInternal() const;
  void setContentSizeInternal(int width, int height);
  void draw(Recorder* recorder) override;
  void measureBounds(tgfx::Rect* bounds) override;
  bool hasClip() const;
  Frame frameDuration() const override;
  bool cacheFilters() const override;
  void invalidateCacheScale() override;
  void onAddToStage(PAGStage* pagStage) override;
  void onRemoveFromStage() override;
  void onAddToRootFile(PAGFile* pagFile) override;
  void onRemoveFromRootFile() override;
  void onTimelineChanged() override;
  void updateRootLocker(std::shared_ptr<std::mutex> locker) override;
  virtual bool doAddLayer(std::shared_ptr<PAGLayer> pagLayer, int index);
  virtual std::shared_ptr<PAGLayer> doRemoveLayer(int index);

 private:
  VectorComposition* emptyComposition = nullptr;

  static void FindLayers(std::function<bool(PAGLayer* pagLayer)> filterFunc,
                         std::vector<std::shared_ptr<PAGLayer>>* result,
                         std::shared_ptr<PAGLayer> pagLayer);
  static void MeasureChildLayer(tgfx::Rect* bounds, PAGLayer* childLayer);
  static void DrawChildLayer(Recorder* recorder, PAGLayer* childLayer);
  static bool GetTrackMatteLayerAtPoint(PAGLayer* childLayer, float x, float y,
                                        std::vector<std::shared_ptr<PAGLayer>>* results);
  static bool GetChildLayerAtPoint(PAGLayer* childLayer, float x, float y,
                                   std::vector<std::shared_ptr<PAGLayer>>* results);

  bool getLayersUnderPointInternal(float x, float y,
                                   std::vector<std::shared_ptr<PAGLayer>>* results);
  int getLayerIndexInternal(std::shared_ptr<PAGLayer> child) const;
  void doSwapLayerAt(int index1, int index2);
  void doSetLayerIndex(std::shared_ptr<PAGLayer> pagLayer, int index);
  bool doContains(PAGLayer* layer) const;
  void updateDurationAndFrameRate();

  friend class PAGLayer;

  friend class PAGFile;

  friend class RenderCache;

  friend class PAGStage;

  friend class PAGPlayer;

  friend class PAGImageLayer;

  friend class FileReporter;

  friend class AudioClip;
};

class PAG_API PAGFile : public PAGComposition {
 public:
  /**
   * The maximum tag level current SDK supports.
   */
  static uint16_t MaxSupportedTagLevel();
  /**
   *  Load a pag file from byte data, return null if the bytes is empty or it's not a valid pag
   * file.
   */
  static std::shared_ptr<PAGFile> Load(const void* bytes, size_t length,
                                       const std::string& filePath = "");
  /**
   *  Load a pag file from path, return null if the file does not exist or the data is not a pag
   * file.
   */
  static std::shared_ptr<PAGFile> Load(const std::string& filePath);

  PAGFile(std::shared_ptr<File> file, PreComposeLayer* layer);

  /**
   * The tag level this pag file requires.
   */
  uint16_t tagLevel();

  /**
   * The number of replaceable texts.
   */
  int numTexts();

  /**
   * The number of replaceable images.
   */
  int numImages();

  /**
   * The number of video compositions.
   */
  int numVideos();

  /**
   * The path string of this file, returns empty string if the file is loaded from byte stream.
   */
  std::string path() const;

  /**
   * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
   * Note: It always returns the default text data.
   */
  std::shared_ptr<TextDocument> getTextData(int editableTextIndex);

  /**
   * Replace the text data of the specified index. The index ranges from 0 to PAGFile.numTexts - 1.
   * Passing in null for the textData parameter will reset it to default text data.
   */
  void replaceText(int editableTextIndex, std::shared_ptr<TextDocument> textData);

  /**
   * Replace file's image content of the specified index with a PAGImage object. The index ranges from
   * 0 to PAGFile.numImages - 1. Passing in null for the image parameter will reset it to default
   * image content.
   */
  void replaceImage(int editableImageIndex, std::shared_ptr<PAGImage> image);

  /**
   * Return an array of layers by specified editable index and layer type.
   */
  std::vector<std::shared_ptr<PAGLayer>> getLayersByEditableIndex(int editableIndex,
                                                                  LayerType layerType);

  /**
   * Returns the indices of the editable layers in this PAGFile.
   * If the editableIndex of a PAGLayer is not present in the returned indices, the PAGLayer should
   * not be treated as editable.
   */
  std::vector<int> getEditableIndices(LayerType layerType);

  /**
   * Indicate how to stretch the original duration to fit target duration when file's duration is
   * changed. The default value is PAGTimeStretchMode::Repeat.
   */
  Enum timeStretchMode() const;

  /**
   * Set the timeStretchMode of this file.
   */
  void setTimeStretchMode(Enum value);

  /**
   * Set the duration of this PAGFile. Passing a value less than or equal to 0 resets the duration
   * to its default value.
   */
  void setDuration(int64_t duration);

  /**
   * Make a copy of the original file, any modification to current file has no effect on the result
   * file.
   */
  std::shared_ptr<PAGFile> copyOriginal();

  bool isPAGFile() const override;

 protected:
  bool gotoTime(int64_t layerTime) override;
  Frame childFrameToLocal(Frame childFrame, float childFrameRate) const override;
  Frame localFrameToChild(Frame localFrame, float childFrameRate) const override;
  std::vector<std::shared_ptr<PAGLayer>> getLayersByEditableIndexInternal(int editableIndex,
                                                                          LayerType layerType);
  Frame stretchedFrameDuration() const override;
  Frame stretchedContentFrame() const override;

 private:
  static std::shared_ptr<PAGFile> MakeFrom(std::shared_ptr<File> file);
  static std::shared_ptr<PAGLayer> BuildPAGLayer(std::shared_ptr<File> file, pag::Layer* layer);

  void setDurationInternal(int64_t duration);
  Frame stretchedFrameToFileFrame(Frame stretchedFrame) const;
  int64_t stretchedTimeToFileTime(int64_t stretchedTime) const;
  Frame scaledFrameToFileFrame(Frame scaledFrame, const TimeRange& scaledTimeRange) const;
  Frame fileFrameToStretchedFrame(Frame fileFrame) const;
  Frame fileFrameToScaledFrame(Frame fileFrame, const TimeRange& scaledTimeRange) const;
  void replaceImageInternal(int editableImageIndex, std::shared_ptr<PAGImage> image);

  Frame _stretchedContentFrame = 0;
  Frame _stretchedFrameDuration = 1;
  Enum _timeStretchMode = PAGTimeStretchMode::Repeat;

  friend class PAGImageLayer;

  friend class LayerRenderer;

  friend class AudioClip;
};

class Composition;

class PAGPlayer;

class GLRestorer;

class PAG_API PAGSurface {
 public:
  /**
   *  Creates a new PAGSurface from specified GPUDrawable, Returns nullptr if drawable is nullptr.
   */
  static std::shared_ptr<PAGSurface> MakeFrom(std::shared_ptr<Drawable> drawable);

  /**
   * Creates a new PAGSurface from specified backend render target and origin. The PAGSurface uses
   * the GPU context on the calling thread directly. Returns null if the render target is invalid.
   */
  static std::shared_ptr<PAGSurface> MakeFrom(const BackendRenderTarget& renderTarget,
                                              ImageOrigin origin);

  /**
   * Creates a new PAGSurface from specified backend texture and origin. Note, the texture must not
   * be bound to any frame buffer. Passes true in the forAsyncThread parameter if the returned
   * PAGSurface will be used for asynchronous rendering. If passing true, the PAGSurface internally
   * creates a independent GPU context, and the caller can use semaphore objects to synchronise
   * content (see flushAndSignalSemaphore() and wait()), otherwise, it uses the GPU context on the
   * calling thread directly. Returns null if the texture is invalid.
   */
  static std::shared_ptr<PAGSurface> MakeFrom(const BackendTexture& texture, ImageOrigin origin,
                                              bool forAsyncThread = false);

  /**
   *  Creates a new PAGSurface for off-screen rendering. Allocates memory for pixels, based on the
   *  width and height, which can be accessed by readPixels(). Returns null if the specified size
   *  is not valid.
   */
  static std::shared_ptr<PAGSurface> MakeOffscreen(int width, int height);

  /**
   * Returns the width in pixels of the surface.
   */
  int width();

  /**
   * Returns the height in pixels of the surface.
   */
  int height();

  /**
   * Update the size of surface, and reset the internal surface.
   */
  void updateSize();

  /**
   * Erases all pixels of this surface with transparent color. Returns true if the content has
   * changed.
   */
  bool clearAll();

  /**
   * Free the cache created by the surface immediately. Can be called to reduce memory pressure.
   */
  void freeCache();

  /**
   * Copies pixels from current PAGSurface to dstPixels with specified color type, alpha type and
   * row bytes. Returns true if pixels are copied to dstPixels.
   */
  bool readPixels(ColorType colorType, AlphaType alphaType, void* dstPixels, size_t dstRowBytes);

 private:
  uint32_t contentVersion = 0;
  PAGPlayer* pagPlayer = nullptr;
  std::shared_ptr<std::mutex> rootLocker = nullptr;
  std::shared_ptr<Drawable> drawable = nullptr;
  std::shared_ptr<tgfx::Surface> surface = nullptr;
  bool contextAdopted = false;
  GLRestorer* glRestorer = nullptr;

  explicit PAGSurface(std::shared_ptr<Drawable> drawable, bool contextAdopted = false);

  bool draw(RenderCache* cache, std::shared_ptr<Graphic> graphic, BackendSemaphore* signalSemaphore,
            bool autoClear = true);
  bool hitTest(RenderCache* cache, std::shared_ptr<Graphic> graphic, float x, float y);
  tgfx::Context* lockContext();
  void unlockContext();
  bool wait(const BackendSemaphore& waitSemaphore);
  void freeCacheInternal();

  friend class PAGPlayer;

  friend class FileReporter;
};

class FileReporter;

class PAG_API PAGPlayer {
 public:
  PAGPlayer();

  virtual ~PAGPlayer();

  /**
   * Returns the current PAGComposition for PAGPlayer to render as content.
   */
  std::shared_ptr<PAGComposition> getComposition();

  /**
   * Sets a new PAGComposition for PAGPlayer to render as content.
   * Note: If the composition is already added to another PAGPlayer, it will be removed from the
   * previous PAGPlayer.
   */
  void setComposition(std::shared_ptr<PAGComposition> newComposition);

  /**
   * Returns the PAGSurface object for PAGPlayer to render onto.
   */
  std::shared_ptr<PAGSurface> getSurface();

  /**
   * Set the PAGSurface object for PAGPlayer to render onto.
   */
  void setSurface(std::shared_ptr<PAGSurface> newSurface);

  /**
   * If set to false, PAGPlayer skips rendering for video composition.
   */
  bool videoEnabled();

  /**
   * Set the value of videoEnabled property.
   */
  void setVideoEnabled(bool value);

  /**
   * If set to true, PAGPlayer caches an internal bitmap representation of the static content for
   * each layer. This caching can increase performance for layers that contain complex vector
   * content. The execution speed can be significantly faster depending on the complexity of the
   * content, but it requires extra graphics memory. The default value is true.
   */
  bool cacheEnabled();

  /**
   * Set the value of cacheEnabled property.
   */
  void setCacheEnabled(bool value);

  /**
   * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
   * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of
   * graphics memory which leads to better performance. The default value is 1.0.
   */
  float cacheScale();

  /**
   * Set the value of cacheScale property.
   */
  void setCacheScale(float value);

  /**
   * The maximum frame rate for rendering, ranges from 1 to 60. If set to a value less than the
   * actual frame rate from composition, it drops frames but increases performance. Otherwise, it
   * has no effect. The default value is 60.
   */
  float maxFrameRate();

  /**
   * Sets the maximum frame rate for rendering.
   */
  void setMaxFrameRate(float value);

  /**
   * Returns the current scale mode.
   */
  int scaleMode();

  /**
   * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
   * changes when this method is called.
   */
  void setScaleMode(int mode);

  /**
   * Returns a copy of current matrix.
   */
  Matrix matrix();

  /**
   * Sets the transformation which will be applied to the composition. The scaleMode property
   * will be set to PAGScaleMode::None when this method is called.
   */
  void setMatrix(const Matrix& matrix);

  /**
   * The duration of composition in microseconds.
   */
  int64_t duration();

  /**
   * Set the progress of play position to next frame. It is applied only when the composition is not
   * null.
   */
  void nextFrame();

  /**
   * Set the progress of play position to previous frame. It is applied only when the composition is
   * not null.
   */
  void preFrame();

  /**
   * Returns the current progress of play position, the value is from 0.0 to 1.0.
   */
  double getProgress();

  /**
   * Sets the progress of play position, the value ranges from 0.0 to 1.0. It is applied only when
   * the composition is not null.
   */
  void setProgress(double percent);

  /**
   * If true, PAGPlayer clears the whole content of PAGSurface before drawing anything new to it.
   * The default value is true.
   */
  bool autoClear();

  /**
   * Sets the autoClear property.
   */
  void setAutoClear(bool value);

  /**
   * Prepares the player for the next flush() call. It collects all CPU tasks from the current
   * progress of the composition and runs them asynchronously in parallel. It is usually used for
   * speeding up the first frame rendering.
   */
  void prepare();

  /**
   * Inserts a GPU semaphore that the current GPU-backed API must wait on before executing any more
   * commands on the GPU for this player. It is usually called before PAGPlayer.flush(). PAG will
   * take ownership of the underlying semaphore and delete it once it has been signaled and waited
   * on. If this call returns false, then the GPU back-end will not wait on the passed semaphore,
   * and the client will still own the semaphore. Returns true if GPU is waiting on the semaphore.
   * It is used when we need to draw a PAGImage which is made from a BackendTexture.
   *
   * @param waitSemaphore semaphore container
   * @returns true if GPU is waiting on semaphore
   */
  bool wait(const BackendSemaphore& waitSemaphore);

  /**
   * Apply all pending changes to the target surface immediately. Returns true if the content has
   * changed.
   */
  bool flush();

  /**
   * Apply all pending changes to the target surface immediately. Returns true if the content has
   * changed. After issuing all commands, the signalSemaphore will be signaled by the GPU.
   *
   * If the signalSemaphore is not null and uninitialized, a new semaphore is created and
   * initializes BackendSemaphore.
   *
   * The caller must delete the semaphore returned in signalSemaphore.
   * BackendSemaphore can be deleted as soon as this function returns.
   *
   * If the back-end API is OpenGL only uninitialized backend semaphores are supported.
   *
   * If signalSemaphore is uninitialized, the GPU back-end did not create or add a semaphore to
   * signal on the GPU; the caller should not instruct the GPU to wait on the semaphore.
   *
   * It is used when the PAGSurface is made from a BackendRenderTarget or BackendTexture.
   *
   * @param signalSemaphore semaphore container
   */
  bool flushAndSignalSemaphore(BackendSemaphore* signalSemaphore);

  /**
   * Returns a rectangle that defines the displaying area of the specified layer, which is in the
   * coordinate of the PAGSurface.
   */
  Rect getBounds(std::shared_ptr<PAGLayer> pagLayer);

  /**
   * Returns an array of layers that lie under the specified point. The point is in the coordinate
   * space of the PAGSurface.
   */
  std::vector<std::shared_ptr<PAGLayer>> getLayersUnderPoint(float surfaceX, float surfaceY);

  /**
   * Evaluates the PAGLayer to see if it overlaps or intersects with the specified point. The point
   * is in the coordinate space of the PAGSurface, not the PAGComposition that contains the
   * PAGLayer. It always returns false if the PAGLayer or its parent (or parent's parent...) has not
   * been added to this PAGPlayer. The pixelHitTest parameter indicates whether or not to check
   * against the actual pixels of the object (true) or the bounding box (false). Returns true if the
   * PAGLayer overlaps or intersects with the specified point.
   */
  bool hitTestPoint(std::shared_ptr<PAGLayer> pagLayer, float surfaceX, float surfaceY,
                    bool pixelHitTest = false);

  /**
   * The time cost by rendering in microseconds.
   */
  int64_t renderingTime();

  /**
   * The time cost by image decoding in microseconds.
   */
  int64_t imageDecodingTime();

  /**
   * The time cost by presenting in microseconds.
   */
  int64_t presentingTime();

  /**
   * The memory cost by graphics in bytes.
   */
  int64_t graphicsMemory();

 protected:
  std::shared_ptr<std::mutex> rootLocker = nullptr;
  std::shared_ptr<PAGStage> stage = nullptr;
  RenderCache* renderCache = nullptr;
  std::shared_ptr<PAGSurface> pagSurface = nullptr;
  uint32_t contentVersion = 0;
  std::shared_ptr<Graphic> lastGraphic = nullptr;

  virtual void updateScaleModeIfNeed();
  virtual bool flushInternal(BackendSemaphore* signalSemaphore);

 private:
  FileReporter* reporter = nullptr;
  float _maxFrameRate = 60;
  int _scaleMode = PAGScaleMode::LetterBox;
  bool _autoClear = true;

  void updateStageSize();
  void setSurfaceInternal(std::shared_ptr<PAGSurface> newSurface);
  int64_t getTimeStampInternal();
  void prepareInternal();
  int64_t durationInternal();

  friend class PAGSurface;
};

/**
 * Defines methods to control video decoding capabilities of PAG.
 */
class PAG_API PAGVideoDecoder {
 public:
  /**
   * Set the maximum number of hardware video decoders that PAG can create.
   */
  static void SetMaxHardwareDecoderCount(int count);

  /**
   * Register a software decoder factory to PAG, which can be used to create video decoders for
   * decoding video sequences from a pag file, if hardware decoders are not available.
   */
  static void RegisterSoftwareDecoderFactory(SoftwareDecoderFactory* decoderFactory);
};

class PAG_API PAG {
 public:
  /**
   * Get SDK version information.
   */
  static std::string SDKVersion();
};

}  // namespace pag
