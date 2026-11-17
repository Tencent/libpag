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
#include "pagx/utils/RasterLayerParent.h"

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
 * Owns the prepared target-view wrapper across draws for a single PAGScene. It keeps the shared
 * parenting tool alive so repeated draws do not re-walk the tree, allocate a wrapper or toggle mask
 * ownership. Construction, preparation and drawing are separated so a scene can prepare once and
 * reuse the state until an edit, resource update or display change invalidates it.
 */
class OutputTargetState {
 public:
  OutputTargetState() = default;

  OutputTargetState(const OutputTargetState&) = delete;
  OutputTargetState& operator=(const OutputTargetState&) = delete;

  /**
   * Prepares the wrapper for the given root and target, checking that the target belongs to the
   * tree and enabling temporary release of an external mask owner. On failure the tree is left
   * unchanged and the state stays invalid.
   * @param root the root layer of the tree that owns the target.
   * @param target the layer to draw.
   * @return true when the wrapper is ready for drawing.
   */
  bool prepare(const std::shared_ptr<tgfx::Layer>& root,
               const std::shared_ptr<tgfx::Layer>& target);

  /**
   * Draws the target through the prepared wrapper. The target's own transform, alpha, visibility,
   * mask and scrollRect are applied exactly once, and the canvas matrix is scoped to this call.
   * @return false without drawing when the state is not prepared or the hookup was disturbed;
   *         true after a draw was issued (TGFX drawing itself does not report GPU failures).
   */
  bool draw(tgfx::Canvas* canvas);

  /**
   * Returns true when the wrapper is prepared and still occupies the target's original slot.
   */
  bool isPrepared() const;

  /**
   * Returns true when this state was prepared for the given root and target. Used to detect a
   * binding that changed without the owning scene invalidating the state first.
   */
  bool binds(const std::shared_ptr<tgfx::Layer>& root,
             const std::shared_ptr<tgfx::Layer>& target) const {
    return this->root == root && this->target == target;
  }

  /**
   * Restores the original tree and mask ownership and drops the prepared state. Idempotent.
   */
  void reset();

 private:
  RasterLayerParent parent = {};
  std::shared_ptr<tgfx::Layer> root = nullptr;
  std::shared_ptr<tgfx::Layer> target = nullptr;
};

/**
 * Draws only the given target layer onto the canvas in a single scoped preparation. The layer's
 * own transform, alpha, visibility, mask and scrollRect are applied exactly once through a
 * temporary parent, and backdrop-dependent layer styles sample the ancestors and prior siblings of
 * the target in the original tree while later siblings are excluded. A target that another layer
 * references as its mask is temporarily released from that ownership so the mask skip rule does not
 * drop it from the output. The original tree shape is restored before returning.
 * @param canvas the destination canvas in the target's parent coordinate space.
 * @param root the root layer of the tree that owns the target; used as the coordinate space.
 * @param targetLayer the layer to draw.
 * @return false without drawing when the arguments are null, the target is not in the tree, or the
 *         temporary parent could not be prepared; true after a draw was issued.
 */
bool DrawOutputTarget(tgfx::Canvas* canvas, const std::shared_ptr<tgfx::Layer>& root,
                      const std::shared_ptr<tgfx::Layer>& targetLayer);

}  // namespace pagx
