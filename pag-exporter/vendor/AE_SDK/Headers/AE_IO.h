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

#ifndef _AEIO_Public_H
#define _AEIO_Public_H

#include <A.h>
#include <AE_IO_FileExt.h>
#include <SPBasic.h>
#include <AE_Effect.h>
#include <FIEL_Public.h>

#include <adobesdk/config/PreConfig.h>


///////////////////////////////////////////////////
////////////////// PUBLIC ////////////////////////
//////////////////////////////////////////////////

#define AEIO_MAX_MODULE_NAME_LEN		31
#define AEIO_MAX_TYPES					16
#define AEIO_MAX_SEQ_NAME_LEN			31
#define AEIO_MAX_MESSAGE_LEN			127
#define AEIO_MAX_AUX_EXT				16

#define AEIO_ANY_CREATOR				-1

#ifdef __cplusplus
	extern "C" {
#endif

#ifndef AEGP_INTERNAL 
		typedef const struct _AEGP_MarkerVal			*AEGP_ConstMarkerValP;
#else
#ifndef _H_AE_GENERALPLUG_PRIVATE_H
#error include general plug private header before including AEIO
#endif
#endif



/**
 ** in data is passed to every function block function
 **/

typedef void (*AEIO_MessageFunc) (A_Err err_number, const A_char *msgA);

typedef struct AEIO_InData {
	AEIO_MessageFunc			msg_func;
	const struct SPBasicSuite	*pica_basicP;
	A_long						aegp_plug_id;
	void						*aegp_refconPV;
} AEIO_BasicData;

enum {
	AEIO_Err_UNSUPPORTED_CALLBACK = (((39)<<8)+1), 
	AEIO_Err_UNIMPLEMENTED,
	AEIO_Err_UNSUPPORTED_FILETYPE,
	AEIO_Err_INAPPROPRIATE_ACTION,
	AEIO_Err_BAD_BMWORLD,
	AEIO_Err_INCONSISTENT_PARAMETERS,
	AEIO_Err_INVALID_TIME,
	AEIO_Err_USE_DFLT_CALLBACK,
	AEIO_Err_USER_CANCEL,
	AEIO_Err_DISK_FULL,
	AEIO_Err_INITIALIZE_FAILED,
	AEIO_Err_BAD_FILENAME,
	AEIO_Err_PARSING,
	AEIO_Err_NOT_SEQUENCE,
	AEIO_Err_USE_DFLT_GETSIZES_FREESPACE
};

typedef struct _Up_OpaqueMem			**AEIO_Handle;

enum {
	AEIO_DFlags_NONE		=		0,
	AEIO_DFlags_DID_DEINT	= 		(1L<<1),	// I already did deinterlacing
	AEIO_DFlags_DID_ALPHA_CONV =	(1L<<2)	// I already did alpha conversion to what you wanted.
};
typedef A_long AEIO_DrawingFlags;

/* 	NOTE: AEIO_DFlags_NO_SCALE and AEIO_DFlags_DID_TIME_FILTER,
	previously included in the above, were never honored (so
	we removed them). -bbb 8/14/02
*/

enum {
	AEIO_Qual_LOW,
	AEIO_Qual_HIGH
};
typedef A_short	AEIO_Quality;

enum {
	AEIO_TimeDir_FORWARD 			= 0x0000,
	AEIO_TimeDir_BACKWARD 			= 0x0001,
	AEIO_TimeDir_INCLUDE_BASE_TIME 	= 0x1000
};
typedef A_short	AEIO_TimeDir;

enum {
	AEIO_IdleFlag_NONE 			= 0,
	AEIO_IdleFlag_PURGED_MEM	= 1L << 0,
	AEIO_IdleFlag_ADD_YOUR_OWN 	= 1L << 1
};
typedef A_long AEIO_IdleFlags;

enum {
	AEIO_SndQuality_APPROX = -1,		// This quality is used to draw the audio waveform.  -jja
	AEIO_SndQuality_LO = 0,
	AEIO_SndQuality_HI
};

typedef A_long AEIO_SndQuality;

enum {
	AEIO_E_UNSIGNED_PCM = 1,
	AEIO_E_SIGNED_PCM = 2,
	AEIO_E_SIGNED_FLOAT = 3
};
typedef A_short	AEIO_SndEncoding;

enum {
	AEIO_SS_1 = 1,
	AEIO_SS_2 = 2,
	AEIO_SS_4 = 4
};
typedef A_short	AEIO_SndSampleSize;

enum {
	AEIO_SndChannels_MONO = 1,
	AEIO_SndChannels_STEREO = 2
};
typedef A_short	AEIO_SndChannels;

enum {
	AEIO_Marker_URL_FLIP,
	AEIO_Marker_CHAPTER,
	AEIO_Marker_HOTSPOT,
	AEIO_Marker_NONE
};
typedef A_u_char AEIO_MarkerType;

enum {
	AEIO_Field_FRAME = 0,
	AEIO_Field_UPPER,
	AEIO_Field_LOWER
};
typedef A_long AEIO_Field;


typedef struct {
	const A_char		*chapterZ0;
	const A_char		*commentZ0;
	const A_char		*urlZ0;
	const A_char		*url_frame_targetZ0; // optionally used if urlZ0 != NULL
} AEIO_Marker;


enum {
	AEIO_FrameBlend_NONE = 0,
	AEIO_FrameBlend_LOW = 1,
	AEIO_FrameBlend_HIGH = 2
};
typedef A_char AEIO_FrameBlend;


enum {
	AEIO_LFlag_ALFA = 0x1,
	AEIO_LFlag_FIELDS = 0x2,
	AEIO_LFlag_SIGNATURE = 0x4
};
typedef A_long AEIO_LabelFlags;

enum {
	AEIO_InputDepth_1		= 1,
	AEIO_InputDepth_2  		= 2,
	AEIO_InputDepth_4  		= 4,
	AEIO_InputDepth_8  		= 8, 
	AEIO_InputDepth_16 		= 16,
	AEIO_InputDepth_24 		= 24, 
	AEIO_InputDepth_30		= 30,
	AEIO_InputDepth_32 		= 32, 
	AEIO_InputDepth_GRAY_2 	= 34,
	AEIO_InputDepth_GRAY_4 	= 36,
	AEIO_InputDepth_GRAY_8 	= 40,
	AEIO_InputDepth_48	 	= 48,
	AEIO_InputDepth_64	 	= 64,
	AEIO_InputDepth_96		= 96,		// RGB float
	AEIO_InputDepth_128		= 128,		// ARGB float
	AEIO_InputDepth_GRAY_16	= -16,		
	AEIO_InputDepth_GRAY_32	= -32
};

typedef A_long AEIO_InputDepth;

enum {
	AEIO_SupportedDepthFlags_NONE 			= 0,		
	AEIO_SupportedDepthFlags_DEPTH_1  		= 1L << 1,	// Enable 1 bit images. Put an interface on it, if you like
	AEIO_SupportedDepthFlags_DEPTH_2  		= 1L << 2,	// Enable 2 bit images. etc.
	AEIO_SupportedDepthFlags_DEPTH_4  		= 1L << 3,
	AEIO_SupportedDepthFlags_DEPTH_8  		= 1L << 4,
	AEIO_SupportedDepthFlags_DEPTH_16 		= 1L << 5,
	AEIO_SupportedDepthFlags_DEPTH_24 		= 1L << 6,
	AEIO_SupportedDepthFlags_DEPTH_32 		= 1L << 7,	// Enable  32-bit images (with alpha channels)
	AEIO_SupportedDepthFlags_DEPTH_GRAY_2 	= 1L << 8,	// greyscale 
	AEIO_SupportedDepthFlags_DEPTH_GRAY_4 	= 1L << 9,
	AEIO_SupportedDepthFlags_DEPTH_GRAY_8 	= 1L << 10,
	AEIO_SupportedDepthFlags_DEPTH_48		= 1L << 11,
	AEIO_SupportedDepthFlags_DEPTH_64		= 1L << 12,
	AEIO_SupportedDepthFlags_DEPTH_GRAY_16 	= 1L << 13,
	AEIO_SupportedDepthFlags_DEPTH_96		= 1L << 14,
	AEIO_SupportedDepthFlags_DEPTH_128		= 1L << 15,
	AEIO_SupportedDepthFlags_LAST	 		= 1L << 16
};

typedef A_long AEIO_SupportedDepthFlags;

enum {
	AEIO_Phase_NO_PULLDOWN = 0,
	AEIO_Phase_WSSWW = 1,
	AEIO_Phase_SSWWW,
	AEIO_Phase_SWWWS,
	AEIO_Phase_WWWSS,
	AEIO_Phase_WWSSW
};
typedef A_long AEIO_Pulldown;

typedef	A_long	AEIO_FileType;
typedef	A_long	AEIO_Creator;

typedef struct {
	AEIO_FileType type;
	AEIO_Creator creator;
} PFILE_FileKind;


typedef union {
	PFILE_FileKind		mac;
	AEIO_FileExt		ext;
	A_long				scrap;
} AEIO_FileKind;


typedef struct {
	A_char				name[AEIO_MAX_SEQ_NAME_LEN+1];			// usually the filename
	A_char				type[AEIO_MAX_SEQ_NAME_LEN+1];			// e.g. "PICT Sequence" "QT Movie"
	A_char				sub_type[AEIO_MAX_MESSAGE_LEN+1];
} AEIO_Verbiage;


#ifdef __cplusplus
	}
#endif


/////////////////////////////////////////////////////////
////////////////// redefined in PRIVATE /////////////////
////////////////////////////////////////////////////////

#ifdef A_INTERNAL
	#include <AE_IO_Private.h>
#else  
	#ifdef __cplusplus
		extern "C" {
	#endif

	typedef struct AEIO_SeqSpec		**AEIO_InSpecH;
	typedef struct AEIO_OutSpec		**AEIO_OutSpecH;

	typedef A_long	AEIO_ModuleSignature;				// please register your module signature with adobe
														// mailto:bbb@adobe.com?subject=AEIO_Signature

	typedef struct {
		A_Ratio	x;
		A_Ratio	y;
	} AEIO_RationalScale;


	#define AEIO_AlphaLabel_VERSION	0x0101

	enum {
		AEIO_AlphaPremul 		= 0x1,		// otherwise straight
		AEIO_AlphaInverted		= 0x2		//  255 = transparent
	};

	typedef A_u_long	AEIO_AlphaFlags;

	enum {
		AEIO_Alpha_STRAIGHT,
		AEIO_Alpha_PREMUL,
		AEIO_Alpha_IGNORE,
		AEIO_Alpha_NONE
	};

	typedef A_u_char	AEIO_AlphaType;

            
    #pragma pack( push, CoSAalign, 2 )

	typedef struct {
			A_short				version;
			AEIO_AlphaFlags		flags;
			A_u_char			red;		// color that was matted (if premul)
			A_u_char			green;
			A_u_char			blue;
			AEIO_AlphaType		alpha;	
		} AEIO_AlphaLabel;
#if defined(A_INTERNAL) && defined(__cplusplus)
	AE_STRUCT_SIZE_ASSERT(AEIO_AlphaLabel, 10);
#endif

    #pragma pack( pop, CoSAalign)


	typedef A_Err (*AEIO_AbortProc)(void *refcon);
	typedef A_Err (*AEIO_ProgressProc)(void *refcon, A_long count, A_long total);


	typedef struct {
		void				*refcon;
		AEIO_AbortProc		abort0;
		AEIO_ProgressProc	progress0;
	} AEIO_InterruptFuncs;


	//	All coordinates are in the scaled coordinate system.
	typedef struct {
		AEIO_Quality		qual;
		AEIO_AlphaLabel		alpha_label;
		AEIO_Field			field_request;
		AEIO_RationalScale	rs;
		A_Time				tr;
		A_Time				duration;
		AEIO_FrameBlend		time_filter;
		// CW adds 1 pad byte here
		A_LRect				required_region0;		// empty rect means entire
		// CW adds 2 pad bytes here
		AEIO_InterruptFuncs	inter;
	} AEIO_DrawFramePB;
	
	enum {
		AEIO_RenderMarkerFlag_NONE = 0x00000000,
		AEIO_RenderMarkerFlag_COMP = 0x00000001 // if 1 means comp marker; 0 means Layer marker
	};
	typedef A_long	AEIO_RenderMarkerFlag;

	#ifdef __cplusplus
	}
	#endif
#endif // A_INTERNAL

#ifdef __cplusplus
	extern "C" {
#endif


#define	AEIO_FileType_DIR		-2L
#define	AEIO_FileType_NONE		-1L
#define	AEIO_FileType_ANY		0L
#define AEIO_FileType_GENERIC	1L


////////////////////////////////////////////////////////////////////
///////////////////////// PUBLIC ///////////////////////////////////
////////////////////////////////////////////////////////////////////


enum {
	AEIO_MFlag_NONE						= 0,
	AEIO_MFlag_INPUT					= (1L<<0),	// input module
	AEIO_MFlag_OUTPUT					= (1L<<1),	// output module (can be both)
	AEIO_MFlag_FILE						= (1L<<2),	// direct correspondence to file in file system
	AEIO_MFlag_STILL					= (1L<<3),	// still image support (not VIDEO)
	AEIO_MFlag_VIDEO					= (1L<<4),	// multiple image support (not STILL)
	AEIO_MFlag_AUDIO					= (1L<<5),	// audio support
	AEIO_MFlag_NO_TIME					= (1L<<6),	// time-independent frame-store? always true if STILL; PICS example of non-STILL one
	AEIO_MFlag_INTERACTIVE_GET			= (1L<<7),	// user interaction for new seq, required if !FILE & INPUT
	AEIO_MFlag_INTERACTIVE_PUT			= (1L<<8),	// user interaction for new out, required if !FILE & OUTPUT
	AEIO_MFlag_CANT_CLIP				= (1L<<9),	// DrawFrame can't accept worlds smaller than requested dimensions
	AEIO_MFlag_MUST_INTERACT_PUT		= (1L<<10),	// dialog box can't be avoided, even if optionsH available
	AEIO_MFlag_CANT_SOUND_INTERLEAVE	= (1L<<11),	// all frames must be added, then sound
	AEIO_MFlag_CAN_ADD_FRAMES_NON_LINEAR= (1L<<12),	// AddFrame can handle non-sequential times
	AEIO_MFlag_HOST_DEPTH_DIALOG		= (1L<<13),	// expects host to bring up a dialog with depth
	AEIO_MFlag_HOST_FRAME_START_DIALOG	= (1L<<14),	// expects host to bring up dialog with starting frame #
	AEIO_MFlag_RESERVED1				= (1L<<15),	
	AEIO_MFlag_NO_OPTIONS				= (1L<<16),	// set this bit if the module does not accept output options
	AEIO_MFlag_RESERVED2				= (1L<<17),
	
	AEIO_MFlag_RESERVED3				= (1L<<18),	
	AEIO_MFlag_NO_PIXELS				= (1L<<19),	// this file format doesn't store real pixels, only geometry (or whatever)
	AEIO_MFlag_SEQUENCE_OPTIONS_OK		= (1L<<20),	// this module should take the options of its parent sequence when a folder is selected
	AEIO_MFlag_INPUT_OPTIONS			= (1L<<21),	// this module has user options associated with an input sequence. NOTE: options must be flat.
	AEIO_MFlag_HSF_AWARE			  	= (1L<<22),	// this module set the hsf for incoming sequences -> don't guess what it is!
	AEIO_MFlag_HAS_LAYERS				= (1L<<23),	// this module supports multiple layers in a single document
	AEIO_MFlag_SCRAP					= (1L<<24),	// module has a memory parsing (clipboard) component
	AEIO_MFlag_NO_UI					= (1L<<25),	// don't show any UI for this module
	AEIO_MFlag_SEQ_OPTIONS_DLG			= (1L<<26),	// module has sequence options
	AEIO_MFlag_HAS_AUX_DATA				= (1L<<27),	// has depth, or normals, or anything that is on a per pixel basis besides color
	AEIO_MFlag_HAS_META_DATA			= (1L<<28),	// supports user definable metadata
	AEIO_MFlag_CAN_DO_MARKERS			= (1L<<29),	// supports markers (e.g. URL flips, chapters)
	AEIO_MFlag_CAN_DRAW_DEEP			= (1L<<30),	// module can draw into 64 bpp BM_Worlds
	AEIO_MFlag_RESERVED4				= (1L<<31)	
};
typedef A_u_long AEIO_ModuleFlags;

enum {
	AEIO_MFlag2_NONE = 0,
	AEIO_MFlag2_AUDIO_OPTIONS					= (1L<<0),		// has audio options
	AEIO_MFlag2_RESERVED1						= (1L<<1),
	AEIO_MFlag2_SEND_ADDMARKER_BEFORE_ADDFRAME	= (1L<<2),		// otherwise it is sent just after; use with AEIO_MFlag_CAN_DO_MARKERS
	AEIO_MFlag2_CAN_DO_MARKERS_2				= (1L<<3),		// supports combined markers (e.g. URL flips, chapters, comments)
	AEIO_MFlag2_CAN_DRAW_FLOAT					= (1L<<4),
	AEIO_MFlag2_RESERVED2						= (1L<<5),
	AEIO_MFlag2_CAN_DO_AUDIO_32					= (1L<<6),		// supports 32 bit audio output.
	AEIO_MFlag2_RESERVED3						= (1L<<7),
	AEIO_MFlag2_SUPPORTS_ICC_PROFILES			= (1L<<8),
	AEIO_MFlag2_CAN_DO_MARKERS_3				= (1L<<9),		// supports cue points
	AEIO_MFlag2_SEND_ADDMARKER_BEFORE_STARTADDING = (1L<<10),
	AEIO_MFlag2_RESERVED4					  = (1L<<11),			
	AEIO_MFlag2_USES_QUICKTIME				  = (1L<<12),			// Module uses quicktime for decompression or compression
	AEIO_MFlag2_RESERVED5					  = (1L<<13),
	AEIO_MFlag2_RESERVED6					  = (1L<<14),
	AEIO_MFlag2_RESERVED7					  = (1L<<15),
	AEIO_MFlag2_RESERVED8					  = (1L<<16),
	AEIO_MFlag2_RESERVED9					  = (1L<<17),
	AEIO_MFlag2_RESERVED10					  = (1L<<18)
};
typedef A_u_long AEIO_ModuleFlags2;

typedef struct {
	A_char extension[4]; // includes '.'
	A_long type;		 // mac type
	A_long creator;		 // mac creator
} AEIO_AuxExt;

typedef struct {	
	AEIO_ModuleSignature	sig;							// please register number this with adobe
	A_char					name[AEIO_MAX_MODULE_NAME_LEN+1];
	AEIO_ModuleFlags		flags;
	AEIO_ModuleFlags2		flags2;
	A_long					max_width;
	A_long					max_height;
	A_short					num_filetypes;					// # of filetype/creator pairs supported
	A_short					num_extensions;					// # of .XXX extensions supported
	A_short					num_clips;						// # of clipboard types supported
	A_short					pad;
	PFILE_FileKind			create_kind;					// type/creator for newly created files
	AEIO_FileExt			create_ext;						// extension for new created DOS files
	AEIO_FileKind			read_kinds[AEIO_MAX_TYPES];		// mac types first, then DOS, then clipboard
	A_short					num_aux_extensionsS;
	// CW adds 2 pad bytes here
	AEIO_AuxExt				aux_ext[AEIO_MAX_AUX_EXT];
} AEIO_ModuleInfo;

typedef struct AEIO_OptionsCBInfo {
	AEIO_SupportedDepthFlags 	o_flags;
	AEIO_OutSpecH		out_specH;
} AEIO_OptionsCBInfo;

typedef struct {
		AEIO_Quality		qual;
		// CW adds 2 pad bytes here
		AEIO_RationalScale	rs;
		A_Time				tr;
		A_Time				duration;
		A_LRect				required_region;		// empty rect means entire
		AEIO_InterruptFuncs	inter;
} AEIO_DrawSparseFramePB;


/***********************   plugin entry points  *****************************
 ** the main routine of each plugin fills in these function pointers
 ** AE will call them as appropriate
 ****************************************************************************/

typedef struct _AEIO_FunctionBlock4 { // revved to 4 in AE 10.  Frozen in AE10.

	A_Err	(*AEIO_InitInSpecFromFile)(
		AEIO_BasicData		*basic_dataP,
		const A_UTF16Char	*file_pathZ,		// Null terminated UTF16 string with platform separators
		AEIO_InSpecH		inH);

	A_Err	(*AEIO_InitInSpecInteractive)(
		AEIO_BasicData	*basic_dataP,
		AEIO_InSpecH	inH);

	A_Err	(*AEIO_DisposeInSpec)(
		AEIO_BasicData	*basic_dataP,
		AEIO_InSpecH	inH);

	A_Err	(*AEIO_FlattenOptions)(
		AEIO_BasicData	*basic_dataP,
		AEIO_InSpecH	inH,
		AEIO_Handle		*flat_optionsPH);		

	A_Err	(*AEIO_InflateOptions)(
		AEIO_BasicData	*basic_dataP,
		AEIO_InSpecH	inH,
		AEIO_Handle		flat_optionsH);		

	A_Err	(*AEIO_SynchInSpec)(
		AEIO_BasicData	*basic_dataP,
		AEIO_InSpecH	inH, 
		A_Boolean		*changed0);

	A_Err	(*AEIO_GetActiveExtent)(
		AEIO_BasicData		*basic_dataP,
		AEIO_InSpecH		inH,				/* >> */
		const	A_Time		*tr,				/* >> */
		A_LRect				*extent);			/* << */

	A_Err	(*AEIO_GetInSpecInfo)(
		AEIO_BasicData		*basic_dataP,
		AEIO_InSpecH		inH, 
		AEIO_Verbiage		*verbiageP);

	//	All coordinates are in the scaled coordinate system.
	A_Err	(*AEIO_DrawSparseFrame)(
		AEIO_BasicData					*basic_dataP,
		AEIO_InSpecH					inH,
		const AEIO_DrawSparseFramePB	*sparse_framePPB, 
		PF_EffectWorld					*worldP,
		AEIO_DrawingFlags				*draw_flagsP);

	A_Err	(*AEIO_GetDimensions)(
		AEIO_BasicData	*basic_dataP,
		AEIO_InSpecH			 inH, 
		const AEIO_RationalScale *rs0,
		A_long					 *width0, 
		A_long					 *height0);

	A_Err	(*AEIO_GetDuration)(
		AEIO_BasicData		*basic_dataP,
		AEIO_InSpecH		inH, 
		A_Time				*tr);

	A_Err	(*AEIO_GetTime)(
		AEIO_BasicData		*basic_dataP,
		AEIO_InSpecH		inH, 
		A_Time				*tr);

	A_Err	(*AEIO_GetSound)(
		AEIO_BasicData				*basic_dataP,	
		AEIO_InSpecH				inH,
		AEIO_SndQuality				quality,
		const AEIO_InterruptFuncs 	*interrupt_funcsP0,	
		const A_Time				*startPT,	
		const A_Time				*durPT,	
		A_u_long					start_sampLu,
		A_u_long					num_samplesLu,
		void						*dataPV);

	A_Err	(*AEIO_InqNextFrameTime)(
		AEIO_BasicData			*basic_dataP,
		AEIO_InSpecH			inH, 
		const A_Time			*base_time_tr,
		AEIO_TimeDir			time_dir, 
		A_Boolean				*found0,
		A_Time					*key_time_tr0);

	A_Err	(*AEIO_InitOutputSpec)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		A_Boolean			*user_interacted);

	A_Err	(*AEIO_GetFlatOutputOptions)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		AEIO_Handle			*optionsH);

	A_Err	(*AEIO_DisposeOutputOptions)(
		AEIO_BasicData		*basic_dataP,
		void				*optionsPV);

	A_Err	(*AEIO_UserOptionsDialog)(
		AEIO_BasicData			*basic_dataP,
		AEIO_OutSpecH			outH, 
		const PF_EffectWorld	*sample0,
		A_Boolean				*user_interacted0);

	A_Err	(*AEIO_GetOutputInfo)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH,
		AEIO_Verbiage		*verbiage);

	A_Err	(*AEIO_OutputInfoChanged)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH);

	A_Err	(*AEIO_SetOutputFile)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		const A_UTF16Char	*file_pathZ);		// Null terminated UTF16 string with platform separators

	A_Err	(*AEIO_StartAdding)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		A_long				flags);

	A_Err	(*AEIO_AddFrame)(
		AEIO_BasicData				*basic_dataP,
		AEIO_OutSpecH				outH, 
		A_long						frame_index, 
		A_long						frames,
		const PF_EffectWorld		*wP, 
		const A_LPoint				*origin0,
		A_Boolean					was_compressedB,	
		AEIO_InterruptFuncs			*inter0);

	A_Err	(*AEIO_EndAdding)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		A_long				flags);

	A_Err	(*AEIO_OutputFrame)(
		AEIO_BasicData			*basic_dataP,
		AEIO_OutSpecH			outH, 
		const PF_EffectWorld	*wP);

	A_Err	(*AEIO_WriteLabels)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		AEIO_LabelFlags		*written);

	A_Err	(*AEIO_GetSizes)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		A_u_longlong		*free_space, 
		A_u_longlong		*file_size);

	A_Err	(*AEIO_Flush)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH);

	A_Err	(*AEIO_AddSoundChunk)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		const A_Time		*start, 
		A_u_long			num_samplesLu,
		const void			*dataPV);

	A_Err	(*AEIO_Idle)(
		AEIO_BasicData			*basic_dataP,
		AEIO_ModuleSignature	sig,
		AEIO_IdleFlags			*idle_flags0);		/* >> */

	A_Err	(*AEIO_GetDepths)(
		AEIO_BasicData			*basic_dataP,
		AEIO_OutSpecH			outH, 
		AEIO_SupportedDepthFlags		*which);

	A_Err	(*AEIO_GetOutputSuffix)(
		AEIO_BasicData			*basic_dataP,
		AEIO_OutSpecH			outH, 
		A_char					*suffix);

	A_Err	(*AEIO_SeqOptionsDlg)(
		AEIO_BasicData			*basic_dataP,
		AEIO_InSpecH			inH,  
		A_Boolean				*user_interactedPB0);

	A_Err	(*AEIO_GetNumAuxChannels)(
		AEIO_BasicData			*basic_dataP,
		AEIO_InSpecH			inH,
		A_long					*num_channelsPL);

	A_Err	(*AEIO_GetAuxChannelDesc)(
		AEIO_BasicData			*basic_dataP,	
		AEIO_InSpecH			inH,
		A_long					chan_indexL,
		PF_ChannelDesc			*descP);

	A_Err	(*AEIO_DrawAuxChannel)(
		AEIO_BasicData			*basic_dataP,
		AEIO_InSpecH			inH,
		A_long					chan_indexL,
		const AEIO_DrawFramePB	*pbP,
		PF_ChannelChunk			*chunkP);

	A_Err	(*AEIO_FreeAuxChannel)(	
		AEIO_BasicData			*basic_dataP,
		AEIO_InSpecH			inH,
		PF_ChannelChunk			*chunkP);

	A_Err	(*AEIO_NumAuxFiles)(	
		AEIO_BasicData			*basic_dataP,	
		AEIO_InSpecH			seqH,
		A_long					*files_per_framePL0);

	A_Err	(*AEIO_GetNthAuxFileSpec)(
		AEIO_BasicData			*basic_dataP,
		AEIO_InSpecH			seqH,
		A_long					frame_numL, 
		A_long					n,
		AEGP_MemHandle			*pathPH);			// << handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle 

	A_Err	(*AEIO_CloseSourceFiles)(
		AEIO_BasicData			*basic_dataP,
		AEIO_InSpecH			seqH);

	A_Err	(*AEIO_CountUserData)(
		AEIO_BasicData			*basic_dataP,
		AEIO_InSpecH			inH,
		A_u_long 				typeLu,
		A_u_long				max_sizeLu,
		A_u_long				*num_of_typePLu);

	A_Err	(*AEIO_SetUserData)(    
		AEIO_BasicData			*basic_dataP,
		AEIO_OutSpecH			outH,
		A_u_long				typeLu,
		A_u_long				indexLu,
		const AEIO_Handle		dataH);

	A_Err	(*AEIO_GetUserData)(  
		AEIO_BasicData			*basic_dataP, 
		AEIO_InSpecH			inH,
		A_u_long 				typeLu,
		A_u_long				indexLu,
		A_u_long				max_sizeLu,
		AEIO_Handle				*dataPH);

	A_Err	(*AEIO_AddMarker)(	
		AEIO_BasicData			*basic_dataP,	
		AEIO_OutSpecH 			outH, 
		A_long 					frame_index, 
		AEIO_MarkerType 		marker_type, 
		void					*marker_dataPV, 
		AEIO_InterruptFuncs		*inter0);

	A_Err	(*AEIO_VerifyFileImportable)(
		AEIO_BasicData			*basic_dataP,
		AEIO_ModuleSignature	sig, 
		const A_UTF16Char	*	file_pathZ,		// Null terminated UTF16 string with platform separators
		A_Boolean				*importablePB);	

	A_Err	(*AEIO_UserAudioOptionsDialog)(
		AEIO_BasicData		*basic_dataP,
		AEIO_OutSpecH		outH, 
		A_Boolean			*user_interacted0);

	A_Err	(*AEIO_AddMarker2)(	
		AEIO_BasicData			*basic_dataP,	
		AEIO_OutSpecH 			outH, 
		A_long 					frame_index, 
		const AEIO_Marker		*markerP, 
		AEIO_InterruptFuncs		*inter0);

	A_Err	(*AEIO_AddMarker3)(	
		AEIO_BasicData			*basic_dataP,	
		AEIO_OutSpecH 			outH, 
		A_long 					frame_index, 
		AEGP_ConstMarkerValP 	marker_valP,
		AEIO_RenderMarkerFlag	marker_flag,
		AEIO_InterruptFuncs		*inter0);
		
	A_Err	(*AEIO_GetMimeType)(
		AEIO_BasicData	*basic_dataP,
		AEIO_OutSpecH	outH,
		A_long			mime_type_sizeL,
		char			*mime_typeZ);

} AEIO_FunctionBlock4;

#ifdef __cplusplus
	}
#endif

#include <adobesdk/config/PostConfig.h>

#endif
