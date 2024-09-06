/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

import * as pag from 'libpag';

@Entry
@Component
struct Index {
  @State message: string = "";
  @State @Watch("updateMessage") stateString: string = "";
  @State @Watch("updateMessage") progress: number = 0;
  @State viewController: pag.PAGViewController = new pag.PAGViewController();

  aboutToAppear(): void {
    let manager = getContext(this).resourceManager;
    let file = pag.PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.viewController.setComposition(file);
    this.viewController.setRepeatCount(1);
    this.viewController.addListener(new WeakRef(this));
    this.viewController.play();
  }

  onAnimationStart = (viewController: pag.PAGViewController) => {
    this.stateString = viewController.uniqueID() + ` PAG start`;
  }
  onAnimationEnd = (viewController: pag.PAGViewController) => {
    this.stateString = viewController.uniqueID() + ` PAG end`;
  }
  onAnimationRepeat = (viewController: pag.PAGViewController) => {
    this.stateString = viewController.uniqueID() + ` PAG repeat`;
  }
  onAnimationCancel = (viewController: pag.PAGViewController) => {
    this.stateString = viewController.uniqueID() + ` PAG cancel`;
  }
  onAnimationUpdate = (viewController: pag.PAGViewController) => {
    this.progress = viewController.getProgress();
  }

  updateMessage() {
    this.message = this.stateString + ` progress ${this.progress.toFixed(2)}`;
  }

  build() {
    Row() {
      Column() {
        pag.PAGView({
          controller: this.viewController
        })
          .height('50%')
          .onClick(() => {
            if (this.viewController.isPlaying()) {
              this.viewController.pause();
            } else {
              this.viewController.play();
            }
          })

        Text(this.message)
          .fontSize(50)
          .fontWeight(FontWeight.Bold)
          .onClick(() => {
            this.viewController.setProgress(0.5);
            this.viewController.setRepeatCount(0);
          })
          .height('50%')
      }
      .width('100%')

    }
    .height('100%')
  }
}
