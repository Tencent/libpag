/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <atomic>
#include <mutex>
#include "pag/types.h"

#ifdef PAG_USE_RTTR
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "rttr/array_range.h"
#include "rttr/constructor.h"
#include "rttr/destructor.h"
#include "rttr/enum_flags.h"
#include "rttr/enumeration.h"
#include "rttr/library.h"
#include "rttr/method.h"
#include "rttr/property.h"
#include "rttr/rttr_cast.h"
#include "rttr/rttr_enable.h"
#include "rttr/type.h"
#pragma clang diagnostic pop
#else

#define RTTR_ENABLE(...)

#endif

namespace pag {
/**
 * The maximum number of type is 1023. It only allows to add new tag to the end before
 * TagCode::Count.
 */
enum class RTTR_AUTO_REGISTER_CLASS TagCode {
  End = 0,
  FontTables = 1,
  VectorCompositionBlock = 2,
  CompositionAttributes = 3,
  ImageTables = 4,
  LayerBlock = 5,
  LayerAttributes = 6,
  SolidColor = 7,
  TextSource = 8,
  TextMoreOption = 10,
  ImageReference = 11,
  CompositionReference = 12,
  Transform2D = 13,
  MaskBlock = 14,
  ShapeGroup = 15,
  Rectangle = 16,
  Ellipse = 17,
  PolyStar = 18,
  ShapePath = 19,
  Fill = 20,
  Stroke = 21,
  GradientFill = 22,
  GradientStroke = 23,
  MergePaths = 24,
  TrimPaths = 25,
  Repeater = 26,
  RoundCorners = 27,
  Performance = 28,
  DropShadowStyle = 29,
  CachePolicy = 30,
  FileAttributes = 31,
  TimeStretchMode = 32,
  Mp4Header = 33,

  // 34 ~ 44 are preserved

  BitmapCompositionBlock = 45,
  BitmapSequence = 46,
  ImageBytes = 47,
  ImageBytesV2 = 48,  // with scaleFactor
  ImageBytesV3 = 49,  // width transparent border stripped.

  VideoCompositionBlock = 50,
  VideoSequence = 51,

  LayerAttributesV2 = 52,
  MarkerList = 53,
  ImageFillRule = 54,
  AudioBytes = 55,
  MotionTileEffect = 56,
  LevelsIndividualEffect = 57,
  CornerPinEffect = 58,
  BulgeEffect = 59,
  FastBlurEffect = 60,
  GlowEffect = 61,

  LayerAttributesV3 = 62,
  LayerAttributesExtra = 63,

  TextSourceV2 = 64,

  DropShadowStyleV2 = 65,
  DisplacementMapEffect = 66,
  ImageFillRuleV2 = 67,

  TextSourceV3 = 68,
  TextPathOption = 69,

  TextAnimator = 70,
  TextRangeSelector = 71,
  TextAnimatorPropertiesTrackingType = 72,
  TextAnimatorPropertiesTrackingAmount = 73,
  TextAnimatorPropertiesFillColor = 74,
  TextAnimatorPropertiesStrokeColor = 75,
  TextAnimatorPropertiesPosition = 76,
  TextAnimatorPropertiesScale = 77,
  TextAnimatorPropertiesRotation = 78,
  TextAnimatorPropertiesOpacity = 79,
  TextWigglySelector = 80,

  RadialBlurEffect = 81,
  MosaicEffect = 82,
  EditableIndices = 83,
  MaskBlockV2 = 84,
  GradientOverlayStyle = 85,
  BrightnessContrastEffect = 86,
  HueSaturationEffect = 87,

  LayerAttributesExtraV2 = 88,  // add PreserveAlpha
  EncryptedData = 89,
  Transform3D = 90,
  CameraOption = 91,
  
  StrokeStyle = 92,
  OuterGlowStyle = 93,
  ImageScaleModes = 94,

  // add new tags here...

  Count
};

struct RTTR_AUTO_REGISTER_CLASS Ratio {
  float value() const {
    return static_cast<float>(numerator) / denominator;
  }

  int32_t numerator;
  uint32_t denominator;
};

inline bool operator==(const Ratio& left, const Ratio& right) {
  return left.numerator == right.numerator &&
         (left.numerator == 0 || left.denominator == right.denominator);
}

inline bool operator!=(const Ratio& left, const Ratio& right) {
  return !(left == right);
}

static constexpr Ratio DefaultRatio = {1, 1};

enum class RTTR_AUTO_REGISTER_CLASS PathDataVerb { MoveTo, LineTo, CurveTo, Close };

/**
 * The Path object encapsulates information describing a path in a shape layer, or the outline path
 * of a Mask.
 */
class PAG_API PathData {
 public:
  std::vector<PathDataVerb> verbs;
  std::vector<Point> points;
  void moveTo(float x, float y);
  void lineTo(float x, float y);
  void cubicTo(float controlX1, float controlY1, float controlX2, float controlY2, float x,
               float y);
  /**
   * If the last point does not equal with the last moveTo point, add a lineTo verb and then close
   * the path.
   */
  void close();
  bool isClosed() const;
  void reverse();
  void clear();
  void interpolate(const PathData& path, PathData* result, float t);

 private:
  Point lastMoveTo = {};
};

typedef std::shared_ptr<PathData> PathHandle;

enum class PAG_API KeyframeInterpolationType : uint8_t {
  None = 0,
  Linear = 1,
  Bezier = 2,
  Hold = 3
};

template <typename T>
class RTTR_AUTO_REGISTER_CLASS Keyframe {
 public:
  virtual ~Keyframe() = default;

  virtual void initialize() {
  }

  virtual T getValueAt(Frame) {
    return startValue;
  }

  bool containsTime(Frame time) const {
    return time >= startTime && time < endTime;
  }

  T startValue;
  T endValue;
  Frame startTime = ZeroFrame;
  Frame endTime = ZeroFrame;
  KeyframeInterpolationType interpolationType = KeyframeInterpolationType::Hold;
  std::vector<Point> bezierOut;
  std::vector<Point> bezierIn;
  Point3D spatialOut = Point3D::Zero();
  Point3D spatialIn = Point3D::Zero();
};

/**
 * The Property object contains value information about a particular AE property of a layer.
 */
template <typename T>
class RTTR_AUTO_REGISTER_CLASS Property {
 public:
  T value;
  Property() = default;

  explicit Property(const T& value) : value(value) {
  }

  virtual ~Property() = default;

  virtual bool animatable() const {
    return false;
  }

  virtual T getValueAt(Frame) {
    return value;
  }

  virtual void excludeVaryingRanges(std::vector<TimeRange>*) const {
  }

  RTTR_ENABLE()
};

Frame PAG_API ConvertFrameByStaticTimeRanges(const std::vector<TimeRange>& timeRanges, Frame frame);
void PAG_API SubtractFromTimeRanges(std::vector<TimeRange>* timeRanges, Frame startTime,
                                    Frame endTime);
void PAG_API SplitTimeRangesAt(std::vector<TimeRange>* timeRanges, Frame startTime);
void PAG_API MergeTimeRanges(std::vector<TimeRange>* timeRanges,
                             const std::vector<TimeRange>* otherRanges);
std::vector<TimeRange> PAG_API OffsetTimeRanges(const std::vector<TimeRange>& timeRanges,
                                                Frame offsetTime);
bool PAG_API HasVaryingTimeRange(const std::vector<TimeRange>* staticTimeRanges, Frame startTime,
                                 Frame duration);
TimeRange PAG_API GetTimeRangeContains(const std::vector<TimeRange>& timeRanges, Frame frame);

template <typename T>
class RTTR_AUTO_REGISTER_CLASS AnimatableProperty : public Property<T> {
 public:
  explicit AnimatableProperty(const std::vector<Keyframe<T>*>& keyframes)
      : keyframes(keyframes), lastKeyframeIndex(0) {
    this->value = keyframes[0]->startValue;
    for (Keyframe<T>* keyframe : keyframes) {
      keyframe->initialize();
    }
  }

  ~AnimatableProperty() override {
    for (auto& keyframe : keyframes) {
      delete keyframe;
    }
  }

  bool animatable() const override {
    return true;
  }

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override {
    for (Keyframe<T>* keyframe : keyframes) {
      switch (keyframe->interpolationType) {
        case KeyframeInterpolationType::Bezier:
        case KeyframeInterpolationType::Linear:
          SubtractFromTimeRanges(timeRanges, keyframe->startTime, keyframe->endTime - 1);
          break;
        default:
          SplitTimeRangesAt(timeRanges, keyframe->startTime);
          SplitTimeRangesAt(timeRanges, keyframe->endTime);
          break;
      }
    }
  }

  T getValueAt(Frame frame) override {
    T result;
    size_t lastKeyframeIndexInternal = lastKeyframeIndex;
    Keyframe<T>* lastKeyframe = keyframes[lastKeyframeIndexInternal];
    if (lastKeyframe->containsTime(frame)) {
      return lastKeyframe->getValueAt(frame);
    }
    if (frame < lastKeyframe->startTime) {
      while (lastKeyframeIndexInternal > 0) {
        lastKeyframeIndexInternal--;
        if (keyframes[lastKeyframeIndexInternal]->containsTime(frame)) {
          break;
        }
      }
    } else {
      while (lastKeyframeIndexInternal < keyframes.size() - 1) {
        lastKeyframeIndexInternal++;
        if (keyframes[lastKeyframeIndexInternal]->containsTime(frame)) {
          break;
        }
      }
    }
    lastKeyframe = keyframes[lastKeyframeIndexInternal];
    if (frame <= lastKeyframe->startTime) {
      result = lastKeyframe->startValue;
    } else if (frame >= lastKeyframe->endTime) {
      result = lastKeyframe->endValue;
    } else {
      result = lastKeyframe->getValueAt(frame);
    }
    lastKeyframeIndex = lastKeyframeIndexInternal;
    return result;
  }

  /**
   * The keyframe list in this property.
   */
  std::vector<Keyframe<T>*> keyframes;

 private:
  std::atomic_size_t lastKeyframeIndex;

  RTTR_ENABLE(Property<T>)
};

class PAG_API Transform2D {
 public:
  static std::unique_ptr<Transform2D> MakeDefault();
  ~Transform2D();

  Property<Point>* anchorPoint = nullptr;  // spatial
  Property<Point>* position = nullptr;     // spatial
  Property<float>* xPosition = nullptr;
  Property<float>* yPosition = nullptr;
  Property<Point>* scale = nullptr;  // multidimensional
  Property<float>* rotation = nullptr;
  Property<Opacity>* opacity = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

class PAG_API Transform3D {
 public:
  ~Transform3D();

  Property<Point3D>* anchorPoint = nullptr;  // spatial
  Property<Point3D>* position = nullptr;     // spatial
  Property<float>* xPosition = nullptr;
  Property<float>* yPosition = nullptr;
  Property<float>* zPosition = nullptr;
  Property<Point3D>* scale = nullptr;  // multidimensional
  Property<Point3D>* orientation = nullptr;
  Property<float>* xRotation = nullptr;
  Property<float>* yRotation = nullptr;
  Property<float>* zRotation = nullptr;
  Property<Opacity>* opacity = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

enum class PAG_API MaskMode : uint8_t {
  None = 0,  // mask shape does nothing
  Add = 1,   // shape is added into mask (normal behavior (really screen!))
  Subtract = 2,
  Intersect = 3,
  Lighten = 4,
  Darken = 5,
  Difference = 6,
  Accum = 7   // real add, not screen (not exposed in UI!)
};

class PAG_API MaskData {
 public:
  ~MaskData();

  ID id = ZeroID;
  bool inverted = false;
  MaskMode maskMode = MaskMode::Add;
  Property<PathHandle>* maskPath = nullptr;
  Property<Point>* maskFeather = nullptr;
  Property<Opacity>* maskOpacity = nullptr;
  Property<float>* maskExpansion = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

enum class RTTR_AUTO_REGISTER_CLASS EffectType {
  Unknown,
  Fill,
  MotionTile,
  LevelsIndividual,
  CornerPin,
  Bulge,
  FastBlur,
  Glow,
  DisplacementMap,
  RadialBlur,
  Mosaic,
  BrightnessContrast,
  HueSaturation,
};

class PAG_API Effect {
 public:
  Effect();

  virtual ~Effect();

  /**
   * Returns a globally unique id for this object.
   */
  ID uniqueID = 0;

  virtual EffectType type() const {
    return EffectType::Unknown;
  }

  /**
   * Indicate whether this effect process only the visible area of the input texture.
   */
  virtual bool processVisibleAreaOnly() const {
    return false;
  }

  /**
   * Check whether or not this effect is visible at the specified frame.
   */
  virtual bool visibleAt(Frame) const {
    return false;
  }

  /**
   * Calculate the bounds after this effect being applied.
   */
  virtual void transformBounds(Rect*, const Point&, Frame) const {
  }

  virtual Point getMaxScaleFactor(const Rect&) const {
    return Point::Make(1.f, 1.f);
  }

  virtual void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  virtual bool verify() const;

  Property<Opacity>* effectOpacity = nullptr;
  std::vector<MaskData*> maskReferences;  // mask reference

  RTTR_ENABLE()
};

class PAG_API MotionTileEffect : public Effect {
 public:
  ~MotionTileEffect() override;

  EffectType type() const override {
    return EffectType::MotionTile;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<Point>* tileCenter = nullptr;  // spatial
  Property<float>* tileWidth = nullptr;
  Property<float>* tileHeight = nullptr;
  Property<float>* outputWidth = nullptr;
  Property<float>* outputHeight = nullptr;
  Property<bool>* mirrorEdges = nullptr;
  Property<float>* phase = nullptr;
  Property<bool>* horizontalPhaseShift = nullptr;

  RTTR_ENABLE(Effect)
};

class PAG_API LevelsIndividualEffect : public Effect {
 public:
  ~LevelsIndividualEffect() override;

  EffectType type() const override {
    return EffectType::LevelsIndividual;
  }

  bool processVisibleAreaOnly() const override {
    return true;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  // RGB
  Property<float>* inputBlack = nullptr;
  Property<float>* inputWhite = nullptr;
  Property<float>* gamma = nullptr;
  Property<float>* outputBlack = nullptr;
  Property<float>* outputWhite = nullptr;

  // Red
  Property<float>* redInputBlack = nullptr;
  Property<float>* redInputWhite = nullptr;
  Property<float>* redGamma = nullptr;
  Property<float>* redOutputBlack = nullptr;
  Property<float>* redOutputWhite = nullptr;

  // green
  Property<float>* greenInputBlack = nullptr;
  Property<float>* greenInputWhite = nullptr;
  Property<float>* greenGamma = nullptr;
  Property<float>* greenOutputBlack = nullptr;
  Property<float>* greenOutputWhite = nullptr;

  // blue
  Property<float>* blueInputBlack = nullptr;
  Property<float>* blueInputWhite = nullptr;
  Property<float>* blueGamma = nullptr;
  Property<float>* blueOutputBlack = nullptr;
  Property<float>* blueOutputWhite = nullptr;

  RTTR_ENABLE(Effect)
};

class PAG_API CornerPinEffect : public Effect {
 public:
  ~CornerPinEffect() override;

  EffectType type() const override {
    return EffectType::CornerPin;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  Point getMaxScaleFactor(const Rect& bounds) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<Point>* upperLeft = nullptr;   // spatial
  Property<Point>* upperRight = nullptr;  // spatial
  Property<Point>* lowerLeft = nullptr;   // spatial
  Property<Point>* lowerRight = nullptr;  // spatial

  RTTR_ENABLE(Effect)
};

class PAG_API BulgeEffect : public Effect {
 public:
  ~BulgeEffect() override;

  EffectType type() const override {
    return EffectType::Bulge;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<float>* horizontalRadius = nullptr;
  Property<float>* verticalRadius = nullptr;
  Property<Point>* bulgeCenter = nullptr;  // spatial
  Property<float>* bulgeHeight = nullptr;
  Property<float>* taperRadius = nullptr;
  Property<bool>* pinning = nullptr;

  RTTR_ENABLE(Effect)
};

enum class PAG_API BlurDimensionsDirection : uint8_t {
  All = 0,
  Horizontal = 1,
  Vertical = 2
};

class PAG_API FastBlurEffect : public Effect {
 public:
  ~FastBlurEffect() override;

  EffectType type() const override {
    return EffectType::FastBlur;
  }

  bool processVisibleAreaOnly() const override {
    return true;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<float>* blurriness = nullptr;
  Property<BlurDimensionsDirection>* blurDimensions = nullptr;
  Property<bool>* repeatEdgePixels = nullptr;

  RTTR_ENABLE(Effect)
};

class PAG_API GlowEffect : public Effect {
 public:
  ~GlowEffect() override;

  EffectType type() const override {
    return EffectType::Glow;
  }

  bool processVisibleAreaOnly() const override {
    return true;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<Percent>* glowThreshold = nullptr;
  Property<float>* glowRadius = nullptr;
  Property<float>* glowIntensity = nullptr;

  RTTR_ENABLE(Effect)
};

enum class PAG_API DisplacementMapSource : uint8_t {
  Red = 0,
  Green = 1,
  Blue = 2,
  Alpha = 3,
  Luminance = 4,
  Hue = 5,
  Lightness = 6,
  Saturation = 7,
  Full = 8,
  Half = 9,
  Off = 10
};

enum class RTTR_AUTO_REGISTER_CLASS DisplacementMapBehavior : uint8_t {
  CenterMap = 0,
  StretchMapToFit = 1,
  TileMap = 2
};

class Layer;

class PAG_API DisplacementMapEffect : public Effect {
 public:
  ~DisplacementMapEffect() override;

  EffectType type() const override {
    return EffectType::DisplacementMap;
  }

  bool processVisibleAreaOnly() const override {
    return true;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Layer* RTTR_SKIP_REGISTER_PROPERTY displacementMapLayer = nullptr;                   // ref layer
  Property<DisplacementMapSource>* useForHorizontalDisplacement = nullptr;
  Property<float>* maxHorizontalDisplacement = nullptr;
  Property<DisplacementMapSource>* useForVerticalDisplacement = nullptr;
  Property<float>* maxVerticalDisplacement = nullptr;
  Property<DisplacementMapBehavior>* displacementMapBehavior = nullptr;
  Property<bool>* edgeBehavior = nullptr;
  Property<bool>* expandOutput = nullptr;

  RTTR_ENABLE(Effect)
};

enum class PAG_API RadialBlurMode : uint8_t {
  Spin = 0,
  Zoom = 1
};

enum class PAG_API RadialBlurAntialias : uint8_t {
  Low = 0,
  High = 1
};

class PAG_API RadialBlurEffect : public Effect {
 public:
  ~RadialBlurEffect() override;

  EffectType type() const override {
    return EffectType::RadialBlur;
  }

  bool processVisibleAreaOnly() const override {
    return true;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<float>* amount = nullptr;
  Property<Point>* center = nullptr;  // spatial
  Property<RadialBlurMode>* mode = nullptr;
  Property<RadialBlurAntialias>* antialias = nullptr;

  RTTR_ENABLE(Effect)
};

class PAG_API MosaicEffect : public Effect {
 public:
  ~MosaicEffect() override;

  EffectType type() const override {
    return EffectType::Mosaic;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<uint16_t>* horizontalBlocks = nullptr;
  Property<uint16_t>* verticalBlocks = nullptr;  // spatial
  Property<bool>* sharpColors = nullptr;

  RTTR_ENABLE(Effect)
};

class PAG_API BrightnessContrastEffect : public Effect {
 public:
  ~BrightnessContrastEffect() override;

  EffectType type() const override {
    return EffectType::BrightnessContrast;
  }

  bool processVisibleAreaOnly() const override {
    return true;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<float>* brightness = nullptr;
  Property<float>* contrast = nullptr;
  Property<bool>* useOldVersion = nullptr;

  RTTR_ENABLE(Effect)
};

enum class PAG_API ChannelControlType : uint8_t {
  Master = 0,
  Reds = 1,
  Yellows = 2,
  Greens = 3,
  Cyans = 4,
  Blues = 5,
  Magentas = 6,
  Count = 7
};

class PAG_API HueSaturationEffect : public Effect {
 public:
  ~HueSaturationEffect() override;

  EffectType type() const override {
    return EffectType::HueSaturation;
  }

  bool processVisibleAreaOnly() const override {
    return true;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  ChannelControlType channelControl = ChannelControlType::Master;
  std::vector<float> hue = std::vector<float>(static_cast<size_t>(ChannelControlType::Count), 0.0f);
  std::vector<float> saturation = std::vector<float>(static_cast<size_t>(ChannelControlType::Count), 0.0f);
  std::vector<float> lightness = std::vector<float>(static_cast<size_t>(ChannelControlType::Count), 0.0f);
  bool colorize = false;
  Property<float>* colorizeHue = nullptr;
  Property<float>* colorizeSaturation = nullptr;
  Property<float>* colorizeLightness = nullptr;

  RTTR_ENABLE(Effect)
};

enum class RTTR_AUTO_REGISTER_CLASS LayerStyleType { Unknown, DropShadow, Stroke, GradientOverlay, OuterGlow };

enum class RTTR_AUTO_REGISTER_CLASS LayerStylePosition { Above, Blow };

class PAG_API LayerStyle {
 public:
  LayerStyle();

  virtual ~LayerStyle() = default;

  /**
   * Returns a globally unique id for this object.
   */
  ID uniqueID = 0;

  virtual LayerStyleType type() const {
    return LayerStyleType::Unknown;
  }

  virtual LayerStylePosition drawPosition() const {
    return LayerStylePosition::Blow;
  }

  /**
   * Check whether or not this layer style is visible at the specified frame.
   */
  virtual bool visibleAt(Frame) const {
    return false;
  }

  /**
   * Calculate the bounds after this layerStyle being applied.
   */
  virtual void transformBounds(Rect*, const Point&, Frame) const {
  }

  virtual void excludeVaryingRanges(std::vector<TimeRange>*) const {
  }

  virtual bool verify() const {
    return true;
  }

  RTTR_ENABLE()
};

enum class PAG_API StrokePosition : uint8_t {
  Outside = 0,
  Inside = 1,
  Center = 2,
  Invalid = 255
};

enum class PAG_API GlowColorType : uint8_t {
  SingleColor = 0,
  Gradient = 1
};

enum class PAG_API GlowTechniqueType : uint8_t {
  Softer = 0,
  Precise = 1
};

enum class PAG_API LineCap : uint8_t {
  Butt = 0,
  Round = 1,
  Square = 2
};

enum class PAG_API LineJoin : uint8_t {
  Miter = 0,
  Round = 1,
  Bevel = 2
};

enum class PAG_API FillRule : uint8_t {
  NonZeroWinding = 0,
  EvenOdd = 1
};

enum class PAG_API GradientFillType : uint8_t {
  Linear = 0,
  Radial = 1,
  Angle = 2,
  Reflected = 3,
  Diamond = 4  // not supported yet
};

class PAG_API DropShadowStyle : public LayerStyle {
 public:
  ~DropShadowStyle() override;

  LayerStyleType type() const override {
    return LayerStyleType::DropShadow;
  }

  LayerStylePosition drawPosition() const override {
    return LayerStylePosition::Blow;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<BlendMode>* blendMode = nullptr;
  Property<Color>* color = nullptr;
  Property<Opacity>* opacity = nullptr;
  Property<float>* angle = nullptr;
  Property<float>* distance = nullptr;
  Property<float>* size = nullptr;
  Property<Percent>* spread = nullptr;

  RTTR_ENABLE(LayerStyle)
};

class PAG_API StrokeStyle : public LayerStyle {
 public:
  ~StrokeStyle() override;

  LayerStyleType type() const override {
    return LayerStyleType::Stroke;
  }

  LayerStylePosition drawPosition() const override {
    return LayerStylePosition::Above;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<BlendMode>* blendMode = nullptr;
  Property<Color>* color = nullptr;
  Property<float>* size = nullptr;
  Property<Opacity>* opacity = nullptr;
  Property<StrokePosition>* position = nullptr;

  RTTR_ENABLE(LayerStyle)
};

struct AlphaStop {
  float position = 0.0f;
  float midpoint = 0.5f;
  Opacity opacity = Opaque;
};

struct ColorStop {
  float position = 0.0f;
  float midpoint = 0.5f;
  Color color = Black;
};

class PAG_API GradientColor {
 public:
  std::vector<AlphaStop> alphaStops;
  std::vector<ColorStop> colorStops;

  void interpolate(const GradientColor& other, GradientColor* result, float t);
};

typedef std::shared_ptr<GradientColor> GradientColorHandle;

class PAG_API GradientOverlayStyle : public LayerStyle {
 public:
  ~GradientOverlayStyle() override;

  LayerStyleType type() const override {
    return LayerStyleType::GradientOverlay;
  }

  LayerStylePosition drawPosition() const override {
    return LayerStylePosition::Above;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<BlendMode>* blendMode = nullptr;
  Property<Opacity>* opacity = nullptr;
  Property<GradientColorHandle>* colors = nullptr;
  Property<float>* gradientSmoothness = nullptr;
  Property<float>* angle = nullptr;
  Property<GradientFillType>* style = nullptr;
  Property<bool>* reverse = nullptr;
  Property<bool>* alignWithLayer = nullptr;
  Property<float>* scale = nullptr;
  Property<Point>* offset = nullptr;

  RTTR_ENABLE(LayerStyle)
};

class PAG_API OuterGlowStyle : public LayerStyle {
 public:
  ~OuterGlowStyle() override;

  LayerStyleType type() const override {
    return LayerStyleType::OuterGlow;
  }

  LayerStylePosition drawPosition() const override {
    return LayerStylePosition::Blow;
  }

  bool visibleAt(Frame layerFrame) const override;

  void transformBounds(Rect* contentBounds, const Point& filterScale,
                       Frame layerFrame) const override;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  Property<BlendMode>* blendMode = nullptr;
  Property<Opacity>* opacity = nullptr;
  Property<Percent>* noise = nullptr;
  Property<GlowColorType>* colorType = nullptr;
  Property<Color>* color = nullptr;
  Property<GradientColorHandle>* colors = nullptr;
  Property<Percent>* gradientSmoothness = nullptr;
  Property<GlowTechniqueType>* technique = nullptr;
  Property<Percent>* spread = nullptr;
  Property<float>* size = nullptr;
  Property<Percent>* range = nullptr;
  Property<Percent>* jitter = nullptr;

  RTTR_ENABLE(LayerStyle)
};

class PAG_API TextPathOptions {
 public:
  ~TextPathOptions();

  MaskData* path = nullptr;  // mask reference
  Property<bool>* reversedPath = nullptr;
  Property<bool>* perpendicularToPath = nullptr;
  Property<bool>* forceAlignment = nullptr;
  Property<float>* firstMargin = nullptr;
  Property<float>* lastMargin = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

enum class PAG_API AnchorPointGrouping : uint8_t{
  Character = 0,
  Word = 1,
  Line = 2,
  All = 3
};

class PAG_API TextMoreOptions {
 public:
  ~TextMoreOptions();

  AnchorPointGrouping anchorPointGrouping = AnchorPointGrouping::Character;
  Property<Point>* groupingAlignment = nullptr;  // multidimensional Percent

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

enum class PAG_API TextRangeSelectorUnits : uint8_t {
  Percentage = 0,
  Index = 1
};

enum class PAG_API TextSelectorBasedOn : uint8_t {
  Characters = 0,
  CharactersExcludingSpaces = 1,
  Words = 2,
  Lines = 3
};

enum class PAG_API TextSelectorMode : uint8_t {
  None = 0,
  Add = 1,
  Subtract = 2,
  Intersect = 3,
  Min = 4,
  Max = 5,
  Difference = 6
};

enum class PAG_API TextRangeSelectorShape : uint8_t {
  Square = 0,
  RampUp = 1,
  RampDown = 2,
  Triangle = 3,
  Round = 4,
  Smooth = 5
};

enum class PAG_API TextAnimatorTrackingType : uint8_t {
  BeforeAndAfter = 0,
  Before = 1,
  After = 2
};

enum class PAG_API TextSelectorType : uint8_t {
  Range = 0,
  Wiggly = 1,
  Expression = 2
};

class PAG_API TextSelector {
 public:
  virtual ~TextSelector() {
  }

  virtual TextSelectorType type() const = 0;

  virtual void excludeVaryingRanges(std::vector<TimeRange>*) const = 0;

  virtual bool verify() const = 0;
};

class PAG_API TextRangeSelector : public TextSelector {
 public:
  ~TextRangeSelector();

  TextSelectorType type() const override {
    return TextSelectorType::Range;
  };

  Property<Percent>* start = nullptr;
  Property<Percent>* end = nullptr;
  Property<float>* offset = nullptr;
  TextRangeSelectorUnits units = TextRangeSelectorUnits::Percentage;
  TextSelectorBasedOn basedOn = TextSelectorBasedOn::Characters;
  Property<TextSelectorMode>* mode = nullptr;
  Property<Percent>* amount = nullptr;
  TextRangeSelectorShape shape = TextRangeSelectorShape::Square;
  Property<Percent>* smoothness = nullptr;
  Property<Percent>* easeHigh = nullptr;
  Property<Percent>* easeLow = nullptr;
  bool randomizeOrder = false;
  Property<uint16_t>* randomSeed = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;
};

class PAG_API TextWigglySelector : public TextSelector {
 public:
  ~TextWigglySelector();

  TextSelectorType type() const override {
    return TextSelectorType::Wiggly;
  }

  Property<TextSelectorMode>* mode = nullptr;
  Property<Percent>* maxAmount = nullptr;
  Property<Percent>* minAmount = nullptr;
  TextSelectorBasedOn basedOn = TextSelectorBasedOn::Characters;
  Property<float>* wigglesPerSecond = nullptr;
  Property<Percent>* correlation = nullptr;
  Property<float>* temporalPhase = nullptr;
  Property<float>* spatialPhase = nullptr;
  Property<bool>* lockDimensions = nullptr;
  Property<uint16_t>* randomSeed = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;
};

class PAG_API TextAnimatorColorProperties {
 public:
  ~TextAnimatorColorProperties();

  Property<Color>* fillColor = nullptr;
  Property<Color>* strokeColor = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

class PAG_API TextAnimatorTypographyProperties {
 public:
  ~TextAnimatorTypographyProperties();

  Property<TextAnimatorTrackingType>* trackingType = nullptr;
  Property<float>* trackingAmount = nullptr;
  Property<Point>* position = nullptr;  // spatial
  Property<Point>* scale = nullptr;     // multidimensional
  Property<float>* rotation = nullptr;
  Property<Opacity>* opacity = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

class PAG_API TextAnimator {
 public:
  ~TextAnimator();

  // Text Animator selectors
  std::vector<TextSelector*> selectors;

  // Text Animator Properties
  TextAnimatorColorProperties* colorProperties = nullptr;
  TextAnimatorTypographyProperties* typographyProperties = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

class PAG_API ShapeTransform {
 public:
  ~ShapeTransform();

  Property<Point>* anchorPoint = nullptr;  // spatial
  Property<Point>* position = nullptr;     // spatial
  Property<Point>* scale = nullptr;        // multidimensional
  Property<float>* skew = nullptr;
  Property<float>* skewAxis = nullptr;
  Property<float>* rotation = nullptr;
  Property<Opacity>* opacity = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

enum class RTTR_AUTO_REGISTER_CLASS ShapeType {
  Unknown,
  ShapeGroup,
  Rectangle,
  Ellipse,
  PolyStar,
  ShapePath,
  Fill,
  Stroke,
  GradientFill,
  GradientStroke,
  MergePaths,
  TrimPaths,
  Repeater,
  RoundCorners
};

class PAG_API ShapeElement {
 public:
  virtual ~ShapeElement() = default;

  virtual ShapeType type() const {
    return ShapeType::Unknown;
  }

  virtual void excludeVaryingRanges(std::vector<TimeRange>*) const {
  }

  virtual bool verify() const {
    return true;
  }

  RTTR_ENABLE()
};

class PAG_API ShapeGroupElement : public ShapeElement {
 public:
  ~ShapeGroupElement() override;

  ShapeType type() const override {
    return ShapeType::ShapeGroup;
  }

  BlendMode blendMode = BlendMode::Normal;
  ShapeTransform* transform = nullptr;
  std::vector<ShapeElement*> elements;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API RectangleElement : public ShapeElement {
 public:
  ~RectangleElement() override;

  ShapeType type() const override {
    return ShapeType::Rectangle;
  }

  bool reversed = false;
  Property<Point>* size = nullptr;      // multidimensional
  Property<Point>* position = nullptr;  // spatial
  Property<float>* roundness = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API EllipseElement : public ShapeElement {
 public:
  ~EllipseElement() override;

  ShapeType type() const override {
    return ShapeType::Ellipse;
  }

  bool reversed = false;
  Property<Point>* size = nullptr;      // multidimensional
  Property<Point>* position = nullptr;  // spatial

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

enum class PAG_API PolyStarType : uint8_t {
  Star = 0,
  Polygon = 1
};

class PAG_API PolyStarElement : public ShapeElement {
 public:
  ~PolyStarElement() override;

  ShapeType type() const override {
    return ShapeType::PolyStar;
  }

  bool reversed = false;
  PolyStarType polyType = PolyStarType::Star;
  Property<float>* points = nullptr;
  Property<Point>* position = nullptr;  // spatial
  Property<float>* rotation = nullptr;
  Property<float>* innerRadius = nullptr;
  Property<float>* outerRadius = nullptr;
  Property<Percent>* innerRoundness = nullptr;
  Property<Percent>* outerRoundness = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API ShapePathElement : public ShapeElement {
 public:
  ~ShapePathElement() override;

  ShapeType type() const override {
    return ShapeType::ShapePath;
  }

  Property<PathHandle>* shapePath = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

enum class PAG_API CompositeOrder : uint8_t {
  BelowPreviousInSameGroup = 0,
  AbovePreviousInSameGroup = 1
};

class PAG_API FillElement : public ShapeElement {
 public:
  ~FillElement() override;

  ShapeType type() const override {
    return ShapeType::Fill;
  }

  BlendMode blendMode = BlendMode::Normal;
  CompositeOrder composite = CompositeOrder::BelowPreviousInSameGroup;
  FillRule fillRule = FillRule::NonZeroWinding;
  Property<Color>* color = nullptr;
  Property<Opacity>* opacity = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API StrokeElement : public ShapeElement {
 public:
  ~StrokeElement() override;

  ShapeType type() const override {
    return ShapeType::Stroke;
  }

  BlendMode blendMode = BlendMode::Normal;
  CompositeOrder composite = CompositeOrder::BelowPreviousInSameGroup;
  Property<Color>* color = nullptr;
  Property<Opacity>* opacity = nullptr;
  Property<float>* strokeWidth = nullptr;
  LineCap lineCap = LineCap::Butt;
  LineJoin lineJoin = LineJoin::Miter;
  Property<float>* miterLimit = nullptr;
  Property<float>* dashOffset = nullptr;
  std::vector<Property<float>*> dashes;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API GradientFillElement : public ShapeElement {
 public:
  ~GradientFillElement() override;

  ShapeType type() const override {
    return ShapeType::GradientFill;
  }

  BlendMode blendMode = BlendMode::Normal;
  CompositeOrder composite = CompositeOrder::BelowPreviousInSameGroup;
  FillRule fillRule = FillRule::NonZeroWinding;
  GradientFillType fillType = GradientFillType::Linear;
  Property<Opacity>* opacity = nullptr;
  Property<Point>* startPoint = nullptr;  // spatial
  Property<Point>* endPoint = nullptr;    // spatial
  Property<GradientColorHandle>* colors = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API GradientStrokeElement : public ShapeElement {
 public:
  ~GradientStrokeElement() override;

  ShapeType type() const override {
    return ShapeType::GradientStroke;
  }

  BlendMode blendMode = BlendMode::Normal;
  CompositeOrder composite = CompositeOrder::BelowPreviousInSameGroup;
  GradientFillType fillType = GradientFillType::Linear;
  LineCap lineCap = LineCap::Butt;
  LineJoin lineJoin = LineJoin::Miter;
  Property<float>* miterLimit = nullptr;
  Property<Point>* startPoint = nullptr;  // spatial
  Property<Point>* endPoint = nullptr;    // spatial
  Property<GradientColorHandle>* colors = nullptr;
  Property<Opacity>* opacity = nullptr;
  Property<float>* strokeWidth = nullptr;
  Property<float>* dashOffset = nullptr;
  std::vector<Property<float>*> dashes;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

enum class PAG_API MergePathsMode : uint8_t {
  Merge = 0,
  Add = 1,
  Subtract = 2,
  Intersect = 3,
  ExcludeIntersections = 4
};

class PAG_API MergePathsElement : public ShapeElement {
 public:
  ShapeType type() const override {
    return ShapeType::MergePaths;
  }

  MergePathsMode mode = MergePathsMode::Add;

  RTTR_ENABLE(ShapeElement)
};

enum class PAG_API TrimPathsType : uint8_t {
  Simultaneously = 0,
  Individually = 1
};

class PAG_API TrimPathsElement : public ShapeElement {
 public:
  ~TrimPathsElement() override;

  ShapeType type() const override {
    return ShapeType::TrimPaths;
  }

  Property<Percent>* start = nullptr;
  Property<Percent>* end = nullptr;
  Property<float>* offset = nullptr;
  TrimPathsType trimType = TrimPathsType::Simultaneously;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API RepeaterTransform {
 public:
  ~RepeaterTransform();

  Property<Point>* anchorPoint = nullptr;  // spatial
  Property<Point>* position = nullptr;     // spatial
  Property<Point>* scale = nullptr;        // multidimensional
  Property<float>* rotation = nullptr;
  Property<Opacity>* startOpacity = nullptr;
  Property<Opacity>* endOpacity = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

enum class PAG_API RepeaterOrder : uint8_t {
  Below = 0,
  Above = 1
};

class PAG_API RepeaterElement : public ShapeElement {
 public:
  ~RepeaterElement() override;

  ShapeType type() const override {
    return ShapeType::Repeater;
  }

  Property<float>* copies = nullptr;
  Property<float>* offset = nullptr;
  RepeaterOrder composite = RepeaterOrder::Below;
  RepeaterTransform* transform = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API RoundCornersElement : public ShapeElement {
 public:
  ~RoundCornersElement() override;

  ShapeType type() const override {
    return ShapeType::RoundCorners;
  }

  Property<float>* radius = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
};

enum class PAG_API IrisShapeType : uint8_t {
  FastRectangle = 0,
  Triangle = 1,
  Square = 2,
  Pentagon = 3,
  Hexagon = 4,
  Heptagon = 5,
  Octagon = 6,
  Nonagon = 7,
  Decagon = 8
};

class PAG_API CameraOption {
 public:
  ~CameraOption();

  Property<float>* zoom = nullptr;
  Property<bool>* depthOfField = nullptr;
  Property<float>* focusDistance = nullptr;
  Property<float>* aperture = nullptr;
  Property<Percent>* blurLevel = nullptr;
  Property<IrisShapeType>* irisShape = nullptr;
  Property<float>* irisRotation = nullptr;
  Property<Percent>* irisRoundness = nullptr;
  Property<float>* irisAspectRatio = nullptr;
  Property<float>* irisDiffractionFringe = nullptr;
  Property<float>* highlightGain = nullptr;
  Property<float>* highlightThreshold = nullptr;
  Property<float>* highlightSaturation = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

class Composition;

class VectorComposition;

class BitmapComposition;

class ImageBytes;

class PAG_API Cache {
 public:
  virtual ~Cache() = default;
};

enum class PAG_API TrackMatteType : uint8_t {
  None = 0,
  Alpha = 1,
  AlphaInverted = 2,
  Luma = 3,
  LumaInverted = 4
};

enum class PAG_API CachePolicy : uint8_t {
  Auto = 0,
  Enable = 1,
  Disable = 2
};

/**
 * The Layer object provides access to layers within compositions.
 */
class PAG_API Layer {
 public:
  Layer();

  virtual ~Layer();

  /**
   * The type of the layer.
   */
  virtual LayerType type() const {
    return LayerType::Unknown;
  }

  /**
   * Returns a globally unique id for this object.
   */
  ID uniqueID = 0;

  /**
   * The id of this layer.
   */
  ID id = 0;
  /**
   * The parent of this layer.
   */
  Layer* RTTR_SKIP_REGISTER_PROPERTY parent = nullptr;  // layer reference

  VectorComposition* RTTR_SKIP_REGISTER_PROPERTY containingComposition = nullptr;  // composition reference

  /**
   * The name of the layer.
   */
  std::string name = "";
  /**
   * The time stretch percentage of the layer.
   */
  Ratio stretch = DefaultRatio;
  /**
   * The start time of the layer, indicates the start position of the visible range. It could be a
   * negative value.
   */
  Frame startTime = ZeroFrame;
  /**
   * The duration of the layer, indicates the length of the visible range.
   */
  Frame duration = ZeroFrame;
  /**
   * When true, the layer' automatic orientation is enabled.
   */
  bool autoOrientation = false;
  /**
   * When true, the layer' motion blur is enabled.
   */
  bool motionBlur = false;
  /**
   * The 2D transformation of the layer.
   */
  Transform2D* transform = nullptr;
  /**
   * The 3D transformation of the layer.
   */
  Transform3D* transform3D = nullptr;
  /**
   * When false, the layer should be skipped during the rendering loop.
   */
  bool isActive = true;
  /**
   * The blending mode of the layer.
   */
  BlendMode blendMode = BlendMode::Normal;
  /**
   * If layer has a track matte, specifies the way it is applied.
   */
  TrackMatteType trackMatteType = TrackMatteType::None;
  Layer* trackMatteLayer = nullptr;
  Property<float>* timeRemap = nullptr;
  std::vector<MaskData*> masks;
  std::vector<Effect*> effects;
  std::vector<LayerStyle*> layerStyles;
  std::vector<Marker*> markers;
  CachePolicy cachePolicy = CachePolicy::Auto;

  Cache* RTTR_SKIP_REGISTER_PROPERTY cache = nullptr;
  std::mutex locker = {};

  virtual void excludeVaryingRanges(std::vector<TimeRange>* timeRanges);
  virtual bool verify() const;
  Point getMaxScaleFactor();
  std::pair<Point, Point> getScaleFactor();
  TimeRange visibleRange();
  virtual Rect getBounds() const;

 private:
  bool verifyExtra() const;

  RTTR_ENABLE()
};

class PAG_API NullLayer : public Layer {
 public:
  /**
   * The type of the layer.
   */
  LayerType type() const override {
    return LayerType::Null;
  };

  RTTR_ENABLE(Layer)
};

class PAG_API SolidLayer : public Layer {
 public:
  LayerType type() const override {
    return LayerType::Solid;
  };
  /**
   * The background color of the solid layer.
   */
  Color solidColor = Black;
  /**
   * The width of the SolidLayer.
   */
  int32_t width = 0;
  /**
   * The height of the SolidLayer.
   */
  int32_t height = 0;

  bool verify() const override;

  Rect getBounds() const override;

  RTTR_ENABLE(Layer)
};

class PAG_API TextLayer : public Layer {
 public:
  ~TextLayer() override;

  LayerType type() const override {
    return LayerType::Text;
  };

  Property<TextDocumentHandle>* sourceText = nullptr;
  TextPathOptions* pathOption = nullptr;
  TextMoreOptions* moreOption = nullptr;
  std::vector<TextAnimator*> animators;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) override;
  bool verify() const override;

  Rect getBounds() const override;

  std::shared_ptr<TextDocument> getTextDocument();

  RTTR_ENABLE(Layer)
};

class PAG_API ShapeLayer : public Layer {
 public:
  ~ShapeLayer() override;

  LayerType type() const override {
    return LayerType::Shape;
  };

  std::vector<ShapeElement*> contents;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) override;
  bool verify() const override;

  Rect getBounds() const override;

  RTTR_ENABLE(Layer)
};

class PAG_API ImageFillRule {
 public:
  ~ImageFillRule();

  PAGScaleMode scaleMode = PAGScaleMode::LetterBox;

  // timeRemap基于composition时间轴
  Property<Frame>* timeRemap = nullptr;
};

class PAG_API ImageLayer : public Layer {
 public:
  ~ImageLayer() override;

  LayerType type() const override {
    return LayerType::Image;
  };

  // it is owned by the PAG file, there is no need to delete it.
  ImageBytes* imageBytes = nullptr;

  ImageFillRule* imageFillRule = nullptr;

  bool verify() const override;

  Rect getBounds() const override;

  RTTR_ENABLE(Layer)
};

class PAG_API PreComposeLayer : public Layer {
 public:
  static std::unique_ptr<PreComposeLayer> Wrap(Composition* composition);

  LayerType type() const override {
    return LayerType::PreCompose;
  };

  Composition* composition = nullptr;  // composition reference

  /**
   * Indicates when the first frame of the composition shows in the layer's timeline. It could be a
   * negative value. It is in layer's frame rate.
   */
  Frame compositionStartTime = ZeroFrame;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) override;
  bool verify() const override;
  std::vector<TimeRange> getContentStaticTimeRanges() const;
  Frame getCompositionFrame(Frame layerFrame);
  Rect getBounds() const override;

  RTTR_ENABLE(Layer)
};

class PAG_API CameraLayer : public Layer {
 public:
  ~CameraLayer() override;

  LayerType type() const override {
    return LayerType::Camera;
  };

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) override;
  bool verify() const override;

  Rect getBounds() const override;

  CameraOption* cameraOption = nullptr;

  RTTR_ENABLE(Layer)
};

/**
 * Compositions are always one of the following types.
 */
enum class RTTR_AUTO_REGISTER_CLASS CompositionType { Unknown, Vector, Bitmap, Video };

class PAG_API Composition {
 public:
  Composition();

  virtual ~Composition();
  /**
   * Returns a globally unique id for this object.
   */
  ID uniqueID = 0;
  /**
   * A unique identifier for this item.
   */
  ID id = ZeroID;
  /**
   * The width of the Composition.
   */
  int32_t width = 0;
  /**
   * The height of the item.
   */
  int32_t height = 0;
  /**
   * The total duration of the item.
   */
  Frame duration = ZeroFrame;
  /**
   * The frame rate of the Composition.
   */
  float frameRate = 30;
  /**
   * The background color of the composition.
   */
  Color backgroundColor = White;

  /**
   * The audio data of this composition, which is an AAC audio in an MPEG-4 container.
   */
  ByteData* audioBytes = nullptr;
  std::vector<Marker*> audioMarkers;

  /**
   * Indicates when the first frame of the audio plays in the composition's timeline.
   */
  Frame audioStartTime = ZeroFrame;

  /**
   * [FrameStart, FrameEnd(included)], [FrameStar, FrameEnd]...
   */
  std::vector<TimeRange> staticTimeRanges;
  Cache* RTTR_SKIP_REGISTER_PROPERTY cache = nullptr;
  std::mutex locker = {};

  bool staticContent() const;

  /**
   * The type of the Composition.
   */
  virtual CompositionType type() const;

  /**
   * Returns true if this composition contains ImageLayers, BitmapCompositions or VideoCompositions.
   */
  virtual bool hasImageContent() const;

  virtual bool verify() const;

 protected:
  // Called by Codec.
  virtual void updateStaticTimeRanges();

 private:
  bool staticTimeRangeUpdated = false;

  friend class Codec;

  friend class VectorComposition;

  RTTR_ENABLE()
};

class PAG_API VectorComposition : public Composition {
 public:
  ~VectorComposition() override;
  /**
   * The layers of the composition.
   */
  std::vector<Layer*> layers;

  /**
   * The type of the Composition.
   */
  CompositionType type() const override;

  bool hasImageContent() const override;

  bool verify() const override;

 protected:
  void updateStaticTimeRanges() override;

  RTTR_ENABLE(Composition)
};

class PAG_API ImageBytes {
 public:
  ImageBytes();

  ~ImageBytes();
  /**
   * Returns a globally unique id for this object.
   */
  ID uniqueID = 0;
  /**
   * The id for this image bytes.
   */
  ID id = ZeroID;
  /**
   * The width of the image. It is set when the image is stripped of its transparent border,
   * otherwise the value would be 0.
   */
  int32_t width = 0;
  /**
   * The height of the image. It is set when the image is stripped of its transparent border,
   * otherwise the value would be 0.
   */
  int32_t height = 0;
  /**
   * The anchor x of the image. It is set when the image is stripped of its transparent border.
   */
  int32_t anchorX = 0;
  /**
   * The anchor y of the image. It is set when the image is stripped of its transparent border.
   */
  int32_t anchorY = 0;
  /**
   * The scale factor of image, ranges from 0.0 to 1.0.
   */
  float scaleFactor = 1.0f;
  /**
   * The file data of this image bytes.
   */
  ByteData* fileBytes = nullptr;

  bool verify() const;

  Cache* RTTR_SKIP_REGISTER_PROPERTY cache = nullptr;
  std::mutex locker = {};

 private:
  bool encrypted = false;

  friend class ImageBytesCache;
  friend class EncryptedCodec;
};

class PAG_API BitmapRect {
 public:
  ~BitmapRect();

  /**
   * The x position of the bitmap rect.
   */
  int32_t x = 0;
  /**
   * The y position of the bitmap rect.
   */
  int32_t y = 0;
  /**
   * The file bytes of the image.
   */
  ByteData* fileBytes = nullptr;
};

class PAG_API Sequence {
 public:
  /**
   * Returns the sequence in specified composition, returns nullptr if the type of composition is
   * not video or bitmap.
   */
  static Sequence* Get(Composition* composition);

  virtual ~Sequence() = default;
  /**
   * The Composition which owns this Sequence.
   */
  Composition* RTTR_SKIP_REGISTER_PROPERTY composition = nullptr;
  /**
   * The width of the sequence.
   */
  int32_t width = 0;
  /**
   * The height of the sequence.
   */
  int32_t height = 0;
  /**
   * The frame rate of sequence.
   */
  float frameRate = 30;
  /**
   * The total duration of the item.
   */
  virtual Frame duration() const = 0;

  virtual bool verify() const;

  Frame toSequenceFrame(Frame compositionFrame);

  RTTR_ENABLE()
};

class PAG_API BitmapFrame {
 public:
  ~BitmapFrame();
  /**
   * Indicates whether or not it is a key frame.
   */
  bool isKeyframe = false;
  /**
   * The image bytes need to be drawn onto.
   */
  std::vector<BitmapRect*> bitmaps;

  bool verify() const;
};

class PAG_API BitmapSequence : public Sequence {
 public:
  ~BitmapSequence() override;
  /**
   * The bitmap frames of the Composition.
   */
  std::vector<BitmapFrame*> frames;

  virtual bool isEmptyBitmapFrame(size_t frameIndex);

  Frame duration() const override {
    return static_cast<Frame>(frames.size());
  }

  bool verify() const override;

  RTTR_ENABLE(Sequence)
};

class PAG_API BitmapComposition : public Composition {
 public:
  ~BitmapComposition() override;
  /**
   * The type of the Composition.
   */
  CompositionType type() const override;
  /**
   * The bitmap frames of the Composition. It is sorted in ascending order.
   */
  std::vector<BitmapSequence*> sequences;

  bool hasImageContent() const override;

  bool verify() const override;

 protected:
  void updateStaticTimeRanges() override;

  RTTR_ENABLE(Composition)
};

class PAG_API VideoFrame {
 public:
  ~VideoFrame();
  /**
   * Indicates whether or not it is a key frame.
   */
  bool isKeyframe = false;

  /**
   * The presentation frame index.
   */
  Frame frame = 0;

  /**
   * The file bytes of the video frame.
   */
  ByteData* fileBytes = nullptr;
};

class PAG_API VideoSequence : public Sequence {
 public:
  ~VideoSequence() override;
  /**
   * The x position of the alpha start point.
   */
  int32_t alphaStartX = 0;
  /**
   * The y position of the alpha start point.
   */
  int32_t alphaStartY = 0;
  /**
   * The video frames of the Composition.
   */
  std::vector<VideoFrame*> frames;
  /**
   * Codec specific data.
   */
  std::vector<ByteData*> headers;

  std::vector<TimeRange> staticTimeRanges;

  ByteData* MP4Header = nullptr;

  Frame duration() const override {
    return static_cast<Frame>(frames.size());
  }

  bool verify() const override;

  int32_t getVideoWidth() const;

  int32_t getVideoHeight() const;

  RTTR_ENABLE(Sequence)
};

class PAG_API VideoComposition : public Composition {
 public:
  ~VideoComposition() override;
  /**
   * The type of the Composition.
   */
  CompositionType type() const override;

  /**
   * The video frames of the Composition. It is sorted in ascending order.
   */
  std::vector<VideoSequence*> sequences;

  bool hasImageContent() const override;

  bool verify() const override;

 protected:
  void updateStaticTimeRanges() override;

  RTTR_ENABLE(Composition)
};

class PAG_API FontData {
 public:
  FontData(std::string fontFamily, std::string fontStyle)
      : fontFamily(std::move(fontFamily)), fontStyle(std::move(fontStyle)) {
  }

  const std::string fontFamily;
  const std::string fontStyle;
};

class PAG_API FileAttributes {
 public:
  bool empty() const {
    return timestamp == 0 && pluginVersion.empty() && aeVersion.empty() && systemVersion.empty() &&
           author.empty() && scene.empty() && warnings.empty();
  }

  int64_t timestamp = 0;
  std::string pluginVersion = "";
  std::string aeVersion = "";
  std::string systemVersion = "";
  std::string author = "";
  std::string scene = "";
  std::vector<std::string> warnings;
};

struct PerformanceData {
  /**
   * The time cost by rendering in microseconds.
   */
  int64_t renderingTime;

  /**
   * The time cost by image decoding in microseconds.
   */
  int64_t imageDecodingTime;

  /**
   * The time cost by presenting in microseconds.
   */
  int64_t presentingTime;

  /**
   * The memory cost by graphics in bytes.
   */
  int64_t graphicsMemory;
};

class PAG_API File {
 public:
  /**
   * The maximum tag level current SDK supports.
   */
  static uint16_t MaxSupportedTagLevel();
  /**
   *  Load a pag file from byte data, return null if the bytes is empty or it's not a valid pag
   * file.
   */
  static std::shared_ptr<File> Load(const void* bytes, size_t length,
                                    const std::string& filePath = "");
  /**
   *  Load a pag file from path, return null if the file does not exist or the data is not a pag
   * file.
   */
  static std::shared_ptr<File> Load(const std::string& filePath);

  ~File();

  /**
   * Return the duration of the pag file in frames.
   */
  int64_t duration() const;

  /**
   * The frame rate of the pag file.
   */
  float frameRate() const;

  /**
   * Return the original display width in pixels of pag file.
   */
  int32_t width() const;

  /**
   * Return the original display height in pixels of pag file.
   */
  int32_t height() const;

  /**
   * The tag level this pag file requires.
   */
  uint16_t tagLevel() const;

  /**
   * The background color of the pag file.
   */
  Color backgroundColor() const;

  /**
   * The number of replaceable texts.
   */
  int numTexts() const;

  /**
   * The number of replaceable images.
   */
  int numImages() const;

  /**
   * The number of video compositions.
   */
  int numVideos() const;

  /**
   * The 'total' number of layers in this pag file, for profiling only.
   */
  int numLayers() const;

  /**
   * Get a text data of the specified index. The index ranges from 0 to numTexts - 1.
   */
  TextDocumentHandle getTextData(int index) const;

  PreComposeLayer* getRootLayer() const;

  TextLayer* getTextAt(int index) const;

  std::vector<ImageLayer*> getImageAt(int index) const;

  int getEditableIndex(TextLayer* textLayer) const;

  int getEditableIndex(ImageLayer* imageLayer) const;

  bool hasScaledTimeRange() const;

  /**
   * Indicates how to stretch the duration of File when rendering.
   */
  PAGTimeStretchMode timeStretchMode = PAGTimeStretchMode::Repeat;
  TimeRange scaledTimeRange = {};
  FileAttributes fileAttributes = {};
  std::string path = "";
  std::vector<ImageBytes*> images;
  std::vector<Composition*> compositions;

  std::vector<int>* editableImages = nullptr;
  std::vector<int>* editableTexts = nullptr;
  std::vector<PAGScaleMode>* imageScaleModes = nullptr;

  RTTR_REGISTER_FUNCTION_AS_PROPERTY("duration", duration)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("frameRate", frameRate)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("width", width)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("height", height)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("tagLevel", tagLevel)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("backgroundColor", backgroundColor)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("numTexts", numTexts)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("numImages", numImages)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("numVideos", numVideos)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("numLayers", numLayers)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("rootLayer", getRootLayer)
  RTTR_REGISTER_FUNCTION_AS_PROPERTY("hasScaledTimeRange", hasScaledTimeRange)

 private:
  PreComposeLayer* rootLayer = nullptr;
  Composition* mainComposition = nullptr;
  uint16_t _tagLevel = 1;
  int _numLayers = 0;
  bool encrypted = false;

  // Just references, no need to delete them.
  std::vector<TextLayer*> textLayers = {};

  // Just references, no need to delete them.
  std::vector<std::vector<ImageLayer*>> imageLayers = {};

  File(std::vector<Composition*> compositionList, std::vector<pag::ImageBytes*> imageList);
  void updateEditables(Composition* composition);

  friend class Codec;
  friend class SequenceInfo;
  friend class EncryptedCodec;
};

/**
 * Calculate the memory cost by graphics of file in bytes.
 */
int64_t PAG_API CalculateGraphicsMemory(std::shared_ptr<File> file);

class CodecContext;

class PAG_API Codec {
 public:
  /**
   * Return the maximum supported tag level.
   */
  static uint16_t MaxSupportedTagLevel();

  /**
   * Replace all temporary mask with real references.
   */
  static void InstallReferences(Layer* layer);

  /**
   * Replace all temporary layer with real references.
   */
  static void InstallReferences(const std::vector<Layer*>& layers);

  /**
   * Replace all temporary composition with real references.
   */
  static void InstallReferences(const std::vector<Composition*>& compositions);

  /**
   * Return null if the the specified data are not valid to create a pag file.
   */
  static std::shared_ptr<File> VerifyAndMake(const std::vector<Composition*>& compositions,
                                             const std::vector<pag::ImageBytes*>& images);

  /**
   * Decode a pag file from the specified byte data, return null if the bytes is empty or it's not a
   * valid pag file.
   */
  static std::shared_ptr<File> Decode(const void* bytes, uint32_t byteLength,
                                      const std::string& path);

  /**
   * Encode a pag file to byte data, return null if the file is null.
   */
  static std::unique_ptr<ByteData> Encode(std::shared_ptr<File> file);

  /**
   * Encode a pag file with the corresponding performance data to byte data, return null if the file
   * is null.
   */
  static std::unique_ptr<ByteData> Encode(std::shared_ptr<File> pagFile,
                                          std::shared_ptr<PerformanceData> performanceData);

  /**
   * Read the performance data from the specified byte data, return null if the byte data contains
   * no performance data.
   */
  static std::shared_ptr<PerformanceData> ReadPerformanceData(const void* bytes,
                                                              uint32_t byteLength);

 protected:
  static void UpdateFileAttributes(std::shared_ptr<File> file, CodecContext* context,
                                   const std::string& filePath);
};
}  // namespace pag
