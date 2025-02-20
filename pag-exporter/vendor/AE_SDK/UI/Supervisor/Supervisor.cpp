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
	Supervisor.cpp
	
	This plug-in demonstrates parameter supervision.

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			created													bbb			9/1/2000
	1.1			updated for 16bpc										bbb			2/14/2001
	1.2			added use of START_COLLAPSED_FLAG,						bbb			9/1/2001
				fixed a supervision logic bug or two...								
	1.3			Fixed usage of initializedB flag						bbb			1/1/2002
	2.0			Bumped version number higher to 						bbb			2/20/2003
				reflect a bunch of changes. Adding
				lots of comments.
	3.0			Replaced param supervision with whizzy					bbb			5/7/2003
				new DynamicStreamSuite calls.
	4.0			I am so SMRT! I am so SMRT! S-M-R-T!					bbb			10/4/2006
				No, wait...S-M-A-R-T!
	5.0			Premiere Pro compatibility								zal			2/20/2008
	5.1			Better param supervision logic							zal			5/28/2009
	5.2			Demonstrate renaming choices in popup param				zal			6/2/2009
	5.5			64-bit													zal			2/2/2010
	5.6			Demonstrate HaveInputsChangedOverTimeSpan				zal			4/20/2011
	5.7			Demonstrate changing param values						zal			6/22/2011
				Improved supervision logic, code cleanup
	5.8			Added new entry point									zal			9/18/2017
	5.9			Remove deprecated 'register' keyword					cb			12/18/2020
	5.10		Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

*/

#include "Supervisor.h"


static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	
	PF_Err	err			=	PF_Err_NONE;
	AEGP_SuiteHandler		suites(in_data->pica_basicP);
	PF_AppPersonalTextInfo	personal_info;

	AEFX_CLR_STRUCT(personal_info);

	ERR(suites.AppSuite4()->PF_GetPersonalInfo(&personal_info));
		
	if (!err) {
		suites.ANSICallbacksSuite1()->sprintf(	out_data->return_msg,
										"%s\r%s\r%s, v%d.%d\r\r%s",
										STR(StrID_Name), 
										personal_info.name,
										personal_info.serial_str,
										MAJOR_VERSION, 
										MINOR_VERSION, 
										STR(StrID_Description));
	}
	return err;
}

static void
GetPresetColorValue(
	A_long			flavor,
	PF_Pixel		*scratchP)
{
	switch (flavor)	{
		case FLAVOR_CHOCOLATE:			

			scratchP->red	=	136;
			scratchP->green	=	83;
			scratchP->blue	=	51;
			break;

		case FLAVOR_STRAWBERRY:

			scratchP->red	=	232;
			scratchP->green	=	21;
			scratchP->blue	=	84;
			break;

		case FLAVOR_SHERBET:

			scratchP->red	=	255;
			scratchP->green	=	128;
			scratchP->blue	=	0;

			break;

		default:
			break;
	}
	scratchP->alpha = 0;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err 				err 		= PF_Err_NONE;
	
	PF_Handle			globH		= NULL;
	my_global_dataP		globP		= NULL;

	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,	
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	out_data->out_flags		=	PF_OutFlag_PIX_INDEPENDENT 			| 
								PF_OutFlag_SEND_UPDATE_PARAMS_UI	|
								PF_OutFlag_USE_OUTPUT_EXTENT		|
								PF_OutFlag_DEEP_COLOR_AWARE			|
								PF_OutFlag_WIDE_TIME_INPUT;

	// 	This new outflag, added in 5.5, makes After Effects
	//	honor the initial state of the parameter's (in our case,
	//	the parameter group's) collapsed flag.
	
	out_data->out_flags2	=	PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG	|
								PF_OutFlag2_FLOAT_COLOR_AWARE					|
								PF_OutFlag2_SUPPORTS_SMART_RENDER				|
								PF_OutFlag2_DOESNT_NEED_EMPTY_PIXELS			|
								PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT;
    
    /*
     This plugin is not marked as Thread-Safe because it is writing to sequence data during PF_Cmd_UPDATE_PARAMS_UI.
     There is an exisiting issue that's preventing this from working during Multi-Frame rendering.
     */
    

	//	This looks more complex than it is. Basically, this
	//	code allocates a handle, and (if successful) sets a 
	//	sentinel value for later use, and gets an ID for using
	//	AEGP calls. It then stores those value in  in_data->global_data.
	
	globH	=	suites.HandleSuite1()->host_new_handle(sizeof(my_global_data));
	
	if (globH) {
		globP = reinterpret_cast<my_global_dataP>(suites.HandleSuite1()->host_lock_handle(globH));
		if (globP) {
			globP->initializedB 	= TRUE;

			if (in_data->appl_id != 'PrMr') {
				// This is only needed for the AEGP suites, which Premiere Pro doesn't support
				ERR(suites.UtilitySuite3()->AEGP_RegisterWithAEGP(NULL, STR(StrID_Name), &globP->my_id));
			}
				if (!err){
					out_data->global_data 	= globH;
			}
		}
		suites.HandleSuite1()->host_unlock_handle(globH);
	} else	{
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}

static PF_Err 
SequenceSetdown (	
					PF_InData		*in_data,
					PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	
	if (in_data->sequence_data){
		AEGP_SuiteHandler suites(in_data->pica_basicP);
		suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
	}
	return err;
}

static PF_Err 
SequenceSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	err = SequenceSetdown(in_data, out_data);
	
	if (!err){
		PF_Handle	seq_dataH =	suites.HandleSuite1()->host_new_handle(sizeof(my_sequence_data));
		
		if (seq_dataH){
			my_sequence_dataP seqP = static_cast<my_sequence_dataP>(suites.HandleSuite1()->host_lock_handle(seq_dataH));
			if (seqP){
				if (in_data->appl_id != 'PrMr')
				{
					ERR(suites.ParamUtilsSuite3()->PF_GetCurrentState(	in_data->effect_ref,
																		SUPER_SLIDER,
																		NULL,
																		NULL,
																		&seqP->state));
				}
				seqP->advanced_modeB = FALSE;

				out_data->sequence_data = seq_dataH;
				suites.HandleSuite1()->host_unlock_handle(seq_dataH);
			}
		} else {	// whoa, we couldn't allocate sequence data; bail!
			err = PF_Err_OUT_OF_MEMORY;
		}
	}
	return err;
}

static PF_Err	
ParamsSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err				err					= PF_Err_NONE;
	PF_ParamDef			def;

	AEFX_CLR_STRUCT(def);
	def.flags		=	PF_ParamFlag_SUPERVISE			|
						PF_ParamFlag_CANNOT_TIME_VARY	|
						PF_ParamFlag_CANNOT_INTERP;
						
	def.ui_flags	=	PF_PUI_STD_CONTROL_ONLY; 

	// Defaults to "Basic"
	PF_ADD_POPUP(		STR(StrID_ModeName), 
						MODE_SIZE,
						MODE_BASIC,
						STR(StrID_ModeChoices),
						SUPER_MODE_DISK_ID);
		
	AEFX_CLR_STRUCT(def);
	
	def.flags		=	PF_ParamFlag_SUPERVISE |
						PF_ParamFlag_CANNOT_INTERP;
	
	PF_ADD_POPUP(		STR(StrID_FlavorName), 
						FLAVOR_SIZE,
						FLAVOR_CHOCOLATE,
						STR(StrID_FlavorChoices),
						SUPER_FLAVOR_DISK_ID);
	
	AEFX_CLR_STRUCT(def);
	
	PF_Pixel temp;
	
	GetPresetColorValue(FLAVOR_CHOCOLATE, &temp);

	PF_ADD_COLOR(	STR(StrID_ColorName), 
					temp.red, 
					temp.green, 
					temp.blue, 
					SUPER_COLOR_DISK_ID);
	
	AEFX_CLR_STRUCT(def);

	PF_ADD_FIXED(	STR(StrID_SliderName), 
					0, 
					100, 
					0, 
					100, 
					28, 
					1, 
					1,
					PF_ParamFlag_EXCLUDE_FROM_HAVE_INPUTS_CHANGED,
					SUPER_SLIDER_DISK_ID);
	
	AEFX_CLR_STRUCT(def);

	def.flags		|=	PF_ParamFlag_SUPERVISE |
						PF_ParamFlag_CANNOT_TIME_VARY;

	def.ui_flags	= PF_PUI_STD_CONTROL_ONLY;

	PF_ADD_CHECKBOX(STR(StrID_CheckboxName),
					STR(StrID_CheckboxCaption),
					FALSE, 
					0,
					SUPER_CHECKBOX_DISK_ID);

	AEFX_CLR_STRUCT(def);

	out_data->num_params = SUPER_NUM_PARAMS;

	return err;
}

static PF_Err 
PixelFunc8 (	
	void 		*refcon,
	A_long 		xL,
	A_long 		yL,
	PF_Pixel8 	*inP,
	PF_Pixel8 	*outP)
{
	prerender_stuff*	stuffP = reinterpret_cast<prerender_stuff*>(refcon);

	A_short tempS = 0;
	if (stuffP){
		tempS = (A_short)(inP->red + stuffP->color.red * (PF_FpShort)PF_MAX_CHAN8 / 2);
		if (tempS > PF_MAX_CHAN8){
			outP->red = PF_MAX_CHAN8;
		} else {
			outP->red = tempS;
		}
		tempS = (A_short)(inP->green + stuffP->color.green * PF_MAX_CHAN8 / 2);
		if (tempS > PF_MAX_CHAN8){
			outP->green = PF_MAX_CHAN8;
		} else {
			outP->green = tempS;
		}
		tempS = (A_short)(inP->blue +	stuffP->color.blue * PF_MAX_CHAN8 / 2);
		if (tempS > PF_MAX_CHAN8){
			outP->blue = PF_MAX_CHAN8;
		} else {
			outP->blue = tempS;
		}
		outP->alpha	= inP->alpha;
	}	
	return PF_Err_NONE;
}

static PF_Err 
PixelFunc16(	
	void 		*refcon,
	A_long 		xL,
	A_long 		yL,
	PF_Pixel16 *inP,
	PF_Pixel16 *outP)
{
	prerender_stuff*	stuffP = reinterpret_cast<prerender_stuff*>(refcon);
	
	A_long tempL = 0;
	if (stuffP){
		tempL = (A_long)(inP->red			+	stuffP->color.red * PF_MAX_CHAN16		/ 2);
		if (tempL > PF_MAX_CHAN16){
			outP->red = PF_MAX_CHAN16;
		} else {
			outP->red = tempL;
		}
		tempL = (A_long)(inP->green			+	stuffP->color.green * PF_MAX_CHAN16		/ 2);
		if (tempL > PF_MAX_CHAN16){
			outP->green = PF_MAX_CHAN16;
		} else {
			outP->green = tempL;
		}
		tempL = (A_long)(inP->blue			+	stuffP->color.blue * PF_MAX_CHAN16		/ 2);
		if (tempL > PF_MAX_CHAN16){
			outP->blue = PF_MAX_CHAN16;
		} else {
			outP->blue = tempL;
		}
		outP->alpha	= inP->alpha;
	}	
	return PF_Err_NONE;
}

static PF_Err 
PixelFuncFloat(	
	void			*refcon,
	A_long			xL,
	A_long			yL,
	PF_PixelFloat	*inP,
	PF_PixelFloat	*outP)
{
	prerender_stuff*	stuffP = reinterpret_cast<prerender_stuff*>(refcon);
	
	if (stuffP){
		outP->red	= ((inP->red	+	(stuffP->color.red))		/ 2);
		outP->green	= ((inP->green	+	(stuffP->color.green))		/ 2);
		outP->blue	= ((inP->blue	+	(stuffP->color.blue))		/ 2);
		outP->alpha	= inP->alpha;
	}
	return PF_Err_NONE;
}

static PF_Err  
MakeParamCopy(
	PF_ParamDef *actual[],	/* >> */ 
	PF_ParamDef copy[])		/* << */
{
	for (A_short iS = 0; iS < SUPER_NUM_PARAMS; ++iS)	{
		AEFX_CLR_STRUCT(copy[iS]);	// clean params are important!
	}
	copy[SUPER_INPUT]			= *actual[SUPER_INPUT];
	copy[SUPER_MODE]			= *actual[SUPER_MODE];
	copy[SUPER_FLAVOR]			= *actual[SUPER_FLAVOR];
	copy[SUPER_COLOR]			= *actual[SUPER_COLOR];
	copy[SUPER_SLIDER]			= *actual[SUPER_SLIDER];
	copy[SUPER_CHECKBOX]		= *actual[SUPER_CHECKBOX];

	return PF_Err_NONE;

}

static PF_Err
UserChangedParam(
	PF_InData						*in_data,
	PF_OutData						*out_data,
	PF_ParamDef						*params[],
	PF_LayerDef						*outputP,
	const PF_UserChangedParamExtra	*which_hitP)
{
	PF_Err				err					= PF_Err_NONE;

	if (which_hitP->param_index == SUPER_CHECKBOX)
	{
		// If checkbox is checked, change slider value to 50 and flip checkbox back off
		if (params[SUPER_CHECKBOX]->u.bd.value == TRUE) {
			params[SUPER_SLIDER]->u.sd.value = INT2FIX(50);
			params[SUPER_SLIDER]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;

			params[SUPER_CHECKBOX]->u.bd.value = FALSE;

			AEGP_SuiteHandler		suites(in_data->pica_basicP);

			ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref, 
															SUPER_CHECKBOX,
															params[SUPER_CHECKBOX]));
		}
	}

	return err;
}

static PF_Err
UpdateParameterUI(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	PF_ParamDef			*params[],
	PF_LayerDef			*outputP)
{
	PF_Err				err					= PF_Err_NONE,
						err2				= PF_Err_NONE;
	my_global_dataP		globP				= reinterpret_cast<my_global_dataP>(DH(out_data->global_data));
	my_sequence_dataP	seqP				= reinterpret_cast<my_sequence_dataP>(DH(out_data->sequence_data));
	AEGP_StreamRefH		flavor_streamH		= NULL,
						color_streamH		= NULL,
						slider_streamH		= NULL,
						checkbox_streamH	= NULL;
	PF_ParamType		param_type;
	PF_ParamDefUnion	param_union;
	
	A_Boolean			hide_themB			= FALSE;
						
	AEGP_EffectRefH			meH				= NULL;					
	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	//	Before we can change the enabled/disabled state of parameters,
	//	we need to make a copy (remember, parts of those passed into us
	//	are read-only).

	PF_ParamDef		param_copy[SUPER_NUM_PARAMS];
	ERR(MakeParamCopy(params, param_copy));

	// Toggle enable/disabled state of flavor param
	if (!err &&
		(MODE_BASIC == params[SUPER_MODE]->u.pd.value)) {

		param_copy[SUPER_FLAVOR].param_type	=	PF_Param_POPUP;
		param_copy[SUPER_FLAVOR].ui_flags	&= ~PF_PUI_DISABLED;
		strcpy(param_copy[SUPER_FLAVOR].name, STR(StrID_FlavorName));		
		
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref, 
														SUPER_FLAVOR,
														&param_copy[SUPER_FLAVOR]));
	} else if (!err &&
				(MODE_ADVANCED == params[SUPER_MODE]->u.pd.value) &&
				(!(param_copy[SUPER_FLAVOR].ui_flags & PF_PUI_DISABLED))) {

		param_copy[SUPER_FLAVOR].param_type		=	PF_Param_POPUP;
		param_copy[SUPER_FLAVOR].ui_flags		|=	PF_PUI_DISABLED;
		strcpy(param_copy[SUPER_FLAVOR].name, STR(StrID_FlavorNameDisabled));
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
														SUPER_FLAVOR,
														&param_copy[SUPER_FLAVOR]));
	}

	if (in_data->appl_id != 'PrMr') {
		if (params[SUPER_MODE]->u.pd.value == MODE_ADVANCED) {
			seqP->advanced_modeB = TRUE;
		} else {
			seqP->advanced_modeB = FALSE;
			hide_themB		 = TRUE;
		}

		// Twirl open the slider param
		param_copy[SUPER_SLIDER].param_type	=	PF_Param_FIX_SLIDER;
		param_copy[SUPER_SLIDER].flags		&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
		
		ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref, 
														SUPER_SLIDER,
														&param_copy[SUPER_SLIDER]));

		// Changing visibility of params in AE is handled through stream suites
		ERR(suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(globP->my_id, in_data->effect_ref, &meH));

		ERR(suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(globP->my_id, meH, SUPER_FLAVOR, 	&flavor_streamH));
		ERR(suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(globP->my_id, meH, SUPER_COLOR, 	&color_streamH));
		ERR(suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(globP->my_id, meH, SUPER_SLIDER,	&slider_streamH));
		ERR(suites.StreamSuite2()->AEGP_GetNewEffectStreamByIndex(globP->my_id, meH, SUPER_CHECKBOX, &checkbox_streamH));

		// Toggle visibility of parameters
		ERR(suites.DynamicStreamSuite2()->AEGP_SetDynamicStreamFlag(color_streamH, 		AEGP_DynStreamFlag_HIDDEN, FALSE, hide_themB));
		ERR(suites.DynamicStreamSuite2()->AEGP_SetDynamicStreamFlag(slider_streamH, 	AEGP_DynStreamFlag_HIDDEN, FALSE, hide_themB));
		ERR(suites.DynamicStreamSuite2()->AEGP_SetDynamicStreamFlag(checkbox_streamH, 	AEGP_DynStreamFlag_HIDDEN, FALSE, hide_themB));

		// Change popup menu items
		ERR(suites.EffectSuite3()->AEGP_GetEffectParamUnionByIndex(globP->my_id, meH, SUPER_FLAVOR_DISK_ID, &param_type, &param_union));
		if (param_type == PF_Param_POPUP)
		{
			strcpy((char*)param_union.pd.u.namesptr, GetStringPtr(StrID_FlavorChoicesChanged));
		}

		if (meH){
			ERR2(suites.EffectSuite2()->AEGP_DisposeEffect(meH));
		}
		if (flavor_streamH){
			ERR2(suites.StreamSuite2()->AEGP_DisposeStream(flavor_streamH));
		}
		if (color_streamH){
			ERR2(suites.StreamSuite2()->AEGP_DisposeStream(color_streamH));
		}
		if (slider_streamH){
			ERR2(suites.StreamSuite2()->AEGP_DisposeStream(slider_streamH));
		}
		if (checkbox_streamH){
			ERR2(suites.StreamSuite2()->AEGP_DisposeStream(checkbox_streamH));
		}
		if (!err){
			out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
		}

		// Demonstrate using PF_AreStatesIdentical to check whether a parameter has changed
		PF_State		new_state;
		A_Boolean		something_changedB		= FALSE;

		ERR(suites.ParamUtilsSuite3()->PF_GetCurrentState(		in_data->effect_ref,
																SUPER_SLIDER,
																NULL,
																NULL,
																&new_state));

		ERR(suites.ParamUtilsSuite3()->PF_AreStatesIdentical(	in_data->effect_ref,
																&seqP->state,
																&new_state,
																&something_changedB));

		if (something_changedB || !globP->initializedB)	{
			//	If something changed (or it's the first time we're being called),
			//	get the new state and store it in our sequence data

			ERR(suites.ParamUtilsSuite3()->PF_GetCurrentState(	in_data->effect_ref,
																SUPER_SLIDER,
																NULL,
																NULL,
																&seqP->state));
		}

	} else { // Premiere Pro doesn't support the stream suites, but uses a UI flag instead

		//	If our global data is present...
		if (!err && globP) {
			// Test all parameters except layers for changes
			
			//	If the mode is currently Basic, hide the advanced-only params			
			if (!err && (MODE_BASIC == params[SUPER_MODE]->u.pd.value)) {

				if (!err) {
					param_copy[SUPER_COLOR].ui_flags	|=	PF_PUI_INVISIBLE;
					ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
																	SUPER_COLOR,
																	&param_copy[SUPER_COLOR]));
				}

				if (!err) {
					param_copy[SUPER_SLIDER].ui_flags |= PF_PUI_INVISIBLE;
					ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref, 
																	SUPER_SLIDER,
																	&param_copy[SUPER_SLIDER]));
				}

				if (!err) {
					param_copy[SUPER_CHECKBOX].ui_flags |=	PF_PUI_INVISIBLE;
					ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref, 
																	SUPER_CHECKBOX,
																	&param_copy[SUPER_CHECKBOX]));
				}

			} else {
				// 	Since we're in advanced mode, show the advanced-only params 
				
				if (!err) {
					param_copy[SUPER_COLOR].ui_flags &= ~PF_PUI_INVISIBLE;

					ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref,
																	SUPER_COLOR,
																	&param_copy[SUPER_COLOR]));
				}

				if (!err && (param_copy[SUPER_SLIDER].ui_flags & PF_PUI_INVISIBLE)) {
					// Note: As of Premiere Pro CS5.5, this is currently not honored
					param_copy[SUPER_SLIDER].flags	&= ~PF_ParamFlag_COLLAPSE_TWIRLY;
					param_copy[SUPER_SLIDER].ui_flags &= ~PF_PUI_INVISIBLE;

					ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref, 
																	SUPER_SLIDER,
																	&param_copy[SUPER_SLIDER]));
				}
				
				if (!err && (param_copy[SUPER_CHECKBOX].ui_flags & PF_PUI_INVISIBLE)) {
					param_copy[SUPER_CHECKBOX].ui_flags &= ~PF_PUI_INVISIBLE;

					ERR(suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref, 
																	SUPER_CHECKBOX,
																	&param_copy[SUPER_CHECKBOX]));
				}

				if (err) { // <--- Note: not !err, err!
					strcpy(out_data->return_msg, STR(StrID_GeneralError));
					
					out_data->out_flags = PF_OutFlag_DISPLAY_ERROR_MESSAGE;
				}
				else {
					globP->initializedB = TRUE;	// set our cheesy first time indicator
				}
			}
		} 

		if (!err) {
			globP->initializedB	=	TRUE;
		}
		out_data->out_flags |= 	PF_OutFlag_REFRESH_UI 		|
								PF_OutFlag_FORCE_RERENDER;
	}

	return err;
}

static PF_Err	
GlobalSetdown(
	PF_InData		*in_data)
{
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	if (in_data->global_data) {
		suites.HandleSuite1()->host_dispose_handle(in_data->global_data);
	}

	return PF_Err_NONE;
}

static PF_Err	
Render16(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_Pixel8		*scratchP)
{
	AEGP_SuiteHandler		suites(in_data->pica_basicP);
	
	PF_Pixel16		scratch16;

	scratch16.red		=	CONVERT8TO16(scratchP->red);
	scratch16.green		=	CONVERT8TO16(scratchP->green);
	scratch16.blue		=	CONVERT8TO16(scratchP->blue);
	scratch16.alpha		=	CONVERT8TO16(scratchP->alpha);

	return suites.Iterate16Suite2()->iterate(	in_data,
												0,
												(output->extent_hint.bottom - output->extent_hint.top),
												&params[SUPER_INPUT]->u.ld,
												&output->extent_hint,
												(void*)&scratch16,
												PixelFunc16,
												output);
}

static PF_Err	
Render8(	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_Pixel8		*scratchP)
{
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	prerender_stuff		stuff;

	stuff.color.red		= (PF_FpShort)scratchP->red		/ PF_MAX_CHAN8;
	stuff.color.green	= (PF_FpShort)scratchP->green	/ PF_MAX_CHAN8;
	stuff.color.blue	= (PF_FpShort)scratchP->blue	/ PF_MAX_CHAN8;
	stuff.color.alpha	= (PF_FpShort)scratchP->alpha	/ PF_MAX_CHAN8;
	
	return suites.Iterate8Suite2()->iterate(in_data,
											0,
											(output->extent_hint.bottom - output->extent_hint.top),
											&params[SUPER_INPUT]->u.ld,
											&output->extent_hint,
											(void*)&stuff,
											PixelFunc8,
											output);
}

static PF_Err	
Render(	PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output)
{
	PF_Err	err	= PF_Err_NONE;

	PF_Pixel scratch8;

	//	If we're in Basic mode, use a preset. 
	//	Otherwise, use the value of our color param.
	
	if (MODE_BASIC == params[SUPER_MODE]->u.pd.value)	{
		GetPresetColorValue(params[SUPER_FLAVOR]->u.pd.value, &scratch8);
	} else {
		scratch8 = params[SUPER_COLOR]->u.cd.value;
	}
	PF_Boolean deepB =	PF_WORLD_IS_DEEP(output);
	
	if (deepB){
		ERR(Render16(in_data,
					out_data,
					params,
					output,
					&scratch8));

	} else {
		ERR(Render8(in_data,
					out_data,
					params,
					output,
					&scratch8));
	}
	return err;
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extraP)
{
	PF_Err	err				= PF_Err_NONE;

	PF_RenderRequest req	= extraP->input->output_request;

	PF_CheckoutResult		in_result;
	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	PF_Handle	infoH		=	suites.HandleSuite1()->host_new_handle(sizeof(prerender_stuff));

	prerender_stuff		*stuffP = NULL;

	if (infoH){
		stuffP = reinterpret_cast<prerender_stuff*>(suites.HandleSuite1()->host_lock_handle(infoH));
		if (stuffP){
			extraP->output->pre_render_data = infoH;
			
			/*	
				Let's investigate our input parameters, and save ourselves
				a few clues for rendering later. Because pre-render gets 
				called A LOT, it's best to put checkouts you'll always need
				in SmartRender(). Because smart pre-computing of what you'll
				actually NEED can save time, it's best to check conditionals
				here in pre-render.
			*/
			
			AEFX_CLR_STRUCT(in_result);

			if (!err){
				req.preserve_rgb_of_zero_alpha = FALSE;	
				
				ERR(extraP->cb->checkout_layer(	in_data->effect_ref,
												SUPER_INPUT,
												SUPER_INPUT,
												&req,
												in_data->current_time,
												in_data->time_step,
												in_data->time_scale,
												&in_result));
				if (!err){
					UnionLRect(&in_result.result_rect, 		&extraP->output->result_rect);
					UnionLRect(&in_result.max_result_rect, 	&extraP->output->max_result_rect);	
				}
			}
			suites.HandleSuite1()->host_unlock_handle(infoH);
		}
	}
	return err;
}

static PF_Err
SmartRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_SmartRenderExtra		*extraP)
{
	
	PF_Err			err			= PF_Err_NONE,
					err2		= PF_Err_NONE;
	
	PF_EffectWorld	*inputP		= NULL,
					*outputP	= NULL;
	
	PF_PixelFormat	format		=	PF_PixelFormat_INVALID;
	PF_WorldSuite2	*wsP		=	NULL;
	my_sequence_dataP	seqP	= reinterpret_cast<my_sequence_dataP>(DH(in_data->sequence_data));

	AEGP_SuiteHandler suites(in_data->pica_basicP);

	// Why the old way? Because AEGP_SuiteHandler has a collision with
	// AEGPWorldSuite2 and PF_WorldSuite2. That, and I'm too lazy
	// don't want to fiddle with a utility class shared by N effects,
	// where N approaches "a laughable amount of testing change for
	// questionable benefit".
	
	ERR(AEFX_AcquireSuite(	in_data, 
							out_data, 
							kPFWorldSuite, 
							kPFWorldSuiteVersion2, 
							STR(StrID_Err_LoadSuite),
							(void**)&wsP));
	if (!err && wsP && seqP){

		prerender_stuff *stuffP = reinterpret_cast<prerender_stuff*>(suites.HandleSuite1()->host_lock_handle(reinterpret_cast<PF_Handle>(extraP->input->pre_render_data)));
		if (stuffP){

			// checkout input & output buffers.

			ERR((extraP->cb->checkout_layer_pixels(	in_data->effect_ref, SUPER_INPUT, &inputP)));
			ERR(extraP->cb->checkout_output(in_data->effect_ref, &outputP));
			
			// determine requested output depth
			ERR(wsP->PF_GetPixelFormat(outputP, &format)); 

			if (seqP->advanced_modeB){

				PF_ParamDef		color_param;
				AEFX_CLR_STRUCT(color_param);

				ERR(PF_CHECKOUT_PARAM(	in_data, 
										SUPER_COLOR, 
										in_data->current_time, 
										in_data->time_step, 
										in_data->time_scale, 
										&color_param));
					
				ERR(suites.ColorParamSuite1()->PF_GetFloatingPointColorFromColorDef(in_data->effect_ref, &color_param, &stuffP->color));
				ERR2(PF_CHECKIN_PARAM(in_data, &color_param));
			} else {
				// Basic mode
				PF_ParamDef	flavor_param;
				PF_Pixel8 lo_rent_color;

				AEFX_CLR_STRUCT(lo_rent_color);
				AEFX_CLR_STRUCT(flavor_param);

				ERR(PF_CHECKOUT_PARAM(	in_data, 
										SUPER_FLAVOR, 
										in_data->current_time, 
										in_data->time_step, 
										in_data->time_scale, 
										&flavor_param));
				if (!err){
					GetPresetColorValue(flavor_param.u.pd.value, &lo_rent_color);
					
					// Rounding slop? Yes! But hey, whaddayawant, they're 0-255 presets...
					
					stuffP->color.red	= (PF_FpShort)lo_rent_color.red		/ PF_MAX_CHAN8;
					stuffP->color.green	= (PF_FpShort)lo_rent_color.green	/ PF_MAX_CHAN8;
					stuffP->color.blue	= (PF_FpShort)lo_rent_color.blue	/ PF_MAX_CHAN8;
					stuffP->color.alpha	= (PF_FpShort)lo_rent_color.alpha	/ PF_MAX_CHAN8;
				}
				ERR2(PF_CHECKIN_PARAM(in_data, &flavor_param));
			}

			switch (format) {
				
				case PF_PixelFormat_ARGB128:
					ERR(suites.IterateFloatSuite2()->iterate(	in_data,
																0,
																(outputP->extent_hint.bottom - outputP->extent_hint.top),
																inputP,
																&outputP->extent_hint,
																(void*)stuffP,
																PixelFuncFloat,
																outputP));
					break;
					
				case PF_PixelFormat_ARGB64:

					ERR(suites.Iterate16Suite2()->iterate(	in_data,
															0,
															(outputP->extent_hint.bottom - outputP->extent_hint.top),
															inputP,
															&outputP->extent_hint,
															(void*)stuffP,
															PixelFunc16,
															outputP));
					break;
					
				case PF_PixelFormat_ARGB32:

					ERR(suites.Iterate8Suite2()->iterate(	in_data,
															0,
															(outputP->extent_hint.bottom - outputP->extent_hint.top),
															inputP,
															&outputP->extent_hint,
															(void*)stuffP,
															PixelFunc8,
															outputP));

					break;

				default:
					err = PF_Err_INTERNAL_STRUCT_DAMAGED;
					break;
			}
			ERR2(extraP->cb->checkin_layer_pixels(in_data->effect_ref, SUPER_INPUT));
		}
		suites.HandleSuite1()->host_unlock_handle(reinterpret_cast<PF_Handle>(extraP->input->pre_render_data));
	}
	ERR2(AEFX_ReleaseSuite(	in_data,
							out_data,
							kPFWorldSuite, 
							kPFWorldSuiteVersion2, 
							STR(StrID_Err_FreeSuite)));
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
		"Supervisor", // Name
		"ADBE Supervisor", // Match Name
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
			
			case PF_Cmd_GLOBAL_SETDOWN:
				err = GlobalSetdown(in_data);
									
				break;

			case PF_Cmd_PARAMS_SETUP:
				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;

			case PF_Cmd_SEQUENCE_SETUP:
				err = SequenceSetup(in_data,out_data);
				break;

			case PF_Cmd_SEQUENCE_SETDOWN:
				err = SequenceSetdown(in_data,out_data);
				break;

			case PF_Cmd_SEQUENCE_RESETUP:
				err = SequenceSetup(in_data,out_data);
				break;

			case PF_Cmd_RENDER:
				err = Render(	in_data,
								out_data,
								params,
								output);
				break;

			case PF_Cmd_SMART_RENDER:
				err = SmartRender(	in_data,
									out_data,
									reinterpret_cast<PF_SmartRenderExtra*>(extra));
				break;

			case PF_Cmd_SMART_PRE_RENDER:
				err = PreRender(	in_data,
									out_data,
									reinterpret_cast<PF_PreRenderExtra*>(extra));
				break;
				
			case PF_Cmd_USER_CHANGED_PARAM:
				err = UserChangedParam(	in_data,
										out_data,
										params,
										output,
										reinterpret_cast<const PF_UserChangedParamExtra *>(extra));
				break;
				
			// Handling this selector will ensure that the UI will be properly initialized,
			// even before the user starts changing parameters to trigger PF_Cmd_USER_CHANGED_PARAM
			case PF_Cmd_UPDATE_PARAMS_UI:
				err = UpdateParameterUI(	in_data,
											out_data,
											params,
											output);

			default:
				break;
		}
	}
	catch(PF_Err &thrown_err){
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}
