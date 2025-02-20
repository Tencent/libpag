#ifndef _H_AE_PIXEL_FORMAT
#define _H_AE_PIXEL_FORMAT

#include "A.h"

/* data types for PF_EffectWorlds that are not just 8bpc ARGB
 *
 * see AE_EffectCBSuites.h for more details
 *
 */

#ifndef MAKE_PIXEL_FORMAT_FOURCC

#define MAKE_PIXEL_FORMAT_FOURCC(ch0, ch1, ch2, ch3)                              \
                ((A_u_long)(A_u_char)(ch0) | ((A_u_long)(A_u_char)(ch1) << 8) |       \
                ((A_u_long)(A_u_char)(ch2) << 16) | ((A_u_long)(A_u_char)(ch3) << 24 ))

#endif


//
// note! MAKE_PIXEL_FORMAT_FOURCC('a', 'r', 'g', 'b') does not give the same result
// as the literal 'argb' in your compiler
//
// MS image compression interfaces define FOURCC this way (unlike QuickTime) so please
// be careful and use the enums as defined rather than rolling your own.
//

enum {
	PF_PixelFormat_ARGB32 		= MAKE_PIXEL_FORMAT_FOURCC('a', 'r', 'g', 'b'),		// After Effects-style ARGB, 8 bits per channel, range 0...255
																					// trillions of pixels served since 1992. support required for After Effects
	
	PF_PixelFormat_ARGB64		= MAKE_PIXEL_FORMAT_FOURCC('a', 'e', '1', '6'),		// After Effects-style ARGB, 16 bits per channel, range 0...32768
	PF_PixelFormat_ARGB128		= MAKE_PIXEL_FORMAT_FOURCC('a', 'e', '3', '2'),		// After Effects-style ARGB, 32 bits floating point per channel, 1.0 is "white"
	
	
	/* -------------------------------------------------------------------------------------- */
	
	PF_PixelFormat_GPU_BGRA128		= MAKE_PIXEL_FORMAT_FOURCC('@', 'C', 'D', 'A'),		// GPU, BGRA, 32 bits floating point per channel.
	
	PF_PixelFormat_RESERVED			= MAKE_PIXEL_FORMAT_FOURCC('@', 'C', 'D', 'a'),		// reserved for future use.
	
	/* -------------------------------------------------------------------------------------- */
	
	PF_PixelFormat_BGRA32		= MAKE_PIXEL_FORMAT_FOURCC('b', 'g', 'r', 'a'),		// Premiere-style BGRA, 8 bits per channel. Premiere-only; support required for Premiere
	PF_PixelFormat_VUYA32		= MAKE_PIXEL_FORMAT_FOURCC('v', 'u', 'y', 'a'),		// Premiere-style YUVA, 8 bits per channel. Premiere-only
		
	PF_PixelFormat_NTSCDV25		= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'n', '2'),		// compressed DV-25. Premiere only.
	PF_PixelFormat_PALDV25		= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', 'p', '2'),		// compressed DV-25. Premiere only.

	PF_PixelFormat_INVALID		= MAKE_PIXEL_FORMAT_FOURCC('b', 'a', 'd', 'f'),		// invalid pixel format - this is used for intialization and error conditions


	PF_PixelFormat_FORCE_LONG_INT = 0xFFFFFFFF
};

typedef A_long PF_PixelFormat;


#ifdef PREMIERE
// for Premiere-specific pixel format support
// some of these are aliases of formats already available as a PF_PixelFormat

#ifndef PRSDKPIXELFORMAT_H
typedef  PF_PixelFormat PrPixelFormat;

enum {
	// Uncompressed formats - these are most common for effects
	PrPixelFormat_BGRA_4444_8u		= PF_PixelFormat_BGRA32,
	PrPixelFormat_VUYA_4444_8u		= PF_PixelFormat_VUYA32,
	PrPixelFormat_ARGB_4444_8u		= PF_PixelFormat_ARGB32,
	PrPixelFormat_BGRA_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('B', 'g', 'r', 'a'),		// 16 bit integer per component BGRA
	PrPixelFormat_VUYA_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('V', 'u', 'y', 'a'),		// 16 bit integer per component VUYA
	PrPixelFormat_ARGB_4444_16u		= MAKE_PIXEL_FORMAT_FOURCC('A', 'r', 'g', 'b'),		// 16 bit integer per component ARGB 
	PrPixelFormat_BGRA_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('B', 'G', 'r', 'a'),		// 32 bit float per component BGRA
	PrPixelFormat_VUYA_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('V', 'U', 'y', 'a'),		// 32 bit float per component VUYA
	PrPixelFormat_ARGB_4444_32f		= MAKE_PIXEL_FORMAT_FOURCC('A', 'R', 'g', 'b'),		// 32 bit float per component ARGB

	// Packed formats
	PrPixelFormat_YUYV_422_8u_601	= MAKE_PIXEL_FORMAT_FOURCC('y', 'u', 'y', '2'),		// 8 bit 422 YUY2 601 colorspace
	PrPixelFormat_YUYV_422_8u_709	= MAKE_PIXEL_FORMAT_FOURCC('y', 'u', 'y', '3'),		// 8 bit 422 YUY2 709 colorspace
	PrPixelFormat_UYVY_422_8u_601	= MAKE_PIXEL_FORMAT_FOURCC('u', 'y', 'v', 'y'),		// 8 bit 422 UYVY 601 colorspace
	PrPixelFormat_UYVY_422_8u_709	= MAKE_PIXEL_FORMAT_FOURCC('u', 'y', 'v', '7'),		// 8 bit 422 UYVY 709 colorspace
	PrPixelFormat_V210_422_10u_601	= MAKE_PIXEL_FORMAT_FOURCC('v', '2', '1', '0'),		// packed uncompressed 10 bit 422 YUV aka V210 601 colorspace
	PrPixelFormat_V210_422_10u_709	= MAKE_PIXEL_FORMAT_FOURCC('v', '2', '1', '1'),	

	// Planar formats
	PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', '1', '2'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2)
	PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u	= MAKE_PIXEL_FORMAT_FOURCC('y', 'v', 'i', '2'),		//	YUV with 2x2 chroma subsampling. Planar. (for MPEG-2)

	// Compressed formats
	PrPixelFormat_NTSCDV25			= PF_PixelFormat_NTSCDV25,		// compressed DV-25
	PrPixelFormat_PALDV25			= PF_PixelFormat_PALDV25,		// compressed DV-25
	PrPixelFormat_720pDV100			= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', '7', '1'),		// compressed DV-100 720p
	PrPixelFormat_1080iDV100		= MAKE_PIXEL_FORMAT_FOURCC('d', 'v', '1', '1'),		// compressed DV-100 1080i

	// Raw, opaque data formats
	PrPixelFormat_Raw				= MAKE_PIXEL_FORMAT_FOURCC('r', 'a', 'w', 'w'),		// raw, opaque data, with no row bytes or height

	// Invalid
	PrPixelFormat_Invalid			= PF_PixelFormat_INVALID,		// invalid pixel format - this is used for intialization and error conditions

	PrPixelFormat_Any				= 0
};

#endif	// PRSDKPIXELFORMAT_H

#endif // PREMIERE

#endif	// AE_PixelFormat
