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

#include "QueueBert.h"

static AEGP_PluginID	S_my_id				= 0;

static SPBasicSuite		*sP 				= NULL;

static AEGP_Command		S_queuebert_cmd 	= 0;

			
static A_Err
CommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,		/* >> */
	AEGP_CommandRefcon	refconPV,				/* >> */
	AEGP_Command		command,				/* >> */
	AEGP_HookPriority	hook_priority,			/* >> */
	A_Boolean			already_handledB,		/* >> */
	A_Boolean			*handledPB)				/* << */
{
	A_Err 				err 	= A_Err_NONE;

	AEGP_SuiteHandler	suites(sP);
	
	if (command == S_queuebert_cmd) {
		A_long					outmod_countL		= 0L,
								rq_item_countL 		= 0L;

		AEGP_LogType			log_type 		= AEGP_LogType_NONE;
		AEGP_EmbeddingType		embed_type	 	= AEGP_Embedding_NONE;
		AEGP_PostRenderAction	post_action 	= AEGP_PostRenderOptions_NONE;
		AEGP_OutputTypes		output			= AEGP_OutputType_NONE;
		A_Boolean				stretchB		= FALSE,
								lock_aspectB	= FALSE,
								cropB			= FALSE,
								audio_enabledB	= FALSE;
		A_Time					startT			= {0,1},
								elapsedT		= {0,1};
		AEGP_RenderItemStatusType	status		= AEGP_RenderItemStatus_NONE;
		AEGP_VideoChannels		vid_channels	= AEGP_VideoChannels_NONE;
		AEGP_StretchQuality		qual			= AEGP_StretchQual_NONE;
		A_Rect					cropR			= {0,0,0,0};
		
		AEGP_SoundDataFormat	audio_settings;
		
		AEGP_OutputModuleRefH	omrefH			= NULL;
		
		A_char					commentZ[AEGP_MAX_RQITEM_COMMENT_SIZE]	 = {'\0'};
		
		AEGP_CompH				compH			= NULL; 
		AEGP_RQItemRefH			rq_itemH		= NULL;
		
		ERR(suites.RQItemSuite3()->AEGP_GetNumRQItems(&rq_item_countL));

		if (rq_item_countL){
			ERR(suites.RQItemSuite3()->AEGP_GetCompFromRQItem(0, &compH));
			if (!err){
				for (A_long iL = 0; !err && iL < 6; ++iL){
				ERR(suites.RenderQueueSuite1()->AEGP_AddCompToRenderQueue(	compH, "Yesod:output.mov"));
				ERR(suites.RQItemSuite3()->AEGP_GetNextRQItem((AEGP_RQItemRefH)iL, &rq_itemH));
				ERR(suites.RQItemSuite3()->AEGP_SetRenderState(rq_itemH, AEGP_RenderItemStatus_UNQUEUED));
				}
			}
		}

		if (rq_item_countL)	{

			ERR(suites.RQItemSuite3()->AEGP_GetNumOutputModulesForRQItem(0, &outmod_countL));
			if (outmod_countL) {
				ERR(suites.RQItemSuite3()->AEGP_GetLogType(0, &log_type));
				ERR(suites.RQItemSuite3()->AEGP_SetLogType(0, AEGP_LogType_PLUS_SETTINGS));
				ERR(suites.RQItemSuite3()->AEGP_GetRenderState(0, &status));
				ERR(suites.RQItemSuite3()->AEGP_SetRenderState(0, TRUE));
				ERR(suites.RQItemSuite3()->AEGP_GetStartedTime(0, &startT));
				ERR(suites.RQItemSuite3()->AEGP_GetElapsedTime(0, &elapsedT));
				
				//	Now, let's play with the zeroeth output module.	
				
				ERR(suites.OutputModuleSuite4()->AEGP_GetEnabledOutputs(0, 0, &output));

				output = AEGP_OutputType_AUDIO | AEGP_OutputType_VIDEO;

				ERR(suites.OutputModuleSuite4()->AEGP_SetEnabledOutputs(0, 0, output));
				ERR(suites.OutputModuleSuite4()->AEGP_GetEnabledOutputs(0, 0, &output));	// did it work?
				ERR(suites.OutputModuleSuite4()->AEGP_GetOutputChannels(0, 0, &vid_channels));
				ERR(suites.OutputModuleSuite4()->AEGP_SetOutputChannels(0, 0, AEGP_VideoChannels_RGBA));
				ERR(suites.OutputModuleSuite4()->AEGP_GetStretchInfo(	0, 
																0, 
																&stretchB,
																&qual,
																&lock_aspectB));
				ERR(suites.OutputModuleSuite4()->AEGP_SetStretchInfo(	0, 
																0, 
																TRUE,
																AEGP_StretchQual_HIGH));
				ERR(suites.OutputModuleSuite4()->AEGP_GetCropInfo(	0,
															0,
															&cropB,
															&cropR));
				if (!err) {
					cropR.right 	= 200;
					cropR.bottom 	= 100;
					
					ERR(suites.OutputModuleSuite4()->AEGP_SetCropInfo(	0,
																0,
																TRUE,
																cropR));
				}
				
				ERR(suites.OutputModuleSuite4()->AEGP_GetSoundFormatInfo(	0,
																	0,
																	&audio_settings,
																	&audio_enabledB));
																		
				if (!audio_enabledB || !audio_settings.sample_rateF){
					
					audio_settings.num_channelsL 		= 1;
					audio_settings.sample_rateF			= 44100;
					audio_settings.bytes_per_sampleL	= 4;
					audio_settings.encoding				= AEGP_SoundEncoding_FLOAT;

					ERR(suites.OutputModuleSuite4()->AEGP_SetSoundFormatInfo(	0,
																		0,
																		audio_settings,
																		audio_enabledB));
				}
																		
				
				ERR(suites.OutputModuleSuite4()->AEGP_GetEmbedOptions(	0, 0, &embed_type));
		
				ERR(suites.OutputModuleSuite4()->AEGP_SetEmbedOptions(	0, 0, AEGP_Embedding_LINK_AND_COPY));
				ERR(suites.OutputModuleSuite4()->AEGP_GetEmbedOptions(	0, 0, &embed_type));
				ERR(suites.OutputModuleSuite4()->AEGP_GetPostRenderAction(	0, 0, &post_action));
				ERR(suites.OutputModuleSuite4()->AEGP_SetPostRenderAction(	0, 0, AEGP_PostRenderOptions_IMPORT_AND_REPLACE_USAGE));
				ERR(suites.OutputModuleSuite4()->AEGP_GetPostRenderAction(	0, 0, &post_action));
				ERR(suites.RQItemSuite3()->AEGP_GetNumOutputModulesForRQItem(0, &outmod_countL));
				ERR(suites.OutputModuleSuite4()->AEGP_AddDefaultOutputModule(0, &omrefH));
				ERR(suites.RQItemSuite3()->AEGP_GetNumOutputModulesForRQItem(0, &outmod_countL));
				ERR(suites.RQItemSuite3()->AEGP_RemoveOutputModule(0,0));
				ERR(suites.OutputModuleSuite4()->AEGP_SetOutputFilePath(	0,
																	(AEGP_OutputModuleRefH)(outmod_countL - 1),	/* it's zero-based, remember?	*/
																	STR(StrID_Path)));
				ERR(suites.RQItemSuite3()->AEGP_GetComment(	0, commentZ));
				ERR(suites.RQItemSuite3()->AEGP_SetComment(	0, STR(StrID_Pronounce)));
			}
			
		} 
		*handledPB = TRUE;
	} else { 
		*handledPB = FALSE;
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
	A_long				num_rq_itemsL	=	0;
	AEGP_SuiteHandler	suites(sP);
	
	ERR(suites.RQItemSuite3()->AEGP_GetNumRQItems(&num_rq_itemsL));
	
	if (num_rq_itemsL){
		ERR(suites.CommandSuite1()->AEGP_EnableCommand(S_queuebert_cmd));
	} else {
		ERR(suites.CommandSuite1()->AEGP_DisableCommand(S_queuebert_cmd));
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

	err = suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_queuebert_cmd);

	if (S_queuebert_cmd){
		ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(S_queuebert_cmd, 
															STR(StrID_Name), 
															AEGP_Menu_FILE, 
															AEGP_MENU_INSERT_SORTED));

		ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(	S_my_id, 
																AEGP_HP_BeforeAE, 
																AEGP_Command_ALL, 
																CommandHook, 
																NULL));

		ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(	S_my_id, 
																	UpdateMenuHook, 
																	NULL));
	}
	if (err){// not !err, err!
		ERR2(suites.UtilitySuite3()->AEGP_ReportInfo(S_my_id, STR(StrID_Troubles)));
	}
	return err;
}

