/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/** A.h

	Adobe-standard (hopefully one day) types to promote plug-in sharing.

**/
#ifndef _H_A
#define _H_A

#ifdef A_INTERNAL

	#include <A_Private.h>

#else
	#include "stdint.h"

	typedef int32_t			A_long;
	typedef uint32_t		A_u_long;
	typedef char			A_char;
	typedef double			A_FpLong;
	typedef float			A_FpShort;
	typedef A_long			A_Err;
	typedef void *			A_Handle;
	typedef A_long			A_Fixed;
	typedef A_u_long		A_UFixed;

	#if defined( __MWERKS__) || defined (__GNUC__)  // metrowerks codewarrior and XCode/GCC
		typedef int16_t			A_short;
		typedef uint16_t		A_u_short;
		typedef uint8_t			A_u_char;
		typedef uint8_t			A_Boolean;
		typedef intptr_t		A_intptr_t;
	#else // windows
		typedef short			A_short;
		typedef unsigned short	A_u_short;
		typedef unsigned char	A_u_char;
		typedef unsigned char	A_Boolean;	
		#ifdef  _WIN64
			typedef __int64     A_intptr_t;
		#else
			typedef  int32_t       A_intptr_t;
		#endif
	#endif

#ifdef _MSC_VER	// visual c++ 
	typedef unsigned __int64 	A_u_longlong;
#endif

#if defined( __MWERKS__) || defined (__GNUC__)  // metrowerks codewarrior and XCode/GCC
	typedef uint64_t	A_u_longlong;
#endif


#include <adobesdk/config/PreConfig.h>


	typedef struct {
		A_long		value;
		A_u_long	scale;
	} A_Time;

	typedef struct {
		A_long		num;	/* numerator */
		A_u_long	den;	/* denominator */
	} A_Ratio;
	
	typedef struct {
		A_FpLong			x, y;
	} A_FloatPoint;
	
	typedef struct {
		A_FpLong			x, y, z;
	} A_FloatPoint3;

	typedef struct {
		A_FpLong			left, top, right, bottom;
	} A_FloatRect;

	typedef struct {
		A_FpLong			mat[3][3];
	} A_Matrix3;

	typedef struct {
		A_FpLong			mat[4][4];
	} A_Matrix4;

	typedef struct {
		A_short				top, left, bottom, right;
	} A_LegacyRect;

	typedef struct {
		A_long				left, top, right, bottom;
	} A_LRect;

	typedef		A_LRect		A_Rect;

	typedef struct {
		A_long				x, y;
	} A_LPoint;

	typedef struct {
		A_FpLong			radius, angle; 
	} A_FloatPolar; 

	typedef		A_u_longlong	A_HandleSize;
	
#include <adobesdk/config/PostConfig.h>




#endif


#include <adobesdk/config/PreConfig.h>

typedef struct {
	A_FpLong		alpha;
	A_FpLong		red;
	A_FpLong		green;
	A_FpLong		blue;

} A_Color;

#include <adobesdk/config/PostConfig.h>





#define A_THROW(ERR)		throw ((A_long) ERR)	

enum {
	A_Err_NONE = 0,
	A_Err_GENERIC,
	A_Err_STRUCT,
	A_Err_PARAMETER,
	A_Err_ALLOC,
	A_Err_WRONG_THREAD,					// Some calls can only be used on UI (Main) or Render threads. Also, calls back to AE can only be made from the same thread AE called you on
	A_Err_CONST_PROJECT_MODIFICATION,	// An attempt was made to write to a read only copy of an AE project. Project changes must originate in the UI/Main thread.
	A_Err_RESERVED_7,
	A_Err_RESERVED_8,
	A_Err_RESERVED_9,
	A_Err_RESERVED_10,
	A_Err_RESERVED_11,
	A_Err_RESERVED_12,
    A_Err_MISSING_SUITE = 13,	// acquire suite failed on a required suite
	A_Err_RESERVED_14,
	A_Err_RESERVED_15,
	A_Err_RESERVED_16,
	A_Err_RESERVED_17,
	A_Err_RESERVED_18,
	A_Err_RESERVED_19,
	A_Err_RESERVED_20,
	A_Err_RESERVED_21,
	A_Err_NOT_IN_CACHE_OR_COMPUTE_PENDING,
	A_Err_PROJECT_LOAD_FATAL,
	A_Err_EFFECT_APPLY_FATAL,

	A_Err_LAST
};

typedef struct {
	A_short		majorS;
	A_short		minorS;
} A_Version;

typedef struct _Up_OpaqueMem **AEGP_MemHandle;

typedef	A_u_short			A_UTF16Char;

#if defined( __MWERKS__) || defined (__GNUC__)  // metrowerks codewarrior and XCode/GCC
	typedef A_char			A_LegacyEnumType;
#else // windows
	typedef A_long			A_LegacyEnumType;
#endif

#endif
