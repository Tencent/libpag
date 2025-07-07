/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "DisplayLinkWrapper.h"

static pag_display_link_functions* DisplayLinkFunctions = nullptr;

void pag_display_link_set_functions(pag_display_link_functions* functions) {
  DisplayLinkFunctions = functions;
}

namespace pag {
static void DisplayLinkWrapperUpdate(void* user) {
  if (user == nullptr) {
    return;
  }
  auto* wrapper = static_cast<DisplayLinkWrapper*>(user);
  wrapper->update();
}

std::shared_ptr<DisplayLink> DisplayLinkWrapper::Make(std::function<void()> callback) {
  if (DisplayLinkFunctions == nullptr || callback == nullptr) {
    return nullptr;
  }
  auto* ret = new DisplayLinkWrapper();
  if (auto* displayLink = DisplayLinkFunctions->create(ret, DisplayLinkWrapperUpdate)) {
    ret->displayLink = displayLink;
    ret->callback = std::move(callback);
    return std::shared_ptr<DisplayLink>(ret);
  }
  delete ret;
  return nullptr;
}

DisplayLinkWrapper::~DisplayLinkWrapper() {
  DisplayLinkWrapper::stop();
  DisplayLinkFunctions->release(displayLink);
}

void DisplayLinkWrapper::start() {
  if (started) {
    return;
  }
  started = true;
  DisplayLinkFunctions->start(displayLink);
}

void DisplayLinkWrapper::stop() {
  if (!started) {
    return;
  }
  started = false;
  DisplayLinkFunctions->stop(displayLink);
}

void DisplayLinkWrapper::update() {
  callback();
}
}  // namespace pag
