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

#include "GLYUVTextureEffect.h"
#include "gpu/YUVTextureEffect.h"

namespace tgfx {
void GLYUVTextureEffect::emitCode(EmitArgs& args) {
  const auto* yuvFP = static_cast<const YUVTextureEffect*>(args.fragmentProcessor);
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;

  auto yuvTexture = static_cast<const YUVTexture*>(yuvFP->texture);
  fragBuilder->codeAppend("vec3 yuv;");
  fragBuilder->codeAppend("yuv.x = ");
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], (*args.transformedCoords)[0].name());
  fragBuilder->codeAppend(".r;");
  if (yuvTexture->pixelFormat() == YUVPixelFormat::I420) {
    fragBuilder->codeAppend("yuv.y = ");
    fragBuilder->appendTextureLookup((*args.textureSamplers)[1],
                                     (*args.transformedCoords)[0].name());
    fragBuilder->codeAppend(".r;");
    fragBuilder->codeAppend("yuv.z = ");
    fragBuilder->appendTextureLookup((*args.textureSamplers)[2],
                                     (*args.transformedCoords)[0].name());
    fragBuilder->codeAppend(".r;");
  } else if (yuvTexture->pixelFormat() == YUVPixelFormat::NV12) {
    fragBuilder->codeAppend("yuv.yz = ");
    fragBuilder->appendTextureLookup((*args.textureSamplers)[1],
                                     (*args.transformedCoords)[0].name());
    fragBuilder->codeAppend(".ra;");
  }
  if (yuvFP->texture->colorRange() == YUVColorRange::MPEG) {
    fragBuilder->codeAppend("yuv.x -= (16.0 / 255.0);");
  }
  fragBuilder->codeAppend("yuv.yz -= vec2(0.5, 0.5);");
  std::string mat3Name;
  mat3ColorConversionUniform = uniformHandler->addUniform(
      ShaderFlags::Fragment, ShaderVar::Type::Float3x3, "Mat3ColorConversion", &mat3Name);
  fragBuilder->codeAppendf("vec3 rgb = clamp(%s * yuv, 0.0, 1.0);", mat3Name.c_str());
  if (yuvFP->layout == nullptr) {
    fragBuilder->codeAppendf("%s = vec4(rgb, 1.0) * %s;", args.outputColor.c_str(),
                             args.inputColor.c_str());
  } else {
    std::string alphaStartName;
    alphaStartUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                                   "AlphaStart", &alphaStartName);
    std::string alphaVertexColor = "alphaVertexColor";
    fragBuilder->codeAppendf("vec2 %s = %s + %s;", alphaVertexColor.c_str(),
                             (*args.transformedCoords)[0].name().c_str(), alphaStartName.c_str());
    fragBuilder->codeAppend("float yuv_a = ");
    fragBuilder->appendTextureLookup((*args.textureSamplers)[0], alphaVertexColor);
    fragBuilder->codeAppend(".r;");
    fragBuilder->codeAppend(
        "// 为避免因压缩误差、精度等原因造成不透明变成部分透明（比如255变成254），\n");
    fragBuilder->codeAppend("// 下面进行了减1.0/255.0的精度修正。\n");
    fragBuilder->codeAppend("yuv_a = (yuv_a - 16.0/255.0) / (219.0/255.0 - 1.0/255.0);");
    fragBuilder->codeAppend("yuv_a = clamp(yuv_a, 0.0, 1.0);");
    fragBuilder->codeAppendf("%s = vec4(rgb * yuv_a, yuv_a) * %s;", args.outputColor.c_str(),
                             args.inputColor.c_str());
  }
}

static const float kColorConversion601LimitRange[] = {
    1.164, 1.164, 1.164, 0.0, -0.392f, 2.017, 1.596, -0.813f, 0.0,
};

static const float kColorConversion601FullRange[] = {
    1.0, 1.0, 1.0, 0.0, -0.344f, 1.772, 1.402, -0.714f, 0.0,
};

static const float kColorConversion709LimitRange[] = {
    1.164, 1.164, 1.164, 0.0, -0.213, 2.112, 1.793, -0.533, 0.0,
};

static const float kColorConversion709FullRange[] = {
    1.0, 1.0, 1.0, 0.0, -0.187, 1.856, 1.575, -0.468, 0.0,
};

static const float kColorConversion2020LimitRange[] = {
    1.168, 1.168, 1.168, 0.0, -0.188, 2.148, 1.684, -0.652, 0.0,
};

static const float kColorConversion2020FullRange[] = {
    1.0, 1.0, 1.0, 0.0, -0.165, 1.881, 1.475, -0.571, 0.0,
};

void GLYUVTextureEffect::onSetData(const ProgramDataManager& programDataManager,
                                   const FragmentProcessor& fragmentProcessor) {
  const auto& yuvFP = static_cast<const YUVTextureEffect&>(fragmentProcessor);
  if (alphaStartUniform.isValid()) {
    auto alphaStart = yuvFP.texture->getTextureCoord(static_cast<float>(yuvFP.layout->alphaStartX),
                                                     static_cast<float>(yuvFP.layout->alphaStartY));
    if (alphaStart != alphaStartPrev) {
      alphaStartPrev = alphaStart;
      programDataManager.set2f(alphaStartUniform, alphaStart.x, alphaStart.y);
    }
  }
  if (yuvFP.texture->colorSpace() != colorSpacePrev ||
      yuvFP.texture->colorRange() != colorRangePrev) {
    colorSpacePrev = yuvFP.texture->colorSpace();
    colorRangePrev = yuvFP.texture->colorRange();
    switch (yuvFP.texture->colorSpace()) {
      case YUVColorSpace::Rec601: {
        if (yuvFP.texture->colorRange() == YUVColorRange::JPEG) {
          programDataManager.setMatrix3f(mat3ColorConversionUniform, kColorConversion601FullRange);
        } else {
          programDataManager.setMatrix3f(mat3ColorConversionUniform, kColorConversion601LimitRange);
        }
        break;
      }
      case YUVColorSpace::Rec709: {
        if (yuvFP.texture->colorRange() == YUVColorRange::JPEG) {
          programDataManager.setMatrix3f(mat3ColorConversionUniform, kColorConversion709FullRange);
        } else {
          programDataManager.setMatrix3f(mat3ColorConversionUniform, kColorConversion709LimitRange);
        }
        break;
      }
      case YUVColorSpace::Rec2020: {
        if (yuvFP.texture->colorRange() == YUVColorRange::JPEG) {
          programDataManager.setMatrix3f(mat3ColorConversionUniform, kColorConversion2020FullRange);
        } else {
          programDataManager.setMatrix3f(mat3ColorConversionUniform,
                                         kColorConversion2020LimitRange);
        }
        break;
      }
      default:
        break;
    }
  }
}
}  // namespace tgfx
