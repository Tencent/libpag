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
  @Param pagBackground: ResourceColor = Color.Transparent;

  aboutToAppear(): void {
    this.controller.attachToView(new WeakRef(this));
    this.controller.pause();
  }

  build() {
    XComponent({
      id: this.controller.uniqueID(),
      type: XComponentType.SURFACE,
      libraryname: "pag",
    })
      .backgroundColor(this.pagBackground)
      .onVisibleAreaChange([0.0, 1.0], (isExpanding: boolean, currentRatio: number) => {
        if (isExpanding && currentRatio >= 0.0) {
          if (!this.controller.isPlaying()) {
            this.controller.play();
          }
        }
        if (!isExpanding && currentRatio <= 0.0) {
          if (this.controller.isPlaying()) {
            this.controller.pause();
          }
        }
      })
      .onLoad(() => {
        this.controller.update();
      })
  }

  onPageShow(): void {
    this.controller.update();
  }

  aboutToDisappear(): void {
    this.controller.detachFromView();
  }
}
