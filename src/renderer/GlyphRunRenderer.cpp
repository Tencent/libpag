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
#include "tgfx/core/TextBlobBuilder.h"

namespace pagx {

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

    int unitsPerEm = (run->font != nullptr) ? run->font->unitsPerEm : 1;
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

    bool hasTransforms = !run->scales.empty() || !run->rotations.empty() || !run->skews.empty();

    if (hasTransforms) {
      // Matrix mode: build a full affine transform per glyph, then apply inverse matrix.
      // allocRunMatrix uses 6 floats per glyph: [scaleX, skewX, transX, skewY, scaleY, transY].
      auto& buffer = builder.allocRunMatrix(font, count);
      memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
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
        matrix.preConcat(inverseMatrix);

        // Write 6-float affine matrix
        size_t offset = i * 6;
        buffer.positions[offset + 0] = matrix.getScaleX();
        buffer.positions[offset + 1] = matrix.getSkewX();
        buffer.positions[offset + 2] = matrix.getTranslateX();
        buffer.positions[offset + 3] = matrix.getSkewY();
        buffer.positions[offset + 4] = matrix.getScaleY();
        buffer.positions[offset + 5] = matrix.getTranslateY();
      }
    } else if (!run->positions.empty() && run->positions.size() >= count) {
      // Point mode: apply inverse matrix to each position.
      auto& buffer = builder.allocRunPos(font, count);
      memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
      auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
      for (size_t i = 0; i < count; i++) {
        float posX = run->x + run->positions[i].x;
        float posY = run->y + run->positions[i].y;
        if (i < run->xOffsets.size()) {
          posX += run->xOffsets[i];
        }
        auto pt = tgfx::Point::Make(posX, posY);
        inverseMatrix.mapPoints(&pt, 1);
        positions[i] = pt;
      }
    } else if (!run->xOffsets.empty() && run->xOffsets.size() >= count) {
      // Horizontal mode: convert to Point mode for inverse matrix application.
      auto& buffer = builder.allocRunPos(font, count);
      memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
      auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
      for (size_t i = 0; i < count; i++) {
        auto pt = tgfx::Point::Make(run->x + run->xOffsets[i], run->y);
        inverseMatrix.mapPoints(&pt, 1);
        positions[i] = pt;
      }
    } else {
      // Default mode: compute positions from font advances, then apply inverse matrix.
      auto& buffer = builder.allocRunPos(font, count);
      memcpy(buffer.glyphs, run->glyphs.data(), count * sizeof(tgfx::GlyphID));
      auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
      float currentX = run->x;
      for (size_t i = 0; i < count; i++) {
        auto pt = tgfx::Point::Make(currentX, run->y);
        inverseMatrix.mapPoints(&pt, 1);
        positions[i] = pt;
        currentX += font.getAdvance(run->glyphs[i]);
      }
    }
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
    // Ensure positions match glyphs count; skip malformed runs.
    if (run.positions.size() < run.glyphs.size()) {
      continue;
    }
    size_t count = run.glyphs.size();
    bool hasXforms = !run.xforms.empty();

    if (hasXforms) {
      // RSXform mode: apply inverse matrix to RSXform translation only.
      auto& buffer = builder.allocRunRSXform(run.font, count);
      for (size_t i = 0; i < count; i++) {
        buffer.glyphs[i] = run.glyphs[i];
      }
      auto* xforms = reinterpret_cast<tgfx::RSXform*>(buffer.positions);
      for (size_t i = 0; i < count; i++) {
        auto xf = run.xforms[i];
        auto pt = tgfx::Point::Make(xf.tx, xf.ty);
        inverseMatrix.mapPoints(&pt, 1);
        xforms[i] = tgfx::RSXform::Make(xf.scos, xf.ssin, pt.x, pt.y);
      }
    } else {
      // Horizontal / upright vertical glyphs: simple Point mode with inverse matrix.
      auto& buffer = builder.allocRunPos(run.font, count);
      for (size_t i = 0; i < count; i++) {
        buffer.glyphs[i] = run.glyphs[i];
      }
      auto* positions = reinterpret_cast<tgfx::Point*>(buffer.positions);
      for (size_t i = 0; i < count; i++) {
        auto pt = (i < run.positions.size()) ? run.positions[i] : tgfx::Point::Zero();
        inverseMatrix.mapPoints(&pt, 1);
        positions[i] = pt;
      }
    }
  }

  shapedText.textBlob = builder.build();
  return shapedText;
}

}  // namespace pagx
