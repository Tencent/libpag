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

#include "GLTiledTextureEffect.h"
#include "gpu/SamplerState.h"

namespace tgfx {
std::unique_ptr<FragmentProcessor> TiledTextureEffect::Make(std::shared_ptr<Texture> texture,
                                                            SamplerState samplerState,
                                                            const Matrix* localMatrix) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto matrix = localMatrix ? *localMatrix : Matrix::I();
  auto subset = Rect::MakeWH(texture->width(), texture->height());
  Sampling sampling(texture.get(), samplerState, subset, texture->getContext()->caps());
  return std::unique_ptr<TiledTextureEffect>(
      new GLTiledTextureEffect(std::move(texture), sampling, matrix));
}

GLTiledTextureEffect::GLTiledTextureEffect(std::shared_ptr<Texture> texture,
                                           const TiledTextureEffect::Sampling& sampling,
                                           const Matrix& localMatrix)
    : TiledTextureEffect(std::move(texture), sampling, localMatrix) {
}

bool GLTiledTextureEffect::ShaderModeRequiresUnormCoord(TiledTextureEffect::ShaderMode mode) {
  switch (mode) {
    case TiledTextureEffect::ShaderMode::None:
    case TiledTextureEffect::ShaderMode::Clamp:
    case TiledTextureEffect::ShaderMode::RepeatNearestNone:
    case TiledTextureEffect::ShaderMode::MirrorRepeat:
      return false;
    case TiledTextureEffect::ShaderMode::RepeatLinearNone:
    case TiledTextureEffect::ShaderMode::RepeatNearestMipmap:
    case TiledTextureEffect::ShaderMode::RepeatLinearMipmap:
    case TiledTextureEffect::ShaderMode::ClampToBorderNearest:
    case TiledTextureEffect::ShaderMode::ClampToBorderLinear:
      return true;
  }
}

bool GLTiledTextureEffect::ShaderModeUsesSubset(TiledTextureEffect::ShaderMode m) {
  switch (m) {
    case TiledTextureEffect::ShaderMode::None:
    case TiledTextureEffect::ShaderMode::Clamp:
      return false;
    case TiledTextureEffect::ShaderMode::RepeatNearestNone:
    case TiledTextureEffect::ShaderMode::RepeatLinearNone:
    case TiledTextureEffect::ShaderMode::RepeatNearestMipmap:
    case TiledTextureEffect::ShaderMode::RepeatLinearMipmap:
    case TiledTextureEffect::ShaderMode::MirrorRepeat:
    case TiledTextureEffect::ShaderMode::ClampToBorderNearest:
    case TiledTextureEffect::ShaderMode::ClampToBorderLinear:
      return true;
  }
}

bool GLTiledTextureEffect::ShaderModeUsesClamp(TiledTextureEffect::ShaderMode m) {
  switch (m) {
    case TiledTextureEffect::ShaderMode::None:
    case TiledTextureEffect::ShaderMode::ClampToBorderNearest:
      return false;
    case TiledTextureEffect::ShaderMode::Clamp:
    case TiledTextureEffect::ShaderMode::RepeatNearestNone:
    case TiledTextureEffect::ShaderMode::RepeatLinearNone:
    case TiledTextureEffect::ShaderMode::RepeatNearestMipmap:
    case TiledTextureEffect::ShaderMode::RepeatLinearMipmap:
    case TiledTextureEffect::ShaderMode::MirrorRepeat:
    case TiledTextureEffect::ShaderMode::ClampToBorderLinear:
      return true;
  }
}

void GLTiledTextureEffect::readColor(EmitArgs& args, const std::string& dimensionsName,
                                     const std::string& coord, const char* out) const {
  std::string normCoord;
  if (!dimensionsName.empty()) {
    normCoord = "(" + coord + ") * " + dimensionsName + "";
  } else {
    normCoord = coord;
  }
  auto* fragBuilder = args.fragBuilder;
  fragBuilder->codeAppendf("vec4 %s = ", out);
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], normCoord);
  fragBuilder->codeAppend(";");
}

void GLTiledTextureEffect::subsetCoord(EmitArgs& args, TiledTextureEffect::ShaderMode mode,
                                       const std::string& subsetName, const char* coordSwizzle,
                                       const char* subsetStartSwizzle,
                                       const char* subsetStopSwizzle, const char* extraCoord,
                                       const char* coordWeight) const {
  auto* fragBuilder = args.fragBuilder;
  switch (mode) {
    case TiledTextureEffect::ShaderMode::None:
    case TiledTextureEffect::ShaderMode::ClampToBorderNearest:
    case TiledTextureEffect::ShaderMode::ClampToBorderLinear:
    case TiledTextureEffect::ShaderMode::Clamp:
      fragBuilder->codeAppendf("subsetCoord.%s = inCoord.%s;", coordSwizzle, coordSwizzle);
      break;
    case TiledTextureEffect::ShaderMode::RepeatNearestNone:
    case TiledTextureEffect::ShaderMode::RepeatLinearNone:
      fragBuilder->codeAppendf("subsetCoord.%s = mod(inCoord.%s - %s.%s, %s.%s - %s.%s) + %s.%s;",
                               coordSwizzle, coordSwizzle, subsetName.c_str(), subsetStartSwizzle,
                               subsetName.c_str(), subsetStopSwizzle, subsetName.c_str(),
                               subsetStartSwizzle, subsetName.c_str(), subsetStartSwizzle);
      break;
    case TiledTextureEffect::ShaderMode::RepeatNearestMipmap:
    case TiledTextureEffect::ShaderMode::RepeatLinearMipmap:
      fragBuilder->codeAppend("{");
      fragBuilder->codeAppendf("float w = %s.%s - %s.%s;", subsetName.c_str(), subsetStopSwizzle,
                               subsetName.c_str(), subsetStartSwizzle);
      fragBuilder->codeAppendf("float w2 = 2.0 * w;");
      fragBuilder->codeAppendf("float d = inCoord.%s - %s.%s;", coordSwizzle, subsetName.c_str(),
                               subsetStartSwizzle);
      fragBuilder->codeAppend("float m = mod(d, w2);");
      fragBuilder->codeAppend("float o = mix(m, w2 - m, step(w, m));");
      fragBuilder->codeAppendf("subsetCoord.%s = o + %s.%s;", coordSwizzle, subsetName.c_str(),
                               subsetStartSwizzle);
      fragBuilder->codeAppendf("%s = w - o + %s.%s;", extraCoord, subsetName.c_str(),
                               subsetStartSwizzle);
      // coordWeight is used as the third param of mix() to blend between a sample taken using
      // subsetCoord and a sample at extraCoord.
      fragBuilder->codeAppend("float hw = w / 2.0;");
      fragBuilder->codeAppend("float n = mod(d - hw, w2);");
      fragBuilder->codeAppendf("%s = clamp(mix(n, w2 - n, step(w, n)) - hw + 0.5, 0.0, 1.0);",
                               coordWeight);
      fragBuilder->codeAppend("}");
      break;
    case TiledTextureEffect::ShaderMode::MirrorRepeat:
      fragBuilder->codeAppend("{");
      fragBuilder->codeAppendf("float w = %s.%s - %s.%s;", subsetName.c_str(), subsetStopSwizzle,
                               subsetName.c_str(), subsetStartSwizzle);
      fragBuilder->codeAppendf("float w2 = 2.0 * w;");
      fragBuilder->codeAppendf("float m = mod(inCoord.%s - %s.%s, w2);", coordSwizzle,
                               subsetName.c_str(), subsetStartSwizzle);
      fragBuilder->codeAppendf("subsetCoord.%s = mix(m, w2 - m, step(w, m)) + %s.%s;", coordSwizzle,
                               subsetName.c_str(), subsetStartSwizzle);
      fragBuilder->codeAppend("}");
      break;
  }
}

void GLTiledTextureEffect::clampCoord(EmitArgs& args, bool clamp, const std::string& clampName,
                                      const char* coordSwizzle, const char* clampStartSwizzle,
                                      const char* clampStopSwizzle) const {
  auto* fragBuilder = args.fragBuilder;
  if (clamp) {
    fragBuilder->codeAppendf("clampedCoord%s = clamp(subsetCoord%s, %s%s, %s%s);", coordSwizzle,
                             coordSwizzle, clampName.c_str(), clampStartSwizzle, clampName.c_str(),
                             clampStopSwizzle);
  } else {
    fragBuilder->codeAppendf("clampedCoord%s = subsetCoord%s;", coordSwizzle, coordSwizzle);
  }
}

void GLTiledTextureEffect::clampCoord(EmitArgs& args, const bool useClamp[2],
                                      const std::string& clampName) const {
  if (useClamp[0] == useClamp[1]) {
    clampCoord(args, useClamp[0], clampName, "", ".xy", ".zw");
  } else {
    clampCoord(args, useClamp[0], clampName, ".x", ".x", ".z");
    clampCoord(args, useClamp[1], clampName, ".y", ".y", ".w");
  }
}

GLTiledTextureEffect::UniformNames GLTiledTextureEffect::initUniform(EmitArgs& args,
                                                                     const bool useSubset[2],
                                                                     const bool useClamp[2]) const {
  UniformNames names = {};
  auto* uniformHandler = args.uniformHandler;
  if (useSubset[0] || useSubset[1]) {
    names.subsetName =
        uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4, "Subset");
  }
  if (useClamp[0] || useClamp[1]) {
    names.clampName =
        uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4, "Clamp");
  }
  bool unormCoordsRequiredForShaderMode =
      ShaderModeRequiresUnormCoord(shaderModeX) || ShaderModeRequiresUnormCoord(shaderModeY);
  bool sampleCoordsMustBeNormalized = texture->getSampler()->type() != TextureType::Rectangle;
  if (unormCoordsRequiredForShaderMode && sampleCoordsMustBeNormalized) {
    names.dimensionsName =
        uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2, "Dimension");
  }
  return names;
}

void GLTiledTextureEffect::emitCode(EmitArgs& args) const {
  auto* fragBuilder = args.fragBuilder;
  auto vertexColor = (*args.transformedCoords)[0].name();
  if (args.coordFunc) {
    vertexColor = args.coordFunc(vertexColor);
  }
  if (shaderModeX == TiledTextureEffect::ShaderMode::None &&
      shaderModeY == TiledTextureEffect::ShaderMode::None) {
    fragBuilder->codeAppendf("%s = ", args.outputColor.c_str());
    fragBuilder->appendTextureLookup((*args.textureSamplers)[0], vertexColor);
    fragBuilder->codeAppendf(" * %s;", args.inputColor.c_str());
  } else {
    fragBuilder->codeAppendf("vec2 inCoord = %s;", vertexColor.c_str());
    bool useSubset[2] = {ShaderModeUsesSubset(shaderModeX), ShaderModeUsesSubset(shaderModeY)};
    bool useClamp[2] = {ShaderModeUsesClamp(shaderModeX), ShaderModeUsesClamp(shaderModeY)};
    auto names = initUniform(args, useSubset, useClamp);
    if (!names.dimensionsName.empty()) {
      fragBuilder->codeAppendf("inCoord /= %s;", names.dimensionsName.c_str());
    }

    const char* extraRepeatCoordX = nullptr;
    const char* repeatCoordWeightX = nullptr;
    const char* extraRepeatCoordY = nullptr;
    const char* repeatCoordWeightY = nullptr;

    bool mipMapRepeatX = shaderModeX == TiledTextureEffect::ShaderMode::RepeatNearestMipmap ||
                         shaderModeX == TiledTextureEffect::ShaderMode::RepeatLinearMipmap;
    bool mipMapRepeatY = shaderModeY == TiledTextureEffect::ShaderMode::RepeatNearestMipmap ||
                         shaderModeY == TiledTextureEffect::ShaderMode::RepeatLinearMipmap;

    if (mipMapRepeatX || mipMapRepeatY) {
      fragBuilder->codeAppend("vec2 extraRepeatCoord;");
    }
    if (mipMapRepeatX) {
      fragBuilder->codeAppend("float repeatCoordWeightX;");
      extraRepeatCoordX = "extraRepeatCoord.x";
      repeatCoordWeightX = "repeatCoordWeightX";
    }
    if (mipMapRepeatY) {
      fragBuilder->codeAppend("float repeatCoordWeightY;");
      extraRepeatCoordY = "extraRepeatCoord.y";
      repeatCoordWeightY = "repeatCoordWeightY";
    }

    fragBuilder->codeAppend("highp vec2 subsetCoord;");
    subsetCoord(args, shaderModeX, names.subsetName, "x", "x", "z", extraRepeatCoordX,
                repeatCoordWeightX);
    subsetCoord(args, shaderModeY, names.subsetName, "y", "y", "w", extraRepeatCoordY,
                repeatCoordWeightY);

    fragBuilder->codeAppend("vec2 clampedCoord;");
    clampCoord(args, useClamp, names.clampName);

    if (mipMapRepeatX && mipMapRepeatY) {
      fragBuilder->codeAppendf("extraRepeatCoord = clamp(extraRepeatCoord, %s.xy, %s.zw);",
                               names.clampName.c_str(), names.clampName.c_str());
    } else if (mipMapRepeatX) {
      fragBuilder->codeAppendf("extraRepeatCoord.x = clamp(extraRepeatCoord.x, %s.x, %s.z);",
                               names.clampName.c_str(), names.clampName.c_str());
    } else if (mipMapRepeatY) {
      fragBuilder->codeAppendf("extraRepeatCoord.y = clamp(extraRepeatCoord.y, %s.y, %s.w);",
                               names.clampName.c_str(), names.clampName.c_str());
    }

    if (mipMapRepeatX && mipMapRepeatY) {
      const char* textureColor1 = "textureColor1";
      readColor(args, names.dimensionsName, "clampedCoord", textureColor1);
      const char* textureColor2 = "textureColor2";
      readColor(args, names.dimensionsName, "vec2(extraRepeatCoord.x, clampedCoord.y)",
                textureColor2);
      const char* textureColor3 = "textureColor3";
      readColor(args, names.dimensionsName, "vec2(clampedCoord.x, extraRepeatCoord.y)",
                textureColor3);
      const char* textureColor4 = "textureColor4";
      readColor(args, names.dimensionsName, "vec2(extraRepeatCoord.x, extraRepeatCoord.y)",
                textureColor4);
      fragBuilder->codeAppendf(
          "vec4 textureColor = mix(mix(%s, %s, repeatCoordWeightX), mix(%s, %s, "
          "repeatCoordWeightX), repeatCoordWeightY);",
          textureColor1, textureColor2, textureColor3, textureColor4);
    } else if (mipMapRepeatX) {
      const char* textureColor1 = "textureColor1";
      readColor(args, names.dimensionsName, "clampedCoord", textureColor1);
      const char* textureColor2 = "textureColor2";
      readColor(args, names.dimensionsName, "vec2(extraRepeatCoord.x, clampedCoord.y)",
                textureColor2);
      fragBuilder->codeAppendf("vec4 textureColor = mix(%s, %s, repeatCoordWeightX);",
                               textureColor1, textureColor2);
    } else if (mipMapRepeatY) {
      const char* textureColor1 = "textureColor1";
      readColor(args, names.dimensionsName, "clampedCoord", textureColor1);
      const char* textureColor2 = "textureColor2";
      readColor(args, names.dimensionsName, "vec2(clampedCoord.x, extraRepeatCoord.y)",
                textureColor2);
      fragBuilder->codeAppendf("vec4 textureColor = mix(%s, %s, repeatCoordWeightY);",
                               textureColor1, textureColor2);
    } else {
      readColor(args, names.dimensionsName, "clampedCoord", "textureColor");
    }

    static const char* repeatReadX = "repeatReadX";
    static const char* repeatReadY = "repeatReadY";
    bool repeatX = shaderModeX == TiledTextureEffect::ShaderMode::RepeatLinearNone ||
                   shaderModeX == TiledTextureEffect::ShaderMode::RepeatLinearMipmap;
    bool repeatY = shaderModeY == TiledTextureEffect::ShaderMode::RepeatLinearNone ||
                   shaderModeY == TiledTextureEffect::ShaderMode::RepeatLinearMipmap;
    if (repeatX || shaderModeX == TiledTextureEffect::ShaderMode::ClampToBorderLinear) {
      fragBuilder->codeAppend("float errX = subsetCoord.x - clampedCoord.x;");
      if (repeatX) {
        fragBuilder->codeAppendf("float repeatCoordX = errX > 0.0 ? %s.x : %s.z;",
                                 names.clampName.c_str(), names.clampName.c_str());
      }
    }
    if (repeatY || shaderModeY == TiledTextureEffect::ShaderMode::ClampToBorderLinear) {
      fragBuilder->codeAppend("float errY = subsetCoord.y - clampedCoord.y;");
      if (repeatY) {
        fragBuilder->codeAppendf("float repeatCoordY = errY > 0.0 ? %s.y : %s.w;",
                                 names.clampName.c_str(), names.clampName.c_str());
      }
    }

    const char* ifStr = "if";
    if (repeatX && repeatY) {
      readColor(args, names.dimensionsName, "vec2(repeatCoordX, clampedCoord.y)", repeatReadX);
      readColor(args, names.dimensionsName, "vec2(clampedCoord.x, repeatCoordY)", repeatReadY);
      static const char* repeatReadXY = "repeatReadXY";
      readColor(args, names.dimensionsName, "vec2(repeatCoordX, repeatCoordY)", repeatReadXY);
      fragBuilder->codeAppend("if (errX != 0.0 && errY != 0.0) {");
      fragBuilder->codeAppend("errX = abs(errX);");
      fragBuilder->codeAppendf(
          "textureColor = mix(mix(textureColor, %s, errX), mix(%s, %s, errX), abs(errY));",
          repeatReadX, repeatReadY, repeatReadXY);
      fragBuilder->codeAppend("}");
      ifStr = "else if";
    }
    if (repeatX) {
      fragBuilder->codeAppendf("%s (errX != 0.0) {", ifStr);
      readColor(args, names.dimensionsName, "vec2(repeatCoordX, clampedCoord.y)", repeatReadX);
      fragBuilder->codeAppendf("textureColor = mix(textureColor, %s, errX);", repeatReadX);
      fragBuilder->codeAppend("}");
    }
    if (repeatY) {
      fragBuilder->codeAppendf("%s (errY != 0.0) {", ifStr);
      readColor(args, names.dimensionsName, "vec2(clampedCoord.x, repeatCoordY)", repeatReadY);
      fragBuilder->codeAppendf("textureColor = mix(textureColor, %s, errY);", repeatReadY);
      fragBuilder->codeAppend("}");
    }

    if (shaderModeX == TiledTextureEffect::ShaderMode::ClampToBorderLinear) {
      fragBuilder->codeAppend("textureColor = mix(textureColor, vec4(0.0), min(abs(errX), 1.0));");
    }
    if (shaderModeY == TiledTextureEffect::ShaderMode::ClampToBorderLinear) {
      fragBuilder->codeAppendf("textureColor = mix(textureColor, vec4(0.0), min(abs(errY), 1.0));");
    }
    if (shaderModeX == TiledTextureEffect::ShaderMode::ClampToBorderNearest) {
      fragBuilder->codeAppend("float snappedX = floor(inCoord.x + 0.001) + 0.5;");
      fragBuilder->codeAppendf("if (snappedX < %s.x || snappedX > %s.z) {",
                               names.subsetName.c_str(), names.subsetName.c_str());
      fragBuilder->codeAppend("textureColor = vec4(0.0);");  // border color
      fragBuilder->codeAppend("}");
    }
    if (shaderModeY == TiledTextureEffect::ShaderMode::ClampToBorderNearest) {
      fragBuilder->codeAppend("float snappedY = floor(inCoord.y + 0.001) + 0.5;");
      fragBuilder->codeAppendf("if (snappedY < %s.y || snappedY > %s.w) {",
                               names.subsetName.c_str(), names.subsetName.c_str());
      fragBuilder->codeAppend("textureColor = vec4(0.0);");  // border color
      fragBuilder->codeAppend("}");
    }
    fragBuilder->codeAppendf("%s = textureColor * %s;", args.outputColor.c_str(),
                             args.inputColor.c_str());
  }
}

void GLTiledTextureEffect::onSetData(UniformBuffer* uniformBuffer) const {
  auto hasDimensionUniform =
      (ShaderModeRequiresUnormCoord(shaderModeX) || ShaderModeRequiresUnormCoord(shaderModeY)) &&
      texture->getSampler()->type() != TextureType::Rectangle;
  if (hasDimensionUniform) {
    auto dimensions = texture->getTextureCoord(1.f, 1.f);
    uniformBuffer->setData("Dimension", &dimensions);
  }
  auto pushRect = [&](Rect subset, const std::string& uni) {
    float rect[4] = {subset.left, subset.top, subset.right, subset.bottom};
    if (texture->origin() == ImageOrigin::BottomLeft) {
      auto h = static_cast<float>(texture->height());
      rect[1] = h - rect[1];
      rect[3] = h - rect[3];
      std::swap(rect[1], rect[3]);
    }
    auto type = texture->getSampler()->type();
    if (!hasDimensionUniform && type != TextureType::Rectangle) {
      auto lt = texture->getTextureCoord(static_cast<float>(rect[0]), static_cast<float>(rect[1]));
      auto rb = texture->getTextureCoord(static_cast<float>(rect[2]), static_cast<float>(rect[3]));
      rect[0] = lt.x;
      rect[1] = lt.y;
      rect[2] = rb.x;
      rect[3] = rb.y;
    }
    uniformBuffer->setData(uni, &rect);
  };
  if (ShaderModeUsesSubset(shaderModeX) || ShaderModeUsesSubset(shaderModeY)) {
    pushRect(subset, "Subset");
  }
  if (ShaderModeUsesClamp(shaderModeX) || ShaderModeUsesClamp(shaderModeY)) {
    pushRect(clamp, "Clamp");
  }
}
}  // namespace tgfx
