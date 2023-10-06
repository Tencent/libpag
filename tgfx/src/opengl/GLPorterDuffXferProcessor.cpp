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

#include "GLPorterDuffXferProcessor.h"
#include "GLBlend.h"
#include "gpu/PorterDuffXferProcessor.h"

namespace tgfx {
void GLPorterDuffXferProcessor::emitCode(const EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;
  const auto& dstColor = fragBuilder->dstColor();

  if (args.dstTextureSamplerHandle.isValid()) {
    // We don't think any shaders actually output negative coverage, but just as a safety
    // check for floating point precision errors we compare with <= here. We just check the
    // rgb values of the coverage since the alpha may not have been set when using lcd. If
    // we are using single channel coverage alpha will equal to rgb anyways.
    //
    // The discard here also helps for batching text draws together which need to read from
    // a dst copy for blends. Though this only helps the case where the outer bounding boxes
    // of each letter overlap and not two actually parts of the text.
    fragBuilder->codeAppendf("if (%s.r <= 0.0 && %s.g <= 0.0 && %s.b <= 0.0) {",
                             args.inputCoverage.c_str(), args.inputCoverage.c_str(),
                             args.inputCoverage.c_str());
    fragBuilder->codeAppend("discard;");
    fragBuilder->codeAppend("}");

    std::string dstTopLeftName;
    std::string dstCoordScaleName;
    dstTopLeftUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                                   "DstTextureUpperLeft", &dstTopLeftName);
    dstScaleUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                                 "DstTextureCoordScale", &dstCoordScaleName);

    fragBuilder->codeAppend("// Read color from copy of the destination.\n");
    std::string dstTexCoord = "_dstTexCoord";
    fragBuilder->codeAppendf("vec2 %s = (gl_FragCoord.xy - %s) * %s;", dstTexCoord.c_str(),
                             dstTopLeftName.c_str(), dstCoordScaleName.c_str());

    fragBuilder->codeAppendf("vec4 %s = ", dstColor.c_str());
    fragBuilder->appendTextureLookup(args.dstTextureSamplerHandle, dstTexCoord);
    fragBuilder->codeAppend(";");
  }

  const char* outColor = "localOutputColor";
  fragBuilder->codeAppendf("vec4 %s;", outColor);
  const auto* pdXP = static_cast<const PorterDuffXferProcessor*>(args.xferProcessor);
  AppendMode(fragBuilder, args.inputColor, dstColor, outColor, pdXP->getBlend());
  fragBuilder->codeAppendf("%s = %s * %s + (vec4(1.0) - %s) * %s;", outColor,
                           args.inputCoverage.c_str(), outColor, args.inputCoverage.c_str(),
                           dstColor.c_str());
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), outColor);
}

void GLPorterDuffXferProcessor::setData(UniformBuffer* uniformBuffer, const XferProcessor&,
                                        const Texture* dstTexture, const Point& dstTextureOffset) {
  if (dstTexture) {
    if (dstTopLeftUniform.isValid()) {
      uniformBuffer->setData("DstTextureUpperLeft", &dstTextureOffset);
      int width;
      int height;
      if (dstTexture->getSampler()->type() == TextureType::Rectangle) {
        width = 1;
        height = 1;
      } else {
        width = dstTexture->width();
        height = dstTexture->height();
      }
      float scales[] = {1.f / static_cast<float>(width), 1.f / static_cast<float>(height)};
      uniformBuffer->setData("DstTextureCoordScale", scales);
    }
  }
}
}  // namespace tgfx
