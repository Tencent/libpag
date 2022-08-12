import { WEBGL_CONTEXT_ATTRIBUTES } from '../constant';
import { destroyVerify } from '../decorators';
import { PAGFile } from '../pag-file';
import { FRAGMENT_2D_SHADER, FRAGMENT_2D_SHADER_TRANSPARENT, VERTEX_2D_SHADER } from './shader';
import { createAndSetupTexture, createProgram, detectWebGLContext, getShaderSourceFromString } from './utils';
import { RenderOptions, View } from './view';

@destroyVerify
export class PAGWebGLView extends View {
  private gl: WebGLRenderingContext;
  private program: WebGLProgram;
  private positionLocation = 0;
  private texcoordLocation = 0;
  private alphaStartLocation: WebGLUniformLocation | null = null;
  private positionBuffer: WebGLBuffer | null = null;
  private texcoordBuffer: WebGLBuffer | null = null;
  private originalVideoTexture: WebGLTexture | null = null;
  private renderingTexture: WebGLTexture | null = null;
  private renderingFbo: WebGLFramebuffer | null = null;

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement, options: RenderOptions) {
    if (detectWebGLContext() === false) throw new Error('WebGL is not supported!');
    super(pagFile, canvas, options);
    const gl = this.canvas?.getContext('webgl', {
      ...WEBGL_CONTEXT_ATTRIBUTES,
    });
    if (!gl) throw new Error("Can't get WebGL context!");
    this.gl = gl;
    if (this.videoParam.hasAlpha) {
      this.program = createProgram(
        this.gl,
        getShaderSourceFromString(VERTEX_2D_SHADER),
        getShaderSourceFromString(FRAGMENT_2D_SHADER_TRANSPARENT),
      );
    } else {
      this.program = createProgram(
        this.gl,
        getShaderSourceFromString(VERTEX_2D_SHADER),
        getShaderSourceFromString(FRAGMENT_2D_SHADER),
      );
    }
    this.loadContext();
  }

  protected override loadContext() {
    // look up where the vertex data needs to go.
    this.positionLocation = this.gl.getAttribLocation(this.program, 'a_position');
    if (this.positionLocation === -1) throw new Error('unable to get attribute location for a_position');
    this.texcoordLocation = this.gl.getAttribLocation(this.program, 'a_texCoord');
    if (this.texcoordLocation === -1) throw new Error('unable to get attribute location for a_texCoord');
    this.alphaStartLocation = this.gl.getUniformLocation(this.program, 'v_alphaStart');
    if (!this.alphaStartLocation) throw new Error('unable to get attribute location for v_alphaStart');

    // Create a buffer to put three 2d clip space points in
    this.positionBuffer = this.gl.createBuffer();

    // Bind it to ARRAY_BUFFER (think of it as ARRAY_BUFFER = positionBuffer)
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.positionBuffer);
    // Set a rectangle the same size as the image.
    this.setRectangle(this.gl, 0, 0, this.videoParam.MP4Width, this.videoParam.MP4Height);

    // provide texture coordinates for the rectangle.
    this.texcoordBuffer = this.gl.createBuffer();
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texcoordBuffer);
    this.gl.bufferData(
      this.gl.ARRAY_BUFFER,
      new Float32Array([0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0]),
      this.gl.STATIC_DRAW,
    );

    // Create texture and attach them to framebuffer.
    this.renderingTexture = createAndSetupTexture(this.gl);
    // make the texture the same size as the sequence.
    this.gl.texImage2D(
      this.gl.TEXTURE_2D,
      0,
      this.gl.RGBA,
      this.videoParam.sequenceWidth,
      this.videoParam.sequenceHeight,
      0,
      this.gl.RGBA,
      this.gl.UNSIGNED_BYTE,
      null,
    );
    // Create a framebuffer
    this.renderingFbo = this.gl.createFramebuffer();
    if (!this.renderingFbo) throw new Error('unable to create framebuffer');
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, this.renderingFbo);
    // Attach a texture to it.
    this.gl.framebufferTexture2D(
      this.gl.FRAMEBUFFER,
      this.gl.COLOR_ATTACHMENT0,
      this.gl.TEXTURE_2D,
      this.renderingTexture,
      0,
    );

    // Create a texture and put the video in it.
    this.originalVideoTexture = createAndSetupTexture(this.gl);
  }

  protected override flushInternal() {
    this.gl.bindTexture(this.gl.TEXTURE_2D, this.originalVideoTexture);
    // Upload the video into the texture.
    this.gl.texImage2D(
      this.gl.TEXTURE_2D,
      0,
      this.gl.RGBA,
      this.gl.RGBA,
      this.gl.UNSIGNED_BYTE,
      this.videoElement as HTMLVideoElement,
    );

    // lookup uniforms
    if (!this.program) throw new Error('program is not initialized');
    const resolutionLocation = this.gl.getUniformLocation(this.program, 'u_resolution');

    // Tell WebGL how to convert from clip space to pixels
    // this.gl.viewport(this.viewportSize.x, this.viewportSize.y, this.viewportSize.width, this.viewportSize.height);

    // Clear the canvas
    this.gl.clearColor(0, 0, 0, 0);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);

    // Tell it to use our program (pair of shaders)
    this.gl.useProgram(this.program);

    // Turn on the position attribute
    this.gl.enableVertexAttribArray(this.positionLocation);

    // Bind the position buffer
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.positionBuffer);

    // Tell the position attribute how to get data out of positionBuffer (ARRAY_BUFFER)
    const size = 2; // 2 components per iteration
    const type: number = this.gl.FLOAT; // the data is 32bit floats
    const normalize = false; // don't normalize the data
    const stride = 0; // 0 = move forward size * sizeof(type) each iteration to get the next position
    const offset = 0; // start at the beginning of the buffer
    this.gl.vertexAttribPointer(this.positionLocation, size, type, normalize, stride, offset);

    // Turn on the teccord attribute
    this.gl.enableVertexAttribArray(this.texcoordLocation);

    // Bind the position buffer.
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.texcoordBuffer);

    this.gl.vertexAttribPointer(this.texcoordLocation, size, type, normalize, stride, offset);

    if (this.videoParam.hasAlpha) {
      this.gl.uniform2f(
        this.alphaStartLocation,
        this.videoParam.alphaStartX / this.videoParam.MP4Width,
        this.videoParam.alphaStartY / this.videoParam.MP4Height,
      );
    }

    this.gl.bindTexture(this.gl.TEXTURE_2D, this.originalVideoTexture);
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, this.renderingFbo);
    this.gl.uniform2f(resolutionLocation, this.videoParam.sequenceWidth, this.videoParam.sequenceHeight);
    this.gl.viewport(0, 0, this.videoParam.sequenceWidth, this.videoParam.sequenceHeight);
    const primitiveType: number = this.gl.TRIANGLES;
    const count = 6;
    this.gl.drawArrays(primitiveType, offset, count);

    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    this.gl.uniform2f(resolutionLocation, this.videoParam.sequenceWidth, this.videoParam.sequenceHeight);
    this.gl.viewport(this.viewportSize.x, this.viewportSize.y, this.viewportSize.width, this.viewportSize.height);
    this.gl.drawArrays(primitiveType, offset, count);
  }

  protected override clearRender() {
    this.gl.clearColor(0, 0, 0, 0);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);
  }

  private setRectangle(gl: WebGLRenderingContext, x: number, y: number, width: number, height: number) {
    const x1: number = x;
    const x2: number = x + width;
    const y1: number = y;
    const y2: number = y + height;
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([x1, y1, x2, y1, x1, y2, x1, y2, x2, y1, x2, y2]), gl.STATIC_DRAW);
  }
}
