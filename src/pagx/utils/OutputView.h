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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/utils/RasterLayerParent.h"

namespace tgfx {
class Canvas;
class Layer;
struct Point;
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
 * Resolves the Layer referenced by the document's parsed `targetLayerId` without checking whether
 * the whole output view is supported at runtime. Reference validity covers only the ID syntax,
 * node existence, id uniqueness, Layer type and unique reachability from the document layer tree;
 * outputBounds, animations, state machines and data binds are deliberately ignored so consumers
 * such as the optimizer can keep the declared target even when the view itself is unsupported.
 * @param document the document to inspect. May be null.
 * @param error optional out parameter that receives a diagnostic when resolution fails.
 * @return the resolved Layer owned by the document, or nullptr when the id is missing or cannot be
 *         resolved unambiguously.
 */
const Layer* ResolveOutputTargetLayer(const PAGXDocument* document, std::string* error);

/**
 * Returns true when the document declares target view fields at all, including invalid ones. Used
 * by consumers that cannot honor the view to reject the document explicitly instead of silently
 * exporting the whole canvas.
 * @param document the document to inspect.
 */
bool HasDeclaredOutputView(const PAGXDocument& document);

/**
 * Rejects a document that declares a target view when the calling exporter cannot honor it.
 * Centralizes the support check and the diagnostic so every exporter reports the same rule.
 * @param document the document to inspect.
 * @param format the exporter name used in the diagnostic, for example "SVG".
 * @param errorMsg optional out parameter that receives the rejection reason when the document is
 *        rejected.
 * @return true when the document declares no target view and may be exported; false when it must
 *         be rejected.
 */
bool CheckOutputViewExportSupport(const PAGXDocument& document, const char* format,
                                  std::string* errorMsg);

/**
 * Returns true when the rectangle is usable as a target view window: all components are finite,
 * width and height are greater than zero, and the computed far edges stay finite. Shared by the
 * importer and the runtime resolver so both entry points apply one numeric rule.
 * @param bounds the rectangle to validate.
 */
bool IsValidOutputBounds(const Rect& bounds);

/**
 * Returns the human-readable requirement that a valid target view window must satisfy. Shared by
 * the importer and the runtime resolver so both diagnostics describe the same rule.
 */
const char* OutputBoundsRequirement();

/**
 * Grants internal validation helpers read access to the node registration version kept on
 * PAGXDocument.
 */
class OutputViewValidationAccess {
 public:
  static uint64_t RegistryVersion(const PAGXDocument& document);
};

/**
 * Captures everything in a document that can affect a Valid output view declaration. PAGScene keeps
 * one after each successful full validation and compares it against a later notification to decide
 * whether the declaration has to be resolved again.
 */
struct OutputViewValidationSnapshot {
  std::string targetId = {};
  std::optional<Rect> bounds = std::nullopt;
  std::optional<std::string> declaredTargetLayer = std::nullopt;
  std::optional<std::string> declaredBounds = std::nullopt;
  std::vector<const Layer*> layers = {};
  std::vector<const Node*> animations = {};
  std::vector<const DataBind*> dataBinds = {};
  const Layer* targetLayer = nullptr;
  uint64_t registryVersion = 0;
};

/**
 * Captures a validation snapshot for an already Valid declaration.
 * @param document the document that was validated.
 * @param targetLayer the resolved source target layer.
 * @param snapshot receives the captured state; must not be null.
 * @return true when the snapshot was captured.
 */
bool CaptureOutputViewValidation(const PAGXDocument& document, const Layer* targetLayer,
                                 OutputViewValidationSnapshot* snapshot);

/**
 * Returns true when the notification provably cannot change the declaration semantics, so the
 * previously validated status and target can be reused without running ResolveOutputView(). The
 * caller must still mark the scene dirty and refresh its runtime tree; only the extra declaration
 * scans are skipped.
 *
 * The proof requires a render-only edit batch: every reported node is a document-owned
 * SolidColor/Rectangle/Image leaf, the batch does not affect layout, no reported node now carries
 * the target id, the node registry and declaration fields are unchanged, the top-level layer,
 * animation and data bind lists are unchanged, and the validated target is still owned by the
 * document under the same id.
 * @param document the document being edited.
 * @param reportedDirty the pre-collapse owned dirty nodes from the notification.
 * @param layoutChanged whether the notification reported a layout-affecting edit.
 * @param snapshot the snapshot captured by the last successful validation.
 */
bool CanReuseOutputViewValidation(const PAGXDocument& document,
                                  const std::vector<Node*>& reportedDirty, bool layoutChanged,
                                  const OutputViewValidationSnapshot& snapshot);

/**
 * Owns the prepared target-view wrapper across draws for a single PAGScene. It keeps the shared
 * parenting tool alive so ordinary targets reuse their wrapper across draws. A target used as an
 * external mask instead restores its hookup after each draw: releasing that ownership between
 * frames would change public aggregate bounds queries. Both paths use the same parenting tool.
 */
class OutputTargetState {
 public:
  OutputTargetState() = default;
  ~OutputTargetState();

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
   * Returns true when the wrapper is intact, or when a scoped target is back in the original
   * tree and ready for the next draw.
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
  bool scopedParenting = false;
};

/**
 * Applies the default output view transform to a canvas: the content offset in surface pixels, then
 * the zoom scale, then the window origin, so a root point maps to
 * contentOffset + zoom * (rootPoint - bounds.origin). Shared by PAGScene and the CLI so the
 * translate order and bounds handling stay in one place. Callers keep their own background and
 * clipping policy because the two pipelines differ there.
 * @param canvas the destination canvas in surface coordinates.
 * @param bounds the output window in document-root coordinates.
 * @param zoom the scale from root units to surface pixels. Must be positive.
 * @param contentOffset the surface-space offset applied before scaling.
 */
void ApplyOutputViewTransform(tgfx::Canvas* canvas, const Rect& bounds, float zoom,
                              const tgfx::Point& contentOffset);

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
