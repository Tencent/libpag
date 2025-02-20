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


/*	PathMaster.cpp

	This sample shows how to access and rasterize paths using 
	PICA suites.

	Revision history

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			First release											bbb			6/28/2000		
	2.0			Updated to "old school" coding guidelines, added mask	bbb			5/20/2003
				color fiddling
	3.0			Added sequence data handling							bbb			8/1/2006
	3.5			Fixed memory issues, cleaned up code					zal			11/12/2010
	3.6			Delete unflat sequence data after flattening during		zal			6/11/2014
				SEQUENCE_FLATTEN. Fix incorrect comment.
	4.0			Support PF_Cmd_GET_FLATTENED_SEQUENCE_DATA.  Fix more	zal			3/24/2015
				memory issues.
	4.1			Added new entry point									zal			9/15/2017
	4.2			Remove deprecated 'register' keyword					cb			12/18/2020
	4.3			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023
*/

#include "PathMaster.h"

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
								"%s v%d.%d\r%s",
								STR(StrID_Name), 
								MAJOR_VERSION, 
								MINOR_VERSION, 
								STR(StrID_Description));
	return err;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data)
{	
	PF_Err err = PF_Err_NONE;

	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);
		
	out_data->out_flags = 	PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING	|
							PF_OutFlag_DEEP_COLOR_AWARE;

	out_data->out_flags2 =	PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
							PF_OutFlag2_SUPPORTS_GET_FLATTENED_SEQUENCE_DATA;

	return err;
}

static PF_Err 
SequenceSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err				err = PF_Err_NONE,
						err2 = PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	//	Here's how to get and set the color of each mask attached to a layer.
	if (!err) {
		AEGP_LayerH 	layerH 		= NULL;
		A_long 			mask_countL = 0;
		AEGP_MaskRefH 	mask_refH 	= NULL;
		AEGP_ColorVal   old_color, new_color;

		ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(in_data->effect_ref, &layerH));

		ERR(suites.MaskSuite5()->AEGP_GetLayerNumMasks(layerH, &mask_countL));

		for (A_long iL = 0; !err && (iL < mask_countL); ++iL){
			ERR(suites.MaskSuite5()->AEGP_GetLayerMaskByIndex(layerH, iL, &mask_refH));

			if (mask_refH){
				ERR(suites.MaskSuite5()->AEGP_GetMaskColor(mask_refH, &old_color));
				if (!err){
	        		new_color.redF		= 1 - old_color.greenF;
	        		new_color.greenF	= 1 - old_color.blueF;
	        		new_color.blueF		= 1 - old_color.redF;

					// This seems to cause recursion when called from an effect like this
	        		ERR(suites.MaskSuite5()->AEGP_SetMaskColor(mask_refH, &new_color));
				}

	       		ERR2(suites.MaskSuite5()->AEGP_DisposeMask(mask_refH));
			}
		}
	}

	// Create sequence data
	PF_Handle			seq_dataH =	suites.HandleSuite1()->host_new_handle(sizeof(Unflat_Seq_Data));

	if (seq_dataH){
		Unflat_Seq_Data	*seqP = reinterpret_cast<Unflat_Seq_Data*>(suites.HandleSuite1()->host_lock_handle(seq_dataH));

		if (seqP){
			AEFX_CLR_STRUCT(*seqP);

			// We need to allocate some non-flat memory for this arbitrary string.
			seqP->stringP = new A_char[strlen(STR(StrID_Really_Long_String)) + 1];

			//	WARNING: The following assignments are not safe for cross-platform use.
			//	Production code would enforce byte order consistency, across platforms.

			if (seqP->stringP){
				suites.ANSICallbacksSuite1()->strcpy(seqP->stringP, STR(StrID_Really_Long_String));
			}
		
			seqP->fixed_valF		= 123;
			seqP->flatB				= FALSE;
			out_data->sequence_data = seq_dataH;

			suites.HandleSuite1()->host_unlock_handle(seq_dataH);
		}
	} else {	// whoa, we couldn't allocate sequence data; bail!
		err = PF_Err_OUT_OF_MEMORY;
	}
	return err;
}

static PF_Err 
SequenceSetdown (	
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	// Flat or unflat, get rid of it
	if (in_data->sequence_data){
		Unflat_Seq_Data	*seqP = reinterpret_cast<Unflat_Seq_Data*>(DH(in_data->sequence_data));
		if (!seqP->flatB) {
			delete [] seqP->stringP;
		}
		suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
	}
	return err;
}

static PF_Err 
SequenceFlatten(	
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// Make a flat copy of whatever is in the unflat seq data handed to us. 
	
	if (in_data->sequence_data){
		Unflat_Seq_Data* unflat_seq_dataP = reinterpret_cast<Unflat_Seq_Data*>(DH(in_data->sequence_data));

		if (unflat_seq_dataP){
			PF_Handle flat_seq_dataH = suites.HandleSuite1()->host_new_handle(sizeof(Flat_Seq_Data));

			if (flat_seq_dataH){
				Flat_Seq_Data*	flat_seq_dataP = reinterpret_cast<Flat_Seq_Data*>(suites.HandleSuite1()->host_lock_handle(flat_seq_dataH));

				if (flat_seq_dataP){
					AEFX_CLR_STRUCT(*flat_seq_dataP);
					flat_seq_dataP->flatB		= TRUE;
					flat_seq_dataP->fixed_valF	= unflat_seq_dataP->fixed_valF;

					#ifdef AE_OS_WIN
						strncpy_s(flat_seq_dataP->string, unflat_seq_dataP->stringP, PF_MAX_EFFECT_MSG_LEN);
					#else
						strncpy(flat_seq_dataP->string, unflat_seq_dataP->stringP, PF_MAX_EFFECT_MSG_LEN);
					#endif

					// In SequenceSetdown we toss out the unflat data
					delete [] unflat_seq_dataP->stringP;
					suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);

					out_data->sequence_data = flat_seq_dataH;
					suites.HandleSuite1()->host_unlock_handle(flat_seq_dataH);
				}
			} else {
				err = PF_Err_INTERNAL_STRUCT_DAMAGED;
			}
		}
	} else {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}

static PF_Err 
GetFlattenedSequenceData(	
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// Make a flat copy of whatever is in the unflat seq data handed to us. 
	
	if (in_data->sequence_data){
		Unflat_Seq_Data* unflat_seq_dataP = reinterpret_cast<Unflat_Seq_Data*>(DH(in_data->sequence_data));

		if (unflat_seq_dataP){
			PF_Handle flat_seq_dataH = suites.HandleSuite1()->host_new_handle(sizeof(Flat_Seq_Data));

			if (flat_seq_dataH){
				Flat_Seq_Data*	flat_seq_dataP = reinterpret_cast<Flat_Seq_Data*>(suites.HandleSuite1()->host_lock_handle(flat_seq_dataH));

				if (flat_seq_dataP){
					AEFX_CLR_STRUCT(*flat_seq_dataP);
					flat_seq_dataP->flatB		= TRUE;
					flat_seq_dataP->fixed_valF	= unflat_seq_dataP->fixed_valF;

					#ifdef AE_OS_WIN
						strncpy_s(flat_seq_dataP->string, unflat_seq_dataP->stringP, PF_MAX_EFFECT_MSG_LEN);
					#else
						strncpy(flat_seq_dataP->string, unflat_seq_dataP->stringP, PF_MAX_EFFECT_MSG_LEN);
					#endif

					// The whole point of this function is that we don't dispose of the unflat data!
					// delete [] unflat_seq_dataP->stringP;
					//suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);

					out_data->sequence_data = flat_seq_dataH;
					suites.HandleSuite1()->host_unlock_handle(flat_seq_dataH);
				}
			} else {
				err = PF_Err_INTERNAL_STRUCT_DAMAGED;
			}
		}
	} else {
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}

static PF_Err 
SequenceResetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// We got here because we're either opening a project w/saved (flat) sequence data,
	// or we've just been asked to flatten our sequence data (for a save) and now 
	// we're blowing it back up.
	
	if (in_data->sequence_data){
		Flat_Seq_Data*	flatP = reinterpret_cast<Flat_Seq_Data*>(DH(in_data->sequence_data));

		if (flatP){
			PF_Handle unflat_seq_dataH = suites.HandleSuite1()->host_new_handle(sizeof(Unflat_Seq_Data));

			if (unflat_seq_dataH){
				Unflat_Seq_Data* unflatP = reinterpret_cast<Unflat_Seq_Data*>(suites.HandleSuite1()->host_lock_handle(unflat_seq_dataH));

				if (unflatP){
					AEFX_CLR_STRUCT(*unflatP);
					unflatP->fixed_valF = flatP->fixed_valF;	// Warning: NOT X-PLATFORM SAFE!
					unflatP->flatB		= FALSE;
					A_short	lengthS		= strlen(flatP->string);

					unflatP->stringP	= new A_char[lengthS + 1];

					if (unflatP->stringP){
						suites.ANSICallbacksSuite1()->strcpy(unflatP->stringP, flatP->string);

						// if it all went well, set the sequence data to our new, inflated seq data.
						out_data->sequence_data = unflat_seq_dataH;
					} else {
						err = PF_Err_INTERNAL_STRUCT_DAMAGED;
					}
					suites.HandleSuite1()->host_unlock_handle(unflat_seq_dataH);
				}
			}
		}
	}
	return err;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[])
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	AEFX_CLR_STRUCT(def);
	def.param_type = PF_Param_PATH;
	def.uu.id = PATH_DISK_ID;
	PF_STRCPY(def.name, STR(StrID_PathName)); 
	def.u.path_d.dephault	= 0;

	PF_ADD_PARAM(in_data, -1, &def);

	AEFX_CLR_STRUCT(def);

	PF_ADD_COLOR(	STR(StrID_ColorName), 
					PF_MAX_CHAN8,	// Red channel set to max
					0,
					0,
					COLOR_DISK_ID);
	
	AEFX_CLR_STRUCT(def);
						
  	PF_ADD_FIXED (	STR(StrID_X_Feather),
  					FEATHER_VALID_MIN,
  					FEATHER_VALID_MAX,
  					FEATHER_MIN,
  					FEATHER_MAX,
  					FEATHER_DFLT,
  					1,
  					0,
  					0,
  					X_FEATHER_DISK_ID);
					
	AEFX_CLR_STRUCT(def);

  	PF_ADD_FIXED (	STR(StrID_Y_Feather),
  					FEATHER_VALID_MIN,
  					FEATHER_VALID_MAX,
  					FEATHER_MIN,
  					FEATHER_MAX,
  					FEATHER_DFLT,
  					1,
  					0,
  					0,
  					Y_FEATHER_DISK_ID);

	AEFX_CLR_STRUCT(def);

	PF_ADD_CHECKBOX(	STR(StrID_CheckName), 
						STR(StrID_CheckDetail), 
						FALSE,
						0, 
						DOWNSAMPLE_DISK_ID);
	
	AEFX_CLR_STRUCT(def);

  	PF_ADD_FIXED (	STR(StrID_OpacityName),
  					OPACITY_VALID_MIN,
  					OPACITY_VALID_MAX,
  					OPACITY_VALID_MIN,
  					OPACITY_VALID_MAX,
  					OPACITY_DFLT,
  					OPACITY_PRECISION,
  					PF_ValueDisplayFlag_PERCENT,
  					0,
  					OPACITY_DISK_ID);
	
	out_data->num_params = PATHMASTER_NUM_PARAMS;

	return err;
}

static PF_Err
BlendColor(
	void		*refcon, 
	A_long		xL, 
	A_long		yL, 
	PF_Pixel 	*inP, 
	PF_Pixel 	*outP)
{
	BlendInfo *biP = reinterpret_cast<BlendInfo*>(refcon);
	
	outP->blue	=	static_cast<A_u_char>((inP->blue	+ biP->color.blue)	/ 2);
	outP->green	=	static_cast<A_u_char>((inP->green	+ biP->color.green)	/ 2);
	outP->red	=	static_cast<A_u_char>((inP->red		+ biP->color.red)	/ 2);
	
	if (biP->invertB) {
		outP->alpha = static_cast<A_u_char>(PF_MAX_CHAN8 - outP->alpha);
	} else {
		outP->alpha	= inP->alpha;
	}
	return PF_Err_NONE;
}

static PF_Err
BlendColor16(
	void		*refcon, 
	A_long		xL, 
	A_long		yL, 
	PF_Pixel16 *inP, 
	PF_Pixel16 *outP)
{
	BlendInfo *biP = reinterpret_cast<BlendInfo*>(refcon);
	
	outP->blue	=	static_cast<A_u_short>((inP->blue	+ biP->deep_color.blue)		/ 2);
	outP->green	=	static_cast<A_u_short>((inP->green	+ biP->deep_color.green)	/ 2);
	outP->red	=	static_cast<A_u_short>((inP->red	+ biP->deep_color.red)		/ 2);
	
	if (biP->invertB) {
		outP->alpha = static_cast<A_u_short>(PF_MAX_CHAN16 - outP->alpha);
	} else {
		outP->alpha	=	inP->alpha;
	}
	return PF_Err_NONE;
}

static PF_Err 
Render (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err				err			= PF_Err_NONE,
						err2		= PF_Err_NONE;
						
	PF_EffectWorld		*inputP		= NULL;

	A_long				mask_idL	= params[PATHMASTER_PATH]->u.path_d.path_id;
	PF_PathOutlinePtr	maskP		= 0;
	PF_MaskSuite1		*msP		= NULL;
	PF_Boolean			openB 		= TRUE,
						deepB		= PF_WORLD_IS_DEEP(output);
	PF_Pixel			color		= params[PATHMASTER_COLOR]->u.cd.value;
	PF_Pixel16			deep_color;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	ERR(AEFX_AcquireSuite(in_data,
						  out_data,
						  kPF_MaskSuite,
						  kPF_MaskSuiteVersion1,
						  STR(StrID_Err_LoadSuite),
						  reinterpret_cast<void**>(&msP)));
						  
	inputP 	= &params[PATHMASTER_INPUT]->u.ld;

	if (mask_idL) {
		ERR(suites.PathQuerySuite1()->PF_CheckoutPath(	in_data->effect_ref, 
														mask_idL,
														in_data->current_time,
														in_data->time_step,
														in_data->time_scale,
														&maskP));

		ERR(suites.PathDataSuite1()->PF_PathIsOpen(	in_data->effect_ref, maskP,	&openB));

		if (!err && !openB) {

			// Fill the frame with the color chosen in the color parameter
			if (deepB){
				deep_color.red		= CONVERT8TO16(color.red);
				deep_color.blue		= CONVERT8TO16(color.blue);
				deep_color.green	= CONVERT8TO16(color.green);
				
				ERR(suites.FillMatteSuite2()->fill16(in_data->effect_ref,
													&deep_color, 
													0, 
													output));
			} else {
				ERR(suites.FillMatteSuite2()->fill(in_data->effect_ref,
												&color, 
												0, 
												output));
			}

			// Mask and feather the color using the path chosen in the path parameter
			PF_FpLong x_resF = in_data->downsample_x.num / 
												static_cast<PF_FpLong>(in_data->downsample_x.den);
			PF_FpLong y_resF = in_data->downsample_y.num / 
												static_cast<PF_FpLong>(in_data->downsample_y.den);

			PF_FpLong x_featherF = FIX_2_FLOAT(params[PATHMASTER_X_FEATHER]->u.fd.value * x_resF);
			x_featherF = FIX_2_FLOAT(params[PATHMASTER_X_FEATHER]->u.fd.value) * x_resF;

			ERR(msP->PF_MaskWorldWithPath(	in_data->effect_ref,
											&maskP,
											FIX_2_FLOAT(params[PATHMASTER_X_FEATHER]->u.fd.value) * x_resF, // float version of x feather
											FIX_2_FLOAT(params[PATHMASTER_Y_FEATHER]->u.fd.value) * y_resF, // float version of y feather
											params[PATHMASTER_INVERT]->u.bd.value,
											FIX_2_FLOAT(params[PATHMASTER_OPACITY]->u.fd.value), // opacity value
											in_data->quality,
											output,
											&output->extent_hint));

			// Copy the original frame back, respecting the mask
			if (!err) 
			{
				PF_CompositeMode	mode;
				PF_Rect				full;

				full.left 		= 	0;
				full.top		=	0;
				full.bottom 	= 	output->height;
				full.right	 	=  	output->width;

				mode.xfer		= PF_Xfer_IN_FRONT;
				mode.rand_seed	= 0;
				mode.opacity	= PF_MAX_CHAN8;
				mode.rgb_only	= FALSE;
				mode.opacitySu	= PF_MAX_CHAN16;

				err = suites.WorldTransformSuite1()->transfer_rect(	in_data->effect_ref,
																	in_data->quality,
																	PF_MF_Alpha_STRAIGHT,
																	PF_Field_FRAME, 
																	&full,
																	inputP,
																	&mode,
																	NULL,
																	0,
																	0,
																	output);

			}
		} else if (!err) { 
			// the mask selected is not closed, so just copy the input to the output
			if (in_data->quality == PF_Quality_HI){
				ERR(suites.WorldTransformSuite1()->copy_hq(	in_data->effect_ref,
															inputP,
															output,
															0,
															0));
			} else {
				ERR(suites.WorldTransformSuite1()->copy(in_data->effect_ref,
														inputP,
														output,
														0,
														0));
			}	
		}
		ERR2(suites.PathQuerySuite1()->PF_CheckinPath(	in_data->effect_ref, 
														mask_idL,
														FALSE, 
														maskP));
	} else {
		// If there are no masks on the layer, just copy the input to the output
		ERR(suites.WorldTransformSuite1()->copy(in_data->effect_ref,
												inputP,
												output,
												0,
												0));
	}

	// Blend image inside mask with color
	if (!err) {
		BlendInfo	bi;
		PF_FpLong	opacityF	= FIX_2_FLOAT(params[PATHMASTER_OPACITY]->u.fd.value);
		bi.opacityCu			= (A_u_char) ((opacityF * PF_MAX_CHAN8) + 0.5);
		bi.opacitySu			= (A_u_short) ((opacityF * PF_MAX_CHAN16) + 0.5);

		if (bi.opacitySu > PF_MAX_CHAN16) {
			bi.opacitySu = PF_MAX_CHAN16;
		}
		
		bi.color		= params[PATHMASTER_COLOR]->u.cd.value;
		bi.invertB		= params[PATHMASTER_INVERT]->u.bd.value;

		if (deepB){
			bi.deep_color.red		= deep_color.red;
			bi.deep_color.blue		= deep_color.blue;
			bi.deep_color.green		= deep_color.green;
			bi.deep_color.alpha		= deep_color.alpha;

			ERR(suites.Iterate16Suite2()->iterate(	in_data,
													0, 
													output->height, 
													output,
													0, 
													reinterpret_cast<void*>(&bi),
													BlendColor16, 
													output));
		} else {

			ERR(suites.Iterate8Suite2()->iterate(	in_data,
													0, 
													output->height, 
													output,
													0, 
													reinterpret_cast<void*>(&bi),
													BlendColor, 
													output));
		}
	}
	
	if (msP){
		ERR2(AEFX_ReleaseSuite(in_data,
							   out_data,
							   kPF_MaskSuite, 
							   kPF_MaskSuiteVersion1,
							   STR(StrID_Err_FreeSuite)));
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
		"PathMaster", // Name
		"ADBE PathMaster", // Match Name
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
				err = About(in_data,out_data,params,output);
				break;
			case PF_Cmd_SEQUENCE_SETUP:
				err = SequenceSetup(in_data,out_data);
				break;
			case PF_Cmd_SEQUENCE_SETDOWN:
				err = SequenceSetdown(in_data,out_data);
				break;
			case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA:
				err = GetFlattenedSequenceData(in_data,out_data);
				break;
			case PF_Cmd_SEQUENCE_FLATTEN:
				err = SequenceFlatten(in_data,out_data);
				break;
			case PF_Cmd_SEQUENCE_RESETUP:
				err = SequenceResetup(in_data,out_data);
				break;
			case PF_Cmd_GLOBAL_SETUP:
				err = GlobalSetup(in_data,out_data);
				break;
			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(in_data,out_data,params);
				break;
			case PF_Cmd_RENDER:
				err = Render(in_data,out_data,params,output);
				break;
			default:
				break;
		}
	} catch(PF_Err &thrown_err) {
		// Never EVER throw exceptions into AE.
		err = thrown_err;
	}
	return err;
}
