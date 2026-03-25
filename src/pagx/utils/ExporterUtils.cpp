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

#include "pagx/utils/ExporterUtils.h"
#include <cmath>
#include "base/utils/MathUtil.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Text.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents) {
  FillStrokeInfo info = {};
  for (const auto* element : contents) {
    if (element->nodeType() == NodeType::Fill && !info.fill) {
      info.fill = static_cast<const Fill*>(element);
    } else if (element->nodeType() == NodeType::Stroke && !info.stroke) {
      info.stroke = static_cast<const Stroke*>(element);
    } else if (element->nodeType() == NodeType::TextBox && !info.textBox) {
      info.textBox = static_cast<const TextBox*>(element);
    }
    if (info.fill && info.stroke && info.textBox) {
      break;
    }
  }
  return info;
}

Matrix BuildLayerMatrix(const Layer* layer) {
  Matrix m = layer->matrix;
  if (layer->x != 0.0f || layer->y != 0.0f) {
    m = Matrix::Translate(layer->x, layer->y) * m;
  }
  return m;
}

Matrix BuildGroupMatrix(const Group* group) {
  bool hasAnchor = !FloatNearlyZero(group->anchor.x) || !FloatNearlyZero(group->anchor.y);
  bool hasPosition = !FloatNearlyZero(group->position.x) || !FloatNearlyZero(group->position.y);
  bool hasRotation = !FloatNearlyZero(group->rotation);
  bool hasScale =
      !FloatNearlyZero(group->scale.x - 1.0f) || !FloatNearlyZero(group->scale.y - 1.0f);
  bool hasSkew = !FloatNearlyZero(group->skew);

  if (!hasAnchor && !hasPosition && !hasRotation && !hasScale && !hasSkew) {
    return {};
  }

  Matrix m = {};
  if (hasAnchor) {
    m = Matrix::Translate(-group->anchor.x, -group->anchor.y);
  }
  if (hasScale) {
    m = Matrix::Scale(group->scale.x, group->scale.y) * m;
  }
  if (hasSkew) {
    m = Matrix::Rotate(group->skewAxis) * m;
    Matrix shear = {};
    shear.c = std::tan(DegreesToRadians(group->skew));
    m = shear * m;
    m = Matrix::Rotate(-group->skewAxis) * m;
  }
  if (hasRotation) {
    m = Matrix::Rotate(group->rotation) * m;
  }
  if (hasPosition) {
    m = Matrix::Translate(group->position.x, group->position.y) * m;
  }

  return m;
}

std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY) {
  std::vector<GlyphPath> result;
  for (const auto* run : text.glyphRuns) {
    if (!run->font || run->glyphs.empty()) {
      continue;
    }
    float scale = run->fontSize / static_cast<float>(run->font->unitsPerEm);
    float currentX = textPosX + run->x;
    for (size_t i = 0; i < run->glyphs.size(); i++) {
      uint16_t glyphID = run->glyphs[i];
      if (glyphID == 0) {
        continue;
      }
      auto glyphIndex = static_cast<size_t>(glyphID) - 1;
      if (glyphIndex >= run->font->glyphs.size()) {
        continue;
      }
      auto* glyph = run->font->glyphs[glyphIndex];
      if (!glyph || !glyph->path || glyph->path->isEmpty()) {
        continue;
      }

      float posX = 0;
      float posY = 0;
      if (i < run->positions.size()) {
        posX = textPosX + run->x + run->positions[i].x;
        posY = textPosY + run->y + run->positions[i].y;
        if (i < run->xOffsets.size()) {
          posX += run->xOffsets[i];
        }
      } else if (i < run->xOffsets.size()) {
        posX = textPosX + run->x + run->xOffsets[i];
        posY = textPosY + run->y;
      } else {
        posX = currentX;
        posY = textPosY + run->y;
      }
      currentX += glyph->advance * scale;

      Matrix glyphMatrix = Matrix::Translate(posX, posY) * Matrix::Scale(scale, scale);

      bool hasRotation = i < run->rotations.size() && run->rotations[i] != 0;
      bool hasGlyphScale =
          i < run->scales.size() && (run->scales[i].x != 1 || run->scales[i].y != 1);
      bool hasSkew = i < run->skews.size() && run->skews[i] != 0;

      if (hasRotation || hasGlyphScale || hasSkew) {
        float anchorX = glyph->advance * 0.5f;
        float anchorY = 0;
        if (i < run->anchors.size()) {
          anchorX += run->anchors[i].x;
          anchorY += run->anchors[i].y;
        }

        Matrix perGlyph = Matrix::Translate(-anchorX, -anchorY);
        if (hasGlyphScale) {
          perGlyph = Matrix::Scale(run->scales[i].x, run->scales[i].y) * perGlyph;
        }
        if (hasSkew) {
          Matrix shear = {};
          shear.c = std::tan(pag::DegreesToRadians(run->skews[i]));
          perGlyph = shear * perGlyph;
        }
        if (hasRotation) {
          perGlyph = Matrix::Rotate(run->rotations[i]) * perGlyph;
        }
        perGlyph = Matrix::Translate(anchorX, anchorY) * perGlyph;
        glyphMatrix = glyphMatrix * perGlyph;
      }

      result.push_back({glyphMatrix, glyph->path});
    }
  }
  return result;
}

}  // namespace pagx
