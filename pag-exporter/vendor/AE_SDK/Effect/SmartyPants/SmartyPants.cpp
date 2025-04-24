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

/*	SmartyPants.cpp	

	This sample exercises some of After Effects' image processing
	callback functions.

	Revision History

	Version		Change													Engineer	Date
	=======		======													========	======
	1.0			Seemed like a good idea at the time.					bbb			2/11/2005
	1.1			Add demonstration of GuidMixInPtr()						zal			3/24/2015
	1.2			Added new entry point									zal			9/18/2017
	1.3			Remove deprecated 'register' keyword					cb			12/18/2020
	1.4			Added 'Support URL' to PiPL and entry point				cjr			3/31/2023

*/

#include "SmartyPants.h"


static 
A_long 
Pin32(
	A_long bottom, 
	A_long target, 
	A_long top)
{
	return MAX(bottom, MIN(target, top));
}

static A_long
RoundDouble(
	PF_FpLong	xF)
{
	A_long	retF = 0;
	
	if (xF > 0) {
		retF = (A_long)((xF + 0.5));
	} else {
		if ((A_long)(xF + 0.5) == (xF + 0.5)) {
			retF = (A_long)xF;
		} else {
			retF = (A_long)(xF - 0.5);
		}
	}
	return retF;
}

static void
FindDeepHSL(	
	A_u_short 	cmax,
	A_u_short 	cmidSu,
	A_u_short 	cminSu,
	A_long 		*hueP,
	A_long 		*satP,
	A_long 		*lightP)
{
	*lightP 	= (A_long)cminSu + cmax;
	*satP 		= (A_long)cmax - cminSu;
	
	if (*satP != 0){
		*hueP = (4096 * ((A_long)cmidSu - cminSu) + ((*satP) >> 1)) / (*satP);
	} else{
		*hueP = 0;
	}
}

  
static void 
RGBtoHSL16(	
	A_u_short 	r,
	A_u_short 	g,
	A_u_short 	b,
	A_long 		*H,
	A_long 		*S,
	A_long 		*L )
{
	if (g >= b) {
		if (r >= b) {
			if (r >= g) {
				FindDeepHSL (r, g, b, H, S, L);
			} else {
				FindDeepHSL (g, r, b, H, S, L);
				*H = 8192 - *H;
			}
		} else {
			FindDeepHSL (g, b, r, H, S, L);
			*H = 8192 + *H;
		}
	} else {
		if (r >= g)	{
			if (r >= b)	{
				FindDeepHSL (r, b, g, H, S, L);
				if (*H != 0){
					*H = 24576 - *H;
				}
			} else {
				FindDeepHSL (b, r, g, H, S, L);
				*H = 16384 + *H;
			}
		} else {
			FindDeepHSL (b, g, r, H, S, L);
			*H = 16384 - *H;
		}
	}
}

static void 
PutHSL16 (
	A_long 		hue,
	A_long 		sat,
	A_long 		light,
	A_u_short 	*cmax,
	A_u_short 	*cmid,
	A_u_short 	*cmin)

{
	*cmin = (A_u_short) light;
	*cmid = (A_u_short) (light + (( (hue * sat) + 2048) / 4096));
	*cmax = (A_u_short) (sat + light);
}

static void 
PutHSLFloat (
	A_long 		hue,
	A_long 		sat,
	A_long 		light,
	PF_FpShort 	*cmax,
	PF_FpShort 	*cmid,
	PF_FpShort 	*cmin)

{
	*cmin = (PF_FpShort) (light / 32768.0);
	*cmid = (PF_FpShort) ((light + (( (hue * sat) + 2048) / 4096)) / 32768.0);
	*cmax = (PF_FpShort) ((sat + light) / 32768.0);
}

 
  
static void 
HSLtoRGB16(	
	A_long 		H,							   
	A_long 		S,
	A_long 		L,
	A_u_short 	*r,
	A_u_short 	*g,
	A_u_short 	*b )

{
	A_long temp = L;

	/* Find maximum gap */
	if (temp > 32768){
		temp = 65536 - temp;
	}
	/* Clamp gap to maximum */
	if (S > temp){
		S = temp;
	}
	/* Convert lightness to minimum */
	L = (L - S) >> 1;
	
	if (H < 4096){
		PutHSL16 (H, S, L, r, g, b);
	} else if (H < 8192) {
		PutHSL16 (8192 - H, S, L, g, r, b);
	} else if (H < 12288) {
		PutHSL16 (H - 8192, S, L, g, b, r);
	} else if (H < 16384) {
		PutHSL16 (16384 - H, S, L, b, g, r);
	} else if (H < 20480) {
		PutHSL16 (H - 16384, S, L, b, r, g);
	} else {
		PutHSL16 (24576 - H, S, L, r, b, g);
	}
}

static void 
HSLtoRGBFloat(	
	A_long 		H,							   
	A_long 		S,
	A_long 		L,
	PF_FpShort 	*r,
	PF_FpShort 	*g,
	PF_FpShort 	*b )

{
	A_long temp = L;

	/* Find maximum gap */
	if (temp > 32768){
		temp = 65536 - temp;
	}
	/* Clamp gap to maximum */
	if (S > temp){
		S = temp;
	}
	/* Convert lightness to minimum */
	L = (L - S) >> 1;
	
	if (H < 4096){
		PutHSLFloat (H, S, L, r, g, b);
	} else if (H < 8192) {
		PutHSLFloat (8192 - H, S, L, g, r, b);
	} else if (H < 12288) {
		PutHSLFloat (H - 8192, S, L, g, b, r);
	} else if (H < 16384) {
		PutHSLFloat (16384 - H, S, L, b, g, r);
	} else if (H < 20480) {
		PutHSLFloat (H - 16384, S, L, b, r, g);
	} else {
		PutHSLFloat (24576 - H, S, L, r, b, g);
	}
}

static void
RGBtoYIQ(	
	A_u_short		rSu,
	A_u_short		gSu,
	A_u_short		bSu,
	A_FpLong		*yP,
	A_FpLong		*iP,
	A_FpLong		*qP ) 

{
	*yP = rSu * 0.2989 + gSu * 0.5866 + bSu * 0.1144;
 	*iP = rSu * 0.5959 - gSu * 0.2741 - bSu * 0.3218;
	*qP = rSu * 0.2113 - gSu * 0.5227 + bSu * 0.3113;
}

static void
YIQtoRGB(	
	A_FpLong		yF,
	A_FpLong		iF,
	A_FpLong      	qF,
	A_u_short		*rP,
	A_u_short		*gP,
	A_u_short		*bP)
{
  	*rP = Pin32(0, RoundDouble(yF + iF * 0.9562 + qF * 0.6210), PF_MAX_CHAN16);
	*gP = Pin32(0, RoundDouble(yF - iF * 0.2717 - qF * 0.6485), PF_MAX_CHAN16);
	*bP = Pin32(0, RoundDouble(yF - iF * 1.1053 + qF * 1.7020), PF_MAX_CHAN16);
}

static void
YIQtoRGBFloat(	
	A_FpLong		yF,
	A_FpLong		iF,
	A_FpLong      	qF,
	PF_FpShort		*rP,
	PF_FpShort		*gP,
	PF_FpShort		*bP)
{
  	*rP = (PF_FpShort)RoundDouble(yF + iF * 0.9562 + qF * 0.6210) / 32768.0;
	*gP = (PF_FpShort)RoundDouble(yF - iF * 0.2717 - qF * 0.6485) / 32768.0;
	*bP = (PF_FpShort)RoundDouble(yF - iF * 1.1053 + qF * 1.7020) / 32768.0;
}

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_SPRINTF(	out_data->return_msg,
				"%s %d.%d\r\r%s",
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
	//	We do very little here.
		
	out_data->my_version 	= 	PF_VERSION(	MAJOR_VERSION, 
											MINOR_VERSION,
											BUG_VERSION, 
											STAGE_VERSION, 
											BUILD_VERSION);

	out_data->out_flags |=	PF_OutFlag_DEEP_COLOR_AWARE |
							PF_OutFlag_PIX_INDEPENDENT |
							PF_OutFlag_USE_OUTPUT_EXTENT;
							
	out_data->out_flags2 	=	PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG	|
								PF_OutFlag2_SUPPORTS_SMART_RENDER				|
								PF_OutFlag2_FLOAT_COLOR_AWARE					|
								PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS		|
								PF_OutFlag2_I_MIX_GUID_DEPENDENCIES				|
								PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	
	return PF_Err_NONE;
}

static PF_Err
PreRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_PreRenderExtra		*extra)
{
	PF_Err err = PF_Err_NONE;
	PF_ParamDef channel_param;
	PF_RenderRequest req = extra->input->output_request;
	PF_CheckoutResult in_result;

	// Mix in the background color of the comp, as a demonstration of GuidMixInPtr()
	// When the background color changes, the effect will need to be rerendered.
	// Note: This doesn't handle the collapsed comp case
	// Your effect can use a similar approach to trigger a rerender based on changes beyond just its effect parameters.
	AEGP_LayerH		layerH;
	AEGP_CompH		compH;
	AEGP_ColorVal	bg_color;
	if (extra->cb->GuidMixInPtr) {

		AEFX_SuiteScoper<AEGP_PFInterfaceSuite1> PFInterfaceSuite = AEFX_SuiteScoper<AEGP_PFInterfaceSuite1>(	in_data,
																												kAEGPPFInterfaceSuite,
																												kAEGPPFInterfaceSuiteVersion1,
																												out_data);
		AEFX_SuiteScoper<AEGP_LayerSuite8> layerSuite = AEFX_SuiteScoper<AEGP_LayerSuite8>(	in_data,
																							kAEGPLayerSuite,
																							kAEGPLayerSuiteVersion8,
																							out_data);
		AEFX_SuiteScoper<AEGP_CompSuite10> compSuite = AEFX_SuiteScoper<AEGP_CompSuite10>(  in_data,
																							kAEGPCompSuite,
																							kAEGPCompSuiteVersion10,
																							out_data);

		PFInterfaceSuite->AEGP_GetEffectLayer(in_data->effect_ref, &layerH);
		layerSuite->AEGP_GetLayerParentComp(layerH, &compH);
		compSuite->AEGP_GetCompBGColor(compH, &bg_color);

		extra->cb->GuidMixInPtr(in_data->effect_ref, sizeof(bg_color), reinterpret_cast<void *>(&bg_color));
	}
	
	AEFX_CLR_STRUCT(channel_param);
	
	//	In order to know whether we care about alpha
	//	or not, we check out our channel pull-down
	//	(the old-fashioned way); if it's set to alpha,
	//	we care. 			-bbb 10/4/05.
	
	ERR(PF_CHECKOUT_PARAM(	in_data, 
							SMARTY_CHANNEL, 
							in_data->current_time, 
							in_data->time_step, 
							in_data->time_scale, 
							&channel_param));

	if (channel_param.u.pd.value == Channel_ALPHA){
		req.channel_mask |=  PF_ChannelMask_ALPHA;	
	}

	req.preserve_rgb_of_zero_alpha = TRUE;	//	Hey, we care.

	ERR(extra->cb->checkout_layer(	in_data->effect_ref,
									SMARTY_INPUT,
									SMARTY_INPUT,
									&req,
									in_data->current_time,
									in_data->time_step,
									in_data->time_scale,
									&in_result));


	UnionLRect(&in_result.result_rect, 		&extra->output->result_rect);
	UnionLRect(&in_result.max_result_rect, 	&extra->output->max_result_rect);	
	
	//	Notice something missing, namely the PF_CHECKIN_PARAM to balance
	//	the old-fashioned PF_CHECKOUT_PARAM, above? 
	
	//	For SmartFX, AE automagically checks in any params checked out 
	//	during PF_Cmd_SMART_PRE_RENDER, new or old-fashioned.
	
	return err;
}

static PF_Err
InvertPixel8(
	void 		*refcon, 
	A_long 		x, 
	A_long 		y, 
	PF_Pixel 	*in, 
	PF_Pixel 	*out)
{
	SmartyPantsDataPlus	*idp = (SmartyPantsDataPlus*)refcon;
	SmartyPantsData		*id = idp->id;
	A_long					i;

	for (i=x; i<idp->outputP->width+x; i++) {
		PF_YIQ_Pixel			yiq;
		PF_HLS_Pixel			hls;
	
		switch (id->channel) {
			case Channel_ALPHA:
				*out = *in;
				out->alpha = (id->blend * in->alpha + (256 - id->blend) * (PF_MAX_CHAN8 - in->alpha)) >> 8;
				break;

			case Channel_RGB:
				out->red 	= PF_MAX_CHAN8 - in->red;
				out->green 	= PF_MAX_CHAN8 - in->green;
				out->blue 	= PF_MAX_CHAN8 - in->blue;
				out->alpha 	= in->alpha;
				break;

			case Channel_RED:
				out->red 	= PF_MAX_CHAN8 - in->red;
				out->green 	= in->green;
				out->blue 	= in->blue;
				out->alpha 	= in->alpha;
				break;

			case Channel_GREEN:
				out->red 	= in->red;
				out->green 	= PF_MAX_CHAN8 - in->green;
				out->blue 	= in->blue;
				out->alpha 	= in->alpha;
				break;

			case Channel_BLUE:
				out->red 	= in->red;
				out->green 	= in->green;
				out->blue 	= PF_MAX_CHAN8 - in->blue;
				out->alpha 	= in->alpha;
				break;

				
			case Channel_HLS:
			case Channel_HUE:
			case Channel_LIGHTNESS:
			case Channel_SATURATION:
				out->alpha 	= in->alpha;
				idp->RGBtoHLScb(idp->in_data->effect_ref, in, hls);

				switch (id->channel) {
					case Channel_HLS:
						hls[0] = INT2FIX(180) - hls[0];
						if (hls[0] < 0) {
							hls[0] = hls[0] + INT2FIX(360);
						}
						hls[1] = A_Fixed_ONE - hls[1];
						hls[2] = A_Fixed_ONE - hls[2];
						break;
					case Channel_HUE:
						hls[0] = INT2FIX(180) - hls[0];
						if (hls[0] < 0) {
							hls[0] = hls[0] + INT2FIX(360);
						}
						break;
					case Channel_LIGHTNESS:
						hls[1] = A_Fixed_ONE - hls[1];
						break;
					case Channel_SATURATION:
						hls[2] = A_Fixed_ONE - hls[2];
						break;
				}
				idp->HLStoRGBcb(idp->in_data->effect_ref, hls, out);
				break;

			case Channel_YIQ:
			case Channel_LUMINANCE:
			case Channel_IN_PHASE_CHROMINANCE:
			case Channel_QUADRATURE_CHROMINANCE:
				out->alpha = in->alpha;
				idp->RGBtoYIQcb(idp->in_data->effect_ref, in, yiq); 


				switch (id->channel) {
				case Channel_YIQ:
					yiq[0] = A_Fixed_ONE - yiq[0];
					yiq[1] =  - yiq[1];
					yiq[2] =  - yiq[2];
					break;
				case Channel_LUMINANCE:
					yiq[0] = A_Fixed_ONE - yiq[0];
					break;
				case Channel_IN_PHASE_CHROMINANCE:
					yiq[1] = - yiq[1];
					break;
				case Channel_QUADRATURE_CHROMINANCE:
					yiq[2] = - yiq[2];
					break;
				}

				idp->YIQtoRGBcb(idp->in_data->effect_ref, yiq, out); 
				break;

			default:
				*out = *in;
				break;
		}

		if (id->blend && (id->channel != Channel_ALPHA)) {
			out->red 	= out->red 	 + (((in->red - out->red) 		* id->blend) >> 8);
			out->green 	= out->green + (((in->green - out->green) 	* id->blend) >> 8);
			out->blue 	= out->blue  + (((in->blue - out->blue) 	* id->blend) >> 8);
		}
		++in;
		++out;
	}
	return PF_Err_NONE;
}

static PF_Err
InvertPixel16(
	void		*refcon, 
	A_long 		x, 
	A_long 		y, 
	PF_Pixel16 	*in, 
	PF_Pixel16 	*out)
{
	SmartyPantsDataPlus	*idp 	= (SmartyPantsDataPlus*)refcon;
	SmartyPantsData		*id		= idp->id;

	for (A_long iL = x; iL < idp->outputP->width+x; iL++) {
		A_long 	hueL, satL, lightL;
		A_FpLong yF, iF, qF;
 
 		switch (id->channel) {
			case Channel_ALPHA:
				*out = *in;
 				out->alpha = (id->blend * in->alpha + (PF_MAX_CHAN8 - id->blend) * (PF_MAX_CHAN16 - in->alpha)) >> 8;
				break;
			case Channel_RGB:
				out->red 	= PF_MAX_CHAN16 - in->red;
				out->green 	= PF_MAX_CHAN16 - in->green;
				out->blue 	= PF_MAX_CHAN16 - in->blue;
				out->alpha 	= in->alpha;
				break;
			case Channel_RED:
				out->red 	= PF_MAX_CHAN16 - in->red;
				out->green 	= in->green;
				out->blue 	= in->blue;
				out->alpha 	= in->alpha;
				break;
			case Channel_GREEN:
				out->red 	= in->red;
				out->green 	= PF_MAX_CHAN16 - in->green;
				out->blue 	= in->blue;
				out->alpha 	= in->alpha;
				break;
			case Channel_BLUE:
				out->red 	= in->red;
				out->green 	= in->green;
				out->blue 	= PF_MAX_CHAN16 - in->blue;
				out->alpha 	= in->alpha;
				break;

				
			case Channel_HLS:
			case Channel_HUE:
			case Channel_LIGHTNESS:
			case Channel_SATURATION:
 
				out->alpha = in->alpha;
 				RGBtoHSL16(in->red, in->green, in->blue, &hueL, &satL, &lightL);

				switch (id->channel) {
				case Channel_HLS:
 					hueL =	12288 - hueL;
					if(hueL < 0) {
						hueL = hueL + 24576; 
					}
					satL = PF_MAX_CHAN16 - satL;
					lightL = (PF_MAX_CHAN16 << 1) - lightL;
					break;
				case Channel_HUE:
 					hueL =	12288 - hueL;
					if(hueL < 0) {
						hueL = hueL + 24576; 
					}
 					break;
				case Channel_LIGHTNESS:
					lightL = (PF_MAX_CHAN16 << 1) - lightL;
 					break;
				case Channel_SATURATION:
					satL = PF_MAX_CHAN16 - satL;
 					break;
				}

 				HSLtoRGB16(	hueL, 
							satL, 
							lightL, 
							&(out->red), 
							&(out->green), 
							&(out->blue));
 
				break;

			case Channel_YIQ:
			case Channel_LUMINANCE:
			case Channel_IN_PHASE_CHROMINANCE:
			case Channel_QUADRATURE_CHROMINANCE:
				out->alpha = in->alpha;
				RGBtoYIQ(	in->red, 
							in->green, 
							in->blue, 
							&yF, 
							&iF, 
							&qF ); 

 				switch (id->channel) {
					case Channel_YIQ:

						yF = PF_MAX_CHAN16 - yF;
						iF = -iF;
						qF = -qF;
	  					break;
	  					
					case Channel_LUMINANCE:
						yF = PF_MAX_CHAN16 - yF;
	  					break;
	  					
					case Channel_IN_PHASE_CHROMINANCE:
	 					iF = -iF;
						break;
						
					case Channel_QUADRATURE_CHROMINANCE:
						qF = -qF;
	 					break;
				}
				YIQtoRGB(	yF, 
							iF, 
							qF, 
							&(out->red), 
							&(out->green), 
							&(out->blue)); 
 				break;

			default:
				*out = *in;
				break;
		}

		if (id->blend && (id->channel != Channel_ALPHA)) {
			out->red 	= out->red 		+ (((in->red - out->red) 		* id->blend) >> 8);
			out->green 	= out->green 	+ (((in->green - out->green) 	* id->blend) >> 8);
			out->blue 	= out->blue 	+ (((in->blue - out->blue) 		* id->blend) >> 8);
		}
		++in;
		++out;
	}
 	return PF_Err_NONE;
}

static PF_Err
InvertPixelFloat(
	void			*refcon, 
	A_long 			x, 
	A_long 			y, 
	PF_PixelFloat 	*in, 
	PF_PixelFloat 	*out)
{
	SmartyPantsDataPlus	*idp 	= reinterpret_cast<SmartyPantsDataPlus*>(refcon);
	SmartyPantsData		*id		= idp->id;

	for (A_long iL = x; iL < idp->outputP->width+x; iL++) {
		A_long	        		hueL, satL, lightL;
		A_FpLong					yF, iF, qF;
 
 		switch (id->channel) {
			case Channel_ALPHA:
				*out = *in;
 				out->alpha = 1.0 - in->alpha;
				break;
			case Channel_RGB:
				out->red 	= 1.0 - in->red;
				out->green 	= 1.0 - in->green;
				out->blue 	= 1.0 - in->blue;
				out->alpha 	= in->alpha;
				break;
			case Channel_RED:
				*out 		= *in;
				out->red 	= 1.0 - in->red;
				break;
			case Channel_GREEN:
				*out 		= *in;
				out->green 	= 1.0 - in->green;
				break;
			case Channel_BLUE:
				*out 		= *in;
				out->blue 	= 1.0 - in->blue;
				break;

				
			case Channel_HLS:
			case Channel_HUE:
			case Channel_LIGHTNESS:
			case Channel_SATURATION:
 
				out->alpha = in->alpha;
 				RGBtoHSL16((A_u_short)(32768.0 * in->red), (A_u_short)(32768.0 * in->green), (A_u_short)(32768.0 * in->blue), &hueL, &satL, &lightL);

				switch (id->channel) {
				case Channel_HLS:
 					hueL =	12288 - hueL;
					if(hueL < 0) {
						hueL = hueL + 24576; 
					}
					satL = PF_MAX_CHAN16 - satL;
					lightL = (PF_MAX_CHAN16 << 1) - lightL;
					break;
				case Channel_HUE:
 					hueL =	12288 - hueL;
					if(hueL < 0) {
						hueL = hueL + 24576; 
					}
 					break;
				case Channel_LIGHTNESS:
					lightL = (PF_MAX_CHAN16 << 1) - lightL;
 					break;
				case Channel_SATURATION:
					satL = PF_MAX_CHAN16 - satL;
 					break;
				}

 				HSLtoRGBFloat(	hueL, 
 								satL, 
 								lightL, 
 								&(out->red), 
 								&(out->green), 
 								&(out->blue));
 
				break;

			case Channel_YIQ:
			case Channel_LUMINANCE:
			case Channel_IN_PHASE_CHROMINANCE:
			case Channel_QUADRATURE_CHROMINANCE:
				out->alpha = in->alpha;
				RGBtoYIQ(	(A_u_short)(32768.0 * in->red), 
							(A_u_short)(32768.0 * in->green), 
							(A_u_short)(32768.0 * in->blue), 
							&yF, 
							&iF, 
							&qF ); 

 				switch (id->channel) {
					case Channel_YIQ:

						yF = PF_MAX_CHAN16 - yF;
						iF = -iF;
						qF = -qF;
	  					break;
					case Channel_LUMINANCE:
						yF = PF_MAX_CHAN16 - yF;
	  					break;
					case Channel_IN_PHASE_CHROMINANCE:
	 					iF = -iF;
						break;
					case Channel_QUADRATURE_CHROMINANCE:
						qF = -qF;
	 					break;
				}
				YIQtoRGBFloat(	yF, 
								iF, 
								qF, 
								&(out->red), 
								&(out->green), 
								&(out->blue)); 

 				break;

			default:
				*out = *in;
				break;
		}

		if (id->blend && (id->channel != Channel_ALPHA)) {
			/*
			out->red 	= (out->red 	+ in->red)		/ 2.0;
			out->green 	= (out->green 	+ in->green)	/ 2.0;
			out->blue 	= (out->blue 	+ in->blue)		/ 2.0;
			*/
			out->red 	= out->red 		+ (((in->red - out->red) 		* id->blend) / 256.0);
			out->green 	= out->green 	+ (((in->green - out->green) 	* id->blend) / 256.0);
			out->blue 	= out->blue 	+ (((in->blue - out->blue) 		* id->blend) / 256.0);
		}
		++in;
		++out;
	}
 	return PF_Err_NONE;
}

static PF_Err
ActuallyRender(
	PF_InData		*in_data,
	PF_EffectWorld 	*input,
	PF_ParamDef		*blend_paramP,
	PF_ParamDef		*channel_paramP,
	PF_OutData		*out_data,
	PF_EffectWorld	*output)
{
	PF_Err				err 	= PF_Err_NONE,
						err2 	= PF_Err_NONE;
	SmartyPantsData		*id;
	SmartyPantsDataPlus	idp;
	PF_Point			origin;
	PF_Rect				src_rect, areaR;
	PF_PixelFormat		format	=	PF_PixelFormat_INVALID;
	PF_WorldSuite2		*wsP	=	NULL;
	
	AEGP_SuiteHandler suites(in_data->pica_basicP);
 
	src_rect.left 	= -in_data->output_origin_x;
	src_rect.top 	= -in_data->output_origin_y;
	src_rect.bottom = src_rect.top + output->height;
	src_rect.right 	= src_rect.left + output->width;

	ERR(AEFX_AcquireSuite(	in_data, 
							out_data, 
							kPFWorldSuite, 
							kPFWorldSuiteVersion2, 
							"Couldn't load suite.",
							(void**)&wsP));


	if (!in_data->sequence_data) {
		PF_COPY(input, output, &src_rect, NULL);
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}

	if (!err){
		id = (SmartyPantsData*)*(in_data->sequence_data);

		id->blend = (A_short)(2.56 * blend_paramP->u.fs_d.value);
		
		idp.id 			= id;
		idp.in_data 	= in_data;
		idp.RGBtoYIQcb 	= ((PF_UtilCallbacks*)(in_data->utils))->colorCB.RGBtoYIQ;
		idp.RGBtoHLScb 	= ((PF_UtilCallbacks*)(in_data->utils))->colorCB.RGBtoHLS;
		idp.HLStoRGBcb 	= ((PF_UtilCallbacks*)(in_data->utils))->colorCB.HLStoRGB;
		idp.YIQtoRGBcb 	= ((PF_UtilCallbacks*)(in_data->utils))->colorCB.YIQtoRGB;
		idp.outputP 	= output;

		if (id->blend == 256) {
			err = PF_COPY(input, output, &src_rect, NULL);
		} else {
			if (id->channel != channel_paramP->u.pd.value) {
				id->channel = channel_paramP->u.pd.value;
			}

			origin.h = in_data->output_origin_x;
			origin.v = in_data->output_origin_y;

			areaR.top		= 
			areaR.left 		= 0;
			areaR.right		= 1;
			areaR.bottom	= output->height;

			ERR(wsP->PF_GetPixelFormat(input, &format));

			switch (format) {
			
				case PF_PixelFormat_ARGB128:
				
					ERR(suites.IterateFloatSuite2()->iterate_origin(in_data,
																	0,
																	output->height,
																	input,
																	&areaR,
																	&origin,
																	(void*)(&idp),
																	InvertPixelFloat,
																	output));
					break;
					
				case PF_PixelFormat_ARGB64:
			
					ERR(suites.Iterate16Suite2()->iterate_origin(	in_data,
																	0,
																	output->height,
																	input,
																	&areaR,
																	&origin,
																	(void*)(&idp),
																	InvertPixel16,
																	output));
					break;
					
				case PF_PixelFormat_ARGB32:
					 
					
					ERR(suites.Iterate8Suite2()->iterate_origin(	in_data,
																	0,
																	output->height,
																	input,
																	&areaR,
																	&origin,
																	(void*)(&idp),
																	InvertPixel8,
																	output));
					break;

				default:
					err = PF_Err_BAD_CALLBACK_PARAM;
					break;
			}
		}
	}
	ERR2(AEFX_ReleaseSuite(	in_data,
							out_data,
							kPFWorldSuite, 
							kPFWorldSuiteVersion2, 
							"Couldn't release suite."));
	return err;
}

static PF_Err
SmartRender(
	PF_InData				*in_data,
	PF_OutData				*out_data,
	PF_SmartRenderExtra		*extra)

{

	PF_Err			err		= PF_Err_NONE,
					err2 	= PF_Err_NONE;
					
	PF_EffectWorld	*input_worldP	= NULL, 
					*output_worldP  = NULL;
	
	PF_ParamDef channel_param, blend_param;

	AEFX_CLR_STRUCT(channel_param);
	AEFX_CLR_STRUCT(blend_param);
	
	// checkout input & output buffers.
	ERR((extra->cb->checkout_layer_pixels(	in_data->effect_ref, SMARTY_INPUT, &input_worldP)));

	ERR(extra->cb->checkout_output(	in_data->effect_ref, &output_worldP));

	// checkout the required params
	ERR(PF_CHECKOUT_PARAM(	in_data, 
							SMARTY_BLEND, 
							in_data->current_time, 
							in_data->time_step, 
							in_data->time_scale, 
							&blend_param));
							
	ERR(PF_CHECKOUT_PARAM(	in_data, 
							SMARTY_CHANNEL, 
							in_data->current_time, 
							in_data->time_step, 
							in_data->time_scale, 
							&channel_param));

	ERR(ActuallyRender(	in_data, 
						input_worldP, 
						&blend_param, 
						&channel_param,
						out_data, 
						output_worldP));

	// Always check in, no matter what the error condition!
	
	ERR2(PF_CHECKIN_PARAM(in_data, &blend_param));
	ERR2(PF_CHECKIN_PARAM(in_data, &channel_param));

	return err;
  
}


static PF_Err 
ParamsSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err			err = PF_Err_NONE;

	A_char			*popup = (A_char*)STR(StrID_Channels); //"RGB|Red|Green|Blue|(-|HLS|Hue|Lightness|Saturation|(-|YIQ|Luminance|In Phase Chrominance|Quadrature Chrominance|(-|Alpha";
	PF_ParamDef		def;
	AEFX_CLR_STRUCT(def);

	def.flags = 0;
	def.ui_flags = def.ui_width = def.ui_height = 0;

	def.param_type 			= PF_Param_POPUP;
	def.u.pd.dephault 		= def.u.pd.value = Channel_RGB;
	def.u.pd.num_choices 	= Channel_ALPHA; // last param
	def.u.pd.u.namesptr 	= popup;

	PF_STRCPY(def.name, STR(StrID_ChannelParam_Name)); //"Channel"

	ERR(PF_ADD_PARAM(in_data, -1, &def));

	def.param_type 				= PF_Param_FLOAT_SLIDER;
	def.u.fs_d.valid_min 		= def.u.fs_d.slider_min 	= SMARTY_BLEND_MIN;
	def.u.fs_d.valid_max 		= def.u.fs_d.slider_max 	= SMARTY_BLEND_MAX;
	def.u.fs_d.value 			= def.u.fs_d.dephault 		= SMARTY_BLEND_DFLT;
	def.u.fs_d.precision 		= 1;	
	def.u.fs_d.display_flags	= PF_ValueDisplayFlag_PERCENT;
	
	PF_STRCPY(def.name, STR(StrID_Layer_Param_Name)); //"Layer Blend Ratio";

	ERR(PF_ADD_PARAM(in_data, -1, &def));

	out_data->num_params = SMARTY_NUM_PARAMS;
	
	return err;
}

static PF_Err
SequenceSetup(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	PF_Err	err	=	PF_Err_NONE;

	if ((out_data->sequence_data = PF_NEW_HANDLE(sizeof(SmartyPantsData)))) {
		((SmartyPantsData *)DH(out_data->sequence_data))->channel = -1L;
		out_data->flat_sdata_size = sizeof(SmartyPantsData);

	} else {
		PF_STRCPY(out_data->return_msg, STR(StrID_Err_LoadSuite));
		out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
		err = PF_Err_INTERNAL_STRUCT_DAMAGED;
	}
	return err;
}

static PF_Err
SequenceSetdown(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output)
{
	if (in_data->sequence_data) {
		PF_DISPOSE_HANDLE(in_data->sequence_data);
		in_data->sequence_data = out_data->sequence_data = NULL;
	}
	return PF_Err_NONE;
}

static PF_Err
SequenceResetup(
	PF_InData		*in_data,
	PF_OutData				*out_data,
	PF_ParamDef				*params[],
	PF_LayerDef				*output)
{
	if (in_data->sequence_data) {
		PF_DISPOSE_HANDLE(in_data->sequence_data);
	}
	return SequenceSetup(in_data, out_data, params, output);
}

static PF_Err
QueryDynamicFlags(	
	PF_InData		*in_data,	
	PF_OutData		*out_data,	
	PF_ParamDef		*params[],	
	void			*extra)	
{
	PF_Err 	err 	= PF_Err_NONE,
			err2 	= PF_Err_NONE;

	PF_ParamDef def;

	AEFX_CLR_STRUCT(def);
	
	ERR(PF_CHECKOUT_PARAM(	in_data, 
							SMARTY_CHANNEL, 
							in_data->current_time,
							in_data->time_step,
							in_data->time_scale,
							&def));

	if (!err && (def.u.pd.value == Channel_ALPHA)){
		out_data->out_flags2 &= PF_OutFlag2_DOESNT_NEED_EMPTY_PIXELS;
	}
	ERR2(PF_CHECKIN_PARAM(in_data, &def));
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
		"SmartyPants", // Name
		"ADBE SmartyPants", // Match Name
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
	
	try	{
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
			case PF_Cmd_SEQUENCE_RESETUP:
				err = SequenceResetup(in_data,out_data,params,output);
				break;
			case PF_Cmd_SEQUENCE_SETDOWN:
				err = SequenceSetdown(in_data,out_data,params,output);
				break;
			case PF_Cmd_QUERY_DYNAMIC_FLAGS:
				err = QueryDynamicFlags(in_data,out_data,params,extra);
				break;
			case PF_Cmd_SMART_PRE_RENDER:
				err = PreRender(in_data, out_data, (PF_PreRenderExtra*)extra);
				break;
			case PF_Cmd_SMART_RENDER:
				err = SmartRender(in_data, out_data, (PF_SmartRenderExtra*)extra);
				break;
		}
	} catch(PF_Err &thrown_err) {
		err = thrown_err;
	}
	return err;
}
