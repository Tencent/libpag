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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"

namespace tgfx {
class Canvas;
class Layer;
}  // namespace tgfx

namespace pagx {

class Layer;

/**
 * Grants the importer, exporter and resolver access to the raw target-view attributes kept on
 * PAGXDocument. Other consumers must go through ResolveOutputView()/HasDeclaredOutputView().
 */
class OutputViewDeclarationAccess {
 public:
  static std::optional<std::string> DeclaredTargetLayer(const PAGXDocument& document);
  static std::optional<std::string> DeclaredBounds(const PAGXDocument& document);
  static void SetDeclared(PAGXDocument* document, std::optional<std::string> targetLayer,
                          std::optional<std::string> bounds);
};

/**
 * Validates the document's target view fields and resolves the referenced Layer.
 * @param document the document to inspect. May be null, in which case the result is None.
 * @param targetLayer optional out parameter that receives the resolved layer when the result is
 *        Valid. The pointer is owned by the document.
 * @param error optional out parameter that receives a human-readable diagnostic when the result
 *        is Invalid.
 * @return None when neither field is present, Valid when both fields resolve to a unique Layer,
 *         or Invalid with a diagnostic when the fields are inconsistent or the target cannot be
 *         resolved unambiguously.
 */
OutputViewStatus ResolveOutputView(const PAGXDocument* document, const Layer** targetLayer,
                                   std::string* error);

/**
 * Returns true when the document declares target view fields at all, including invalid ones. Used
 * by consumers that cannot honor the view to reject the document explicitly instead of silently
 * exporting the whole canvas.
 * @param document the document to inspect.
 */
bool HasDeclaredOutputView(const PAGXDocument& document);

/**
 * Draws only the given target layer onto the canvas. The layer's own transform, alpha,
 * visibility, mask and scrollRect are applied exactly once through a temporary parent, and
 * backdrop-dependent layer styles sample the ancestors and prior siblings of the target in the
 * original tree while later siblings are excluded. A target that another layer references as its
 * mask is temporarily released from that ownership so the mask skip rule does not drop it from the
 * output. The original tree shape is restored before returning.
 * @param canvas the destination canvas in the target's parent coordinate space.
 * @param root the root layer of the tree that owns the target; used as the coordinate space.
 * @param targetLayer the layer to draw.
 */
void DrawOutputTarget(tgfx::Canvas* canvas, const std::shared_ptr<tgfx::Layer>& root,
                      const std::shared_ptr<tgfx::Layer>& targetLayer);

}  // namespace pagx
