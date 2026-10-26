import { IPHONE } from './ua';

import type { PAG } from '../types';

export const setMixin = (module: PAG) => {
  module.traceImage = function (info, pixels) {
    const canvas = document.createElement('canvas');
    canvas.width = info.width;
    canvas.height = info.height;
    const context = canvas.getContext('2d') as CanvasRenderingContext2D;
    const imageData = new ImageData(new Uint8ClampedArray(pixels), canvas.width, canvas.height);
    context.putImageData(imageData, 0, 0);
    document.body.appendChild(canvas);
  };
  module.registerSoftwareDecoderFactory = function (factory = null) {
    module._registerSoftwareDecoderFactory(factory);
    module._useSoftwareDecoder = true;
  };
  module.SDKVersion = function () {
    return module._SDKVersion();
  };
  module.isIPhone = () => IPHONE;
};
