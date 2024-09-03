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
import { PAGComposition, PAGDiskCache } from 'libpag';

class Temp {
  constructor(composition: PAGComposition | null) {
    this.composition = composition;
  }

  public composition: pag.PAGComposition | null = null;
  public currentFrame: number = 0;
}

@Entry
@Component
struct Index {
  @State message: string = "";
  @State @Watch("updateMessage") stateString: string = "";
  @State compositions: Array<Temp> = Array<Temp>();
  @State @Watch("updateMessage") currentFrame: number = 0;
  @State isPlaying: boolean = false;
  @State repeatCount: number = 0;

  aboutToAppear(): void {
    // PAGDiskCache.RemoveAll();
    let manager = getContext(this).resourceManager;
    let file = pag.PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.compositions.push(new Temp(file));
    this.isPlaying = true;
  }

  onAnimationStart = (view: pag.PAGImageView) => {
    this.stateString = 'PAG start';
  }
  onAnimationEnd = (view: pag.PAGImageView) => {
    this.stateString = `PAG end`;
  }
  onAnimationRepeat = (view: pag.PAGImageView) => {
    this.stateString = `PAG repeat`;
  }
  onAnimationCancel = (view: pag.PAGImageView) => {
    this.stateString = `PAG cancel`;
  }
  onAnimationUpdate = (view: pag.PAGImageView) => {
  }

  updateMessage() {
    this.message = this.stateString + ` currentFrame ${this.currentFrame}`;
  }

  build() {
    Row() {
      Column() {
        Grid() {
          ForEach(this.compositions, (composition: Temp) => {
            GridItem() {
              pag.PAGImageView({
                composition: composition.composition,
                currentFrame: composition.currentFrame,
                isPlaying: this.isPlaying,
                repeatCount: this.repeatCount,
                listeners: [new WeakRef(this)]
              })// TestComponent({ testProperty: composition.currentFrame })
                .width('100%')
                .height('100%')
            }
          })
        }
      }


      //   Text(this.message)
      //     .fontSize(50)
      //     .fontWeight(FontWeight.Bold)
      //     .onClick(() => {
      //       this.currentFrame = 4;
      //       this.repeatCount = 0;
      //     })
      //     .height('50%')
      // }
      // .width('100%')
    }
  }
}