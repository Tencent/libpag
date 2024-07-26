import hello2d from 'libhello2d.so';
import display from '@ohos.display';
import { common } from '@kit.AbilityKit';

@Entry
@Component
struct Index {
  @State message: string = 'Hello TGFX';
  private drawIndex: number = 0;

  build() {
    Row() {
      Column() {
        XComponent({
          id: "xcomponent",
          type: XComponentType.SURFACE,
          libraryname: "hello2d"
        })
      }
      .width('100%')
      .onClick(() => {
        hello2d.draw(this.drawIndex++);
      })
    }
    .height('100%')
  }
}
