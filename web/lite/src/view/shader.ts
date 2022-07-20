export const VERTEX_2D_SHADER = `
      attribute vec2 a_position;
      attribute vec2 a_texCoord;
      
      uniform vec2 u_resolution;
      
      varying vec2 v_texCoord;
    
      
      void main() {
         // convert the rectangle from pixels to 0.0 to 1.0
         vec2 zeroToOne = a_position / u_resolution;
      
         // convert from 0->1 to 0->2
         vec2 zeroToTwo = zeroToOne * 2.0;
      
         // convert from 0->2 to -1->+1 (clipspace)
         vec2 clipSpace = zeroToTwo - 1.0;
      
         gl_Position = vec4(clipSpace * vec2(1, -1), 0, 1);
      
         // pass the texCoord to the fragment shader
         // The GPU will interpolate this value between points.
         v_texCoord = a_texCoord;
      }
        `;

export const FRAGMENT_2D_SHADER = `
      precision mediump float;

      // our texture
      uniform sampler2D u_image;
      
      // the texCoords passed in from the vertex shader.
      varying vec2 v_texCoord;
      
      void main() {
         gl_FragColor = texture2D(u_image, v_texCoord);
      }
         `;
export const FRAGMENT_2D_SHADER_TRANSPARENT = `
      precision mediump float;
      // our texture
      uniform sampler2D u_image;
      
      // the texCoords passed in from the vertex shader.
      varying vec2 v_texCoord;
      uniform vec2 v_alphaStart;
      
      void main() {
         vec4 color = texture2D(u_image, v_texCoord);
         vec4 alpha = texture2D(u_image, vec2(v_texCoord.x + v_alphaStart.x, v_texCoord.y + v_alphaStart.y));
         gl_FragColor = vec4(color.rgb * alpha.r, alpha.r);
      }     
         `;
