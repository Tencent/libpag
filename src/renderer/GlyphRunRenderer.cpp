/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "GlyphRunRenderer.h"
#include <cmath>
#include "pagx/ShapedText.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Text.h"
#include "pagx/utils/Base64.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/RSXform.h"
#include "tgfx/core/TextBlobBuilder.h"

namespace pagx {

// Returns true if the matrix can be represented as an RSXform (uniform scale + rotation +
// translation), i.e. the matrix has the form [scos, -ssin, tx; ssin, scos, ty; 0, 0, 1].
static bool IsRSXformCompatible(const tgfx::Matrix& matrix) {
  return matrix.getScaleX() == matrix.getScaleY() && matrix.getSkewX() == -matrix.getSkewY();
}

// Positioning modes for TextBlob runs, ordered from most compact to most general.
enum class GlyphPositionMode { Point, RSXform, Matrix };

// Returns the minimum positioning mode that can represent the given matrix.
static GlyphPositionMode ClassifyMatrix(const tgfx::Matrix& matrix) {
  if (matrix.isTranslate()) {
    return GlyphPositionMode::Point;
  }
  if (IsRSXformCompatible(matrix)) {
    return GlyphPositionMode::RSXform;
  }
  return GlyphPositionMode::Matrix;
}

// Promotes the mode to the more general of the two.
static GlyphPositionMode PromoteMode(GlyphPositionMode a, GlyphPositionMode b) {
  return a > b ? a : b;
}

static void WriteGlyphsWithMode(tgfx::TextBlobBuilder& builder, const tgfx::Font& font,
                                const tgfx::GlyphID* glyphs, size_t count,
                                const std::vector<tgfx::Matrix>& glyphMatrices,
                                GlyphPositionMode mode) {
  if (mode == GlyphPositionMode::Matrix) {
    auto& buffer = builder.allocRunMatrix(font, count);
    memcpy(buffer.glyphs, glyphs, count * sizeof(tgfx::GlyphID));
    for (size_t i = 0; i < count; i++) {
      auto& m = glyphMatrices[i];
      size_t offset = i * 6;
      buffer.positions[offset + 0] = m.getScaleX();
      buffer.positions[offset + 1] = m.getSkewX();
      buffer.positions[offset + 2] = m.getTranslateX();
      buffer.positions[offset + 3] = m.getSkewY();
      buffer.positions[offset + 4] = m.getScaleY();
      buffer.positions[offset + 5] = m.getTranslateY();
    }
  } else if (mode == GlyphPositionMode::RSXform) {
    auto& buffer = builder.allocRunRSXform(font, count);
    memcpy(buffer.glyphs, glyphs, count * sizeof(tgfx::GlyphID));
    auto* xforms = reinterpret_cast<tgfx::RSXform*>(buffer.positions);
    for (size_t i = 0; i < count; i++) {
      auto& m = glyphMatrices[i];
      xforms[i] =
          tgfx::RSXform::Make(m.getScaleX(), m.getSkewY(), m.getTranslateX(), m.getTranslateY());
    }
  } else {
    auto& buffer = builder.allocRunPos(font, count);
    memcpy(buffer.glyphs, glyphs, count * sizeof(tgfx::GlyphID));
    auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
    for (size_t i = 0; i < count; i++) {
      positions[i] =
          tgfx::Point::Make(glyphMatrices[i].getTranslateX(), glyphMatrices[i].getTranslateY());
    }
  }
}

std::shared_ptr<tgfx::Typeface> GlyphRunRenderer::BuildTypefaceFromFont(const Font* fontNode) {
  if (fontNode == nullptr || fontNode->glyphs.empty()) {
    return nullptr;
  }

  bool hasPath = false;
  bool hasImage = false;
  for (const auto& glyph : fontNode->glyphs) {
    if (glyph->path != nullptr) {
      hasPath = true;
    }
    if (glyph->image != nullptr) {
      hasImage = true;
    }
  }

  std::shared_ptr<tgfx::Typeface> typeface = nullptr;
  if (hasPath && !hasImage) {
    tgfx::PathTypefaceBuilder builder;
    for (const auto& glyph : fontNode->glyphs) {
      if (glyph->path != nullptr) {
        auto path = ToTGFX(*glyph->path);
        if (glyph->offset.x != 0 || glyph->offset.y != 0) {
          path.transform(tgfx::Matrix::MakeTrans(glyph->offset.x, glyph->offset.y));
        }
        builder.addGlyph(path, glyph->advance);
      } else {
        builder.addGlyph(tgfx::Path(), glyph->advance);
      }
    }
    typeface = builder.detach();
  } else if (hasImage && !hasPath) {
    tgfx::ImageTypefaceBuilder builder;
    for (const auto& glyph : fontNode->glyphs) {
      if (glyph->image != nullptr) {
        std::shared_ptr<tgfx::ImageCodec> codec = nullptr;
        auto imageNode = glyph->image;
        if (imageNode->data != nullptr) {
          codec = tgfx::ImageCodec::MakeFrom(ToTGFXData(imageNode->data));
        } else if (imageNode->filePath.find("data:") == 0) {
          auto data = DecodeBase64DataURI(imageNode->filePath);
          if (data) {
            codec = tgfx::ImageCodec::MakeFrom(ToTGFXData(data));
          }
        } else if (!imageNode->filePath.empty()) {
          codec = tgfx::ImageCodec::MakeFrom(imageNode->filePath);
        }
        if (codec) {
          builder.addGlyph(codec, ToTGFX(glyph->offset), glyph->advance);
        }
      }
    }
    typeface = builder.detach();
  }

  return typeface;
}

ShapedText GlyphRunRenderer::BuildTextBlob(const Text* text, const tgfx::Matrix& inverseMatrix) {
  ShapedText shapedText = {};
  tgfx::TextBlobBuilder builder = {};

  for (const auto& run : text->glyphRuns) {
    if (run->glyphs.empty()) {
      continue;
    }

    auto typeface = BuildTypefaceFromFont(run->font);
    if (typeface == nullptr) {
      continue;
    }

    int unitsPerEm = run->font->unitsPerEm;
    if (unitsPerEm <= 0) {
      unitsPerEm = 1;
    }
    float fontSizeForTypeface = run->fontSize / static_cast<float>(unitsPerEm);
    tgfx::Font font(typeface, fontSizeForTypeface);
    font.setFauxBold(text->fauxBold);
    font.setFauxItalic(text->fauxItalic);
    size_t count = run->glyphs.size();

    // Collect anchors for each glyph in this run.
    if (!run->anchors.empty()) {
      for (size_t i = 0; i < count; i++) {
        if (i < run->anchors.size()) {
          shapedText.anchors.push_back(tgfx::Point::Make(run->anchors[i].x, run->anchors[i].y));
        } else {
          shapedText.anchors.push_back(tgfx::Point::Zero());
        }
      }
    }

    // Step 1: Compute the final matrix for each glyph (source transform × inverseMatrix).
    bool hasTransforms = !run->scales.empty() || !run->rotations.empty() || !run->skews.empty();
    std::vector<tgfx::Matrix> glyphMatrices(count);
    float currentX = run->x;
    for (size_t i = 0; i < count; i++) {
      float posX = 0;
      float posY = 0;
      if (i < run->positions.size()) {
        posX = run->x + run->positions[i].x;
        posY = run->y + run->positions[i].y;
        if (i < run->xOffsets.size()) {
          posX += run->xOffsets[i];
        }
      } else if (i < run->xOffsets.size()) {
        posX = run->x + run->xOffsets[i];
        posY = run->y;
      } else {
        posX = currentX;
        posY = run->y;
        currentX += font.getAdvance(run->glyphs[i]);
      }

      if (hasTransforms) {
        float sx = (i < run->scales.size()) ? run->scales[i].x : 1.0f;
        float sy = (i < run->scales.size()) ? run->scales[i].y : 1.0f;
        float rotation = (i < run->rotations.size()) ? run->rotations[i] : 0.0f;
        float skew = (i < run->skews.size()) ? run->skews[i] : 0.0f;

        float anchorX = font.getAdvance(run->glyphs[i]) * 0.5f;
        float anchorY = 0.0f;
        if (i < run->anchors.size()) {
          anchorX += run->anchors[i].x;
          anchorY += run->anchors[i].y;
        }

        auto matrix = tgfx::Matrix::I();
        matrix.preTranslate(-anchorX, -anchorY);
        if (sx != 1.0f || sy != 1.0f) {
          matrix.preScale(sx, sy);
        }
        if (skew != 0.0f) {
          float skewRadians = skew * static_cast<float>(M_PI) / 180.0f;
          auto skewMatrix = tgfx::Matrix::I();
          skewMatrix.setSkewX(-std::tan(skewRadians));
          matrix.preConcat(skewMatrix);
        }
        if (rotation != 0.0f) {
          matrix.preRotate(rotation);
        }
        matrix.preTranslate(anchorX, anchorY);
        matrix.preTranslate(posX, posY);
        matrix.postConcat(inverseMatrix);
        glyphMatrices[i] = matrix;
      } else {
        auto matrix = tgfx::Matrix::MakeTrans(posX, posY);
        matrix.postConcat(inverseMatrix);
        glyphMatrices[i] = matrix;
      }
    }

    // Step 2: Classify all glyph matrices to find the most compact positioning mode.
    auto mode = GlyphPositionMode::Point;
    for (size_t i = 0; i < count; i++) {
      mode = PromoteMode(mode, ClassifyMatrix(glyphMatrices[i]));
      if (mode == GlyphPositionMode::Matrix) {
        break;
      }
    }

    // Step 3: Write glyphs into the TextBlob with the determined mode.
    WriteGlyphsWithMode(builder, font, run->glyphs.data(), count, glyphMatrices, mode);
  }

  shapedText.textBlob = builder.build();
  return shapedText;
}

ShapedText GlyphRunRenderer::BuildTextBlobFromLayoutRuns(
    const std::vector<TextLayoutGlyphRun>& runs, const tgfx::Matrix& inverseMatrix) {
  ShapedText shapedText = {};
  tgfx::TextBlobBuilder builder = {};

  for (auto& run : runs) {
    if (run.glyphs.empty() || run.font.getTypeface() == nullptr) {
      continue;
    }
    if (run.positions.size() < run.glyphs.size()) {
      continue;
    }
    size_t count = run.glyphs.size();
    bool hasXforms = !run.xforms.empty() && run.xforms.size() >= count;

    // Step 1: Compute the final matrix for each glyph.
    std::vector<tgfx::Matrix> glyphMatrices(count);
    for (size_t i = 0; i < count; i++) {
      tgfx::Matrix matrix;
      if (hasXforms) {
        auto& xf = run.xforms[i];
        matrix.setAll(xf.scos, -xf.ssin, xf.tx, xf.ssin, xf.scos, xf.ty);
      } else {
        auto& pos = run.positions[i];
        matrix = tgfx::Matrix::MakeTrans(pos.x, pos.y);
      }
      matrix.postConcat(inverseMatrix);
      glyphMatrices[i] = matrix;
    }

    // Step 2: Classify all glyph matrices to find the most compact positioning mode.
    auto mode = GlyphPositionMode::Point;
    for (size_t i = 0; i < count; i++) {
      mode = PromoteMode(mode, ClassifyMatrix(glyphMatrices[i]));
      if (mode == GlyphPositionMode::Matrix) {
        break;
      }
    }

    // Step 3: Write glyphs into the TextBlob with the determined mode.
    WriteGlyphsWithMode(builder, run.font, run.glyphs.data(), count, glyphMatrices, mode);
  }

  shapedText.textBlob = builder.build();
  return shapedText;
}

}  // namespace pagx
