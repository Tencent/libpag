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

#include "GeometryProcessor.h"

namespace tgfx {
/**
 * Skia's sharing：
 * https://www.essentialmath.com/GDC2015/VanVerth_Jim_DrawingAntialiasedEllipse.pdf
 *
 * There is a formula that calculates the approximate distance from the point to the ellipse,
 * and the proof of the formula can be found in the link below.
 * https://www.researchgate.net/publication/222440289_Sampson_PD_Fitting_conic_sections_to_very_scattered_data_An_iterative_refinement_of_the_Bookstein_algorithm_Comput_Graphics_Image_Process_18_97-108
 */
class EllipseGeometryProcessor : public GeometryProcessor {
 public:
  static std::unique_ptr<EllipseGeometryProcessor> Make(int width, int height, bool stroke,
                                                        bool useScale, const Matrix& localMatrix);

  std::string name() const override {
    return "EllipseGeometryProcessor";
  }

  std::unique_ptr<GLGeometryProcessor> createGLInstance() const override;

 private:
  EllipseGeometryProcessor(int width, int height, bool stroke, bool useScale,
                           const Matrix& localMatrix);

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  Attribute inPosition;
  Attribute inColor;
  Attribute inEllipseOffset;
  Attribute inEllipseRadii;

  int width = 1;
  int height = 1;
  // UV 也使用 inPosition 的坐标，localMatrix 可以把 inPosition 的坐标转换成 UV 的
  Matrix localMatrix;
  bool stroke;
  bool useScale;

  friend class GLEllipseGeometryProcessor;
};
}  // namespace tgfx
