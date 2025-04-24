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

#ifndef _H_AE_EffectSuites
#define _H_AE_EffectSuites

#include <AE_Effect.h>
#include <AE_EffectUI.h>		// for PF_CursorType
#include <SPBasic.h>

#include <adobesdk/config/PreConfig.h>

#ifdef __cplusplus
	extern "C" {
#endif



#define kPFPathQuerySuite			"PF Path Query Suite"
#define kPFPathQuerySuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct PF_PathOutline *PF_PathOutlinePtr;
typedef struct PF_PathSegPrep *PF_PathSegPrepPtr;

typedef struct PF_PathQuerySuite1 {

	SPAPI PF_Err	(*PF_NumPaths)(	PF_ProgPtr			effect_ref,
									A_long				*num_pathsPL);	/* << */

	SPAPI PF_Err	(*PF_PathInfo)(	PF_ProgPtr			effect_ref,
									A_long				indexL,
									PF_PathID			*unique_idP);	/* << */

	SPAPI PF_Err	(*PF_CheckoutPath)(	PF_ProgPtr			effect_ref,
										PF_PathID			unique_id,
										A_long				what_time,
										A_long				time_step,
										A_u_long		time_scale,
										PF_PathOutlinePtr	*pathPP);	/* << */	// can return NULL ptr if path doesn't exist

	SPAPI PF_Err	(*PF_CheckinPath)(	PF_ProgPtr			effect_ref,
										PF_PathID			unique_id,
										PF_Boolean			changedB,
										PF_PathOutlinePtr	pathP);

} PF_PathQuerySuite1;



/* -------------------------------------------------------------------- */


#define kPFPathDataSuite			"PF Path Data Suite"
#define kPFPathDataSuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct {

	PF_FpLong				x, y;
	PF_FpLong				tan_in_x, tan_in_y;
	PF_FpLong				tan_out_x, tan_out_y;

} PF_PathVertex;

#define	PF_MAX_PATH_NAME_LEN		31


typedef struct PF_PathDataSuite1 {

	SPAPI PF_Err	(*PF_PathIsOpen)(		PF_ProgPtr			effect_ref0,
											PF_PathOutlinePtr	pathP,
											PF_Boolean			*openPB);

	// N segments means there are segments [0..N-1]; segment J is defined by vertex J & J+1
	SPAPI PF_Err 	(*PF_PathNumSegments)(	PF_ProgPtr			effect_ref0,
											PF_PathOutlinePtr	pathP,
											A_long				*num_segmentsPL);
	
	// which_pointL range: [0..num_segments]; for closed paths vertex[0] == vertex[num_segments]
	SPAPI PF_Err 	(*PF_PathVertexInfo)(	PF_ProgPtr			effect_ref0,
											PF_PathOutlinePtr	pathP,
											A_long				which_pointL,
											PF_PathVertex		*vertexP);

	SPAPI PF_Err 	(*PF_PathPrepareSegLength)(
											PF_ProgPtr			effect_ref0,
											PF_PathOutlinePtr	pathP,
											A_long				which_segL,
											A_long				frequencyL,
											PF_PathSegPrepPtr	*lengthPrepPP);

	SPAPI PF_Err 	(*PF_PathGetSegLength)(	PF_ProgPtr			effect_ref0,
											PF_PathOutlinePtr	pathP,
											A_long				which_segL,
											PF_PathSegPrepPtr	*lengthPrepP0,
											PF_FpLong			*lengthPF);
											
	SPAPI PF_Err 	(*PF_PathEvalSegLength)(PF_ProgPtr			effect_ref0,
											PF_PathOutlinePtr	pathP,
											PF_PathSegPrepPtr	*lengthPrepPP0,
											A_long				which_segL,
											PF_FpLong			lengthF,
											PF_FpLong			*x,
											PF_FpLong			*y);

	SPAPI PF_Err 	(*PF_PathEvalSegLengthDeriv1)(
											PF_ProgPtr			effect_ref0,
											PF_PathOutlinePtr	pathP,
											PF_PathSegPrepPtr	*lengthPrepPP0,
											A_long				which_segL,
											PF_FpLong			lengthF,
											PF_FpLong			*x,
											PF_FpLong			*y,
											PF_FpLong			*deriv1x,
											PF_FpLong			*deriv1y);


	SPAPI PF_Err 	(*PF_PathCleanupSegLength)(
											PF_ProgPtr			effect_ref0,
											PF_PathOutlinePtr	pathP,
											A_long				which_segL,
											PF_PathSegPrepPtr	*lengthPrepPP);


	SPAPI PF_Err 	(*PF_PathIsInverted)(	PF_ProgPtr			effect_ref,
											PF_PathID			unique_id,
											PF_Boolean			*invertedB);

	SPAPI PF_Err 	(*PF_PathGetMaskMode)(	PF_ProgPtr			effect_ref,
											PF_PathID			unique_id,
											PF_MaskMode			*modeP);

	SPAPI PF_Err	(*PF_PathGetName)(		PF_ProgPtr			effect_ref,
											PF_PathID			unique_id,
											A_char				*nameZ);		/* << can be up to PF_MAX_PATH_NAME_LEN+1 bytes long */


} PF_PathDataSuite1;


/* -------------------------------------------------------------------- */

// New versions of state related APIs added in CS6

#define kPFParamUtilsSuite			"PF Param Utils Suite"
#define kPFParamUtilsSuiteVersion3	3	// Frozen in AE CS6 [aka AE11.0]

typedef struct {
	A_long	reservedAL[4];
} PF_State;


#define PF_ParamIndex_NONE							(-1L)
#define PF_ParamIndex_CHECK_ALL						(-2L)
#define PF_ParamIndex_CHECK_ALL_EXCEPT_LAYER_PARAMS	(-3L)
#define PF_ParamIndex_CHECK_ALL_HONOR_EXCLUDE		(-4L)	// Like PF_ParamIndex_CHECK_ALL, but honor PF_ParamFlag_EXCLUDE_FROM_HAVE_INPUTS_CHANGED

#define PF_KeyIndex_NONE							(-1L)


enum {
	PF_TimeDir_GREATER_THAN				= 0x0000,
	PF_TimeDir_LESS_THAN				= 0x0001,
	PF_TimeDir_GREATER_THAN_OR_EQUAL	= 0x1000,
	PF_TimeDir_LESS_THAN_OR_EQUAL		= 0x1001
};
typedef A_long	PF_TimeDir;

typedef A_long	PF_KeyIndex;


/** PF_ParamUtilsSuite3

	PF_UpdateParamUI()

  		You can call this function for each param whose UI settings you
		want to change when handling a PF_Cmd_USER_CHANGED_PARAM or
		PF_Cmd_UPDATE_PARAMS_UI.  These changes are cosmetic only, and don't
		go into the undo buffer.
		
		The ONLY fields that can be changed in this way are:

			PF_ParamDef
				ui_flags: PF_PUI_ECW_SEPARATOR, PF_PUI_DISABLED only (and PF_PUI_INVISIBLE in Premiere).
				ui_width
				ui_height
				name
				flags: PF_ParamFlag_COLLAPSE_TWIRLY only

				PF_ParamDefUnion:
					slider_min, slider_max, precision, display_flags of any slider type

		For PF_PUI_STD_CONTROL_ONLY params, you can also change the value field by setting
		PF_ChangeFlag_CHANGED_VALUE before returning.  But you are not allowed to change
		the value during PF_Cmd_UPDATE_PARAMS_UI.

		PF_GetCurrentState() / PF_AreStatesIdentical()
			This API lets you determine if a set of your inputs (either layers, other properties, or both) 
			are different between when you first called PF_GetCurrentState() and a current call, so it can
			be used for caching. You can specify a range of time to consider or all of time.

			For effects that do simulation across time and therefore set PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT, 
			when you ask about a time range, it will be expanded to include any times needed to produce
			that range.

			See doc on the old PF_HaveInputsChangedOverTimeSpan() for historical context.

**/

typedef struct PF_ParamUtilsSuite3 {

	SPAPI PF_Err	(*PF_UpdateParamUI)(
								PF_ProgPtr			effect_ref,
								PF_ParamIndex		param_index,
								const PF_ParamDef	*defP);

	// IMPORTANT: as of 13.5 to avoid threading deadlock problems, PF_GetCurrentState() returns a random state
	// if used in the context of UPDATE_PARAMS_UI only. In other selectors this will behave normally.
	SPAPI PF_Err	(*PF_GetCurrentState)(
								PF_ProgPtr			effect_ref,
								PF_ParamIndex		param_index,
								const A_Time		*startPT0,		// NULL for both start & duration means over all of time
								const A_Time		*durationPT0,
								PF_State			*stateP);		/* << */

	SPAPI PF_Err	(*PF_AreStatesIdentical)(
								PF_ProgPtr			effect_ref,
								const PF_State		*state1P,	
								const PF_State		*state2P,	
								A_Boolean			*samePB);		/* << */

	SPAPI PF_Err	(*PF_IsIdenticalCheckout)(
								PF_ProgPtr			effect_ref,
								PF_ParamIndex		param_index,
								A_long				what_time1,
								A_long				time_step1,
								A_u_long			time_scale1,
								A_long				what_time2,
								A_long				time_step2,
								A_u_long			time_scale2,
								PF_Boolean			*identicalPB);		/* << */

	SPAPI PF_Err	(*PF_FindKeyframeTime)(
								PF_ProgPtr			effect_ref,
								PF_ParamIndex		param_index,
								A_long				what_time,
								A_u_long		time_scale,
								PF_TimeDir			time_dir,
								PF_Boolean			*foundPB,			/* << */
								PF_KeyIndex			*key_indexP0,		/* << */
								A_long				*key_timeP0,		/* << */	// you can ask for either:
								A_u_long		*key_timescaleP0);	/* << */	// time&timescale OR neither

	SPAPI PF_Err	(*PF_GetKeyframeCount)(
								PF_ProgPtr			effect_ref,
								PF_ParamIndex		param_index,
								PF_KeyIndex			*key_countP);		/* << */	// returns PF_KeyIndex_NONE for constant

	SPAPI PF_Err	(*PF_CheckoutKeyframe)(
								PF_ProgPtr			effect_ref,
								PF_ParamIndex		param_index,
								PF_KeyIndex			key_index,			// zero-based
								A_long				*key_timeP0,		/* << */	// you can ask for either:
								A_u_long		*key_timescaleP0,	/* << */	// time&timescale OR neither
								PF_ParamDef			*paramP0);			/* << */

	SPAPI PF_Err	(*PF_CheckinKeyframe)(
								PF_ProgPtr			effect_ref,
								PF_ParamDef			*paramP);

	SPAPI PF_Err	(*PF_KeyIndexToTime)(
								PF_ProgPtr			effect_ref,
								PF_ParamIndex		param_index,
								PF_KeyIndex			key_indexP,			/* >> */
								A_long				*key_timeP,			/* >> */
								A_u_long		*key_timescaleP);	/* << */

} PF_ParamUtilsSuite3;




/* -------------------------------------------------------------------- */


#define kPFColorParamSuite					"PF ColorParamSuite"
#define kPFColorParamSuiteVersion1	1		/* frozen in AE 7.0 */

/** PF_ColorParamSuite1

**/

typedef struct PF_ColorParamSuite1 {

	// floating point color is in the working space of
	// your effect, i.e. the same color space
	// as the pixels you get
	
	// note that overrange and underrange values are possible
	// even when project is not set to 32bpc
	
	SPAPI PF_Err	(*PF_GetFloatingPointColorFromColorDef)(
								PF_ProgPtr			effect_ref,			/* >> */
								const PF_ParamDef	*color_defP,		/* >> */
								PF_PixelFloat		*fp_colorP);		/* << */

} PF_ColorParamSuite1;




/* -------------------------------------------------------------------- */

#define kPFPointParamSuite					"PF PointParamSuite"
#define kPFPointParamSuiteVersion1		1	/* frozen in AE 10.5 */

/** PF_PointParamSuite1

**/

typedef struct PF_PointParamSuite1 {

	// this api returns floating point value of a point parameter.

	SPAPI PF_Err	(*PF_GetFloatingPointValueFromPointDef)(
								PF_ProgPtr			effect_ref,			/* >> */
								const PF_ParamDef	*point_defP,		/* >> */
								A_FloatPoint		*fp_pointP);		/* << */

} PF_PointParamSuite1;


/* -------------------------------------------------------------------- */

#define kPFAngleParamSuite					"PF AngleParamSuite"
#define kPFAngleParamSuiteVersion1			1	/* frozen in AE 11.0.x */

/** PF_AngleParamSuite1

**/

typedef struct PF_AngleParamSuite1 {

	// this api returns floating point value of an angle parameter.

	SPAPI PF_Err	(*PF_GetFloatingPointValueFromAngleDef)(
		PF_ProgPtr			effect_ref,			/* >> */
		const PF_ParamDef	*angle_defP,		/* >> */
		A_FpLong			*fp_valueP);		/* << */

} PF_AngleParamSuite1;


/* -------------------------------------------------------------------- */

#define kPFAppSuite			"PF AE App Suite"
#define kPFAppSuiteVersion6	1	/* frozen in AE 13.1 */
	
enum {
	PF_App_Color_NONE = -1, 

	PF_App_Color_FRAME, 
	PF_App_Color_FILL, 
	PF_App_Color_TEXT, 
	PF_App_Color_LIGHT_TINGE, 
	PF_App_Color_DARK_TINGE, 
	PF_App_Color_HILITE, 
	PF_App_Color_SHADOW, 
	
	PF_App_Color_BUTTON_FRAME, 
	PF_App_Color_BUTTON_FILL, 
	PF_App_Color_BUTTON_TEXT, 
	PF_App_Color_BUTTON_LIGHT_TINGE, 
	PF_App_Color_BUTTON_DARK_TINGE, 
	PF_App_Color_BUTTON_HILITE, 
	PF_App_Color_BUTTON_SHADOW, 
	
	PF_App_Color_BUTTON_PRESSED_FRAME, 
	PF_App_Color_BUTTON_PRESSED_FILL, 
	PF_App_Color_BUTTON_PRESSED_TEXT, 
	PF_App_Color_BUTTON_PRESSED_LIGHT_TINGE, 
	PF_App_Color_BUTTON_PRESSED_DARK_TINGE, 
	PF_App_Color_BUTTON_PRESSED_HILITE, 
	PF_App_Color_BUTTON_PRESSED_SHADOW, 

	/********************************/

	PF_App_Color_FRAME_DISABLED, 
	PF_App_Color_FILL_DISABLED, 
	PF_App_Color_TEXT_DISABLED, 
	PF_App_Color_LIGHT_TINGE_DISABLED, 
	PF_App_Color_DARK_TINGE_DISABLED, 
	PF_App_Color_HILITE_DISABLED, 
	PF_App_Color_SHADOW_DISABLED, 
	
	PF_App_Color_BUTTON_FRAME_DISABLED, 
	PF_App_Color_BUTTON_FILL_DISABLED, 
	PF_App_Color_BUTTON_TEXT_DISABLED, 
	PF_App_Color_BUTTON_LIGHT_TINGE_DISABLED, 
	PF_App_Color_BUTTON_DARK_TINGE_DISABLED, 
	PF_App_Color_BUTTON_HILITE_DISABLED, 
	PF_App_Color_BUTTON_SHADOW_DISABLED, 
	
	PF_App_Color_BUTTON_PRESSED_FRAME_DISABLED, 
	PF_App_Color_BUTTON_PRESSED_FILL_DISABLED, 
	PF_App_Color_BUTTON_PRESSED_TEXT_DISABLED, 
	PF_App_Color_BUTTON_PRESSED_LIGHT_TINGE_DISABLED, 
	PF_App_Color_BUTTON_PRESSED_DARK_TINGE_DISABLED, 
	PF_App_Color_BUTTON_PRESSED_HILITE_DISABLED, 
	PF_App_Color_BUTTON_PRESSED_SHADOW_DISABLED, 
	
	/********************************/
	PF_App_Color_BLACK, 
	PF_App_Color_WHITE, 
	PF_App_Color_GRAY, 
	PF_App_Color_RED, 
	PF_App_Color_YELLOW, 
	PF_App_Color_GREEN, 
	PF_App_Color_CYAN,

	/********************************/
	PF_App_Color_TLW_NEEDLE_CURRENT_TIME,
	PF_App_Color_TLW_NEEDLE_PREVIEW_TIME,
	PF_App_Color_TLW_CACHE_MARK_MEM,
	PF_App_Color_TLW_CACHE_MARK_DISK,
	PF_App_Color_TLW_CACHE_MARK_MIX,
	PF_App_Color_FILL_LIGHT, 
	PF_App_Color_HOT_TEXT,
	PF_App_Color_HOT_TEXT_DISABLED,

	/********************************/
	PF_App_Color_LABEL_0, 
	PF_App_Color_LABEL_1, 
	PF_App_Color_LABEL_2, 
	PF_App_Color_LABEL_3, 
	PF_App_Color_LABEL_4, 
	PF_App_Color_LABEL_5, 
	PF_App_Color_LABEL_6, 
	PF_App_Color_LABEL_7, 
	PF_App_Color_LABEL_8,
	PF_App_Color_LABEL_9,
	PF_App_Color_LABEL_10,
	PF_App_Color_LABEL_11,
	PF_App_Color_LABEL_12,
	PF_App_Color_LABEL_13,
	PF_App_Color_LABEL_14,
	PF_App_Color_LABEL_15,
	PF_App_Color_LABEL_16,

	/********************************/
	PF_App_Color_TLW_CACHE_MARK_MEM_DUBIOUS,
	PF_App_Color_TLW_CACHE_MARK_DISK_DUBIOUS,
	PF_App_Color_TLW_CACHE_MARK_MIX_DUBIOUS,
	PF_App_Color_HOT_TEXT_PRESSED,
	PF_App_Color_HOT_TEXT_WARNING,
	PF_App_Color_PURE_BLACK, 
	PF_App_Color_PURE_WHITE, 

	PF_App_Color_PANEL_BACKGROUND = 1000,
	PF_App_Color_LIST_BOX_FILL,
	PF_App_Color_DARK_CAPTION_FILL,
	PF_App_Color_DARK_CAPTION_TEXT,
	PF_App_Color_TEXT_ON_LIGHTER_BG,

	PF_App_Color_NUMTYPES
};
typedef A_short	PF_App_ColorType;

enum {
	// the first entry allows the user to do it the way they normally do with
	// ae native color pickers (straight is the default, shift gives you premul)
	PF_EyeDropperSampleMode_DEFAULT,
	PF_EyeDropperSampleMode_STRAIGHT,
	PF_EyeDropperSampleMode_PREMUL
};

typedef A_short PF_EyeDropperSampleMode;

typedef	struct PF_App_Color {
		A_u_short red;		
		A_u_short green;	
		A_u_short blue;	
} PF_App_Color;


#define	PF_APP_MAX_PERS_LEN				63

typedef struct PF_AppPersonalTextInfo {
	A_char		name[PF_APP_MAX_PERS_LEN + 1];
	A_char		org[PF_APP_MAX_PERS_LEN + 1];
	A_char		serial_str[PF_APP_MAX_PERS_LEN + 1];
} PF_AppPersonalTextInfo;


enum {
	PF_FontStyle_NONE		= -1,	// sentinel
	PF_FontStyle_SYS		= 0,	// system font, system size, system style (0, 0, 0)
	PF_FontStyle_SMALL,				// usually small annotation text
	PF_FontStyle_SMALL_BOLD,		// more important small annotations
	PF_FontStyle_SMALL_ITALIC,		// missing things, etc.
	PF_FontStyle_MED,				// times in in/out panels
	PF_FontStyle_MED_BOLD,			// 
	PF_FontStyle_APP,				// 
	PF_FontStyle_APP_BOLD,			// time in TL window
	PF_FontStyle_APP_ITALIC			// 
};
typedef A_LegacyEnumType PF_FontStyleSheet;



#define	PF_FONT_NAME_LEN				255

typedef struct PF_FontName {
	A_char		font_nameAC[PF_FONT_NAME_LEN+1];
} PF_FontName;
		
	
#define PF_APP_LANG_TAG_SIZE	(5 + 1)
		
typedef struct _PF_AppProgressDialog	*PF_AppProgressDialogP;
		

typedef struct PFAppSuite6 {		/* frozen in AE 13.1 */

	SPAPI PF_Err 	(*PF_AppGetBgColor)(	PF_App_Color			*bg_colorP);		/* << */

	SPAPI PF_Err 	(*PF_AppGetColor)(		PF_App_ColorType		color_type,			/* >> */
											PF_App_Color			*app_colorP);		/* << */
	
	// Provides the active displayed language of AE UI so plugin can match. e.g. "en_US"
	SPAPI PF_Err 	(*PF_AppGetLanguage)(	A_char					*lang_tagZ);		/* << up to PF_APP_LANG_TAG_SIZE-1  */

	SPAPI PF_Err 	(*PF_GetPersonalInfo)(	PF_AppPersonalTextInfo	*ptiP);				/* << */

	SPAPI PF_Err 	(*PF_GetFontStyleSheet)(PF_FontStyleSheet		sheet,				/* >> */
											PF_FontName				*font_nameP0,		/* << */			
											A_short					*font_numPS0,		/* << */
											A_short					*sizePS0,			/* << */
											A_short					*stylePS0);			/* << */

	// normally the effect should respond to PF_Event_ADJUST_CURSOR, but for changing
	//	the cursor during modal situations, you can use this API
	SPAPI PF_Err	(*PF_SetCursor)(		PF_CursorType		cursor);				/* >> */

	// as of AE6.5, this function returns TRUE if installed app is the render engine (as before)
	//				OR if the app is being run with no UI OR if the app is in watch-folder mode
	SPAPI PF_Err	(*PF_IsRenderEngine)(	PF_Boolean				*render_enginePB);		/* >> */

	// will return PF_Interrupt_CANCEL if user cancels dialog. color is in project working colorspace
	// if use_ws_to_monitor_xformB is TRUE, then the color chips and pickers are run through the
	// working space -> display transformation while interacting. set FALSE to have the raw RGB values
	// pushed directly to the screen. TRUE is intended for when the returned color is used in rendering, FALSE
	// is intended if the color is for UI elements or other nonrenderables.
	
	SPAPI PF_Err	(*PF_AppColorPickerDialog)(	const A_char			*dialog_titleZ0,				/* >> */
												const PF_PixelFloat		*sample_colorP,					/* >> */
												PF_Boolean				use_ws_to_monitor_xformB, 		/* >> */
												PF_PixelFloat			*new_colorP);					/* << */

	// for use only when processing an event in an effect with custom UI
	SPAPI PF_Err	(*PF_GetMouse)(PF_Point* pointP);

	// NEW api in version 4.
	// Use it to invalidate rect of current window being drawn. Invalidated rect will be updated during idle time. 
	// Specify PF_EO_UPDATE_NOW out flag to update the window immediately after the event returns. Specify rectP0 
	// as NULL to invalidate the whole window.
	// Only valid while handling an non-draw event in the effect.
	SPAPI PF_Err	(*PF_InvalidateRect)(	const PF_ContextH	contextH, 
											const PF_Rect*		rectP0);

	// only safe to use when processing an event from a custom UI event.
	SPAPI PF_Err	(*PF_ConvertLocalToGlobal)(const PF_Point* localP, PF_Point* globalP);
	
	// this will return a deep color if over a content window containing 32bpc.
	// eyeSize == 0 will use the application pref as set by the user.
	SPAPI PF_Err	(*PF_GetColorAtGlobalPoint)(const PF_Point* globalP, A_short eyeSize, PF_EyeDropperSampleMode mode, PF_PixelFloat* outColorP);
	

	// Manages a modal progress dialog session for use inside of time-consuming AEGP commands
	// Only one dialog active at a time is supported, and it may not get displayed in cases where AE UI is disabled (become no-ops)
	// These calls must be used from the AE main/UI thread
	// The app busy cursor will automatically set/reset as needed, regardless of whether dialog is displayed
	SPAPI PF_Err	(*PF_CreateNewAppProgressDialog)(	
											const A_UTF16Char*	titleZ,			// >> title of the progress dialog
											const A_UTF16Char*	cancel_strZ0,	// >> [optional] what the name on the button should be. 
																				// If NULL, AE localized Cancel used by default
											PF_Boolean			indeterminateB,	// >> TRUE shows "barber pole" style animation with no incremental progress
											PF_AppProgressDialogP *prog_dlgPP);	// <> Returns allocated ProgressDialog session (may not display actual dialog yet)
	
	// This will trigger initial dialog display once timeout elapses.
	// While dialog is displayed, this updates animation/progress status.  
	// If the session is disposed before the elapsed time the dialog is never displayed.
	// This must be called frequently during your algorithm to keep UI responsive and animation moving. About 100ms interval would be a good target.
	// Returns PF_Interrupt_CANCEL if user canceled the dialog
	SPAPI PF_Err	(*PF_AppProgressDialogUpdate)(
											PF_AppProgressDialogP prog_dlgP,	// >> The allocated session
											A_long countL, A_long totalL);		// >>  count/total is the fraction of progress to display (when not indeterminate style)
																				//     (pass zeros to keep barber-pole animation alive)
																				
	// PF_AppProgressDialogP MUST be disposed whether the actual dialog is displayed or not
	SPAPI PF_Err	(*PF_DisposeAppProgressDialog)(PF_AppProgressDialogP prog_dlgP);	// >> The allocated session.  MUST be disposed.

} PFAppSuite6;



#define kPFEffectUISuite			"PF Effect UI Suite"
#define kPFEffectUISuiteVersion1	1 /* frozen in 5.5 */

typedef struct PF_EffectUISuite1 {

	SPAPI PF_Err	(*PF_SetOptionsButtonName)(	PF_ProgPtr			effect_ref,
												const A_char			*nameZ);

} PF_EffectUISuite1;




#define kPFEffectCustomUISuite			"PF Effect Custom UI Suite"
#define kPFEffectCustomUISuiteVersion2	2 /* frozen in 13.5 */

typedef struct _PF_AsyncManager	 *PF_AsyncManagerP;		// manage multiple asynchronous render requests during lifetime, especially for better caching on UI thread

// apis that relate to

typedef struct PF_EffectCustomUISuite2 {

	// This provides basic apis needed for custom ui drawing in any window (Comp/Layer/ECW)
	// Get the drawing reference for custom ui drawing.
	SPAPI PF_Err	(*PF_GetDrawingReference)(	const PF_ContextH		effect_contextH,		/* >> */
												DRAWBOT_DrawRef			*referenceP0);			/* << */

        // NEW in version 2 (ae13.5)
        // When using PF_OutFlag2_CUSTOM_UI_ASYNC_MANAGER, use this to get the async manager associated with the effect PF_ContextH.  
		// The managed can then be asked to do Async render requests
	SPAPI PF_Err    (*PF_GetContextAsyncManager)(  PF_InData* in_data, PF_EventExtra* extra, PF_AsyncManagerP*               managerPP0);

} PF_EffectCustomUISuite2;




#define kPFEffectCustomUIOverlayThemeSuite				"PF Effect Custom UI Overlay Theme Suite"
#define kPFEffectCustomUIOverlayThemeSuiteVersion1		1 /* frozen in 10.0 */

// This suite should be used for stroking/filling paths, vertices etc on Comp/Layer window. After Effects is internally using it
// so use it to make custom ui look consistent across effects. The foreground/shadow colors are computed based on the app brightness
// level so custom ui is always visible regardless of app brightness.

typedef struct PF_EffectCustomUIOverlayThemeSuite1 {

	// Get foreground/shadow colors preferred for custom ui drawing in AE. 
	// These colors are used by AE custom ui effects.
	SPAPI PF_Err	(*PF_GetPreferredForegroundColor)(	DRAWBOT_ColorRGBA		*foreground_colorP);	/* << */

	SPAPI PF_Err	(*PF_GetPreferredShadowColor)	(	DRAWBOT_ColorRGBA		*shadow_colorP);		/* << */


	// Get foreground & shadow stroke width, vertex size and shadow offset preferred for custom ui drawing in AE. 
	// These settings are used by AE custom ui effects.
	SPAPI PF_Err	(*PF_GetPreferredStrokeWidth)(	float					*stroke_widthPF);		/* << for both foreground and shadow stroke */

	SPAPI PF_Err	(*PF_GetPreferredVertexSize)(	float					*vertex_sizePF);		/* << */
	
	SPAPI PF_Err	(*PF_GetPreferredShadowOffset)(	A_LPoint				*shadow_offsetP);		/* << */


	// Stroke the path with overlay theme foreground color. It can also draw shadow using overlay theme shadow color.
	// It uses overlay theme stroke width for stroking foreground and shadow strokes.
	SPAPI PF_Err	(*PF_StrokePath)	(	const DRAWBOT_DrawRef			drawbot_ref,			/* >> */
											const DRAWBOT_PathRef			path_ref,				/* >> */
											PF_Boolean						draw_shadowB);			/* >> */


	// Fills the path with overlay theme foreground color. It can also draw shadow using overlay theme shadow color.
	// It can be used for drawing in any Comp/Layer/EC windows.
	SPAPI PF_Err	(*PF_FillPath)		(	const DRAWBOT_DrawRef			drawbot_ref,			/* >> */
											const DRAWBOT_PathRef			path_ref,				/* >> */
											PF_Boolean						draw_shadowB);			/* >> */


	// Fills a square (vertex) around the center point using overlay theme foreground color and vertex size.
	SPAPI PF_Err	(*PF_FillVertex)	(	const DRAWBOT_DrawRef			drawbot_ref,			/* >> */
											const A_FloatPoint				*center_pointP,			/* >> */
											PF_Boolean						draw_shadowB);			/* >> */

} PF_EffectCustomUIOverlayThemeSuite1;


// we include this at the end here to maintain source-level compatibility with
//	files that #include AE_EffectSuite but are still using the old suites
//	note: this is _inside_ the pre/post & ifdefs, as this is just a snippet,
//	and not designed to be #include by anyone else
#define _H_AE_EffectSuitesOld
	#include "AE_EffectSuitesOld.h"
#undef _H_AE_EffectSuitesOld

/**********************************************************/
/**********************************************************/

#ifdef __cplusplus
	}
#endif


#include <adobesdk/config/PostConfig.h>

#endif
