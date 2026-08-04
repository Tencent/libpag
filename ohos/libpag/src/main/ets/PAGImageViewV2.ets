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

import { PAGImageViewController } from './PAGImageViewController'

@ComponentV2
export struct PAGImageViewV2 {
  @Require @Param controller!: PAGImageViewController;
  // 标记动画是否因组件不可见而被组件暂停，重新可见时只恢复组件自己暂停的播放。
  private pausedForInvisible: boolean = false;

  build() {
    XComponent({
      id: this.controller?.uniqueID(),
      type: XComponentType.SURFACE,
      libraryname: "pag",
    })
      .backgroundColor(Color.Transparent)
      .onVisibleAreaChange([0.0, 1.0], this.onVisibleAreaChanged)
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

  private onVisibleAreaChanged(isExpanding: boolean, currentRatio: number): void {
    if (currentRatio <= 0.0) {
      if (this.controller.isPlaying() && !this.pausedForInvisible) {
        this.pausedForInvisible = true;
        this.controller.pauseForInvisible();
      }
    } else if (isExpanding) {
      if (this.pausedForInvisible) {
        this.pausedForInvisible = false;
        if (this.controller.isUserWantsPlaying()) {
          this.controller.playForVisible();
        }
      }
    }
  }
}
