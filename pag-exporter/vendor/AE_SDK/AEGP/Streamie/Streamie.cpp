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
	Streamie.cpp
	
	Revision History

	Version		Change																					Engineer	Date
	=======		======																					========	======
		1.0		(from the mists of time)																dmw	
		1.x		Various fixes																			bbb
		1.6		Fix error when opening context menu or Layer menu with camera or light layer selected	zal			4/3/2015

*/

#include "Streamie.h"

static AEGP_PluginID	S_my_id				= 0;

static SPBasicSuite		*sP 				= NULL;

static AEGP_Command		S_Streamie_cmd 		= 0;

		
static A_Err
CheckForPaintStreams(
	AEGP_LayerH			layerH,						/* >> */
	AEGP_StreamRefH		*stroke_group_streamPH)		/* << */
{
	A_Err 				err 						= A_Err_NONE, 
						err2 						= A_Err_NONE;

	AEGP_ObjectType		object_type					= AEGP_ObjectType_NONE;

	A_Boolean			paint_foundB				= FALSE;
	
	A_long				num_strokesL			 	= 0;

	AEGP_StreamRefH		layer_base_streamH			= NULL,
						effects_streamH				= NULL,
						paint_streamH				= NULL;

	AEGP_StreamGroupingType	group_type				= AEGP_StreamGroupingType_NONE;

	AEGP_SuiteHandler	suites(sP);
	AEGP_ErrReportState	err_state;

	if (layerH){
		ERR(suites.LayerSuite8()->AEGP_GetLayerObjectType(layerH, &object_type));

		// Don't call AEGP_GetNewStreamRefByMatchname for AEGP_StreamGroupName_EFFECT_PARADE on a camera or light layer - it will pop an error
		if (object_type != AEGP_ObjectType_CAMERA && object_type != AEGP_ObjectType_LIGHT)
		{
			ERR(suites.DynamicStreamSuite2()->AEGP_GetNewStreamRefForLayer(	S_my_id, layerH, &layer_base_streamH));

			ERR(suites.DynamicStreamSuite2()->AEGP_GetNewStreamRefByMatchname(	S_my_id,
																				layer_base_streamH,
																				AEGP_StreamGroupName_EFFECT_PARADE, 
																				&effects_streamH));
			ERR(suites.DynamicStreamSuite2()->AEGP_GetStreamGroupingType(effects_streamH, &group_type));

			if (group_type == AEGP_StreamGroupingType_INDEXED_GROUP){
				ERR(suites.DynamicStreamSuite2()->AEGP_GetNewStreamRefByIndex(	S_my_id,
																				effects_streamH,
																				0, 
																				&paint_streamH));

				//	I'm quieting errors here, as AEGP_GetNewStreamRefByMatchname
				//	will complain if there aren't any paint strokes. 
				ERR(suites.UtilitySuite3()->AEGP_StartQuietErrors(&err_state));
				//	We don't check for errors, since it is not a problem if there are no paint strokes (thanks jwfearn)
				suites.DynamicStreamSuite2()->AEGP_GetNewStreamRefByMatchname(	S_my_id,
																				paint_streamH,
																				"ADBE Paint Group", 
																				stroke_group_streamPH);
				ERR(suites.UtilitySuite3()->AEGP_EndQuietErrors(false, &err_state));

				if (*stroke_group_streamPH){
					ERR(suites.DynamicStreamSuite2()->AEGP_GetNumStreamsInGroup(*stroke_group_streamPH, &num_strokesL));
					if (!err && num_strokesL){
						paint_foundB = TRUE;
					}
				}
			}
		}
	}
	if (layer_base_streamH){
		ERR2(suites.StreamSuite2()->AEGP_DisposeStream(layer_base_streamH));
	}
	if (effects_streamH){
		ERR2(suites.StreamSuite2()->AEGP_DisposeStream(effects_streamH));
	}
	if (paint_streamH){
		ERR2(suites.StreamSuite2()->AEGP_DisposeStream(paint_streamH));
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
	A_Err 				err 				= A_Err_NONE, 
						err2 				= A_Err_NONE;

	A_long				num_strokesL	 	= 0,
						new_indexL			= 0;
	
	A_char				nameAC[AEGP_MAX_STREAM_MATCH_NAME_SIZE]  = {'\0'};

	AEGP_LayerH 		layerH						= NULL;

	AEGP_StreamRefH		stroke_group_streamH		= NULL,
						stroke_streamH				= NULL;
	

	AEGP_SuiteHandler	suites(sP);
	
	if (command == S_Streamie_cmd) {
		try {
			ERR(suites.UtilitySuite3()->AEGP_StartUndoGroup(STR(StrID_Name)));
			ERR(suites.LayerSuite5()->AEGP_GetActiveLayer(&layerH));
			if (layerH){
				ERR(CheckForPaintStreams(layerH, &stroke_group_streamH));
				if (stroke_group_streamH){
					ERR(suites.DynamicStreamSuite2()->AEGP_GetNumStreamsInGroup(stroke_group_streamH, &num_strokesL));
					if (!err && num_strokesL){
						--num_strokesL;	// the last stream is "paint on transparency", which we don't want -bbb 
						for (A_long iL = 0; !err && iL < num_strokesL; ++iL){
							ERR(suites.DynamicStreamSuite2()->AEGP_GetNewStreamRefByIndex(	S_my_id,
																							stroke_group_streamH,
																							iL, 
																							&stroke_streamH));

							ERR(suites.StreamSuite2()->AEGP_GetStreamName(stroke_streamH, TRUE, nameAC)); // just for fun. 
							ERR(suites.DynamicStreamSuite2()->AEGP_ReorderStream(stroke_streamH, (num_strokesL - iL)));
							if (iL == 2){ // just picking one at random
								ERR(suites.DynamicStreamSuite2()->AEGP_SetStreamName(stroke_streamH, STR(StrID_ChangedName)));
							}
							if (iL == 4){ // just picking another one
								ERR(suites.DynamicStreamSuite2()->AEGP_DuplicateStream(S_my_id, stroke_streamH, &new_indexL));
							}
							if (iL == 3){ //...and another one
								ERR(suites.DynamicStreamSuite2()->AEGP_DeleteStream(stroke_streamH));
							}
						} 
					}		
				}
			}
			if (stroke_streamH){
				ERR2(suites.StreamSuite2()->AEGP_DisposeStream(stroke_streamH));
			}
			if (stroke_group_streamH){
				ERR2(suites.StreamSuite2()->AEGP_DisposeStream(stroke_group_streamH));
			}	
			*handledPB = TRUE;
			ERR2(suites.UtilitySuite3()->AEGP_EndUndoGroup());									
		} catch (A_Err &thrown_err){
			err = thrown_err;
		}	
		if (err) { // not !err, err!
			ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_Troubles)));
		}
	}
	return err;
}

static A_Err
UpdateMenuHook(
			   AEGP_GlobalRefcon		plugin_refconPV,	/* >> */
			   AEGP_UpdateMenuRefcon	refconPV,			/* >> */
			   AEGP_WindowType			active_window)		/* >> */
			   {
	A_Err 				err 					=	A_Err_NONE,
	err2					=	A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);
	AEGP_LayerH			layerH 					=	NULL;
	AEGP_StreamRefH		stroke_group_streamH 	=	NULL;
	
	try {
		ERR(suites.LayerSuite5()->AEGP_GetActiveLayer(&layerH));
		
		if (layerH){
			ERR(CheckForPaintStreams(layerH, &stroke_group_streamH));
			if (stroke_group_streamH){
				ERR(suites.CommandSuite1()->AEGP_SetMenuCommandName(S_Streamie_cmd, STR(StrID_Name)));
				ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_Streamie_cmd));
			} else	{
				ERR(suites.CommandSuite1()->AEGP_DisableCommand(S_Streamie_cmd));
			}
		} else {
			ERR(suites.CommandSuite1()->AEGP_SetMenuCommandName(S_Streamie_cmd, STR(StrID_CommandName)));
		}
		if (stroke_group_streamH){
			ERR2(suites.StreamSuite2()->AEGP_DisposeStream(stroke_group_streamH));
		}	
	} catch (A_Err &thrown_err){
		err = thrown_err;
	}	
	if (err) { // not !err, err!
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_Troubles)));
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
	A_Err 			err					=	A_Err_NONE, 
					err2 				=	A_Err_NONE;
					sP					=	pica_basicP;
					S_my_id				=	aegp_plugin_id;	

	AEGP_SuiteHandler	suites(sP);
	try {
		ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_Streamie_cmd));

		ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(	S_Streamie_cmd, 
															STR(StrID_Name), 
															AEGP_Menu_ANIMATION, 
															AEGP_MENU_INSERT_AT_TOP));

		ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(	S_my_id, 
																AEGP_HP_BeforeAE, 
																AEGP_Command_ALL, 
																CommandHook, 
																NULL));

		ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(S_my_id, UpdateMenuHook, NULL));
	} catch (A_Err &thrown_err){
		err = thrown_err;
	}	
	if (err) { // not !err, err!
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_Troubles)));
	}
	return err;
}

