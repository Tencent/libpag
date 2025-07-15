/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <functional>
#include "codec/CodecContext.h"
#include "codec/TagHeader.h"

namespace pag {

#define SPATIAL_PRECISION 0.05f
#define BEZIER_PRECISION 0.005f
#define GRADIENT_PRECISION 0.00002f

Ratio ReadRatio(DecodeStream* stream);
Frame ReadTime(DecodeStream* stream);
Color ReadColor(DecodeStream* stream);
float ReadFloat(DecodeStream* stream);
bool ReadBoolean(DecodeStream* stream);
uint8_t ReadUint8(DecodeStream* stream);
ID ReadID(DecodeStream* stream);
Layer* ReadLayerID(DecodeStream* stream);
MaskData* ReadMaskID(DecodeStream* stream);
Composition* ReadCompositionID(DecodeStream* stream);
Opacity ReadOpacity(DecodeStream* stream);
Point ReadPoint(DecodeStream* stream);
Point3D ReadPoint3D(DecodeStream* stream);
PathHandle ReadPath(DecodeStream* stream);
TextDocumentHandle ReadTextDocument(DecodeStream* stream);
TextDocumentHandle ReadTextDocumentV2(DecodeStream* stream);
TextDocumentHandle ReadTextDocumentV3(DecodeStream* stream);
GradientColorHandle ReadGradientColor(DecodeStream* stream);

void WriteRatio(EncodeStream* stream, const Ratio& ratio);
void WriteTime(EncodeStream* stream, Frame time);
void WriteColor(EncodeStream* stream, const Color& color);
void WriteFloat(EncodeStream* stream, float value);
void WriteBoolean(EncodeStream* stream, bool value);
void WriteUint8(EncodeStream* stream, uint8_t value);
void WriteID(EncodeStream* stream, ID value);
void WriteLayerID(EncodeStream* stream, Layer* layer);
void WriteMaskID(EncodeStream* stream, MaskData* mask);
void WriteCompositionID(EncodeStream* stream, Composition* composition);
void WriteOpacity(EncodeStream* stream, Opacity value);
void WritePoint(EncodeStream* stream, Point value);
void WritePoint3D(EncodeStream* stream, Point3D value);
void WritePath(EncodeStream* stream, PathHandle value);
void WriteTextDocument(EncodeStream* stream, pag::TextDocumentHandle value);
void WriteTextDocumentV2(EncodeStream* stream, pag::TextDocumentHandle value);
void WriteTextDocumentV3(EncodeStream* stream, pag::TextDocumentHandle value);
void WriteGradientColor(EncodeStream* stream, pag::GradientColorHandle value);
}  // namespace pag
