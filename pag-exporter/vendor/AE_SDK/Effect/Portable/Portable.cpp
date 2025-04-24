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
	Portable.cpp

	This sample demonstrates how to detect (and, to a slight extent, respond to) different hosts.
	
	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Created for use in Premiere AE API support testing 		bbb
	2.0			Put on some weight. In a good way.						bbb
	3.0			Removed redundant code already in Gamma Table sample.	zal			8/15/2011
	3.1			Added new entry point									zal			9/15/2017
	3.2			Remove deprecated 'register' keyword					cb			12/18/2020
	3.3			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023
*/

#include "Portable.h"

// Check the host app and version number in in_data
// Return an appropriate message in out_data

static PF_Err
DetectHost (
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;

	switch (in_data->appl_id){

		case 'FXTC':
			if (in_data->version.major >= 12)
			{
                if (in_data->version.major == PF_AE171_PLUG_IN_VERSION && in_data->version.minor >= PF_AE171_PLUG_IN_SUBVERS) {
                    PF_SPRINTF(out_data->return_msg, "Running in After Effects 2020 (17.1) or later.");
                    
                } else if (in_data->version.major == PF_AE170_PLUG_IN_VERSION && in_data->version.minor == PF_AE170_PLUG_IN_SUBVERS) {
                    PF_SPRINTF(out_data->return_msg, "Running in After Effects 2020 (17.0).");
                    
                } else if (in_data->version.major == PF_AE161_PLUG_IN_VERSION && in_data->version.minor == PF_AE161_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects 2019 (16.1).");

				} else if (in_data->version.major == PF_AE160_PLUG_IN_VERSION && in_data->version.minor >= PF_AE160_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC 2019 (16.0).");

				} else if (in_data->version.major == PF_AE151_PLUG_IN_VERSION && in_data->version.minor >= PF_AE151_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC 2018 (15.1).");
					
				} else if (in_data->version.major == PF_AE150_PLUG_IN_VERSION && in_data->version.minor >= PF_AE150_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC 2017 (15.0).");

				} else if (in_data->version.major == PF_AE140_PLUG_IN_VERSION && in_data->version.minor >= PF_AE140_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC 2017 (14.0).");

				} else if (in_data->version.major == PF_AE138_PLUG_IN_VERSION && in_data->version.minor == PF_AE138_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC 2015.3 (13.8).");

				} else if (in_data->version.major == PF_AE136_PLUG_IN_VERSION && in_data->version.minor == PF_AE136_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC 2015.1 or 2015.2 (13.6 or 13.7).");

				} else if (in_data->version.major == PF_AE135_PLUG_IN_VERSION && in_data->version.minor == PF_AE135_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC 2015 (13.5).");

				} else if (in_data->version.major == PF_AE130_PLUG_IN_VERSION && in_data->version.minor == PF_AE130_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC 2014 (13.0 - 13.2).");

				} else if (in_data->version.major == PF_AE122_PLUG_IN_VERSION && in_data->version.minor == PF_AE122_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC (12.2).");

				} else if (in_data->version.major == PF_AE121_PLUG_IN_VERSION && in_data->version.minor == PF_AE121_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC (12.1).");

				} else if (in_data->version.major == PF_AE120_PLUG_IN_VERSION && in_data->version.minor == PF_AE120_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CC (12.0).");

				} else if (in_data->version.major == PF_AE1101_PLUG_IN_VERSION && in_data->version.minor == PF_AE1101_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CS6.0.1 or CS6.0.2.");

				} else if (in_data->version.major == PF_AE110_PLUG_IN_VERSION && in_data->version.minor == PF_AE110_PLUG_IN_SUBVERS) {
					PF_SPRINTF(out_data->return_msg, "Running in After Effects CS6.0.");

				} else {
					
					//	Q. How can I tell the difference between versions where the API version is the same, such as AE 6.5 and 7.0?
					
					//	A. The effect API didn't change the only way to differentiate
					//	between them is to check for the presence of a version of a 
					//	suite new in 7.0. Say, something 32bpc-ish. To avoid AEGP_SuiteHandler
					//	throwing if	the suite isn't present, we'll acquire it the old-school way.
					
					PF_iterateFloatSuite2 *ifsP = NULL;
					
					PF_Err just_checking_err = AEFX_AcquireSuite(	in_data, 
																	out_data, 
																	kPFIterateFloatSuite, 
																	kPFIterateFloatSuiteVersion2, 
																	NULL, 
																	reinterpret_cast<void**>(&ifsP));
					if (!just_checking_err){
						if (!ifsP){
							PF_SPRINTF(out_data->return_msg, "Running in After Effects 6.5 or earlier.");
						} else {
							PF_SPRINTF(out_data->return_msg, "Running in After Effects between 7.0 and CS4.");
						}

						//	Thanks, you have your suite back...
						
						just_checking_err = AEFX_ReleaseSuite(in_data, 
															  out_data, 
															  kPFIterateFloatSuite, 
															  kPFIterateFloatSuiteVersion2,
															  NULL);
					}
				}
			} else {	// Wow, an antique!
				
				//	DisplayWarningAboutUsingPluginInOldHost();
				//	SuggestUpgradingAESoYourPluginGetsNewAPIFeatures();
				PF_SPRINTF(out_data->return_msg, "Hey, some unknown version of After Effects!");

			} 
			break;
		
		case 'PrMr':
			{
				// The major/minor versions provide basic differentiation.
				// If you need finer granularity, e.g. differentiating between
				// PPro CC 2015.3 and CC 2017, then use the App Info Suite from
				// the Premiere Pro SDK
				if (in_data->version.major == 13 && in_data->version.minor >= 4) {
					PF_SPRINTF(out_data->return_msg, "Hey, Premiere Pro CC, CC 2014, or later!");

				} else if (in_data->version.major == 13 && in_data->version.minor == 2) {
					PF_SPRINTF(out_data->return_msg, "Hey, Premiere Pro CS6!");

				} else {
					PF_SPRINTF(out_data->return_msg, "Hey, some unknown version of Premiere!");
				}
			}
			break;

		default:
			PF_SPRINTF(out_data->return_msg, "Wow, we're running in some oddball host.");
			break;
	}

	return err;
}

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_SPRINTF(	out_data->return_msg, 
				"%s, v%d.%d\r%s",
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
	PF_Err		err					= PF_Err_NONE;

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

	PF_ADD_FLOAT_SLIDERX(	"Mix channels", 
							PORTABLE_MIN, 
							PORTABLE_BIG_MAX, 
							PORTABLE_MIN, 
							PORTABLE_MAX, 
							PORTABLE_DFLT,
							SLIDER_PRECISION, 
							DISPLAY_FLAGS,
							0,
							PORTABLE_DISK_ID);

	out_data->num_params = PORTABLE_NUM_PARAMS;

	return err;
}


static PF_Err 
SequenceSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err	= PF_Err_NONE;

	// Run the host-detection code when the effect is first applied to a clip
	// In After Effects, the message will display as a popup
	// In Premiere Pro, as of CS6, the message will be logged to the Events panel
	err = DetectHost(in_data, out_data);

	return err;
}

// This function is used to calculate the per-pixel result of the effect

static PF_Err 
PortableFunc (	
	void *refcon, 
	A_long x, 
	A_long y, 
	PF_Pixel *inP, 
	PF_Pixel *outP)
{
	PF_Err				err			= PF_Err_NONE;
	PortableRenderInfo	*sliderP	= (PortableRenderInfo*)refcon;
	A_FpShort			average		= 0;
	A_FpShort			midway_calc	= 0;
	
	// Mix the values
	// The higher the slider, the more we blend the channel with the average of all channels
	if (sliderP){
		average = (A_FpShort)(inP->red + inP->green + inP->blue) / 3;

		outP->alpha	= inP->alpha;
		midway_calc = (sliderP->slider_value * average);
		midway_calc = ((sliderP->slider_value * average) + (PORTABLE_MAX - sliderP->slider_value) * inP->red);
		outP->red	= (A_u_char) MIN(PF_MAX_CHAN8, ((sliderP->slider_value * average) + (100.0 - sliderP->slider_value) * inP->red) / 100.0);
		outP->green	= (A_u_char) MIN(PF_MAX_CHAN8, ((sliderP->slider_value * average) + (100.0 - sliderP->slider_value) * inP->green) / 100.0);
		outP->blue	= (A_u_char) MIN(PF_MAX_CHAN8, ((sliderP->slider_value * average) + (100.0 - sliderP->slider_value) * inP->blue) / 100.0);
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
	PF_Err				err			 		= PF_Err_NONE;
	PortableRenderInfo	render_info;

	render_info.slider_value = params[PORTABLE_SLIDER]->u.fs_d.value;
	
	// If the slider is 0 just make a direct copy.
	
	if (render_info.slider_value < 0.001) {
		ERR(PF_COPY(&params[0]->u.ld, 
					output, 
					NULL, 
					NULL));
	} else {
		 
		// clear all pixels outside extent_hint.

		if (in_data->extent_hint.left	!=	output->extent_hint.left	||
			in_data->extent_hint.top	!=	output->extent_hint.top		||
			in_data->extent_hint.right	!=	output->extent_hint.right	||
			in_data->extent_hint.bottom	!=	output->extent_hint.bottom) {
	
			ERR(PF_FILL(NULL, &output->extent_hint, output));
		}

		if (!err) {
		
			// iterate over image data.

			A_long progress_heightL = in_data->extent_hint.top - in_data->extent_hint.bottom;
			
			ERR(PF_ITERATE(	0, 
							progress_heightL,
							&params[PORTABLE_INPUT]->u.ld, 
							&in_data->extent_hint,
							(void*)&render_info, 
							PortableFunc, 
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
		"Portable", // Name
		"ADBE Portable", // Match Name
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
		case PF_Cmd_RENDER:
			err = Render(in_data,out_data,params,output);
			break;
	}
	return err;
}

