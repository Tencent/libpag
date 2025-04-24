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

/**
	SDK_Backwards.cpp

	This effect exercises the new audio related fields in PF_InData & PF_OutData.
	Although the effect itself isnt' too exciting, this should serve as a good
	starting point for all types of audio plug-in.

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Created													jja			4/1/2001
	1.1			Edited for inclusion in SDK								bbb			5/1/2001
	1.2			Updated for 5.0; added PF_CHECKOUT_LAYER_AUDIO usage				9/1/2001
	1.3			Added new entry point									zal			9/15/2017
	1.4			Remove deprecated 'register' keyword					cb			12/18/2020
	1.5			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

**/

#include "SDK_Backwards.h"

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_SPRINTF(	out_data->return_msg, 
				"%s, v%d.%d\n%s",
				NAME, 
				MAJOR_VERSION, 
				MINOR_VERSION, 
				DESCRIPTION);

	return PF_Err_NONE;
}


static PF_Err 
GlobalSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	out_data->out_flags 	=	PF_OutFlag_AUDIO_EFFECT_ONLY	|
								PF_OutFlag_AUDIO_FLOAT_ONLY	|
								PF_OutFlag_AUDIO_IIR;

	out_data->out_flags2 = PF_OutFlag2_SUPPORTS_THREADED_RENDERING;

	out_data->my_version 	= 	PF_VERSION(	MAJOR_VERSION, 
											MINOR_VERSION,
											BUG_VERSION, 
											STAGE_VERSION, 
											BUILD_VERSION);
	return PF_Err_NONE;
}


static PF_Err 
ParamsSetup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{

	PF_Err			err = PF_Err_NONE;
	PF_ParamDef		def;
	AEFX_CLR_STRUCT(def);


	PF_ADD_FLOAT_SLIDER("Tone Frequency", 
						AR_FREQ_MIN, 
						AR_FREQ_VALID_MAX, 
						AR_FREQ_MIN, 
						AR_FREQ_MAX, 
						AR_FREQ_CURVE_TOLER,
						AR_FREQ_DFLT,
						AR_FREQ_DISPLAY_PRECISION,
						AR_FREQ_DISPLAY_FLAGS, 
						TRUE,
						FREQ_DISK_ID);

	AEFX_CLR_STRUCT(def);
	
	PF_ADD_FLOAT_SLIDER("Tone Level", 
						AR_LEVEL_MIN, 
						AR_LEVEL_VALID_MAX, 
						AR_LEVEL_MIN, 
						AR_LEVEL_MAX, 
						AR_LEVEL_CURVE_TOLER,
						AR_LEVEL_DFLT,
						AR_LEVEL_DISPLAY_PRECISION,
						AR_LEVEL_DISPLAY_FLAGS, 
						FALSE,
						LEVEL_DISK_ID);

	out_data->num_params 	=	AR_NUM_PARAMS;
	out_data->out_flags 	|=	PF_OutFlag_AUDIO_EFFECT_ONLY;

	return err;
}

static PF_Err 
Audio_Setup (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err	err = PF_Err_NONE;

	out_data->start_sampL	= in_data->total_sampL - in_data->dur_sampL - in_data->start_sampL;		// calculate new sample
	out_data->dur_sampL		= in_data->dur_sampL;

	if (out_data->start_sampL < 0){	
		out_data->start_sampL = 0;
	}

	if (out_data->dur_sampL > in_data->total_sampL)	{
		out_data->dur_sampL	= in_data->total_sampL;
	}

	return	err;
}

#include "AEFX_SND_Stuff.cpp"

static PF_Err 
Audio_Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	
	
	PF_Err				err 				= PF_Err_NONE,
						err2 				= PF_Err_NONE;
	PF_LayerAudio		audio 				= NULL;
	PF_SndSamplePtr		data0 				= NULL;

	A_long				current_timeL 		= 0, 
						num_samples_bufferL	= 0, 
						durationL 			= 0, 
						out_dur_sampL 		= 0;
	PF_FpLong			levelF				= params[AR_LEVEL]->u.fs_d.value,
						freq_rateF			= params[AR_FREQ]->u.fs_d.value,
						phaseF				= params[AR_FREQ]->u.fs_d.phase,

	// second dimension of time varying audio parameters

						level2F				= params[AR_NUM_PARAMS + AR_LEVEL]->u.fs_d.value,
						freq_rate2F			= params[AR_NUM_PARAMS + AR_FREQ]->u.fs_d.value,
						inv_num_samplesF	= 1.0 / out_data->dur_sampL,
						inc_freq_rateF		= (freq_rate2F - freq_rateF) * inv_num_samplesF,
						inc_levelF			= (level2F - levelF) * inv_num_samplesF,
						inv_samp_rateF		= 1.0 / out_data->dest_snd.fi.rateF;
	
	
	current_timeL 	= in_data->current_time;
	out_dur_sampL = out_data->dur_sampL;

	ERR(PF_CHECKOUT_LAYER_AUDIO (	in_data,		
									AR_INPUT,					// Param index
									in_data->start_sampL,			// start time
									in_data->dur_sampL,			// duration
									in_data->time_scale,
									SND_RATE_44100,
									PF_SSS_4,	
									PF_Channels_MONO,
									PF_SIGNED_FLOAT,			// fmt
									&audio ));
										
										
	ERR(PF_GET_AUDIO_DATA (	in_data,
							audio,
							&data0,
							&num_samples_bufferL,
							NULL,
							NULL,
							NULL,
							NULL));

	if (!err) {
		// This function returns an extra zero sample at the end.
		if (num_samples_bufferL) {
			--num_samples_bufferL;
		}		
	}
								
	if (out_data->dest_snd.fi.num_channels == in_data->src_snd.fi.num_channels) {
		void	*end_addrL = (void*) ((A_intptr_t) in_data->src_snd.dataP +
							((in_data->dur_sampL - 1) * 
							in_data->src_snd.fi.sample_size	*
							in_data->src_snd.fi.num_channels));

        // XCode complains expression result for operations on srcP are unused.  Suppress that.
        #ifdef AE_OS_MAC
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wunused-value"
        #endif
        
		if ((in_data->src_snd.fi.format == PF_SIGNED_FLOAT) &&
			(in_data->src_snd.fi.sample_size == PF_SSS_4) ) {
			PF_FpShort				*destP	= (PF_FpShort*)out_data->dest_snd.dataP;
			PF_FpShort				*srcP	= (PF_FpShort*)end_addrL;

			while(out_dur_sampL--) {
				A_long iL = in_data->src_snd.fi.num_channels;

				while(iL--) {
					PF_FpLong	accumF = *srcP;
					accumF		+= params[AR_LEVEL]->u.fs_d.value * PF_SIN(phaseF * PF_2PI);

					*destP++	= (PF_FpShort)accumF;
					*srcP++;
				}

				levelF			+= inc_levelF;
				freq_rateF		+= inc_freq_rateF;
				phaseF			+= freq_rateF * inv_samp_rateF;

				srcP -= (in_data->src_snd.fi.num_channels << 1);
			}
		} 
		
		else if ((in_data->src_snd.fi.format == PF_SIGNED_PCM) &&
					(in_data->src_snd.fi.sample_size == PF_SSS_2)) {
			A_short	*destP	= (A_short*)out_data->dest_snd.dataP,
					*srcP	= (A_short*)end_addrL;

			while(out_dur_sampL--) {
				A_long iL = in_data->src_snd.fi.num_channels;
				while(iL--) {
					PF_FpLong	accumF = AEFX_Type16toF(*srcP);
					
					accumF += params[AR_LEVEL]->u.fs_d.value * PF_SIN(phaseF * PF_2PI);
					AEFX_CLIP_FLOAT_SIGNED( accumF );

					*destP++ = AEFX_TypeFto16(accumF);
					*srcP++;
				}

				levelF			+= inc_levelF;
				freq_rateF		+= inc_freq_rateF;
				phaseF			+= freq_rateF * inv_samp_rateF;
				srcP			-= (in_data->src_snd.fi.num_channels << 1);
			}
		} else 	if ((in_data->src_snd.fi.format == PF_SIGNED_PCM) &&
					(in_data->src_snd.fi.sample_size == PF_SSS_1)) 	{
			
			A_char				*destP	= (A_char*)out_data->dest_snd.dataP,
								*srcP	= (A_char*)end_addrL;

			while(out_dur_sampL--){
				A_long iL = in_data->src_snd.fi.num_channels;
				while(iL--) {
					PF_FpLong	accumF = AEFX_Type8toF(*srcP);
					
					accumF += params[AR_LEVEL]->u.fs_d.value * PF_SIN(phaseF * PF_2PI);
					AEFX_CLIP_FLOAT_SIGNED( accumF );
 					*destP++ = AEFX_TypeFto8(accumF);
					*srcP++;
				}

				levelF			+= 	inc_levelF;
				freq_rateF		+= 	inc_freq_rateF;
				phaseF			+= 	freq_rateF * inv_samp_rateF;
				srcP			-=  (in_data->src_snd.fi.num_channels << 1);
			}
		} else 	{
			err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
		}
		ERR2(PF_CHECKIN_LAYER_AUDIO (in_data, audio));
        
        #ifdef AE_OS_MAC
        #pragma clang diagnostic pop
        #endif
	}
	return err;
} 

static A_long
RoundDouble(PF_FpLong x)
{
	A_long	retL;
	
	if (x > 0) {
		retL = (A_long)(x + 0.5);
	} else {
		if ((A_long)(x + 0.5) == (x + 0.5)) {
			retL = (A_long)x;
		} else {
			retL = (A_long)(x - 0.5);
		}
	
	}
	return retL;
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
		"SDK_Backwards", // Name
		"ADBE SDK_Backwards", // Match Name
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
   PF_LayerDef		*output )
{
	PF_Err		err = PF_Err_NONE;
	
	switch (cmd) {
		
		case PF_Cmd_ABOUT:
			err = About(in_data, out_data, params, output);
			break;
		case PF_Cmd_GLOBAL_SETUP:
			err = GlobalSetup(in_data, out_data, params, output);
			break;
		case PF_Cmd_PARAMS_SETUP:
			err = ParamsSetup(in_data, out_data, params, output);
			break;
		case PF_Cmd_AUDIO_RENDER:
			err = Audio_Render(in_data, out_data, params, output);
			break;
		case PF_Cmd_AUDIO_SETUP:
			err = Audio_Setup(in_data, out_data, params, output);
			break;
	}
	return err;
}

