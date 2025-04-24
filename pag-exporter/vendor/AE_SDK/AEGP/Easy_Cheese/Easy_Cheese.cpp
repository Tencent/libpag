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
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/


/*	
	Easy_Cheese.cpp

	Demonstrates usage of Keyframer API, specifically the 
	KeyFrameSuite. 
	

	version		notes							engineer		date
	
	1.0			First implementation			bbb				10/15/00
	2.0			Added AEGP_SuiteHandler usage	bbb				11/6/00
	2.5			Added ADM Palette handling		bbb				1/4/01
	3.0			Re-write; updated for 5.5		bbb				8/21/01
	3.1			Removed ADM (now exercised in	bbb				3/31/02
				Mangler sample), cleaned up
				functionality and names.
	3.1.1		Fixed a bug setting temporal	zal				10/15/14
				ease when calling
				AEGP_Get/SetKeyframeTemporalEase
	
*/

#include "Easy_Cheese.h"

static AEGP_Command			S_Easy_Cheese_cmd	=	0L;
static AEGP_PluginID		S_my_id				=	0L;
static A_long				S_idle_count		=	0L;
static SPBasicSuite			*sP					=	NULL;

static	A_Err	IdleHook(
	AEGP_GlobalRefcon	plugin_refconP,	
	AEGP_IdleRefcon		refconP,		
	A_long				*max_sleepPL)
{
	A_Err					err			= A_Err_NONE;
	S_idle_count++;
	return err;
}

static A_Err
UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,	/* >> */
	AEGP_UpdateMenuRefcon	refconPV,			/* >> */
	AEGP_WindowType			active_window)		/* >> */
{
	A_Err 				err 			=	A_Err_NONE,
						err2			=	A_Err_NONE;
		
	AEGP_ItemH			active_itemH	=	NULL;
	
	AEGP_ItemType		item_type		=	AEGP_ItemType_NONE;
	
	AEGP_SuiteHandler	suites(sP);
	
	err = suites.ItemSuite6()->AEGP_GetActiveItem(&active_itemH);

	if (!err && active_itemH){
		err	=	suites.ItemSuite6()->AEGP_GetItemType(active_itemH, &item_type);
		
		if (!err && (	AEGP_ItemType_COMP 		==	item_type	||
						AEGP_ItemType_FOOTAGE	==	item_type))	{
			ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_Easy_Cheese_cmd));
		}
	} else {
		ERR2(suites.CommandSuite1()->AEGP_DisableCommand(S_Easy_Cheese_cmd));
	}					
	return err;
}

static A_Err
PrepareForCheese(
	AEGP_CompH		*current_compPH,
	AEGP_ItemH		*active_itemPH)
{
	A_Err			err 					= 	A_Err_NONE;
	
	AEGP_ItemType	type	 				= 	AEGP_ItemType_NONE;

	A_char			item_nameAC[AEGP_MAX_ITEM_NAME_SIZE] = 	{'\0'};

	AEGP_SuiteHandler	suites(sP);

	err	=	suites.ItemSuite6()->AEGP_GetActiveItem(active_itemPH);

	if (!err && active_itemPH){
		err = suites.ItemSuite6()->AEGP_GetItemType(*active_itemPH, &type);
	}

	// If we have an active comp, let's do this thing! 
	
	if (!err && (AEGP_ItemType_COMP == type)) {
		ERR(suites.CompSuite4()->AEGP_GetCompFromItem(*active_itemPH, current_compPH));
	}

	if (current_compPH)	{
		
		/*	Bonus behavior: since we have the item,
			let's read it's name.
		*/
		ERR(suites.ItemSuite6()->AEGP_GetItemName(*active_itemPH, item_nameAC));
	}	
	
	ERR(suites.UtilitySuite3()->AEGP_StartUndoGroup("Easy Cheese!"));

	return err;
}	

static A_Err
GetLayerToBeCheesed(
	const	AEGP_CompH		compH,
			AEGP_LayerH		*layerPH)
{
	AEGP_SuiteHandler	suites(sP);
	
	return	suites.LayerSuite5()->AEGP_GetCompLayerByIndex(compH, 	0, layerPH);
}

static A_Err
PlayWithMasks(
	AEGP_LayerH		*layerPH)
{
	A_Err				err			=	A_Err_NONE;
	AEGP_MaskRefH		maskH		=	NULL;
	AEGP_MaskMBlur		blur_state	=	AEGP_MaskMBlur_OFF;
	A_long				num_masksL	=	0;
	
	AEGP_SuiteHandler	suites(sP);

	ERR(suites.MaskSuite4()->AEGP_GetLayerNumMasks(*layerPH, &num_masksL));
				
	// If there are masks, mess around with them (technically speaking). 
		
	if (!err && num_masksL){
		err = suites.MaskSuite4()->AEGP_GetLayerMaskByIndex(	*layerPH,
															0,
															&maskH);
	
		if (!err && maskH){
			err = suites.MaskSuite4()->AEGP_GetMaskMotionBlurState(	maskH, &blur_state);
			if (!err){
				blur_state = AEGP_MaskMBlur_ON;
				err = suites.MaskSuite4()->AEGP_SetMaskMotionBlurState(	maskH, blur_state);
			}
			ERR(suites.MaskSuite4()->AEGP_SetMaskInvert(	maskH,	TRUE));
			ERR(suites.MaskSuite4()->AEGP_SetMaskMode(	maskH, PF_MaskMode_DARKEN));
		}
	}
	return err;
}

static	A_Err
PlayWithExpressions(
	AEGP_StreamRefH		*stream_refPH)
{
	A_Err				err 			= 	A_Err_NONE,
						err2 			= 	A_Err_NONE;
						
	AEGP_MemHandle 		expressionHZ 	=	NULL;
	A_Boolean			expressions_onB =	FALSE;

	AEGP_SuiteHandler	suites(sP);
							
	// If there's an expression, take a look at it.
	// Expressions are simply NULL-terminated strings.
	
	err = suites.StreamSuite2()->AEGP_GetExpressionState(	S_my_id, *stream_refPH, &expressions_onB);

	if (!err && expressions_onB)
	{
		err = suites.StreamSuite2()->AEGP_GetExpression(	S_my_id, *stream_refPH, &expressionHZ);

		if (!err && *expressionHZ)
		{
				ERR(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, "I found an expression."));
				ERR2(suites.MemorySuite1()->AEGP_FreeMemHandle(expressionHZ));
		}
	}
	return err;
}

static A_Err
GetPositionStreamInfo(
	AEGP_LayerH		*layerPH,
	AEGP_StreamRefH	*position_streamPH,
	A_long			*num_keyframesPL,
	A_short			*value_dimsPS,
	A_Boolean		*variesPB)
{
	A_Err				err 				= A_Err_NONE;
						 
	AEGP_StreamFlags	stream_flags 		= AEGP_StreamFlag_NONE;
	

	A_FpLong			minF 				= 0, 
						maxF 				= 0;

	AEGP_SuiteHandler	suites(sP);
							
	err = suites.StreamSuite2()->AEGP_GetNewLayerStream(S_my_id, 
														*layerPH,
														AEGP_LayerStream_POSITION, 
														position_streamPH);
														
	if(!err && *position_streamPH) {
		err = suites.StreamSuite2()->AEGP_GetStreamProperties(	*position_streamPH,
																&stream_flags,
																&minF,
																&maxF);
	}
	ERR(suites.StreamSuite2()->AEGP_IsStreamTimevarying(*position_streamPH, variesPB));
	ERR(suites.KeyframeSuite3()->AEGP_GetStreamValueDimensionality(*position_streamPH, value_dimsPS));
	return err;
}

static A_Err
MessWithMarkers(
	AEGP_LayerH		*layerPH,
	AEGP_StreamRefH	*stream_refPH,
	A_Time			*timePT)
{
	A_Err				err 				=  A_Err_NONE, 
						err2 				=  A_Err_NONE;

	A_long				new_indexL			= 0;
						
						
	AEGP_StreamValue	value;	
					
	AEGP_SuiteHandler	suites(sP);
	
	AEFX_CLR_STRUCT(value);	
	
	
	ERR(suites.KeyframeSuite3()->AEGP_InsertKeyframe(*stream_refPH,
													AEGP_LTimeMode_LayerTime,
													timePT,
													&new_indexL));
	
	ERR(suites.KeyframeSuite3()->AEGP_GetNewKeyframeValue(	S_my_id,
															*stream_refPH,
															new_indexL,
															&value));

	if (!err){
		suites.ANSICallbacksSuite1()->strcpy((*(value.val.markerH))->nameAC, STR(StrID_MarkerText));
		suites.ANSICallbacksSuite1()->strcpy((*(value.val.markerH))->urlAC, STR(StrID_URL));
		suites.ANSICallbacksSuite1()->sprintf((*(value.val.markerH))->chapterAC, STR(StrID_Chapter), new_indexL + 1);

		err = suites.KeyframeSuite3()->AEGP_SetKeyframeValue(*stream_refPH, new_indexL, &value);
	}

	if (value.streamH){
		ERR2(suites.StreamSuite2()->AEGP_DisposeStreamValue(&value));
	}

	return err;
}

static A_Err
GetCheesyWithIt(
	AEGP_StreamRefH		*stream_refPH,
	A_long				*keyframe_indexPL,
	A_short				*value_dimsPS)
{
	A_Err			err 		= 	A_Err_NONE;

	AEGP_KeyframeEase	ease_in, 
						ease_out;

	A_short				temporal_dimsS		= 0;
	
	AEFX_CLR_STRUCT(ease_in);
	AEFX_CLR_STRUCT(ease_out);
					
	AEGP_SuiteHandler	suites(sP);
					
	
	ERR(suites.KeyframeSuite3()->AEGP_GetStreamTemporalDimensionality(	*stream_refPH, &temporal_dimsS));

	ERR(suites.KeyframeSuite3()->AEGP_GetKeyframeTemporalEase(	*stream_refPH,
																*keyframe_indexPL,
																temporal_dimsS - 1,
																&ease_in,
																&ease_out));
	if (!err) {
		ease_in.speedF		=	ease_out.speedF		= 0.0;
		ease_in.influenceF	=	ease_out.influenceF = .33333333;
		
		err = suites.KeyframeSuite3()->AEGP_SetKeyframeTemporalEase(*stream_refPH,
																	*keyframe_indexPL,
																	temporal_dimsS - 1,
																	&ease_in,
																	&ease_out);
		ERR(suites.KeyframeSuite3()->AEGP_SetKeyframeInterpolation( *stream_refPH,
																	*keyframe_indexPL,
																 	AEGP_KeyInterp_BEZIER,
																 	AEGP_KeyInterp_BEZIER));
	}
	return err;
}
			
static A_Err
CommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,		/* >> */
	AEGP_CommandRefcon	refconPV,				/* >> */
	AEGP_Command		command,				/* >> */
	AEGP_HookPriority	hook_priority,			/* >> */
	A_Boolean			already_handledB,		/* >> */
	A_Boolean			*handledPB)				/* << */
{
	A_Err			err 		= 	A_Err_NONE; 

					
	AEGP_SuiteHandler	suites(sP);
	
	try {
		if (S_Easy_Cheese_cmd == command){
			*handledPB = TRUE;
		
			A_Err				err2 				= A_Err_NONE;
			AEGP_CompH			current_compH		= NULL;
			AEGP_ItemH			active_itemH 		= NULL;
			AEGP_LayerH			current_layerH		= NULL;
	 		AEGP_StreamRefH		position_streamH	= NULL,
								marker_streamH		= NULL;
			A_Boolean 			variesB 			= FALSE;
			A_Time 				key_timeT 			= {0,1};
			A_long				num_keyframesL		= 0;
			A_short 			value_dimsS 		= 0;

			err	=	PrepareForCheese(&current_compH, &active_itemH);
			
			if (!err && current_compH) {
				err	=	GetLayerToBeCheesed(current_compH, &current_layerH);
			}

			if (!err && current_layerH) {
				err	=	PlayWithMasks(&current_layerH);
			}
			
			ERR(GetPositionStreamInfo(	&current_layerH, 
										&position_streamH,
										&num_keyframesL,
										&value_dimsS, 
										&variesB));
		
			if (!err && position_streamH){
				err	=	PlayWithExpressions(&position_streamH);
			}
			
			if (!err && variesB) {
				err = suites.KeyframeSuite3()->AEGP_GetStreamNumKFs(position_streamH, &num_keyframesL);
			
				if (!err && num_keyframesL) {
					err = suites.StreamSuite2()->AEGP_GetNewLayerStream(	S_my_id, 
																		current_layerH,
																		AEGP_LayerStream_MARKER,
																		&marker_streamH);

					for (A_long indexL = 0; (!err && indexL < num_keyframesL) ; indexL++){
						
						err = suites.KeyframeSuite3()->AEGP_GetKeyframeTime(	position_streamH,
																			indexL,
																			AEGP_LTimeMode_LayerTime,
																			&key_timeT);
							
						ERR(GetCheesyWithIt(	&position_streamH, 
												&indexL,
												&value_dimsS));

						if (!err && marker_streamH) {
							err	=	MessWithMarkers(&current_layerH, &marker_streamH, &key_timeT);
						}
					}
					ERR2(suites.StreamSuite2()->AEGP_DisposeStream(marker_streamH));
				}	
			}	
			if (position_streamH){
				ERR2(suites.StreamSuite2()->AEGP_DisposeStream(position_streamH));
			}
			ERR2(suites.UtilitySuite3()->AEGP_EndUndoGroup());
		}
	} catch (A_Err &thrown_err) {
		err = thrown_err;
	}
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
	S_my_id										= aegp_plugin_id;
	A_Err 					err 				= A_Err_NONE, 
		err2 				= A_Err_NONE;
	
	sP = pica_basicP;
	
	AEGP_SuiteHandler suites(pica_basicP);
	
	
	err = suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_Easy_Cheese_cmd);
	
	
	if (!err && S_Easy_Cheese_cmd) {
		ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(S_Easy_Cheese_cmd, "Easy Cheese", AEGP_Menu_KF_ASSIST, AEGP_MENU_INSERT_SORTED));
	} 
	ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(	S_my_id, 
															AEGP_HP_BeforeAE, 
															AEGP_Command_ALL, 
															CommandHook, 
															0));
	
	ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(S_my_id, UpdateMenuHook, 0));
	
	ERR(suites.RegisterSuite5()->AEGP_RegisterIdleHook(S_my_id, IdleHook, 0));
	
	if (err){ // not !err, err!
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, "Easy_Cheese : Could not register command hook."));
	}
	return err;
}
				   