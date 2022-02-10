///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  The MIT License (MIT)
//
//  Copyright (c) 2016-present, Tencent. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
//  and associated documentation files (the "Software"), to deal in the Software without
//  restriction, including without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//      The above copyright notice and this permission notice shall be included in all copies or
//      substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include "gpu/GradientShader.h"
#include "base/utils/MathExtra.h"
#include "gpu/ConstColorProcessor.h"
#include "gpu/GradientCache.h"
#include "gpu/gradients/ClampedGradientEffect.h"
#include "gpu/gradients/DualIntervalGradientColorizer.h"
#include "gpu/gradients/LinearGradientLayout.h"
#include "gpu/gradients/RadialGradientLayout.h"
#include "gpu/gradients/SingleIntervalGradientColorizer.h"
#include "gpu/gradients/TextureGradientColorizer.h"
#include "gpu/gradients/UnrolledBinaryGradientColorizer.h"

namespace pag {
// Intervals smaller than this (that aren't hard stops) on low-precision-only devices force us to
// use the textured gradient
static constexpr float kLowPrecisionIntervalLimit = 0.01f;

// Analyze the shader's color stops and positions and chooses an appropriate colorizer to represent
// the gradient.
static std::unique_ptr<FragmentProcessor> MakeColorizer(Context* context, const Color4f* colors,
                                                        const float* positions, int count) {
  // If there are hard stops at the beginning or end, the first and/or last color should be
  // ignored by the colorizer since it should only be used in a clamped border color. By detecting
  // and removing these stops at the beginning, it makes optimizing the remaining color stops
  // simpler.
  bool bottomHardStop = FloatNearlyEqual(positions[0], positions[1]);
  bool topHardStop = FloatNearlyEqual(positions[count - 2], positions[count - 1]);
  int offset = 0;
  if (bottomHardStop) {
    offset += 1;
    count--;
  }
  if (topHardStop) {
    count--;
  }

  // Two remaining colors means a single interval from 0 to 1
  // (but it may have originally been a 3 or 4 color gradient with 1-2 hard stops at the ends)
  if (count == 2) {
    return SingleIntervalGradientColorizer::Make(colors[offset], colors[offset + 1]);
  }

  bool tryAnalyticColorizer = count <= UnrolledBinaryGradientColorizer::kMaxColorCount;

  // The remaining analytic colorizes use scale*t+bias, and the scale/bias values can become
  // quite large when thresholds are close (but still outside the hard stop limit). If float isn't
  // 32-bit, output can be incorrect if the thresholds are too close together. However, the
  // analytic shaders are higher quality, so they can be used with lower precision hardware when
  // the thresholds are not ill-conditioned.
  if (!context->caps()->floatIs32Bits && tryAnalyticColorizer) {
    // Could run into problems, check if thresholds are close together (with a limit of .01, so
    // that scales will be less than 100, which leaves 4 decimals of precision on 16-bit).
    for (int i = offset; i < count - 1; i++) {
      auto delta = std::abs(positions[i] - positions[i + 1]);
      if (delta <= kLowPrecisionIntervalLimit && delta > FLOAT_NEARLY_ZERO) {
        tryAnalyticColorizer = false;
        break;
      }
    }
  }
  if (tryAnalyticColorizer) {
    if (count == 3) {
      // Must be a dual interval gradient, where the middle point is at offset+1 and the two
      // intervals share the middle color stop.
      return DualIntervalGradientColorizer::Make(colors[offset], colors[offset + 1],
                                                 colors[offset + 1], colors[offset + 2],
                                                 positions[offset + 1]);
    } else if (count == 4 && FloatNearlyEqual(positions[offset + 1], positions[offset + 2])) {
      // Two separate intervals that join at the same threshold position
      return DualIntervalGradientColorizer::Make(colors[offset], colors[offset + 1],
                                                 colors[offset + 2], colors[offset + 3],
                                                 positions[offset + 1]);
    }

    // The single and dual intervals are a specialized case of the unrolled binary search
    // colorizer which can analytically render gradients of up to 8 intervals (up to 9 or 16
    // colors depending on how many hard stops are inserted).
    auto unrolled =
        UnrolledBinaryGradientColorizer::Make(colors + offset, positions + offset, count);
    if (unrolled) {
      return unrolled;
    }
  }

  // Otherwise, fall back to a raster gradient sample by a texture, which can handle
  // arbitrary gradients (the only downside being sampling resolution).
  return TextureGradientColorizer::Make(
      context->getGradient(colors + offset, positions + offset, count));
}

// Combines the colorizer and layout with an appropriately configured master effect based on the
// gradient's tile mode
static std::unique_ptr<FragmentProcessor> MakeGradient(Context* context,
                                                       const GradientShaderBase& shader,
                                                       std::unique_ptr<FragmentProcessor> layout) {
  if (layout == nullptr) {
    return nullptr;
  }
  bool allOpaque = true;
  for (const auto& color : shader.originalColors) {
    if (!FloatNearlyEqual(color.a, 1.0)) {
      allOpaque = false;
      break;
    }
  }
  // All gradients are colorized the same way, regardless of layout
  std::unique_ptr<FragmentProcessor> colorizer =
      MakeColorizer(context, &(shader.originalColors[0]), &(shader.originalPositions[0]),
                    static_cast<int>(shader.originalColors.size()));
  if (colorizer == nullptr) {
    return nullptr;
  }

  // The master effect has to export premultiply colors, but under certain conditions it doesn't
  // need to do anything to achieve that: i.e. all the colors have a = 1, in which case
  // premultiply is a no op.
  return ClampedGradientEffect::Make(
      std::move(colorizer), std::move(layout), shader.originalColors[0],
      shader.originalColors[shader.originalColors.size() - 1], !allOpaque);
}

std::unique_ptr<FragmentProcessor> Color4Shader::asFragmentProcessor(const FPArgs&) const {
  return ConstColorProcessor::Make(color);
}

GradientShaderBase::GradientShaderBase(const std::vector<Color4f>& colors,
                                       const std::vector<float>& positions,
                                       const Matrix& pointsToUnit)
    : pointsToUnit(pointsToUnit) {
  auto dummyFirst = false;
  auto dummyLast = false;
  if (!positions.empty()) {
    dummyFirst = positions[0] != 0;
    dummyLast = positions[positions.size() - 1] != 1.0f;
  }

  // Now copy over the colors, adding the dummies as needed
  if (dummyFirst) {
    originalColors.push_back(colors[0]);
  }
  originalColors.insert(originalColors.end(), colors.begin(), colors.end());
  if (dummyLast) {
    originalColors.push_back(colors[colors.size() - 1]);
  }
  if (positions.empty()) {
    auto posScale = 1.0f / static_cast<float>(positions.size() - 1);
    for (size_t i = 0; i < positions.size(); i++) {
      originalPositions.push_back(static_cast<float>(i) * posScale);
    }
  } else {
    float prev = 0;
    originalPositions.push_back(prev);  // force the first pos to 0
    for (size_t i = dummyFirst ? 0 : 1; i < colors.size() + dummyLast; ++i) {
      // Pin the last value to 1.0, and make sure position is monotonic.
      float curr;
      if (i != colors.size()) {
        curr = std::max(std::min(positions[i], 1.0f), prev);
      } else {
        curr = 1.0f;
      }
      originalPositions.push_back(curr);
      prev = curr;
    }
  }
}

static Matrix PointsToUnitMatrix(const Point& startPoint, const Point& endPoint) {
  Point vec = {endPoint.x - startPoint.x, endPoint.y - startPoint.y};
  float mag = Point::Length(vec.x, vec.y);
  float inv = mag != 0 ? 1 / mag : 0;
  vec.set(vec.x * inv, vec.y * inv);
  auto matrix = Matrix::I();
  matrix.setSinCos(-vec.y, vec.x, startPoint.x, startPoint.y);
  matrix.postTranslate(-startPoint.x, -startPoint.y);
  matrix.postScale(inv, inv);
  return matrix;
}

LinearGradient::LinearGradient(const Point& startPoint, const Point& endPoint,
                               const std::vector<Color4f>& colors,
                               const std::vector<float>& positions)
    : GradientShaderBase(colors, positions, PointsToUnitMatrix(startPoint, endPoint)) {
}

std::unique_ptr<FragmentProcessor> LinearGradient::asFragmentProcessor(const FPArgs& args) const {
  auto matrix = args.localMatrix;
  matrix.postConcat(pointsToUnit);
  return MakeGradient(args.context, *this, LinearGradientLayout::Make(matrix));
}

static Matrix RadialToUnitMatrix(const Point& center, float radius) {
  float inv = 1 / radius;
  auto matrix = Matrix::MakeTrans(-center.x, -center.y);
  matrix.postScale(inv, inv);
  return matrix;
}

RadialGradient::RadialGradient(const Point& center, float radius,
                               const std::vector<Color4f>& colors,
                               const std::vector<float>& positions)
    : GradientShaderBase(colors, positions, RadialToUnitMatrix(center, radius)) {
}

std::unique_ptr<FragmentProcessor> RadialGradient::asFragmentProcessor(const FPArgs& args) const {
  auto matrix = args.localMatrix;
  matrix.postConcat(pointsToUnit);
  return MakeGradient(args.context, *this, RadialGradientLayout::Make(matrix));
}

// The default SkScalarNearlyZero threshold of .0024 is too big and causes regressions for svg
// gradients defined in the wild.
static constexpr float kDegenerateThreshold = 1.0f / (1 << 15);

std::unique_ptr<Shader> GradientShader::MakeLinear(const Point& startPoint, const Point& endPoint,
                                                   const std::vector<Color4f>& colors,
                                                   const std::vector<float>& positions) {
  if (!std::isfinite(Point::Distance(endPoint, startPoint))) {
    return nullptr;
  }
  if (colors.empty()) {
    return nullptr;
  }
  if (1 == colors.size()) {
    return std::make_unique<Color4Shader>(colors[0]);
  }
  if (FloatNearlyZero((endPoint - startPoint).length(), kDegenerateThreshold)) {
    // Degenerate gradient, the only tricky complication is when in clamp mode, the limit of
    // the gradient approaches two half planes of solid color (first and last). However, they
    // are divided by the line perpendicular to the start and end point, which becomes undefined
    // once start and end are exactly the same, so just use the end color for a stable solution.
    return std::make_unique<Color4Shader>(colors[0]);
  }
  return std::make_unique<LinearGradient>(startPoint, endPoint, colors, positions);
}

std::unique_ptr<Shader> GradientShader::MakeRadial(const Point& center, float radius,
                                                   const std::vector<Color4f>& colors,
                                                   const std::vector<float>& positions) {
  if (radius < 0) {
    return nullptr;
  }
  if (colors.empty()) {
    return nullptr;
  }
  if (1 == colors.size()) {
    return std::make_unique<Color4Shader>(colors[0]);
  }

  if (FloatNearlyZero(radius, kDegenerateThreshold)) {
    // Degenerate gradient optimization, and no special logic needed for clamped radial gradient
    return std::make_unique<Color4Shader>(colors[colors.size() - 1]);
  }
  return std::make_unique<RadialGradient>(center, radius, colors, positions);
}

std::unique_ptr<Shader> Shader::MakeColorShader(Color color, Opacity opacity) {
  return std::make_unique<Color4Shader>(
      Color4f{static_cast<float>(color.red) / 255.0f, static_cast<float>(color.green) / 255.0f,
              static_cast<float>(color.blue) / 255.0f, static_cast<float>(opacity) / 255.0f});
}
}  // namespace pag
