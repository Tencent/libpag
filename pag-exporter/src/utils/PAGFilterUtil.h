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
#ifndef PAGFILTERUTIL_H
#define PAGFILTERUTIL_H
#include "pag/file.h"
#include "src/utils/Context.h"

using ProcessLayerFP = void (*)(pagexporter::Context& context, pag::Layer* layer, void* ctx);
using ProcessLayerWithTimeFP = void (*)(pagexporter::Context& context, pag::Layer* layer, pag::Frame compositionStartTime,
                                        void* ctx);

class PAGFilterUtil {
public:
  static void TraversalLayers(pagexporter::Context& context,
                              pag::Composition* composition,
                              pag::LayerType layerType,
                              ProcessLayerFP processLayerFP,
                              void* ctx);

  static void TraversalLayers(pagexporter::Context& context,
                              pag::Composition* composition,
                              pag::LayerType layerType,
                              pag::Frame compositionStartTime,
                              ProcessLayerWithTimeFP processLayerWithTimeFP,
                              void* ctx);

  static void TraversalLayers(pagexporter::Context& context,
                              std::vector<pag::Composition*>& compositions,
                              pag::LayerType layerType,
                              ProcessLayerFP processLayerFP,
                              void* ctx);
};

#endif //PAGFILTERUTIL_H
