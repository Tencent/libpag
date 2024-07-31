import { JsPAGFile } from 'liblibpag.so'
import { PAGComposition } from './PAGComposition'
import { resourceManager } from '@kit.LocalizationKit';

export class PAGFile extends PAGComposition {
  constructor(nativeFile: JsPAGFile) {
    super(nativeFile);
    this.nativeFile = nativeFile;
  }

  static LoadFromPath(filePath: string): JsPAGFile | null {
    let native = JsPAGFile.LoadFromPath(filePath);
    if (native == null) {
      return null;
    }
    return new PAGFile(native);
  }

  static LoadFromBytes(data: Int8Array, filePath?: string): JsPAGFile | null {
    let native = JsPAGFile.LoadFromBytes(data, filePath);
    if (native == null) {
      return null;
    }
    return new PAGFile(native);
  }

  static LoadFromAssets(manager: resourceManager.ResourceManager, name: string): JsPAGFile | null {
    let native = JsPAGFile.LoadFromAssets(manager, name);
    if (native == null) {
      return null;
    }
    return new PAGFile(native);
  }

  readonly nativeFile: JsPAGFile;
}