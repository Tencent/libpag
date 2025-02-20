// ADOBE SYSTEMS INCORPORATED
// (c) Copyright  2009 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE:  Adobe permits you to use, modify, and distribute this 
// file in accordance with the terms of the Adobe license agreement
// accompanying it.  If you have received this file from a source
// other than Adobe, then your use, modification, or distribution
// of it requires the prior written permission of Adobe.
//-------------------------------------------------------------------
/**
*
*	\file PSIntTypes.h
*
*	\brief 
*      	Fixed sized integer types used in Photoshop
*
*      Distribution:
*      	PUBLIC
*      
*/     	

#ifndef __PSIntTypes__
#define __PSIntTypes__

//-------------------------------------------------------------------

#if defined(_MSC_VER) && defined(__cplusplus)
#include <cstdint>
#else
#include <stdint.h>
#endif

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef uint8_t  unsigned8;
typedef uint16_t unsigned16;
typedef uint32_t unsigned32;
typedef uint64_t unsigned64;


//-------------------------------------------------------------------
//-------------------------------------------------------------------

#ifndef _BOOL8
    #define _BOOL8
	typedef unsigned char	Bool8;
#endif

#ifndef _BOOL32
	#define _BOOL32
	typedef uint32 Bool32;
#endif

//-------------------------------------------------------------------
//-------------------------------------------------------------------
//REVISIT - PSFixedTypes.h would probably be a more appropriate name for this file

#ifndef _REAL32
    #define _REAL32
	typedef float	real32;
#endif

#ifndef _REAL64
    #define _REAL64
	typedef double	real64;
#endif

typedef double nativeFloat;

#endif /* __PSIntTypes.h__ */
