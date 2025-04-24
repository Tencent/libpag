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

#include "Text_Twiddler.h"

static AEGP_PluginID	S_my_id				= 0;
static SPBasicSuite		*sP 				= NULL;
static AEGP_Command		S_Text_Twiddler_cmd = 0;

			
static A_Err
CommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,		/* >> */
	AEGP_CommandRefcon	refconPV,				/* >> */
	AEGP_Command		command,				/* >> */
	AEGP_HookPriority	hook_priority,			/* >> */
	A_Boolean			already_handledB,		/* >> */
	A_Boolean			*handledPB)				/* << */
{
	A_Err 				err 					= A_Err_NONE, 
						err2 					= A_Err_NONE;

	AEGP_StreamType		stream_type				= AEGP_StreamType_NO_DATA;
	AEGP_StreamRefH		new_streamH				= NULL,
						text_streamH			= NULL;

	A_Time				timeT					= {0,1};
	AEGP_LTimeMode		time_mode				= AEGP_LTimeMode_LayerTime;
	AEGP_LayerH			layerH 					= NULL;
	AEGP_StreamValue	val;
	
	AEGP_SuiteHandler	suites(sP);

	if (command == S_Text_Twiddler_cmd) {
		try {
			ERR(suites.UtilitySuite3()->AEGP_StartUndoGroup(STR(StrID_Name)));
			AEFX_CLR_STRUCT(val);	// wash before first use.
			ERR(suites.LayerSuite5()->AEGP_GetActiveLayer(&layerH));			
			
			// 	How do we know there's only one text layer selected? Because
			//	our command wouldn't have been enabled if it weren't. -bbb 5/12/04
			
			ERR(suites.DynamicStreamSuite2()->AEGP_GetNewStreamRefForLayer(S_my_id, layerH, &new_streamH));
			ERR(suites.StreamSuite2()->AEGP_GetNewLayerStream(	S_my_id, 
																layerH,
																AEGP_LayerStream_SOURCE_TEXT,
																&text_streamH));
			ERR(suites.StreamSuite2()->AEGP_GetStreamType(text_streamH, &stream_type));
			
			if (!err && (stream_type == AEGP_StreamType_TEXT_DOCUMENT)){
				ERR(suites.LayerSuite5()->AEGP_GetLayerCurrentTime(layerH, time_mode, &timeT));
				ERR(suites.StreamSuite2()->AEGP_GetNewStreamValue(	S_my_id,
																	text_streamH,
																	AEGP_LTimeMode_LayerTime,
																	&timeT,
																	TRUE,
																	&val));
																	
				//	Yes, Unicode looks funny. Use your favorite platform-specific
				//	routine to turn plain old char *'s into Unicode. Or just write hex!
				
				const A_u_short unicode[] = {0x0042, 0x0042, 0x0042};  // "BBB"

				A_long	lengthL = sizeof(unicode) / sizeof(A_u_short);
				
				ERR(suites.TextDocumentSuite1()->AEGP_SetText(val.val.text_documentH, unicode, lengthL));
				ERR(suites.StreamSuite2()->AEGP_SetStreamValue(S_my_id, text_streamH, &val));
				*handledPB = TRUE;
			}
			ERR2(suites.StreamSuite2()->AEGP_DisposeStreamValue(&val));
			if (new_streamH){
				ERR2(suites.StreamSuite2()->AEGP_DisposeStream(new_streamH));
			}
			if (text_streamH){
				ERR2(suites.StreamSuite2()->AEGP_DisposeStream(text_streamH));
			}
			ERR2(suites.UtilitySuite3()->AEGP_EndUndoGroup());
		} catch (A_Err &thrown_err){
			err = thrown_err;
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
	A_Err 				err 			=	A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);
	AEGP_LayerH			layerH 			=	NULL;
	AEGP_ObjectType		type			=	AEGP_ObjectType_NONE;
	
	try {
		ERR(suites.LayerSuite5()->AEGP_GetActiveLayer(&layerH));
		if (layerH){
			ERR(suites.LayerSuite5()->AEGP_GetLayerObjectType(layerH, &type));	
			if (type == AEGP_ObjectType_TEXT){
				ERR(suites.CommandSuite1()->AEGP_SetMenuCommandName(S_Text_Twiddler_cmd, STR(StrID_Name)));
				ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_Text_Twiddler_cmd));
			} else	{
				ERR(suites.CommandSuite1()->AEGP_DisableCommand(S_Text_Twiddler_cmd));
			}
		} else {
			ERR(suites.CommandSuite1()->AEGP_SetMenuCommandName(S_Text_Twiddler_cmd, STR(StrID_Selection)));
		}
	} catch (A_Err &thrown_err){
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
	A_Err 			err					=	A_Err_NONE, 
					err2 				=	A_Err_NONE;
					sP					=	pica_basicP;
					S_my_id				=	aegp_plugin_id;	

	try {
		AEGP_SuiteHandler	suites(sP);

		ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_Text_Twiddler_cmd));

		ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(	S_Text_Twiddler_cmd, 
															STR(StrID_Selection), 
															AEGP_Menu_LAYER, 
															AEGP_MENU_INSERT_AT_TOP));

		ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(	S_my_id, 
																AEGP_HP_BeforeAE, 
																AEGP_Command_ALL, 
																CommandHook, 
																NULL));

		ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(S_my_id, UpdateMenuHook, NULL));

		if (err) { // not !err, err!
			ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_Troubles)));
		}
	} catch (A_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}
