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
	Custom_ECW_UI.cpp
	
	This plug-in demonstrates many Custom ECW UI features. 

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			created by												dmw
	1.1			modified for inclusion in the SDK by					bbb
	1.2			added PICA suite usage									jja
	1.3			fixed some drawing problems								bbb
	1.4			fixed more drawing problems								bbb
	1.5			enforced some CoSA guidelines,							bbb
				fixed adjust cursor behavior.
	1.6			replace macros with suites								bbb
	2.0			C++ification											bbb
	3.0			ported UI code to Drawbot								pn
	3.1			Added new entry point									zal			9/18/2017
	3.2			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

**/													

#include "Custom_ECW_UI.h"

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err						err	= PF_Err_NONE;
	
	AEGP_SuiteHandler			suites(in_data->pica_basicP);
	
	PF_AppPersonalTextInfo		personal_info;
	
	AEFX_CLR_STRUCT(personal_info);
	
	ERR(suites.AppSuite4()->PF_GetPersonalInfo(&personal_info));
	
	if (!err){
		suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
						"%s v%d.%d,\r%s\r%s\r%s",
						STR(StrID_Name), 
						MAJOR_VERSION, 
						MINOR_VERSION, 
						personal_info.name,
						personal_info.serial_str,
						STR(StrID_Description));
	}
	return err;
}


static PF_Err 
GlobalSetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err					err			= PF_Err_NONE;
	PF_Handle				globH		= NULL;
	my_global_dataP			globP		= NULL;
	
	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	globH					= suites.HandleSuite1()->host_new_handle(sizeof(my_global_data));
	
	if (globH) {
		globP					= reinterpret_cast<my_global_dataP>(suites.HandleSuite1()->host_lock_handle(globH));
		out_data->global_data	= globH;
		globP->number			= 93;

		// Premiere Pro/Elements does not support this suite
		if (in_data->appl_id != 'PrMr')
		{
			suites.ANSICallbacksSuite1()->strcpy(globP->name, STR(StrID_Frank));
		}
		else
		{
			#ifdef AE_OS_WIN
			strcpy_s(globP->name, 10, STR(StrID_Frank));
			#else
			strcpy(globP->name, STR(StrID_Frank));
			#endif
		}
	}

	out_data->out_flags		|=	PF_OutFlag_CUSTOM_UI 			|
								PF_OutFlag_PIX_INDEPENDENT 		| 
								PF_OutFlag_USE_OUTPUT_EXTENT	|
								PF_OutFlag_DEEP_COLOR_AWARE;
    
    out_data->out_flags2    |=  PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,	
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	return err;
}

static PF_Err
ParamsSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err 			err = PF_Err_NONE;
	PF_ParamDef		def;

	AEFX_CLR_STRUCT(def);

	def.flags		=	PF_ParamFlag_SUPERVISE;
	def.ui_flags	=	PF_PUI_CONTROL;
	def.ui_width	=	UI_BOX_WIDTH;
	def.ui_height	=	UI_BOX_HEIGHT;

	// Premiere Pro/Elements does not support a standard parameter type
	// with custom UI (bug #1235407).  Use an arbitrary or null parameter instead.
	if (in_data->appl_id != 'PrMr')
	{
		PF_ADD_COLOR(	STR(StrID_Color_Param_Name),  
						0, 
						0, 
						0,
						ECW_UI_COLOR_DISK_ID);
	}
	else
	{
		PF_ADD_ARBITRARY2(	STR(StrID_Color_Param_Name),
							UI_BOX_WIDTH,
							UI_BOX_HEIGHT,
							0,
							PF_PUI_CONTROL,
							0,
							ECW_UI_COLOR_DISK_ID,
							0);
		// [TODO] Implement arbitrary data selectors 
		// for a color data type
	}
					
	if (!err) {
		PF_CustomUIInfo			ci;

		AEFX_CLR_STRUCT(ci);
		
		ci.events				= PF_CustomEFlag_EFFECT;
 		
		ci.comp_ui_width		= 0;
		ci.comp_ui_height		= 0;
		ci.comp_ui_alignment	= PF_UIAlignment_NONE;
		
		ci.layer_ui_width		= 0;
		ci.layer_ui_height		= 0;
		ci.layer_ui_alignment	= PF_UIAlignment_NONE;
		
		ci.preview_ui_width		= 0;
		ci.preview_ui_height	= 0;
		ci.layer_ui_alignment	= PF_UIAlignment_NONE;

		err = (*(in_data->inter.register_ui))(in_data->effect_ref, &ci);
	}
	
	out_data->num_params = ECW_UI_NUM_PARAMS;

	return err;
}

static PF_Err 
PixelFunc8 (	
	void		*refcon,
	A_long		x,
	A_long		y,
	PF_Pixel8	*inP,
	PF_Pixel8	*outP)
{
	PF_Pixel8*	colorP	=	reinterpret_cast<PF_Pixel8*>(refcon);
	
	outP->red	= 	(inP->red 	+ 	colorP->red)	>> 1;
	outP->green	=	(inP->green	+	colorP->green)	>> 1;
	outP->blue	=	(inP->blue	+	colorP->blue)	>> 1;

	outP->alpha  = inP->alpha;

	return PF_Err_NONE;
}

static PF_Err 
PixelFunc16 (	
	void		*refcon,
	A_long		x,
	A_long		y,
	PF_Pixel16	*inP,
	PF_Pixel16	*outP)
{
	PF_Pixel8*	small_colorP	=	reinterpret_cast<PF_Pixel8*>(refcon);
	
	PF_Pixel16	color;
	
	color.red	=	CONVERT8TO16(small_colorP->red);
	color.green	=	CONVERT8TO16(small_colorP->green);
	color.blue	=	CONVERT8TO16(small_colorP->blue);
	
	outP->red	= 	(inP->green	+	color.green)	>> 1;
	outP->blue	=	(inP->blue	+	color.blue)		>> 1;
	outP->green =	(inP->green +	color.green)	>> 1;
	outP->alpha  = 	inP->alpha;

	return PF_Err_NONE;
}

static PF_Err	
Render(	
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output)
{
	PF_Err				err		= PF_Err_NONE;
	
	AEGP_SuiteHandler		suites(in_data->pica_basicP);
	PF_Iterate8Suite2 	*it8P	= suites.Iterate8Suite2();

	// Premiere Pro/Elements does not support this suite
	if (in_data->appl_id != 'PrMr')
	{
		PF_Iterate16Suite2 	*it16P	= suites.Iterate16Suite2();

		if (PF_WORLD_IS_DEEP(output)) {
			ERR(it16P->iterate(	in_data,
								0,
								(output->extent_hint.bottom - output->extent_hint.top),
								&params[ECW_UI_INPUT]->u.ld,
								&output->extent_hint,
								(void*)&params[ECW_UI_COLOR]->u.cd.value,
								PixelFunc16,
								output));		
		} else {
			ERR(it8P->iterate(	in_data,
								0,
								(output->extent_hint.bottom - output->extent_hint.top),
								&params[ECW_UI_INPUT]->u.ld,
								&output->extent_hint,
								(void*)&params[ECW_UI_COLOR]->u.cd.value,
								PixelFunc8,
								output));		
		}
	}
	else
	{
		ERR(it8P->iterate(	in_data,
					0,
					(output->extent_hint.bottom - output->extent_hint.top),
					&params[ECW_UI_INPUT]->u.ld,
					&output->extent_hint,
					(void*)&params[ECW_UI_COLOR]->u.cd.value,
					PixelFunc8,
					output));
	}
	return err;
}

static PF_Err	
GlobalSetdown(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err	err		=	PF_Err_NONE;

	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	if (in_data->global_data){
		suites.HandleSuite1()->host_dispose_handle(in_data->global_data);
	}
	return err;
}


extern "C" DllExport
PF_Err PluginDataEntryFunction2(
	PF_PluginDataPtr inPtr,
	PF_PluginDataCB2 inPluginDataCallBackPtr,
	SPBasicSuite* inSPBasicSuitePtr,
	const char* inHostName,
	const char* inHostVersion)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;

	result = PF_REGISTER_EFFECT_EXT2(
		inPtr,
		inPluginDataCallBackPtr,
		"Custom_ECW_UI", // Name
		"ADBE Custom_ECW_UI", // Match Name
		"Sample Plug-ins", // Category
		AE_RESERVED_INFO, // Reserved Info
		"EffectMain",	// Entry point
		"https://www.adobe.com");	// support URL

	return result;
}


PF_Err
EffectMain(
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra )
{
	
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			
			case PF_Cmd_ABOUT:
				err = About(in_data, out_data, params, output);
				break;
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(in_data, out_data, params, output);
				break;
			case PF_Cmd_GLOBAL_SETDOWN:
				err = GlobalSetdown(in_data, out_data, params, output);
				break;
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(in_data, out_data, params, output);
				break;
			case PF_Cmd_RENDER:
				err = Render(in_data, out_data, params, output);
				break;
			case PF_Cmd_EVENT:
				err = HandleEvent(in_data, out_data, params, output, reinterpret_cast<PF_EventExtra*>(extra));
				break;
			default:
				break;
		}
	} catch (PF_Err &thrown_err) {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}

