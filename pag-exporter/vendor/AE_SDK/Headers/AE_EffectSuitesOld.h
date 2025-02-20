// AE_EffectSuitesOld.h
//
// Copyright (c) 2011 Adobe Systems Inc, Seattle WA
// All Rights Reserved
//
// These are old, deprecated versions of some suites. Please use the ones in AE_EffectSuites.h instead if at all possible.
//
//

// 
#ifndef _H_AE_EffectSuitesOld
	#error this file is designed to be included only by AE_EffectSuites.h; do not include directly
#endif

/** PF_ParamUtilsSuite1

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

	PF_HaveInputsChangedOverTimeSpan() -- OBSOLETE, see PF_AreStatesIdentical() instead.
	This API is handy for effects that do simulation across time, where frame N is
	dependent on frame N-1, and you have a cache in your sequence data that needs validating.
	When asked to render frame N, assume you have your cached data from frame N-1, you'd call
	PF_HaveInputsChangedOverTimeSpan(start=0, duration=N-1) to see if your cache is still valid.
	The state of all parameters (except those with PF_ParamFlag_EXCLUDE_FROM_HAVE_INPUTS_CHANGED
	set), including layer parameters (including param[0]) are checked over the passed time
	span. This is done efficiently, as the change tracking is done with timestamps.

	Requires PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT to be set by the effect. If validating a
	cache for use during a render, the call to PF_HaveInputsChangedOverTimeSpan() must
	happen during one of the rendering PF_Cmds (PF_Cmd_FRAME_SETUP, PF_Cmd_RENDER,
	PF_Cmd_FRAME_SETDOWN,PF_Cmd_SMART_PRE_RENDER, PF_Cmd_SMART_RENDER).

	If *changedPB is returned FALSE, you can safely use your cache, AND the internal
	caching system will assume that you have a temporal dependency on the passed range,
	so if something changes upstream, AE's caches will be properly invalidated.		

**/

#define kPFParamUtilsSuiteVersion1	2	/* 64-bit version frozen in AE 10.0 */	


typedef struct PF_ParamUtilsSuite1 {

	SPAPI PF_Err	(*PF_UpdateParamUI)(
		PF_ProgPtr			effect_ref,
		PF_ParamIndex		param_index,
		const PF_ParamDef	*defP);

	// The next 3 methods have had "Obsolete" added to their name to intentionally
	// break compile compatibility. They continue to work and are binary compatible, but
	// are more conservative and inefficient than in previous versions. Please
	// switch to the current version of this suite for maximum benefit.
	SPAPI PF_Err	(*PF_GetCurrentStateObsolete)(
		PF_ProgPtr			effect_ref,
		PF_State			*stateP);		/* << */

	SPAPI PF_Err	(*PF_HasParamChangedObsolete)(
		PF_ProgPtr			effect_ref,
		const PF_State		*stateP,		// has param changed since this state was grabbed
		PF_ParamIndex		param_index,	// ignored, always treated as PF_ParamIndex_CHECK_ALL_HONOR_EXCLUDE - go use the modern version of this suite!
		//	have changed including layer param[0];
		// pass PF_ParamIndex_CHECK_ALL_EXCEPT_LAYER_PARAMS to see
		//	if any non-layer param values have changed
		PF_Boolean			*changedPB);	/* << */

	SPAPI PF_Err	(*PF_HaveInputsChangedOverTimeSpanObsolete)(			// see comment above
		PF_ProgPtr			effect_ref,
		const PF_State		*stateP,		// has param changed since this state was grabbed
		const A_Time		*startPT0,		// NULL for both start & duration mean at any time
		const A_Time		*durationPT0,
		PF_Boolean			*changedPB);	/* << */

	SPAPI PF_Err	(*PF_IsIdenticalCheckout)(
		PF_ProgPtr			effect_ref,
		PF_ParamIndex		param_index,
		A_long				what_time1,
		A_long				time_step1,
		A_u_long		time_scale1,
		A_long				what_time2,
		A_long				time_step2,
		A_u_long		time_scale2,
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

} PF_ParamUtilsSuite1;


/* -------------------------------------------------------------------- */

#define kPFAppSuite			"PF AE App Suite"
#define kPFAppSuiteVersion4	6	/* frozen in AE 10.0 */


typedef struct PFAppSuite4 {		/* frozen in AE 10.0 */
	
	SPAPI PF_Err 	(*PF_AppGetBgColor)(	PF_App_Color			*bg_colorP);		/* << */
	
	SPAPI PF_Err 	(*PF_AppGetColor)(		PF_App_ColorType		color_type,			/* >> */
									  PF_App_Color			*app_colorP);		/* << */
	
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
	
} PFAppSuite4;


/* -------------------------------------------------------------------- */


#define kPFAppSuiteVersion5	7	/* frozen in AE 12.0 */

typedef struct PFAppSuite5 {		/* frozen in AE 12.0 */
	
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
	
	
} PFAppSuite5;

/* -------------------------------------------------------------------- */

#define kPFEffectCustomUISuite			"PF Effect Custom UI Suite"
#define kPFEffectCustomUISuiteVersion1	1 /* frozen in 10.0 */

// This suite provides basic apis needed for custom ui drawing in any window (Comp/Layer/ECW)

typedef struct PF_EffectCustomUISuite1 {
	
	// Get the drawing reference for custom ui drawing.
	SPAPI PF_Err	(*PF_GetDrawingReference)(	const PF_ContextH		effect_contextH,		/* >> */
											  DRAWBOT_DrawRef			*referenceP0);			/* << */
	
} PF_EffectCustomUISuite1;


