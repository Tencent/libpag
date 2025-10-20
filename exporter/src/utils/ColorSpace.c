/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "ColorSpace.h"

#include <assert.h>
#include <string.h>

/* function pointers */

///* input */
//ColorConvertFuncPtr RGB555ToYUV420;
//ColorConvertFuncPtr RGB565ToYUV420;
//ColorConvertFuncPtr BGRToYUV420;
//ColorConvertFuncPtr BGRAToYUV420;
//ColorConvertFuncPtr ABGRToYUV420;
//ColorConvertFuncPtr RGBAToYUV420;
//ColorConvertFuncPtr ARGBToYUV420;
//ColorConvertFuncPtr YUVToYUV420;
//ColorConvertFuncPtr YUYVToYUV420;
//ColorConvertFuncPtr UYVYToYUV420;
//
//ColorConvertFuncPtr RGB555IToYUV420;
//ColorConvertFuncPtr RGB565IToYUV420;
//ColorConvertFuncPtr BGRIToYUV420;
//ColorConvertFuncPtr BGRAIToYUV420;
//ColorConvertFuncPtr ABGRIToYUV420;
//ColorConvertFuncPtr RGBAIToYUV420;
//ColorConvertFuncPtr ARGBIToYUV420;
//ColorConvertFuncPtr YUYVIToYUV420;
//ColorConvertFuncPtr UYVYIToYUV420;
//
//ColorConvertFuncPtr YUYVToBGRA;
//
///* output */
//ColorConvertFuncPtr YUV420ToRGB555;
//ColorConvertFuncPtr YUV420ToRGB565;
//ColorConvertFuncPtr YUV420ToRGB;
//ColorConvertFuncPtr YUV420ToBGR;
//ColorConvertFuncPtr YUV420ToBGRA;
//ColorConvertFuncPtr YUV420ToABGR;
//ColorConvertFuncPtr YUV420ToRGBA;
//ColorConvertFuncPtr YUV420ToARGB;
//ColorConvertFuncPtr YUV420ToYUV;
//ColorConvertFuncPtr YUV420ToYUYV;
//ColorConvertFuncPtr YUV420ToUYVY;
//
//ColorConvertFuncPtr YUV420ToRGB555I;
//ColorConvertFuncPtr YUV420ToRGB565I;
//ColorConvertFuncPtr YUV420ToBGRI;
//ColorConvertFuncPtr YUV420ToBGRAI;
//ColorConvertFuncPtr YUV420ToABGRI;
//ColorConvertFuncPtr YUV420ToRGBAI;
//ColorConvertFuncPtr YUV420ToARGBI;
//ColorConvertFuncPtr YUV420ToYUYVI;
//ColorConvertFuncPtr YUV420ToUYVYI;

int32_t RGB_Y_tab[256];
int32_t B_U_tab[256];
int32_t G_U_tab[256];
int32_t G_V_tab[256];
int32_t R_V_tab[256];

/********** generic colorspace macro **********/

#define MAKE_COLORSPACE(NAME, SIZE, PIXELS, VPIXELS, FUNC, C1, C2, C3, C4) \
void        \
NAME        \
(          \
  uint8_t  * x_ptr,  \
  int x_stride,  \
  uint8_t  * y_ptr,  \
  uint8_t  * u_ptr,  \
  uint8_t  * v_ptr,  \
  int y_stride,  \
  int uv_stride,  \
  int width,    \
  int height,    \
  int vflip    \
)          \
{          \
  int fixed_width = (width + 1) & ~1;        \
  int x_dif = x_stride - (SIZE) * fixed_width;  \
  int y_dif = y_stride - fixed_width;        \
  int uv_dif = uv_stride - (fixed_width / 2);    \
  int x, y;                    \
  if (vflip)                    \
  {                        \
    x_ptr += (height - 1) * x_stride;      \
    x_dif = -(SIZE)*fixed_width - x_stride;    \
    x_stride = -x_stride;            \
  }                        \
  for (y = 0; y < height; y+=(VPIXELS))      \
  {                        \
    FUNC##_ROW(SIZE,C1,C2,C3,C4);        \
    for (x = 0; x < fixed_width; x+=(PIXELS))  \
    {                      \
      FUNC(SIZE,C1,C2,C3,C4);          \
      x_ptr += (PIXELS)*(SIZE);        \
      y_ptr += (PIXELS);            \
      u_ptr += (PIXELS)/2;          \
      v_ptr += (PIXELS)/2;          \
    }                      \
    x_ptr += x_dif + (VPIXELS-1)*x_stride;    \
    y_ptr += y_dif + (VPIXELS-1)*y_stride;    \
    u_ptr += uv_dif + ((VPIXELS/2)-1)*uv_stride;\
    v_ptr += uv_dif + ((VPIXELS/2)-1)*uv_stride;\
  }                        \
}



/********** colorspace input (xxx_to_yv12) functions **********/

/*	rgb -> yuv def's

	this following constants are "official spec"
	Video Demystified" (ISBN 1-878707-09-4)

	rgb<->yuv _is_ lossy, since most programs do the conversion differently

	SCALEBITS/FIX taken from  ffmpeg
*/

#ifdef COLOR_FULL_RANGE

#define Y_R_IN            0.299
#define Y_G_IN            0.587
#define Y_B_IN            0.114
#define Y_ADD_IN          0

#define U_R_IN            0.1687
#define U_G_IN            0.3313
#define U_B_IN            0.5
#define U_ADD_IN          128

#define V_R_IN            0.5
#define V_G_IN            0.4187
#define V_B_IN            0.0813
#define V_ADD_IN          128

#else

#define Y_R_IN            0.257
#define Y_G_IN            0.504
#define Y_B_IN            0.098
#define Y_ADD_IN          16

#define U_R_IN            0.148
#define U_G_IN            0.291
#define U_B_IN            0.439
#define U_ADD_IN          128

#define V_R_IN            0.439
#define V_G_IN            0.368
#define V_B_IN            0.071
#define V_ADD_IN          128

#endif

#define SCALEBITS_IN  8
#define FIX_IN(x)    ((uint16_t) ((x) * (1L<<SCALEBITS_IN) + 0.5))  //x*2^8


/* rgb16/rgb16i input */

#define MK_RGB555_B(RGB)  (((RGB) << 3) & 0xf8)
#define MK_RGB555_G(RGB)  (((RGB) >> 2) & 0xf8)
#define MK_RGB555_R(RGB)  (((RGB) >> 7) & 0xf8)

#define MK_RGB565_B(RGB)  (((RGB) << 3) & 0xf8)
#define MK_RGB565_G(RGB)  (((RGB) >> 3) & 0xfc)
#define MK_RGB565_R(RGB)  (((RGB) >> 8) & 0xf8)


#define READ_RGB16_Y(ROW, UVID, C1, C2, C3, C4)              \
  rgb = *(uint16_t *) (x_ptr + ((ROW)*x_stride) + 0);          \
  b##UVID += b = C1##_B(rgb);        \
  g##UVID += g = C1##_G(rgb);        \
  r##UVID += r = C1##_R(rgb);        \
  y_ptr[(ROW)*y_stride+0] =        \
    (uint8_t) ((FIX_IN(Y_R_IN) * r + FIX_IN(Y_G_IN) * g +      \
          FIX_IN(Y_B_IN) * b) >> SCALEBITS_IN) + Y_ADD_IN;  \
  rgb = *(uint16_t *) (x_ptr + ((ROW)*x_stride) + 2);          \
  b##UVID += b = C1##_B(rgb);        \
  g##UVID += g = C1##_G(rgb);        \
  r##UVID += r = C1##_R(rgb);        \
  y_ptr[(ROW)*y_stride+1] =        \
    (uint8_t) ((FIX_IN(Y_R_IN) * r + FIX_IN(Y_G_IN) * g +      \
          FIX_IN(Y_B_IN) * b) >> SCALEBITS_IN) + Y_ADD_IN;

#define READ_RGB16_UV(UV_ROW, UVID)      \
  u_ptr[(UV_ROW)*uv_stride] =        \
    (uint8_t) ((-FIX_IN(U_R_IN) * r##UVID - FIX_IN(U_G_IN) * g##UVID +      \
          FIX_IN(U_B_IN) * b##UVID) >> (SCALEBITS_IN + 2)) + U_ADD_IN;  \
  v_ptr[(UV_ROW)*uv_stride] =                            \
    (uint8_t) ((FIX_IN(V_R_IN) * r##UVID - FIX_IN(V_G_IN) * g##UVID -      \
          FIX_IN(V_B_IN) * b##UVID) >> (SCALEBITS_IN + 2)) + V_ADD_IN;

#define RGB16_TO_YV12_ROW(SIZE, C1, C2, C3, C4) \
  /* nothing */

#define RGB16_TO_YV12(SIZE, C1, C2, C3, C4)    \
  uint32_t rgb, r, g, b, r0, g0, b0;    \
  r0 = g0 = b0 = 0;            \
  READ_RGB16_Y (0, 0, C1,C2,C3,C4)    \
  READ_RGB16_Y (1, 0, C1,C2,C3,C4)    \
  READ_RGB16_UV(0, 0)


#define RGB16I_TO_YV12_ROW(SIZE, C1, C2, C3, C4)\
  /* nothing */

#define RGB16I_TO_YV12(SIZE, C1, C2, C3, C4)      \
  uint32_t rgb, r, g, b, r0, g0, b0, r1, g1, b1;  \
  r0 = g0 = b0 = r1 = g1 = b1 = 0;    \
  READ_RGB16_Y (0, 0, C1,C2,C3,C4)    \
  READ_RGB16_Y (1, 1, C1,C2,C3,C4)    \
  READ_RGB16_Y (2, 0, C1,C2,C3,C4)    \
  READ_RGB16_Y (3, 1, C1,C2,C3,C4)    \
  READ_RGB16_UV(0, 0)            \
  READ_RGB16_UV(1, 1)


/* rgb/rgbi input */

#define READ_RGB_Y(SIZE, ROW, UVID, C1, C2, C3, C4)          \
  r##UVID += r = x_ptr[(ROW)*x_stride+(C1)];            \
  g##UVID += g = x_ptr[(ROW)*x_stride+(C2)];            \
  b##UVID += b = x_ptr[(ROW)*x_stride+(C3)];            \
  y_ptr[(ROW)*y_stride+0] =                    \
    (uint8_t) ((FIX_IN(Y_R_IN) * r + FIX_IN(Y_G_IN) * g +    \
          FIX_IN(Y_B_IN) * b) >> SCALEBITS_IN) + Y_ADD_IN;\
  r##UVID += r = x_ptr[(ROW)*x_stride+(SIZE)+(C1)];        \
  g##UVID += g = x_ptr[(ROW)*x_stride+(SIZE)+(C2)];        \
  b##UVID += b = x_ptr[(ROW)*x_stride+(SIZE)+(C3)];        \
  y_ptr[(ROW)*y_stride+1] =                    \
    (uint8_t) ((FIX_IN(Y_R_IN) * r + FIX_IN(Y_G_IN) * g +    \
          FIX_IN(Y_B_IN) * b) >> SCALEBITS_IN) + Y_ADD_IN;

#define READ_RGB_UV(UV_ROW, UVID)        \
  u_ptr[(UV_ROW)*uv_stride] =          \
    (uint8_t) ((-FIX_IN(U_R_IN) * r##UVID - FIX_IN(U_G_IN) * g##UVID +      \
          FIX_IN(U_B_IN) * b##UVID) >> (SCALEBITS_IN + 2)) + U_ADD_IN;  \
  v_ptr[(UV_ROW)*uv_stride] =                            \
    (uint8_t) ((FIX_IN(V_R_IN) * r##UVID - FIX_IN(V_G_IN) * g##UVID -      \
          FIX_IN(V_B_IN) * b##UVID) >> (SCALEBITS_IN + 2)) + V_ADD_IN;


#define RGB_TO_YV12_ROW(SIZE, C1, C2, C3, C4)    \
  /* nothing */

#define RGB_TO_YV12(SIZE, C1, C2, C3, C4)      \
  uint32_t r, g, b, r0, g0, b0;        \
  r0 = g0 = b0 = 0;              \
  READ_RGB_Y(SIZE, 0, 0, C1,C2,C3,C4)      \
  READ_RGB_Y(SIZE, 1, 0, C1,C2,C3,C4)      \
  READ_RGB_UV(     0, 0)

#define RGBI_TO_YV12_ROW(SIZE, C1, C2, C3, C4)    \
  /* nothing */

#define RGBI_TO_YV12(SIZE, C1, C2, C3, C4)      \
  uint32_t r, g, b, r0, g0, b0, r1, g1, b1;  \
  r0 = g0 = b0 = r1 = g1 = b1 = 0;      \
  READ_RGB_Y(SIZE, 0, 0, C1,C2,C3,C4)      \
  READ_RGB_Y(SIZE, 1, 1, C1,C2,C3,C4)      \
  READ_RGB_Y(SIZE, 2, 0, C1,C2,C3,C4)      \
  READ_RGB_Y(SIZE, 3, 1, C1,C2,C3,C4)      \
  READ_RGB_UV(     0, 0)            \
  READ_RGB_UV(     1, 1)


/* yuyv/yuyvi input */

#define READ_YUYV_Y(ROW, C1, C2, C3, C4)            \
  y_ptr[(ROW)*y_stride+0] = x_ptr[(ROW)*x_stride+(C1)];  \
  y_ptr[(ROW)*y_stride+1] = x_ptr[(ROW)*x_stride+(C3)];

#define READ_YUYV_UV(UV_ROW, ROW1, ROW2, C1, C2, C3, C4)      \
  u_ptr[(UV_ROW)*uv_stride] = (x_ptr[(ROW1)*x_stride+(C2)] + x_ptr[(ROW2)*x_stride+(C2)] + 1) / 2;  \
  v_ptr[(UV_ROW)*uv_stride] = (x_ptr[(ROW1)*x_stride+(C4)] + x_ptr[(ROW2)*x_stride+(C4)] + 1) / 2;

#define YUYV_TO_YV12_ROW(SIZE, C1, C2, C3, C4)  \
  /* nothing */

#define YUYV_TO_YV12(SIZE, C1, C2, C3, C4)    \
  READ_YUYV_Y (0,      C1,C2,C3,C4)    \
  READ_YUYV_Y (1,      C1,C2,C3,C4)    \
  READ_YUYV_UV(0, 0,1, C1,C2,C3,C4)

#define YUYVI_TO_YV12_ROW(SIZE, C1, C2, C3, C4)  \
  /* nothing */

#define YUYVI_TO_YV12(SIZE, C1, C2, C3, C4)    \
  READ_YUYV_Y (0, C1,C2,C3,C4)      \
  READ_YUYV_Y (1, C1,C2,C3,C4)      \
  READ_YUYV_Y (2, C1,C2,C3,C4)      \
  READ_YUYV_Y (3, C1,C2,C3,C4)      \
  READ_YUYV_UV(0, 0,2, C1,C2,C3,C4)    \
  READ_YUYV_UV(1, 1,3, C1,C2,C3,C4)


MAKE_COLORSPACE(RGB555ToYUV420_c, 2, 2, 2, RGB16_TO_YV12, MK_RGB555, 0, 0, 0)

MAKE_COLORSPACE(RGB565ToYUV420_c, 2, 2, 2, RGB16_TO_YV12, MK_RGB565, 0, 0, 0)

MAKE_COLORSPACE(BGRToYUV420_c, 3, 2, 2, RGB_TO_YV12, 2, 1, 0, 0)

MAKE_COLORSPACE(BGRAToYUV420_c, 4, 2, 2, RGB_TO_YV12, 2, 1, 0, 0)

MAKE_COLORSPACE(ABGRToYUV420_c, 4, 2, 2, RGB_TO_YV12, 3, 2, 1, 0)

MAKE_COLORSPACE(RGBAToYUV420_c, 4, 2, 2, RGB_TO_YV12, 0, 1, 2, 0)

MAKE_COLORSPACE(ARGBToYUV420_c, 4, 2, 2, RGB_TO_YV12, 1, 2, 3, 0)

MAKE_COLORSPACE(YUYVToYUV420_c, 2, 2, 2, YUYV_TO_YV12, 0, 1, 2, 3)

MAKE_COLORSPACE(UYVYToYUV420_c, 2, 2, 2, YUYV_TO_YV12, 1, 0, 3, 2)

MAKE_COLORSPACE(RGB555IToYUV420_c, 2, 2, 4, RGB16I_TO_YV12, MK_RGB555, 0, 0, 0)

MAKE_COLORSPACE(RGB565IToYUV420_c, 2, 2, 4, RGB16I_TO_YV12, MK_RGB565, 0, 0, 0)

MAKE_COLORSPACE(BGRIToYUV420_c, 3, 2, 4, RGBI_TO_YV12, 2, 1, 0, 0)

MAKE_COLORSPACE(BGRAIToYUV420_c, 4, 2, 4, RGBI_TO_YV12, 2, 1, 0, 0)

MAKE_COLORSPACE(ABGRIToYUV420_c, 4, 2, 4, RGBI_TO_YV12, 3, 2, 1, 0)

MAKE_COLORSPACE(RGBAIToYUV420_c, 4, 2, 4, RGBI_TO_YV12, 0, 1, 2, 0)

MAKE_COLORSPACE(ARGBIToYUV420_c, 4, 2, 4, RGBI_TO_YV12, 1, 2, 3, 0)

MAKE_COLORSPACE(YUYVIToYUV420_c, 2, 2, 4, YUYVI_TO_YV12, 0, 1, 2, 3)

MAKE_COLORSPACE(UYVYIToYUV420_c, 2, 2, 4, YUYVI_TO_YV12, 1, 0, 3, 2)


/********** colorspace output (yv12_to_xxx) functions **********/

/* yuv -> rgb def's */

#ifdef COLOR_FULL_RANGE

#define RGB_Y_OUT        1.0
#define B_U_OUT          1.772
#define Y_ADD_OUT        0

#define G_U_OUT          0.34414
#define G_V_OUT          0.71414
#define U_ADD_OUT        128

#define R_V_OUT          1.402
#define V_ADD_OUT        128

#else

#define RGB_Y_OUT        1.164
#define B_U_OUT            2.018
#define Y_ADD_OUT        16

#define G_U_OUT            0.391
#define G_V_OUT            0.813
#define U_ADD_OUT        128

#define R_V_OUT            1.596
#define V_ADD_OUT        128

#endif

#define SCALEBITS_OUT  13
#define FIX_OUT(x)    ((uint16_t) ((x) * (1L<<SCALEBITS_OUT) + 0.5))


/* rgb16/rgb16i output */

#define MK_RGB555(R, G, B)          \
  ((C264_MAX(0,C264_MIN(255, R)) << 7) & 0x7c00) |  \
  ((C264_MAX(0,C264_MIN(255, G)) << 2) & 0x03e0) |  \
  ((C264_MAX(0,C264_MIN(255, B)) >> 3) & 0x001f)

#define MK_RGB565(R, G, B)          \
  ((C264_MAX(0,C264_MIN(255, R)) << 8) & 0xf800) |  \
  ((C264_MAX(0,C264_MIN(255, G)) << 3) & 0x07e0) |  \
  ((C264_MAX(0,C264_MIN(255, B)) >> 3) & 0x001f)

#define WRITE_RGB16(ROW, UV_ROW, C1)      \
  rgb_y = RGB_Y_tab[ y_ptr[y_stride*(ROW) + 0] ];              \
  b[ROW] = (b[ROW] & 0x7) + ((rgb_y + b_u##UV_ROW) >> SCALEBITS_OUT);    \
  g[ROW] = (g[ROW] & 0x7) + ((rgb_y - g_uv##UV_ROW) >> SCALEBITS_OUT);  \
  r[ROW] = (r[ROW] & 0x7) + ((rgb_y + r_v##UV_ROW) >> SCALEBITS_OUT);    \
  *(uint16_t *) (x_ptr+((ROW)*x_stride)+0) = C1(r[ROW], g[ROW], b[ROW]);  \
  rgb_y = RGB_Y_tab[ y_ptr[y_stride*(ROW) + 1] ];              \
  b[ROW] = (b[ROW] & 0x7) + ((rgb_y + b_u##UV_ROW) >> SCALEBITS_OUT);    \
  g[ROW] = (g[ROW] & 0x7) + ((rgb_y - g_uv##UV_ROW) >> SCALEBITS_OUT);  \
  r[ROW] = (r[ROW] & 0x7) + ((rgb_y + r_v##UV_ROW) >> SCALEBITS_OUT);    \
  *(uint16_t *) (x_ptr+((ROW)*x_stride)+2) = C1(r[ROW], g[ROW], b[ROW]);

#define YV12_TO_RGB16_ROW(SIZE, C1, C2, C3, C4) \
  int r[2], g[2], b[2];          \
  r[0] = r[1] = g[0] = g[1] = b[0] = b[1] = 0;

#define YV12_TO_RGB16(SIZE, C1, C2, C3, C4)            \
  int rgb_y;                        \
  int b_u0 = B_U_tab[ u_ptr[0] ];              \
  int g_uv0 = G_U_tab[ u_ptr[0] ] + G_V_tab[ v_ptr[0] ];  \
  int r_v0 = R_V_tab[ v_ptr[0] ];              \
  WRITE_RGB16(0, 0, C1)                  \
  WRITE_RGB16(1, 0, C1)

#define YV12_TO_RGB16I_ROW(SIZE, C1, C2, C3, C4)  \
  int r[4], g[4], b[4];            \
  r[0] = r[1] = r[2] = r[3] = 0;        \
  g[0] = g[1] = g[2] = g[3] = 0;        \
  b[0] = b[1] = b[2] = b[3] = 0;

#define YV12_TO_RGB16I(SIZE, C1, C2, C3, C4)            \
  int rgb_y;                          \
  int b_u0 = B_U_tab[ u_ptr[0] ];                \
  int g_uv0 = G_U_tab[ u_ptr[0] ] + G_V_tab[ v_ptr[0] ];    \
  int r_v0 = R_V_tab[ v_ptr[0] ];                \
    int b_u1 = B_U_tab[ u_ptr[uv_stride] ];            \
  int g_uv1 = G_U_tab[ u_ptr[uv_stride] ] + G_V_tab[ v_ptr[uv_stride] ];  \
  int r_v1 = R_V_tab[ v_ptr[uv_stride] ];            \
    WRITE_RGB16(0, 0, C1)                    \
  WRITE_RGB16(1, 1, C1)                    \
    WRITE_RGB16(2, 0, C1)                    \
  WRITE_RGB16(3, 1, C1)                    \


/* rgb/rgbi output */

#if 0
#define WRITE_RGB(SIZE,ROW,UV_ROW,C1,C2,C3,C4)				\
  rgb_y = RGB_Y_tab[ y_ptr[(ROW)*y_stride + 0] ];			\
  x_ptr[(ROW)*x_stride+(C3)] = C264_MAX(0, C264_MIN(255, (rgb_y + b_u##UV_ROW) >> SCALEBITS_OUT));	\
  x_ptr[(ROW)*x_stride+(C2)] = C264_MAX(0, C264_MIN(255, (rgb_y - g_uv##UV_ROW) >> SCALEBITS_OUT));	\
  x_ptr[(ROW)*x_stride+(C1)] = C264_MAX(0, C264_MIN(255, (rgb_y + r_v##UV_ROW) >> SCALEBITS_OUT));	\
  if ((SIZE)>3) x_ptr[(ROW)*x_stride+(C4)] = 0;									\
  rgb_y = RGB_Y_tab[ y_ptr[(ROW)*y_stride + 1] ];									\
  x_ptr[(ROW)*x_stride+(SIZE)+(C3)] = C264_MAX(0, C264_MIN(255, (rgb_y + b_u##UV_ROW) >> SCALEBITS_OUT));	\
  x_ptr[(ROW)*x_stride+(SIZE)+(C2)] = C264_MAX(0, C264_MIN(255, (rgb_y - g_uv##UV_ROW) >> SCALEBITS_OUT));	\
  x_ptr[(ROW)*x_stride+(SIZE)+(C1)] = C264_MAX(0, C264_MIN(255, (rgb_y + r_v##UV_ROW) >> SCALEBITS_OUT));	\
  if ((SIZE)>3) x_ptr[(ROW)*x_stride+(SIZE)+(C4)] = 0;
#else
#define WRITE_RGB(SIZE, ROW, UV_ROW, C1, C2, C3, C4)        \
  rgb_y = RGB_Y_tab[ y_ptr[(ROW)*y_stride + 0] ];      \
  x_ptr[(ROW)*x_stride+(C3)] = xx_clip_uint8((rgb_y + b_u##UV_ROW) >> SCALEBITS_OUT);  \
  x_ptr[(ROW)*x_stride+(C2)] = xx_clip_uint8((rgb_y - g_uv##UV_ROW) >> SCALEBITS_OUT);  \
  x_ptr[(ROW)*x_stride+(C1)] = xx_clip_uint8((rgb_y + r_v##UV_ROW) >> SCALEBITS_OUT);  \
  if ((SIZE)>3) x_ptr[(ROW)*x_stride+(C4)] = 0;                  \
  rgb_y = RGB_Y_tab[ y_ptr[(ROW)*y_stride + 1] ];                  \
  x_ptr[(ROW)*x_stride+(SIZE)+(C3)] = xx_clip_uint8((rgb_y + b_u##UV_ROW) >> SCALEBITS_OUT);  \
  x_ptr[(ROW)*x_stride+(SIZE)+(C2)] = xx_clip_uint8((rgb_y - g_uv##UV_ROW) >> SCALEBITS_OUT);  \
  x_ptr[(ROW)*x_stride+(SIZE)+(C1)] = xx_clip_uint8((rgb_y + r_v##UV_ROW) >> SCALEBITS_OUT);  \
  if ((SIZE)>3) x_ptr[(ROW)*x_stride+(SIZE)+(C4)] = 0;
#endif

#define YV12_TO_RGB_ROW(SIZE, C1, C2, C3, C4)  /* nothing */

#define YV12_TO_RGB(SIZE, C1, C2, C3, C4)            \
  int rgb_y;                        \
  int b_u0 = B_U_tab[ u_ptr[0] ];              \
  int g_uv0 = G_U_tab[ u_ptr[0] ] + G_V_tab[ v_ptr[0] ];  \
  int r_v0 = R_V_tab[ v_ptr[0] ];              \
  WRITE_RGB(SIZE, 0, 0, C1,C2,C3,C4)            \
  WRITE_RGB(SIZE, 1, 0, C1,C2,C3,C4)

#define YV12_TO_RGBI_ROW(SIZE, C1, C2, C3, C4)  /* nothing */

#define YV12_TO_RGBI(SIZE, C1, C2, C3, C4)            \
  int rgb_y;                        \
  int b_u0 = B_U_tab[ u_ptr[0] ];              \
  int g_uv0 = G_U_tab[ u_ptr[0] ] + G_V_tab[ v_ptr[0] ];  \
  int r_v0 = R_V_tab[ v_ptr[0] ];              \
    int b_u1 = B_U_tab[ u_ptr[uv_stride] ];          \
  int g_uv1 = G_U_tab[ u_ptr[uv_stride] ] + G_V_tab[ v_ptr[uv_stride] ];  \
  int r_v1 = R_V_tab[ v_ptr[uv_stride] ];          \
  WRITE_RGB(SIZE, 0, 0, C1,C2,C3,C4)            \
  WRITE_RGB(SIZE, 1, 1, C1,C2,C3,C4)            \
  WRITE_RGB(SIZE, 2, 0, C1,C2,C3,C4)            \
  WRITE_RGB(SIZE, 3, 1, C1,C2,C3,C4)


/* yuyv/yuyvi output */

#define WRITE_YUYV(ROW, UV_ROW, C1, C2, C3, C4)            \
  x_ptr[(ROW)*x_stride+(C1)] = y_ptr[   (ROW)*y_stride +0];  \
  x_ptr[(ROW)*x_stride+(C2)] = u_ptr[(UV_ROW)*uv_stride+0];  \
  x_ptr[(ROW)*x_stride+(C3)] = y_ptr[   (ROW)*y_stride +1];  \
  x_ptr[(ROW)*x_stride+(C4)] = v_ptr[(UV_ROW)*uv_stride+0];  \

#define YV12_TO_YUYV_ROW(SIZE, C1, C2, C3, C4)  /* nothing */

#define YV12_TO_YUYV(SIZE, C1, C2, C3, C4)  \
  WRITE_YUYV(0, 0, C1,C2,C3,C4)    \
  WRITE_YUYV(1, 0, C1,C2,C3,C4)

#define YV12_TO_YUYVI_ROW(SIZE, C1, C2, C3, C4) /* nothing */

#define YV12_TO_YUYVI(SIZE, C1, C2, C3, C4)  \
  WRITE_YUYV(0, 0, C1,C2,C3,C4)    \
  WRITE_YUYV(1, 1, C1,C2,C3,C4)    \
  WRITE_YUYV(2, 0, C1,C2,C3,C4)    \
  WRITE_YUYV(3, 1, C1,C2,C3,C4)


MAKE_COLORSPACE(YUV420ToRGB555_c, 2, 2, 2, YV12_TO_RGB16, MK_RGB555, 0, 0, 0)

MAKE_COLORSPACE(YUV420ToRGB565_c, 2, 2, 2, YV12_TO_RGB16, MK_RGB565, 0, 0, 0)

MAKE_COLORSPACE(YUV420ToBGR_c, 3, 2, 2, YV12_TO_RGB, 2, 1, 0, 0)

MAKE_COLORSPACE(YUV420ToRGB_c, 3, 2, 2, YV12_TO_RGB, 0, 1, 2, 0)

MAKE_COLORSPACE(YUV420ToBGRA_c, 4, 2, 2, YV12_TO_RGB, 2, 1, 0, 3)

MAKE_COLORSPACE(YUV420ToABGR_c, 4, 2, 2, YV12_TO_RGB, 3, 2, 1, 0)

MAKE_COLORSPACE(YUV420ToRGBA_c, 4, 2, 2, YV12_TO_RGB, 0, 1, 2, 3)

MAKE_COLORSPACE(YUV420ToARGB_c, 4, 2, 2, YV12_TO_RGB, 1, 2, 3, 0)

MAKE_COLORSPACE(YUV420ToYUYV_c, 2, 2, 2, YV12_TO_YUYV, 0, 1, 2, 3)

MAKE_COLORSPACE(YUV420ToUYVY_c, 2, 2, 2, YV12_TO_YUYV, 1, 0, 3, 2)

MAKE_COLORSPACE(YUV420ToRGB555I_c, 2, 2, 4, YV12_TO_RGB16I, MK_RGB555, 0, 0, 0)

MAKE_COLORSPACE(YUV420ToRGB565I_c, 2, 2, 4, YV12_TO_RGB16I, MK_RGB565, 0, 0, 0)

MAKE_COLORSPACE(YUV420ToBGRI_c, 3, 2, 4, YV12_TO_RGBI, 2, 1, 0, 0)

MAKE_COLORSPACE(YUV420ToBGRAI_c, 4, 2, 4, YV12_TO_RGBI, 2, 1, 0, 3)

MAKE_COLORSPACE(YUV420ToABGRI_c, 4, 2, 4, YV12_TO_RGBI, 3, 2, 1, 0)

MAKE_COLORSPACE(YUV420ToRGBAI_c, 4, 2, 4, YV12_TO_RGBI, 0, 1, 2, 3)

MAKE_COLORSPACE(YUV420ToARGBI_c, 4, 2, 4, YV12_TO_RGBI, 1, 2, 3, 0)

MAKE_COLORSPACE(YUV420ToYUYVI_c, 2, 2, 4, YV12_TO_YUYVI, 0, 1, 2, 3)

MAKE_COLORSPACE(YUV420ToUYVYI_c, 2, 2, 4, YV12_TO_YUYVI, 1, 0, 3, 2)

void colorspace_init(void) {
  int32_t i;
  static int n = 0;
  if (n != 0) {
    return;
  }
  n++;

  for (i = 0; i < 256; i++) {
    RGB_Y_tab[i] = FIX_OUT(RGB_Y_OUT) * (i - Y_ADD_OUT);
    B_U_tab[i] = FIX_OUT(B_U_OUT) * (i - U_ADD_OUT);
    G_U_tab[i] = FIX_OUT(G_U_OUT) * (i - U_ADD_OUT);
    G_V_tab[i] = FIX_OUT(G_V_OUT) * (i - V_ADD_OUT);
    R_V_tab[i] = FIX_OUT(R_V_OUT) * (i - V_ADD_OUT);
  }
}


// xx_csp_convert_pf xx_i420_to_i420 = NULL;
// xx_csp_convert_pf xx_nv21_to_i420 = NULL;
// xx_csp_convert_pf xx_nv12_to_i420 = NULL;
// xx_csp_convert_pf xx_yuyv_to_i420 = NULL;
// xx_csp_convert_pf xx_i420_to_yuyv = NULL;
// xx_csp_convert_pf xx_i420_to_nv12 = NULL;
// xx_csp_convert_pf xx_i420_to_rgb565 = NULL;


/*
void
YUV420ToYUV420_c
(
	uint8_t * y_dst,
	uint8_t * u_dst,
	uint8_t * v_dst,
	int y_dst_stride,
	int uv_dst_stride,
	uint8_t * y_src,
	uint8_t * u_src,
	uint8_t * v_src,
	int y_src_stride,
	int uv_src_stride,
	int width,
	int height,
	int vflip
)
{
	int width2 = width / 2;
	int height2 = height / 2;
	int y;

	if ( !vflip
		&& y_dst_stride == width && y_src_stride == width
		&& uv_dst_stride == width2 && uv_src_stride == width2 )
	{
		memcpy(y_dst, y_src, width*height);
		memcpy(u_dst, u_src, width2*height2);
		memcpy(v_dst, v_src, width2*height2);
		return;
	}

	if ( vflip )
	{
		y_src += (height - 1) * y_src_stride;
		u_src += (height2 - 1) * uv_src_stride;
		v_src += (height2 - 1) * uv_src_stride;
		y_src_stride = -y_src_stride;
		uv_src_stride = -uv_src_stride;
	}

	for (y = height; y > 0; y--)
	{
		memcpy( y_dst, y_src, width );
		y_src += y_src_stride;
		y_dst += y_dst_stride;
	}

	for (y = height2; y > 0; y--)
	{
		memcpy( u_dst, u_src, width2 );
		u_src += uv_src_stride;
		u_dst += uv_dst_stride;
	}

	for (y = height2; y > 0; y--)
	{
		memcpy( v_dst, v_src, width2 );
		v_src += uv_src_stride;
		v_dst += uv_dst_stride;
	}
}
//*/

void xx_i420_to_i420_c(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[], int width, int height,
                       int vflip) {
  int j, k;
/*
	memcpy(dst[0], src[0], width*height);
	memcpy(dst[1], src[1], width*height/4);
	memcpy(dst[2], src[2], width*height/4);
	return;
//*/
/*
	YUV420ToYUV420_c(dst[0], dst[1], dst[2], dst_stride[0], dst_stride[1],
		src[0], src[1], src[2], src_stride[0], src_stride[1], width, height, vflip);
	return;
//*/
  for (k = 0; k < 3; k++) {
    int w = (k == 0) ? width : (width >> 1);
    int h = (k == 0) ? height : (height >> 1);

    uint8_t* p_dst = dst[k];
    uint8_t* p_src = src[k];
    int d_stride = dst_stride[k];
    int s_stride = src_stride[k];

    if (vflip) {
      p_src = src[k] + (h - 1) * src_stride[k];
      s_stride = -src_stride[k];
    }

    for (j = 0; j < h; j++) {
      memcpy(p_dst, p_src, w);
      p_dst += d_stride;
      p_src += s_stride;
    }
  }
}

void xx_yuyv_to_i420_c(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[], int width, int height,
                       int vflip) {
  int i, j;

  uint8_t* p_y = dst[0];
  uint8_t* p_u = dst[1];
  uint8_t* p_v = dst[2];
  int y_stride = dst_stride[0];
  int u_stride = dst_stride[1];
  int v_stride = dst_stride[2];

  uint8_t* p_s = src[0];
  int s_stride = src_stride[0];

  if (vflip) {
    p_s = src[0] + src_stride[0] * (height - 1);
    s_stride = -src_stride[0];
  }

  for (j = 0; j < height; j += 2) {
    for (i = 0; i < width; i += 2) {
      p_y[i + 0] = p_s[(i << 1) + 0];
      p_y[i + 1] = p_s[(i << 1) + 2];
      p_y[y_stride + i + 0] = p_s[s_stride + (i << 1) + 0];
      p_y[y_stride + i + 1] = p_s[s_stride + (i << 1) + 2];

      p_u[i >> 1] = p_s[(i << 1) + 1];
      p_v[i >> 1] = p_s[(i << 1) + 3];
    }
    p_y += y_stride << 1;
    p_u += u_stride;
    p_v += v_stride;
    p_s += s_stride << 1;
  }
}

void xx_i420_to_yuyv_c(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[], int width, int height,
                       int vflip) {
  int i, j;

  uint8_t* p_y = src[0];
  uint8_t* p_u = src[1];
  uint8_t* p_v = src[2];
  int y_stride = src_stride[0];
  int u_stride = src_stride[1];
  int v_stride = src_stride[2];

  uint8_t* p_d = dst[0];
  int d_stride = dst_stride[0];

  if (vflip) {
    p_d = dst[0] + dst_stride[0] * (height - 1);
    d_stride = -dst_stride[0];
  }

  for (j = 0; j < height; j += 2) {
    for (i = 0; i < width; i += 2) {
      p_d[(i << 1) + 0] = p_y[i + 0];
      p_d[(i << 1) + 2] = p_y[i + 1];
      p_d[d_stride + (i << 1) + 0] = p_y[y_stride + i + 0];
      p_d[d_stride + (i << 1) + 2] = p_y[y_stride + i + 1];

      p_d[(i << 1) + 1] = p_u[i >> 1];
      p_d[(i << 1) + 3] = p_v[i >> 1];
      p_d[d_stride + (i << 1) + 1] = p_u[i >> 1];
      p_d[d_stride + (i << 1) + 3] = p_v[i >> 1];
    }
    p_y += y_stride << 1;
    p_u += u_stride;
    p_v += v_stride;
    p_d += d_stride << 1;
  }
}

#ifdef WIN32
static void xx_nv12_to_i420_c(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[], int width, int height, int vflip) {
#else
static void xx_nv12_to_i420_c(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[] __attribute__((unused)), int width, int height, int vflip) {
#endif
  int i, j, k;
  uint8_t* p_dst[3];
  int d_stride[3];
  uint8_t* p_src = src[0];

  for (k = 0; k < 3; k++) {
    int h = (k == 0) ? height : (height >> 1);

    if (vflip) {
      p_dst[k] = dst[k] + (h - 1) * dst_stride[k];
      d_stride[k] = -dst_stride[k];
    } else {
      p_dst[k] = dst[k];
      d_stride[k] = dst_stride[k];
    }
  }

  for (j = 0; j < height; j++) {
    memcpy(p_dst[0], p_src, width);
    p_dst[0] += d_stride[0];
    p_src += width;
  }

  {
    uint8_t* u_dst = p_dst[1];
    uint8_t* v_dst = p_dst[2];

    p_src = src[0] + width * height;

    for (j = 0; j < height / 2; j++) {
      for (i = 0; i < width / 2; i++) {
        u_dst[i] = p_src[(i << 1) + 0];
        v_dst[i] = p_src[(i << 1) + 1];
      }
      u_dst += d_stride[1];
      v_dst += d_stride[2];
      p_src += width;
    }
  }
}

void xx_nv21_to_i420_c(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[], int width, int height,
                       int vflip) {
  uint8_t* tdst[4] = {dst[0], dst[2], dst[1], NULL};
  int tstride[4] = {dst_stride[0], dst_stride[2], dst_stride[1], 0};

  if (xx_nv12_to_i420 != NULL) {
    xx_nv12_to_i420(tdst, tstride, src, src_stride, width, height, vflip);
  } else {
    xx_nv12_to_i420_c(tdst, tstride, src, src_stride, width, height, vflip);
  }
}

static void xx_i420_to_nv12_c(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[], int width, int height,
                              int vflip) {
  int i, j, k;
  uint8_t* p_src[3];
  int s_stride[3];
  uint8_t* p_dst = NULL;

  for (k = 0; k < 3; k++) {
    int h = (k == 0) ? height : (height >> 1);

    if (vflip) {
      p_src[k] = src[k] + (h - 1) * src_stride[k];
      s_stride[k] = -src_stride[k];
    } else {
      p_src[k] = src[k];
      s_stride[k] = src_stride[k];
    }
  }

  p_dst = dst[0];
  for (j = 0; j < height; j++) {
    memcpy(dst[0], p_src[0], width);
    p_src[0] += s_stride[0];
    p_dst += dst_stride[0];
  }

  {
    uint8_t* u_src = p_src[1];
    uint8_t* v_src = p_src[2];

    p_dst = dst[1];

    for (j = 0; j < height / 2; j++) {
      for (i = 0; i < width / 2; i++) {
        p_dst[(i << 1) + 0] = u_src[i];
        p_dst[(i << 1) + 1] = v_src[i];
      }
      u_src += s_stride[1];
      v_src += s_stride[2];
      p_dst += dst_stride[1];
    }
  }
}

#ifndef XX_CLIP_UINT8
#define XX_CLIP_UINT8(a) (((a)&(~255)) ? (unsigned char)((-(a))>>31) : (unsigned char)(a))
#endif

#define YUV_TO_RGB_COEFF_Y    ((int)(1.164*256 + 0.5))   // 298
#define YUV_TO_RGB_COEFF_BU    ((int)(2.018*256 + 0.5))   // 517
#define YUV_TO_RGB_COEFF_GU    ((int)(0.391*256 + 0.5))   // 100
#define YUV_TO_RGB_COEFF_GV    ((int)(0.813*256 + 0.5))   // 208
#define YUV_TO_RGB_COEFF_RV    ((int)(1.596*256 + 0.5))   // 409

#ifdef WIN32
void xx_i420_to_rgb565_c(unsigned char* dst[], int dst_stride[], unsigned char* src[], int src_stride[], int width,
                         int height, int vflip) {
#else
void xx_i420_to_rgb565_c(unsigned char* dst[], int dst_stride[], unsigned char* src[], int src_stride[], int width,
                         int height, int vflip __attribute__((unused))) {
#endif
  int Y;
  int i, j;
  int R, G, B, Cr, Cb;
  unsigned char* py = src[0];
  unsigned char* pu = src[1];
  unsigned char* pv = src[2];
  unsigned short* out0 = (unsigned short*)dst[0];

  assert(vflip == 0);

  for (j = 0; j < height; j += 2) {
    unsigned short* out1 = out0 + dst_stride[0] / 2;   // next line
    unsigned char* py0 = py;
    unsigned char* py1 = py + src_stride[0];

    for (i = 0; i < width; i += 2) {
      int rv;
      int guv;
      int bu;
#define RESETRGB() //{B = 0; R = 0; G = 255;}
      Cb = pu[i >> 1] - 128;
      Cr = pv[i >> 1] - 128;
      rv = (YUV_TO_RGB_COEFF_RV * Cr) >> 8;
      guv = (YUV_TO_RGB_COEFF_GU * Cb + YUV_TO_RGB_COEFF_GV * Cr) >> 8;
      bu = (YUV_TO_RGB_COEFF_BU * Cb) >> 8;

      Y = (YUV_TO_RGB_COEFF_Y * (py0[i] - 16)) >> 8;
      R = Y + rv;
      G = Y - guv;
      B = Y + bu;
      RESETRGB()
      R = XX_CLIP_UINT8(R);
      G = XX_CLIP_UINT8(G);
      B = XX_CLIP_UINT8(B);
      out0[i] = ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);

      Y = (YUV_TO_RGB_COEFF_Y * (py0[i + 1] - 16)) >> 8;
      R = Y + rv;
      G = Y - guv;
      B = Y + bu;
      RESETRGB()
      R = XX_CLIP_UINT8(R);
      G = XX_CLIP_UINT8(G);
      B = XX_CLIP_UINT8(B);
      out0[i + 1] = ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);

      Y = (YUV_TO_RGB_COEFF_Y * (py1[i] - 16)) >> 8;
      R = Y + rv;
      G = Y - guv;
      B = Y + bu;
      RESETRGB()
      R = XX_CLIP_UINT8(R);
      G = XX_CLIP_UINT8(G);
      B = XX_CLIP_UINT8(B);
      out1[i] = ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);

      Y = (YUV_TO_RGB_COEFF_Y * (py1[i + 1] - 16)) >> 8;
      R = Y + rv;
      G = Y - guv;
      B = Y + bu;
      RESETRGB()
      R = XX_CLIP_UINT8(R);
      G = XX_CLIP_UINT8(G);
      B = XX_CLIP_UINT8(B);
      out1[i + 1] = ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);
    }
    py += src_stride[0] * 2;
    pu += src_stride[1];
    pv += src_stride[2];
    out0 += dst_stride[0]; // two line
  }
}

/* input */
ColorConvertFuncPtr RGB555ToYUV420 = RGB555ToYUV420_c;
ColorConvertFuncPtr RGB565ToYUV420 = RGB565ToYUV420_c;
ColorConvertFuncPtr BGRToYUV420 = BGRToYUV420_c;
ColorConvertFuncPtr BGRAToYUV420 = BGRAToYUV420_c;
ColorConvertFuncPtr ABGRToYUV420 = ABGRToYUV420_c;
ColorConvertFuncPtr RGBAToYUV420 = RGBAToYUV420_c;
ColorConvertFuncPtr ARGBToYUV420 = ARGBToYUV420_c;
ColorConvertFuncPtr YUYVToYUV420 = YUYVToYUV420_c;
ColorConvertFuncPtr UYVYToYUV420 = UYVYToYUV420_c;

ColorConvertFuncPtr RGB555IToYUV420 = RGB555IToYUV420_c;
ColorConvertFuncPtr RGB565IToYUV420 = RGB565IToYUV420_c;
ColorConvertFuncPtr BGRIToYUV420 = BGRIToYUV420_c;
ColorConvertFuncPtr BGRAIToYUV420 = BGRAIToYUV420_c;
ColorConvertFuncPtr ABGRIToYUV420 = ABGRIToYUV420_c;
ColorConvertFuncPtr RGBAIToYUV420 = RGBAIToYUV420_c;
ColorConvertFuncPtr ARGBIToYUV420 = ARGBIToYUV420_c;
ColorConvertFuncPtr YUYVIToYUV420 = YUYVIToYUV420_c;
ColorConvertFuncPtr UYVYIToYUV420 = UYVYIToYUV420_c;

/* output */
ColorConvertFuncPtr YUV420ToRGB555 = YUV420ToRGB555_c;
ColorConvertFuncPtr YUV420ToRGB565 = YUV420ToRGB565_c;
ColorConvertFuncPtr YUV420ToBGR = YUV420ToBGR_c;
ColorConvertFuncPtr YUV420ToRGB = YUV420ToRGB_c;
ColorConvertFuncPtr YUV420ToBGRA = YUV420ToBGRA_c;
ColorConvertFuncPtr YUV420ToABGR = YUV420ToABGR_c;
ColorConvertFuncPtr YUV420ToRGBA = YUV420ToRGBA_c;
ColorConvertFuncPtr YUV420ToARGB = YUV420ToARGB_c;
ColorConvertFuncPtr YUV420ToYUYV = YUV420ToYUYV_c;
ColorConvertFuncPtr YUV420ToUYVY = YUV420ToUYVY_c;

ColorConvertFuncPtr YUV420ToRGB555I = YUV420ToRGB555I_c;
ColorConvertFuncPtr YUV420ToRGB565I = YUV420ToRGB565I_c;
ColorConvertFuncPtr YUV420ToBGRI = YUV420ToBGRI_c;
ColorConvertFuncPtr YUV420ToBGRAI = YUV420ToBGRAI_c;
ColorConvertFuncPtr YUV420ToABGRI = YUV420ToABGRI_c;
ColorConvertFuncPtr YUV420ToRGBAI = YUV420ToRGBAI_c;
ColorConvertFuncPtr YUV420ToARGBI = YUV420ToARGBI_c;
ColorConvertFuncPtr YUV420ToYUYVI = YUV420ToYUYVI_c;
ColorConvertFuncPtr YUV420ToUYVYI = YUV420ToUYVYI_c;

xx_csp_convert_pf xx_i420_to_i420 = xx_i420_to_i420_c;
xx_csp_convert_pf xx_nv21_to_i420 = xx_nv21_to_i420_c;
xx_csp_convert_pf xx_nv12_to_i420 = xx_nv12_to_i420_c;
xx_csp_convert_pf xx_yuyv_to_i420 = xx_yuyv_to_i420_c;
xx_csp_convert_pf xx_i420_to_yuyv = xx_i420_to_yuyv_c;
xx_csp_convert_pf xx_i420_to_nv12 = xx_i420_to_nv12_c;
xx_csp_convert_pf xx_i420_to_rgb565 = xx_i420_to_rgb565_c;

#ifdef WIN32
void c264_csp_init(uint32_t cpu) {
#else
void c264_csp_init(uint32_t cpu __attribute__((unused))) {
#endif
  RGB555ToYUV420 = RGB555ToYUV420_c;
  RGB565ToYUV420 = RGB565ToYUV420_c;
  BGRToYUV420 = BGRToYUV420_c;
  BGRAToYUV420 = BGRAToYUV420_c;
  ABGRToYUV420 = ABGRToYUV420_c;
  RGBAToYUV420 = RGBAToYUV420_c;
  ARGBToYUV420 = ARGBToYUV420_c;
  YUYVToYUV420 = YUYVToYUV420_c;
  UYVYToYUV420 = UYVYToYUV420_c;

  RGB555IToYUV420 = RGB555IToYUV420_c;
  RGB565IToYUV420 = RGB565IToYUV420_c;
  BGRIToYUV420 = BGRIToYUV420_c;
  BGRAIToYUV420 = BGRAIToYUV420_c;
  ABGRIToYUV420 = ABGRIToYUV420_c;
  RGBAIToYUV420 = RGBAIToYUV420_c;
  ARGBIToYUV420 = ARGBIToYUV420_c;
  YUYVIToYUV420 = YUYVIToYUV420_c;
  UYVYIToYUV420 = UYVYIToYUV420_c;

  // YV12->User Format

  YUV420ToRGB555 = YUV420ToRGB555_c;
  YUV420ToRGB565 = YUV420ToRGB565_c;
  YUV420ToBGR = YUV420ToBGR_c;
  YUV420ToRGB = YUV420ToRGB_c;
  YUV420ToBGRA = YUV420ToBGRA_c;
  YUV420ToABGR = YUV420ToABGR_c;
  YUV420ToRGBA = YUV420ToRGBA_c;
  YUV420ToARGB = YUV420ToARGB_c;
  YUV420ToYUYV = YUV420ToYUYV_c;
  YUV420ToUYVY = YUV420ToUYVY_c;

  YUV420ToRGB555I = YUV420ToRGB555I_c;
  YUV420ToRGB565I = YUV420ToRGB565I_c;
  YUV420ToBGRI = YUV420ToBGRI_c;
  YUV420ToBGRAI = YUV420ToBGRAI_c;
  YUV420ToABGRI = YUV420ToABGRI_c;
  YUV420ToRGBAI = YUV420ToRGBAI_c;
  YUV420ToARGBI = YUV420ToARGBI_c;
  YUV420ToYUYVI = YUV420ToYUYVI_c;
  YUV420ToUYVYI = YUV420ToUYVYI_c;

  xx_i420_to_i420 = xx_i420_to_i420_c;
  xx_nv21_to_i420 = xx_nv21_to_i420_c;
  xx_nv12_to_i420 = xx_nv12_to_i420_c;
  xx_yuyv_to_i420 = xx_yuyv_to_i420_c;
  xx_i420_to_yuyv = xx_i420_to_yuyv_c;
  xx_i420_to_nv12 = xx_i420_to_nv12_c;
  xx_i420_to_rgb565 = xx_i420_to_rgb565_c;

#if USE_MMX
  if (cpu & XX_CPU_MMX)
  {
    //xx_i420_to_i420 = xx_i420_to_i420_x86;
    //xx_i420_to_i420 = xx_i420_to_i420_mmx;

    xx_yuyv_to_i420 = xx_yuyv_to_i420_mmx;
    xx_i420_to_yuyv = xx_i420_to_yuyv_mmx;
  }

  if (cpu & XX_CPU_SSE2)
  {
    xx_yuyv_to_i420 = xx_yuyv_to_i420_sse2;
    xx_i420_to_yuyv = xx_i420_to_yuyv_sse2;
  }
#endif

#if USE_ARM_ASM
  if (cpu & XX_CPU_ARMV6)
  {
  }
  if (cpu & XX_CPU_NEON)
  {
    xx_nv12_to_i420 = xx_nv12_to_i420_neon;
    xx_i420_to_rgb565 = xx_i420_to_rgb565_neon;
  }
#endif
}
