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

#ifndef _H_AE_PlugGeneral
#define _H_AE_PlugGeneral

#include <A.h>
#include <FIEL_Public.h>
#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <SPBasic.h>

#include <PR_Public.h>
#include <PF_Masks.h>
#include <AE_EffectSuites.h>
#ifdef AEGP_INTERNAL
#include <AE_GeneralPlug_Private.h>
#endif
#include <AE_IO.h>
#include <PT_Public.h>


#include <AE_GeneralPlugPre.h>

#define AEGP_PLUGIN_TYPE			'AEgx'

#define AEGP_INITFUNC_MAJOR_VERSION			1
#define AEGP_INITFUNC_MINOR_VERSION			9

#ifndef AEGP_INTERNAL
	typedef struct _AEGP_Project			**AEGP_ProjectH;
	typedef struct _AEGP_Item 				**AEGP_ItemH;
	typedef struct _AEGP_Comp				**AEGP_CompH;
	typedef struct _AEGP_Footage			**AEGP_FootageH;
	typedef struct _AEGP_Layer				**AEGP_LayerH;
	typedef struct _AEGP_Effect				**AEGP_EffectRefH;
	typedef struct _AEGP_Mask				**AEGP_MaskRefH;
	typedef struct _AEGPp_Stream			**AEGP_StreamRefH;
	typedef struct _AEGP_LayerContext		**AEGP_RenderLayerContextH;
	typedef struct _AEGP_PersistentBlob		**AEGP_PersistentBlobH;
	typedef struct _AEGP_MaskOutline		**AEGP_MaskOutlineValH;
	typedef struct _AEGP_Collection			**AEGP_CollectionH;
	typedef struct _AEGP_Collection2		**AEGP_Collection2H;
	typedef struct _AEGP_SoundData			**AEGP_SoundDataH;
	typedef struct _AEGP_AddKeyframesInfo	**AEGP_AddKeyframesInfoH;
	typedef struct _AEGP_RenderReceipt		**AEGP_RenderReceiptH;
	typedef struct _AEGP_World				**AEGP_WorldH;
	typedef struct _AEGP_RenderOptions		**AEGP_RenderOptionsH;
	typedef struct _AEGP_LayerRenderOptions	**AEGP_LayerRenderOptionsH;
	typedef struct _AEGP_FrameReceipt		**AEGP_FrameReceiptH;
	typedef struct _AEGP_RenderQueueItem 	**AEGP_RQItemRefH;
	typedef struct _AEGP_OutputModule		**AEGP_OutputModuleRefH;
	typedef struct _AEGP_TextDocument		**AEGP_TextDocumentH;
	typedef struct _AEGP_MarkerVal			*AEGP_MarkerValP;
    // AEGP_ConstMarkerValP is defined in AE_IO.h;
	typedef struct _AEGP_TextOutlines		**AEGP_TextOutlinesH;
	typedef struct _AEGP_TimeStamp{A_char a[4];}	AEGP_TimeStamp;
	typedef struct _AEGP_PlatformWorld		**AEGP_PlatformWorldH;
	typedef	struct _AEGP_ItemView			*AEGP_ItemViewP;
	typedef struct _AEGP_ColorProfile		*AEGP_ColorProfileP;
	typedef const struct _AEGP_ColorProfile	*AEGP_ConstColorProfileP;
#endif

typedef A_long	AEGP_SubLayerIndex;

#define AEGP_SubLayer_ALL			(-1L)

typedef A_long	AEGP_PluginID;
// define a _AEGP_Refcon1 struct to use a typesafe refcon.
typedef struct _AEGP_GlobalRefcon				*AEGP_GlobalRefcon;
typedef struct _AEGP_CommandRefcon				*AEGP_CommandRefcon;
typedef struct _AEGP_UpdateMenuRefcon			*AEGP_UpdateMenuRefcon;
typedef struct _AEGP_DeathRefcon				*AEGP_DeathRefcon;
typedef struct _AEGP_VersionRefcon				*AEGP_VersionRefcon;
typedef struct _AEGP_AboutStringRefcon			*AEGP_AboutStringRefcon;
typedef struct _AEGP_AboutRefcon				*AEGP_AboutRefcon;
typedef struct _AEGP_AsyncFrameRequestRefcon	*AEGP_AsyncFrameRequestRefcon;
typedef struct _AEGP_IdleRefcon					*AEGP_IdleRefcon;
typedef struct _AEGP_IORefcon					*AEGP_IORefcon;
typedef struct _AEGP_CancelRefcon				*AEGP_CancelRefcon;


// _SIZE constants include space for the terminating null. -1 for string length.
#define		AEGP_MAX_PATH_SIZE					260
#define		AEGP_MAX_ABOUT_STRING_SIZE			256
#define		AEGP_MAX_RQITEM_COMMENT_SIZE		256
#define		AEGP_MAX_TYPE_NAME_SIZE				32
#define		AEGP_MAX_ITEM_NAME_SIZE				32
#define		AEGP_MAX_ITEM_NAME_SIZE				32
#define		AEGP_MAX_LAYER_NAME_SIZE			32
#define		AEGP_MAX_MASK_NAME_SIZE				32
#define		AEGP_MAX_EFFECT_NAME_SIZE			(PF_MAX_EFFECT_NAME_LEN + 17)
#define		AEGP_MAX_EFFECT_MATCH_NAME_SIZE		(PF_MAX_EFFECT_NAME_LEN + 17)
#define		AEGP_MAX_EFFECT_CATEGORY_NAME_SIZE	(PF_MAX_EFFECT_CATEGORY_NAME_LEN + 1)
#define		AEGP_MAX_STREAM_NAME_SIZE			(PF_MAX_EFFECT_PARAM_NAME_LEN + 1)
#define		AEGP_MAX_STREAM_UNITS_SIZE			(PF_MAX_EFFECT_PARAM_NAME_LEN + 1)
#define		AEGP_MAX_PROJ_NAME_SIZE				48
#define		AEGP_MAX_PLUGIN_NAME_SIZE			32

#define		AEGP_MAX_MARKER_NAME_SIZE			64
#define		AEGP_MAX_MARKER_URL_SIZE			1024
#define		AEGP_MAX_MARKER_TARGET_SIZE			128
#define		AEGP_MAX_MARKER_CHAPTER_SIZE		128


enum {
	AEGP_Platform_MAC,
	AEGP_Platform_WIN
};
typedef	A_long	AEGP_Platform;

enum {
	AEGP_ProjBitDepth_8 = 0,
	AEGP_ProjBitDepth_16,
	AEGP_ProjBitDepth_32,
	AEGP_ProjBitDepth_NUM_VALID_DEPTHS
};
typedef A_char AEGP_ProjBitDepth;

#define AEGP_Index_NONE			((A_long)0x80000000)
#define AEGP_Index_START		((A_long)0)
#define AEGP_Index_END			((A_long)-1)

typedef A_long 	AEGP_Index;

#define AEGP_LayerIDVal_NONE			(0L)

typedef A_long			AEGP_LayerIDVal;

#define AEGP_MaskIDVal_NONE			(0L)

typedef A_long			AEGP_MaskIDVal;

typedef struct {
	A_FpLong		alphaF, redF, greenF, blueF;  // in the range [0.0 - 1.0]
} AEGP_ColorVal;


enum {
	AEGP_CameraType_NONE = -1,

	AEGP_CameraType_PERSPECTIVE,
	AEGP_CameraType_ORTHOGRAPHIC,

	AEGP_CameraType_NUM_TYPES
};

typedef A_u_long AEGP_CameraType;

enum {
	AEGP_FootageDepth_1		= 1,
	AEGP_FootageDepth_2  		= 2,
	AEGP_FootageDepth_4  		= 4,
	AEGP_FootageDepth_8  		= 8,
	AEGP_FootageDepth_16 		= 16,
	AEGP_FootageDepth_24 		= 24,
	AEGP_FootageDepth_30		= 30,
	AEGP_FootageDepth_32 		= 32,
	AEGP_FootageDepth_GRAY_2 	= 34,
	AEGP_FootageDepth_GRAY_4 	= 36,
	AEGP_FootageDepth_GRAY_8 	= 40,
	AEGP_FootageDepth_48	 	= 48,
	AEGP_FootageDepth_64	 	= 64,
	AEGP_FootageDepth_GRAY_16	= -16
};


enum {
	AEGP_FilmSizeUnits_NONE = 0,
	AEGP_FilmSizeUnits_HORIZONTAL,
	AEGP_FilmSizeUnits_VERTICAL,
	AEGP_FilmSizeUnits_DIAGONAL
};
typedef A_long AEGP_FilmSizeUnits;


enum {
	AEGP_LightType_NONE = -1,

	AEGP_LightType_PARALLEL,
	AEGP_LightType_SPOT,
	AEGP_LightType_POINT,
	AEGP_LightType_AMBIENT,
	AEGP_LightType_RESERVED1,

	AEGP_LightType_NUM_TYPES
};

typedef A_u_long AEGP_LightType;


enum {
	AEGP_LightFalloff_NONE = 0,
	AEGP_LightFalloff_SMOOTH,
	AEGP_LightFalloff_INVERSE_SQUARE_CLAMPED
};

typedef A_u_long AEGP_LightFalloffType;


/* -------------------------------------------------------------------- */

enum {
	AEGP_TimeDisplayType_TIMECODE = 0,
	AEGP_TimeDisplayType_FRAMES,
	AEGP_TimeDisplayType_FEET_AND_FRAMES
};
typedef A_char AEGP_TimeDisplayType;

#define AEGP_FramesPerFoot_35MM		16
#define AEGP_FramesPerFoot_16MM		40

typedef struct {	// note: unused values are still stored in settings and used when cycling through
					//	the 3 types using cmd/ctrl-click on timecode
	AEGP_TimeDisplayType	time_display_type;
	A_char					timebaseC;			// only used for AEGP_TimeDisplayType_TIMECODE, 1 to 100
	A_Boolean				non_drop_30B;		// only used for AEGP_TimeDisplayType_TIMECODE,
												//	when timebaseC == 30 && item framerate == 29.97, use drop frame or non-drop?
	A_char					frames_per_footC;	// only used for AEGP_TimeDisplayType_FEET_AND_FRAMES
	A_long					starting_frameL;	// usually 0 or 1, not used for AEGP_TimeDisplayType_TIMECODE
	A_Boolean				auto_timecode_baseB;
} AEGP_TimeDisplay2;

enum {
	AEGP_TimeDisplay_TIMECODE = 0,
	AEGP_TimeDisplay_FRAMES
};
typedef char AEGP_TimeDisplayMode;

enum {
	AEGP_SourceTimecode_ZERO	= 0,
	AEGP_SourceTimecode_SOURCE_TIMECODE
};
typedef char AEGP_SourceTimecodeDisplayMode;

enum {
	AEGP_Frames_ZERO_BASED	= 0,
	AEGP_Frames_ONE_BASED,
	AEGP_Frames_TIMECODE_CONVERSION
};
typedef char AEGP_FramesDisplayMode;

typedef struct {
	AEGP_TimeDisplayMode				display_mode;
	AEGP_SourceTimecodeDisplayMode		footage_display_mode;
	A_Boolean							display_dropframeB;
	A_Boolean							use_feet_framesB;
	A_char								timebaseC;
	A_char								frames_per_footC;
	AEGP_FramesDisplayMode				frames_display_mode;
} AEGP_TimeDisplay3;



#define kAEGPProjSuite				"AEGP Proj Suite"
#define kAEGPProjSuiteVersion6		9	/* frozen in AE 10.5 */

typedef struct AEGP_ProjSuite6 {

	SPAPI A_Err	(*AEGP_GetNumProjects)(									/* will always (in 5.0) return 1 if project is open */
		A_long				*num_projPL);				/* << */

	SPAPI A_Err	(*AEGP_GetProjectByIndex)(
		A_long				proj_indexL,				/* >> */
		AEGP_ProjectH		*projPH);					/* << */

	SPAPI A_Err	(*AEGP_GetProjectName)(
		AEGP_ProjectH		projH,						/* >> */
		A_char 				*nameZ);					/* << space for A_char[AEGP_MAX_PROJ_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetProjectPath)(
		AEGP_ProjectH		projH,						/* >> */
		AEGP_MemHandle		*unicode_pathPH);			// << empty string if no file. handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_GetProjectRootFolder)(
		AEGP_ProjectH		projH,						/* >> */
		AEGP_ItemH			*root_folderPH);			/* << */

	SPAPI A_Err (*AEGP_SaveProjectToPath)(
		AEGP_ProjectH		projH,						/* >> */
		const A_UTF16Char	*pathZ);					// >> null terminated unicode path with platform separators

	SPAPI A_Err (*AEGP_GetProjectTimeDisplay)(
		AEGP_ProjectH		projH,						/* >> */
		AEGP_TimeDisplay3	*time_displayP);			/* << */

	SPAPI A_Err (*AEGP_SetProjectTimeDisplay)(							/* UNDOABLE */
		AEGP_ProjectH			projH,					/* <> */
		const AEGP_TimeDisplay3	*time_displayP);		/* >> */

	SPAPI A_Err (*AEGP_ProjectIsDirty)(
		AEGP_ProjectH		projH,						/* >> */
		A_Boolean			*is_dirtyPB);				/* << */

	SPAPI A_Err (*AEGP_SaveProjectAs)(
		AEGP_ProjectH		projH,						/* >> */
		const A_UTF16Char	*pathZ);					// >> null terminated unicode path with platform separators

	SPAPI A_Err (*AEGP_NewProject)(
		AEGP_ProjectH	*new_projectPH);				/* << WARNING: Will close any open projects! */

	// WARNING: Will close any open projects!
	SPAPI A_Err (*AEGP_OpenProjectFromPath)(
		const A_UTF16Char	*pathZ,						// >> null terminated unicode path with platform separators
		AEGP_ProjectH		*projectPH);				/* << */

	SPAPI A_Err	(*AEGP_GetProjectBitDepth)(
		AEGP_ProjectH		projectH,					/* >> */
		AEGP_ProjBitDepth	*bit_depthP);				/* << */

	SPAPI A_Err	(*AEGP_SetProjectBitDepth)(								/* UNDOABLE */
		AEGP_ProjectH		projectH,					/* >> */
		AEGP_ProjBitDepth	bit_depth);					/* >> */

} AEGP_ProjSuite6;

/* -------------------------------------------------------------------- */

enum {
	AEGP_SoundEncoding_UNSIGNED_PCM = 3,
	AEGP_SoundEncoding_SIGNED_PCM,
	AEGP_SoundEncoding_FLOAT,
	AEGP_SoundEncoding_END,
	AEGP_SoundEncoding_BEGIN = AEGP_SoundEncoding_UNSIGNED_PCM
};
typedef A_long AEGP_SoundEncoding;

typedef struct AEGP_SoundDataFormat {
	A_FpLong				sample_rateF;
	AEGP_SoundEncoding		encoding;
	A_long					bytes_per_sampleL;		/* 1, 2, or 4 only (ignored if encoding == AEGP_SoundEncoding_FLOAT) */
	A_long					num_channelsL;			/* 1 for mono, 2 for stereo */
}AEGP_SoundDataFormat;


enum {
	AEGP_ItemType_NONE,
	AEGP_ItemType_FOLDER,
	AEGP_ItemType_COMP,
	AEGP_ItemType_SOLID_defunct,	// as of AE6, solids are now just AEGP_ItemType_FOOTAGE with AEGP_FootageSignature_SOLID
	AEGP_ItemType_FOOTAGE,
	AEGP_ItemType_NUM_TYPES1
};
typedef A_short AEGP_ItemType;


enum {
	AEGP_ItemFlag_MISSING			= 0x1,			/* footage property: here for convenience	*/
	AEGP_ItemFlag_HAS_PROXY			= 0x2,
	AEGP_ItemFlag_USING_PROXY		= 0x4,			/* is using the proxy as source				*/
	AEGP_ItemFlag_MISSING_PROXY		= 0x8,			/* footage property: here for convenience	*/
	AEGP_ItemFlag_HAS_VIDEO			= 0x10,			/* is there a video track?					*/
	AEGP_ItemFlag_HAS_AUDIO			= 0x20,			/* is there an audio track?					*/
	AEGP_ItemFlag_STILL				= 0x40,			/* are all frames exactly the same			*/
	AEGP_ItemFlag_HAS_ACTIVE_AUDIO	= 0x80			/* new in AE CS 5.5: is there an audio track, and is it also turned on right now? */
};
typedef A_long AEGP_ItemFlags;


enum {
	AEGP_Label_NONE = -1,			/* undefined sentinel value	*/
	AEGP_Label_NO_LABEL = 0,
	AEGP_Label_1,
	AEGP_Label_2,
	AEGP_Label_3,
	AEGP_Label_4,
	AEGP_Label_5,
	AEGP_Label_6,
	AEGP_Label_7,
	AEGP_Label_8,
	AEGP_Label_9,
	AEGP_Label_10,
	AEGP_Label_11,
	AEGP_Label_12,
	AEGP_Label_13,
	AEGP_Label_14,
	AEGP_Label_15,
	AEGP_Label_16,			// label 16 is new in AE 10.0 (CS5)

	AEGP_Label_NUMTYPES
};
typedef A_char AEGP_LabelID;

/* -------------------------------------------------------------------- */

enum {
	AEGP_PersistentType_MACHINE_SPECIFIC,
	AEGP_PersistentType_MACHINE_INDEPENDENT,
	AEGP_PersistentType_MACHINE_INDEPENDENT_RENDER,
	AEGP_PersistentType_MACHINE_INDEPENDENT_OUTPUT,
	AEGP_PersistentType_MACHINE_INDEPENDENT_COMPOSITION,
	AEGP_PersistentType_MACHINE_SPECIFIC_TEXT,
	AEGP_PersistentType_MACHINE_SPECIFIC_PAINT,
	AEGP_PersistentType_MACHINE_SPECIFIC_EFFECTS,
	AEGP_PersistentType_MACHINE_SPECIFIC_EXPRESSION_SNIPPETS,
	AEGP_PersistentType_MACHINE_SPECIFIC_SCRIPT_SNIPPETS,

	AEGP_PersistentType_NUMTYPES
};
typedef A_long AEGP_PersistentType;


#define kAEGPItemSuite				"AEGP Item Suite"
#define kAEGPItemSuiteVersion9		14	/* frozen in AE 14.1 */

typedef struct AEGP_ItemSuite9 {

	SPAPI A_Err	(*AEGP_GetFirstProjItem)(
						AEGP_ProjectH		projectH,				/* >> */
						AEGP_ItemH			*itemPH);					/* << */

	SPAPI A_Err	(*AEGP_GetNextProjItem)(
						AEGP_ProjectH		projectH,				/* >> */
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
						AEGP_ItemH			*itemPH);				/* << NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			selectB,				/* >>	allows to select or deselect the item */
						A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
						AEGP_ItemType		item_type,				/* << */
						A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
						AEGP_PluginID			pluginID,				// in
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_MemHandle		*unicode_namePH);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_SetItemName)(								/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> */
						const A_UTF16Char	*nameZ);				/* >> null terminated UTF16 */

	SPAPI A_Err	(*AEGP_GetItemID)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
						A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*parent_folder_itemPH);	/* << */

	SPAPI A_Err	(*AEGP_SetItemParentFolder)(
						AEGP_ItemH			itemH,					/* <> */
						AEGP_ItemH			parent_folder_itemH);	/* >> */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
						AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
						A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
																	/*			Folder			-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
						AEGP_ItemH 			itemH,					/* >>	*/
						A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long	 			*widthPL,				/* << */
						A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
						AEGP_ItemH 			itemH,					/* >> */
						A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
						AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
						const A_UTF16Char	*nameZ,				/* >> null terminated UTF16 */
						AEGP_ItemH			parent_folderH0,		/* >> */
						AEGP_ItemH			*new_folderPH);			/* << allocated and owned by AE */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* UNDOABLE. Use the item's native timespace */
						AEGP_ItemH 			itemH,					/* >> */
						const A_Time		*new_timePT);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemComment)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_MemHandle		*unicode_namePH);		/* << */

	SPAPI A_Err (*AEGP_SetItemComment)(
						AEGP_ItemH			itemH, 					/* >> UNDOABLE */
						const A_UTF16Char	*commentZ);				/* >> */

	SPAPI A_Err (*AEGP_GetItemLabel)(
						AEGP_ItemH			itemH, 					/* >> */
						AEGP_LabelID		*labelP);				/* << */

	SPAPI A_Err (*AEGP_SetItemLabel)(
						AEGP_ItemH			itemH, 					/* >> UNDOABLE */
						AEGP_LabelID		label);					/* >> */

	SPAPI A_Err (*AEGP_GetItemMRUView)(
						AEGP_ItemH			itemH,					// >>
						AEGP_ItemViewP		*mru_viewP);			// <<

} AEGP_ItemSuite9;


/* -------------------------------------------------------------------- */

#define kAEGPItemViewSuite                             "AEGP Item View Suite"
#define kAEGPItemViewSuiteVersion1                  1 /* frozen in AE 13.6 */

typedef struct AEGP_ItemViewSuite1 {
	// Each item view can have its own playback time, if previewing is active, otherwise the current associated item time
        SPAPI A_Err (*AEGP_GetItemViewPlaybackTime)(
		AEGP_ItemViewP 	item_viewP,						/* >> */
		A_Boolean		*is_currently_previewingPB0,	/* >>  whether the time is actually playback time (TRUE),  FALSE is default item current_time */
		A_Time          *curr_timePT );					/* << */
		
} AEGP_ItemViewSuite1;



/* -------------------------------------------------------------------- */

#define kAEGPSoundDataSuite				"AEGP Sound Data Suite"
#define kAEGPSoundDataVersion1			1 /* frozen in AE 5.0 */


typedef struct AEGP_SoundDataSuite1 {
	SPAPI A_Err (*AEGP_NewSoundData)(								/* Must be disposed with DisposeSoundData	*/
					const AEGP_SoundDataFormat*	sound_formatP,
					AEGP_SoundDataH			*new_sound_dataPH);		/* << can return NULL if no audio			*/

	SPAPI A_Err (*AEGP_DisposeSoundData)(
					AEGP_SoundDataH	sound_dataH); 					/* >> */

	SPAPI A_Err (*AEGP_GetSoundDataFormat)(
					AEGP_SoundDataH			soundH,					/* >> */
					AEGP_SoundDataFormat	*sound_formatP);		/* << */


	/*
		If the sound format has two channels, the data is interleaved
		left (0), right(1), left(0), right(1), ...

		AEGP_SoundEncoding_FLOAT has a type of FpShort

		For bytes_per_sample == 1
			AEGP_SoundEncoding_UNSIGNED_PCM == A_u_char
			AEGP_SoundEncoding_SIGNED_PCM	== A_char

		For bytes_per_sample == 2
			AEGP_SoundEncoding_UNSIGNED_PCM == A_u_short
			AEGP_SoundEncoding_SIGNED_PCM	== A_short

		For bytes_per_sample == 4
			AEGP_SoundEncoding_UNSIGNED_PCM == A_u_long
			AEGP_SoundEncoding_SIGNED_PCM	== A_long

		usage:
		void * sound_dataP;
		sds->AEGP_LockSoundDataSamples( soundH, &sound_dataP);
		A_u_long* correct_samples = (A_u_long*)sound_dataP; // for AEGP_SoundEncoding_UNSIGNED_PCM
	*/

	SPAPI A_Err (*AEGP_LockSoundDataSamples)(
					AEGP_SoundDataH			soundH,				/* >> */
					void					**samples);			/* << use the correct combination of unsigned/signed/float and bytes_per_sample to determine type */

	SPAPI A_Err (*AEGP_UnlockSoundDataSamples)(
					AEGP_SoundDataH			soundH);			/* >> */

	SPAPI A_Err (*AEGP_GetNumSamples)(
					AEGP_SoundDataH			soundH,				/* >> */
					A_long					*num_samplesPL);	/* << */

} AEGP_SoundDataSuite1 ;



/* -------------------------------------------------------------------- */


typedef struct {
	A_short	xS, yS;
} AEGP_DownsampleFactor;

enum {
	AEGP_CompFlag_SHOW_ALL_SHY			= 0x1,
	AEGP_CompFlag_RESERVED_1			= 0x2,
	AEGP_CompFlag_RESERVED_2			= 0x4,
	AEGP_CompFlag_ENABLE_MOTION_BLUR	= 0x8,
	AEGP_CompFlag_ENABLE_TIME_FILTER	= 0x10,
	AEGP_CompFlag_GRID_TO_FRAMES		= 0x20,
	AEGP_CompFlag_GRID_TO_FIELDS		= 0x40,
	AEGP_CompFlag_USE_LOCAL_DSF			= 0x80,		// If on, use the dsf in the comp, not the RO
	AEGP_CompFlag_DRAFT_3D				= 0x100,
	AEGP_CompFlag_SHOW_GRAPH			= 0x200,
	AEGP_CompFlag_RESERVED_3			= 0x400

};
typedef A_long	AEGP_CompFlags;




#define kAEGPCompSuite				"AEGP Comp Suite"
#define kAEGPCompSuiteVersion11		25 /* frozen in AE 14.2 */

typedef struct AEGP_CompSuite11 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
						AEGP_CompH				compH,				/* >> */
						AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
						AEGP_CompH					compH,			/* <> */
						const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err (*AEGP_SetCompBGColor)(
						AEGP_CompH				compH,				/* >> */
						const AEGP_ColorVal		*bg_colorP);		/* >> */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompFlags		*comp_flagsP);			/* << */

	/*Opens the comp*/
	SPAPI A_Err	(*AEGP_GetShowLayerNameOrSourceName)(
						AEGP_CompH 				compH,					/* >> */
						A_Boolean				*layer_names_shownPB);	/* << true if layer names, false if source names */

	/*Opens the comp*/
	SPAPI A_Err	(*AEGP_SetShowLayerNameOrSourceName)(
						AEGP_CompH 				compH,					/* >> */
						A_Boolean				show_layer_namesB);		/* >> true to show layer names, false to show source names */
	
	/*Opens the comp*/
	SPAPI A_Err	(*AEGP_GetShowBlendModes)(
						AEGP_CompH 				compH,					/* >> */
						A_Boolean				*blend_modes_shownPB);	/* << */

	/*Opens the comp*/
	SPAPI A_Err	(*AEGP_SetShowBlendModes)(
						AEGP_CompH 				compH,					/* >> */
						A_Boolean				show_blend_modesB);		/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
						AEGP_CompH			compH,					/* >> */
						A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err (*AEGP_SetCompFrameRate)(
						AEGP_CompH			compH,					/* >> */
						const A_FpLong		*fpsPF);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
						AEGP_CompH			compH,					/* >> */
						A_Ratio				*angle,					/* << */
						A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*comp_timeP,			/* >> */
						A_Time				*start,					/* << */
						A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompSuggestedMotionBlurSamples)(
						AEGP_CompH			compH,					/* >> */
						A_long				*samplesPL);			/* << */

	SPAPI A_Err	(*AEGP_SetCompSuggestedMotionBlurSamples)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						A_long				samplesL);				/* >> */
						
	SPAPI A_Err	(*AEGP_GetCompMotionBlurAdaptiveSampleLimit)(
						AEGP_CompH			compH,					/* >> */
						A_long				*samplesPL);			/* << */

	SPAPI A_Err	(*AEGP_SetCompMotionBlurAdaptiveSampleLimit)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						A_long				samplesL);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						const A_Time 		*work_area_startPT,		/* >> */
						const A_Time 		*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const AEGP_ColorVal	*color,					/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
						AEGP_ItemH			parent_folderH0,		/* >> */
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL,				/* >> */
						const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
						const A_Time		*durationPT,			/* >> */
						const A_Ratio		*frameratePRt,			/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
						AEGP_PluginID		plugin_id,				/* >> */
						AEGP_CompH			compH,					/* >> */
						AEGP_Collection2H	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
						AEGP_CompH			compH,					/* >> */
						AEGP_Collection2H	collectionH);			/* >> not adopted */

	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
						AEGP_CompH			compH,					/* >> */
						A_Time				*start_timePT);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
						AEGP_CompH			compH,					/* >> */
						const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*durationPT);			/* >> */

	SPAPI A_Err (*AEGP_CreateNullInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_null_solidPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
						AEGP_CompH			compH,					/* >> */
						const A_Ratio		*pix_aspectratioPRt);	/* >> */

	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						A_Boolean			select_new_layerB,		/* >> */
						AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_CreateBoxTextLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						A_Boolean			select_new_layerB,		/* >> */
						A_FloatPoint		box_dimensions,			/* >> */ // (width and height)
						AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompDimensions)(
						AEGP_CompH			compH,					/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL);				/* >> */

	SPAPI A_Err (*AEGP_DuplicateComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFrameDuration)(
						AEGP_CompH			compH,    				/* >> */
						A_Time				*timeP);				/* << */

	SPAPI A_Err (*AEGP_GetMostRecentlyUsedComp)(
						AEGP_CompH			*compPH);				/* << If compPH returns NULL, there's no available comp */

	SPAPI A_Err (*AEGP_CreateVectorLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_vector_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetNewCompMarkerStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_StreamRefH		*streamPH);				/* << must be disposed by caller */

	SPAPI A_Err (*AEGP_GetCompDisplayDropFrame)(
						AEGP_CompH			compH,					/* >> */
						A_Boolean			*dropFramePB);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayDropFrame)(
						AEGP_CompH			compH,					/* >> */
						A_Boolean			dropFrameB);			/* << */
	
	SPAPI A_Err (*AEGP_ReorderCompSelection)(
						AEGP_CompH			compH,					/* >> */
						A_long				index);					/* >> */

} AEGP_CompSuite11;


/* -------------------------------------------------------------------- */


#define kAEGPMemorySuite				"AEGP Memory Suite"
#define kAEGPMemorySuiteVersion1		1 /* frozen in AE 5.0 */

enum {
	AEGP_MemFlag_NONE 	= 0x0,
	AEGP_MemFlag_CLEAR 	= 0x01,
	AEGP_MemFlag_QUIET 	= 0x02
};
typedef A_long	AEGP_MemFlag;

typedef A_u_long	AEGP_MemSize;


typedef struct AEGP_MemorySuite1 {


	SPAPI A_Err	 (*AEGP_NewMemHandle)( AEGP_PluginID	plugin_id,			/* >> */
									   const A_char	*whatZ,					/* >> */
									   AEGP_MemSize	size,					/* >> */
									   AEGP_MemFlag	flags,					/* >> */
									   AEGP_MemHandle	*memPH);			/* << */

	SPAPI A_Err	 (*AEGP_FreeMemHandle)(		AEGP_MemHandle memH);

	SPAPI A_Err	 (*AEGP_LockMemHandle)(		AEGP_MemHandle memH,			/* nestable */
											void			**ptr_to_ptr);	/* << */

	SPAPI A_Err	 (*AEGP_UnlockMemHandle)(	AEGP_MemHandle memH);

	SPAPI A_Err	 (*AEGP_GetMemHandleSize)(	AEGP_MemHandle	memH,
											AEGP_MemSize	*sizeP);		/* << */

	SPAPI A_Err	 (*AEGP_ResizeMemHandle)(	const A_char		*whatZ,			/* >> */
											AEGP_MemSize	new_size,		/* >> */
											AEGP_MemHandle	memH);			/* <> */

	SPAPI A_Err	 (*AEGP_SetMemReportingOn)(	A_Boolean 		turn_OnB);

	SPAPI A_Err	 (*AEGP_GetMemStats)(	AEGP_PluginID	plugin_id,			/* >> */
										A_long			*countPL,			/* << */
										A_long			*sizePL);			/* << */
} AEGP_MemorySuite1;

/* -------------------------------------------------------------------- */


enum {
	AEGP_TransferFlag_PRESERVE_ALPHA		= 0x1,
	AEGP_TransferFlag_RANDOMIZE_DISSOLVE	= 0x2
};
typedef A_long	AEGP_TransferFlags;

enum {
	AEGP_TrackMatte_NO_TRACK_MATTE,
	AEGP_TrackMatte_ALPHA,
	AEGP_TrackMatte_NOT_ALPHA,
	AEGP_TrackMatte_LUMA,
	AEGP_TrackMatte_NOT_LUMA
};
typedef A_long	AEGP_TrackMatte;

typedef struct {
	PF_TransferMode			mode;				// defined in AE_EffectCB.h
	AEGP_TransferFlags		flags;
	AEGP_TrackMatte			track_matte;
} AEGP_LayerTransferMode;



enum {
	AEGP_LayerQual_NONE = -1,					// sentinel
	AEGP_LayerQual_WIREFRAME,					// wire-frames only
	AEGP_LayerQual_DRAFT,						// LO-qual filters, LO-qual geom
	AEGP_LayerQual_BEST							// HI-qual filters, HI-qual geom
};
typedef A_short AEGP_LayerQuality;


enum {
	AEGP_LayerSamplingQual_BILINEAR,						// bilinear interpolation
	AEGP_LayerSamplingQual_BICUBIC							// bicubic interpolation
};
typedef A_short AEGP_LayerSamplingQuality;

enum {
	AEGP_LayerFlag_NONE				 			= 0x00000000,
	AEGP_LayerFlag_VIDEO_ACTIVE		 			= 0x00000001,
	AEGP_LayerFlag_AUDIO_ACTIVE 				= 0x00000002,
	AEGP_LayerFlag_EFFECTS_ACTIVE	 			= 0x00000004,
	AEGP_LayerFlag_MOTION_BLUR 					= 0x00000008,
	AEGP_LayerFlag_FRAME_BLENDING	 			= 0x00000010,
	AEGP_LayerFlag_LOCKED 						= 0x00000020,
	AEGP_LayerFlag_SHY 							= 0x00000040,
	AEGP_LayerFlag_COLLAPSE						= 0x00000080,
	AEGP_LayerFlag_AUTO_ORIENT_ROTATION			= 0x00000100,
	AEGP_LayerFlag_ADJUSTMENT_LAYER				= 0x00000200,
	AEGP_LayerFlag_TIME_REMAPPING				= 0x00000400,
  	AEGP_LayerFlag_LAYER_IS_3D					= 0x00000800,
  	AEGP_LayerFlag_LOOK_AT_CAMERA				= 0x00001000,
  	AEGP_LayerFlag_LOOK_AT_POI					= 0x00002000,
	AEGP_LayerFlag_SOLO							= 0x00004000,
	AEGP_LayerFlag_MARKERS_LOCKED				= 0x00008000,
	AEGP_LayerFlag_NULL_LAYER					= 0x00010000,
	AEGP_LayerFlag_HIDE_LOCKED_MASKS			= 0x00020000,
	AEGP_LayerFlag_GUIDE_LAYER					= 0x00040000,
	AEGP_LayerFlag_ADVANCED_FRAME_BLENDING		= 0x00080000,
	AEGP_LayerFlag_SUBLAYERS_RENDER_SEPARATELY	= 0x00100000,
	AEGP_LayerFlag_ENVIRONMENT_LAYER			= 0x00200000
};
typedef A_long	AEGP_LayerFlags;


// Layers are always one of the following types.

enum {
	AEGP_ObjectType_NONE = -1,
	AEGP_ObjectType_AV,			/* Includes all pre-AE 5.0 layer types (audio or video source, including adjustment layers)	*/
	AEGP_ObjectType_LIGHT,
	AEGP_ObjectType_CAMERA,
	AEGP_ObjectType_TEXT,
	AEGP_ObjectType_VECTOR,
	AEGP_ObjectType_RESERVED1,
	AEGP_ObjectType_RESERVED2,
	AEGP_ObjectType_RESERVED3,
	AEGP_ObjectType_RESERVED4,
	AEGP_ObjectType_RESERVED5,
	AEGP_ObjectType_RESERVED6,
	AEGP_ObjectType_NUM_TYPES
};
typedef A_long AEGP_ObjectType;

enum {
	AEGP_LTimeMode_LayerTime,
	AEGP_LTimeMode_CompTime
};
typedef A_short				AEGP_LTimeMode;

#define	AEGP_REORDER_LAYER_TO_END	-1

#define kAEGPLayerSuite				"AEGP Layer Suite"

#define kAEGPLayerSuiteVersion9		15	/* frozen AE 23.0 */

typedef struct AEGP_LayerSuite9 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
						AEGP_CompH			compH,					/* >> */
						A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
						AEGP_CompH			compH,					/* >> */
						A_long				layer_indexL,			/* >> */
						AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
						AEGP_LayerH			*layerPH);				/* << returns non null only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err (*AEGP_GetLayerSourceItemID)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*source_item_idPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
						AEGP_PluginID			pluginID,				// in
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_MemHandle		*utf_layer_namePH0,		// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
						AEGP_MemHandle		*utf_source_namePH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		single_flag,			/* >> */
						A_Boolean			valueB);				/* >> */

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time 		*in_pointPT,			/* >> */
						const A_Time 		*durationPT);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Time 		*offsetPT);				/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Ratio 		*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
						AEGP_LayerH 			layerH,				/* >> */
						AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(								/* UNDOABLE */
						AEGP_LayerH 					layerH,				/* >> */
						const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
						AEGP_ItemH 			item_to_addH,			/* >> */
						AEGP_CompH 			into_compH,				/* >> */
						A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
						AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
						AEGP_CompH 			into_compH,				/* >> */
						AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
						AEGP_LayerH 		layerH,			/* >> */
						AEGP_LTimeMode		time_mode,		/* >> */
						const A_Time	 	*timePT,		/* >> */
						A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_LTimeMode		time_mode,			/* >> */
						const A_Time	 	*timePT,			/* >> */
						A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			fill_must_be_activeB,	/* >> */
						A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_Time			*layer_timePT);					/* << */

	SPAPI A_Err	(*AEGP_ConvertLayerToCompTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*layer_timePT,					/* >> */
					A_Time			*comp_timePT) ;					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
					AEGP_LayerH				layerH,				/* >> */
					AEGP_LayerIDVal			*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*transform);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*view_timeP,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*transform);		/* << */

	SPAPI A_Err (*AEGP_SetLayerName)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_UTF16Char		*new_nameZ);				/* >> null terminated UTF16 */

	SPAPI A_Err	(*AEGP_GetLayerParent)(
				const AEGP_LayerH		layerH,				/* >> */
				AEGP_LayerH				*parent_layerPH);	/* << NULL if no parent */

	SPAPI A_Err (*AEGP_SetLayerParent)(
				AEGP_LayerH				layerH,				/* >> */
				const AEGP_LayerH		parent_layerH0);	/* >> */

	SPAPI A_Err (*AEGP_DeleteLayer)(
				AEGP_LayerH				layerH);			/* >>  	UNDOABLE */

	SPAPI A_Err (*AEGP_DuplicateLayer)(
				AEGP_LayerH				orig_layerH,			/* >> */
				AEGP_LayerH				*duplicate_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetLayerFromLayerID)(
				AEGP_CompH				parent_compH,			/* >> */
				AEGP_LayerIDVal			id,						/* >> */
				AEGP_LayerH				*layerPH);				/* << */

	SPAPI A_Err (*AEGP_GetLayerLabel)(
				AEGP_LayerH				layerH,					/* >> */
				AEGP_LabelID			*labelP);				/* << */

	SPAPI A_Err (*AEGP_SetLayerLabel)(		/* UNDOABLE */
				AEGP_LayerH				layerH,					/* >> */
				AEGP_LabelID			label);					/* >> */

	SPAPI A_Err	(*AEGP_GetLayerSamplingQuality)(
				AEGP_LayerH					layerH,					/* >> */
				AEGP_LayerSamplingQuality	*qualityP);				/* << */

	/* Option is explicitly set on the layer independent of layer quality. 
	If you want to force it on you must also set the layer quality to 
	AEGP_LayerQual_BEST with AEGP_SetLayerQuality. Otherwise it will only be using
	the specified layer sampling quality whenever the layer quality is set to AEGP_LayerQual_BEST*/
	SPAPI A_Err	(*AEGP_SetLayerSamplingQuality)(			/* UNDOABLE */
				AEGP_LayerH					layerH,					/* >> */
				AEGP_LayerSamplingQuality	quality);				/* >> */

	SPAPI A_Err (*AEGP_GetTrackMatteLayer)(
				const AEGP_LayerH		layerH,					/* >> */
				AEGP_LayerH				*track_matte_layerPH);	/* << NULL if no track matte layer */

	SPAPI A_Err (*AEGP_SetTrackMatte)(
				AEGP_LayerH				layerH,					/* >> */
				const AEGP_LayerH		track_matte_layerH0,	/* >> */
				const AEGP_TrackMatte	track_matte_type);		/* >> */

	SPAPI A_Err (*AEGP_RemoveTrackMatte)(
				AEGP_LayerH				layerH);				/* >> */

} AEGP_LayerSuite9;





/* -------------------------------------------------------------------- */


enum {
	/*
	 	WARNING!!
	 	don't reorder these items, don't add in the middle
	 	these IDs are saved to disk!
	 	
	 	only ever add to the end of the list, right before AEGP_LayerStream_NUMTYPES
	 */
	
	AEGP_LayerStream_NONE = -1,
	AEGP_LayerStream_ANCHORPOINT,
	AEGP_LayerStream_POSITION,
	AEGP_LayerStream_SCALE,
	AEGP_LayerStream_ROTATION,
	AEGP_LayerStream_ROTATE_Z = AEGP_LayerStream_ROTATION, 		/* for 3D streams */
	AEGP_LayerStream_OPACITY,
	AEGP_LayerStream_AUDIO,
	AEGP_LayerStream_MARKER,
	AEGP_LayerStream_TIME_REMAP,
	AEGP_LayerStream_ROTATE_X,
	AEGP_LayerStream_ROTATE_Y,
	AEGP_LayerStream_ORIENTATION,

	// only valid for AEGP_ObjectType == AEGP_ObjectType_CAMERA
	AEGP_LayerStream_ZOOM,
	AEGP_LayerStream_DEPTH_OF_FIELD,
	AEGP_LayerStream_FOCUS_DISTANCE,
	AEGP_LayerStream_APERTURE,
	AEGP_LayerStream_BLUR_LEVEL,

	// only valid for AEGP_ObjectType == AEGP_ObjectType_LIGHT
	AEGP_LayerStream_INTENSITY,
	AEGP_LayerStream_COLOR,
	AEGP_LayerStream_CONE_ANGLE,
	AEGP_LayerStream_CONE_FEATHER,
	AEGP_LayerStream_SHADOW_DARKNESS,
	AEGP_LayerStream_SHADOW_DIFFUSION,

	// only valid for AEGP_ObjectType == AEGP_ObjectType_AV
	AEGP_LayerStream_ACCEPTS_SHADOWS,
	AEGP_LayerStream_ACCEPTS_LIGHTS,
	AEGP_LayerStream_AMBIENT_COEFF,
	AEGP_LayerStream_DIFFUSE_COEFF,
	AEGP_LayerStream_SPECULAR_INTENSITY,
	AEGP_LayerStream_SPECULAR_SHININESS,

	AEGP_LayerStream_CASTS_SHADOWS, 		// LIGHT, and AV only, no CAMERA
	AEGP_LayerStream_LIGHT_TRANSMISSION,	// AV Layer only
	AEGP_LayerStream_METAL,			 		// AV layer only

	AEGP_LayerStream_SOURCE_TEXT,

	// only valid for AEGP_ObjectType == AEGP_ObjectType_CAMERA
	AEGP_LayerStream_IRIS_SHAPE,
	AEGP_LayerStream_IRIS_ROTATION,
	AEGP_LayerStream_IRIS_ROUNDNESS,
	AEGP_LayerStream_IRIS_ASPECT_RATIO,
	AEGP_LayerStream_IRIS_DIFFRACTION_FRINGE,
	AEGP_LayerStream_IRIS_HIGHLIGHT_GAIN,
	AEGP_LayerStream_IRIS_HIGHLIGHT_THRESHOLD,
	AEGP_LayerStream_IRIS_HIGHLIGHT_SATURATION,

	// only valid for AEGP_ObjectType == AEGP_ObjectType_LIGHT
	AEGP_LayerStream_LIGHT_FALLOFF_TYPE,
	AEGP_LayerStream_LIGHT_FALLOFF_START,
	AEGP_LayerStream_LIGHT_FALLOFF_DISTANCE,

	// only valid for AEGP_ObjectType == AEGP_ObjectType_AV
	AEGP_LayerStream_REFLECTION_INTENSITY,
	AEGP_LayerStream_REFLECTION_SHARPNESS,
	AEGP_LayerStream_REFLECTION_ROLLOFF,
	AEGP_LayerStream_TRANSPARENCY_COEFF,
	AEGP_LayerStream_TRANSPARENCY_ROLLOFF,
	AEGP_LayerStream_INDEX_OF_REFRACTION,
	
	AEGP_LayerStream_EXTRUSION_BEVEL_STYLE,
	AEGP_LayerStream_EXTRUSION_BEVEL_DIRECTION,
	AEGP_LayerStream_EXTRUSION_BEVEL_DEPTH,
	AEGP_LayerStream_EXTRUSION_HOLE_BEVEL_DEPTH,
	AEGP_LayerStream_EXTRUSION_DEPTH,
	AEGP_LayerStream_PLANE_CURVATURE,
	AEGP_LayerStream_PLANE_SUBDIVISION,

	AEGP_LayerStream_LIGHT_BACKGROUND_VISIBLE,
	AEGP_LayerStream_LIGHT_BACKGROUND_OPACITY,
	AEGP_LayerStream_LIGHT_BACKGROUND_BLUR,

	/*********************************************************/
	AEGP_LayerStream_NUMTYPES,
	AEGP_LayerStream_BEGIN	= AEGP_LayerStream_NONE + 1,
	AEGP_LayerStream_END	= AEGP_LayerStream_NUMTYPES

};
typedef A_long				AEGP_LayerStream;

enum {
	AEGP_MaskStream_OUTLINE = 400,
	AEGP_MaskStream_OPACITY,
	AEGP_MaskStream_FEATHER,
	AEGP_MaskStream_EXPANSION,

	// useful for iteration
	AEGP_MaskStream_BEGIN = AEGP_MaskStream_OUTLINE,
	AEGP_MaskStream_END =	AEGP_MaskStream_EXPANSION+1
};
typedef A_long				AEGP_MaskStream;


enum {
	AEGP_StreamFlag_NONE			= 0,
	AEGP_StreamFlag_HAS_MIN			= 0x01,
	AEGP_StreamFlag_HAS_MAX			= 0x02,
	AEGP_StreamFlag_IS_SPATIAL		= 0x04
};
typedef A_long			AEGP_StreamFlags;


typedef A_FpLong		AEGP_OneDVal;

typedef struct {
	A_FpLong	x,y;
} AEGP_TwoDVal;			// if audio, rt is x, left is y

typedef struct {
	A_FpLong	x,y,z;
} AEGP_ThreeDVal;

typedef A_FpLong		AEGP_FourDVal[4];

typedef A_Handle		AEGP_ArbBlockVal;

enum {
	AEGP_KeyInterp_NONE = 0,
	AEGP_KeyInterp_LINEAR,
	AEGP_KeyInterp_BEZIER,
	AEGP_KeyInterp_HOLD,

	AEGP_Interp_NUM_VALUES
};
typedef A_long	AEGP_KeyframeInterpolationType;

/* AEGP_KeyInterpMask -- allowed interpolation mask constants and type
 */
enum {
	AEGP_KeyInterpMask_NONE				= 0,
	AEGP_KeyInterpMask_LINEAR			= 1 << 0,
	AEGP_KeyInterpMask_BEZIER			= 1 << 1,
	AEGP_KeyInterpMask_HOLD				= 1 << 2,
	AEGP_KeyInterpMask_CUSTOM			= 1 << 3,

	AEGP_KeyInterpMask_ANY				= 0xFFFF
};
typedef A_long	AEGP_KeyInterpolationMask;

typedef struct {
	A_FpLong	speedF;
	A_FpLong	influenceF;
} AEGP_KeyframeEase;


typedef union {
	AEGP_FourDVal			four_d;
	AEGP_ThreeDVal			three_d;
	AEGP_TwoDVal			two_d;
	AEGP_OneDVal			one_d;
	AEGP_ColorVal			color;
	AEGP_ArbBlockVal		arbH;
	AEGP_MarkerValP			markerP;
	AEGP_LayerIDVal			layer_id;
	AEGP_MaskIDVal			mask_id;
	AEGP_MaskOutlineValH	mask;
	AEGP_TextDocumentH		text_documentH;
} AEGP_StreamVal2;

typedef struct {
	AEGP_StreamRefH 	streamH;
	AEGP_StreamVal2		val;
} AEGP_StreamValue2;

enum {
	AEGP_StreamType_NO_DATA,
	AEGP_StreamType_ThreeD_SPATIAL,
	AEGP_StreamType_ThreeD,
	AEGP_StreamType_TwoD_SPATIAL,
	AEGP_StreamType_TwoD,
	AEGP_StreamType_OneD,
	AEGP_StreamType_COLOR,
	AEGP_StreamType_ARB,
	AEGP_StreamType_MARKER,
	AEGP_StreamType_LAYER_ID,
	AEGP_StreamType_MASK_ID,
	AEGP_StreamType_MASK,
	AEGP_StreamType_TEXT_DOCUMENT
};
typedef A_long				AEGP_StreamType;


#define kAEGPStreamSuite				"AEGP Stream Suite"
#define kAEGPStreamSuiteVersion6		11 /* frozen in AE 22.5 */

typedef struct AEGP_StreamSuite6 {
	// the only diff from this vs. last rev the new AEGP_GetUniqueStreamID function, which given an AEGP_StreamRef will
	// return the corresponding unique identifier for it.

	SPAPI A_Err (*AEGP_IsStreamLegal)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						A_Boolean*			is_legalP);				/* << */


	SPAPI A_Err (*AEGP_CanVaryOverTime)(
						AEGP_StreamRefH streamH,					/* >> */
						A_Boolean* can_varyPB);						/* << */

	SPAPI A_Err (*AEGP_GetValidInterpolations)(
						AEGP_StreamRefH				streamH,					/* >> */
						AEGP_KeyInterpolationMask*	valid_interpolationsP);		/* << */

	SPAPI A_Err	(*AEGP_GetNewLayerStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetEffectNumParamStreams)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						A_long				*num_paramsPL);			/* << */

	SPAPI A_Err	(*AEGP_GetNewEffectStreamByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
						AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewMaskStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_MaskRefH		mask_refH,				/* >> */
						AEGP_MaskStream		which_stream,			/* >> */
						AEGP_StreamRefH 	*mask_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_DisposeStream)(
						AEGP_StreamRefH 	streamH);				/* >> */

	SPAPI A_Err	(*AEGP_GetStreamName)(
						AEGP_PluginID			pluginID,				// in
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			force_englishB,			/* >> */
						AEGP_MemHandle		*utf_stream_namePH);		// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_GetStreamUnitsText)(
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			force_englishB,			/* >> */
						A_char				*unitsZ);				/* << space for A_char[AEGP_MAX_STREAM_UNITS_SIZE] */

	SPAPI A_Err	(*AEGP_GetStreamProperties)(
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamFlags 	*flagsP,				/* << */
						A_FpLong 			*minP0,					/* << */
						A_FpLong 			*maxP0);				/* << */

	SPAPI A_Err	(*AEGP_IsStreamTimevarying)(						/* takes expressions into account */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			*is_timevaryingPB);		/* << */

	SPAPI A_Err	(*AEGP_GetStreamType)(
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamType		*stream_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetNewStreamValue)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time		*timePT,				/* >> */
						A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
						AEGP_StreamValue2	*valueP);				/* << must be disposed */

	SPAPI A_Err	(*AEGP_DisposeStreamValue)(
						AEGP_StreamValue2	*valueP);				/* <> */


	SPAPI A_Err	(*AEGP_SetStreamValue)(								// only legal to call when AEGP_GetStreamNumKFs==0 or NO_DATA
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamValue2	*valueP);				/* << */

	// this is only valid on streams with primitive types. It is illegal on
	// AEGP_ArbBlockVal || AEGP_MarkerValP || AEGP_MaskOutlineValH

	SPAPI A_Err	(*AEGP_GetLayerStreamValue)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time		*timePT,				/* >> */
						A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
						AEGP_StreamVal2		*stream_valP,			/* << */
						AEGP_StreamType 	*stream_typeP0);		/* << */

	SPAPI A_Err	(*AEGP_GetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			*enabledPB);			/* >> */

	SPAPI A_Err	(*AEGP_SetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			enabledB);				/* >> */

	SPAPI A_Err(*AEGP_GetExpression)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_MemHandle		*unicodeHZ);			/* << must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err(*AEGP_SetExpression)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						const A_UTF16Char*	expressionP);			/* >> not adopted */

	SPAPI A_Err (*AEGP_DuplicateStreamRef)(					// must dispose yourself
						AEGP_PluginID		aegp_plugin_id,			// in
						AEGP_StreamRefH		streamH,				// in
						AEGP_StreamRefH		*dup_streamPH);			// out
    
	SPAPI A_Err (*AEGP_GetUniqueStreamID)(
						AEGP_StreamRefH		streamH,				// in
						int32_t				*outID);				// out
    
} AEGP_StreamSuite6;


/* -------------------------------------------------------------------- */
/*	new Dynamic Streams API for AE6

	Used to access new tracker, text, & paint streams. Eventually
	will work for all streams & groups on a layer.

*/

#define AEGP_MAX_STREAM_MATCH_NAME_SIZE		40


enum {
	AEGP_StreamGroupingType_NONE = -1,
	AEGP_StreamGroupingType_LEAF,
	AEGP_StreamGroupingType_NAMED_GROUP,
	AEGP_StreamGroupingType_INDEXED_GROUP
};
typedef A_long			AEGP_StreamGroupingType;

enum {
	AEGP_DynStreamFlag_ACTIVE_EYEBALL				= 1L << 0,	// read/write
	AEGP_DynStreamFlag_HIDDEN						= 1L << 1,	// read/write, shown in UI at the moment?
	AEGP_DynStreamFlag_DISABLED						= 1L << 4,	// read-only, greyed out in UI?
	AEGP_DynStreamFlag_ELIDED						= 1L << 5,	// read-only, user never sees, children still seen and not indented
	AEGP_DynStreamFlag_SHOWN_WHEN_EMPTY				= 1L << 10,	// read-only, should this group be shown when empty?
	AEGP_DynStreamFlag_SKIP_REVEAL_WHEN_UNHIDDEN	= 1L << 16	// read-only, do not auto reveal this property when un-hidden
};
typedef A_u_long		AEGP_DynStreamFlags;

//	Here are some matchnames for use with
//	AEGP_GetNewStreamRefByMatchname().

#define	AEGP_StreamGroupName_MASK_PARADE		"ADBE Mask Parade"
#define	AEGP_StreamGroupName_MASK_ATOM			"ADBE Mask Atom"
#define	AEGP_StreamName_MASK_FEATHER			"ADBE Mask Feather"
#define	AEGP_StreamName_MASK_OPACITY			"ADBE Mask Opacity"
#define AEGP_StreamName_MASK_OFFSET				"ADBE Mask Offset"
#define AEGP_StreamGroupName_EFFECT_PARADE		"ADBE Effect Parade"
#define AEGP_StreamGroupName_LAYER				"ADBE Abstract Layer"
#define AEGP_StreamGroupName_AV_LAYER			"ADBE AV Layer"
#define AEGP_StreamGroupName_TEXT_LAYER			"ADBE Text Layer"
#define AEGP_StreamGroupName_CAMERA_LAYER		"ADBE Camera Layer"
#define AEGP_StreamGroupName_LIGHT_LAYER		"ADBE Light Layer"
#define AEGP_StreamGroupName_AUDIO				"ADBE Audio Group"
#define AEGP_StreamGroupName_MATERIAL_OPTIONS	"ADBE Material Options Group"
#define AEGP_StreamGroupName_TRANSFORM			"ADBE Transform Group"
#define AEGP_StreamGroupName_LIGHT_OPTIONS		"ADBE Light Options Group"
#define AEGP_StreamGroupName_CAMERA_OPTIONS		"ADBE Camera Options Group"

#define kAEGPDynamicStreamSuite				"AEGP Dynamic Stream Suite"
#define kAEGPDynamicStreamSuiteVersion4		5	/* frozen in AE 9.0 */

typedef struct AEGP_DynamicStreamSuite4 {

	SPAPI A_Err	(*AEGP_GetNewStreamRefForLayer)(						// used to start recursive walk of layer,
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_LayerH				layerH,					/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewStreamRefForMask)(						// used to start recursive walk of layer,
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_MaskRefH			maskH,					/* >> */
		AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */


	SPAPI A_Err (*AEGP_GetStreamDepth)(									// layer is depth 0
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*depthPL);				/* << */


	SPAPI A_Err	(*AEGP_GetStreamGroupingType)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_StreamGroupingType	*group_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetNumStreamsInGroup)(	// error on leaf
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*num_streamsPL);		/* << */


	SPAPI A_Err	(*AEGP_GetDynamicStreamFlags)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_DynStreamFlags		*stream_flagsP);		/* << */

	SPAPI A_Err	(*AEGP_SetDynamicStreamFlag)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_DynStreamFlags		one_flag,				/* >> */
						A_Boolean				undoableB,				/* true if you want this to be an undoable change */
																		/* if false, the only legal flag is AEGP_DynStreamFlag_HIDDEN */
						A_Boolean				setB);					/* >> */


	SPAPI A_Err	(*AEGP_GetNewStreamRefByIndex)(		// legal for namedgroup, indexedgroup, not leaf
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			parent_groupH,			/* >> */
						A_long					indexL,					/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewStreamRefByMatchname)(	// legal for namedgroup
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			parent_groupH,			/* >> */
						const A_char			*utf8_match_nameZ,		/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_DeleteStream)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH);			/* >> */ // must still dispose the streamref later

	SPAPI A_Err	(*AEGP_ReorderStream)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH,			/* <> updated to refer to newly ordered stream */
						A_long 					new_indexL);		/* >> */

	SPAPI A_Err (*AEGP_DuplicateStream)(	/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*new_indexPL0);			/* << */

	/* GetStreamName is in main kAEGPStreamSuite, and works on dynamic streams including groups */

	SPAPI A_Err	(*AEGP_SetStreamName)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH,				/* >> */
						const A_UTF16Char		*nameZ);				/* >> null terminated UTF16 */

	SPAPI A_Err (*AEGP_CanAddStream)(
						AEGP_StreamRefH			group_streamH,			/* >> */
						const A_char			*utf8_match_nameZ,		/* >> make note: this is now defined as a UTF8 string! */
						A_Boolean				*can_addPB);			/* << */

	SPAPI A_Err (*AEGP_AddStream)(				/* UNDOABLE, only valid for AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			indexed_group_streamH,	/* >> */
						const A_char			*utf8_match_nameZ,		/* >> make note: this is now defined as a UTF8 string! */
						AEGP_StreamRefH			*streamPH0);			/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetMatchName)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_char					*utf8_match_nameZ);		/* << UTF8!! use A_char[AEGP_MAX_STREAM_MATCH_NAME_SIZE] for buffer */

	SPAPI A_Err (*AEGP_GetNewParentStreamRef)(
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_StreamRefH			*parent_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetStreamIsModified)(	// i.e. changed from defaults, like the UU key
						AEGP_StreamRefH			streamH,				/* >> */
						A_Boolean				*modifiedPB);			/* << */

	SPAPI A_Err (*AEGP_GetStreamIndexInParent)(	// only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP
						AEGP_StreamRefH			streamH,				/* >> */
						A_long 					*indexPL);				/* << */

	// Valid on leaf streams only. Returns true if this stream is a multidimensional stream
	// that can have its dimensions separated, though they may not be currently separated.
	// Terminology: A Leader is the stream that can be separated, a Follower is one
	// of N automatically streams that correspond to the N dimensions of the Leader.
	// A Leader isn't always separated, call AEGP_AreDimensionsSeparated to find out if it is.
	// As of 8/20/2007, the only stream that is ever separarated is the layer's Position property.
	// Please *do not* write code assuming that, we anticipate allowing separation of more streams in the future.
	SPAPI A_Err (*AEGP_IsSeparationLeader)(
						AEGP_StreamRefH			streamH,			/* >> */
						A_Boolean 				*leaderPB);			/* << */

	// Valid on leaf streams only. Returns true if AEGP_IsSeparationLeader() is true and the
	// dimensions are currently separated. If this is the case, this stream will have
	// AEGP_DynStreamFlag_HIDDEN set, and AEGP_GetSeparationFollower() will help you find the individual
	// dimension streams. However, some simple APIs can continue to be used on this leader, and
	// they will simply automatically propagate to the follower, including:
	//
	//		AEGP_GetNewStreamValue
	//		AEGP_SetStreamValue
	//		AEGP_GetLayerStreamValue
	//
	// Methods such as AEGP_GetNewKeyframeValue that work on keyframe indices will most definitely *not* work on the Leader property,
	// you will need to retrieve and operate on the Followers explicitly.
	SPAPI A_Err (*AEGP_AreDimensionsSeparated)(
						AEGP_StreamRefH		leader_streamH,			/* >> */
						A_Boolean 			*separatedPB);			/* << */

	// Valid only if AEGP_IsSeparationLeader() is true.
	SPAPI A_Err (*AEGP_SetDimensionsSeparated)(
						AEGP_StreamRefH		leader_streamH,			/* >> */
						A_Boolean 			separatedB);			/* << */

	// Retrieve the Follower stream corresponding to a given dimension of the Leader stream. dimS
	// can range from 0 to AEGP_GetStreamValueDimensionality(leader_streamH) - 1.
	SPAPI A_Err (*AEGP_GetSeparationFollower)(
						AEGP_StreamRefH		leader_streamH,			/* >> */
						A_short				dimS,					/* >> */
						AEGP_StreamRefH 	*follower_streamPH);	/* << */

	// Valid on leaf streams only. Returns true if this stream is a one dimensional property
	// that represents one of the dimensions of a Leader. You can retrieve stream from the Leader
	// using AEGP_GetSeparationFollower().
	SPAPI A_Err (*AEGP_IsSeparationFollower)(
						AEGP_StreamRefH		streamH,				/* >> */
						A_Boolean 			*followerPB);			/* << */

	// Valid on separation Followers only, returns the Leader it is part of
	SPAPI A_Err (*AEGP_GetSeparationLeader)(
						AEGP_StreamRefH		follower_streamH,		/* >> */
						AEGP_StreamRefH 	*leader_streamPH);		/* << */

	// Valid on separation Followers only, returns which dimension of the Leader it corresponds to
	SPAPI A_Err (*AEGP_GetSeparationDimension)(
						AEGP_StreamRefH		follower_streamH,		/* >> */
						A_short				*dimPS);				/* << */

} AEGP_DynamicStreamSuite4;



/* -------------------------------------------------------------------- */

typedef A_long AEGP_KeyframeIndex;

enum {
	AEGP_KeyframeFlag_NONE					= 0x00,
	AEGP_KeyframeFlag_TEMPORAL_CONTINUOUS	= 0x01,
	AEGP_KeyframeFlag_TEMPORAL_AUTOBEZIER	= 0x02,
	AEGP_KeyframeFlag_SPATIAL_CONTINUOUS	= 0x04,
	AEGP_KeyframeFlag_SPATIAL_AUTOBEZIER	= 0x08,
	AEGP_KeyframeFlag_ROVING				= 0x10
};
typedef A_long	AEGP_KeyframeFlags;


enum {
	AEGP_NumKF_NO_DATA		= -1
};

#define kAEGPKeyframeSuite				"AEGP Keyframe Suite"
#define kAEGPKeyframeSuiteVersion5		5 	/* frozen in 22.5 */

typedef struct AEGP_KeyframeSuite5 {
	//	the only diff from this vs. last rev is the addition of new keyframe label utils

	// returns AEGP_NumKF_NO_DATA if it's a AEGP_StreamType_NO_DATA, and you can't retrieve any values
	// returns zero if no keyframes (but might have an expression, so not necessarily constant)
	SPAPI A_Err	(*AEGP_GetStreamNumKFs)(
						AEGP_StreamRefH 		streamH,				/* >> */
						A_long					*num_kfsPL);			/* << */


	SPAPI A_Err (*AEGP_GetKeyframeTime)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_LTimeMode			time_mode,				/* >> */
						A_Time					*timePT);				/* << */

	// leaves stream unchanged if a keyframe already exists at specified time
	SPAPI A_Err (*AEGP_InsertKeyframe)(									/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* <> */
						AEGP_LTimeMode			time_mode,				/* >> */
						const A_Time			*timePT,				/* >> */
						AEGP_KeyframeIndex		*key_indexP);			/* << */

	SPAPI A_Err (*AEGP_DeleteKeyframe)(									/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* <> */
						AEGP_KeyframeIndex		key_index);				/* >> */

	SPAPI A_Err (*AEGP_GetNewKeyframeValue)(							// dispose using AEGP_DisposeStreamValue()
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_StreamValue2		*valueP);				/* << */

	SPAPI A_Err (*AEGP_SetKeyframeValue)(								/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						const AEGP_StreamValue2	*valueP);				/* >>  not adopted */

	SPAPI A_Err (*AEGP_GetStreamValueDimensionality)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_short					*value_dimPS);			/* << */

	SPAPI A_Err (*AEGP_GetStreamTemporalDimensionality)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_short					*temporal_dimPS);		/* << */

	SPAPI A_Err (*AEGP_GetNewKeyframeSpatialTangents)(					// dispose using AEGP_DisposeStreamValue()
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_StreamValue2		*in_tanP0,				/* << */
						AEGP_StreamValue2		*out_tanP0);			/* << */

	SPAPI A_Err (*AEGP_SetKeyframeSpatialTangents)(						/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						const AEGP_StreamValue2	*in_tanP0,				/* >>  not adopted */
						const AEGP_StreamValue2	*out_tanP0);			/* >>  not adopted */

	SPAPI A_Err (*AEGP_GetKeyframeTemporalEase)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						A_long					dimensionL,				/* >> ranges from 0..TemporalDimensionality-1 */
						AEGP_KeyframeEase		*in_easeP0,				/* << */
						AEGP_KeyframeEase		*out_easeP0);			/* << */

	SPAPI A_Err (*AEGP_SetKeyframeTemporalEase)(						/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						A_long					dimensionL,				/* >> ranges from 0..TemporalDimensionality-1 */
						const AEGP_KeyframeEase	*in_easeP0,				/* >> not adopted */
						const AEGP_KeyframeEase	*out_easeP0);			/* >> not adopted */

	SPAPI A_Err (*AEGP_GetKeyframeFlags)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_KeyframeFlags		*flagsP);				/* << */

	SPAPI A_Err (*AEGP_SetKeyframeFlag)(								/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_KeyframeFlags		flag,					/* >> set one flag at a time */
						A_Boolean				true_falseB);			/* >> */

	SPAPI A_Err (*AEGP_GetKeyframeInterpolation)(
						AEGP_StreamRefH					streamH,		/* >> */
						AEGP_KeyframeIndex				key_index,		/* >> */
						AEGP_KeyframeInterpolationType	*in_interpP0,	/* << */
						AEGP_KeyframeInterpolationType	*out_interpP0);	/* << */

	SPAPI A_Err (*AEGP_SetKeyframeInterpolation)(						/* UNDOABLE */
						AEGP_StreamRefH					streamH,		/* >> */
						AEGP_KeyframeIndex				key_index,		/* >> */
						AEGP_KeyframeInterpolationType	in_interp,		/* >> */
						AEGP_KeyframeInterpolationType	out_interp);	/* >> */

	SPAPI A_Err (*AEGP_StartAddKeyframes)(
						AEGP_StreamRefH			streamH,
						AEGP_AddKeyframesInfoH	*akPH);			/* << */


	SPAPI A_Err (*AEGP_AddKeyframes)(
						AEGP_AddKeyframesInfoH	akH,			/* <> */
						AEGP_LTimeMode          time_mode,		/* >> */
						const A_Time            *timePT,		/* >> */
						A_long					*key_indexPL);	/* >> */

	SPAPI A_Err (*AEGP_SetAddKeyframe)(
						AEGP_AddKeyframesInfoH	akH,			/* <> */
						A_long					key_indexL,		/* >> */
						const AEGP_StreamValue2	*valueP);		/* >> */

	SPAPI A_Err (*AEGP_EndAddKeyframes)(						/* UNDOABLE */
						A_Boolean				addB,
						AEGP_AddKeyframesInfoH	akH);			/* >> */

	
	SPAPI A_Err(*AEGP_GetKeyframeLabelColorIndex)(				/* UNDOABLE */
						AEGP_StreamRefH			streamH,		/* >> */
						AEGP_KeyframeIndex		key_index,		/* >> */
						A_long					* key_labelP);	/* >> */

	SPAPI A_Err(*AEGP_SetKeyframeLabelColorIndex)(				/* UNDOABLE */
						AEGP_StreamRefH			streamH,		/* >> */
						AEGP_KeyframeIndex		key_index,		/* >> */
						A_long					key_label);		/* >> */


} AEGP_KeyframeSuite5;

/* -------------------------------------------------------------------- */

#define kAEGPTextDocumentSuite				"AEGP Text Document Suite"
#define kAEGPTextDocumentSuiteVersion1		1 /* frozen in AE 6.0 */

typedef struct AEGP_TextDocumentSuite1 {

	SPAPI A_Err	(*AEGP_GetNewText)(
						AEGP_PluginID			aegp_plugin_id,		/* >> */
						AEGP_TextDocumentH		text_documentH,		/* >> */
						AEGP_MemHandle			*unicodePH);		/* << handle of A_u_short, UTF16, NULL terminated. must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err	(*AEGP_SetText)(
						AEGP_TextDocumentH		text_documentH,		/* >> */
						const A_u_short			*unicodePS,			/* >> */
						A_long					lengthL);			/* >> number of characters */

} AEGP_TextDocumentSuite1;

/* -------------------------------------------------------------------- */

#define kAEGPMarkerSuite				"AEGP Marker Suite"
#define kAEGPMarkerSuiteVersion3		3 /* frozen in AE 16.0 */

enum {
	AEGP_MarkerString_NONE,

	AEGP_MarkerString_COMMENT,
	AEGP_MarkerString_CHAPTER,
	AEGP_MarkerString_URL,
	AEGP_MarkerString_FRAME_TARGET,
	AEGP_MarkerString_CUE_POINT_NAME,

	AEGP_MarkerString_NUMTYPES
};
typedef A_long AEGP_MarkerStringType;

enum {
	AEGP_MarkerFlag_NONE			= 0x00000000,
	AEGP_MarkerFlag_NAVIGATION		= 0x00000001,	//	if this bit is zero, then the marker is for "Event" rather than "Navigation"
	AEGP_MarkerFlag_PROTECT_REGION	= 0x00000002	//	if this bit is 1, then the marker is for a protected region  (protected against timestretching, when existing on a precomp layer)
};
typedef A_long	AEGP_MarkerFlagType;

typedef struct AEGP_MarkerSuite3 {

	SPAPI A_Err	(*AEGP_NewMarker)(
						AEGP_MarkerValP			*markerPP);

	SPAPI A_Err	(*AEGP_DisposeMarker)(
						AEGP_MarkerValP			markerP);

	SPAPI A_Err	(*AEGP_DuplicateMarker)(
						AEGP_MarkerValP			markerP, 			// >>
						AEGP_MarkerValP			*new_markerP);		// <<

	SPAPI A_Err	(*AEGP_SetMarkerFlag)(
						AEGP_MarkerValP			markerP,			// >>
						AEGP_MarkerFlagType		flagType,			// >>
						A_Boolean				valueB);			// >>

	SPAPI A_Err	(*AEGP_GetMarkerFlag)(
						AEGP_ConstMarkerValP	markerP,			// >>
						AEGP_MarkerFlagType		flagType,			// >>
							A_Boolean				*valueBP);			// <<

	SPAPI A_Err	(*AEGP_GetMarkerString)(
						AEGP_PluginID			aegp_plugin_id,		/* >> */
						AEGP_ConstMarkerValP	markerP,			// >>
						AEGP_MarkerStringType	strType,			// >>
						AEGP_MemHandle			*unicodePH);		/* << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err	(*AEGP_SetMarkerString)(
						AEGP_MarkerValP			markerP,			// <<>>
						AEGP_MarkerStringType	strType,			// >>
						const A_u_short			*unicodeP,			// >>
						A_long					lengthL);			// >> number of characters

	SPAPI A_Err	(*AEGP_CountCuePointParams)(
						AEGP_ConstMarkerValP	markerP,			// >>
						A_long					*paramsLP);			// <<

	SPAPI A_Err	(*AEGP_GetIndCuePointParam)(
						AEGP_PluginID			aegp_plugin_id,		// >>
						AEGP_ConstMarkerValP	markerP,			// >>
						A_long					param_indexL,		// >> must be between 0 and count - 1. else error
						AEGP_MemHandle			*unicodeKeyPH,		// << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle
						AEGP_MemHandle			*unicodeValuePH);	// << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_SetIndCuePointParam)(
						AEGP_MarkerValP			markerP,			// >>
						A_long					param_indexL,		// must be between 0 and count - 1. else error
						const A_u_short			*unicodeKeyP,		// >> UTF16
						A_long					key_lengthL,		// >> number of characters
						const A_u_short			*unicodeValueP,		// >> UTF16
						A_long					value_lengthL);		// >> number of characters

	//	this call is followed by AEGP_SetIndCuePointParam() to actually set the data
	//	the ONLY thing this function does is reserve the space for the param, at the provided index
	SPAPI A_Err	(*AEGP_InsertCuePointParam)(
						AEGP_MarkerValP			markerP,			// >>
						A_long					param_indexL);		// must be between 0 and count. else error

	SPAPI A_Err	(*AEGP_DeleteIndCuePointParam)(
						AEGP_MarkerValP			markerP,			// >>
						A_long					param_indexL);		// must be between 0 and count - 1. else error

	SPAPI A_Err	(*AEGP_SetMarkerDuration)(
						AEGP_MarkerValP			markerP,			// >>
						const A_Time			*durationPT);		// >>


	SPAPI A_Err	(*AEGP_GetMarkerDuration)(
						AEGP_ConstMarkerValP	markerP,			// >>
						A_Time					*durationPT);		// <<

	SPAPI A_Err (*AEGP_SetMarkerLabel)(
						AEGP_MarkerValP			markerP,			// >>
						A_long		            value);				// >>

	SPAPI A_Err (*AEGP_GetMarkerLabel)(
						AEGP_ConstMarkerValP	markerP,			// >>
						A_long			        *valueP);		    // <<

} AEGP_MarkerSuite3;


/* -------------------------------------------------------------------- */

#define kAEGPTextLayerSuite				"AEGP Text Layer Suite"
#define kAEGPTextLayerSuiteVersion1		1 /* frozen in AE 6.0 */

typedef struct AEGP_TextLayerSuite1 {

	SPAPI A_Err	(*AEGP_GetNewTextOutlines)(
						AEGP_LayerH				layerH,				/* >> must be a text layer */
						const A_Time			*layer_timePT,		/* >> */
						AEGP_TextOutlinesH		*outlinesPH);		/* << must be disposed with AEGP_DisposeTextOutlines */

	SPAPI A_Err	(*AEGP_DisposeTextOutlines)(
						AEGP_TextOutlinesH		outlinesH);			/* >> */

	SPAPI A_Err	(*AEGP_GetNumTextOutlines)(
						AEGP_TextOutlinesH		outlinesH,			/* >> */
						A_long					*num_outlinesPL);	/* << */

	SPAPI A_Err	(*AEGP_GetIndexedTextOutline)(
						AEGP_TextOutlinesH		outlinesH,			/* >> */
						A_long					path_indexL,		/* >> */
						PF_PathOutlinePtr		*pathPP);			/* << DO NOT DISPOSE */

} AEGP_TextLayerSuite1;


/* -------------------------------------------------------------------- */


typedef A_long	AEGP_InstalledEffectKey;
#define AEGP_InstalledEffectKey_NONE	0

#define RQ_ITEM_INDEX_NONE				-1

enum {
	AEGP_EffectFlags_NONE			=0,
	AEGP_EffectFlags_ACTIVE			=0x01L << 0,
	AEGP_EffectFlags_AUDIO_ONLY		=0x01L << 1,
	AEGP_EffectFlags_AUDIO_TOO		=0x01L << 2,
	AEGP_EffectFlags_MISSING		=0x01L << 3
};
typedef A_long	AEGP_EffectFlags;

typedef A_long AEGP_EffectIndex;


#define kAEGPEffectSuite				"AEGP Effect Suite"
#define kAEGPEffectSuiteVersion4		4 /* frozen in AE 13.0 */

typedef struct AEGP_EffectSuite4 {

	SPAPI A_Err	(*AEGP_GetLayerNumEffects)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*num_effectsPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerEffectByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_EffectIndex	layer_effect_indexL,	/* >> */
						AEGP_EffectRefH		*effectPH);				/* <<  MUST dispose with DisposeEffect*/

	SPAPI A_Err (*AEGP_GetInstalledKeyFromLayerEffect)(
						AEGP_EffectRefH		effect_refH,					/* >> */
						AEGP_InstalledEffectKey	*installed_effect_keyP);	/* << */

	SPAPI A_Err (*AEGP_GetEffectParamUnionByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
						PF_ParamType		*param_typeP,			/* << */
						PF_ParamDefUnion	*uP0);					/* << DO NOT USE THE VALUE FROM THIS PARAMDEF! */

	SPAPI A_Err (*AEGP_GetEffectFlags)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						AEGP_EffectFlags	*effect_flagsP);		/* << */

	SPAPI A_Err (*AEGP_SetEffectFlags)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						AEGP_EffectFlags	effect_flags_set_mask,	/* >> */
						AEGP_EffectFlags	effect_flags);			/* >> */

	SPAPI A_Err	(*AEGP_ReorderEffect)(								/* UNDOABLE */
						AEGP_EffectRefH 	effect_refH,			/* >> */
						A_long 				effect_indexL);			/* >> */

	/** new command parameter addded. To get old behaviour pass in PF_Cmd_COMPLETELY_GENERAL for effect_command **/
	SPAPI A_Err (*AEGP_EffectCallGeneric)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						const A_Time		*timePT,				/* >> Use the timebase of the layer to which effect is applied. */
						PF_Cmd				effect_cmd,				/* >> new parameter from version 2 */
						void				*effect_extraPV);		/* <> */

	SPAPI A_Err (*AEGP_DisposeEffect)(
						AEGP_EffectRefH	effect_refH );				/* >> */

	SPAPI A_Err (*AEGP_ApplyEffect)(
						AEGP_PluginID		aegp_plugin_id,				/* >> */
						AEGP_LayerH			layerH,						/* >> */
						AEGP_InstalledEffectKey	installed_effect_key,	/* >> */
						AEGP_EffectRefH		*effect_refPH);				/* <<  MUST BE DISPOSED with AEGP_DisposeEffect */

	SPAPI A_Err	(*AEGP_DeleteLayerEffect)(
						AEGP_EffectRefH 	effect_refH);				/* >>  undoable */

	SPAPI A_Err (*AEGP_GetNumInstalledEffects)(
						A_long 				*num_installed_effectsPL);	/* << */

	// pass in AEGP_InstalledEffectKey_NONE for installed_effect_key to get first effect key

	SPAPI A_Err (*AEGP_GetNextInstalledEffect)(
									AEGP_InstalledEffectKey	installed_effect_key,	/* >> */
									AEGP_InstalledEffectKey	*next_effectPH);		/* << */

	SPAPI A_Err	(*AEGP_GetEffectName)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*nameZ);				/* << space for A_char[AEGP_MAX_EFFECT_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetEffectMatchName)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*utf8_match_nameZ);		/* << UTF8!! space for A_char[AEGP_MAX_EFFECT_MATCH_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetEffectCategory)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*categoryZ);			/* << space for A_char[AEGP_MAX_EFFECT_CATEGORY_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_DuplicateEffect)(
						AEGP_EffectRefH		original_effect_refH,			/* >> */
						AEGP_EffectRefH		*duplicate_effect_refPH);		/* << */

	/** new in AE 13.0: effect masks */
	SPAPI A_Err	(*AEGP_NumEffectMask)(
						AEGP_EffectRefH		effect_refH,	/* >> */
						A_u_long			*num_masksPL);	/* << */

	SPAPI A_Err	(*AEGP_GetEffectMaskID)(
						AEGP_EffectRefH		effect_refH,	/* >> */
						A_u_long			mask_indexL,	/* >> */
						AEGP_MaskIDVal		*id_valP);		/* << */

	SPAPI A_Err	(*AEGP_AddEffectMask)(						/* UNDOABLE */
						AEGP_EffectRefH		effect_refH,	/* >> */
						AEGP_MaskIDVal		id_val,			/* >> */
						AEGP_StreamRefH 	*streamPH0);	/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_RemoveEffectMask)(					/* UNDOABLE */
					AEGP_EffectRefH		effect_refH,		/* >> */
					AEGP_MaskIDVal		id_val);			/* >> */

	SPAPI A_Err	(*AEGP_SetEffectMask)(						/* UNDOABLE */
					AEGP_EffectRefH		effect_refH,		/* >> */
					A_u_long			mask_indexL,		/* >> */
					AEGP_MaskIDVal		id_val,				/* >> */
					AEGP_StreamRefH 	*streamPH0);		/* << must be disposed by caller! */
} AEGP_EffectSuite4;


/* -------------------------------------------------------------------- */

typedef A_long	AEGP_MaskIndex;

enum {
	AEGP_MaskMBlur_SAME_AS_LAYER,
	AEGP_MaskMBlur_OFF,
	AEGP_MaskMBlur_ON
};
typedef A_u_char AEGP_MaskMBlur; // This must be A_u_char, used in disk safe BEE_MaskInfo

enum {
	AEGP_MaskFeatherFalloff_SMOOTH,
	AEGP_MaskFeatherFalloff_LINEAR
};
typedef A_u_char AEGP_MaskFeatherFalloff; // This must be A_u_char, used in disk safe BEE_MaskInfo

enum {
	AEGP_MaskFeatherInterp_NORMAL,
	AEGP_MaskFeatherInterp_HOLD_CW
};
typedef A_u_char AEGP_MaskFeatherInterp;

enum {
	AEGP_MaskFeatherType_OUTER,
	AEGP_MaskFeatherType_INNER
};
typedef A_u_char AEGP_MaskFeatherType;

#define kAEGPMaskSuite					"AEGP Layer Mask Suite"
#define kAEGPMaskSuiteVersion6			7 /* frozen AE 11 */

typedef struct AEGP_MaskSuite6 {

	SPAPI A_Err	(*AEGP_GetLayerNumMasks)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						A_long					*num_masksPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerMaskByIndex)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						AEGP_MaskIndex			mask_indexL,		/* >> */
						AEGP_MaskRefH			*maskPH);			/* << must be disposed by calling AEGP_DisposeMask() */

	SPAPI A_Err	(*AEGP_DisposeMask)(
						AEGP_MaskRefH			mask_refH);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskInvert)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_Boolean				*invertPB);			/* << */

	SPAPI A_Err (*AEGP_SetMaskInvert)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_Boolean				invertB);			/* << */

	SPAPI A_Err	(*AEGP_GetMaskMode)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						PF_MaskMode				*modeP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskMode)(
						AEGP_MaskRefH			maskH,				/* >> */
						PF_MaskMode				mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskMotionBlurState)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskMBlur			*blur_stateP);		/* << */

	SPAPI A_Err (*AEGP_SetMaskMotionBlurState)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskMBlur			blur_state);		/* >> */

	SPAPI A_Err (*AEGP_GetMaskFeatherFalloff)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskFeatherFalloff *feather_falloffP);	/* << */

	SPAPI A_Err (*AEGP_SetMaskFeatherFalloff)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskFeatherFalloff	feather_falloffP);	/* >> */

	// AEGP_GetMaskName/SetMaskName are obsoleted. Use AEGP_GetNewDynamicStreamForMask
	// and the name functions there

	SPAPI A_Err	(*AEGP_GetMaskID)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskIDVal			*id_valP);			/* << */

	SPAPI A_Err (*AEGP_CreateNewMask)(								/* UNDOABLE */
						AEGP_LayerH				layerH,				/* >> */
						AEGP_MaskRefH			*mask_refPH,		/* << */
						A_long					*mask_indexPL0);	/* << */

	SPAPI A_Err (*AEGP_DeleteMaskFromLayer)(						/* UNDOABLE */
						AEGP_MaskRefH			mask_refH);			/* >> still need to Dispose MaskRefH */

	SPAPI A_Err (*AEGP_GetMaskColor)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_ColorVal			*colorP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskColor)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						const AEGP_ColorVal		*colorP);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskLockState)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				*is_lockedPB);		/* >> */

	SPAPI A_Err	(*AEGP_SetMaskLockState)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				lockB);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskIsRotoBezier)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				*is_roto_bezierPB);	/* << */

	SPAPI A_Err (*AEGP_SetMaskIsRotoBezier)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				is_roto_bezierB);	/* >> */

	SPAPI A_Err (*AEGP_DuplicateMask)(
						AEGP_MaskRefH			orig_mask_refH,			/* >> */
						AEGP_MaskRefH			*duplicate_mask_refPH);	/* << */

} AEGP_MaskSuite6;


/* -------------------------------------------------------------------- */

typedef struct {
	A_long					segment;			/* mask segment where feather is located	*/
	PF_FpLong				segment_sF;			/* 0-1: feather location on segment			*/
	PF_FpLong				radiusF;			/* negative value allowed if type == AEGP_MaskFeatherType_INNER */
	PF_FpShort				ui_corner_angleF;	/* 0-1: angle of UI handle on corners		*/
	PF_FpShort				tensionF;			/* 0-1: tension of boundary at feather pt	*/
	AEGP_MaskFeatherInterp	interp;
	AEGP_MaskFeatherType	type;				
} AEGP_MaskFeather;

typedef A_long	AEGP_FeatherIndex;

typedef PF_PathVertex		AEGP_MaskVertex;

typedef A_long	AEGP_VertexIndex;
#define AEGP_VertexIndex_END 10922

#define kAEGPMaskOutlineSuite				"AEGP Mask Outline Suite"
#define kAEGPMaskOutlineSuiteVersion3		5 /* frozen in AE 11 */

typedef struct AEGP_MaskOutlineSuite3 {

	SPAPI A_Err	(*AEGP_IsMaskOutlineOpen)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_Boolean					*openPB);			/* << */

	SPAPI A_Err (*AEGP_SetMaskOutlineOpen)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_Boolean					openB);				/* >> */

	// N segments means there are segments [0..N-1]; segment J is defined by vertex J & J+1
	SPAPI A_Err	(*AEGP_GetMaskOutlineNumSegments)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_long						*num_segmentsPL);	/* << */

	// which_pointL range: [0..num_segments]; for closed masks vertex[0] == vertex[num_segments]
	SPAPI A_Err	(*AEGP_GetMaskOutlineVertexInfo)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			which_pointL,		/* >> */
						AEGP_MaskVertex				*vertexP);			/* << tangents are relative to position */

	// Setting vertex 0 is special. Its in tangent will actually set the out tangent
	// of the last vertex in the outline.
	SPAPI A_Err	(*AEGP_SetMaskOutlineVertexInfo)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			which_pointL,		/* >> must already exists (use Create) */
						const AEGP_MaskVertex		*vertexP);			/* >> tangents are relative to position */

	SPAPI A_Err (*AEGP_CreateVertex)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			insert_position);	/* >> will insert at this index. moving other verticies index++*/

	SPAPI A_Err (*AEGP_DeleteVertex)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			index);				/* >> */


	SPAPI A_Err	(*AEGP_GetMaskOutlineNumFeathers)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_long						*num_feathersPL);	/* << */

	SPAPI A_Err	(*AEGP_GetMaskOutlineFeatherInfo)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_FeatherIndex			which_featherL,		/* >> */
						AEGP_MaskFeather			*featherP);			/* << */

	SPAPI A_Err	(*AEGP_SetMaskOutlineFeatherInfo)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			which_featherL,		/* >> must already exists (use Create) */
						const AEGP_MaskFeather		*featherP);			/* >> */

	SPAPI A_Err (*AEGP_CreateMaskOutlineFeather)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						const AEGP_MaskFeather		*featherP0,			/* >> */
						AEGP_FeatherIndex			*insert_positionP);	/* << index of new feather */

	SPAPI A_Err (*AEGP_DeleteMaskOutlineFeather)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_FeatherIndex			index);				/* >> */

} AEGP_MaskOutlineSuite3;


/* -------------------------------------------------------------------- */


typedef	FIEL_Label AEGP_InterlaceLabel;

enum {
	AEGP_AlphaPremul 		= 0x1,		/* otherwise straight */
	AEGP_AlphaInverted		= 0x2,		/*  255 = transparent */
	AEGP_AlphaIgnore		= 0x4
};
typedef A_u_long	AEGP_AlphaFlags;

typedef struct {
	AEGP_AlphaFlags	flags;
	A_u_char		redCu;		// color that was matted (if premul)
	A_u_char		greenCu;
	A_u_char		blueCu;
} AEGP_AlphaLabel;

enum {
	AEGP_PulldownPhase_NO_PULLDOWN = 0,
	AEGP_PulldownPhase_WSSWW = 1,
	AEGP_PulldownPhase_SSWWW,
	AEGP_PulldownPhase_SWWWS,
	AEGP_PulldownPhase_WWWSS,
	AEGP_PulldownPhase_WWSSW,
	AEGP_PulldownPhase_WWWSW,
	AEGP_PulldownPhase_WWSWW,
	AEGP_PulldownPhase_WSWWW,
	AEGP_PulldownPhase_SWWWW,
	AEGP_PulldownPhase_WWWWS
};
typedef A_long AEGP_PulldownPhase;

typedef struct {
	A_long			loops;
	A_long			reserved;		/* set to 0; reserved for future use (palindrome, etc.) */
} AEGP_LoopBehavior;

typedef struct {
	AEGP_InterlaceLabel		il;
	AEGP_AlphaLabel			al;
	AEGP_PulldownPhase		pd;
	AEGP_LoopBehavior		loop;
	A_Ratio					pix_aspect_ratio;
	A_FpLong				native_fpsF;
	A_FpLong				conform_fpsF;
	A_long					depthL;
	A_Boolean				motion_dB;
} AEGP_FootageInterp;

#define AEGP_FOOTAGE_LAYER_NAME_LEN		(63)
#define AEGP_LayerIndex_UNKNOWN			(-2)
#define AEGP_LayerIndex_MERGED			(-1)
#define AEGP_LayerID_UNKNOWN			(-1)

enum {
	AEGP_LayerDrawStyle_LAYER_BOUNDS = 0,
	AEGP_LayerDrawStyle_DOCUMENT_BOUNDS
};

typedef A_long AEGP_LayerDrawStyle;


typedef struct {
	A_long					layer_idL;									/* unique ID for layer, as contained in a Photoshop document's 'lyid' resource. */
																		/* pass AEGP_LayerID_UNKNOWN if you don't know this */
	A_long					layer_indexL;								/* zero-based layer index. pass AEGP_LayerIndex_MERGED for merged layers */
	A_char					nameAC[AEGP_FOOTAGE_LAYER_NAME_LEN + 1];	/* used for sequences and backup for linking  */
	AEGP_LayerDrawStyle		layer_draw_style;

} AEGP_FootageLayerKey;

#define AEGP_ANY_FRAME -1

typedef struct {
	A_Boolean	all_in_folderB;                 /* TRUE means intepret as a sequence, FALSE means still frame.
												   If FALSE, other parameters in this structure have no effect. */
	A_Boolean	force_alphabeticalB;			/* if TRUE, filenames of sequence will be forced to alphabetical order */
	A_long		start_frameL;                   /* first frame of sequence, AEGP_ANY_FRAME means earliest frame found */
	A_long		end_frameL;						/* last frame of sequence, AEGP_ANY_FRAME means last frame found */
} AEGP_FileSequenceImportOptions;

enum {
	AEGP_FootageSignature_NONE = -1,		// invalid sig
	AEGP_FootageSignature_MISSING = 0,		// placeholder
	AEGP_FootageSignature_SOLID = 'Soli'
};
typedef A_long AEGP_FootageSignature;


#define AEGP_FOOTAGE_MAIN_FILE_INDEX		0

#define kAEGPFootageSuite				"AEGP Footage Suite"

#define kAEGPFootageSuiteVersion5		11 /* frozen in AE 10.0 */


enum {
	AEGP_InterpretationStyle_NO_DIALOG_GUESS = 0,		// FALSE for backwards compatability:  will guess alpha interpretation even if file contains unknown alpha interpretation and user pref says to ask user
	AEGP_InterpretationStyle_DIALOG_OK = 1,				// TRUE for backwards comptability. Optionally can show a dialog.
	AEGP_InterpretationStyle_NO_DIALOG_NO_GUESS = 2  // used for replace footage implementation
};
typedef A_u_char AEGP_InterpretationStyle;

typedef struct AEGP_FootageSuite5 {

	SPAPI A_Err	(*AEGP_GetMainFootageFromItem)(						/* error if item isn't AEGP_ItemType_FOOTAGE! */
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_FootageH		*footagePH);			/* << */

	SPAPI A_Err	(*AEGP_GetProxyFootageFromItem)(					/* error if has_proxy is false! (note, item could still be a comp) */
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_FootageH		*proxy_footagePH);		/* << */

	SPAPI A_Err	(*AEGP_GetFootageNumFiles)(
						AEGP_FootageH		footageH,				/* >> */
						A_long				*num_main_filesPL0,		/* << */
						A_long				*files_per_framePL0);	/* << includes main file. e.g. 1 for no aux data */

	SPAPI A_Err	(*AEGP_GetFootagePath)(
						AEGP_FootageH		footageH,				/* >> */
						A_long				frame_numL,				/* >> range is 0 to num_main_files */
						A_long			 	file_indexL,			/* >> AEGP_FOOTAGE_MAIN_FILE_INDEX is main file */
						AEGP_MemHandle		*unicode_pathPH);		// << empty string if no file. handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err (*AEGP_GetFootageSignature)(
						AEGP_FootageH			footageH,			/* >> */
						AEGP_FootageSignature	*sigP);				/* << like filetype, but also for non-file types like solids, etc. */

	SPAPI A_Err	(*AEGP_NewFootage)(
						AEGP_PluginID				aegp_plugin_id,		/* >> */
						const A_UTF16Char			*pathZ,				// >> null terminated unicode path with platform separators
						const AEGP_FootageLayerKey	*layer_infoP0,		/* >> optional layer info; pass NULL for merged layers */
						const AEGP_FileSequenceImportOptions *sequence_optionsP0, /* >> optional sequence info; passing NULL means not a sequence */
						AEGP_InterpretationStyle	interp_style,		/* >> (in) */
  						void 						*reserved,			/* >> pass NULL */
						AEGP_FootageH				*footagePH);		/* << caller owns until disposed or added to project */

	SPAPI A_Err	(*AEGP_AddFootageToProject)(						/* UNDOABLE */
						AEGP_FootageH		footageH,				/* >> will be adopted by project, may not be added more than once */
						AEGP_ItemH 			folderH,				/* >> add to this folder */
						AEGP_ItemH			*added_itemPH0);		/* << */

	SPAPI A_Err	(*AEGP_SetItemProxyFootage)(						/* UNDOABLE */
						AEGP_FootageH		footageH,				/* >> will be adopted by project, may not be set more than once */
						AEGP_ItemH 			itemH);					/* >> set for this item */

	SPAPI A_Err	(*AEGP_ReplaceItemMainFootage)(						/* UNDOABLE */
						AEGP_FootageH		footageH,				/* >> will be adopted by project, may not be set more than once */
						AEGP_ItemH 			itemH);					/* >> replace main footage for this item */

	SPAPI A_Err	(*AEGP_DisposeFootage)(
						AEGP_FootageH		footageH);				/* >> do not dipose footage that is owned or has been adopted by project */

	SPAPI A_Err (*AEGP_GetFootageInterpretation)(
						AEGP_ItemH 			itemH,					/* >> note: item that contains footage */
						A_Boolean			proxyB,					/* >> TRUE = get proxy interp; FALSE = get main interp */
						AEGP_FootageInterp	*interpP);				/* << */

	SPAPI A_Err (*AEGP_SetFootageInterpretation)(					/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> note: item that contains footage */
						A_Boolean			proxyB,					/* >> TRUE = set proxy interp; FALSE = set main interp */
						const AEGP_FootageInterp	*interpP);		/* >> */

	SPAPI A_Err (*AEGP_GetFootageLayerKey) (
						AEGP_FootageH			footageH,			/* >> */
						AEGP_FootageLayerKey	*layerKeyP);		/* << the footages layer info */

	SPAPI A_Err (*AEGP_NewPlaceholderFootage)(						/* doesn't modify project, creates footage with AEGP_FootageSignature_MISSING */
						AEGP_PluginID		plugin_id,				/* >> */
						const A_char		*nameZ,					/* >> 	 file name, not the path! */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const A_Time		*durationPT,			/* >> */
						AEGP_FootageH		*footagePH);			/* << caller owns until disposed or added to project */

	SPAPI A_Err	(*AEGP_NewPlaceholderFootageWithPath)(				/* doesn't modify project, creates footage with AEGP_FootageSignature_MISSING */
						AEGP_PluginID		plugin_id,				/* >> */
						const A_UTF16Char	*pathZ,					// >> null terminated unicode path with platform separators
						AEGP_Platform		path_platform,			/* >>  Mac or Win */
						AEIO_FileType		file_type, 				// >>  AEIO_FileType_NONE is now a warning condition. If you pass AEIO_FileType_ANY, then path MUST exist.  if path may not exist: pass AEIO_FileType_DIR for folder, or AEIO_FileType_GENERIC for a file
						A_long				widthL,					/* >> */
						A_long				heightL,				/* >> */
						const	A_Time		*durationPT,			/* >> */
						AEGP_FootageH		*footagePH);			/* << caller owns until disposed or added to project */

	SPAPI A_Err (*AEGP_NewSolidFootage)(							/* doesn't modify project, creates footage with AEGP_FootageSignature_SOLID */
						const A_char		*nameZ,					/* >> 	 file name, not the path! */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const AEGP_ColorVal	*colorP,				/* >> */
						AEGP_FootageH		*footagePH);			/* << caller owns until disposed or added to project */

	SPAPI A_Err	(*AEGP_GetSolidFootageColor)(						/* error if footage isn't AEGP_FootageSignature_SOLID */
						AEGP_ItemH 			itemH,					/* >> note: item that contains footage */
						A_Boolean			proxyB,					/* >> TRUE = get proxy solid color; FALSE = get main solid color */
						AEGP_ColorVal		*colorP);				/* << */

	SPAPI A_Err	(*AEGP_SetSolidFootageColor)(						/* UNDOABLE, error if footage isn't AEGP_FootageSignature_SOLID */
						AEGP_ItemH 			itemH,					/* >> note: item that contains footage */
						A_Boolean			proxyB,					/* >> TRUE = set proxy solid color; FALSE = set main solid color */
						const AEGP_ColorVal	*colorP);				/* >> */

	SPAPI A_Err (*AEGP_SetSolidFootageDimensions)(					/* UNDOABLE, error if footage isn't AEGP_FootageSignature_SOLID */
						AEGP_ItemH 			itemH,					/* >> note: item that contains footage */
						A_Boolean			proxyB,					/* >> TRUE = set proxy solid size; FALSE = set main solid size */
						A_long				widthL,					/* >> min 1, max 30,000 */
						A_long				heightL);				/* >> min 1, max 30,000 */

	SPAPI A_Err (*AEGP_GetFootageSoundDataFormat)(
					AEGP_FootageH			footageH,				/* >> */
					AEGP_SoundDataFormat*	sound_formatP);			/* << */

	SPAPI A_Err (*AEGP_GetFootageSequenceImportOptions)(
					AEGP_FootageH						footageH,		/* >> */
					AEGP_FileSequenceImportOptions		*optionsP);		/* << */

} AEGP_FootageSuite5;

/* -------------------------------------------------------------------- */


typedef A_long	AEGP_Command;

#define AEGP_Command_ALL		0

enum {
	AEGP_WindType_NONE,
	AEGP_WindType_PROJECT,
	AEGP_WindType_COMP,
	AEGP_WindType_TIME_LAYOUT,
	AEGP_WindType_LAYER,
	AEGP_WindType_FOOTAGE,
	AEGP_WindType_RENDER_QUEUE,
	AEGP_WindType_QT,
	AEGP_WindType_DIALOG,
	AEGP_WindType_FLOWCHART,
	AEGP_WindType_EFFECT,
	AEGP_WindType_OTHER
};
typedef A_LegacyEnumType AEGP_WindowType;


enum {
	AEGP_HP_BeforeAE 		= 0x1,		// call hook before AE handles event (if AE handles)
	AEGP_HP_AfterAE			= 0x2		// call hook after AE handles event (if AE handles)
};
typedef A_u_long	AEGP_HookPriority;


typedef A_Err		(*AEGP_CommandHook)(
						AEGP_GlobalRefcon	plugin_refconP,			/* >> */
						AEGP_CommandRefcon	refconP,				/* >> */
						AEGP_Command		command,				/* >> */
						AEGP_HookPriority	hook_priority,			/* >> currently always AEGP_HP_BeforeAE */
						A_Boolean			already_handledB,		/* >> */
						A_Boolean			*handledPB);			/* << whether you handled */

typedef A_Err		(*AEGP_UpdateMenuHook)(
						AEGP_GlobalRefcon		plugin_refconP,		/* >> */
						AEGP_UpdateMenuRefcon	refconP,			/* >> */
						AEGP_WindowType			active_window);		/* >> */

typedef A_Err		(*AEGP_DeathHook)(
						AEGP_GlobalRefcon	plugin_refconP,			/* >> */
						AEGP_DeathRefcon	refconP);				/* >> */

typedef A_Err		(*AEGP_VersionHook)(							/* As of 5.0, not called */
						AEGP_GlobalRefcon	plugin_refconP,			/* >> */
						AEGP_VersionRefcon	refconP,				/* >> */
						A_u_long 			*pf_versionPLu);		/* << use PF_VERSION() macro to build and PF_Version_XXX() macros to access */

					// one line description. when displaying, AE will prepend name and version information.
					// this will be used to display a list of about info for all plugins

typedef A_Err		(*AEGP_AboutStringHook)(						/* As of 5.0, not called */
						AEGP_GlobalRefcon	plugin_refconP,			/* >> */
						AEGP_AboutStringRefcon	refconP,			/* >> */
						A_char				*aboutZ);				/* << space for A_char[AEGP_MAX_ABOUT_STRING_SIZE] */

					// bring up a dialog and tell us about yourself
typedef A_Err		(*AEGP_AboutHook)(								/* As of 5.0, not called */
						AEGP_GlobalRefcon	plugin_refconP,			/* >> */
						AEGP_AboutRefcon	refconP);				/* >> */

typedef A_Err		(*AEGP_IdleHook)(
						AEGP_GlobalRefcon	plugin_refconP,			/* >> */
						AEGP_IdleRefcon		refconP,				/* >> */
						A_long 				*max_sleepPL);			/* <> in 1/60 of a second*/




#define kAEGPRegisterSuite				"AEGP Register Suite"
#define kAEGPRegisterSuiteVersion5		6 /* frozen AE 10.0 */

typedef struct AEGP_RegisterSuite5 {

	SPAPI A_Err	(*AEGP_RegisterCommandHook)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_HookPriority	hook_priority,			/* >> */
						AEGP_Command		command,				/* >> use AEGP_Command_ALL to get all commands */
						AEGP_CommandHook	command_hook_func,		/* >> */
						AEGP_CommandRefcon	refconP);				/* >> */

	// this will be called anytime any menu is about to be drawn. it isn't specific to a menu so you
	// must enable all appropriate menu items when this hook is called.

	SPAPI A_Err	(*AEGP_RegisterUpdateMenuHook)(
						AEGP_PluginID			plugin_id,				/* >> */
						AEGP_UpdateMenuHook		update_menu_hook_func,	/* >> */
						AEGP_UpdateMenuRefcon	refconP);				/* >> */

	SPAPI A_Err	(*AEGP_RegisterDeathHook)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_DeathHook		death_hook_func,		/* >> */
						AEGP_DeathRefcon	refconP);				/* >> */

	SPAPI A_Err	(*AEGP_RegisterVersionHook)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_VersionHook	version_hook_func,		/* >> */
						AEGP_VersionRefcon	refconP);				/* >> */

	SPAPI A_Err	(*AEGP_RegisterAboutStringHook)(
						AEGP_PluginID			aegp_plugin_id,				/* >> */
						AEGP_AboutStringHook	about_string_hook_func,		/* >> */
						AEGP_AboutStringRefcon	refconP);					/* >> */

	SPAPI A_Err	(*AEGP_RegisterAboutHook)(
						AEGP_PluginID		aegp_plugin_id,					/* >> */
						AEGP_AboutHook		about_hook_func,				/* >> */
						AEGP_AboutRefcon	refconP);						/* >> */

	SPAPI A_Err	 (*AEGP_RegisterArtisan) (
						A_Version					api_version,			/* >> */
						A_Version					artisan_version,		/* >> */
						AEGP_PluginID				aegp_plugin_id,			/* >> */
						void						*aegp_refconPV,			/* <> */
						const A_char				*utf8_match_nameZ,		/* >> */
						const A_char				*artisan_nameZ,			/* >> */
						PR_ArtisanEntryPoints		*entry_funcs);			/* >> */

	SPAPI A_Err	 (*AEGP_RegisterIO) (
						AEGP_PluginID				aegp_plugin_id,			/* >> */
						AEGP_IORefcon				aegp_refconP,			/* >> */
						const AEIO_ModuleInfo		*io_infoP,				/* >> */
						const AEIO_FunctionBlock4	*aeio_fcn_blockP);		/* >> */

	SPAPI A_Err	(*AEGP_RegisterIdleHook)(
						AEGP_PluginID		aegp_plugin_id,					/* >> */
						AEGP_IdleHook		idle_hook_func,					/* >> */
						AEGP_IdleRefcon		refconP);						/* >> */


	SPAPI A_Err (*AEGP_RegisterTracker)(
						A_Version					api_version,			/* >> */
						A_Version					tracker_version,		/* >> */
						AEGP_PluginID				aegp_plugin_id,			/* >> */
						const	AEGP_GlobalRefcon	refconP,				/* >> */
						const A_char				*utf8_match_nameZ,		/* >> */
						const A_char				*tracker_nameZ,			/* >> */
						const PT_TrackerEntryPoints	*entry_pointsP);		/* >> */

	SPAPI A_Err	 (*AEGP_RegisterInteractiveArtisan) (
						A_Version					api_version,			/* >> */
						A_Version					artisan_version,		/* >> */
						AEGP_PluginID				aegp_plugin_id,			/* >> */
						void						*aegp_refconPV,			/* <> */
						const A_char				*utf8_match_nameZ,		/* >> */
						const A_char				*artisan_nameZ,			/* >> */
						PR_ArtisanEntryPoints		*entry_funcs);			/* >> */

	// Call this to register as many strings as you like for name-replacement
	// when presets are loaded. Any time a Property name is found, or referred
	// to in an expression, and it starts with an ASCII tab character ('\t'), followed
	// by one of the english names, it will be replaced with the localized name. (In
	// English the tab character will simply be removed).
	SPAPI A_Err	 (*AEGP_RegisterPresetLocalizationString) (
						const A_char				*english_nameZ,			/* >> */
						const A_char				*localized_nameZ);		/* >> */



} AEGP_RegisterSuite5;


/* -------------------------------------------------------------------- */


enum {
	AEGP_Menu_NONE,
	AEGP_Menu_APPLE,
	AEGP_Menu_FILE,
	AEGP_Menu_EDIT,
	AEGP_Menu_COMPOSITION,
	AEGP_Menu_LAYER,
	AEGP_Menu_EFFECT,
	AEGP_Menu_WINDOW,
	AEGP_Menu_FLOATERS,
	AEGP_Menu_KF_ASSIST,
	AEGP_Menu_IMPORT,
	AEGP_Menu_SAVE_FRAME_AS,
	AEGP_Menu_PREFS,
	AEGP_Menu_EXPORT,
	AEGP_Menu_ANIMATION,
	AEGP_Menu_PURGE,
	//the following menu options only valid for AE 12.0 and up
	AEGP_Menu_NEW
};
typedef A_LegacyEnumType AEGP_MenuID;


#define AEGP_MENU_INSERT_SORTED			(-2)
#define AEGP_MENU_INSERT_AT_BOTTOM		(-1)
#define AEGP_MENU_INSERT_AT_TOP			0


#define kAEGPCommandSuite				"AEGP Command Suite"
#define kAEGPCommandSuiteVersion1		1 /* frozen in AE 5.0 */

typedef struct AEGP_CommandSuite1 {

	SPAPI A_Err	(*AEGP_GetUniqueCommand)(
						AEGP_Command		*unique_commandP);		/* << */

	SPAPI A_Err	(*AEGP_InsertMenuCommand)(
						AEGP_Command		command,				/* >> */
						const A_char 		*nameZ,					/* >> */
						AEGP_MenuID 		menu_id,				/* >> */
						A_long 				after_itemL);			/* >> */

	SPAPI A_Err	(*AEGP_RemoveMenuCommand)(
						AEGP_Command		command);				/* >> */

	SPAPI A_Err	(*AEGP_SetMenuCommandName)(
						AEGP_Command		command,				/* >> */
						const A_char		*nameZ);				/* >> */

	SPAPI A_Err	(*AEGP_EnableCommand)(
						AEGP_Command		command);				/* >> */

	SPAPI A_Err	(*AEGP_DisableCommand)(
						AEGP_Command		command);				/* >> */

	SPAPI A_Err	(*AEGP_CheckMarkMenuCommand)(
						AEGP_Command		command,				/* >> */
						A_Boolean			checkB);				/* >> */

	SPAPI A_Err	(*AEGP_DoCommand)(
						AEGP_Command		command);				/* >> */

} AEGP_CommandSuite1;




/* -------------------------------------------------------------------- */


typedef struct {
	A_long				reservedAL[12];
} AEGP_ErrReportState;


enum {
	AEGP_GetPathTypes_PLUGIN = 0,		// (Not Implemented) The path to the executable of the plugin itself.
	AEGP_GetPathTypes_USER_PLUGIN,		// The suite specific location of user specific plugins.
	AEGP_GetPathTypes_ALLUSER_PLUGIN,	// The suite specific location of plugins shared by all users.
	AEGP_GetPathTypes_APP				// The After Effects exe or .app location.  Not plugin specific.
	};
typedef A_u_long AEGP_GetPathTypes;


#define kAEGPUtilitySuite				"AEGP Utility Suite"
#define kAEGPUtilitySuiteVersion6		13 /* frozen in AE 12.0 */

typedef struct AEGP_UtilitySuite6 {

	SPAPI A_Err	(*AEGP_ReportInfo)(										/* displays dialog with name of plugin followed by info string. See also: ReportInfoUnicode */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						const A_char			*info_stringZ);			/* >> */
	
	SPAPI A_Err	(*AEGP_ReportInfoUnicode)(								/* displays dialog with name of plugin followed by info string */
										  AEGP_PluginID			aegp_plugin_id,			/* >> */
										  const A_UTF16Char		*info_stringP);			/* >> */

	SPAPI A_Err	(*AEGP_GetDriverPluginInitFuncVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_GetDriverImplementationVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_StartQuietErrors)(
						AEGP_ErrReportState		*err_stateP);			/* << */

	SPAPI A_Err	(*AEGP_EndQuietErrors)(
						A_Boolean				report_quieted_errorsB,	/* >> currently reports last quieted error */
						AEGP_ErrReportState		*err_stateP);			/* >> */

	SPAPI A_Err (*AEGP_GetLastErrorMessage)(
						A_long					buffer_size,			/* >> size of character buffer */
						A_char					*error_string,					/* << */
						A_Err					*error_num);				/* << */

	SPAPI A_Err	(*AEGP_StartUndoGroup)(									/* MUST be balanced with call to AEGP_EndUndoGroup() */
						const A_char			*undo_nameZ);			/* >> */

	SPAPI A_Err	(*AEGP_EndUndoGroup)(void);

	SPAPI A_Err (*AEGP_RegisterWithAEGP)(
						AEGP_GlobalRefcon 		global_refcon,			/* >> global refcon passed in command handlers */
						const A_char			*plugin_nameZ,			/* >> name of this plugin. AEGP_MAX_PLUGIN_NAME_SIZE */
						AEGP_PluginID			*plugin_id);			/* << id for plugin to use in other AEGP calls */

	SPAPI A_Err (*AEGP_GetMainHWND)(
						void					*main_hwnd);			/* << */

	SPAPI A_Err (*AEGP_ShowHideAllFloaters)(
						A_Boolean				include_tool_palB);		/* >> */

	SPAPI A_Err (*AEGP_PaintPalGetForeColor)(
						AEGP_ColorVal			*fore_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalGetBackColor)(
						AEGP_ColorVal			*back_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalSetForeColor)(
						const AEGP_ColorVal		*fore_colorP);			/* >> */

	SPAPI A_Err (*AEGP_PaintPalSetBackColor)(
						const AEGP_ColorVal		*back_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalGetFillColor)(
						A_Boolean				*is_fill_color_definedPB,	/*  << */
                        AEGP_ColorVal			*fill_colorP); 				/* << only valid if is_fill_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalGetStrokeColor)(
						A_Boolean				*is_stroke_color_definedPB, /*  << */
                        AEGP_ColorVal			*stroke_colorP); 			/* << only valid if is_stroke_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalSetFillColor)(
						const AEGP_ColorVal		*fill_colorP);				/* >> */

	SPAPI A_Err (*AEGP_CharPalSetStrokeColor)(
						const AEGP_ColorVal		*stroke_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalIsFillColorUIFrontmost)(						/* Otherwise, StrokeColor is frontmost */
						A_Boolean				*is_fill_color_selectedPB);	/* << */

	SPAPI A_Err (*AEGP_ConvertFpLongToHSFRatio)(
						A_FpLong				numberF,					/* >> */
						A_Ratio					*ratioPR);					/* << */

	SPAPI A_Err (*AEGP_ConvertHSFRatioToFpLong)(
						A_Ratio					ratioR,						/* << */
						A_FpLong				*numberPF);					/* >> */

	// this routine is safe to call from the non-main
	// thread. It is asynchronous and will return before the idle handler is called.
	// The Suite routines to get this pointer are not
	// thread safe, therefore you need to save it off
	// in the main thread for use by the child thread.
	SPAPI A_Err (*AEGP_CauseIdleRoutinesToBeCalled)(void);


	// Determine if after effects is running in a mode where there is no
	// user interface, and attempting to interact with the user (via a modal dialog)
	// will hang the application.
	// This will not change during a run. Use it to optimize your plugin at startup
	// to not create a user interface and make AE launch faster, and not break
	// when running multiple instances of a service.
	SPAPI	A_Err  (*AEGP_GetSuppressInteractiveUI)(A_Boolean* ui_is_suppressedPB); // out

	// this call writes text to the console if one is available.  One is guaranteed to be available
	// if ui_is_suppressedB == true.
	// In general use the call AEGP_ReportInfo() as it will write to the console in
	// non-interactive modes, and use a dialog in interactive modes.
	SPAPI	A_Err  (*AEGP_WriteToOSConsole)(const A_char* textZ);				// in

	// this writes an entry into the debug log, or to the command line if launched
	// with the -debug flag.
	SPAPI	A_Err  (*AEGP_WriteToDebugLog)(const A_char* subsystemZ,	// in
								const A_char* event_typeZ,				// in
								const A_char * infoZ);					// in


	SPAPI	A_Err  (*AEGP_IsScriptingAvailable)(A_Boolean* outAvailablePB);

	// Execute a script.
	// The script text can either be in UTF-8, or the current
	// application encoding.
	// The result is the result string if OK. It is optional.
	// The error is the error string if an error occurred. It is optional.
	// the result and error are in the encoding specified by platform_encodingB
	SPAPI	A_Err  (*AEGP_ExecuteScript)(AEGP_PluginID			inPlugin_id,
										const A_char* inScriptZ,		// in
										const A_Boolean platform_encodingB,	// in
										AEGP_MemHandle* outResultPH0,
										AEGP_MemHandle* outErrorStringPH0);

	SPAPI A_Err (*AEGP_HostIsActivated)(A_Boolean	*is_activatedPB);

	SPAPI A_Err (*AEGP_GetPluginPlatformRef)(AEGP_PluginID	plug_id, void** plat_refPPV); // on the Mac, it is a CFBundleRef to your mach-o plugin or NULL for a CFM plug-in; on Windows it is set to NULL for now

	SPAPI A_Err (*AEGP_UpdateFontList)(void); // Rescan the system font list.  This will return quickly if the font list hasn't changed.
	
	// return a particular path associated with the plugin
	SPAPI A_Err (*AEGP_GetPluginPaths)( 
							AEGP_PluginID		aegp_plugin_id,			// >> which plugin we are talking about
							AEGP_GetPathTypes	path_type,				// >> which path type to retrieve
							AEGP_MemHandle		*unicode_pathPH);		// << UTF16 mem handle must be disposed with AEGP_FreeMemHandle

} AEGP_UtilitySuite6;



/* -------------------------------------------------------------------- */


#define kAEGPMathSuite				"AEGP Math Suite"
#define kAEGPMathSuiteVersion1		1	/* frozen AE 15.0 */

typedef struct AEGP_MathSuite1 {
	
	// Matrices
	// right-hand rule, Y down, origin in upper left corner of comp.
	
	SPAPI A_Err(*AEGP_IdentityMatrix4)(A_Matrix4  *matrixP);
	
	SPAPI A_Err(*AEGP_MultiplyMatrix4)(const A_Matrix4 *A, const A_Matrix4 *B, A_Matrix4     *resultP);
	
	SPAPI A_Err(*AEGP_Matrix3ToMatrix4)(const A_Matrix3 *A, A_Matrix4     *B);
	
	SPAPI A_Err(*AEGP_MultiplyMatrix4by3)(const A_Matrix4 *A, const A_Matrix3 *B, A_Matrix4     *resultP);
	
	SPAPI A_Err(*AEGP_MatrixDecompose4)(const A_Matrix4 *A, A_FloatPoint3* posVP, A_FloatPoint3* scaleVP, A_FloatPoint3* shearVP, A_FloatPoint3* rotVP);
	
} AEGP_MathSuite1;


/* -------------------------------------------------------------------- */



typedef struct _PF_OpaqueBlendingTables	*PF_EffectBlendingTables;

#define kAEGPColorSettingsSuite			"PF Color Settings Suite"
#define kAEGPColorSettingsSuiteVersion5	6	// frozen in AE 23.2; Add APIs to get OCIO Config file, Display and Working Color Space information

typedef struct AEGP_ColorSettingsSuite5 {

	 SPAPI	A_Err (*AEGP_GetBlendingTables)(
		PR_RenderContextH 		render_contextH,
		PF_EffectBlendingTables *blending_tables);
		
	SPAPI	A_Err (*AEGP_DoesViewHaveColorSpaceXform)(
		AEGP_ItemViewP	viewP,			// >>
		A_Boolean		*has_xformPB);	// <<
		
	SPAPI	A_Err (*AEGP_XformWorkingToViewColorSpace)(
		AEGP_ItemViewP	viewP,		// >>
		AEGP_WorldH		srcH,		// in
		AEGP_WorldH		dstH);		// out; must be the same size (can be the same as source)
		
	SPAPI	A_Err (*AEGP_GetNewWorkingSpaceColorProfile)(
		AEGP_PluginID		aegp_plugin_id,					// >>
		AEGP_CompH			compH,							// >> 
		AEGP_ColorProfileP	*color_profilePP);				// << caller must dispose with AEGP_DisposeColorProfile
		
	SPAPI	A_Err (*AEGP_GetNewColorProfileFromICCProfile)(
		AEGP_PluginID		aegp_plugin_id,		// >>
		A_long				icc_sizeL,			// >>	icc profile size
		const	void		*icc_dataPV,		// >>	icc profile
		AEGP_ColorProfileP	*color_profilePP);	// <<	builds AEGP_ColorProfile from icc profile; caller must dispose with AEGP_DisposeColorProfile
		
	SPAPI	A_Err (*AEGP_GetNewICCProfileFromColorProfile)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstColorProfileP	color_profileP,		// >>
		AEGP_MemHandle			*icc_profilePH);	// <<	extract icc profile from AEGP_ColorProfile; caller must dispose with AEGP_FreeMemHandle
		
	SPAPI	A_Err (*AEGP_GetNewColorProfileDescription)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstColorProfileP	color_profileP,		// >>
		AEGP_MemHandle			*unicode_descPH);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle 
		
	SPAPI	A_Err (*AEGP_DisposeColorProfile)(
		AEGP_ColorProfileP	color_profileP);	// >>
		
	SPAPI	A_Err (*AEGP_GetColorProfileApproximateGamma)(
		AEGP_ConstColorProfileP	color_profileP,		// >>
		A_FpShort				*approx_gammaP);	// <<
		
	SPAPI	A_Err (*AEGP_IsRGBColorProfile)(
		AEGP_ConstColorProfileP	color_profileP,		// <<
		A_Boolean				*is_rgbPB);			// >>
	
	SPAPI	A_Err (*AEGP_SetWorkingColorSpace)(
		AEGP_PluginID			aegp_plugin_id,
		AEGP_CompH				compH,				// >>
		AEGP_ConstColorProfileP	color_profileP);	// >>
		
	SPAPI	A_Err (*AEGP_IsOCIOColorManagementUsed)(
		AEGP_PluginID			aegp_plugin_id,					// >>
		A_Boolean				*is_OCIOColorManagementUsedPB);	// <<

	SPAPI	A_Err (*AEGP_GetOCIOConfigurationFile)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_MemHandle			*config_filePH);	// <<
	
	SPAPI	A_Err (*AEGP_GetOCIOConfigurationFilePath)(
		AEGP_PluginID			aegp_plugin_id,			// >>
		AEGP_MemHandle			*config_filePH);		// <<

	SPAPI	A_Err (*AEGPD_GetOCIOWorkingColorSpace)(
		AEGP_PluginID	aegp_plugin_id,						// >>
		AEGP_MemHandle	*ocio_working_colorspaceH);			// <<

	SPAPI	A_Err (*AEGPD_GetOCIODisplayColorSpace)(
		AEGP_PluginID	aegp_plugin_id,					// >>
		AEGP_MemHandle	*ocio_displayH,					// <<
		AEGP_MemHandle	*ocio_viewH);					// <<
} AEGP_ColorSettingsSuite5;


/* -------------------------------------------------------------------- */
/*
	Render Queue Suite
	Used to add, remove, and modify items in the reder queue.

  */

#define kAEGPRenderQueueSuite			"AEGP Render Queue Suite"
#define kAEGPRenderQueueSuiteVersion1	1 /* frozen in AE 5.0 */

enum {
	AEGP_RenderQueueState_STOPPED,
	AEGP_RenderQueueState_PAUSED,
	AEGP_RenderQueueState_RENDERING
};

typedef A_u_long AEGP_RenderQueueState;

enum {
	AEGP_RenderItemStatus_NONE = -2,

	AEGP_RenderItemStatus_WILL_CONTINUE,	//	-1
	AEGP_RenderItemStatus_NEEDS_OUTPUT, 	//	0
	AEGP_RenderItemStatus_UNQUEUED, 		//	1 ready to be rendered, but not included in the queue
	AEGP_RenderItemStatus_QUEUED, 			//	2 ready AND queued
	AEGP_RenderItemStatus_RENDERING,
	AEGP_RenderItemStatus_USER_STOPPED,
	AEGP_RenderItemStatus_ERR_STOPPED,
	AEGP_RenderItemStatus_DONE,

	AEGP_RenderItemStatus_LAST_PLUS_ONE
};

typedef	A_long	AEGP_RenderItemStatusType;

typedef struct AEGP_RenderQueueSuite1 {
	SPAPI A_Err (*AEGP_AddCompToRenderQueue)(
					AEGP_CompH	comp,		/* >> */
					const A_char*	pathZ);

	// not legal to go from STOPPED to PAUSED.
	SPAPI A_Err (*AEGP_SetRenderQueueState)(
					AEGP_RenderQueueState	state);	/* >> */

	SPAPI A_Err (*AEGP_GetRenderQueueState)(
					AEGP_RenderQueueState	*stateP); /* << */

} AEGP_RenderQueueSuite1;

/* -------------------------------------------------------------------- */

enum {
	AEGP_LogType_NONE = -1,
	AEGP_LogType_ERRORS_ONLY,
	AEGP_LogType_PLUS_SETTINGS,
	AEGP_LogType_PER_FRAME_INFO,
	AEGP_LogType_NUM_TYPES
};

typedef A_long AEGP_LogType;

enum {
	AEGP_Embedding_NONE = -1,
	AEGP_Embedding_NOTHING,
	AEGP_Embedding_LINK,
	AEGP_Embedding_LINK_AND_COPY,
	AEGP_Embedding_NUM_TYPES
};

typedef A_long AEGP_EmbeddingType;

enum {
	AEGP_PostRenderOptions_NONE = -1,
	AEGP_PostRenderOptions_IMPORT,
	AEGP_PostRenderOptions_IMPORT_AND_REPLACE_USAGE,
	AEGP_PostRenderOptions_SET_PROXY,
	AEGP_PostRenderOptions_NUM_OPTIONS
};

typedef A_long AEGP_PostRenderAction;

enum {
	AEGP_OutputType_NONE = 0,
	AEGP_OutputType_VIDEO = 1L << 0,
	AEGP_OutputType_AUDIO = 1L << 1,
	AEGP_OutputType_NUM_TYPES
};

typedef A_long AEGP_OutputTypes;

enum {
	AEGP_VideoChannels_NONE 	= -1,
	AEGP_VideoChannels_RGB,
	AEGP_VideoChannels_RGBA,
	AEGP_VideoChannels_ALPHA,
	AEGP_VideoChannels_NUMTYPES
};

typedef A_long AEGP_VideoChannels;

enum {
	AEGP_StretchQual_NONE = -1,

	AEGP_StretchQual_LOW,
	AEGP_StretchQual_HIGH,

	AEGP_StretchQual_NUMTYPES
};
typedef	A_long	AEGP_StretchQuality;

enum {
	AEGP_OutputColorType_STRAIGHT = -1,
	AEGP_OutputColorType_PREMUL
};

typedef A_long AEGP_OutputColorType;

#define kAEGPRQItemSuite				"AEGP Render Queue Item Suite"
#define kAEGPRQItemSuiteVersion4		5	/* frozen in AE 14.1 */

typedef struct AEGP_RQItemSuite4 {

	SPAPI A_Err	(*AEGP_GetNumRQItems)(
						A_long				*num_itemsPL);		/* << */

	/*	NOTE: 	All AEGP_RQItemRefH are invalidated by ANY
				re-ordering, addition or removal of render
				items. DO NOT CACHE THEM.
	*/

	SPAPI A_Err (*AEGP_GetRQItemByIndex)(
						A_long				rq_item_index,		/* >> */
						AEGP_RQItemRefH		*rq_item_refPH);	/* << */

	SPAPI A_Err (*AEGP_GetNextRQItem)(							/* Pass RQ_ITEM_INDEX_NONE for current_rq_itemH to get first RQItemH. */
						AEGP_RQItemRefH		current_rq_itemH,	/* >> */
						AEGP_RQItemRefH		*next_rq_itemH);	/* << */

	SPAPI A_Err (*AEGP_GetNumOutputModulesForRQItem)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_long				*num_outmodsPL);	/* << */

	SPAPI A_Err (*AEGP_GetRenderState)(
						AEGP_RQItemRefH				rq_itemH,	/* >> */
						AEGP_RenderItemStatusType	*statusP);	/* << */

	/*
		the following now returns:
		Err_PARAMETER if you try to call while AEGP_RenderQueueState != AEGP_RenderQueueState_STOPPED

		if that's okay then:
		Err_RANGE if you pass a status that is illegal in any case
		Err_PARAMETER if you try to pass a status that doesn't make sense right now (eg: trying to Que something for which you haven't set the output path)
	*/
	SPAPI A_Err (*AEGP_SetRenderState)(
						AEGP_RQItemRefH				rq_itemH,	/* >> */
						AEGP_RenderItemStatusType	status);	/* >> */

	SPAPI A_Err (*AEGP_GetStartedTime)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_Time				*started_timePT);	/* <<	Returns {0,1} if not started. */

	SPAPI A_Err (*AEGP_GetElapsedTime)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_Time				*render_timePT);	/* << 	Returns {0,1} if not rendered. */

	SPAPI A_Err	(*AEGP_GetLogType)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						AEGP_LogType		*logtypeP);			/* << */

	SPAPI A_Err	(*AEGP_SetLogType)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						AEGP_LogType		logtype);			/* << */

	SPAPI A_Err	(*AEGP_RemoveOutputModule)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						AEGP_OutputModuleRefH	outmodH);		/* >> */

	SPAPI A_Err (*AEGP_GetComment)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						AEGP_MemHandle			*unicodeH);		/* << */

	SPAPI A_Err (*AEGP_SetComment)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						const A_UTF16Char		*commentZ);		/* >> */

	SPAPI A_Err (*AEGP_GetCompFromRQItem)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						AEGP_CompH				*compPH);		/* << */

	SPAPI A_Err (*AEGP_DeleteRQItem)(
						AEGP_RQItemRefH			rq_itemH);		/* <> 	   UNDOABLE */

} AEGP_RQItemSuite4;


#define kAEGPOutputModuleSuite			"AEGP Output Module Suite"
#define kAEGPOutputModuleSuiteVersion4	4 /* frozen in AE 10.0 */

typedef struct AEGP_OutputModuleSuite4 {

	/*	NOTE: 	All AEGP_OutputModuleRefHs are invalidated by ANY
				re-ordering, addition or removal of output modules
				from a render item. DO NOT CACHE THEM.
	*/

	SPAPI A_Err (*AEGP_GetOutputModuleByIndex)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						A_long					outmod_indexL,		/* >> */
						AEGP_OutputModuleRefH	*outmodPH);			/* << */

	SPAPI A_Err	(*AEGP_GetEmbedOptions)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						AEGP_EmbeddingType		*embed_optionsP);	/* << */

	SPAPI A_Err	(*AEGP_SetEmbedOptions)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						AEGP_EmbeddingType		embed_options);		/* >> */

	SPAPI A_Err	(*AEGP_GetPostRenderAction)(
						AEGP_RQItemRefH			rq_itemH,				/* >> */
						AEGP_OutputModuleRefH	outmodH,				/* >> */
						AEGP_PostRenderAction	*post_render_actionP);	/* << */

	SPAPI A_Err	(*AEGP_SetPostRenderAction)(
						AEGP_RQItemRefH			rq_itemH,				/* >> */
						AEGP_OutputModuleRefH	outmodH,				/* >> */
						AEGP_PostRenderAction	post_render_action);	/* >> */

	SPAPI A_Err	(*AEGP_GetEnabledOutputs)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						AEGP_OutputTypes		*enabled_typesP);	/* << */

	SPAPI A_Err	(*AEGP_SetEnabledOutputs)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						AEGP_OutputTypes		enabled_types);		/* >> */

	SPAPI A_Err (*AEGP_GetOutputChannels)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						AEGP_VideoChannels		*output_channelsP);	/* << */

	SPAPI A_Err (*AEGP_SetOutputChannels)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						AEGP_VideoChannels		output_channels);	/* >> */

	SPAPI A_Err (*AEGP_GetStretchInfo)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						A_Boolean				*is_enabledPB,		/* << */
						AEGP_StretchQuality		*stretch_qualityP,	/* << */
						A_Boolean				*lockedPB);			/* << */

	SPAPI A_Err	(*AEGP_SetStretchInfo)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						A_Boolean				is_enabledB,		/* >> */
						AEGP_StretchQuality		stretch_quality);	/* >> */

	SPAPI A_Err	(*AEGP_GetCropInfo)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						A_Boolean				*is_enabledBP,		/* << */
						A_Rect					*crop_rectP);		/* << */

	SPAPI A_Err (*AEGP_SetCropInfo)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						A_Boolean				enableB,			/* >> */
						A_Rect					crop_rect);			/* >> */

	SPAPI A_Err (*AEGP_GetSoundFormatInfo)(
						AEGP_RQItemRefH			rq_itemH,				/* >> */
						AEGP_OutputModuleRefH	outmodH,				/* >> */
						AEGP_SoundDataFormat	*sound_format_infoP,	/* << */
						A_Boolean				*audio_enabledPB);		/* << */

	SPAPI A_Err (*AEGP_SetSoundFormatInfo)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						AEGP_SoundDataFormat	sound_format_info,	/* >> */
						A_Boolean				audio_enabledB);	/* >> */

	SPAPI A_Err	(*AEGP_GetOutputFilePath)(
						AEGP_RQItemRefH			rq_itemH,					/* >> */
						AEGP_OutputModuleRefH	outmodH,					/* >> */
						AEGP_MemHandle			*unicode_pathPH);			// << empty string if not specified. handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_SetOutputFilePath)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	outmodH,			/* >> */
						const A_UTF16Char		*pathZ);				// >> null terminated unicode path with platform separators

	SPAPI A_Err (*AEGP_AddDefaultOutputModule)(
						AEGP_RQItemRefH			rq_itemH,			/* >> */
						AEGP_OutputModuleRefH	*outmodPH);			/* << */

	SPAPI A_Err	(*AEGP_GetExtraOutputModuleInfo)(
						AEGP_RQItemRefH			rq_itemH,
						AEGP_OutputModuleRefH	outmodH,
						AEGP_MemHandle			*format_unicodePH, 		/* << handle of A_u_short, (contains null terminated UTF16 string) must be disposed with AEGP_FreeMemHandle */
						AEGP_MemHandle			*info__unicodePH,		/* << handle of A_u_short, (contains null terminated UTF16 string) must be disposed with AEGP_FreeMemHandle */
						A_Boolean				*is_sequenceBP,
						A_Boolean				*multi_frameBP);

} AEGP_OutputModuleSuite4;

/* -------------------------------------------------------------------- */

/**
 ** Canvas Suite
 ** Used by artisans to render layers
 **/

#define kAEGPCanvasSuite				"AEGP Canvas Suite"
#define kAEGPCanvasSuiteVersion8		14	/* frozen in AE 12.0*/


enum {
	AEGP_RenderHints_NONE = 0,
	AEGP_RenderHints_IGNORE_EXTENTS = 0x1,
	AEGP_RenderHints_NO_TRANSFER_MODE = 0x2		// prevents application of opacity & transfer mode; for RenderLayer calls
};
typedef A_u_long AEGP_RenderHints;


enum {
	AEGP_RenderReceiptStatus_INVALID	= 0,
	AEGP_RenderReceiptStatus_VALID,
	AEGP_RenderReceiptStatus_VALID_BUT_INCOMPLETE
};
typedef A_u_long	AEGP_RenderReceiptStatus;


enum {
	AEGP_BinType_NONE = -1,
	AEGP_BinType_2D   = 0,
	AEGP_BinType_3D   = 1
};
typedef A_long	AEGP_BinType;


typedef void * AEGP_PlatformWindowRef;



enum  {
	AEGP_DisplayChannel_NONE = 0,
	AEGP_DisplayChannel_RED,
	AEGP_DisplayChannel_GREEN,
	AEGP_DisplayChannel_BLUE,
	AEGP_DisplayChannel_ALPHA,
	AEGP_DisplayChannel_RED_ALT,
	AEGP_DisplayChannel_GREEN_ALT,
	AEGP_DisplayChannel_BLUE_ALT,
	AEGP_DisplayChannel_ALPHA_ALT,
	AEGP_DisplayChannel_NUM_ITEMS
};
typedef A_long AEGP_DisplayChannelType; // disk safe

enum {
	AEGP_RenderNumEffects_ALL_EFFECTS = -1
};

typedef A_short AEGP_NumEffectsToRenderType;


typedef struct AEGP_CanvasSuite8 {

	SPAPI A_Err	(*AEGP_GetCompToRender)(
						PR_RenderContextH 		render_contextH,   	/* >> */
						AEGP_CompH				*compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumLayersToRender)(
				const	PR_RenderContextH	render_contextH,		/* >> */
						A_long				*num_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_GetNthLayerContextToRender)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n,					/* >> */
						AEGP_RenderLayerContextH	*layer_contextPH);	/* << */

	SPAPI A_Err	(*AEGP_GetLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_LayerH					*layerPH);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerAndSubLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_LayerH					*layerPH,			/* << */
						AEGP_SubLayerIndex			*sublayerP);		/* << */

	/**
	 ** With collapsed geometrics "on" this gives the layer in the root comp
	 ** contining the layer context. With collapsed geometrics off
	 ** this is the same as AEGP_GetLayerFromLayerContext.
	 **
	 **/
	SPAPI A_Err	(*AEGP_GetTopLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,
						AEGP_RenderLayerContextH	layer_contextH,
						AEGP_LayerH					*layerPH);

	SPAPI A_Err	(*AEGP_GetCompRenderTime)(
						PR_RenderContextH	render_contextH,	/* >> */
						A_Time				*time,				/* << */
						A_Time				*time_step);

	SPAPI A_Err	(*AEGP_GetCompDestinationBuffer)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_CompH					compH,				/* >> */
						AEGP_WorldH					*dst);				/* << */

	SPAPI A_Err	(*AEGP_GetROI)(
						PR_RenderContextH			render_contextH,	/* <> */
						A_LegacyRect						*roiPR);			/* << */

	// for rendering the texture map of a layer
	SPAPI A_Err	(*AEGP_RenderTexture)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						AEGP_RenderHints			render_hints,			/* >> */
						A_FloatPoint				*suggested_scaleP0,		/* >> */
						A_FloatRect					*suggested_src_rectP0, 	/* >> */
						A_Matrix3					*src_matrixP0,			/* << */
						AEGP_WorldH					*dstPH);				/* <> */


	SPAPI A_Err	(*AEGP_DisposeTexture)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_WorldH					dstH0);				/* <> */

	SPAPI A_Err	(*AEGP_GetFieldRender)(
						PR_RenderContextH 			render_contextH,   	/* >> */
						PF_Field					*field);			/* << */

	// not thread safe on MacOS
	// only call when thread ID = 0
	SPAPI A_Err	(*AEGP_ReportArtisanProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							totalL);			/* >> */

	SPAPI A_Err	(*AEGP_GetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* >> */

	SPAPI A_Err	(*AEGP_IsBlankCanvas)(
						PR_RenderContextH			render_contextH,	/* >> */
						A_Boolean					*is_blankPB);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Matrix4						*transform);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerBounds)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_LegacyRect							*boundsP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderOpacity)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_FpLong						*opacityPF);		/* << */

	SPAPI A_Err (*AEGP_IsRenderLayerActive)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_Boolean						*activePB);			/* << */

	// set the layer index. If total > 0, set it too.
	SPAPI A_Err	(*AEGP_SetArtisanLayerProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							num_layersL);

	// for track mattes.
	// Returns a comp-size buffer, which must be disposed thru AEGP_Dispose in World suite
	SPAPI A_Err	(*AEGP_RenderLayerPlus)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						AEGP_WorldH					*render_bufferPH);	/* <<  must be disposed with AEGP_DisposeWorld */


	SPAPI A_Err	(*AEGP_GetTrackMatteContext)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH 	fill_contextH,		/* << */
						AEGP_RenderLayerContextH	*matte_contextPH);	/* >> */

	// new for 6.0 --get receipt with the returned texture
	// use receipt to determine if a subsequent call to render
	// this layer can be skipped (because the artisan cached it)
	SPAPI A_Err	(*AEGP_RenderTextureWithReceipt)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						AEGP_RenderHints			render_hints,			/* >> */
						AEGP_NumEffectsToRenderType	num_effectsS,			/* >>  number of effect to render, -1 for all */
						A_FloatPoint				*suggested_scaleP0,		/* >> */
						A_FloatRect					*suggested_src_rectP0, 	/* >> */
						A_Matrix3					*src_matrixP0,			/* << */
						AEGP_RenderReceiptH			*render_receiptPH, 	 	/* << must be disposed with AEGP_DisposeRenderReceipt */
						AEGP_WorldH					*dstPH);				/* << */



	SPAPI A_Err	(*AEGP_GetNumberOfSoftwareEffects)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						A_short						*num_software_effectsPS);

	SPAPI A_Err	(*AEGP_RenderLayerPlusWithReceipt)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						AEGP_RenderReceiptH			*render_receiptPH,  /* << must be disposed with AEGP_DisposeRenderReceipt */
						AEGP_WorldH					*render_bufferPH);	/* << */

	SPAPI A_Err	(*AEGP_DisposeRenderReceipt)(
						AEGP_RenderReceiptH			render_receiptH);	/* >> */


	/* modified for 7.0 - added num_effects to check against */
	SPAPI A_Err	(*AEGP_CheckRenderReceipt)(
						PR_RenderContextH			current_render_contextH,	/* in */
						AEGP_RenderLayerContextH	current_layer_contextH,		/* in */
						AEGP_RenderReceiptH			old_render_receiptH,		/* in */
						A_Boolean					check_geometricsB,			/* in */
						AEGP_NumEffectsToRenderType	num_effectsS,				/* in */
						AEGP_RenderReceiptStatus	*receipt_statusP);			/* out */


	/* new in 7.0 generate a receipt for a layer as asd if the first num_effectsS have been rendered */
	SPAPI A_Err	(*AEGP_GenerateRenderReceipt)(
						PR_RenderContextH			current_render_contextH,	/* >> */
						AEGP_RenderLayerContextH	current_layer_contextH,		/* >> */
						AEGP_NumEffectsToRenderType	num_effectsS,				/* in */
						AEGP_RenderReceiptH			*render_receiptPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumBinsToRender)(
				const	PR_RenderContextH	render_contextH,			/* >> */
						A_long				*num_bins_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_SetNthBin)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n);					/* >> */

	SPAPI A_Err	(*AEGP_GetBinType)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_BinType				*bin_typeP);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform2D3D)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Boolean						only_2dB,			/* >> */
					A_Matrix4						*transform);		/* << */


	// interactive artisan information
	// handle to the on-screen window
	SPAPI A_Err (*AEGP_GetPlatformWindowRef)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_PlatformWindowRef				*window_refP);		/* << */


	// the dsf src to frame scale factors
	SPAPI A_Err (*AEGP_GetViewportScale)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_FpLong							*scale_xPF,			/* << */
				A_FpLong							*scale_yPF);		/* << */


	// the dsf src to frame translate
	SPAPI A_Err (*AEGP_GetViewportOrigin)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_long								*origin_xPL,		/* << */
				A_long								*origin_yPL);		/* << */


	SPAPI A_Err (*AEGP_GetViewportRect)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_LegacyRect								*viewport_rectPR);	/* << */


	SPAPI A_Err (*AEGP_GetFallowColor)(
				const PR_RenderContextH				render_contextH,	/* >> */
				PF_Pixel8							*fallow_colorP);	/* << */

	SPAPI A_Err (*AEGP_GetInteractiveBuffer)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_WorldH							*buffer);			/* << */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboard)(
				const PR_RenderContextH				render_contextH,	/* in */
				A_Boolean							*checkerboard_onPB);/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardColors)(
				const PR_RenderContextH				render_contextH,			/* in */
				PF_Pixel							*checkerboard_color1P,		/* out */
				PF_Pixel							*checkerboard_color2P);		/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardSize)(
				const PR_RenderContextH				render_contextH,			/* in */
				A_u_long							*checkerboard_widthPLu,		/* out - width of square*/
				A_u_long							*checkerboard_heightPLu);	/* out - height of square*/

	SPAPI A_Err (*AEGP_GetInteractiveCachedBuffer)(
				const PR_RenderContextH				render_contextH,			/* >> */
				AEGP_WorldH							*buffer);					/* << */


	// should we call AEGP_RenderLayer or AEGP_RenderTexture
	SPAPI A_Err (*AEGP_ArtisanMustRenderAsLayer)(
				const PR_RenderContextH				render_contextH,			/* >> */
				AEGP_RenderLayerContextH			layer_contextH,
				A_Boolean							*use_render_texturePB);


	SPAPI A_Err (*AEGP_GetInteractiveDisplayChannel)(
				const PR_RenderContextH				render_contextH,			/* >> */
				AEGP_DisplayChannelType				*display_channelP);			/* << */

	SPAPI A_Err (*AEGP_GetInteractiveExposure)(
				const PR_RenderContextH				render_contextH,			/* >> */
				A_FpLong							*exposurePF);				/* << */


	SPAPI A_Err (*AEGP_GetColorTransform)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_Boolean							*cms_onB,
				A_u_long							*xform_keyLu,
				void								*xformP);


	SPAPI A_Err	(*AEGP_GetCompShutterTime)(
						PR_RenderContextH	render_contextH,	/* >> */
						A_Time				*shutter_time,		/* << */
						A_Time				*shutter_dur);

	// uses remapping if any
	SPAPI A_Err	(*AEGP_MapCompToLayerTime)(
						PR_RenderContextH			render_contextH,	/* in */
						AEGP_RenderLayerContextH	layer_contextH,		/* in*/
						const A_Time				*comp_timePT,		/* in */
						A_Time						*layer_timePT);		/* out */

} AEGP_CanvasSuite8;







/**
 ** Artisan utility suite
 **
 **/
#define kAEGPArtisanUtilSuite				"AEGP Artisan Util Suite"
#define kAEGPArtisanUtilSuiteVersion1		1 /* frozen in AE 5.0 */


typedef struct AEGP_ArtisanUtilSuite1 {


	SPAPI A_Err	(*AEGP_GetGlobalContextFromInstanceContext)(
		const PR_InstanceContextH		instance_contextH,			/* >> */
			  PR_GlobalContextH			*global_contextPH);			/* << */


	SPAPI A_Err	(*AEGP_GetInstanceContextFromRenderContext)(
		const PR_RenderContextH			render_contextH,			/* >> */
			  PR_InstanceContextH		*instance_contextPH);		/* << */



	SPAPI A_Err	(*AEGP_GetInstanceContextFromQueryContext)(
		const PR_QueryContextH			query_contextH,				/* >> */
			  PR_InstanceContextH		*instance_contextPH);		/* << */


	SPAPI A_Err	(*AEGP_GetGlobalData)(
		const PR_GlobalContextH			global_contextH,			/* >> */
			  PR_GlobalDataH			*global_dataPH);			/* << */


	SPAPI A_Err	(*AEGP_GetInstanceData)(
		const PR_InstanceContextH		instance_contextH,			/* >> */
			  PR_InstanceDataH			*instance_dataPH);			/* << */

	SPAPI A_Err	(*AEGP_GetRenderData)(
		const PR_RenderContextH			render_contextH,			/* >> */
			  PR_RenderDataH			*render_dataPH);			/* << */

} AEGP_ArtisanUtilSuite1;



#define kAEGPCameraSuite				"AEGP Camera Suite"
#define kAEGPCameraSuiteVersion2		2 /* frozen in AE 5.5 */

typedef struct AEGP_CameraSuite2 {

		SPAPI A_Err	(*AEGP_GetCamera)(
						PR_RenderContextH 		render_contextH,   	/* >> */
						const A_Time			*comp_timeP,		/* >> */
						AEGP_LayerH				*camera_layerPH);	/* << */

		SPAPI A_Err	(*AEGP_GetCameraType)(
						AEGP_LayerH				camera_layerH,		/* >> */
						AEGP_CameraType			*camera_typeP);		/* << */


		SPAPI A_Err (*AEGP_GetDefaultCameraDistanceToImagePlane)(
						AEGP_CompH				compH,				/* >> */
						A_FpLong				*dist_to_planePF);	/* << */

		//	If a camera is created using aegp, then you must set the film size units.
		// 	No default is provided.

		SPAPI A_Err	(*AEGP_GetCameraFilmSize)(
						AEGP_LayerH				camera_layerH,			/* >> */
						AEGP_FilmSizeUnits		*film_size_unitsP,		/* << */
						A_FpLong				*film_sizePF0);			/* << in pixels */

		SPAPI A_Err	(*AEGP_SetCameraFilmSize)(
						AEGP_LayerH				camera_layerH,			/* >> */
						AEGP_FilmSizeUnits		film_size_units,		/* >> */
						A_FpLong				*film_sizePF0);			/* >> in pixels */

} AEGP_CameraSuite2;

#define kAEGPLightSuite				"AEGP Light Suite"
#define kAEGPLightSuiteVersion2		2 /* frozen in AE 5.5 */

typedef struct AEGP_LightSuite2 {

	SPAPI A_Err	(*AEGP_GetLightType)(
						AEGP_LayerH				light_layerH,		/* >> */
						AEGP_LightType			*light_typeP);		/* << */

	SPAPI A_Err	(*AEGP_SetLightType)(
					AEGP_LayerH				light_layerH,			/* >> */
					AEGP_LightType			light_type);			/* >> */

} AEGP_LightSuite2;



/**
 ** Query Xform suite
 ** Called by artisans during a response to a Query
 **/

#define kAEGPQueryXformSuite				"AEGP QueryXform Suite"
#define kAEGPQueryXformSuiteVersion2		4 /* frozen in AE 6.0 */


/**
 ** the type of source or dst transformation wanted
 **/
enum {
	AEGP_Query_Xform_LAYER,
	AEGP_Query_Xform_WORLD,
	AEGP_Query_Xform_VIEW,
	AEGP_Query_Xform_SCREEN
};

typedef A_u_long AEGP_QueryXformType;



typedef struct AEGP_QueryXformSuite2 {

	SPAPI A_Err	(*AEGP_QueryXformGetSrcType)(
					PR_QueryContextH		query_contextH,		/* <> */
					AEGP_QueryXformType		*src_type);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetDstType)(
					PR_QueryContextH		query_contextH,		/* <> */
					AEGP_QueryXformType		*dst_type);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetLayer)(
						PR_QueryContextH	query_contextH,		/* <> */
						AEGP_LayerH			*layerPH);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetComp)(
						PR_QueryContextH	query_contextH,		/* <> */
						AEGP_CompH			*compPH);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetTransformTime)(
						PR_QueryContextH	query_contextH,		/* <> */
						A_Time				*time);				/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetViewTime)(
						PR_QueryContextH	query_contextH,		/* <> */
						A_Time				*time);				/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetCamera)(
						PR_QueryContextH	query_contextH,		/* <> */
						AEGP_LayerH			*camera_layerPH);	/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetXform)(
						PR_QueryContextH	query_contextH,		/* <> */
						A_Matrix4			*xform);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformSetXform)(
						PR_QueryContextH	query_contextH,		/* <> */
						A_Matrix4			*xform);			/* >> */

	SPAPI A_Err	(*AEGP_QueryWindowRef)(
						PR_QueryContextH	query_contextH,		/* <> */
						AEGP_PlatformWindowRef	*window_refP);	/* >> */

	SPAPI A_Err	(*AEGP_QueryWindowClear)(
						PR_QueryContextH		query_contextH,		/* <> */
						AEGP_PlatformWindowRef	*window_refP,		/* out */
						A_LegacyRect					*boundsPR);			/* out */

	SPAPI A_Err	(*AEGP_QueryFrozenProxy)(
						PR_QueryContextH		query_contextH,		/* <> */
						A_Boolean				*onPB);				/* out */

	SPAPI A_Err	(*AEGP_QuerySwapBuffer)(
						PR_QueryContextH		query_contextH,		/* <> */
						AEGP_PlatformWindowRef	*window_refP,		/* out */
						AEGP_WorldH				*dest_bufferp);		/* out */

	SPAPI A_Err	(*AEGP_QueryDrawProcs)(
						PR_QueryContextH		query_contextH,		/* <> */
						PR_InteractiveDrawProcs	*window_refP);		/* in */


	SPAPI A_Err	(*AEGP_QueryPrepareForLineDrawing)(
						PR_QueryContextH		query_contextH,		/* <> */
						AEGP_PlatformWindowRef	*window_refP,
						A_LegacyRect			*viewportP,
						A_LPoint				*originP,
						A_FloatPoint			*scaleP);		/* in */

	SPAPI A_Err	(*AEGP_QueryUnprepareForLineDrawing)(
						PR_QueryContextH		query_contextH,		/* <> */
						AEGP_PlatformWindowRef	*window_refP);		/* in */

	SPAPI A_Err	(*AEGP_QueryGetData)(
						PR_QueryContextH		query_contextH,		/* <> */
						A_long					i,					/* in */
						void					**dataPP);			/* out */

	SPAPI A_Err	(*AEGP_QuerySetData)(
						PR_QueryContextH		query_contextH,		/* <> */
						A_long					i,					/* in */
						void					*dataP);			/* in */


}AEGP_QueryXformSuite2;


/* -------------------------------------------------------------------- */



#define kAEGPCompositeSuite				"AEGP Composite Suite"
#define kAEGPCompositeSuiteVersion2		4 /* frozen in AE 10.0 */

typedef struct AEGP_CompositeSuite2 {

	SPAPI A_Err (*AEGP_ClearAlphaExceptRect)(
				A_Rect					*clipped_dest_rectPR, 	/* >> */
				PF_EffectWorld			*dstP);					/* <> */

	SPAPI A_Err  (*AEGP_PrepTrackMatte)(
				A_long				num_pix,			/* >> */
				A_Boolean			deepB,				/* >> */
		const	PF_Pixel			*src_mask,			/* >> */
				PF_MaskFlags		mask_flags,			/* >> */
				PF_Pixel			*dst_mask);			/* << */

	SPAPI A_Err (*AEGP_TransferRect)(
				PF_Quality				quality,			/* >> */
				PF_ModeFlags			m_flags,			/* >> */
				PF_Field				field,				/* >> */
		const	A_Rect					*src_rec,			/* >> */
		const	PF_EffectWorld			*src_world,			/* >> */
		const	PF_CompositeMode		*comp_mode,			/* >> */
				PF_EffectBlendingTables	blend_tablesP0,		/* >>, pass NULL to blend in workingspace*/
		const	PF_MaskWorld			*mask_world0,		/* >> */
				A_long					dest_x,				/* >> */
				A_long					dest_y,				/* >> */
				PF_EffectWorld			*dst_world);		/* <> */

	SPAPI A_Err (*AEGP_CopyBits_LQ) (
				PF_EffectWorld				*src_worldP,	/* >> */
				A_Rect 						*src_r,			/* pass NULL for whole world */
				A_Rect						*dst_r,			/* pass NULL for whole world */
				PF_EffectWorld				*dst_worldP);	/* <> */

	SPAPI A_Err (*AEGP_CopyBits_HQ_Straight) (
				PF_EffectWorld				*src,			/* >> */
				A_Rect 						*src_r,			/* pass NULL for whole world */
				A_Rect						*dst_r,			/* pass NULL for whole world */
				PF_EffectWorld				*dst);			/* <> */

	SPAPI A_Err (*AEGP_CopyBits_HQ_Premul) (
				PF_EffectWorld				*src,			/* >> */
				A_Rect 						*src_r,			/* pass NULL for whole world */
				A_Rect						*dst_r,			/* pass NULL for whole world */
				PF_EffectWorld				*dst);			/* <> */

} AEGP_CompositeSuite2;



/* -------------------------------------------------------------------- */

#define kAEGPIterateSuite				"AEGP Iterate Suite"
#define kAEGPIterateSuiteVersion2		2 /* frozen in AE 22.0 */

typedef struct AEGP_IterateSuite2 {

	SPAPI A_Err (*AEGP_GetNumThreads)(
			A_long	*num_threadsPL);


	SPAPI A_Err (*AEGP_IterateGeneric)(
			A_long			iterationsL,						/* >> */		// can be PF_Iterations_ONCE_PER_PROCESSOR
			void			*refconPV,							/* >> */
			A_Err			(*fn_func)(	void 	*refconPV,		/* >> */
										A_long 	thread_indexL,	/* >> */
										A_long 	i,				/* >> */
										A_long 	iterationsL));	/* >> */

} AEGP_IterateSuite2;

// 	Export the name of this function in your PiPL resource's EntryPoint

typedef A_Err (AEGP_PluginInitFuncPrototype)(
					struct SPBasicSuite		*pica_basicP,			/* >> */
					A_long				 	driver_major_versionL,	/* >> */
					A_long					driver_minor_versionL,	/* >> */
					AEGP_PluginID			aegp_plugin_id,			/* >> */
					AEGP_GlobalRefcon		*plugin_refconP);		/* << will be passed to all hooks! */

typedef AEGP_PluginInitFuncPrototype	*AEGP_PluginInitFunc;


/* -------------------------------------------------------------------- */

/** AEGPPFInterfaceSuite1

	These are basically wrappers for constructing various AEGP objects from
	the information available to an effect plug-in so that various other AEGP suites
	may be used.

	AEGP_GetEffectLayer				-- get AEGP_LayerH corresponding to layer that effect is applied to
	AEGP_GetNewEffectForEffect		-- get AEGP_EffectRefH corresponding to effect
	AEGP_ConvertEffectToCompTime	-- return comp time from time units passed to effect (layer time)
	AEGP_GetEffectCamera			-- get camera AEGP_LayerH which defines current 3D view
									-- NOTE : this may be null if no camera is defined

	AEGP_GetEffectCameraMatrix		-- use this to get the geometry for the camera.

	These may only be called during PF_Cmd_FRAME_SETUP, PF_Cmd_RENDER,
	and PF_Cmd_EVENT::PF_Event_DRAW
**/

#define kAEGPPFInterfaceSuite			"AEGP PF Interface Suite"
#define kAEGPPFInterfaceSuiteVersion1	1 /* frozen in AE 5.0 */


typedef struct AEGP_PFInterfaceSuite1 {

	SPAPI A_Err	(*AEGP_GetEffectLayer)(
								PF_ProgPtr			effect_pp_ref,			/* >> */
								AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetNewEffectForEffect)(								/* must be disposed using AEGP_DisposeEffect */
								AEGP_PluginID		aegp_plugin_id,			/* >> */
								PF_ProgPtr			effect_pp_ref,			/* >> */
								AEGP_EffectRefH		*effect_refPH);			/* << */

	SPAPI A_Err	(*AEGP_ConvertEffectToCompTime)(
								PF_ProgPtr			effect_pp_ref,			/* >> */
								A_long				what_timeL,				/* >> */
								A_u_long			time_scaleLu,			/* from PF_InData */
								A_Time				*comp_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetEffectCamera)(
								PF_ProgPtr			effect_pp_ref,			/* >> */
								const A_Time		*comp_timePT,			/* >> */
								AEGP_LayerH			*camera_layerPH);		/* << */

	SPAPI A_Err	(*AEGP_GetEffectCameraMatrix)(
								PF_ProgPtr			effect_pp_ref,			/* >> */
								const A_Time		*comp_timePT,			/* >> */
								A_Matrix4			*camera_matrixP,		/* <> */
								A_FpLong			*dist_to_image_planePF,	/* <> */
								A_short				*image_plane_widthPL,	/* <> */
								A_short				*image_plane_heightPL);	/* <> */
} AEGP_PFInterfaceSuite1;

//	PIN_FileSize
typedef	A_u_longlong	AEIO_FileSize;

#define kAEGPIOInSuite			"AEGP IO In Suite"
#define kAEGPIOInSuiteVersion5	6 /* frozen in AE 12 */

typedef struct AEGP_IOInSuite5 {

	SPAPI A_Err	(*AEGP_GetInSpecOptionsHandle)(
						AEIO_InSpecH	inH,					/* >> */
						void			**optionsPPV);			/* << */

	SPAPI A_Err	(*AEGP_SetInSpecOptionsHandle)(
						AEIO_InSpecH	inH,					/* >> */
						void			*optionsPV,				/* >> */
						void			**old_optionsPPV);		/* << */

	SPAPI A_Err	(*AEGP_GetInSpecFilePath)(
						AEIO_InSpecH	inH,					/* >> */
						AEGP_MemHandle	*unicode_pathPH);		// << handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_GetInSpecNativeFPS)(
						AEIO_InSpecH	inH,					/* >> */
						A_Fixed			*native_fpsP);			/* << */

	SPAPI A_Err	(*AEGP_SetInSpecNativeFPS)(
						AEIO_InSpecH	inH,					/* >> */
						A_Fixed			native_fps);			/* >> */

	SPAPI A_Err	(*AEGP_GetInSpecDepth)(
						AEIO_InSpecH	inH,					/* >> */
						A_short			*depthPS);				/* << */

	SPAPI A_Err	(*AEGP_SetInSpecDepth)(
						AEIO_InSpecH	inH,					/* >> */
						A_short			depthS);				/* >> */

	SPAPI A_Err	(*AEGP_GetInSpecSize)(
						AEIO_InSpecH	inH,					/* >> */
						AEIO_FileSize	*sizePL);				/* << */

	SPAPI A_Err	(*AEGP_SetInSpecSize)(
						AEIO_InSpecH	inH,					/* >> */
						AEIO_FileSize	sizeL);					/* >> */

	SPAPI A_Err	(*AEGP_GetInSpecInterlaceLabel)(
						AEIO_InSpecH		inH,				/* >> */
						FIEL_Label	 		*interlaceP);		/* << */

	SPAPI A_Err	(*AEGP_SetInSpecInterlaceLabel)(
						AEIO_InSpecH		inH,				/* >> */
						const FIEL_Label	*interlaceP);		/* << */

	SPAPI A_Err	(*AEGP_GetInSpecAlphaLabel)(
						AEIO_InSpecH			inH,			/* >> */
						AEIO_AlphaLabel			*alphaP);		/* << */

	SPAPI A_Err	(*AEGP_SetInSpecAlphaLabel)(
						AEIO_InSpecH			inH,			/* >> */
						const AEIO_AlphaLabel	*alphaP);		/* << */

	SPAPI A_Err	(*AEGP_GetInSpecDuration)(
						AEIO_InSpecH	inH,					/* >> */
						A_Time			*durationP);			/* << */

	SPAPI A_Err	(*AEGP_SetInSpecDuration)(
						AEIO_InSpecH	inH,					/* >> */
						const A_Time	*durationP);			/* >> */

	SPAPI A_Err	(*AEGP_GetInSpecDimensions)(
						AEIO_InSpecH	inH,					/* >> */
						A_long			*widthPL0,				/* << */
						A_long			*heightPL0);

	SPAPI A_Err	(*AEGP_SetInSpecDimensions)(
						AEIO_InSpecH	inH,					/* >> */
						A_long			widthL,					/* >> */
						A_long			heightL);				/* >> */

	SPAPI A_Err	(*AEGP_InSpecGetRationalDimensions)(
						AEIO_InSpecH				inH,		/* >> */
						const AEIO_RationalScale	*rs0,		/* << */
						A_long						*width0,	/* << */
						A_long						*height0,	/* << */
						A_Rect						*r0);		/* << */

	SPAPI A_Err	(*AEGP_GetInSpecHSF)(
						AEIO_InSpecH	inH,					/* >> */
						A_Ratio			*hsfP);					/* << */

	SPAPI A_Err	(*AEGP_SetInSpecHSF)(
						AEIO_InSpecH	inH,					/* >> */
						const A_Ratio	*hsfP);					/* >> */

	SPAPI A_Err	(*AEGP_GetInSpecSoundRate)(
						AEIO_InSpecH	inH,					/* >> */
						A_FpLong		*ratePF);				/* << */

	SPAPI A_Err	(*AEGP_SetInSpecSoundRate)(
						AEIO_InSpecH	inH,					/* >> */
						A_FpLong		rateF);					/* >> */

	SPAPI A_Err	(*AEGP_GetInSpecSoundEncoding)(
						AEIO_InSpecH		inH,				/* >> */
						AEIO_SndEncoding	*encodingP);		/* << */

	SPAPI A_Err	(*AEGP_SetInSpecSoundEncoding)(
						AEIO_InSpecH		inH,				/* >> */
						AEIO_SndEncoding	encoding);			/* >> */

	SPAPI A_Err	(*AEGP_GetInSpecSoundSampleSize)(
						AEIO_InSpecH		inH,				/* >> */
						AEIO_SndSampleSize	*bytes_per_sampleP);/* << */

	SPAPI A_Err	(*AEGP_SetInSpecSoundSampleSize)(
						AEIO_InSpecH		inH,				/* >> */
						AEIO_SndSampleSize	bytes_per_sample);	/* >> */

	SPAPI A_Err	(*AEGP_GetInSpecSoundChannels)(
						AEIO_InSpecH		inH,				/* >> */
						AEIO_SndChannels	*num_channelsP);	/* << */

	SPAPI A_Err	(*AEGP_SetInSpecSoundChannels)(
						AEIO_InSpecH		inH,				/* >> */
						AEIO_SndChannels	num_channels);		/* >> */

	SPAPI A_Err	(*AEGP_AddAuxExtMap)(
						const A_char		*extension,			/* >> */
						A_long				file_type,			/* >> */
						A_long				creator);			/* >> */

	// In case of RGB data, if there is an embedded icc profile, build AEGP_ColorProfile out of this icc profile using AEGP_GetNewColorProfileFromICCProfile and pass it to
	// AEGP_SetInSpecEmbeddedColorProfile, with profile description set to NULL.
	//
	// In case of non-RGB data, if there is an embedded non-RGB icc profile or you know the color space the data is in, pass its description as a null-terminated unicode string
	// to AEGP_SetInSpecEmbeddedColorProfile, with color profile set to NULL. Doing this disables color management UI that allows user to affect
	// profile choice in the application UI.
	//
	// If you are unpacking non-RGB data directly into working space (to get working space use AEGP_GetNewWorkingSpaceColorProfile), you are done.
	//
	// If you are unpacking non-RGB data into specific RGB color space, you must pass the profile describing this space to AEGP_SetInSpecAssignedColorProfile.
	// Otherwise, your RGB data will be incorrectly interpreted as being in working space.
	//
	// Either color profile or profile description should be NULL in AEGP_SetInSpecEmbeddedColorProfile. You cannot use both.
	SPAPI A_Err (*AEGP_SetInSpecEmbeddedColorProfile)(
						AEIO_InSpecH			inH,				// <<
						AEGP_ConstColorProfileP	color_profileP0,	// <<
						const	A_UTF16Char		*profile_descP0);	// <<	pointer to a null-terminated unicode string

	// Assign valid RGB profile to the footage
	SPAPI A_Err (*AEGP_SetInSpecAssignedColorProfile)(
						AEIO_InSpecH			inH,				// <<
						AEGP_ConstColorProfileP	color_profileP);	// <<
	
	
	SPAPI A_Err	(*AEGP_GetInSpecNativeStartTime)(
										   AEIO_InSpecH	inH,					/* >> */
										   A_Time		*startTimeP);			/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecNativeStartTime)(
										   AEIO_InSpecH	inH,					/* >> */
										   const A_Time	*startTimeP);			/* >> */
	
	SPAPI A_Err	(*AEGP_ClearInSpecNativeStartTime)(
											AEIO_InSpecH	inH);				/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecNativeDisplayDropFrame)(
										   AEIO_InSpecH	inH,						/* >> */
										   A_Boolean		*displayDropFrameBP);	/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecNativeDisplayDropFrame)(
										   AEIO_InSpecH	inH,						/* >> */
										   A_Boolean		displayDropFrameB);		/* >> */
	
	SPAPI A_Err	(*AEGP_SetInSpecStillSequenceNativeFPS)(
										   AEIO_InSpecH	inH,					/* >> */
										   A_Fixed		native_still_seq_fps);	/* >> */

} AEGP_IOInSuite5;

#define kAEGPIOOutSuite			"AEGP IO Out Suite"
#define kAEGPIOOutSuiteVersion5	8 /* frozen in AE 17.0 */

typedef struct AEGP_IOOutSuite5 {
	SPAPI A_Err	(*AEGP_GetOutSpecOptionsHandle)(
						AEIO_OutSpecH	outH,					/* >> */
						void			**optionsPPV);			/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecOptionsHandle)(
						AEIO_OutSpecH	outH,					/* >> */
						void			*optionsPV,				/* >> */
						void			**old_optionsPPVO);		/* <> */

	SPAPI A_Err	(*AEGP_GetOutSpecFilePath)(
						AEIO_OutSpecH	outH,					/* >> */
						AEGP_MemHandle	*unicode_pathPH,		// << handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
						A_Boolean		*file_reservedPB);		/* << If the file is reserved, do not create the file.
																	  Otherwise, multi-machine rendering can fail.
																	  If true, an empty file has already been created. */

	SPAPI A_Err	(*AEGP_GetOutSpecFPS)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Fixed			*native_fpsP);			/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecNativeFPS)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Fixed			native_fpsP);			/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecDepth)(
						AEIO_OutSpecH	outH,					/* >> */
						A_short			*depthPS);				/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecDepth)(
						AEIO_OutSpecH	outH,					/* >> */
						A_short			depthPS);				/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecInterlaceLabel)(
						AEIO_OutSpecH		outH,				/* >> */
						FIEL_Label			*interlaceP);		/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecInterlaceLabel)(
						AEIO_OutSpecH		outH,				/* >> */
						const FIEL_Label	*interlaceP);		/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecAlphaLabel)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_AlphaLabel		*alphaP);			/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecAlphaLabel)(
						AEIO_OutSpecH			outH,			/* >> */
						const AEIO_AlphaLabel	*alphaP);		/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecDuration)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Time			*durationP);			/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecDuration)(
						AEIO_OutSpecH	outH,					/* >> */
						const A_Time	*durationP);			/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecDimensions)(
						AEIO_OutSpecH	outH,					/* >> */
						A_long			*widthPL,				/* << */
						A_long			*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecHSF)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Ratio			*hsfP);					/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecHSF)(
						AEIO_OutSpecH	outH,					/* >> */
						const A_Ratio	*hsfP);					/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecSoundRate)(
						AEIO_OutSpecH	outH,					/* >> */
						A_FpLong		*ratePF);				/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecSoundRate)(
						AEIO_OutSpecH	outH,					/* >> */
						A_FpLong		rateF);					/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecSoundEncoding)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndEncoding	*encodingP);		/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecSoundEncoding)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndEncoding	encoding);			/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecSoundSampleSize)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndSampleSize	*bytes_per_sampleP);/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecSoundSampleSize)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndSampleSize	bytes_per_sample);	/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecSoundChannels)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndChannels	*num_channelsP);	/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecSoundChannels)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndChannels	num_channels);		/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecIsStill)(
						AEIO_OutSpecH		outH,				/* >> */
						A_Boolean			*is_stillPB);		/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecPosterTime)(
						AEIO_OutSpecH		outH,				/* >> */
						A_Time				*poster_timeP);		/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecStartFrame)(
						AEIO_OutSpecH		outH,				/* >> */
						A_long				*start_frameP);		/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecPullDown)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_Pulldown		*pulldownP);		/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecIsMissing)(
						AEIO_OutSpecH		outH,				/* >> */
						A_Boolean			*missingPB);		/* << */

	// see if you need to embed outspec's color profile as an icc profile
	SPAPI A_Err (*AEGP_GetOutSpecShouldEmbedICCProfile)(
						AEIO_OutSpecH		outH,				// >>
						A_Boolean			*embedPB);			// <<

	// query outspec's color profile
	SPAPI A_Err (*AEGP_GetNewOutSpecColorProfile)(
						AEGP_PluginID		aegp_plugin_id,		// >>
						AEIO_OutSpecH		outH,				// >>
						AEGP_ColorProfileP	*color_profilePP);	// <<	output color space; caller must dispose with AEGP_DisposeColorProfile

	// Fails if rq_itemP is not found.
	// This API would also fail if the outH is not a confirmed outH and is a copy.
	// e.g. if the Output Module settings dialog is Open.
	SPAPI A_Err	(*AEGP_GetOutSpecOutputModule)(
						AEIO_OutSpecH		outH,				/* >> */
						AEGP_RQItemRefH		*rq_itemP,			/* << */
						AEGP_OutputModuleRefH *om_refP);		/* << */
	
	SPAPI A_Err	(*AEGP_GetOutSpecStartTime)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Time			*outStartTimePT);		/* << */
	
	SPAPI A_Err	(*AEGP_GetOutSpecFrameTime)(					// relative to start time
						AEIO_OutSpecH	outH,					/* >> */
						A_Time			*outFrameTimePT);		/* << */
	
	SPAPI A_Err	(*AEGP_GetOutSpecIsDropFrame)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Boolean		*outIsDropFramePB);		/* << */


} AEGP_IOOutSuite5;



/*	This suite allows you to take advantage of going through AE Import Dialog
	and being treated as a native format type
*/

typedef	A_long	AE_FIM_ImportFlavorRef;

#define	AE_FIM_ImportFlavorRef_NONE	AEGP_Index_NONE

#define	AE_FIM_MAX_FLAVOR_NAME_LEN		63

enum {
	AE_FIM_ImportFlag_NONE 	= 	0x0,
	AE_FIM_ImportFlag_COMP	=	0x2
};
typedef	A_long	AE_FIM_ImportFlags;

enum {
	AE_FIM_SpecialAction_NONE				=	-1,
	AE_FIM_SpecialAction_DRAG_N_DROP_FILE	=	2
};
typedef	A_long	AE_FIM_SpecialAction;


typedef	struct	AE_FIM_RefconTag		*AE_FIM_Refcon;

typedef	struct	AE_FIM_ImportOptionsTag	*AE_FIM_ImportOptions;

// callbacks
typedef A_Err (*AE_FIM_ImportFileCB)(
							const A_UTF16Char	*pathZ,						// >> null terminated unicode path with platform separators
							AE_FIM_ImportOptions		imp_options,		/* >> 	opaque structure; in the future could be expanded with query functions*/
							AE_FIM_SpecialAction		action,				/* >> 	is it a special kind of import situation? */
							AEGP_ItemH					itemH,				/* >> 	meaning varies depending on AE_FIM_SpecialAction */
																			//		both for no special action and drag'n'drop it is
																			//		a folder where imported item should go
							AE_FIM_Refcon				refcon);			/* >> 	the client defines this and it is stored with import callbacks */



typedef A_Err (*AE_FIM_VerifyImportableCB)(
							const A_UTF16Char	*pathZ,						// >> null terminated unicode path with platform separators
							AE_FIM_Refcon		refcon,				/* >> 	the client defines this and it is stored with import callbacks */
							A_Boolean			*importablePB);		/* << */


typedef struct {
	AE_FIM_Refcon				refcon;		// points to whatever you want; stored and passed back with the callbacks
	AE_FIM_ImportFileCB			import_cb;
	AE_FIM_VerifyImportableCB	verify_cb;
} AE_FIM_ImportCallbacks;

#define	kAEGPFIMSuite			"AEGP File Import Manager Suite"
#define	kAEGPFIMSuiteVersion4	4 /* frozen in AE 17.0 */


typedef struct {
	SPAPI A_Err (*AEGP_RegisterImportFlavor)(
				const A_char 				*nameZ,		// format name you'd like to appear
													// in AE's Import Dialog Format pop-up
													// menu.
													// Limited to AE_FIM_MAX_IMPORT_FLAVOR_NAME_LEN.
													// Everything after that will be truncated.
				AE_FIM_ImportFlavorRef	*imp_refP);	// On return it is set to a valid opaque ref.
													// If error occured, it will be returned to
													// the caller and ref will be set to a special
													// value - AE_FIM_ImportFlavorRef_NONE.

	SPAPI A_Err (*AEGP_RegisterImportFlavorFileTypes)(
				AE_FIM_ImportFlavorRef	imp_ref,		// Received from AEGP_RegisterImportFlavor
				A_long					num_filekindsL,	// number of supported file types for this format
				const	AEIO_FileKind	*kindsAP,		// Array of supported file types for this format
				A_long					num_fileextsL,	// number of supported file exts for this format
				const	AEIO_FileKind	*extsAP);		// Array of supported file exts for this format


	SPAPI A_Err (*AEGP_RegisterImportFlavorImportCallbacks)(
				AE_FIM_ImportFlavorRef	imp_ref,			// Received from AEGP_RegisterImportFlavor
				AE_FIM_ImportFlags		single_flag,		// You can register callbacks only per single flag
															// this also registers the flag with the import flavor
				const	AE_FIM_ImportCallbacks	*imp_cbsP);	// Callbacks your format installs per each flag

	// optionally call once from AE_FIM_ImportFileCB.  This is used by the application when re-importing
	// from the render queue and replacing an existing item.
	SPAPI A_Err (*AEGP_SetImportedItem)(
			AE_FIM_ImportOptions		imp_options,		/* <> */
			AEGP_ItemH					imported_itemH);	/* >> */
	
	SPAPI A_Err (*AEGP_FileSequenceImportOptionsFromFIMImportOptions)(
			const AE_FIM_ImportOptions		imp_options,			/* >> */
			AEGP_FileSequenceImportOptions	*seq_import_optionsP);	/* << */

} AEGP_FIMSuite4;



/* --------------------------- Persistent Data Suite ------------------------------*/
/*
The persist data suite allows you to store persistant data with the application.

The data entries are accessed by SectionKey, ValueKey pairs. It is recommended
that plugins use their matchname as their SectionKey, or the prefix if using multiple
section names. THe available data types are void*, floating point numbers, and strings.

Void* unstructured data allows you to store any kind of data. You must pass in a size in
bytes along with the data.

String data supports the full 8 bit space, only 0x00 is reserved for string ending.
This makes them ideal for storing UTF-8 encoded strings, ISO 8859-1, and plain ASCII.
Both Section keys and Value keys are of this type.

FpLongs are stored with 6 decimal places of precision. There is no provision
for specifying a different precision.

Right now the only persistent data host is the application.

*/

#define	kAEGPPersistentDataSuite			"AEGP Persistent Data Suite"
#define	kAEGPPersistentDataSuiteVersion4	4 /* frozen in AE 12.0 */

typedef struct {
	// get a handle of the application blob,
	// modifying this will modify the application
	SPAPI A_Err (*AEGP_GetApplicationBlob)(
		AEGP_PersistentType		blob_type, 			/* >> new in AE 12 */
		AEGP_PersistentBlobH	*blobPH);			/* << */

	// section and value key management
	SPAPI A_Err (*AEGP_GetNumSections)(
						AEGP_PersistentBlobH	blobH,				/* >> */
						A_long					*num_sectionPL);	/* << */

	SPAPI A_Err (*AEGP_GetSectionKeyByIndex)(
						AEGP_PersistentBlobH	blobH,				/* >> */
						A_long					section_index,		/* >> */
						A_long					max_section_size,	/* >> */
						A_char					*section_keyZ);		/* << */

	SPAPI A_Err	(*AEGP_DoesKeyExist)(
						AEGP_PersistentBlobH	blobH,				/* >> */
						const A_char			*section_keyZ,		/* >> */
						const A_char			*value_keyZ,		/* >> */
						A_Boolean				*existsPB);			/* << */

	SPAPI A_Err (*AEGP_GetNumKeys)(
						AEGP_PersistentBlobH	blobH,				/* >> */
						const A_char			*section_keyZ,		/* >> */
						A_long					*num_keysPL);		/* << */

	SPAPI A_Err (*AEGP_GetValueKeyByIndex)(
						AEGP_PersistentBlobH	blobH,				/* >> */
						const A_char			*section_keyZ,		/* >> */
						A_long					key_index,			/* >> */
						A_long					max_key_size,		/* >> */
						A_char					*value_keyZ);		/* << */

	// data access and manipulation

	// For the entry points below, if a given key is not found,
	//	the default value is both written to the blobH and
	//	returned as the value; if no default is provided, a blank value will be written
	//	and returned

	SPAPI A_Err (*AEGP_GetDataHandle)(
						AEGP_PluginID			plugin_id,
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						AEGP_MemHandle			defaultH0,		/* >> never adopted, NULL means no default data */
						AEGP_MemHandle			*valuePH);		/* << newly allocated, owned by caller, NULL if would be zero sized handle */

	SPAPI A_Err (*AEGP_GetData)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						A_u_long				data_sizeLu,	/* >> bufPV & default must be this big, if pref isn't then the default will be used */
						const void				*defaultPV0,	/* >> NULL means all zeros for default */
						void					*bufPV);		/* << */

	SPAPI A_Err (*AEGP_GetString)(
						AEGP_PersistentBlobH	blobH,					/* >> */
						const A_char			*section_keyZ,			/* >> */
						const A_char			*value_keyZ,			/* >> */
						const A_char			*defaultZ0,				/* >> NULL means '\0' is the default */
						A_u_long				buf_sizeLu,				/* >> size of buffer. Behavior dependent on actual_buf_sizeLu0 */
						A_char					*bufZ,					/* << will be "" if buf_size is too small */
						A_u_long				*actual_buf_sizeLu0);	/* << actual size needed to store the buffer (includes terminating NULL). Pass NULL for error reporting if size mismatch.*/

	SPAPI A_Err (*AEGP_GetLong)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						A_long					defaultL,		/* >> */
						A_long					*valuePL);		/* << */

	SPAPI A_Err	(*AEGP_GetFpLong)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						A_FpLong				defaultF,		/* >> */
						A_FpLong				*valuePF);		/* << */

	SPAPI A_Err	(*AEGP_GetTime)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						const A_Time			*defaultPT0,	/* >> */
						A_Time					*valuePT);		/* << */
	
	SPAPI A_Err	(*AEGP_GetARGB)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						const PF_PixelFloat		*defaultP0,		/* >> */
						PF_PixelFloat			*valueP);		/* << */

	// setters
	SPAPI A_Err	(*AEGP_SetDataHandle)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						const AEGP_MemHandle	valueH);		/* >> not adopted */

	SPAPI A_Err	(*AEGP_SetData)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						A_u_long				data_sizeLu,	/* >> */
						const void				*dataPV);		/* >> */

	SPAPI A_Err	(*AEGP_SetString)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						const A_char			*strZ);			/* >> */

	SPAPI A_Err	(*AEGP_SetLong)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						A_long					valueL);		/* >> */

	SPAPI A_Err	(*AEGP_SetFpLong)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						A_FpLong				valueF);		/* >> */

	SPAPI A_Err	(*AEGP_SetTime)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						const A_Time			*valuePT);		/* >> */

	SPAPI A_Err	(*AEGP_SetARGB)(
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ,	/* >> */
						const PF_PixelFloat		*valueP);		/* >> */

	SPAPI A_Err	(*AEGP_DeleteEntry)(							/* no error if entry not found */
						AEGP_PersistentBlobH	blobH,			/* >> */
						const A_char			*section_keyZ,	/* >> */
						const A_char			*value_keyZ);	/* >> */

	SPAPI A_Err	(*AEGP_GetPrefsDirectory)(
						AEGP_MemHandle			*unicode_pathPH);		// << empty string if no file. handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

} AEGP_PersistentDataSuite4;


// AEGP_CollectionSuite1

#define	kAEGPCollectionSuite					"AEGP Collection Suite"
#define	kAEGPCollectionSuiteVersion2			2 /* frozen in AE 6.5 */

enum  {
	AEGP_CollectionItemType_NONE,

	AEGP_CollectionItemType_LAYER,
	AEGP_CollectionItemType_MASK,
	AEGP_CollectionItemType_EFFECT,
	AEGP_CollectionItemType_STREAM,
	AEGP_CollectionItemType_KEYFRAME,
	AEGP_CollectionItemType_MASK_VERTEX,
	AEGP_CollectionItemType_STREAMREF,

	AEGP_CollectionItemType_END,
	AEGP_CollectionItemType_BEGIN = AEGP_CollectionItemType_LAYER
};
typedef A_LegacyEnumType AEGP_CollectionItemType;

typedef struct {
	AEGP_LayerH	layerH;		/* comp derived from layerH */
} AEGP_LayerCollectionItem;

typedef struct {
	AEGP_LayerH		layerH;		/* containing layer */
	AEGP_MaskIndex	index;		/* index to layer. 	*/
}AEGP_MaskCollectionItem;

typedef struct {
	AEGP_LayerH			layerH;		/* containing layer		*/
	AEGP_EffectIndex	index;		/* index to the effect	*/
}AEGP_EffectCollectionItem;

enum  {
	AEGP_StreamCollectionItemType_NONE,
	AEGP_StreamCollectionItemType_LAYER,
	AEGP_StreamCollectionItemType_MASK,
	AEGP_StreamCollectionItemType_EFFECT,
	AEGP_StreamCollectionItemType_END,
	AEGP_StreamCollectionItemType_BEGIN = AEGP_StreamCollectionItemType_LAYER
};
typedef A_LegacyEnumType AEGP_StreamCollectionItemType;

typedef struct {
	AEGP_MaskCollectionItem		mask;
	AEGP_MaskStream				mask_stream;
} AEGP_MaskStreamCollectionItem;

typedef struct {
	AEGP_EffectCollectionItem	effect;
	A_long						param_index;
} AEGP_EffectStreamCollectionItem;

typedef struct {
	AEGP_LayerH						layerH;
	AEGP_LayerStream				layer_stream;
} AEGP_LayerStreamCollectionItem;

typedef struct {
	AEGP_StreamCollectionItemType	type;
	union {
		AEGP_LayerStreamCollectionItem	layer_stream;
		AEGP_MaskStreamCollectionItem	mask_stream;
		AEGP_EffectStreamCollectionItem	effect_stream;
	} u;
}AEGP_StreamCollectionItem;

typedef struct {
	AEGP_MaskCollectionItem	mask_sel;	/* the mask must be selected for a vertex to be selected */
	AEGP_VertexIndex		index;
}AEGP_MaskVertexCollectionItem;

typedef struct {
	AEGP_StreamCollectionItem	stream_coll;
	AEGP_KeyframeIndex	index;
}AEGP_KeyframeCollectionItem;

typedef struct {
	AEGP_CollectionItemType	type;
	// the union is not used for AEGP_CollectionItemType_STREAMREF
	union {
		AEGP_LayerCollectionItem		layer;
		AEGP_MaskCollectionItem			mask;
		AEGP_EffectCollectionItem		effect;
		AEGP_StreamCollectionItem		stream;
		AEGP_MaskVertexCollectionItem	mask_vertex;
		AEGP_KeyframeCollectionItem		keyframe;
	} u;

	AEGP_StreamRefH						stream_refH; // valid for all types
} AEGP_CollectionItemV2;

typedef struct {
	SPAPI A_Err (*AEGP_NewCollection)(									/* dispose with dispose collection */
						AEGP_PluginID	plugin_id,						/* >> */
						AEGP_Collection2H *collectionPH);				/* << */

	SPAPI A_Err (*AEGP_DisposeCollection)(
						AEGP_Collection2H collectionH);					/* >> */

	SPAPI A_Err	(*AEGP_GetCollectionNumItems)(							/* constant time */
						AEGP_Collection2H	collectionH,				/* >> */
						A_u_long		*num_itemsPL);					/* << */

	SPAPI A_Err	(*AEGP_GetCollectionItemByIndex)(						/* constant time */
						AEGP_Collection2H	collectionH,				/* >> */
						A_u_long			indexL,						/* >> */
						AEGP_CollectionItemV2	*collection_itemP);			/* << */

	SPAPI A_Err (*AEGP_CollectionPushBack)(								/* constant time */
						AEGP_Collection2H			collectionH,		/* <> */
						const AEGP_CollectionItemV2	*collection_itemP);	/* >> NOTE:	The passed AEGP_CollectionItemV2, as well as all the AEGP_StreamRefH's
																					it references, will be adopted by AE; DO NOT dispose of it! */

	SPAPI A_Err (*AEGP_CollectionErase)(								/* O(n) */
						AEGP_Collection2H	collectionH,				/* <> */
						A_u_long			index_firstL,				/* >> */
						A_u_long			index_lastL);				/* >> */

} AEGP_CollectionSuite2;


enum
{
	AEGP_WorldType_NONE,
	AEGP_WorldType_8,
	AEGP_WorldType_16,
	AEGP_WorldType_32
};

typedef A_long AEGP_WorldType;


#define	kAEGPWorldSuite					"AEGP World Suite"
#define	kAEGPWorldSuiteVersion3			3 /* frozen in AE 7.0 */

typedef struct {
	SPAPI A_Err (*AEGP_New)(
						AEGP_PluginID	plugin_id,			/* >> */
						AEGP_WorldType	type,				/* >> */
						A_long			widthL,				/* >> */
						A_long			heightL,			/* >> */
						AEGP_WorldH		*worldPH);			/* << */

	SPAPI A_Err (*AEGP_Dispose)(
						AEGP_WorldH		worldH);			/* >> */

	SPAPI A_Err (*AEGP_GetType)(
						AEGP_WorldH		worldH,				/* >> */
						AEGP_WorldType	*typeP);			/* << */

	SPAPI A_Err (*AEGP_GetSize)(
						AEGP_WorldH		worldH,				/* >> */
						A_long			*widthPL,			/* << */
						A_long			*heightPL);			/* << */

	SPAPI A_Err (*AEGP_GetRowBytes)(
						AEGP_WorldH		worldH,				/* >> */
						A_u_long			*row_bytesPL);	/* << */

	SPAPI A_Err (*AEGP_GetBaseAddr8)(
						AEGP_WorldH		worldH,				/* >> error if the worldH is not AEGP_WorldType_8 */
						PF_Pixel8		**base_addrP);		/* << */

	SPAPI A_Err (*AEGP_GetBaseAddr16)(
		AEGP_WorldH		worldH,				/* >> error if the worldH is not AEGP_WorldType_16 */
		PF_Pixel16		**base_addrP);		/* << */

	SPAPI A_Err (*AEGP_GetBaseAddr32)(
		AEGP_WorldH		worldH,				/* >> error if the worldH is not AEGP_WorldType_32 */
		PF_PixelFloat	**base_addrP);		/* << */

	SPAPI A_Err (*AEGP_FillOutPFEffectWorld)(				/*	Provided so you can use some of the PF routines with an AEGPWorld. Pass NULL as the ProgPtr to the PF routines.*/
						AEGP_WorldH		worldH,				/* >> */
						PF_EffectWorld	*pf_worldP);		/* << */

	SPAPI A_Err (*AEGP_FastBlur)(
						A_FpLong		radiusF,			/* >> */
						PF_ModeFlags	mode,				/* >> */
						PF_Quality		quality,			/* >> */
						AEGP_WorldH		worldH);			/* <>  only for user allocated worlds; not for checked-out frames which are read only */

	SPAPI A_Err (*AEGP_NewPlatformWorld)(
		AEGP_PluginID	plugin_id,			/* >> */
		AEGP_WorldType	type,				/* >> */
		A_long			widthL,				/* >> */
		A_long			heightL,			/* >> */
		AEGP_PlatformWorldH		*worldPH);			/* << */

	SPAPI A_Err (*AEGP_DisposePlatformWorld)(
		AEGP_PlatformWorldH		worldH);			/* >> */

	SPAPI A_Err (*AEGP_NewReferenceFromPlatformWorld)(
		AEGP_PluginID	plugin_id,			/* >> */
		AEGP_PlatformWorldH platform_worldH, // >>
		AEGP_WorldH		*worldPH);			/* << */


} AEGP_WorldSuite3;


/* AEGP_RenderOptionsSuite

*/

enum {
	AEGP_MatteMode_STRAIGHT = 0,
	AEGP_MatteMode_PREMUL_BLACK,
	AEGP_MatteMode_PREMUL_BG_COLOR
};
typedef A_long AEGP_MatteMode;

enum {
	AEGP_ChannelOrder_ARGB = 0,
	AEGP_ChannelOrder_BGRA
};
typedef A_char AEGP_ChannelOrder;

enum {
	AEGP_ItemQuality_DRAFT = 0,					/* footage only. perform faster decode at expense of quality and draft-quality deinterlacing. */
	AEGP_ItemQuality_BEST						/* footage only. perform full decode and resampled deinterlacing */
};
typedef A_char AEGP_ItemQuality;

#define	kAEGPRenderOptionsSuite					"AEGP Render Options Suite"
#define	kAEGPRenderOptionsSuiteVersion4			4 /* frozen in AE 10.5 */

typedef struct {
	// fills out
	// Time to 0
	// Time step to the frame duration
	// field render to none
	// depth is best resolution of item
	SPAPI A_Err (*AEGP_NewFromItem)(
					AEGP_PluginID		plugin_id,			/* >> */
					AEGP_ItemH			itemH,				/* >> */
					AEGP_RenderOptionsH	*optionsPH);		/* << */

	SPAPI A_Err (*AEGP_Duplicate)(
					AEGP_PluginID		plugin_id,			/* >> */
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_RenderOptionsH	*copyPH);			/* << */

	SPAPI A_Err (*AEGP_Dispose)(
					AEGP_RenderOptionsH	optionsH);			/* >> */

	SPAPI A_Err (*AEGP_SetTime)(							/* the render time */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_Time				time);				/* >> */

	SPAPI A_Err (*AEGP_GetTime)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Time				*timeP);			/* << */

	SPAPI A_Err (*AEGP_SetTimeStep)(						/* duration of the frame; important for motion blur. */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_Time				time_step);			/* >> */

	SPAPI A_Err (*AEGP_GetTimeStep)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Time				*timePT);			/* << */

	SPAPI A_Err (*AEGP_SetFieldRender)(						/* How fields are to be handled. */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					PF_Field			field_render);		/* >> */

	SPAPI A_Err (*AEGP_GetFieldRender)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					PF_Field			*field_renderP);	/* << */


	SPAPI A_Err (*AEGP_SetWorldType)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					AEGP_WorldType		type);				/* >> */

	SPAPI A_Err (*AEGP_GetWorldType)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_WorldType		*typeP);			/* << */


	// 1 == 100%
	// 2 == 50%
	// ...
	SPAPI A_Err (*AEGP_SetDownsampleFactor)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_short		x,							/* >> */
					A_short		y);							/* >> */

	SPAPI A_Err (*AEGP_GetDownsampleFactor)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_short				*xP,				/* >> */
					A_short				*yP);				/* << */

	SPAPI A_Err	(*AEGP_SetRegionOfInterest)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					const A_LRect		*roiP);				/* >>  {0,0,0,0} for all*/

	SPAPI A_Err (*AEGP_GetRegionOfInterest)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_LRect				*roiP);				/* << */

	SPAPI A_Err	(*AEGP_SetMatteMode)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					AEGP_MatteMode		mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMatteMode)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_MatteMode		*modeP);			/* << */

	SPAPI A_Err	(*AEGP_SetChannelOrder)(
					AEGP_RenderOptionsH optionsH,			/* <> */
					AEGP_ChannelOrder	channel_order);		/* >> */

	SPAPI A_Err (*AEGP_GetChannelOrder)(
					AEGP_RenderOptionsH optionsH,			/* >> */
					AEGP_ChannelOrder	*channelP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderGuideLayers)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Boolean			*will_renderPB);	/* << */

	SPAPI A_Err (*AEGP_SetRenderGuideLayers)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Boolean			render_themB);		/* >> */

	/* AEGP_ItemType_FOOTAGE can be decoded at different with different
	   quality levels. Ignore for other AEGP_ItemType
	   */

	SPAPI A_Err (*AEGP_GetRenderQuality)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_ItemQuality	*qualityP);			/* << */

	SPAPI A_Err (*AEGP_SetRenderQuality)(
					AEGP_RenderOptionsH	optionsH,		/* >> */
					AEGP_ItemQuality	quality);		/* >> */
} AEGP_RenderOptionsSuite4;


#define	kAEGPLayerRenderOptionsSuite					"AEGP Layer Render Options Suite"
#define	kAEGPLayerRenderOptionsSuiteVersion2			2 /* frozen in 13.5 */

typedef struct {
	// optionsPH must be disposed by calling code
	//
	// fills out
	// Time to the layer's current time
	// Time step to layer's frame duration
	// ROI to the layer's nominal bounds
	// EffectsToRender to "all"
	SPAPI A_Err (*AEGP_NewFromLayer)(
					AEGP_PluginID				plugin_id,		/* >> */
					AEGP_LayerH					layerH,			/* >> */
					AEGP_LayerRenderOptionsH	*optionsPH);	/* << */

	// optionsPH must be disposed by calling code
	// like AEGP_NewFromLayer, but sets EffectsToRender to be the index fof effectH
	SPAPI A_Err (*AEGP_NewFromUpstreamOfEffect)(
					AEGP_PluginID				plugin_id,		/* >> */
					AEGP_EffectRefH				effectH,		/* >> */
					AEGP_LayerRenderOptionsH	*optionsPH);	/* << */

	// optionsPH must be disposed by calling code
	// like AEGP_NewFromLayer, but sets EffectsToRender to include the effect output
	// THIS MAY ONLY BE CALLED FROM THE UI THREAD
	SPAPI A_Err (*AEGP_NewFromDownstreamOfEffect)(
												AEGP_PluginID				plugin_id,		/* >> */
												AEGP_EffectRefH				effectH,		/* >> */
												AEGP_LayerRenderOptionsH	*optionsPH);	/* << */

	// copyPH must be disposed by calling code
	SPAPI A_Err (*AEGP_Duplicate)(
					AEGP_PluginID				plugin_id,		/* >> */
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					AEGP_LayerRenderOptionsH	*copyPH);		/* << */

	SPAPI A_Err (*AEGP_Dispose)(
					AEGP_LayerRenderOptionsH	optionsH);		/* >> */

	SPAPI A_Err (*AEGP_SetTime)(								/* the render time */
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					A_Time						time);			/* >> */

	SPAPI A_Err (*AEGP_GetTime)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					A_Time				*timeP);				/* << */

	SPAPI A_Err (*AEGP_SetTimeStep)(							/* duration of the frame; important for motion blur. */
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					A_Time						time_step);		/* >> */

	SPAPI A_Err (*AEGP_GetTimeStep)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					A_Time						*timePT);		/* << */

	SPAPI A_Err (*AEGP_SetWorldType)(
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					AEGP_WorldType				type);			/* >> */

	SPAPI A_Err (*AEGP_GetWorldType)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					AEGP_WorldType				*typeP);		/* << */

	// 1 == 100%
	// 2 == 50%
	// ...
	SPAPI A_Err (*AEGP_SetDownsampleFactor)(
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					A_short						x,				/* >> */
					A_short						y);				/* >> */

	SPAPI A_Err (*AEGP_GetDownsampleFactor)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					A_short						*xP,			/* >> */
					A_short						*yP);			/* << */

	SPAPI A_Err	(*AEGP_SetMatteMode)(
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					AEGP_MatteMode				mode);			/* >> */

	SPAPI A_Err (*AEGP_GetMatteMode)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					AEGP_MatteMode				*modeP);		/* << */
} AEGP_LayerRenderOptionsSuite2;


#define	kAEGPRenderSuite					"AEGP Render Suite"
#define	kAEGPRenderSuiteVersion5			8 /* frozen in 13.5 */

typedef A_u_longlong AEGP_AsyncRequestId;

typedef A_Err (*AEGP_RenderSuiteCheckForCancel)(
					void 		*refcon,
					A_Boolean 	*cancelPB);

typedef A_Err (*AEGP_AsyncFrameReadyCallback)(
					AEGP_AsyncRequestId				request_id,				// this will be the AEGP_AsyncRequestId that was returned from AEGP_RenderAndCheckoutLayerFrame_Async
					A_Boolean						was_canceled,			// will be set to true if this request was canceled via a call to AEGP_CancelAsyncRequest
					A_Err							error,					// will be set to A_Err_NONE (0) if successful
					AEGP_FrameReceiptH 				receiptH,				// frame data (only if successful)
					AEGP_AsyncFrameRequestRefcon	refconP0);				// this is the AEGP_AsyncFrameRequestRefcon that was (optionally) passed in to AEGP_RenderAndCheckoutLayerFrame_Async

typedef struct {
	// IMPORTANT: use of this call on the UI thread should be considered DEPRECATED except in a very narrow  use case:
	// a) the plugin requires this frame to make database change (e.g. an auto color button on an effect)
	// b) the user explicitly requested this operation (e.g. this is not a passive redraw of UI because something else changed)
	// This is to avoid having poor interactivity while the user is doing other unrelated things.
	// Asynchronous APIs should be used on the UI thread except in this specific case
	// On the UI thread, if the frame takes longer than 2 seconds, a progress dialog will automatically open to allow the user to cancel
	// On a render thread, these calls will work as normal with no progress dialog

	// See IMPORTANT note above about using this synchronous call on the UI thread which is considered DEPRECATED behavior
	SPAPI A_Err (*AEGP_RenderAndCheckoutFrame)(
					AEGP_RenderOptionsH				optionsH,					/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_FrameReceiptH				*receiptPH);				/* << check in using AEGP_CheckinFrame to release memory */

		// render_plain_layer_frameB was confusing and is not the design we want going forward
		// eventually this will be replaced with new RenderOptions calls. Until that is available, this will behave as if render_plain_layer_frameB is "false"
		// (upstream effect render). If the "true" behavior for adjustment layer mask is desired, see kAEGPRenderSuiteVersion4 that still has this flag

	// See IMPORTANT note above about using this synchronous call on the UI thread which is considered DEPRECATED behavior
	SPAPI A_Err (*AEGP_RenderAndCheckoutLayerFrame)(
					AEGP_LayerRenderOptionsH			optionsH,					/* >> */
					AEGP_RenderSuiteCheckForCancel		cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon					cancel_function_refconP0,	/* >> optional */
					AEGP_FrameReceiptH					*receiptPH);				/* << check in using AEGP_CheckinFrame to release memory */

	SPAPI A_Err (*AEGP_RenderAndCheckoutLayerFrame_Async)(
					AEGP_LayerRenderOptionsH			optionsH,						/* >> */
					AEGP_AsyncFrameReadyCallback		callback,						/* >> this will be called when the frame is ready (guaranteed to be called unless AE is shutting down) */
					AEGP_AsyncFrameRequestRefcon		request_completion_refconP0,	/* >> optional.  if included, it will be passed into the above callback */
					AEGP_AsyncRequestId					*asyncRequestIdP);				/* << Id associated with frame request.  Can be used to cancel early */

	SPAPI A_Err (*AEGP_CancelAsyncRequest)(
					AEGP_AsyncRequestId					asyncRequestId);				/* >> */

	SPAPI A_Err (*AEGP_CheckinFrame)(
					AEGP_FrameReceiptH		receiptH);							/* >> */

	/*	This returns a read only world that is not-owned by the plugin.
		Call CheckinFrame to release the world when you are done reading from it.
	*/

	SPAPI A_Err (*AEGP_GetReceiptWorld)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					AEGP_WorldH				*worldPH);							/* << */

	SPAPI A_Err (*AEGP_GetRenderedRegion)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					A_LRect					*rendered_regionP);					/* << */

	SPAPI A_Err (*AEGP_IsRenderedFrameSufficient)(
						AEGP_RenderOptionsH rendered_optionsH,					/* >> */
						AEGP_RenderOptionsH proposed_optionsH,					/* >> */
						A_Boolean			*rendered_is_sufficientPB);			/* << */

	// See IMPORTANT note above about using this synchronous call on the UI thread which is considered DEPRECATED behavior
	SPAPI A_Err (*AEGP_RenderNewItemSoundData)( 								/* Works on Compositions and Footage items. */
					AEGP_ItemH						itemH,						/* >> */
					const A_Time					*start_timePT,				/* >> */
					const A_Time					*durationPT,				/* >> */
					const AEGP_SoundDataFormat		*sound_formatP,				/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_SoundDataH					*new_sound_dataPH);			/* << AEGP_SoundDataH must be disposed. Returns NULL if no audio */


	// returns the current timestamp of the project.this is increased any time something is touched in the project
	// that affects rendering
	SPAPI A_Err (*AEGP_GetCurrentTimestamp)(
							AEGP_TimeStamp * time_stampP); // out

	// Lets you know if the video of the item has changed since the input time stamp.
	// Is not affected by audio.
	SPAPI A_Err (*AEGP_HasItemChangedSinceTimestamp)(AEGP_ItemH itemH,					// in
													const A_Time * start_timeP,			// in
													const A_Time* durationP,			//in
													const AEGP_TimeStamp * time_stampP, //in
													A_Boolean * item_has_changedPB);	//out

	// checks whether this frame would be worth rendering externally and
	// checking in to the cache. a speculative renderer should check this twice:
	// (1) before sending the frame out to render
	// (2) when it is complete, before calling AEGP_NewPlatformWorld and checking in.
	// (don't forget to call AEGP_HasItemChangedSinceTimestamp also!)
	SPAPI A_Err (*AEGP_IsItemWorthwhileToRender)(	AEGP_RenderOptionsH roH,					// in
												const AEGP_TimeStamp* time_stampP,		// in
												A_Boolean *worthwhile_to_renderPB);	// out

	// ticks_to_render is the approximate amount of time needed to render the frame
	// on this machine. it is 60Hz.
	SPAPI A_Err (*AEGP_CheckinRenderedFrame)( AEGP_RenderOptionsH roH,					// in
												const AEGP_TimeStamp* time_stampP,		// in
												A_u_long ticks_to_renderL,				// in
												AEGP_PlatformWorldH	imageH);			// in (adopted)
												
	SPAPI A_Err (*AEGP_GetReceiptGuid) (AEGP_FrameReceiptH receiptH, 	// in
											AEGP_MemHandle *guidMH);	// out, must be disposed
} AEGP_RenderSuite5;



// AsyncManager render suite.  For render requests that are managed by AE rather than explicitly by plugin callbacks (as in some cases in AEGP_RenderSuite5+)
// As of 13.5 the only way to get an AsyncManager is via PF_GetContextAsyncManager in PF_EffectCustomUISuite2.  (This Async Manager is PF_Event_DRAW specific
// but there may be others in the future)

#define kAEGPRenderAsyncManagerSuite                       "AEGP Render Asyc Manager Suite"
#define kAEGPRenderAsyncManagerSuiteVersion1       1       /* frozen in 13.5 */


typedef struct {
	
	// An AsyncManager automatically handles possibly multiple async render requests
	// The first use of this in for async requests in CUSTOM_UI of PF_EventDRAW.  See PF_OutFlags2_CUSTOM_UI_ASYNC_MANAGER
	// The purpose_id is a unique constant number that helps the manager understand when it can automatically cancel
	// old requests (because they have the same purpose id and the RO has changed)
	
	SPAPI A_Err (*AEGP_CheckoutOrRender_ItemFrame_AsyncManager)( PF_AsyncManagerP  async_managerP,	// >> the async manager to ask for the render
																 A_u_long			purpose_id,		// >> a unique id to identify requests for the same usage (e.g. to hint cancellation of old)
																 AEGP_RenderOptionsH ro,			// >> the description of the item frame to render
																 AEGP_FrameReceiptH  *out_receiptPH );	// << on success, the rendered frame.  Can succeed and have no pixels (no world)
	
	SPAPI A_Err (*AEGP_CheckoutOrRender_LayerFrame_AsyncManager)( PF_AsyncManagerP  async_managerP,	// >> the async manager to ask for the render
																 A_u_long purpose_id,				// >> a unique id to identify requests for the same usage (e.g. to hint cancellation of old)
																 AEGP_LayerRenderOptionsH lro,		// >> the description of the layer frame to render
																 AEGP_FrameReceiptH    *out_receiptPH );  // << on success, the rendered frame.  Can succeed and have no pixels (no world)
	
	
	
} AEGP_RenderAsyncManagerSuite1;




#define	kAEGPTrackerSuite			"AEGP Tracker Suite"
#define	kAEGPTrackerSuiteVersion1	1    /* frozen in AE 6.0 */


typedef	struct {
	SPAPI A_Err (*AEGP_GetNumFeatures)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					A_long							*num_featuresPL);		/* << */

	SPAPI A_Err (*AEGP_GetFeatureRegionByIndex)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					PT_Index						index,					/* >> */
					A_FloatRect						*rectP);				/* << */

	SPAPI A_Err (*AEGP_GetSearchRegionByIndex)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					PT_Index						index,					/* >> */
					A_FloatRect						*rectP);				/* << */

	SPAPI A_Err (*AEGP_GetFeatureWorldByIndex)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					PT_Index						index,					/* >> */
					AEGP_WorldH						*feature_worldPH);		/* << */

	SPAPI A_Err (*AEGP_GetFrameWorld)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					AEGP_WorldH						*frame_worldPH);		/* << */

	SPAPI A_Err (*AEGP_GetTrackerSourceDimensions)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					A_long							*widthPL,				/* << */
					A_long							*heightPL);

	SPAPI A_Err (*AEGP_SetFeatureRegionByIndex)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					PT_Index						index,					/* >> */
					const	A_FloatRect				*rectP);				/* >> */

	SPAPI A_Err (*AEGP_SetAccuracyByIndex)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					PT_Index						index,					/* >> */
					A_FpLong						accuracyF);				/* >> */

	SPAPI A_Err (*AEGP_ShouldTrackFeature)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					PT_Index						index,					/* >> */
					A_Boolean						*trackPB);				/* << */

} AEGP_TrackerSuite1;


#define	kAEGPTrackerUtilitySuite			"AEGP Tracker Utility Suite"
#define	kAEGPTrackerUtilitySuiteVersion1	1 /* frozen in AE 6.0 */

typedef struct {
	SPAPI A_Err (*AEGP_HasUserCancelled)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					A_Boolean						*user_cancelledPB);		/* << */

	SPAPI A_Err (*AEGP_GetTrackerFromTrackerInstance)(
					const	PT_TrackerInstancePtr	tracker_instanceP,		/* >> */
					PT_TrackerPtr					*trackerPP);			/* << */

	SPAPI A_Err (*AEGP_GetTrackerInstanceFromTrackingContext)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					PT_TrackerInstancePtr			*tracker_instancePP);	/* << */

	SPAPI A_Err (*AEGP_GetGlobalData)(
					const	PT_TrackerPtr			trackerP,				/* >> */
					AEGP_MemHandle					*global_dataPH);		/* << */

	SPAPI A_Err (*AEGP_GetInstanceData)(
					const	PT_TrackerInstancePtr	tracker_instanceP,		/* >> */
					AEGP_MemHandle					*instance_dataPH);		/* << */	// currently has to be flat (no handles inside the handle)

	SPAPI A_Err (*AEGP_GetTrackData)(
					const	PT_TrackingContextPtr	contextP,				/* >> */
					AEGP_MemHandle					*track_dataPH);			/* << */
} AEGP_TrackerUtilitySuite1;



#define kAEGPRenderQueueMonitorSuite				"AEGP RenderQueue Monitor Suite"
#define kAEGPRenderQueueMonitorSuiteVersion1		1 /* frozen AE 11.0 */

typedef struct _AEGP_RQM_Refcon			*AEGP_RQM_Refcon;
typedef A_u_longlong					AEGP_RQM_SessionId;
typedef A_u_longlong					AEGP_RQM_ItemId;
typedef A_u_longlong					AEGP_RQM_FrameId;

typedef enum
{
	AEGP_RQM_FinishedStatus_UNKNOWN,
	AEGP_RQM_FinishedStatus_SUCCEEDED,
	AEGP_RQM_FinishedStatus_ABORTED,
	AEGP_RQM_FinishedStatus_ERRED
} AEGP_RQM_FinishedStatus;

typedef struct _AEGP_RQM_BasicData {
	const struct SPBasicSuite	*pica_basicP;
	A_long						aegp_plug_id;
	AEGP_RQM_Refcon				aegp_refconPV;
} AEGP_RQM_BasicData;

typedef struct _AEGP_RQM_FunctionBlock1 {
	A_Err (*AEGP_RQM_RenderJobStarted)(AEGP_RQM_BasicData *basic_dataP, AEGP_RQM_SessionId jobid);
	A_Err (*AEGP_RQM_RenderJobEnded)(AEGP_RQM_BasicData *basic_dataP, AEGP_RQM_SessionId jobid);
	A_Err (*AEGP_RQM_RenderJobItemStarted)(AEGP_RQM_BasicData *basic_dataP, AEGP_RQM_SessionId jobid, AEGP_RQM_ItemId itemid);
	A_Err (*AEGP_RQM_RenderJobItemUpdated)(AEGP_RQM_BasicData *basic_dataP, AEGP_RQM_SessionId jobid, AEGP_RQM_ItemId itemid, AEGP_RQM_FrameId frameid);
	A_Err (*AEGP_RQM_RenderJobItemEnded)(AEGP_RQM_BasicData *basic_dataP, AEGP_RQM_SessionId jobid, AEGP_RQM_ItemId itemid, AEGP_RQM_FinishedStatus fstatus);
	A_Err (*AEGP_RQM_RenderJobItemReportLog)(AEGP_RQM_BasicData *basic_dataP, AEGP_RQM_SessionId jobid, AEGP_RQM_ItemId itemid, A_Boolean isError, AEGP_MemHandle logbuf);
} AEGP_RQM_FunctionBlock1;

typedef struct AEGP_RenderQueueMonitorSuite1 {

	SPAPI A_Err	 (*AEGP_RegisterListener) (
						AEGP_PluginID					aegp_plugin_id,			/* >> */
						AEGP_RQM_Refcon					aegp_refconP,			/* >> */
						const AEGP_RQM_FunctionBlock1 *	fcn_blockP);			/* >> */

	SPAPI A_Err	 (*AEGP_DeregisterListener) (
						AEGP_PluginID					aegp_plugin_id,			/* >> */
						AEGP_RQM_Refcon					aegp_refconP);			/* >> */

	SPAPI A_Err	 (*AEGP_GetProjectName)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_MemHandle					*utf_project_namePH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	 (*AEGP_GetAppVersion)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_MemHandle					*utf_app_versionPH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	 (*AEGP_GetNumJobItems)(
						AEGP_RQM_SessionId 				sessid,					// >>
						A_long							*num_jobitemsPL);		// <<

	SPAPI A_Err	 (*AEGP_GetJobItemID)(
						AEGP_RQM_SessionId 				sessid,					// >>
						A_long							jobItemIndex,			// >>
						AEGP_RQM_ItemId					*jobItemID);			// <<

	SPAPI A_Err	 (*AEGP_GetNumJobItemRenderSettings)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_RQM_ItemId					itemid,					// >>
						A_long							*num_settingsPL);		// <<

	SPAPI A_Err	 (*AEGP_GetJobItemRenderSetting)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_RQM_ItemId					itemid,					// >>
						A_long							settingIndex,			// >>
						AEGP_MemHandle					*utf_setting_namePH0,	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
						AEGP_MemHandle					*utf_setting_valuePH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	 (*AEGP_GetNumJobItemOutputModules)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_RQM_ItemId					itemid,					// >>
						A_long							*num_outputmodulesPL);	// <<

	SPAPI A_Err	 (*AEGP_GetNumJobItemOutputModuleSettings)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_RQM_ItemId					itemid,					// >>
						A_long							outputModuleIndex,		// >>
						A_long							*num_settingsPL);		// <<

	SPAPI A_Err	 (*AEGP_GetJobItemOutputModuleSetting)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_RQM_ItemId					itemid,					// >>
						A_long							outputModuleIndex,		// >>
						A_long							settingIndex,			// >>
						AEGP_MemHandle					*utf_setting_namePH0,	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
						AEGP_MemHandle					*utf_setting_valuePH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	 (*AEGP_GetNumJobItemOutputModuleWarnings)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_RQM_ItemId					itemid,					// >>
						A_long							outputModuleIndex,		// >>
						A_long							*num_warningsPL);		// <<

	SPAPI A_Err	 (*AEGP_GetJobItemOutputModuleWarning)(
						AEGP_RQM_SessionId 				sessid,					// >>
						AEGP_RQM_ItemId					itemid,					// >>
						A_long							outputModuleIndex,		// >>
						A_long							warningIndex,			// >>
						AEGP_MemHandle					*utf_warning_valuePH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	 (*AEGP_GetNumJobItemFrameProperties)(
							AEGP_RQM_SessionId 				sessid,					// >>
							AEGP_RQM_ItemId					itemid,					// >>
							AEGP_RQM_FrameId				frameid,				// >>
							A_long							*num_propertiesPL);		// <<

	SPAPI A_Err	 (*AEGP_GetJobItemFrameProperty)(
							AEGP_RQM_SessionId 				sessid,					// >>
							AEGP_RQM_ItemId					itemid,					// >>
							AEGP_RQM_FrameId				frameid,				// >>
							A_long							propertyIndex,			// >>
							AEGP_MemHandle					*utf_property_namePH0,	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
							AEGP_MemHandle					*utf_property_valuePH0);// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	 (*AEGP_GetNumJobItemOutputModuleProperties)(
							AEGP_RQM_SessionId 				sessid,					// >>
							AEGP_RQM_ItemId					itemid,					// >>
							A_long							outputModuleIndex,		// >>
							A_long							*num_propertiesPL);		// <<

	SPAPI A_Err	 (*AEGP_GetJobItemOutputModuleProperty)(
							AEGP_RQM_SessionId 				sessid,					// >>
							AEGP_RQM_ItemId					itemid,					// >>
							A_long							outputModuleIndex,		// >>
							A_long							propertyIndex,			// >>
							AEGP_MemHandle					*utf_property_namePH0,	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
							AEGP_MemHandle					*utf_property_valuePH0);// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	 (*AEGP_GetJobItemFrameThumbnail)(
							AEGP_RQM_SessionId 				sessid,					// >>
							AEGP_RQM_ItemId					itemid,					// >>
							AEGP_RQM_FrameId				frameid,				// >>
							A_long							*widthPL,				// <> 	pass in the maximum width, returns the actual width
							A_long							*heightPL,				// <>	pass in the maximum height, returns the actual height
							AEGP_MemHandle					*thumbnailPH0);			// <<	handle of an image memory block in JPEG format

} AEGP_RenderQueueMonitorSuite1;



/* -------------------------------------------------------------------- */

typedef const void *PF_ConstPtr;
typedef const PF_ConstPtr *PF_ConstHandle;

#define kPFEffectSequenceDataSuite				"PF Effect Sequence Data Suite"
#define kPFEffectSequenceDataSuiteVersion1		1 /* frozen in 18.2 */

typedef struct PF_EffectSequenceDataSuite1 {
	
	SPAPI PF_Err	(*PF_GetConstSequenceData)(	PF_ProgPtr		effect_ref,
												PF_ConstHandle *sequence_data);	/* >> */

} PF_EffectSequenceDataSuite1;




#include <AE_GeneralPlugPost.h>


/****************************************************************/
/* include old versions of the suites							*/
/****************************************************************/
#include <AE_GeneralPlugOld.h>

#endif
