/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Inc. and its suppliers, if                    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Inc. and its                    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  DisseminFation of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/


/*	
	Mangler.cpp

	Demonstrates usage of Keyframer API, specifically the 
	KeyframeSuite. 
	
	version		notes							engineer		date
	
	1.0			Time for another sample			bbb				2/14/02
	1.1			Prep for kitchen				bbb				11/13/02
	2.0			Give mangler some teeth			bbb				5/27/03
	2.1			Give mangler fewer bugs			bbb				8/5/04
	3.0			Performed ADM-ectomy			bbb				2/10/06
	3.1			Added MarkerSuite usage			bbb				12/15/06
	3.2			Demo Start/EndAddKeyframe		zal				7/6/10
*/

#include "Mangler.h"

#define REPORT_NUM_IDLE_CALLS 0 // Set to 1 to be notified of number of idle calls to IdleHook

static AEGP_Command			S_Mangler_cmd			=	0L;
static AEGP_PluginID		S_my_id					=	0L;
static A_long				S_idle_count			=	0L;
static ManglerOptions 		S_options;
static SPBasicSuite			*sP						=	NULL;

static A_Err
UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,	/* >> */
	AEGP_UpdateMenuRefcon	refconPV,			/* >> */
	AEGP_WindowType			active_window)		/* >> */
{
	A_Err 				err 			=	A_Err_NONE;
	AEGP_ItemH			active_itemH	=	NULL;
	
	AEGP_SuiteHandler	suites(sP);
	
	ERR(suites.ItemSuite6()->AEGP_GetActiveItem(&active_itemH));

	if (active_itemH){
		ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_Mangler_cmd));
	} else {
		ERR(suites.CommandSuite1()->AEGP_DisableCommand(S_Mangler_cmd));
	}					
	return err;
}

static	A_Err
AddMarker(
	AEGP_StreamRefH		marker_streamH,
	A_Time				key_timeT)
{
	A_Err				err					= A_Err_NONE,
						err2				= A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);
	A_long				new_indexL 			= 0;
	AEGP_StreamValue2 	val;
	AEGP_MarkerValP		newMarkerP			= NULL;
			
	AEFX_CLR_STRUCT(val);

	ERR(suites.KeyframeSuite4()->AEGP_InsertKeyframe(	marker_streamH,
														AEGP_LTimeMode_LayerTime,
														&key_timeT,
														&new_indexL));
	
	ERR(suites.StreamSuite3()->AEGP_GetNewStreamValue(	S_my_id,
														marker_streamH,
														AEGP_LTimeMode_LayerTime,
														&key_timeT,
														TRUE,
														&val));

	if (!err) {
		/*	AE doesn't expose any handy plain-old-char-to-Unicode utility
			functions; use your favorite.
		*/
		
		const A_u_short unicode[] = {0x0041, 0x0042, 0x0043};  // "ABC"
		
		A_long	lengthL = sizeof(unicode) / sizeof(A_u_short);
		
		ERR(suites.MarkerSuite1()->AEGP_NewMarker(&newMarkerP));
		
		ERR(suites.MarkerSuite1()->AEGP_SetMarkerString(newMarkerP,
														AEGP_MarkerString_URL,
														unicode,
														lengthL));

		ERR(suites.MarkerSuite1()->AEGP_SetMarkerString(newMarkerP,
														AEGP_MarkerString_CHAPTER,
														unicode,
														lengthL));

		ERR(suites.MarkerSuite1()->AEGP_SetMarkerString(newMarkerP,
														AEGP_MarkerString_COMMENT,
														unicode,
														lengthL));

		if (!err){
			val.val.markerP = newMarkerP;
			ERR(suites.KeyframeSuite4()->AEGP_SetKeyframeValue(marker_streamH, new_indexL, &val));
		}
	}
	ERR2(suites.StreamSuite3()->AEGP_DisposeStreamValue(&val));

	return err;
}


static	A_Err
AddScaleKeyframe(
	AEGP_AddKeyframesInfoH	addKeyframesInfoH,
	AEGP_StreamRefH			scale_streamH,
	A_Time					key_timeT)
{
	A_Err				err					= A_Err_NONE,
						err2				= A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);
	A_long				new_indexL 			= 0;
	AEGP_StreamValue2 	val;
			
	AEFX_CLR_STRUCT(val);

	ERR(suites.KeyframeSuite4()->AEGP_AddKeyframes(		addKeyframesInfoH,
														AEGP_LTimeMode_LayerTime,
														&key_timeT,
														&new_indexL));
	
	ERR(suites.StreamSuite3()->AEGP_GetNewStreamValue(	S_my_id,
														scale_streamH,
														AEGP_LTimeMode_LayerTime,
														&key_timeT,
														TRUE,
														&val));
	if (!err) {
		val.val.two_d.x = rand() % 100 + 50;
		val.val.two_d.y = val.val.two_d.x;
		ERR(suites.KeyframeSuite4()->AEGP_SetAddKeyframe(addKeyframesInfoH, new_indexL, &val));
	}
	ERR2(suites.StreamSuite3()->AEGP_DisposeStreamValue(&val));

	return err;
}


static	A_Err	
Mangle(void)
{
	A_Err				err					= A_Err_NONE,
						err2				= A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);

	AEGP_LayerH			current_layerH		= NULL;
	AEGP_StreamRefH		position_streamH	= NULL,
						marker_streamH		= NULL,
						scale_streamH		= NULL;
	AEGP_StreamFlags	stream_flags 		= AEGP_StreamFlag_NONE;
	A_Time 				key_timeT 			= {0,0};

	A_Boolean 			expressions_onB 	= FALSE,
						variesB 			= FALSE,
						invertedB			= FALSE,
						lockedB				= FALSE;

	A_long				num_kfsL			= 0,
						num_masksL			= 0;


	A_FpLong			minF 				= 0, 
						maxF 				= 0;

	AEGP_ColorVal		color				= {1.0, 1.0, 0.0, 0.0};
	AEGP_MemHandle 		expressionHZ 		= NULL;
	AEGP_AddKeyframesInfoH	addKeyframesInfoH	= NULL;

	ERR(suites.UtilitySuite3()->AEGP_StartUndoGroup(STR(StrID_Name)));				
	ERR(suites.LayerSuite5()->AEGP_GetActiveLayer(&current_layerH));

	ERR(suites.LayerSuite5()->AEGP_SetLayerName(current_layerH, STR(StrID_MessedUpLayerName)));
	ERR(suites.MaskSuite4()->AEGP_GetLayerNumMasks(current_layerH, &num_masksL));
				
	// If there are masks, mess with them. 
	
	if (!err && num_masksL){
		AEGP_MaskRefH		maskH = NULL;
		for (A_long iL = 0; !err && (iL < num_masksL); ++iL){
			ERR(suites.MaskSuite4()->AEGP_GetLayerMaskByIndex(current_layerH, iL, &maskH));
			if (maskH){
				AEGP_MaskMBlur blur_state = AEGP_MaskMBlur_ON;
				ERR(suites.MaskSuite4()->AEGP_SetMaskName(maskH, STR(StrID_MessedUpMaskName)));
				ERR(suites.MaskSuite4()->AEGP_SetMaskMotionBlurState( maskH, blur_state));
				ERR(suites.MaskSuite4()->AEGP_GetMaskInvert(maskH, &invertedB));
				ERR(suites.MaskSuite4()->AEGP_SetMaskInvert(maskH, !invertedB));
				ERR(suites.MaskSuite4()->AEGP_SetMaskMode(maskH, PF_MaskMode_ACCUM));
				ERR(suites.MaskSuite4()->AEGP_GetMaskLockState(maskH, &lockedB));
				ERR(suites.MaskSuite4()->AEGP_SetMaskLockState(maskH, !lockedB));
				ERR(suites.MaskSuite4()->AEGP_SetMaskColor(maskH, &color));
				// Always dispose, regardless of error state
				ERR2(suites.MaskSuite4()->AEGP_DisposeMask(maskH));	
			}
		}
	}
	ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(	S_my_id, 
														current_layerH,
														AEGP_LayerStream_MARKER,
														&marker_streamH));
	ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(	S_my_id,
														current_layerH,
														AEGP_LayerStream_SCALE,
														&scale_streamH));

	if (marker_streamH && scale_streamH){
		ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(	S_my_id, 
															current_layerH,
															AEGP_LayerStream_POSITION, 
															&position_streamH));

		ERR(suites.StreamSuite2()->AEGP_GetStreamProperties(position_streamH,
															&stream_flags,
															&minF,
															&maxF));
		// If there's an expression, take a look at it.
		// Expressions are simply NULL-terminated strings.
		
		ERR(suites.StreamSuite2()->AEGP_GetExpressionState(	S_my_id, position_streamH, &expressions_onB));
		ERR(suites.StreamSuite2()->AEGP_SetExpressionState(	S_my_id, position_streamH, !expressions_onB));
		ERR(suites.StreamSuite2()->AEGP_GetExpression(	S_my_id, position_streamH, &expressionHZ));
		if (!err && expressionHZ){
				A_char	*expP = NULL;
				ERR(suites.MemorySuite1()->AEGP_LockMemHandle(expressionHZ, reinterpret_cast<void**>(&expP)));
				if (expP){
					// Mean, simply mean...
					//strrev(layer_nameAC); //Apparently, no one needs to reverse strings in Mach-O. -bbb
					ERR(suites.StreamSuite2()->AEGP_SetExpression(S_my_id, position_streamH, expP));
				}
				ERR2(suites.MemorySuite1()->AEGP_FreeMemHandle(expressionHZ));
		}
		ERR(suites.StreamSuite2()->AEGP_IsStreamTimevarying(position_streamH, &variesB));
		if (variesB) {
			ERR(suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(position_streamH, &num_kfsL));
			if (num_kfsL) {

				// Use StartAddKeyframes for scale keyframes (it isn't supported for markers as of CS5)
				suites.KeyframeSuite4()->AEGP_StartAddKeyframes(scale_streamH, &addKeyframesInfoH);

				// 	For each position keyframe, put a marker and a scale keyframe in the stream
				for (A_long jL = 0; !err && (jL < num_kfsL) ; ++jL) {
					
					ERR(suites.KeyframeSuite4()->AEGP_GetKeyframeTime(	position_streamH,
																		jL,
																		AEGP_LTimeMode_LayerTime,
																		&key_timeT));
					AddMarker(marker_streamH, key_timeT);
					AddScaleKeyframe(addKeyframesInfoH, scale_streamH, key_timeT);
				}
				suites.KeyframeSuite4()->AEGP_EndAddKeyframes(TRUE, addKeyframesInfoH);
			}
		}
		// Else if no keyframes, tell the user to make some!
		else {
			ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_NoPositionKeyframes)));
		}
	}
	ERR2(suites.UtilitySuite3()->AEGP_EndUndoGroup());
	if (position_streamH){
		ERR2(suites.StreamSuite2()->AEGP_DisposeStream(position_streamH));
	}
	if (marker_streamH){
		ERR2(suites.StreamSuite2()->AEGP_DisposeStream(marker_streamH));
	}
	if (err) { // not !err, err!
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_UndoError)));
	}
	return err;
}

static 
A_Err
CommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,		/* >> */
	AEGP_CommandRefcon	refconPV,				/* >> */
	AEGP_Command		command,				/* >> */
	AEGP_HookPriority	hook_priority,			/* >> */
	A_Boolean			already_handledB,		/* >> */
	A_Boolean			*handledPB)				/* << */
{
	A_Err err = A_Err_NONE;
	if (command == S_Mangler_cmd){
		ERR(Mangle());
		*handledPB = TRUE;
	}
	return err;
}

static
A_Err
DeathHook(
	AEGP_GlobalRefcon	unused1,
	AEGP_DeathRefcon	unused2) 
{
	A_Err	err		=	A_Err_NONE;
#if REPORT_NUM_IDLE_CALLS
	AEGP_SuiteHandler suites(sP);
	A_char reportAC[AEGP_MAX_ABOUT_STRING_SIZE] = {'\0'};
	suites.ANSICallbacksSuite1()->sprintf(reportAC, "In case you were curious, Mangler's IdleHook was called %d times.", S_idle_count); 
	suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, reportAC);
#endif
	return err;
}
	
static	
A_Err	
IdleHook(
	AEGP_GlobalRefcon	plugin_refconP,	
	AEGP_IdleRefcon		refconP,		
	A_long 				*max_sleepPL)
{
	A_Err	err		=	A_Err_NONE;
	*max_sleepPL 	=	1000;
	S_idle_count++;
	return err;
}

A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,		/* >> */
	A_long				 	major_versionL,		/* >> */		
	A_long					minor_versionL,		/* >> */		
	AEGP_PluginID			aegp_plugin_id,		/* >> */
	AEGP_GlobalRefcon		*global_refconP)	/* << */
{
	S_my_id	= aegp_plugin_id;

	sP 		= pica_basicP;

	A_Err 					err 				= A_Err_NONE, 
							err2 				= A_Err_NONE;
							
	AEGP_SuiteHandler suites(pica_basicP);
	
	S_options.compH		= NULL;	

	ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_Mangler_cmd));
	
	
	if (!err && S_Mangler_cmd){
		ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(S_Mangler_cmd, 
															STR(StrID_Name), 
															AEGP_Menu_KF_ASSIST, 
															AEGP_MENU_INSERT_SORTED));
	}
	ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(	S_my_id, 
															AEGP_HP_BeforeAE, 
															AEGP_Command_ALL, 
															CommandHook, 
															NULL));

	ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(S_my_id, UpdateMenuHook, NULL));
	ERR(suites.RegisterSuite5()->AEGP_RegisterDeathHook(S_my_id, DeathHook, NULL));
	ERR(suites.RegisterSuite5()->AEGP_RegisterIdleHook(S_my_id, IdleHook, NULL));

	if (err){ // not !err, err!
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_AddCommandError)));
	}
	return err;
}