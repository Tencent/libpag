/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2002-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Inc. and its suppliers, if                    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Inc. and its                    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

#ifndef PRSDKPIXELFORMAT_H
#define PRSDKPIXELFORMAT_H

#ifdef PREMIERE_INTERNAL

#ifndef PRSDKTYPES_H
#include "PrSDKTypes.h"
#endif

#endif

#ifndef MAKE_PIXEL_FORMAT_FOURCC
#define MAKE_PIXEL_FORMAT_FOURCC(ch0, ch1, ch2, ch3)                         		    \
                (static_cast<unsigned int>(static_cast<unsigned char>(ch0)) |				\
                (static_cast<unsigned int>(static_cast<unsigned char>(ch1)) << 8) |       \
                (static_cast<unsigned int>(static_cast<unsigned char>(ch2)) << 16) |		\
                (static_cast<unsigned int>(static_cast<unsigned char>(ch3)) << 24 ))
#endif

/**
**	Premiere supported pixel formats for RenderFrame and PPixs
*/

enum PrPixelFormat
{

/**
**	Currently supported types
*/

// Packed formats
PrPixelFormat_BGRA_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('b', 'g', 'r', 'a'),		// 4 byte BGRA, standard windows 32 bit pixels (was kPixelFormat_BGRA32)
PrPixelFormat_VUYA_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('v', 'u', 'y', 'a'),		// 4 byte VUYA (was kPixelFormat_VUYA32)
PrPixelFormat_VUYA_4444_8u_709		= MAKE_PIXEL_FORMAT_FOURCC('V', 'U', 'Y', 'A'),		// 4 byte VUYA (was kPixelFormat_VUYA32)
PrPixelFormat_ARGB_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('a', 'r', 'g', 'b'),		// 4 byte ARGB (the format used by AE) (was kPixelFormat_ARGB32)

PrPixelFormat_BGRX_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('b', 'g', 'r', 'x'),		// 4 byte BGRX
PrPixelFormat_VUYX_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('v', 'u', 'y', 'x'),		// 4 byte VUYX 
PrPixelFormat_VUYX_4444_8u_709		= MAKE_PIXEL_FORMAT_FOURCC('v', 'u', 'x', '7'),		// 4 byte VUYX 
PrPixelFormat_XRGB_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('x', 'r', 'g', 'b'),		// 4 byte XRGB 
																										   
PrPixelFormat_BGRP_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('b', 'g', 'r', 'p'),		// 4 byte BGRP
PrPixelFormat_VUYP_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('v', 'u', 'y', 'p'),		// 4 byte VUYP 
PrPixelFormat_VUYP_4444_8u_709		= MAKE_PIXEL_FORMAT_FOURCC('v', 'u', 'p', '7'),		// 4 byte VUYP
PrPixelFormat_PRGB_4444_8u		= MAKE_PIXEL_FORMAT_FOURCC('p', 'r', 'g', 'b'),		// 4 byte PRGB 

PrPixelFormat_BGRA_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('B', 'g', 'r', 'a'),		// 16 bit integer per component BGRA
PrPixelFormat_VUYA_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('V', 'u', 'y', 'a'),		// 16 bit integer per component VUYA
PrPixelFormat_ARGB_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('A', 'r', 'g', 'b'),		// 16 bit integer per component ARGB 

PrPixelFormat_BGRX_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('B', 'g', 'r', 'x'),		// 16 bit integer per component BGRX
PrPixelFormat_XRGB_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('X', 'r', 'g', 'b'),		// 16 bit integer per component XRGB 

PrPixelFormat_BGRP_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('B', 'g', 'r', 'p'),		// 16 bit integer per component BGRP
PrPixelFormat_PRGB_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('P', 'r', 'g', 'b'),		// 16 bit integer per component PRGB 

PrPixelFormat_BGRA_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('B', 'G', 'r', 'a'),		// 32 bit float per component BGRA
PrPixelFormat_VUYA_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('V', 'U', 'y', 'a'),		// 32 bit float per component VUYA
PrPixelFormat_VUYA_4444_32f_709		= MAKE_PIXEL_FORMAT_FOURCC('v', 'U', 'Y', 'a'),		// 32 bit float per component VUYA
PrPixelFormat_ARGB_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('A', 'R', 'g', 'b'),		// 32 bit float per component ARGB 

PrPixelFormat_BGRX_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('B', 'G', 'r', 'x'),		// 32 bit float per component BGRX
PrPixelFormat_VUYX_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('V', 'U', 'y', 'x'),		// 32 bit float per component VUYX
PrPixelFormat_VUYX_4444_32f_709		= MAKE_PIXEL_FORMAT_FOURCC('V', 'U', 'x', '7'),		// 32 bit float per component VUYX
PrPixelFormat_XRGB_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('X', 'R', 'g', 'b'),		// 32 bit float per component XRGB 

PrPixelFormat_BGRP_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('B', 'G', 'r', 'p'),		// 32 bit float per component BGRX
PrPixelFormat_VUYP_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('V', 'U', 'y', 'p'),		// 32 bit float per component VUYX
PrPixelFormat_VUYP_4444_32f_709		= MAKE_PIXEL_FORMAT_FOURCC('V', 'U', 'p', '7'),		// 32 bit float per component VUYX
PrPixelFormat_PRGB_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('P', 'R', 'g', 'b'),		// 32 bit float per component XRGB 

PrPixelFormat_RGB_444_10u		= MAKE_PIXEL_FORMAT_FOURCC('R', 'G', 'B', '1'),		// Full range 10-bit 444 RGB little-endian

PrPixelFormat_YUYV_422_8u_601	= MAKE_PIXEL_FORMAT_FOURCC('y', 'u', 'y', '2'),		// 8 bit 422 YUY2 601 colorspace
PrPixelFormat_YUYV_422_8u_709	= MAKE_PIXEL_FORMAT_FOURCC('y', 'u', 'y', '3'),		// 8 bit 422 YUY2 709 colorspace

PrPixelFormat_UYVY_422_8u_601	= MAKE_PIXEL_FORMAT_FOURCC('u', 'y', 'v', 'y'),		// 8 bit 422 UYVY 601 colorspace
PrPixelFormat_UYVY_422_8u_709	= MAKE_PIXEL_FORMAT_FOURCC('u', 'y', 'v', '7'),		// 8 bit 422 UYVY 709 colorspace

PrPixelFormat_V210_422_10u_601	= MAKE_PIXEL_FORMAT_FOURCC('v', '2', '1', '0'),		// packed uncompressed 10 bit 422 YUV aka V210 601 colorspace
PrPixelFormat_V210_422_10u_709	= MAKE_PIXEL_FORMAT_FOURCC('v', '2', '1', '1'),		// packed uncompressed 10 bit 422 YUV aka V210 709 colorspace

PrPixelFormat_UYVY_422_32f_601	= MAKE_PIXEL_FORMAT_FOURCC('U', 'Y', 'v', 'y'),
PrPixelFormat_UYVY_422_32f_709	= MAKE_PIXEL_FORMAT_FOURCC('U', 'Y', 'v', '7'),

PrPixelFormat_BGRA_4444_32f_Linear	= MAKE_PIXEL_FORMAT_FOURCC('B', 'G', 'a', 'L'),	// 32 bit float per component linear BGRA
PrPixelFormat_BGRP_4444_32f_Linear	= MAKE_PIXEL_FORMAT_FOURCC('B', 'G', 'p', 'L'),	// 32 bit float per component linear BGRP
PrPixelFormat_BGRX_4444_32f_Linear	= MAKE_PIXEL_FORMAT_FOURCC('B', 'G', 'x', 'L'),	// 32 bit float per component linear BGRX
PrPixelFormat_ARGB_4444_32f_Linear	= MAKE_PIXEL_FORMAT_FOURCC('A', 'R', 'g', 'L'),	// 32 bit float per component linear ARGB
PrPixelFormat_PRGB_4444_32f_Linear	= MAKE_PIXEL_FORMAT_FOURCC('P', 'R', 'g', 'L'),	// 32 bit float per component linear PRGB
PrPixelFormat_XRGB_4444_32f_Linear	= MAKE_PIXEL_FORMAT_FOURCC('X', 'R', 'g', 'L'),	// 32 bit float per component linear XRGB

PrPixelFormat_RGB_444_12u_PQ_709	= MAKE_PIXEL_FORMAT_FOURCC('@', 'P', 'Q', '7'),	// 12 bit integer (in 16 bit words) per component RGB with PQ curve, Rec.709 primaries
PrPixelFormat_RGB_444_12u_PQ_P3		= MAKE_PIXEL_FORMAT_FOURCC('@', 'P', 'Q', 'P'),	// 12 bit integer (in 16 bit words) per component RGB with PQ curve, P3 primaries
PrPixelFormat_RGB_444_12u_PQ_2020	= MAKE_PIXEL_FORMAT_FOURCC('@', 'P', 'Q', '2'), // 12 bit integer (in 16 bit words) per component RGB with PQ curve, Rec.2020 primaries

// Planar formats
PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', '1', '2'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2)
PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', 'i', '2'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2)
PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601_FullRange	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', '1', 'f'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2)
PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601_FullRange	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', 'i', 'f'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2)
PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', '1', '7'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2) 709 colorspace
PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', 'i', '7'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2) 709 colorspace
PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709_FullRange	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', '1', 'F'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2) 709 colorspace
PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709_FullRange	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', 'i', 'F'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2) 709 colorspace

PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601	= MAKE_PIXEL_FORMAT_FOURCC('Y', 'v', '1', '2'),		//	YUV with 2x2 chroma subsampling, center chroma. Planar. 
PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601	= MAKE_PIXEL_FORMAT_FOURCC('Y', 'v', 'i', '2'),		//	YUV with 2x2 chroma subsampling, center chroma. Planar. 
PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange	= MAKE_PIXEL_FORMAT_FOURCC('Y', 'v', '1', 'f'),		//	YUV with 2x2 chroma subsampling, center chroma. Planar. 
PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601_FullRange	= MAKE_PIXEL_FORMAT_FOURCC('Y', 'v', 'i', 'f'),		//	YUV with 2x2 chroma subsampling, center chroma. Planar. 
PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709	= MAKE_PIXEL_FORMAT_FOURCC('Y', 'v', '1', '7'),		//	YUV with 2x2 chroma subsampling, center chroma. Planar. 709 colorspace
PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709	= MAKE_PIXEL_FORMAT_FOURCC('Y', 'v', 'i', '7'),		//	YUV with 2x2 chroma subsampling, center chroma. Planar. 709 colorspace
PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange	= MAKE_PIXEL_FORMAT_FOURCC('Y', 'v', '1', 'F'),		//	YUV with 2x2 chroma subsampling, center chroma. Planar. 709 colorspace
PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709_FullRange	= MAKE_PIXEL_FORMAT_FOURCC('Y', 'v', 'i', 'F'),		//	YUV with 2x2 chroma subsampling, center chroma. Planar. 709 colorspace

// Compressed formats
PrPixelFormat_NTSCDV25			= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'n', '2'),		//	compressed DV-25
PrPixelFormat_PALDV25			= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'p', '2'),		//	compressed DV-25
PrPixelFormat_NTSCDV50			= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'n', '5'),
PrPixelFormat_PALDV50			= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'p', '5'),
PrPixelFormat_NTSCDV100_720p	= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'n', '7'),		//	compressed DV-100 720p, 60 Hz
PrPixelFormat_PALDV100_720p		= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'p', '7'),		//	compressed DV-100 720p, 50 Hz
PrPixelFormat_NTSCDV100_1080i	= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'n', '1'),		//	compressed DV-100 1080i, 60 Hz
PrPixelFormat_PALDV100_1080i	= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'p', '1'),		//	compressed DV-100 1080i, 50 Hz

// Raw, opaque data formats
PrPixelFormat_Raw			= MAKE_PIXEL_FORMAT_FOURCC('r', 'a', 'w', 'w'),		// raw, opaque data, with no row bytes or height.

// Invalid
PrPixelFormat_Invalid		= MAKE_PIXEL_FORMAT_FOURCC('b', 'a', 'd', 'f'),		// invalid pixel format - this is used for intialization and erorr conditions

PrPixelFormat_Any			= 0
};

#define MAKE_ADOBE_PRIVATE_PIXEL_FORMAT_FOURCC(ch1, ch2, ch3) (static_cast<PrPixelFormat>(MAKE_PIXEL_FORMAT_FOURCC('@', ch1, ch2, ch3)))
#define MAKE_THIRD_PARTY_CUSTOM_PIXEL_FORMAT_FOURCC(ch1, ch2, ch3) (static_cast<PrPixelFormat>(MAKE_PIXEL_FORMAT_FOURCC('_', ch1, ch2, ch3)))

#endif // PRSDKPIXELFORMAT_H

