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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define C264_MIN(a, b) ((a) < (b) ? (a) : (b))
#define C264_MAX(a, b) ((a) > (b) ? (a) : (b))

static inline uint8_t xx_clip_uint8(int a) {
  return (a & (~255)) ? (uint8_t)((-a) >> 31) : (uint8_t)a;
}
//#define xx_clip_uint8(a) (((a)&(~255)) ? (uint8_t)((-(a))>>31) : (uint8_t)(a))

static inline int xx_clip3(int v, int min, int max) {
  return ((v < min) ? min : (v > max) ? max : v);
}

/**
 *  ColorConvertFunc
 *
 *	This function converts the RGB image x_ptr to the YUV image y_src, u_src,
 *	v_src according to the following formulas:
 *	Y = +0.299*R + 0.587*G + 0.114*B
 *	U = -0.147*R - 0.289*G + 0.436*B = 0.492*(B - Y)
 *	V = +0.615*R - 0.515*G - 0.100*B = 0.877*(R - Y)
 *	For digital RGB values in the range [0..255], Y has the range [0..255],
 *	U varies in the range [-112..+112], and V in the range [-157..+157].
 *	To fit in the range of [0..255], a constant value 128 is added to computed
 *	U and V values, and V is then saturated.
 *
 *	@param	x_ptr Pointer to the [src / dst] buffer for RGB data.
 *  @param	x_stride Step in bytes through the RGB image buffer.
 *  @param	y_src Pointer to the [src / dst] buffer for y data.
 *  @param	v_src Pointer to the [src / dst] buffer for u data.
 *  @param	u_src Pointer to the [src / dst] buffer for v data.
 *	@param	y_stride Step in bytes through the y image buffer.
 *  @param	uv_stride Step in bytes through the uv image buffer.
 *  @param	width edged image width
 *  @param	height edged image height
 *  @param	vflip a bool value indicating if flip the image
 *
 *  @return	null
 *  @remarks
 */

typedef void(ColorConvertFunc)(uint8_t* x_ptr, int x_stride, uint8_t* y_src, uint8_t* v_src,
                               uint8_t* u_src, int y_stride, int uv_stride, int width, int height,
                               int vflip);

typedef ColorConvertFunc* ColorConvertFuncPtr;

/* initialize tables */

void colorspace_init(void);

extern ColorConvertFuncPtr RGB555ToYUV420;
extern ColorConvertFuncPtr RGB565ToYUV420;
extern ColorConvertFuncPtr BGRToYUV420;
extern ColorConvertFuncPtr BGRAToYUV420;
extern ColorConvertFuncPtr ABGRToYUV420;
extern ColorConvertFuncPtr RGBAToYUV420;
extern ColorConvertFuncPtr ARGBToYUV420;
extern ColorConvertFuncPtr YUYVToYUV420;
extern ColorConvertFuncPtr UYVYToYUV420;

extern ColorConvertFuncPtr RGB555IToYUV420;
extern ColorConvertFuncPtr RGB565IToYUV420;
extern ColorConvertFuncPtr BGRIToYUV420;
extern ColorConvertFuncPtr BGRAIToYUV420;
extern ColorConvertFuncPtr ABGRIToYUV420;
extern ColorConvertFuncPtr RGBAIToYUV420;
extern ColorConvertFuncPtr ARGBIToYUV420;
extern ColorConvertFuncPtr YUYVIToYUV420;
extern ColorConvertFuncPtr UYVYIToYUV420;

extern ColorConvertFuncPtr YUYVToBGRA;

/* plain c */
ColorConvertFunc RGB555ToYUV420_c;
ColorConvertFunc RGB565ToYUV420_c;
ColorConvertFunc BGRToYUV420_c;
ColorConvertFunc BGRAToYUV420_c;
ColorConvertFunc ABGRToYUV420_c;
ColorConvertFunc RGBAToYUV420_c;
ColorConvertFunc ARGBToYUV420_c;
ColorConvertFunc YUYVToYUV420_c;
ColorConvertFunc UYVYToYUV420_c;

ColorConvertFunc RGB555IToYUV420_c;
ColorConvertFunc RGB565IToYUV420_c;
ColorConvertFunc BGRIToYUV420_c;
ColorConvertFunc BGRAIToYUV420_c;
ColorConvertFunc ABGRIToYUV420_c;
ColorConvertFunc RGBAIToYUV420_c;
ColorConvertFunc ARGBIToYUV420_c;
ColorConvertFunc YUYVIToYUV420_c;
ColorConvertFunc UYVYIToYUV420_c;
ColorConvertFunc YUYVToBGRA_c;
ColorConvertFunc YUV420ToRGBAI_c;

ColorConvertFunc BGRToYUV420_mmx;
ColorConvertFunc BGRAToYUV420_mmx;
ColorConvertFunc YUYVToYUV420_mmx;
ColorConvertFunc UYVYToYUV420_mmx;
ColorConvertFunc YUYVToBGRA_mmx;

ColorConvertFunc YUYVToYUV420_3dn;
ColorConvertFunc UYVYToYUV420_3dn;

ColorConvertFunc YUYVToYUV420_xmm;
ColorConvertFunc UYVYToYUV420_xmm;

ColorConvertFunc YUYVToYUV420_sse2;

/* yv12_to_xxx colorspace conversion functions (decoder) */

extern ColorConvertFuncPtr YUV420ToRGB555;
extern ColorConvertFuncPtr YUV420ToRGB565;
extern ColorConvertFuncPtr YUV420ToBGR;
extern ColorConvertFuncPtr YUV420ToRGB;
extern ColorConvertFuncPtr YUV420ToBGRA;
extern ColorConvertFuncPtr YUV420ToABGR;
extern ColorConvertFuncPtr YUV420ToRGBA;
extern ColorConvertFuncPtr YUV420ToARGB;
extern ColorConvertFuncPtr YUV420ToYUYV;
extern ColorConvertFuncPtr YUV420ToUYVY;

extern ColorConvertFuncPtr YUV420ToRGB555I;
extern ColorConvertFuncPtr YUV420ToRGB565I;
extern ColorConvertFuncPtr YUV420ToBGRI;
extern ColorConvertFuncPtr YUV420ToBGRAI;
extern ColorConvertFuncPtr YUV420ToABGRI;
extern ColorConvertFuncPtr YUV420ToRGBAI;
extern ColorConvertFuncPtr YUV420ToARGBI;
extern ColorConvertFuncPtr YUV420ToYUYVI;
extern ColorConvertFuncPtr YUV420ToUYVYI;

/* plain c */
ColorConvertFunc YUV420ToRGB555_c;
ColorConvertFunc YUV420ToRGB565_c;
ColorConvertFunc YUV420ToBGR_c;
ColorConvertFunc YUV420ToRGB_c;
ColorConvertFunc YUV420ToBGRA_c;
ColorConvertFunc YUV420ToABGR_c;
ColorConvertFunc YUV420ToRGBA_c;
ColorConvertFunc YUV420ToARGB_c;
ColorConvertFunc YUV420ToYUYV_c;
ColorConvertFunc YUV420ToUYVY_c;

ColorConvertFunc YUV420ToRGB555I_c;
ColorConvertFunc YUV420ToRGB565I_c;
ColorConvertFunc YUV420ToBGRI_c;
ColorConvertFunc YUV420ToBGRAI_c;
ColorConvertFunc YUV420ToABGRI_c;
ColorConvertFunc YUV420ToGRBAI_c;
ColorConvertFunc YUV420ToARGBI_c;
ColorConvertFunc YUV420ToYUYVI_c;
ColorConvertFunc YUV420ToUYVYI_c;

ColorConvertFunc YUV420ToBGR_mmx;
ColorConvertFunc YUV420ToBGRA_mmx;
ColorConvertFunc YUV420ToYUYV_mmx;
ColorConvertFunc YUV420ToUYVY_mmx;

ColorConvertFunc YUV420ToYUYVI_mmx;
ColorConvertFunc YUV420ToUYVYI_mmx;

typedef void(xx_csp_convert_f)(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[],
                               int32_t width, int32_t height, int vflip);

typedef xx_csp_convert_f* xx_csp_convert_pf;

extern xx_csp_convert_pf xx_i420_to_i420;
extern xx_csp_convert_pf xx_nv21_to_i420;
extern xx_csp_convert_pf xx_nv12_to_i420;
extern xx_csp_convert_pf xx_yuyv_to_i420;
extern xx_csp_convert_pf xx_i420_to_nv12;
extern xx_csp_convert_pf xx_i420_to_rgb565;

void xx_nv21_to_i420_c(uint8_t* dst[], int dst_stride[], uint8_t* src[], int src_stride[],
                       int width, int height, int vflip);

xx_csp_convert_f xx_i420_to_i420_c;
xx_csp_convert_f xx_i420_to_i420_x86;
xx_csp_convert_f xx_i420_to_i420_mmx;

xx_csp_convert_f xx_yuyv_to_i420_c;
xx_csp_convert_f xx_yuyv_to_i420_mmx;
xx_csp_convert_f xx_yuyv_to_i420_sse2;

xx_csp_convert_f xx_i420_to_rgb565_c;

xx_csp_convert_f xx_nv12_to_i420_neon;
xx_csp_convert_f xx_i420_to_rgb565_neon;

extern xx_csp_convert_pf xx_i420_to_yuyv;

xx_csp_convert_f xx_i420_to_yuyv_c;
xx_csp_convert_f xx_i420_to_yuyv_mmx;
xx_csp_convert_f xx_i420_to_yuyv_sse2;

void c264_csp_init(uint32_t cpu);

#ifdef __cplusplus
}
#endif
