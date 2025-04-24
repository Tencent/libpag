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

/*	Paramarama.cpp	

	This sample exercises some of After Effects' image processing
	callback functions.

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Seemed like a good idea at the time						bbb			6/8/2004
	1.1			Added new entry point									zal			9/15/2017
	2.1			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

*/

#include "Paramarama.h"


static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
								"%s v%d.%d\r%s",
								STR(StrID_Name), 
								MAJOR_VERSION, 
								MINOR_VERSION, 
								STR(StrID_Description));
	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	out_data->out_flags = 	PF_OutFlag_DEEP_COLOR_AWARE;

	out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	
	return PF_Err_NONE;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);

	PF_ADD_SLIDER(	STR(StrID_Slider_Param_Name), 
					PARAMARAMA_AMOUNT_MIN, 
					PARAMARAMA_AMOUNT_MAX, 
					PARAMARAMA_AMOUNT_MIN, 
					PARAMARAMA_AMOUNT_MAX, 
					PARAMARAMA_AMOUNT_DFLT,
					AMOUNT_DISK_ID);
	
	AEFX_CLR_STRUCT(def);

	PF_ADD_COLOR(	STR(StrID_Color_Param_Name),
					DEFAULT_RED,
					DEFAULT_BLUE,
					DEFAULT_GREEN,
					COLOR_DISK_ID);
					
	AEFX_CLR_STRUCT(def);

	PF_ADD_FLOAT_SLIDER(STR(StrID_Float_Param_Name),
						static_cast<PF_FpShort>(FLOAT_MIN),
						FLOAT_MAX,
						FLOAT_MIN,
						FLOAT_MAX,
						AEFX_AUDIO_DEFAULT_CURVE_TOLERANCE,
						DEFAULT_FLOAT_VAL,
						2,
						0, // display flags
						1, // wants phase
						FLOAT_DISK_ID);
						
	AEFX_CLR_STRUCT(def);

	PF_ADD_CHECKBOX(STR(StrID_Checkbox_Param_Name), 
					STR(StrID_Checkbox_Description), 
					FALSE,
					0, 
					DOWNSAMPLE_DISK_ID);
					
	AEFX_CLR_STRUCT(def);

	PF_ADD_ANGLE(	STR(StrID_Angle_Param_Name),
					0,
					ANGLE_DISK_ID);
				
	AEFX_CLR_STRUCT(def);

	PF_ADD_POPUP(	STR(StrID_Popup_Param_Name), 
					5,
					1,
					STR(StrID_Popup_Choices),
					POPUP_DISK_ID);

	// Only add 3D point and button where supported, starting in AE CS5.5
	if (in_data->version.major >= PF_AE105_PLUG_IN_VERSION &&
		in_data->version.minor >= PF_AE105_PLUG_IN_SUBVERS) {

		AEFX_CLR_STRUCT(def);

		if (in_data->appl_id == 'FXTC') {

			PF_ADD_POINT_3D(STR(StrID_3D_Point_Param_Name), 
							DEFAULT_POINT_VALS, 
							DEFAULT_POINT_VALS, 
							DEFAULT_POINT_VALS, 
							THREE_D_POINT_DISK_ID);

		} else {

			// Add a placeholder for hosts that don't support 3D points
			PF_ADD_ARBITRARY2(	STR(StrID_3D_Point_Param_Name),
								1,
								1,
								0,
								PF_PUI_NO_ECW_UI,
								0,
								THREE_D_POINT_DISK_ID,
								0);
		}

		AEFX_CLR_STRUCT(def);

		PF_ADD_BUTTON(	STR(StrID_Button_Param_Name),
						STR(StrID_Button_Label_Name),
						0,
						PF_ParamFlag_SUPERVISE,
						BUTTON_DISK_ID);

		out_data->num_params = PARAMARAMA_NUM_PARAMS;

	} else {

		out_data->num_params = PARAMARAMA_NUM_PARAMS - 2;

	}

	return err;
}

static PF_Err
Render(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	PF_Err			err			= 	PF_Err_NONE;
	A_long			convKer[9] 	= 	{0};

	PF_FpLong		sharpen 	= 	PF_CEIL(params[PARAMARAMA_AMOUNT]->u.sd.value / 16),
					kernelSum 	= 	256*9;

	// If sharpen is set to 0, just copy the source to the destination
	if (0 == params[PARAMARAMA_AMOUNT]->u.sd.value) {

		// Premiere Pro/Elements doesn't support WorldTransformSuite1,
		// but it does support many of the callbacks in utils
		if (PF_Quality_HI == in_data->quality && in_data->appl_id != 'PrMr')	{	
			ERR(suites.WorldTransformSuite1()->copy_hq(in_data->effect_ref,
														&params[0]->u.ld, 
														output, 
														NULL, 
														NULL));
		} else if (in_data->appl_id != 'PrMr')	{
			ERR(suites.WorldTransformSuite1()->copy(	in_data->effect_ref,
														&params[0]->u.ld, 
														output, 
														NULL, 
														NULL));
		} else {
			ERR(PF_COPY(&params[0]->u.ld,
						output,
						NULL,
						NULL));
		}

	} else {
		/*	This code is taken directly from the After Effects
			Sharpen filter, modernized to use suites instead of
			our old callback function macros.
		*/
		
		convKer[4]	= (long)(sharpen * kernelSum);
		kernelSum	= (256 * 9 - convKer[4]) / 4;
		convKer[1]	= convKer[3] = convKer[5] = convKer[7] = (long)kernelSum;

		// Premiere Pro/Elements doesn't support WorldTransformSuite1,
		// but it does support many of the callbacks in utils
		if (in_data->appl_id != 'PrMr')	{
			ERR(suites.WorldTransformSuite1()->convolve(	in_data->effect_ref,
															&params[0]->u.ld, 
															&in_data->extent_hint,
															PF_KernelFlag_2D | PF_KernelFlag_CLAMP, 
															KERNEL_SIZE,
															convKer, 
															convKer, 
															convKer, 
															convKer, 
															output));
		} else {
			in_data->utils->convolve(	in_data->effect_ref,
										&params[0]->u.ld, 
										&in_data->extent_hint,
										PF_KernelFlag_2D | PF_KernelFlag_CLAMP, 
										KERNEL_SIZE,
										convKer, 
										convKer, 
										convKer, 
										convKer, 
										output);
		}
	}

	return err;
}

static PF_Err
UserChangedParam(
	PF_InData						*in_data,
	PF_OutData						*out_data,
	PF_ParamDef						*params[],
	const PF_UserChangedParamExtra	*which_hitP)
{
	PF_Err err = PF_Err_NONE;

	if (which_hitP->param_index == PARAMARAMA_BUTTON) {
		if (in_data->appl_id != 'PrMr') {
			PF_STRCPY(	out_data->return_msg, 
						STR(StrID_Button_Message));
			out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
		} else {
			// In Premiere Pro, this message will appear in the Events panel
			PF_STRCPY(	out_data->return_msg, 
						STR(StrID_Button_Message));
		}
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
		"Paramarama", // Name
		"ADBE Paramarama", // Match Name
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
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:

				err = About(in_data,
							out_data,
							params,
							output);
				break;

			case PF_Cmd_GLOBAL_SETUP:

				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_PARAMS_SETUP:

				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_RENDER:

				err = Render(	in_data,
								out_data,
								params,
								output);
				break;

			case PF_Cmd_USER_CHANGED_PARAM:

				err = UserChangedParam(	in_data,
										out_data,
										params,
										reinterpret_cast<const PF_UserChangedParamExtra *>(extra));
				break;
		}
	} catch(PF_Err &thrown_err) {
		err = thrown_err;
	}
	return err;
}
