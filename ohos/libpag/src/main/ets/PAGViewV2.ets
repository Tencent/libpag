/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

import { PAGViewController } from './PAGViewController';

@ComponentV2
export struct PAGViewV2 {
  @Require @Param controller: PAGViewController;

  build() {
    XComponent({
      id: this.controller.uniqueID(),
      type: XComponentType.SURFACE,
      libraryname: "pag",
    })
      .backgroundColor(Color.Transparent)
      .onLoad(() => {
        this.controller.update();
      })
  }

  onPageShow(): void {
    this.controller.update();
  }

  aboutToAppear(): void {
    this.controller.attachToView(new WeakRef(this));
  }

  aboutToDisappear(): void {
    this.controller.detachFromView();
  }
}