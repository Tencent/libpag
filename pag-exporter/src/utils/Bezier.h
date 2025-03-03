/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#ifndef BEZIER_H
#define BEZIER_H
class Bezier {
public:
  static float GetCubicLength(float startX, float startY, float startZ,
                              float controlX1, float controlY1, float controlZ1,
                              float controlX2, float controlY2, float controlZ2,
                              float endX, float endY, float endZ,
                              float precision = 0.005f);
};
#endif //BEZIER_H
