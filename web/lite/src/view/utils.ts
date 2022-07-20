import type { PAGFile } from '../pag-file';
import type { VideoParam } from '../types';
import type { VideoSequence } from '../base/video-sequence';

export const detectWebGLContext = () => {
  const canvas: HTMLCanvasElement = document.createElement('canvas');
  const gl: RenderingContext | null = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
  return !!gl;
};

export const createProgram = (gl: WebGLRenderingContext, vertexShaderSource: string, fragmentShaderSource: string) => {
  const program = gl.createProgram();
  if (!program) throw new Error('Failed to create program.');
  const vShader = createShader(gl, vertexShaderSource, gl.VERTEX_SHADER);
  if (!vShader) throw new Error('Failed to create vertex shader.');
  gl.attachShader(program, vShader);
  const fShader = createShader(gl, fragmentShaderSource, gl.FRAGMENT_SHADER);
  if (!fShader) throw new Error('Failed to create fragment shader.');
  gl.attachShader(program, fShader);
  gl.linkProgram(program);

  const programMessage = gl.getProgramInfoLog(program);
  if (programMessage) console.log(programMessage);
  const vShaderMessage = gl.getShaderInfoLog(vShader);
  if (vShaderMessage) console.log(vShaderMessage);
  const fShaderMessage = gl.getShaderInfoLog(fShader);
  if (fShaderMessage) console.log(fShaderMessage);

  return program;
};

const createShader = (gl: WebGLRenderingContext, source: string, type: GLenum) => {
  const shader = gl.createShader(type);
  if (!shader) throw new Error('Failed to create shader.');
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  return shader;
};

export const getShaderSourceFromString = (str: string) => str.replace(/^\s+|\s+$/g, '');

export const getVideoParam = (pagFile: PAGFile, videoSequence: VideoSequence) => {
  const attribute: VideoParam = {
    width: pagFile.mainComposition.width,
    height: pagFile.mainComposition.height,
    hasAlpha: videoSequence.alphaStartX > 0 || videoSequence.alphaStartY > 0,
    alphaStartX: videoSequence.alphaStartX,
    alphaStartY: videoSequence.alphaStartY,
    sequenceWidth: videoSequence.width,
    sequenceHeight: videoSequence.height,
    MP4Width:
      (videoSequence.width + videoSequence.alphaStartX) % 2 === 0
        ? videoSequence.width + videoSequence.alphaStartX
        : videoSequence.width + videoSequence.alphaStartX + 1, // Must be even number
    MP4Height:
      (videoSequence.height + videoSequence.alphaStartY) % 2 === 0
        ? videoSequence.height + videoSequence.alphaStartY
        : videoSequence.height + videoSequence.alphaStartY + 1, // Must be even number
  };
  return attribute;
};

export const createAndSetupTexture = (gl: WebGLRenderingContext) => {
  const texture = gl.createTexture();
  if (!texture) throw new Error('Failed to create texture.');
  gl.bindTexture(gl.TEXTURE_2D, texture);

  // Set up texture so we can render any size image and so we are
  // working with pixels.
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
  return texture;
};

// Get video initiated token on Wechat browser.
export const getWechatNetwork = () => {
  return new Promise<void>((resolve) => {
    window.WeixinJSBridge.invoke(
      'getNetworkType',
      {},
      () => {
        resolve();
      },
      () => {
        resolve();
      },
    );
  });
};
