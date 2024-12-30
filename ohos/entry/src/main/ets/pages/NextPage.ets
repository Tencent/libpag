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

import { PAGFile, PAGImageViewController, PAGImageView , PAGView, PAGViewController} from 'libpag';

@Entry
@Component
export struct NextPage {
  @State imageViewControllers: Array<PAGImageViewController> = new Array<PAGImageViewController>();
  @State viewController: PAGViewController = new PAGViewController();
  @State pageIndex: number = 0;
  @Consume('pageInfos') pageInfos: NavPathStack;

  aboutToAppear(): void {
    let manager = getContext(this).resourceManager;
    this.viewController = new PAGViewController();
    let file = PAGFile.LoadFromAssets(manager, "castle.pag");
    this.viewController.setComposition(file);
    this.viewController.setRepeatCount(-1);
    this.viewController.play();
    // this.viewController.setProgress(0);
    // this.viewController.flush();
  }

  aboutToDisappear(): void {
    console.log("--------------NextPage aboutToDisappear ----------------\n");
    // this.viewController.release();
  }

  build() {
    NavDestination() {
      Column() {
        Row() {
            PAGView({
              controller: this.viewController
            }).onClick(() => {
              if (this.viewController.isPlaying()) {
                this.viewController.pause();
              } else {
                this.viewController.play();
              }
            })
        }.height("70%")


        Row() {
          Button("PAGView").width("40%").margin(10)
            .onClick(() => {
              this.pageIndex = 0;
              this.viewController.play();
              this.imageViewControllers.forEach((item: PAGImageViewController) => {
                item.pause();
              })
            })
        }

      }
    }.title("NextPage")
    .onBackPressed(() => {
      const popDestinationInfo = this.pageInfos.pop() // 弹出路由栈栈顶元素
      console.log('pop' + '返回值' + JSON.stringify(popDestinationInfo))
      return true
    })
  }
}
