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

#include "pag/file.h"
#include "pag/pag.h"
namespace pag {

class Transform2D;
class Glyph;

class PAGTextLayerPriv {
 public:
  static std::shared_ptr<PAGTextLayerPriv> Make(std::shared_ptr<PAGTextLayer> pagTextLayer);

  /**
   * Returns the text layer's transform.
   */
  const Transform2D* transform() const;

  /**
   * Returns the text layer's animators.
   */
  std::vector<TextAnimator*> textAnimators();

  /**
   * Set the text layer's animators.
   */
  void setAnimators(std::vector<TextAnimator*>* animators);

  /**
   * Set the text layer's typography information.
   */
  void setLayoutGlyphs(const std::vector<std::vector<std::shared_ptr<Glyph>>>& glyphs);

  /**
   * Set the text layer's justification.
   */
  void setJustification(Enum justification);

 private:
  PAGTextLayerPriv(std::shared_ptr<PAGTextLayer> pagTextLayer);

  std::weak_ptr<PAGTextLayer> pagTextLayer;
};

}  // namespace pag
