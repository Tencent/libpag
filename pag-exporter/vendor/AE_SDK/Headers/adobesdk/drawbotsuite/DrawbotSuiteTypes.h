/**************************************************************************
 *
 * ADOBE CONFIDENTIAL
 * ___________________
 *
 *  Copyright 2009 Adobe Systems Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains the property of
 * Adobe Systems Incorporated  and its suppliers,  if any.  The intellectual 
 * and technical concepts contained herein are proprietary to  Adobe Systems 
 * Incorporated  and its suppliers  and may be  covered by U.S.  and Foreign 
 * Patents,patents in process,and are protected by trade secret or copyright 
 * law.  Dissemination of this  information or reproduction of this material
 * is strictly  forbidden  unless prior written permission is  obtained from 
 * Adobe Systems Incorporated.
 **************************************************************************/
// Author: pankajn@adobe.com

#ifndef DRAWBOT_SUITE_TYPES_H
#define DRAWBOT_SUITE_TYPES_H

#include <adobesdk/config/PreConfig.h>


typedef	struct _DRAWBOT_DrawRef				*DRAWBOT_DrawRef;
typedef struct _DRAWBOT_SupplierRef			*DRAWBOT_SupplierRef;
typedef struct _DRAWBOT_SurfaceRef 			*DRAWBOT_SurfaceRef;
typedef struct _DRAWBOT_PenRef				*DRAWBOT_PenRef;
typedef struct _DRAWBOT_PathRef				*DRAWBOT_PathRef;
typedef struct _DRAWBOT_BrushRef			*DRAWBOT_BrushRef;
typedef struct _DRAWBOT_ImageRef			*DRAWBOT_ImageRef;
typedef struct _DRAWBOT_FontRef				*DRAWBOT_FontRef;
typedef struct _DRAWBOT_ObjectRef			*DRAWBOT_ObjectRef;


// 0.0f to 1.0f
typedef struct {
	float	red;
	float	green;
	float	blue;
	float	alpha;
} DRAWBOT_ColorRGBA;


typedef struct {
	float	x;
	float	y;
} DRAWBOT_PointF32;


typedef struct {
	float	left;
	float	top;
	float	width;	//not right
	float	height;	//not bottom
} DRAWBOT_RectF32;


typedef struct {
	int		left;
	int		top;
	int		width;	//not right
	int		height;	//not bottom
} DRAWBOT_Rect32;


typedef struct {
	float	mat[3][3];
} DRAWBOT_MatrixF32;


//Enum to fill a path
enum {
	kDRAWBOT_FillType_EvenOdd = 0,
	kDRAWBOT_FillType_Winding,

	kDRAWBOT_FillType_Default = kDRAWBOT_FillType_Winding
};
typedef	int	DRAWBOT_FillType;


//Enum to specify pixel layout of buffer to create an image.
enum {
	kDRAWBOT_PixelLayout_24RGB = 0,
	kDRAWBOT_PixelLayout_24BGR,
	kDRAWBOT_PixelLayout_32RGB, // ARGB (A is ignored).
	kDRAWBOT_PixelLayout_32BGR, // BGRA (A is ignored).
	kDRAWBOT_PixelLayout_32ARGB_Straight,
	kDRAWBOT_PixelLayout_32ARGB_Premul,
	kDRAWBOT_PixelLayout_32BGRA_Straight,
	kDRAWBOT_PixelLayout_32BGRA_Premul
};
typedef	int	DRAWBOT_PixelLayout;


//Enum to specify the text alignment (used in DrawString)
enum {
	kDRAWBOT_TextAlignment_Left = 0,
	kDRAWBOT_TextAlignment_Center,
	kDRAWBOT_TextAlignment_Right,
	
	kDRAWBOT_TextAlignment_Default = kDRAWBOT_TextAlignment_Left
};
typedef	int	DRAWBOT_TextAlignment;


//Enum to specify text truncation (used in DrawString)
enum {
	kDRAWBOT_TextTruncation_None = 0,
	kDRAWBOT_TextTruncation_End,
	kDRAWBOT_TextTruncation_EndEllipsis,
	kDRAWBOT_TextTruncation_PathEllipsis // tries to preserve the disk name ellipsis in middle of string
};
typedef	int	DRAWBOT_TextTruncation;


//Enum to specify surface interpolation level
enum {
		kDRAWBOT_InterpolationPolicy_None = 0,
		kDRAWBOT_InterpolationPolicy_Med,
		kDRAWBOT_InterpolationPolicy_High,

		kDRAWBOT_InterpolationPolicy_Default = kDRAWBOT_InterpolationPolicy_None
}; 
typedef int	DRAWBOT_InterpolationPolicy;


//Enum to specify drawing anti-aliasing level
enum {
		kDRAWBOT_AntiAliasPolicy_None = 0,
		kDRAWBOT_AntiAliasPolicy_Med,
		kDRAWBOT_AntiAliasPolicy_High,

		kDRAWBOT_AntiAliasPolicy_Default = kDRAWBOT_AntiAliasPolicy_None
}; 
typedef int DRAWBOT_AntiAliasPolicy;


#include <adobesdk/config/PostConfig.h>

#endif //DRAWBOT_SUITE_TYPES_H





	