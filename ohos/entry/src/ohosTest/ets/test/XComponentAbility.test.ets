/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import AbilityDelegatorRegistry from '@ohos.app.ability.abilityDelegatorRegistry';
import { describe, it, expect } from '@ohos/hypium';
import { Driver, ON } from '@ohos.UiTest';

const TAG = '[Sample_NDK_XComponent]';

export default function abilityTest() {

  describe('ActsAbilityTest', () => {
    /**
     * 打开应用
     */
    it('StartAbility_001', 0, async (done: Function) => {
      console.info(TAG, 'StartAbility_001 begin');
      let driver = Driver.create();
      let abilityDelegator = AbilityDelegatorRegistry.getAbilityDelegator();
      try {
        await abilityDelegator.startAbility({
          bundleName: 'com.samples.ndkxcomponent',
          abilityName: 'EntryAbility'
        });
      } catch (exception) {
        console.info(TAG, `StartAbility_001 exception = ${JSON.stringify(exception)}`);
        expect().assertFail();
      }
      await driver.delayMs(1000);
      await driver.assertComponentExist(ON.text('Draw Star'));
      done();
      console.info(TAG, 'StartAbility_001 end');
    })

    /**
     * 点击按钮，绘制图形，之后点击XComponent改变颜色
     */
    it('DrawShape_001', 2, async () => {
      console.info(TAG, 'CreateFiles_001 begin');
      let driver = Driver.create();
      // 判断XComponent onLoad回调执行成功
      await driver.assertComponentExist(ON.text('index'));
      // 判断是否有按键
      await driver.assertComponentExist(ON.text('Draw Star'));
      let drawStarBtn = await driver.findComponent(ON.text('Draw Star'));
      // 点击'Draw Star'按钮
      await drawStarBtn.click();
      await driver.delayMs(1000);
      // 判断drawPattern方法已执行
      await driver.assertComponentExist(ON.text('draw star'));

      // 判断是否有XComponent组件
      let xcomponent = await driver.findComponent(ON.id('xcomponent'));
      // 点击XComponent组件
      await xcomponent.click();
      await driver.delayMs(1000);
      // 判断touch回调已执行
      await driver.assertComponentExist(ON.text('change color'));
      console.info(TAG, 'DrawShape_001 end');
    })
  })
}