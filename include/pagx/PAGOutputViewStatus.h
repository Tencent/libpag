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

namespace pagx {

/**
 * Status of the output view declared by the paired `targetLayer` / `outputBounds`
 * root attributes. PAGScene::getOutputViewStatus() reports the status of a built scene; an invalid
 * declaration makes PAGScene::Make() return nullptr instead of rendering the whole canvas.
 */
enum class OutputViewStatus {
  /**
   * The document does not declare an output view; it renders the whole canvas.
   */
  None,

  /**
   * The document declares a valid output view.
   */
  Valid,

  /**
   * The document declares invalid output view fields. PAGScene::Make() rejects the document.
   */
  Invalid,
};

}  // namespace pagx
