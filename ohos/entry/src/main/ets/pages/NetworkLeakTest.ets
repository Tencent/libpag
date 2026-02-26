/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
import { router } from '@kit.ArkUI';
import fs from '@ohos.file.fs';

// Network PAG URLs for testing
const NETWORK_PAG_URLS: string[] = [
  'https://pag.io/pagtest/1.pag',
  'https://pag.io/pagtest/2.pag',
  'https://pag.io/pagtest/3.pag',
  'https://pag.io/pagtest/4.pag',
  'https://pag.io/pagtest/5.pag',
  'https://pag.io/pagtest/6.pag',
  'https://pag.io/pagtest/7.pag',
  'https://pag.io/pagtest/8.pag',
  'https://pag.io/pagtest/9.pag',
  'https://pag.io/pagtest/10.pag',
  'https://pag.io/pagtest/11.pag',
  'https://pag.io/pagtest/12.pag',
  'https://pag.io/pagtest/13.pag',
  'https://pag.io/pagtest/14.pag',
  'https://pag.io/pagtest/15.pag',
  'https://pag.io/pagtest/16.pag',
  'https://pag.io/pagtest/17.pag',
  'https://pag.io/pagtest/18.pag',
  'https://pag.io/pagtest/19.pag',
  'https://pag.io/pagtest/20.pag',
];

@Entry
@ComponentV2
struct NetworkLeakTest {
  @Local viewControllers: Array<pag.PAGViewController> = [];
  @Local loadCount: number = 0;
  @Local fdCount: number = 0;
  @Local baseFdCount: number = 0;
  @Local isLoading: boolean = false;
  @Local useNetwork: boolean = true;
  private fdMonitorTimer: number = -1;

  aboutToAppear(): void {
    this.baseFdCount = this.getFdCount();
    this.fdCount = this.baseFdCount;
    this.startFdMonitor();
  }

  aboutToDisappear(): void {
    this.stopFdMonitor();
    this.releaseAll();
  }

  // Get current FD count by reading /proc/self/fd
  getFdCount(): number {
    try {
      let fdPath = '/proc/self/fd';
      let files = fs.listFileSync(fdPath);
      return files.length;
    } catch (e) {
      console.error(`[NetworkLeakTest] getFdCount error: ${e}`);
      return -1;
    }
  }

  // Start FD monitoring timer
  startFdMonitor(): void {
    this.fdMonitorTimer = setInterval(() => {
      this.fdCount = this.getFdCount();
    }, 1000);
  }

  // Stop FD monitoring timer
  stopFdMonitor(): void {
    if (this.fdMonitorTimer !== -1) {
      clearInterval(this.fdMonitorTimer);
      this.fdMonitorTimer = -1;
    }
  }

  // Load PAG from network URL
  async loadFromNetwork(url: string): Promise<void> {
    try {
      console.info(`[NetworkLeakTest] Loading from network: ${url}`);
      let file = await pag.PAGFile.LoadFromPathAsync(url);
      if (file) {
        let controller = new pag.PAGViewController();
        controller.setComposition(file);
        controller.setRepeatCount(-1);
        this.viewControllers.push(controller);
        controller.play();
        this.loadCount++;
        console.info(`[NetworkLeakTest] Loaded successfully, total: ${this.loadCount}`);
      }
    } catch (error) {
      console.error(`[NetworkLeakTest] Load failed: ${error}`);
    }
  }

  // Load PAG from local assets
  loadFromLocal(index: number): void {
    try {
      let manager = getContext(this).resourceManager;
      let file = pag.PAGFile.LoadFromAssets(manager, "list/" + index + ".pag");
      if (file) {
        let controller = new pag.PAGViewController();
        controller.setComposition(file);
        controller.setRepeatCount(-1);
        this.viewControllers.push(controller);
        controller.play();
        this.loadCount++;
        console.info(`[NetworkLeakTest] Loaded local file ${index}, total: ${this.loadCount}`);
      }
    } catch (error) {
      console.error(`[NetworkLeakTest] Load local failed: ${error}`);
    }
  }

  // Load multiple PAGs from network
  async loadMultipleNetwork(): Promise<void> {
    if (this.isLoading) {
      return;
    }
    this.isLoading = true;

    for (let url of NETWORK_PAG_URLS) {
      await this.loadFromNetwork(url);
    }

    this.isLoading = false;
  }

  // Load multiple PAGs from local assets
  loadMultipleLocal(): void {
    if (this.isLoading) {
      return;
    }
    this.isLoading = true;

    for (let index = 0; index < 20; index++) {
      this.loadFromLocal(index);
    }

    this.isLoading = false;
  }

  // Release all controllers
  releaseAll(): void {
    for (let controller of this.viewControllers) {
      try {
        controller.pause();
        controller.release();
      } catch (error) {
        console.error(`[NetworkLeakTest] Release failed: ${error}`);
      }
    }
    this.viewControllers = [];
    this.loadCount = 0;
  }

  // Reset baseline FD count
  resetBaseline(): void {
    this.baseFdCount = this.getFdCount();
  }

  build() {
    Column() {
      // Title
      Text('PAG FD Leak Test')
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .margin({ top: 20, bottom: 10 })

      // Mode toggle
      Row() {
        Text('Mode: ')
          .fontSize(14)
        Button(this.useNetwork ? 'Network' : 'Local')
          .fontSize(12)
          .backgroundColor(this.useNetwork ? '#FF6600' : '#00AA00')
          .onClick(() => {
            this.useNetwork = !this.useNetwork;
          })
      }
      .margin({ bottom: 10 })

      // FD Status Panel
      Column() {
        Row() {
          Text('Current FD: ')
            .fontSize(16)
          Text(`${this.fdCount}`)
            .fontSize(20)
            .fontWeight(FontWeight.Bold)
            .fontColor(this.fdCount > this.baseFdCount + 50 ? '#FF0000' : '#00AA00')
        }

        Row() {
          Text('Baseline FD: ')
            .fontSize(14)
            .fontColor('#666666')
          Text(`${this.baseFdCount}`)
            .fontSize(14)
            .fontColor('#666666')
        }
        .margin({ top: 5 })

        Row() {
          Text('FD Increase: ')
            .fontSize(14)
            .fontColor('#666666')
          Text(`+${this.fdCount - this.baseFdCount}`)
            .fontSize(14)
            .fontWeight(FontWeight.Bold)
            .fontColor(this.fdCount > this.baseFdCount + 20 ? '#FF6600' : '#666666')
        }
        .margin({ top: 5 })

        Row() {
          Text('Loaded PAGs: ')
            .fontSize(14)
            .fontColor('#666666')
          Text(`${this.loadCount}`)
            .fontSize(14)
            .fontColor('#666666')
        }
        .margin({ top: 5 })
      }
      .padding(15)
      .margin({ bottom: 10 })
      .backgroundColor('#F5F5F5')
      .borderRadius(10)
      .width('90%')

      // PAG display grid
      if (this.viewControllers.length > 0) {
        Grid() {
          ForEach(this.viewControllers, (controller: pag.PAGViewController, index: number) => {
            GridItem() {
              pag.PAGViewV2({
                controller: controller
              })
            }
            .width('25%')
            .height(80)
          })
        }
        .width('100%')
        .height('30%')
      } else {
        Text('No PAG loaded')
          .fontSize(16)
          .fontColor('#999999')
          .height('30%')
      }

      // Control buttons
      Column() {
        Button(this.useNetwork ? 'Load 20 Network PAGs' : 'Load 20 Local PAGs')
          .width('80%')
          .margin({ bottom: 10 })
          .enabled(!this.isLoading)
          .backgroundColor(this.useNetwork ? '#FF6600' : '#00AA00')
          .onClick(() => {
            if (this.useNetwork) {
              this.loadMultipleNetwork();
            } else {
              this.loadMultipleLocal();
            }
          })

        Button('Release All')
          .width('80%')
          .margin({ bottom: 10 })
          .onClick(() => {
            this.releaseAll();
          })

        Button('Reset FD Baseline')
          .width('80%')
          .margin({ bottom: 10 })
          .backgroundColor('#666666')
          .onClick(() => {
            this.resetBaseline();
          })

        Button('Back to Index')
          .width('80%')
          .backgroundColor('#888888')
          .onClick(() => {
            router.back();
          })
      }
      .margin({ top: 10 })

      // Instructions
      Text('Compare Local vs Network:\n1. Test Local mode first (green) - note FD change\n2. Release All, reset baseline\n3. Test Network mode (orange) - compare FD change\n4. If Network FD does not drop after release = FD leak')
        .fontSize(10)
        .fontColor('#999999')
        .margin({ top: 10 })
        .padding(10)
    }
    .width('100%')
    .height('100%')
    .padding(10)
  }
}
