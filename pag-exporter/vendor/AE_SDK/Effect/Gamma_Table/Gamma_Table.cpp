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
	Gamma_Table.cpp
	
	This demonstrates:
		using a sequence data Handle
		iterating over the image pixels (using PF_ITERATE)
		calling the progress callback
		a Fixed Slider control
		the Extent Hint rectangle (from the InData structure)
		making the application display an error alert for us
		using the PF_ITERATE callback

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Created													rb			3/12/1993
	1.1			Rewritten												bbb
	1.2			Added Windows entry point, made most functions static
	1.5			Added new entry point									zal			9/15/2017
	1.6			Remove deprecated 'register' keyword					cb			12/18/2020
	2.1			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023
*/


#include "Gamma_Table.h"


static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_SPRINTF(	out_data->return_msg, 
				"%s, v%d.%d\r%s",
				NAME, 
				MAJOR_VERSION, 
				MINOR_VERSION, 
				DESCRIPTION);

	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err	err = PF_Err_NONE;
 
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	 
	out_data->out_flags |=	PF_OutFlag_PIX_INDEPENDENT |
							PF_OutFlag_USE_OUTPUT_EXTENT;

	out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

	return err;
}

static PF_Err 
ParamsSetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err			err = PF_Err_NONE;
	PF_ParamDef		def;
	AEFX_CLR_STRUCT(def);

	PF_ADD_FIXED(	"Gamma", 
					GAMMA_MIN, 
					GAMMA_BIG_MAX, 
					GAMMA_MIN, 
					GAMMA_MAX, 
					GAMMA_DFLT,
					1, 
					0,
					0,
					GAMMA_DISK_ID);

	out_data->num_params = GAMMA_NUM_PARAMS;

	return err;
}


static PF_Err 
SequenceSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	Gamma_Table		*g_tableP;
	A_long			iL = 0;

	if (out_data->sequence_data){
		PF_DISPOSE_HANDLE(out_data->sequence_data);
	}
	out_data->sequence_data = PF_NEW_HANDLE(sizeof(Gamma_Table));
	if (!out_data->sequence_data) {
		return PF_Err_INTERNAL_STRUCT_DAMAGED;
	}

	// generate base table

	g_tableP = *(Gamma_Table**)out_data->sequence_data;
	g_tableP->gamma_val = (1L << 16);

	for (iL = 0; iL <= PF_MAX_CHAN8; iL++){
		g_tableP->lut[iL] = (A_u_char)iL;
	}

	return PF_Err_NONE;
}

static PF_Err 
SequenceSetdown (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	if (in_data->sequence_data) {
		PF_DISPOSE_HANDLE(in_data->sequence_data);
		out_data->sequence_data = NULL;
	}
	return PF_Err_NONE;
}


static PF_Err 
SequenceResetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	if (!in_data->sequence_data) {
		return SequenceSetup(in_data, out_data, params, output);
	}
	return PF_Err_NONE;
}

// Computes the gamma-corrected pixel given the lookup table.

static PF_Err 
GammaFunc (	
	void *refcon, 
	A_long x, 
	A_long y, 
	PF_Pixel *inP, 
	PF_Pixel *outP)
{
	PF_Err		err = PF_Err_NONE;
	GammaInfo	*giP = (GammaInfo*)refcon;	
	
	if (giP){
		outP->alpha	= inP->alpha;
		outP->red	= giP->lut[ inP->red ];
		outP->green	= giP->lut[ inP->green ];
		outP->blue	= giP->lut[ inP->blue ];
	} else {
		err = PF_Err_BAD_CALLBACK_PARAM;
	}
	return err;
}

static PF_Err 
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err			err			 		= PF_Err_NONE;
	A_long	xL					= 0,
					progress_heightL	= 0;
	Gamma_Table		*g_tableP			= NULL;
	GammaInfo		gamma_info;
	
	AEFX_CLR_STRUCT(gamma_info);
	
	// If the gamma factor is exactly 1.0 just make a direct copy.
	
	if (params[GAMMA_GAMMA]->u.fd.value == (1L << 16)) {
		ERR(PF_COPY(&params[0]->u.ld, 
					output, 
					NULL, 
					NULL));
	} else {
		 
		// If no table exists, pop an error message.

		if (!out_data->sequence_data) {
			PF_STRCPY(out_data->return_msg, "Gamma effect invoked without lookup table");
			out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
			err = PF_Err_INTERNAL_STRUCT_DAMAGED;
		}
		
		if (!err){
			g_tableP = *(Gamma_Table **)out_data->sequence_data;
		
			if (g_tableP->gamma_val != params[GAMMA_GAMMA]->u.fd.value) {
			
				// if the table values are bad, regenerate table contents.
				 
				double	temp, gamma;

				g_tableP->gamma_val	= params[GAMMA_GAMMA]->u.fd.value;
				gamma				= (PF_FpLong)g_tableP->gamma_val / (double)(1L << 16);
				gamma				= 1.0/gamma;
				
				for (xL = 0; xL <= PF_MAX_CHAN8; ++xL) {
					temp = PF_POW((PF_FpLong)xL / 255.0, gamma);
					g_tableP->lut[xL] = (A_u_char)(temp * 255.0);
				}
			}
			
			// clear all pixels outside extent_hint.

			if (in_data->extent_hint.left	!=	output->extent_hint.left	||
				in_data->extent_hint.top	!=	output->extent_hint.top		||
				in_data->extent_hint.right	!=	output->extent_hint.right	||
				in_data->extent_hint.bottom	!=	output->extent_hint.bottom) {
		
				ERR(PF_FILL(NULL, 
							&output->extent_hint, 
							output));
			}
			
		}
		if (!err) {
		
			// iterate over image data.

			progress_heightL = in_data->extent_hint.top - in_data->extent_hint.bottom;
			gamma_info.lut = g_tableP->lut;
			
			ERR(PF_ITERATE(	0, 
							progress_heightL,
							&params[GAMMA_INPUT]->u.ld, 
							&in_data->extent_hint,
							(void*)&gamma_info, 
							GammaFunc, 
							output));
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
		"Gamma (Table)", // Name
		"ADBE Gamma", // Match Name
		"Sample Plug-ins", // Category
		AE_RESERVED_INFO, // Reserved Info
		"EffectMain",	// Entry point
		"https://www.adobe.com");	// support URL

	return result;
}


PF_Err
EffectMain (	
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err = PF_Err_NONE;
	
	switch (cmd) {
	case PF_Cmd_ABOUT:
		err = About(in_data,out_data,params,output);
		break;
	case PF_Cmd_GLOBAL_SETUP:
		err = GlobalSetup(in_data,out_data,params,output);
		break;
	case PF_Cmd_PARAMS_SETUP:
		err = ParamsSetup(in_data,out_data,params,output);
		break;
	case PF_Cmd_SEQUENCE_SETUP:
		err = SequenceSetup(in_data,out_data,params,output);
		break;
	case PF_Cmd_SEQUENCE_SETDOWN:
		err = SequenceSetdown(in_data,out_data,params,output);
		break;
	case PF_Cmd_SEQUENCE_RESETUP:
		err = SequenceResetup(in_data,out_data,params,output);
		break;
	case PF_Cmd_RENDER:
		err = Render(in_data,out_data,params,output);
		break;
	}
	return err;
}

#ifdef AE_OS_WIN
	BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
	{
		HINSTANCE my_instance_handle = (HINSTANCE)0;
		
		switch (dwReason)
		{
			case DLL_PROCESS_ATTACH:
			my_instance_handle = hDLL;
				break;

			case DLL_THREAD_ATTACH:
			my_instance_handle = hDLL;
				break;
			case DLL_THREAD_DETACH:
			my_instance_handle = 0;
				break;
			case DLL_PROCESS_DETACH:
			my_instance_handle = 0;
				break;
					break;
		}
		return(TRUE);
	}
#endif

