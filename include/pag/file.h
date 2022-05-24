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
enum class TagCode {
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
  // add new tags here...

  Count
};

struct Ratio {
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

enum class PathDataVerb { MoveTo, LineTo, CurveTo, Close };

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

class PAG_API KeyframeInterpolationType {
 public:
  static const Enum None = 0;
  static const Enum Linear = 1;
  static const Enum Bezier = 2;
  static const Enum Hold = 3;
};

template <typename T>
class Keyframe {
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
  Enum interpolationType = KeyframeInterpolationType::Hold;
  std::vector<Point> bezierOut;
  std::vector<Point> bezierIn;
  Point spatialOut = Point::Zero();
  Point spatialIn = Point::Zero();
};

/**
 * The Property object contains value information about a particular AE property of a layer.
 */
template <typename T>
class Property {
 public:
  T value;

  virtual ~Property() = default;

  virtual bool animatable() const {
    return false;
  }

  virtual T getValueAt(Frame) {
    return this->value;
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

template <typename T>
class AnimatableProperty : public Property<T> {
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
    Keyframe<T>* lastKeyframe = keyframes[lastKeyframeIndex];
    if (lastKeyframe->containsTime(frame)) {
      return lastKeyframe->getValueAt(frame);
    }
    if (frame < lastKeyframe->startTime) {
      while (lastKeyframeIndex > 0) {
        lastKeyframeIndex--;
        if (keyframes[lastKeyframeIndex]->containsTime(frame)) {
          break;
        }
      }
    } else {
      while (lastKeyframeIndex < keyframes.size() - 1) {
        lastKeyframeIndex++;
        if (keyframes[lastKeyframeIndex]->containsTime(frame)) {
          break;
        }
      }
    }
    lastKeyframe = keyframes[lastKeyframeIndex];
    if (frame <= lastKeyframe->startTime) {
      result = lastKeyframe->startValue;
    } else if (frame >= lastKeyframe->endTime) {
      result = lastKeyframe->endValue;
    } else {
      result = lastKeyframe->getValueAt(frame);
    }
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
  static Transform2D* MakeDefault();
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

class PAG_API MaskMode {
 public:
  static const Enum None = 0;  // mask shape does nothing
  static const Enum Add = 1;   // shape is added into mask (normal behavior (really screen!))
  static const Enum Subtract = 2;
  static const Enum Intersect = 3;
  static const Enum Lighten = 4;
  static const Enum Darken = 5;
  static const Enum Difference = 6;
  static const Enum Accum = 7;  // real add, not screen (not exposed in UI!)
};

class PAG_API MaskData {
 public:
  ~MaskData();

  ID id = ZeroID;
  bool inverted = false;
  Enum maskMode = MaskMode::Add;
  Property<PathHandle>* maskPath = nullptr;
  Property<Opacity>* maskOpacity = nullptr;
  Property<float>* maskExpansion = nullptr;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

enum class EffectType {
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

class PAG_API BlurDimensionsDirection {
 public:
  static const Enum All = 0;
  static const Enum Horizontal = 1;
  static const Enum Vertical = 2;
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
  Property<Enum>* blurDimensions = nullptr;
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

class PAG_API DisplacementMapSource {
 public:
  static const Enum Red = 0;
  static const Enum Green = 1;
  static const Enum Blue = 2;
  static const Enum Alpha = 3;
  static const Enum Luminance = 4;
  static const Enum Hue = 5;
  static const Enum Lightness = 6;
  static const Enum Saturation = 7;
  static const Enum Full = 8;
  static const Enum Half = 9;
  static const Enum Off = 10;
};

class DisplacementMapBehavior {
 public:
  static const Enum CenterMap = 0;
  static const Enum StretchMapToFit = 1;
  static const Enum TileMap = 2;
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

  Layer* displacementMapLayer = nullptr;                   // ref layer
  Property<Enum>* useForHorizontalDisplacement = nullptr;  // DisplacementMapSource
  Property<float>* maxHorizontalDisplacement = nullptr;
  Property<Enum>* useForVerticalDisplacement = nullptr;  // DisplacementMapSource
  Property<float>* maxVerticalDisplacement = nullptr;
  Property<Enum>* displacementMapBehavior = nullptr;  // DisplacementMapBehavior
  Property<bool>* edgeBehavior = nullptr;
  Property<bool>* expandOutput = nullptr;

  RTTR_ENABLE(Effect)
};

class PAG_API RadialBlurMode {
 public:
  static const Enum Spin = 0;
  static const Enum Zoom = 1;
};

class PAG_API RadialBlurAntialias {
 public:
  static const Enum Low = 0;
  static const Enum High = 1;
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
  Property<Enum>* mode = nullptr;
  Property<Enum>* antialias = nullptr;

  RTTR_ENABLE(Effect)
};

class PAG_API MosaicEffect : public Effect {
 public:
  ~MosaicEffect() override;

  EffectType type() const override {
    return EffectType::Mosaic;
  }

  bool processVisibleAreaOnly() const override {
    return true;
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

enum class LayerStyleType { Unknown, DropShadow, Stroke };

enum class LayerStylePosition { Above, Blow };

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

class PAG_API StrokePosition {
 public:
  static const Enum Outside = 0;
  static const Enum Inside = 1;
  static const Enum Center = 2;
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

  Property<Enum>* blendMode = nullptr;  // BlendMode
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

  Property<Enum>* blendMode = nullptr;  // BlendMode
  Property<Color>* color = nullptr;
  Property<float>* size = nullptr;
  Property<Opacity>* opacity = nullptr;
  Property<Enum>* position = nullptr;  // StrokePosition

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

class PAG_API AnchorPointGrouping {
 public:
  static const Enum Character = 0;
  static const Enum Word = 1;
  static const Enum Line = 2;
  static const Enum All = 3;
};

class PAG_API TextMoreOptions {
 public:
  ~TextMoreOptions();

  Enum anchorPointGrouping = AnchorPointGrouping::Character;
  Property<Point>* groupingAlignment = nullptr;  // multidimensional Percent

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const;

  bool verify() const;
};

class PAG_API TextRangeSelectorUnits {
 public:
  static const Enum Percentage = 0;
  static const Enum Index = 1;
};

class PAG_API TextSelectorBasedOn {
 public:
  static const Enum Characters = 0;
  static const Enum CharactersExcludingSpaces = 1;
  static const Enum Words = 2;
  static const Enum Lines = 3;
};

class PAG_API TextSelectorMode {
 public:
  static const Enum None = 0;
  static const Enum Add = 1;
  static const Enum Subtract = 2;
  static const Enum Intersect = 3;
  static const Enum Min = 4;
  static const Enum Max = 5;
  static const Enum Difference = 6;
};

class PAG_API TextRangeSelectorShape {
 public:
  static const Enum Square = 0;
  static const Enum RampUp = 1;
  static const Enum RampDown = 2;
  static const Enum Triangle = 3;
  static const Enum Round = 4;
  static const Enum Smooth = 5;
};

class PAG_API TextAnimatorTrackingType {
 public:
  static const Enum BeforeAndAfter = 0;
  static const Enum Before = 1;
  static const Enum After = 2;
};

class PAG_API TextSelectorType {
 public:
  static const Enum Range = 0;
  static const Enum Wiggly = 1;
  static const Enum Expression = 2;
};

class PAG_API TextSelector {
 public:
  virtual ~TextSelector() {
  }

  virtual Enum type() const = 0;

  virtual void excludeVaryingRanges(std::vector<TimeRange>*) const = 0;

  virtual bool verify() const = 0;
};

class PAG_API TextRangeSelector : public TextSelector {
 public:
  ~TextRangeSelector();

  Enum type() const override {
    return TextSelectorType::Range;
  };

  Property<Percent>* start = nullptr;
  Property<Percent>* end = nullptr;
  Property<float>* offset = nullptr;
  Enum units = TextRangeSelectorUnits::Percentage;
  Enum basedOn = TextSelectorBasedOn::Characters;
  Property<Enum>* mode = nullptr;
  Property<Percent>* amount = nullptr;
  Enum shape = TextRangeSelectorShape::Square;
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

  Enum type() const override {
    return TextSelectorType::Wiggly;
  }

  Property<Enum>* mode = nullptr;
  Property<Percent>* maxAmount = nullptr;
  Property<Percent>* minAmount = nullptr;
  Enum basedOn = TextSelectorBasedOn::Characters;
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

  Property<Enum>* trackingType = nullptr;  // TextAnimatorTrackingType
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

class PAG_API LineCap {
 public:
  static const Enum Butt = 0;
  static const Enum Round = 1;
  static const Enum Square = 2;
};

class PAG_API LineJoin {
 public:
  static const Enum Miter = 0;
  static const Enum Round = 1;
  static const Enum Bevel = 2;
};

class PAG_API FillRule {
 public:
  static const Enum NonZeroWinding = 0;
  static const Enum EvenOdd = 1;
};

class PAG_API GradientFillType {
 public:
  static const Enum Linear = 0;
  static const Enum Radial = 1;
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

enum class ShapeType {
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

  Enum blendMode = BlendMode::Normal;
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

class PAG_API PolyStarType {
 public:
  static const Enum Star = 0;
  static const Enum Polygon = 1;
};

class PAG_API PolyStarElement : public ShapeElement {
 public:
  ~PolyStarElement() override;

  ShapeType type() const override {
    return ShapeType::PolyStar;
  }

  bool reversed = false;
  Enum polyType = PolyStarType::Star;
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

class PAG_API CompositeOrder {
 public:
  static const Enum BelowPreviousInSameGroup = 0;
  static const Enum AbovePreviousInSameGroup = 1;
};

class PAG_API FillElement : public ShapeElement {
 public:
  ~FillElement() override;

  ShapeType type() const override {
    return ShapeType::Fill;
  }

  Enum blendMode = BlendMode::Normal;
  Enum composite = CompositeOrder::BelowPreviousInSameGroup;
  Enum fillRule = FillRule::NonZeroWinding;
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

  Enum blendMode = BlendMode::Normal;
  Enum composite = CompositeOrder::BelowPreviousInSameGroup;
  Property<Color>* color = nullptr;
  Property<Opacity>* opacity = nullptr;
  Property<float>* strokeWidth = nullptr;
  Enum lineCap = LineCap::Butt;
  Enum lineJoin = LineJoin::Miter;
  Property<float>* miterLimit = nullptr;
  Property<float>* dashOffset = nullptr;
  std::vector<Property<float>*> dashes;

  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;

  bool verify() const override;

  RTTR_ENABLE(ShapeElement)
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

class PAG_API GradientFillElement : public ShapeElement {
 public:
  ~GradientFillElement() override;

  ShapeType type() const override {
    return ShapeType::GradientFill;
  }

  Enum blendMode = BlendMode::Normal;
  Enum composite = CompositeOrder::BelowPreviousInSameGroup;
  Enum fillRule = FillRule::NonZeroWinding;
  Enum fillType = GradientFillType::Linear;
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

  Enum blendMode = BlendMode::Normal;
  Enum composite = CompositeOrder::BelowPreviousInSameGroup;
  Enum fillType = GradientFillType::Linear;
  Enum lineCap = LineCap::Butt;
  Enum lineJoin = LineJoin::Miter;
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

class PAG_API MergePathsMode {
 public:
  static const Enum Merge = 0;
  static const Enum Add = 1;
  static const Enum Subtract = 2;
  static const Enum Intersect = 3;
  static const Enum ExcludeIntersections = 4;
};

class PAG_API MergePathsElement : public ShapeElement {
 public:
  ShapeType type() const override {
    return ShapeType::MergePaths;
  }

  Enum mode = MergePathsMode::Add;

  RTTR_ENABLE(ShapeElement)
};

class PAG_API TrimPathsType {
 public:
  static const Enum Simultaneously = 0;
  static const Enum Individually = 1;
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
  Enum trimType = TrimPathsType::Simultaneously;

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

class PAG_API RepeaterOrder {
 public:
  static const Enum Below = 0;
  static const Enum Above = 1;
};

class PAG_API RepeaterElement : public ShapeElement {
 public:
  ~RepeaterElement() override;

  ShapeType type() const override {
    return ShapeType::Repeater;
  }

  Property<float>* copies = nullptr;
  Property<float>* offset = nullptr;
  Enum composite = RepeaterOrder::Below;
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

class Composition;

class VectorComposition;

class BitmapComposition;

class ImageBytes;

class PAG_API Cache {
 public:
  virtual ~Cache() = default;
};

class PAG_API TrackMatteType {
 public:
  static const Enum None = 0;
  static const Enum Alpha = 1;
  static const Enum AlphaInverted = 2;
  static const Enum Luma = 3;
  static const Enum LumaInverted = 4;
};

class PAG_API CachePolicy {
 public:
  static const Enum Auto = 0;
  static const Enum Enable = 1;
  static const Enum Disable = 2;
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
  Layer* parent = nullptr;  // layer reference

  VectorComposition* containingComposition = nullptr;  // composition reference

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
   * The transformation of the layer.
   */
  Transform2D* transform = nullptr;
  /**
   * When false, the layer should be skipped during the rendering loop.
   */
  bool isActive = true;
  /**
   * The blending mode of the layer.
   */
  Enum blendMode = BlendMode::Normal;
  /**
   * If layer has a track matte, specifies the way it is applied.
   */
  Enum trackMatteType = TrackMatteType::None;
  Layer* trackMatteLayer = nullptr;
  Property<float>* timeRemap = nullptr;
  std::vector<MaskData*> masks;
  std::vector<Effect*> effects;
  std::vector<LayerStyle*> layerStyles;
  std::vector<Marker*> markers;
  Enum cachePolicy = CachePolicy::Auto;

  Cache* cache = nullptr;
  std::mutex locker = {};

  virtual void excludeVaryingRanges(std::vector<TimeRange>* timeRanges);
  virtual bool verify() const;
  Point getMaxScaleFactor();
  TimeRange visibleRange();

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

  RTTR_ENABLE(Layer)
};

class PAG_API ImageFillRule {
 public:
  ~ImageFillRule();

  Enum scaleMode = PAGScaleMode::LetterBox;

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

  RTTR_ENABLE(Layer)
};

/**
 * Compositions are always one of the following types.
 */
enum class CompositionType { Unknown, Vector, Bitmap, Video };

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

  std::vector<TimeRange> staticTimeRanges;
  Cache* cache = nullptr;
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

  Cache* cache = nullptr;
  std::mutex locker = {};
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
  Composition* composition = nullptr;
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
  Enum timeStretchMode = PAGTimeStretchMode::Repeat;
  TimeRange scaledTimeRange = {};
  FileAttributes fileAttributes = {};
  std::string path = "";
  std::vector<ImageBytes*> images;
  std::vector<Composition*> compositions;

  std::vector<int>* editableImages = nullptr;
  std::vector<int>* editableTexts = nullptr;

 private:
  PreComposeLayer* rootLayer = nullptr;
  Composition* mainComposition = nullptr;
  uint16_t _tagLevel = 1;
  int _numLayers = 0;

  // Just references, no need to delete them.
  std::vector<TextLayer*> textLayers = {};

  // Just references, no need to delete them.
  std::vector<std::vector<ImageLayer*>> imageLayers = {};

  File(std::vector<Composition*> compositionList, std::vector<pag::ImageBytes*> imageList);
  void updateEditables(Composition* composition);

  friend class Codec;
};

/**
 * Calculate the memory cost by graphics of file in bytes.
 */
int64_t PAG_API CalculateGraphicsMemory(std::shared_ptr<File> file);

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
};
}  // namespace pag
