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

#ifndef _H_HOOK_AE
#define _H_HOOK_AE


#include <AE_Effect.h>			// for PF_Version, other types


#ifdef __cplusplus
	extern "C" {
#endif


#define AE_HOOK_PLUGIN_TYPE		'AEgp'

#define AE_HOOK_MAJOR_VERSION		3
#define AE_HOOK_MINOR_VERSION		0

enum {
	AE_PixFormat_NONE = -1,		// sentinel -- meaningless
	AE_PixFormat_ARGB = 0,
	AE_PixFormat_BGRA
};

typedef A_long AE_PixFormat;

enum {
	AE_BlitOutFlag_NONE = 0,
	AE_BlitOutFlag_ASYNCHRONOUS = 1L << 0
};

enum {
	AE_BlitInFlag_NONE = 0,
	AE_BlitInFlag_RENDERING = 1L << 0
};

typedef A_long AE_BlitInFlags;
typedef A_long AE_BlitOutFlags;

// AE now supports 8bpp, 16bpp and 32bpc
// depthL, chan_bytesL, and plane_bytesL
// can now change.
typedef struct {
	A_long			widthL;
	A_long			heightL;
	A_long			depthL;			// 32, 64, or 128
	AE_PixFormat	pix_format;		// always AE_PixFormat_ARGB on Mac, BGRA on Windows (for now)

	A_long			row_bytesL;
	A_long			chan_bytesL;	// 4, 8, or 16
	A_long			plane_bytesL;	// 1, 2, or 4 (for float)

	void			*pixelsPV;

} AE_PixBuffer;



typedef struct {
	A_long		frame_widthL;			// original size of image regardless of region of interest
	A_long		frame_heightL;

	A_long		origin_xL;				// where the pix buffer is placed in frame coords
	A_long		origin_yL;		

	A_long		view_rect_leftL;		// what the user is actually seeing in the window
	A_long		view_rect_topL;
	A_long		view_rect_rightL;
	A_long		view_rect_bottomL;


} AE_ViewCoordinates;



typedef struct _AE_BlitReceipt			*AE_BlitReceipt;		// opaque

typedef struct _AE_CursorInfo			*AE_CursorInfo;			// opaque until I can decide what this means



typedef void		(*AE_DeathHook)(	void *hook_refconPV);						/* >> */	

typedef void		(*AE_VersionHook)(	void *hook_refconPV,						/* >> */	
										A_u_long *versionPV);					/* << */


typedef struct		FILE_Spec			**AE_FileSpecH;


typedef void		(*AE_BlitCompleteFunc)(	AE_BlitReceipt			receipt,
											PF_Err					err);		// non zero if error during asynch operation


typedef PF_Err		(*AE_BlitHook)(		void						*hook_refconPV,	/* >> */
										const AE_PixBuffer			*pix_bufP0,		/* >> 	if NULL, display a blank frame */
										const AE_ViewCoordinates	*viewP,			/* >> */
										AE_BlitReceipt				receipt,		/* >> */
										AE_BlitCompleteFunc			complete_func0,	/* >> */
										AE_BlitInFlags				in_flags,		/* >> */
										AE_BlitOutFlags				*out_flags);	/* << */



typedef void		(*AE_CursorHook)(	void						*hook_refconPV,	/* >> */	
										const AE_CursorInfo			*cursorP);		/* >> */

typedef struct {
	// must match NIM_Hooks
	
	void				*hook_refconPV;
	void				*reservedAPV[8];
	AE_DeathHook		death_hook_func;
	AE_VersionHook		version_hook_func;
	struct SPBasicSuite	*pica_basicP;
	AE_BlitHook			blit_hook_func;
	AE_CursorHook		cursor_hook_func;

} AE_Hooks;



typedef PF_Err (*AE_HookPluginEntryFunc)(	A_long major_version,			/* >> */		
											A_long minor_version,			/* >> */		
											AE_FileSpecH file_specH,	/* >> */		
											AE_FileSpecH res_specH,		/* >> */		
											AE_Hooks *hooksP);			/* << */

#ifdef __cplusplus
	}
#endif


#endif

