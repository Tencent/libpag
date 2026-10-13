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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import * as pag from 'libpag';
import { PAGScaleMode } from 'libpag/src/main/ets/PAGScaleMode';

@Entry
@Component
struct Index {
  @State imageViewControllers: Array<pag.PAGImageViewController> = new Array<pag.PAGImageViewController>();
  @State viewController: pag.PAGViewController = new pag.PAGViewController();
  @State scaleTestView: pag.PAGViewController = new pag.PAGViewController();
  @State scaleTestImageView: pag.PAGImageViewController = new pag.PAGImageViewController();
  @State pageIndex: number = 0;
  @State matrixInfo: string = "";
  @State dumpPixelMap: PixelMap | null = null;

  updateMatrixInfo(): void {
    let viewMatrix = this.scaleTestView.matrix();
    let imageMatrix = this.scaleTestImageView.matrix();
    let file = pag.PAGFile.LoadFromAssets(getContext(this).resourceManager, "PAG_LOGO.pag");
    let sizeInfo = file != null ? "comp " + file.width() + "x" + file.height() : "comp ?";
    this.matrixInfo = sizeInfo
      + " | view(" + viewMatrix[0].toFixed(3) + "," + viewMatrix[5].toFixed(3) + ")"
      + " image(" + imageMatrix[0].toFixed(3) + "," + imageMatrix[5].toFixed(3) + ")";
  }

  aboutToAppear(): void {
    let manager = getContext(this).resourceManager;
    let file = pag.PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.viewController.setComposition(file);
    this.viewController.setRepeatCount(-1);
    this.viewController.play();

    let fileForView = pag.PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.scaleTestView.setComposition(fileForView);
    this.scaleTestView.setRepeatCount(-1);
    this.scaleTestView.play();

    let fileForImageView = pag.PAGFile.LoadFromAssets(manager, "PAG_LOGO.pag");
    this.scaleTestImageView.setComposition(fileForImageView);
    this.scaleTestImageView.setRepeatCount(-1);
    this.scaleTestImageView.play();

    for (let index = 0; index < 20; index++) {
      let imageViewController = new pag.PAGImageViewController();
      let file = pag.PAGFile.LoadFromAssets(manager, "list/" + index + ".pag");
      imageViewController.setComposition(file);
      imageViewController.setRepeatCount(-1);
      this.imageViewControllers.push(imageViewController);
    }
  }

  setScaleModeForBoth(scaleMode: PAGScaleMode): void {
    this.scaleTestView.setScaleMode(scaleMode);
    this.scaleTestImageView.setScaleMode(scaleMode);
  }

  build() {
    Column() {
      Row() {
        if (this.pageIndex == 2) {
          Row() {
            Column() {
              Text("PAGView").fontSize(16).fontColor("#666666")
              pag.PAGView({
                controller: this.scaleTestView
              }).width("100%").height("90%")
            }.width("50%").height("100%")

            Column() {
              Text("PAGImageView").fontSize(16).fontColor("#666666")
              pag.PAGImageView({
                controller: this.scaleTestImageView
              }).width("100%").height("90%")
            }.width("50%").height("100%")
          }.width("100%").height("100%")
        } else if (this.pageIndex == 1) {
          Grid() {
            ForEach(this.imageViewControllers, (item: pag.PAGImageViewController) => {
              GridItem() {
                pag.PAGImageView({
                  controller: item
                })
                  .onClick(() => {
                    if (item.isPlaying()) {
                      item.pause();
                    } else {
                      item.play();
                    }
                  })
              }.width("25%").height(100)
            })
          }.width("100%")
        } else {
          pag.PAGView({
            controller: this.viewController
          }).onClick(() => {
            if (this.viewController.isPlaying()) {
              this.viewController.pause();
            } else {
              this.viewController.play();
            }
          })
        }
      }.height("70%")

      if (this.pageIndex == 2) {
        Column() {
          Row() {
            Button("LetterBox").fontSize(14).margin(6).onClick(() => {
              this.setScaleModeForBoth(PAGScaleMode.LetterBox);
              this.updateMatrixInfo();
            })
            Button("Zoom").fontSize(14).margin(6).onClick(() => {
              this.setScaleModeForBoth(PAGScaleMode.Zoom);
              this.updateMatrixInfo();
            })
            Button("None").fontSize(14).margin(6).onClick(() => {
              this.setScaleModeForBoth(PAGScaleMode.None);
              this.updateMatrixInfo();
            })
          }
          Row() {
            Button("LetterBox then None").fontSize(14).margin(6).onClick(() => {
              this.setScaleModeForBoth(PAGScaleMode.LetterBox);
              this.setScaleModeForBoth(PAGScaleMode.None);
              this.updateMatrixInfo();
            })
            Button("Matrix then None").fontSize(14).margin(6).onClick(() => {
              this.scaleTestView.setScaleMode(PAGScaleMode.Zoom);
              this.scaleTestImageView.setScaleMode(PAGScaleMode.Zoom);
              this.scaleTestView.setMatrix(this.scaleTestView.matrix());
              this.scaleTestImageView.setMatrix(this.scaleTestImageView.matrix());
              this.setScaleModeForBoth(PAGScaleMode.None);
              this.updateMatrixInfo();
            })
          }
          Text(this.matrixInfo).fontSize(13).fontColor("#333333").margin(8)
          Row() {
            Button("Dump Bitmap").fontSize(14).margin(6).onClick(() => {
              this.dumpPixelMap = this.scaleTestImageView.currentImage();
            })
            if (this.dumpPixelMap != null) {
              Image(this.dumpPixelMap).width(120).height(120).border({ width: 1, color: "#FF0000" })
            }
          }
        }
      } else {
        Row() {
          Button("PAGView").width("26%").margin(8)
            .onClick(() => {
              this.pageIndex = 0;
              this.viewController.play();
              this.imageViewControllers.forEach((item: pag.PAGImageViewController) => {
                item.pause();
              })
            })

          Button("PAGImageView").width("34%")
            .onClick(() => {
              this.pageIndex = 1;
              this.imageViewControllers.forEach((item: pag.PAGImageViewController) => {
                item.play();
              })
              this.viewController.pause();
            })

          Button("ScaleMode").width("30%").margin(8)
            .onClick(() => {
              this.pageIndex = 2;
              this.scaleTestView.play();
              this.scaleTestImageView.play();
              this.viewController.pause();
              this.imageViewControllers.forEach((item: pag.PAGImageViewController) => {
                item.pause();
              })
            })
        }
      }

    }
  }
}
