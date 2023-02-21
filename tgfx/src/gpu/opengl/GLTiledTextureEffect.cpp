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
#include "gpu/TiledTextureEffect.h"

namespace tgfx {
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

void GLTiledTextureEffect::readColor(EmitArgs& args, const std::string& coord, const char* out) {
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

void GLTiledTextureEffect::subsetCoord(GLFragmentProcessor::EmitArgs& args,
                                       TiledTextureEffect::ShaderMode mode,
                                       const char* coordSwizzle, const char* subsetStartSwizzle,
                                       const char* subsetStopSwizzle, const char* extraCoord,
                                       const char* coordWeight) {
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

void GLTiledTextureEffect::clampCoord(GLFragmentProcessor::EmitArgs& args, bool clamp,
                                      const char* coordSwizzle, const char* clampStartSwizzle,
                                      const char* clampStopSwizzle) {
  auto* fragBuilder = args.fragBuilder;
  if (clamp) {
    fragBuilder->codeAppendf("clampedCoord%s = clamp(subsetCoord%s, %s%s, %s%s);", coordSwizzle,
                             coordSwizzle, clampName.c_str(), clampStartSwizzle, clampName.c_str(),
                             clampStopSwizzle);
  } else {
    fragBuilder->codeAppendf("clampedCoord%s = subsetCoord%s;", coordSwizzle, coordSwizzle);
  }
}

void GLTiledTextureEffect::clampCoord(GLFragmentProcessor::EmitArgs& args, const bool useClamp[2]) {
  if (useClamp[0] == useClamp[1]) {
    clampCoord(args, useClamp[0], "", ".xy", ".zw");
  } else {
    clampCoord(args, useClamp[0], ".x", ".x", ".z");
    clampCoord(args, useClamp[1], ".y", ".y", ".w");
  }
}

void GLTiledTextureEffect::initUniform(GLFragmentProcessor::EmitArgs& args, const bool useSubset[2],
                                       const bool useClamp[2]) {
  auto* uniformHandler = args.uniformHandler;
  if (useSubset[0] || useSubset[1]) {
    subsetUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                               "Subset", &subsetName);
  }
  if (useClamp[0] || useClamp[1]) {
    clampUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                              "Clamp", &clampName);
  }
  const auto* textureFP = static_cast<const TiledTextureEffect*>(args.fragmentProcessor);
  bool unormCoordsRequiredForShaderMode = ShaderModeRequiresUnormCoord(textureFP->shaderModeX) ||
                                          ShaderModeRequiresUnormCoord(textureFP->shaderModeY);
  bool sampleCoordsMustBeNormalized =
      textureFP->texture->getSampler()->type() != TextureType::Rectangle;
  if (unormCoordsRequiredForShaderMode && sampleCoordsMustBeNormalized) {
    dimensionsUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2,
                                                   "Dimension", &dimensionsName);
  }
}

void GLTiledTextureEffect::emitCode(EmitArgs& args) {
  const auto* textureFP = static_cast<const TiledTextureEffect*>(args.fragmentProcessor);
  auto* fragBuilder = args.fragBuilder;

  auto vertexColor = (*args.transformedCoords)[0].name();
  if (args.coordFunc) {
    vertexColor = args.coordFunc(vertexColor);
  }
  if (textureFP->shaderModeX == TiledTextureEffect::ShaderMode::None &&
      textureFP->shaderModeY == TiledTextureEffect::ShaderMode::None) {
    fragBuilder->codeAppendf("%s = ", args.outputColor.c_str());
    fragBuilder->appendTextureLookup((*args.textureSamplers)[0], vertexColor);
    fragBuilder->codeAppendf(" * %s;", args.inputColor.c_str());
  } else {
    fragBuilder->codeAppendf("vec2 inCoord = %s;", vertexColor.c_str());
    bool useSubset[2] = {ShaderModeUsesSubset(textureFP->shaderModeX),
                         ShaderModeUsesSubset(textureFP->shaderModeY)};
    bool useClamp[2] = {ShaderModeUsesClamp(textureFP->shaderModeX),
                        ShaderModeUsesClamp(textureFP->shaderModeY)};
    initUniform(args, useSubset, useClamp);
    if (dimensionsUniform.isValid()) {
      fragBuilder->codeAppendf("inCoord /= %s;", dimensionsName.c_str());
    }

    const char* extraRepeatCoordX = nullptr;
    const char* repeatCoordWeightX = nullptr;
    const char* extraRepeatCoordY = nullptr;
    const char* repeatCoordWeightY = nullptr;

    bool mipMapRepeatX =
        textureFP->shaderModeX == TiledTextureEffect::ShaderMode::RepeatNearestMipmap ||
        textureFP->shaderModeX == TiledTextureEffect::ShaderMode::RepeatLinearMipmap;
    bool mipMapRepeatY =
        textureFP->shaderModeY == TiledTextureEffect::ShaderMode::RepeatNearestMipmap ||
        textureFP->shaderModeY == TiledTextureEffect::ShaderMode::RepeatLinearMipmap;

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
    subsetCoord(args, textureFP->shaderModeX, "x", "x", "z", extraRepeatCoordX, repeatCoordWeightX);
    subsetCoord(args, textureFP->shaderModeY, "y", "y", "w", extraRepeatCoordY, repeatCoordWeightY);

    fragBuilder->codeAppend("vec2 clampedCoord;");
    clampCoord(args, useClamp);

    if (mipMapRepeatX && mipMapRepeatY) {
      fragBuilder->codeAppendf("extraRepeatCoord = clamp(extraRepeatCoord, %s.xy, %s.zw);",
                               clampName.c_str(), clampName.c_str());
    } else if (mipMapRepeatX) {
      fragBuilder->codeAppendf("extraRepeatCoord.x = clamp(extraRepeatCoord.x, %s.x, %s.z);",
                               clampName.c_str(), clampName.c_str());
    } else if (mipMapRepeatY) {
      fragBuilder->codeAppendf("extraRepeatCoord.y = clamp(extraRepeatCoord.y, %s.y, %s.w);",
                               clampName.c_str(), clampName.c_str());
    }

    if (mipMapRepeatX && mipMapRepeatY) {
      const char* textureColor1 = "textureColor1";
      readColor(args, "clampedCoord", textureColor1);
      const char* textureColor2 = "textureColor2";
      readColor(args, "vec2(extraRepeatCoord.x, clampedCoord.y)", textureColor2);
      const char* textureColor3 = "textureColor3";
      readColor(args, "vec2(clampedCoord.x, extraRepeatCoord.y)", textureColor3);
      const char* textureColor4 = "textureColor4";
      readColor(args, "vec2(extraRepeatCoord.x, extraRepeatCoord.y)", textureColor4);
      fragBuilder->codeAppendf(
          "vec4 textureColor = mix(mix(%s, %s, repeatCoordWeightX), mix(%s, %s, "
          "repeatCoordWeightX), repeatCoordWeightY);",
          textureColor1, textureColor2, textureColor3, textureColor4);
    } else if (mipMapRepeatX) {
      const char* textureColor1 = "textureColor1";
      readColor(args, "clampedCoord", textureColor1);
      const char* textureColor2 = "textureColor2";
      readColor(args, "vec2(extraRepeatCoord.x, clampedCoord.y)", textureColor2);
      fragBuilder->codeAppendf("vec4 textureColor = mix(%s, %s, repeatCoordWeightX);",
                               textureColor1, textureColor2);
    } else if (mipMapRepeatY) {
      const char* textureColor1 = "textureColor1";
      readColor(args, "clampedCoord", textureColor1);
      const char* textureColor2 = "textureColor2";
      readColor(args, "vec2(clampedCoord.x, extraRepeatCoord.y)", textureColor2);
      fragBuilder->codeAppendf("vec4 textureColor = mix(%s, %s, repeatCoordWeightY);",
                               textureColor1, textureColor2);
    } else {
      readColor(args, "clampedCoord", "textureColor");
    }

    static const char* repeatReadX = "repeatReadX";
    static const char* repeatReadY = "repeatReadY";
    bool repeatX = textureFP->shaderModeX == TiledTextureEffect::ShaderMode::RepeatLinearNone ||
                   textureFP->shaderModeX == TiledTextureEffect::ShaderMode::RepeatLinearMipmap;
    bool repeatY = textureFP->shaderModeY == TiledTextureEffect::ShaderMode::RepeatLinearNone ||
                   textureFP->shaderModeY == TiledTextureEffect::ShaderMode::RepeatLinearMipmap;
    if (repeatX || textureFP->shaderModeX == TiledTextureEffect::ShaderMode::ClampToBorderLinear) {
      fragBuilder->codeAppend("float errX = subsetCoord.x - clampedCoord.x;");
      if (repeatX) {
        fragBuilder->codeAppendf("float repeatCoordX = errX > 0.0 ? %s.x : %s.z;",
                                 clampName.c_str(), clampName.c_str());
      }
    }
    if (repeatY || textureFP->shaderModeY == TiledTextureEffect::ShaderMode::ClampToBorderLinear) {
      fragBuilder->codeAppend("float errY = subsetCoord.y - clampedCoord.y;");
      if (repeatY) {
        fragBuilder->codeAppendf("float repeatCoordY = errY > 0.0 ? %s.y : %s.w;",
                                 clampName.c_str(), clampName.c_str());
      }
    }

    const char* ifStr = "if";
    if (repeatX && repeatY) {
      readColor(args, "vec2(repeatCoordX, clampedCoord.y)", repeatReadX);
      readColor(args, "vec2(clampedCoord.x, repeatCoordY)", repeatReadY);
      static const char* repeatReadXY = "repeatReadXY";
      readColor(args, "vec2(repeatCoordX, repeatCoordY)", repeatReadXY);
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
      readColor(args, "vec2(repeatCoordX, clampedCoord.y)", repeatReadX);
      fragBuilder->codeAppendf("textureColor = mix(textureColor, %s, errX);", repeatReadX);
      fragBuilder->codeAppend("}");
    }
    if (repeatY) {
      fragBuilder->codeAppendf("%s (errY != 0.0) {", ifStr);
      readColor(args, "vec2(clampedCoord.x, repeatCoordY)", repeatReadY);
      fragBuilder->codeAppendf("textureColor = mix(textureColor, %s, errY);", repeatReadY);
      fragBuilder->codeAppend("}");
    }

    if (textureFP->shaderModeX == TiledTextureEffect::ShaderMode::ClampToBorderLinear) {
      fragBuilder->codeAppend("textureColor = mix(textureColor, vec4(0.0), min(abs(errX), 1.0));");
    }
    if (textureFP->shaderModeY == TiledTextureEffect::ShaderMode::ClampToBorderLinear) {
      fragBuilder->codeAppendf("textureColor = mix(textureColor, vec4(0.0), min(abs(errY), 1.0));");
    }
    if (textureFP->shaderModeX == TiledTextureEffect::ShaderMode::ClampToBorderNearest) {
      fragBuilder->codeAppend("float snappedX = floor(inCoord.x + 0.001) + 0.5;");
      fragBuilder->codeAppendf("if (snappedX < %s.x || snappedX > %s.z) {", subsetName.c_str(),
                               subsetName.c_str());
      fragBuilder->codeAppend("textureColor = vec4(0.0);");  // border color
      fragBuilder->codeAppend("}");
    }
    if (textureFP->shaderModeY == TiledTextureEffect::ShaderMode::ClampToBorderNearest) {
      fragBuilder->codeAppend("float snappedY = floor(inCoord.y + 0.001) + 0.5;");
      fragBuilder->codeAppendf("if (snappedY < %s.y || snappedY > %s.w) {", subsetName.c_str(),
                               subsetName.c_str());
      fragBuilder->codeAppend("textureColor = vec4(0.0);");  // border color
      fragBuilder->codeAppend("}");
    }
    fragBuilder->codeAppendf("%s = textureColor * %s;", args.outputColor.c_str(),
                             args.inputColor.c_str());
  }
}

void GLTiledTextureEffect::onSetData(const ProgramDataManager& programDataManager,
                                     const FragmentProcessor& fragmentProcessor) {
  const auto& textureFP = static_cast<const TiledTextureEffect&>(fragmentProcessor);
  if (dimensionsUniform.isValid()) {
    auto dimensions = textureFP.texture->getTextureCoord(1.f, 1.f);
    if (dimensions != dimensionsPrev) {
      dimensionsPrev = dimensions;
      programDataManager.set2f(dimensionsUniform, dimensions.x, dimensions.y);
    }
  }
  auto pushRect = [&](Rect subset, std::optional<Rect>& prev, UniformHandle uni) {
    if (subset == prev) {
      return;
    }
    prev = subset;
    float rect[4] = {subset.left, subset.top, subset.right, subset.bottom};
    if (textureFP.texture->origin() == SurfaceOrigin::BottomLeft) {
      auto h = static_cast<float>(textureFP.texture->height());
      rect[1] = h - rect[1];
      rect[3] = h - rect[3];
      std::swap(rect[1], rect[3]);
    }
    auto type = textureFP.texture->getSampler()->type();
    if (!dimensionsUniform.isValid() && type != TextureType::Rectangle) {
      auto lt = textureFP.texture->getTextureCoord(static_cast<float>(rect[0]),
                                                   static_cast<float>(rect[1]));
      auto rb = textureFP.texture->getTextureCoord(static_cast<float>(rect[2]),
                                                   static_cast<float>(rect[3]));
      rect[0] = lt.x;
      rect[1] = lt.y;
      rect[2] = rb.x;
      rect[3] = rb.y;
    }
    programDataManager.set4fv(uni, 1, rect);
  };
  if (subsetUniform.isValid()) {
    pushRect(textureFP.subset, subsetPrev, subsetUniform);
  }
  if (clampUniform.isValid()) {
    pushRect(textureFP.clamp, clampPrev, clampUniform);
  }
}
}  // namespace tgfx
