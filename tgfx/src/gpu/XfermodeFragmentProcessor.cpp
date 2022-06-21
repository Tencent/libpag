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

#include "XfermodeFragmentProcessor.h"
#include "ConstColorProcessor.h"
#include "core/utils/UniqueID.h"
#include "opengl/GLXfermodeFragmentProcessor.h"

namespace tgfx {
std::unique_ptr<FragmentProcessor> XfermodeFragmentProcessor::MakeFromSrcProcessor(
    std::unique_ptr<FragmentProcessor> src, BlendMode mode) {
  return XfermodeFragmentProcessor::MakeFromTwoProcessors(std::move(src), nullptr, mode);
}

std::unique_ptr<FragmentProcessor> XfermodeFragmentProcessor::MakeFromDstProcessor(
    std::unique_ptr<FragmentProcessor> dst, BlendMode mode) {
  return XfermodeFragmentProcessor::MakeFromTwoProcessors(nullptr, std::move(dst), mode);
}

std::unique_ptr<FragmentProcessor> XfermodeFragmentProcessor::MakeFromTwoProcessors(
    std::unique_ptr<FragmentProcessor> src, std::unique_ptr<FragmentProcessor> dst,
    BlendMode mode) {
  if (src == nullptr && dst == nullptr) {
    return nullptr;
  }
  switch (mode) {
    case BlendMode::Clear:
      return ConstColorProcessor::Make(Color::Transparent(), InputMode::Ignore);
    case BlendMode::Src:
      return src;
    case BlendMode::Dst:
      return dst;
    default:
      return std::unique_ptr<XfermodeFragmentProcessor>(
          new XfermodeFragmentProcessor(std::move(src), std::move(dst), mode));
  }
}

std::string XfermodeFragmentProcessor::name() const {
  switch (child) {
    case Child::TwoChild:
      return "XfermodeFragmentProcessor - two";
    case Child::DstChild:
      return "XfermodeFragmentProcessor - dst";
    case Child::SrcChild:
      return "XfermodeFragmentProcessor - src";
  }
}

XfermodeFragmentProcessor::XfermodeFragmentProcessor(std::unique_ptr<FragmentProcessor> src,
                                                     std::unique_ptr<FragmentProcessor> dst,
                                                     BlendMode mode)
    : mode(mode) {
  if (src && dst) {
    child = Child::TwoChild;
    registerChildProcessor(std::move(src));
    registerChildProcessor(std::move(dst));
  } else if (src) {
    child = Child::SrcChild;
    registerChildProcessor(std::move(src));
  } else {
    child = Child::DstChild;
    registerChildProcessor(std::move(dst));
  }
}

void XfermodeFragmentProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  uint32_t flag = static_cast<uint32_t>(mode) | (static_cast<uint32_t>(child) << 16);
  bytesKey->write(flag);
}

std::unique_ptr<GLFragmentProcessor> XfermodeFragmentProcessor::onCreateGLInstance() const {
  return std::make_unique<GLXfermodeFragmentProcessor>();
}
}  // namespace tgfx
