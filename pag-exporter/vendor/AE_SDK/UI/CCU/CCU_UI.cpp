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

#include "CCU.h"
#include <adobesdk/DrawbotSuite.h>
#include "AEFX_SuiteHelper.h"

// Calculates the points for an oval bounded by oval_frame 
static void
CalculateOval (
	PF_InData			*in_data,
	PF_Rect				*oval_frame,
	DRAWBOT_PointF32	*poly_oval)
{
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	PF_FpLong 			radF = 0,
						oval_width = (oval_frame->right - oval_frame->left) / 2,
						oval_height = (oval_frame->bottom - oval_frame->top) / 2;

	// Calculate center point for oval
	DRAWBOT_PointF32	center_point;
	center_point.x = oval_frame->left + oval_width;
	center_point.y = oval_frame->top + oval_height;

	// Plot out the oval with OVAL_PTS number of points
	for (A_short iS = 0; iS < OVAL_PTS; ++iS) {
		radF		= (2.0 * PF_PI * iS) / OVAL_PTS;

		// Premiere Pro/Elements doesn't support ANSICallbacksSuite1
		if (in_data->appl_id != 'PrMr') {
			poly_oval[iS].x = suites.ANSICallbacksSuite1()->sin(radF);
			poly_oval[iS].y = suites.ANSICallbacksSuite1()->cos(radF);
		} else {
			poly_oval[iS].x = sin(radF);
			poly_oval[iS].y = cos(radF);
		}

		// Transform the point on a unit circle to the corresponding point on the oval
		poly_oval[iS].x = poly_oval[iS].x * oval_width + center_point.x;
		poly_oval[iS].y = poly_oval[iS].y * oval_height + center_point.y;
	}
}

// Draws an oval
static PF_Err
DrawOval(
	PF_InData		*in_data,
	PF_Rect			*oval_frame,
	DRAWBOT_DrawRef	drawing_ref,
	DRAWBOT_Suites	*drawbot_suitesP)
{
	PF_Err					err = PF_Err_NONE;
	AEGP_SuiteHandler		suites(in_data->pica_basicP);
	DRAWBOT_PointF32		poly_oval[OVAL_PTS];
	DRAWBOT_SupplierRef		supplier_ref = NULL;
	
	ERR(drawbot_suitesP->drawbot_suiteP->GetSupplier(drawing_ref, &supplier_ref));
	DRAWBOT_PathP			pathP(drawbot_suitesP->supplier_suiteP, supplier_ref);

	CalculateOval(in_data, oval_frame, poly_oval);
	
	ERR(drawbot_suitesP->path_suiteP->MoveTo(pathP, poly_oval[0].x, poly_oval[0].y));

	for (A_short i = 0; i < OVAL_PTS; i++) {
		ERR(drawbot_suitesP->path_suiteP->LineTo(pathP, poly_oval[i].x, poly_oval[i].y));
	}

	ERR(drawbot_suitesP->path_suiteP->LineTo(pathP, poly_oval[0].x, poly_oval[0].y));

	// Currently, EffectCustomUIOverlayThemeSuite is unsupported in Premiere Pro/Elements
	if (in_data->appl_id != 'PrMr')
	{
		ERR(suites.EffectCustomUIOverlayThemeSuite1()->PF_StrokePath(drawing_ref, pathP, FALSE));
	}
	else
	{
		DRAWBOT_ColorRGBA	foreground_color;
		DRAWBOT_SurfaceRef	surface_ref;
		foreground_color.alpha = 1.0f;
		foreground_color.blue = 0.9f;
		foreground_color.green = 0.9f;
		foreground_color.red = 0.9f;
		ERR(suites.DrawbotSuiteCurrent()->GetSurface(drawing_ref, &surface_ref));
		DRAWBOT_PenP		penP(drawbot_suitesP->supplier_suiteP, supplier_ref, &foreground_color, 1.0);

		drawbot_suitesP->surface_suiteP->StrokePath(surface_ref, penP, pathP);
	}
	
	return err;
}

// Calculates the frame in fixed coordinates based on the params passed in
void 
FixedFrameFromParams (
	PF_InData		*in_data,
	PF_ParamDef		*params[],
	PF_FixedRect	*frameFiRP)
{
	A_FpLong	xF			= FIX_2_FLOAT(params[CCU_PT]->u.td.x_value),
				yF			= FIX_2_FLOAT(params[CCU_PT]->u.td.y_value),
				x_radF		= FIX_2_FLOAT(params[CCU_X_RADIUS]->u.fd.value),
				y_radF		= FIX_2_FLOAT(params[CCU_Y_RADIUS]->u.fd.value),
				par_invF	= static_cast<A_FpLong>(in_data->pixel_aspect_ratio.den) / in_data->pixel_aspect_ratio.num;
	
	frameFiRP->top			= FLOAT2FIX(yF - y_radF);
	frameFiRP->bottom		= FLOAT2FIX(yF + y_radF);
	frameFiRP->left			= FLOAT2FIX(xF - x_radF * par_invF);
	frameFiRP->right		= FLOAT2FIX(xF + x_radF * par_invF);
}

void 
FrameFromParams (
	PF_InData		*in_data,
	PF_ParamDef		*params[],
	PF_Rect			*frameRP)
{
	A_FpLong	xF			= FIX2INT(params[CCU_PT]->u.td.x_value),		
				yF			= FIX2INT(params[CCU_PT]->u.td.y_value),
				x_radF		= FIX2INT(params[CCU_X_RADIUS]->u.fd.value),
				y_radF		= FIX2INT(params[CCU_Y_RADIUS]->u.fd.value),
				par_invF	= static_cast<A_FpLong>(in_data->pixel_aspect_ratio.den) / in_data->pixel_aspect_ratio.num; 
	
	frameRP->top			= static_cast<A_short>(yF - y_radF);
	frameRP->bottom			= static_cast<A_short>(yF + y_radF);
	frameRP->left			= static_cast<A_short>(xF - x_radF * par_invF);
	frameRP->right			= static_cast<A_short>(xF + x_radF * par_invF);
}

void 
Source2FrameRect (
				  PF_InData		*in_data,
				  PF_EventExtra	*event_extraP,
				  PF_FixedRect	*fx_frameRP,
				  PF_FixedPoint	*bounding_boxFiPtAP)		// Array of four -> shows real bounding box
{
	bounding_boxFiPtAP[0].x = (fx_frameRP->left);
	bounding_boxFiPtAP[0].y = (fx_frameRP->top);
	
	bounding_boxFiPtAP[1].x = (fx_frameRP->right);
	bounding_boxFiPtAP[1].y = (fx_frameRP->top);
	
	bounding_boxFiPtAP[2].x = (fx_frameRP->right);
	bounding_boxFiPtAP[2].y = (fx_frameRP->bottom);
	
	bounding_boxFiPtAP[3].x = (fx_frameRP->left);
	bounding_boxFiPtAP[3].y = (fx_frameRP->bottom);
	
	if (PF_Window_COMP == (*event_extraP->contextH)->w_type) {
		for (A_short iS = 0; iS < 4; ++iS){
			event_extraP->cbs.layer_to_comp(	event_extraP->cbs.refcon, 
												event_extraP->contextH, 
												in_data->current_time, 
												in_data->time_scale, 
												&bounding_boxFiPtAP[iS]);
		}
	}
	for (A_short jS = 0; jS < 4; ++jS) {
		event_extraP->cbs.source_to_frame(	event_extraP->cbs.refcon, 
											event_extraP->contextH, 
											&bounding_boxFiPtAP[jS]);
	}	
	
	fx_frameRP->left = bounding_boxFiPtAP[0].x;
	fx_frameRP->top = bounding_boxFiPtAP[0].y;
	fx_frameRP->right = bounding_boxFiPtAP[1].x;
	fx_frameRP->bottom = bounding_boxFiPtAP[2].y;
}

void	
CompFrame2Layer(
				PF_InData		*in_data,	
				PF_EventExtra	*event_extraP,		
				PF_Point		*framePt,		
				PF_Point		*lyrPt,		
				PF_FixedPoint	*fix_lyrFiPt)	
{
	fix_lyrFiPt->x = INT2FIX(framePt->h);
	fix_lyrFiPt->y = INT2FIX(framePt->v);
	
	event_extraP->cbs.frame_to_source(	event_extraP->cbs.refcon, 
										event_extraP->contextH, 
										fix_lyrFiPt);
	
	// Now back into layer space
	
	event_extraP->cbs.comp_to_layer(event_extraP->cbs.refcon, 
									event_extraP->contextH, 
									in_data->current_time, 
									in_data->time_scale, 
									fix_lyrFiPt);
	
	lyrPt->h = fix_lyrFiPt->x;
	lyrPt->v = fix_lyrFiPt->y;
}

void 
Layer2CompFrame (
				 PF_InData		*in_data,	
				 PF_EventExtra	*event_extraP,		
				 PF_Point		*layerPtP,		
				 PF_Point		*framePtP,		
				 PF_FixedPoint	*fix_frameFiPtP)
{
	fix_frameFiPtP->x = INT2FIX(layerPtP->h);
	fix_frameFiPtP->y = INT2FIX(layerPtP->v);
	
	event_extraP->cbs.layer_to_comp(event_extraP->cbs.refcon, 
									event_extraP->contextH, 
									in_data->current_time, 
									in_data->time_scale, 
									fix_frameFiPtP);
	
	event_extraP->cbs.source_to_frame(	event_extraP->cbs.refcon, 
										event_extraP->contextH, 
										fix_frameFiPtP);
	
	framePtP->h = FIX2INT(fix_frameFiPtP->x);
	framePtP->v = FIX2INT(fix_frameFiPtP->y);
}

void	
LayerFrame2Layer(
				 PF_InData		*in_data,	
				 PF_EventExtra	*event_extraP,		
				 PF_Point		*framePtP,	
				 PF_Point		*lyrPtP,	
				 PF_FixedPoint	*fix_lyrPtP)
{
	fix_lyrPtP->x = INT2FIX(framePtP->h);
	fix_lyrPtP->y = INT2FIX(framePtP->v);
	
	event_extraP->cbs.frame_to_source(	event_extraP->cbs.refcon, 
										event_extraP->contextH, 
										fix_lyrPtP);
	
	// Now back into layer space
	
	lyrPtP->h = FIX2INT(fix_lyrPtP->x);
	lyrPtP->v = FIX2INT(fix_lyrPtP->y);
}

void 
Layer2LayerFrame (
				  PF_InData		*in_data,	
				  PF_EventExtra	*event_extraP,		
				  PF_Point		*layerPtP,		
				  PF_Point		*framePtP,		
				  PF_FixedPoint	*fix_frameFiPtP)
{
	fix_frameFiPtP->x = INT2FIX(layerPtP->h);
	fix_frameFiPtP->y = INT2FIX(layerPtP->v);
	
	event_extraP->cbs.source_to_frame(	event_extraP->cbs.refcon, 
										event_extraP->contextH, 
										fix_frameFiPtP);
	
	framePtP->h = FIX2INT(fix_frameFiPtP->x);
	framePtP->v = FIX2INT(fix_frameFiPtP->y);
}

void 
DrawHandles (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_EventExtra	*event_extraP)
{
	PF_Err				err			= PF_Err_NONE;

	DRAWBOT_Suites		drawbot_suites;
	DRAWBOT_DrawRef		drawing_ref = NULL;
	DRAWBOT_SurfaceRef	surface_ref = NULL;

	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	DRAWBOT_ColorRGBA	foreground_color;
	DRAWBOT_RectF32		box			=	{0.0,0.0,6.0,6.0};
	PF_Rect				frameR;			
	PF_FixedRect		fx_frameFiR	=	{0,0,0,0};

	PF_FixedPoint		pointsFiPtA[4];

	FixedFrameFromParams(in_data, params, &fx_frameFiR);

	Source2FrameRect(in_data, event_extraP, &fx_frameFiR, pointsFiPtA);
		
	PF_FIXEDRECT_2_RECT(fx_frameFiR, frameR);
	
	AEFX_AcquireDrawbotSuites(in_data, out_data, &drawbot_suites);
	ERR(suites.EffectCustomUISuite1()->PF_GetDrawingReference(event_extraP->contextH, &drawing_ref));
	
	if (drawing_ref)
	{
		ERR(suites.DrawbotSuiteCurrent()->GetSurface(drawing_ref, &surface_ref));

		// Currently, EffectCustomUIOverlayThemeSuite is unsupported in Premiere Pro/Elements
		if (in_data->appl_id != 'PrMr')
		{
			ERR(suites.EffectCustomUIOverlayThemeSuite1()->PF_GetPreferredForegroundColor(&foreground_color));
		}
		else
		{
			foreground_color.alpha = 1.0f;
			foreground_color.blue = 0.9f;
			foreground_color.green = 0.9f;
			foreground_color.red = 0.9f;
		}

		DrawOval(in_data, &frameR, drawing_ref, &drawbot_suites);

		box.top			= static_cast<float>(frameR.top 	- 3);
		box.left		= static_cast<float>(frameR.left 	- 3);
		ERR(suites.SurfaceSuiteCurrent()->PaintRect(surface_ref, &foreground_color, &box));

		box.top			= static_cast<float>(frameR.top 	- 3);
		box.left		= static_cast<float>(frameR.right 	- 3);
		ERR(suites.SurfaceSuiteCurrent()->PaintRect(surface_ref, &foreground_color, &box));

		box.top			= static_cast<float>(frameR.bottom	- 3);
		box.left		= static_cast<float>(frameR.left 	- 3);
		ERR(suites.SurfaceSuiteCurrent()->PaintRect(surface_ref, &foreground_color, &box));

		box.top			= static_cast<float>(frameR.bottom - 3);
		box.left		= static_cast<float>(frameR.right 	- 3);
		ERR(suites.SurfaceSuiteCurrent()->PaintRect(surface_ref, &foreground_color, &box));

		AEFX_ReleaseDrawbotSuites(in_data, out_data);
	}
}

PF_Err 
DrawEvent (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extraP)
{
	PF_Err			err 		= PF_Err_NONE;

	if ((PF_Window_LAYER	==	(*event_extraP->contextH)->w_type)	||
		(PF_Window_COMP		==	(*event_extraP->contextH)->w_type)	) {

		DrawHandles(in_data, out_data, params, event_extraP);

		event_extraP->evt_out_flags = PF_EO_HANDLED_EVENT;
	}
	return err;
}

PF_Boolean 
DoClickHandles (
	PF_InData		*in_data,
	PF_Rect			*frameRP,					
	PF_ParamDef		*params[],
	FrameFunc		frame_func,
	PF_EventExtra	*event_extraP)
{
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	PF_Boolean		doneB 			= 	FALSE;

	PF_Point		mouse_downPt	=	{0,0}, 
					cornersPtA[4];

	PF_FixedPoint	mouse_layerFiPt	=	{0,0};

	A_short			hitS 		= 	-1;
					
	A_long			slopL 		= 	0;

	cornersPtA[0].h 	= frameRP->left;
	cornersPtA[0].v 	= frameRP->top;
	
	cornersPtA[1].h 	= frameRP->right;
	cornersPtA[1].v 	= frameRP->top;
	
	cornersPtA[2].h 	= frameRP->right;
	cornersPtA[2].v 	= frameRP->bottom;
	
	cornersPtA[3].h 	= frameRP->left;
	cornersPtA[3].v 	= frameRP->bottom;
  	
	mouse_downPt 		= *(reinterpret_cast<PF_Point*>(&event_extraP->u.do_click.screen_point));

	for (A_short iS = 0; iS < 4; ++iS) {
		// Convert cornersPtA to comp frameRP
		
		(*frame_func)(	in_data, 
						event_extraP, 
						&cornersPtA[iS], 
						&cornersPtA[iS], 
						&mouse_layerFiPt);

		// Premiere doesn't support the ANSICallbacksSuite
		if (in_data->appl_id != 'PrMr') {
			slopL	= 		(long)suites.ANSICallbacksSuite1()->fabs(cornersPtA[iS].h	- mouse_downPt.h);
			slopL	+=	 	(long)suites.ANSICallbacksSuite1()->fabs(cornersPtA[iS].v	- mouse_downPt.v);
		} else {
			slopL	= 		(long)abs(cornersPtA[iS].h	- mouse_downPt.h);
			slopL	+=	 	(long)abs(cornersPtA[iS].v	- mouse_downPt.v);
		}
		
		if (slopL < CCU_SLOP) {
			hitS	= iS;
			doneB	= TRUE;

			event_extraP->u.do_click.send_drag				= TRUE;
			event_extraP->u.do_click.continue_refcon[0] 	= CCU_Handles;
			event_extraP->u.do_click.continue_refcon[1] 	= mouse_layerFiPt.x;
			event_extraP->u.do_click.continue_refcon[2] 	= mouse_layerFiPt.y;
			event_extraP->u.do_click.continue_refcon[3] 	= FALSE;				
			break;
		}
	}	
	return doneB;
}

PF_Boolean	
DoDragHandles(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	FrameFunc		frame_func,
	PF_EventExtra	*event_extraP)
{
	AEGP_SuiteHandler	suites(in_data->pica_basicP);
	
	PF_Boolean		doneB 			= FALSE;

	PF_Point		mouse_downPt	=	{0,0};

	PF_FixedPoint	mouse_layerFiPt	=	{0,0},
					centerFiPt		=	{0,0},
					old_centerFiPt	=	{0,0};

	A_FpLong		new_xF			=	0,
					parF			=	static_cast<A_FpLong>(in_data->pixel_aspect_ratio.num) / in_data->pixel_aspect_ratio.den;

	PF_Boolean		drawB 			= 	TRUE;

	if (event_extraP->evt_in_flags & PF_EI_DONT_DRAW) {
		drawB = FALSE;
	}

	mouse_downPt = *(reinterpret_cast<PF_Point*>(&event_extraP->u.do_click.screen_point));

	(*frame_func)(	in_data, 
					event_extraP, 
					&mouse_downPt, 
					(PF_Point*)&mouse_downPt, 
					&mouse_layerFiPt);
 
 	old_centerFiPt.x = event_extraP->u.do_click.continue_refcon[1];
	old_centerFiPt.y = event_extraP->u.do_click.continue_refcon[2];
	
	centerFiPt.x = params[CCU_PT]->u.td.x_value;
	centerFiPt.y = params[CCU_PT]->u.td.y_value;

	// Calculate new radius
	new_xF = FIX_2_FLOAT(centerFiPt.x - mouse_layerFiPt.x) * parF;	
	if (in_data->appl_id != 'PrMr') {
		params[CCU_X_RADIUS]->u.fd.value 		= (A_long)suites.ANSICallbacksSuite1()->fabs(FLOAT2FIX(new_xF));
		params[CCU_Y_RADIUS]->u.fd.value 		= (A_long)suites.ANSICallbacksSuite1()->fabs(centerFiPt.y - mouse_layerFiPt.y);
	} else {
		params[CCU_X_RADIUS]->u.fd.value 		= (A_long)abs(FLOAT2FIX(new_xF));
		params[CCU_Y_RADIUS]->u.fd.value 		= (A_long)abs(centerFiPt.y - mouse_layerFiPt.y);
	}

	params[CCU_X_RADIUS]->uu.change_flags 	= PF_ChangeFlag_CHANGED_VALUE;
	params[CCU_Y_RADIUS]->uu.change_flags 	= PF_ChangeFlag_CHANGED_VALUE;

	doneB 											= TRUE;
	event_extraP->u.do_click.send_drag 				= TRUE;
	event_extraP->u.do_click.continue_refcon[0] 	= CCU_Handles;
	event_extraP->u.do_click.continue_refcon[1] 	= mouse_layerFiPt.x;
	event_extraP->u.do_click.continue_refcon[2] 	= mouse_layerFiPt.y;
	event_extraP->u.do_click.continue_refcon[3] 	= TRUE;
	
	if (event_extraP->u.do_click.last_time) {
		event_extraP->u.do_click.continue_refcon[0] = 0;
		event_extraP->u.do_click.send_drag 			= FALSE;
	}	
	return doneB;
}

PF_Err 
DoClick (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extraP)
{
	PF_Err 					err 		= 	PF_Err_NONE;
	PF_ContextH				contextH 	= 	event_extraP->contextH;
	PF_Rect					frameR		=	{0,0,0,0};
	
	FrameFromParams(in_data, 
					params, 
					&frameR);

	if (PF_Window_LAYER == (*contextH)->w_type) {
		if (DoClickHandles(	in_data, 
							&frameR, 
							params, 
							Layer2LayerFrame, 
							event_extraP)) 	{
			event_extraP->evt_out_flags = PF_EO_HANDLED_EVENT;
		}
		
	} else if (PF_Window_COMP == (*contextH)->w_type) {
		if (DoClickHandles(	in_data, 
							&frameR, 
							params, 
							Layer2CompFrame, 
							event_extraP)) {
			event_extraP->evt_out_flags = PF_EO_HANDLED_EVENT;
		}
	}	

	return err;
}

PF_Err	
DoDrag(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*event_extraP)
{
	PF_Err 					err 		= 	PF_Err_NONE;
	PF_ContextH				contextH 	= 	event_extraP->contextH;

	PF_DoClickEventInfo 	do_click;

	FrameFunc				frame_func	= 	NULL;

	PF_Rect					framePR		=	{0,0,0,0};

	AEFX_CLR_STRUCT(do_click);
	
	FrameFromParams(in_data, 
					params, 
					&framePR);

	do_click = event_extraP->u.do_click;
	
	if (PF_Window_LAYER == (*contextH)->w_type) {
		frame_func = LayerFrame2Layer;
	} else if (PF_Window_COMP == (*contextH)->w_type) {
		frame_func = CompFrame2Layer;
	}
	
	if (frame_func) {
		switch (do_click.continue_refcon[0]) {		
			case CCU_Handles:
				DoDragHandles(	in_data,
								out_data,
								params, 
								frame_func, 
								event_extraP);
				event_extraP->evt_out_flags = PF_EO_HANDLED_EVENT;
				break;
		}
	}
	return err;
}

PF_Err 
HandleEvent (
			 PF_InData		*in_data,
			 PF_OutData		*out_data,
			 PF_ParamDef		*params[],
			 PF_LayerDef		*output,
			 PF_EventExtra	*event_extraP)
{
	PF_Err		err = PF_Err_NONE;
	
	if (PF_Event_DO_CLICK == event_extraP->e_type) {
		if (event_extraP->u.do_click.send_drag) {
			event_extraP->e_type = PF_Event_DRAG;
		}
	}
	
	switch (event_extraP->e_type) {
		
		case PF_Event_NEW_CONTEXT:
			break;
			
		case PF_Event_ACTIVATE:
			break;
			
		case PF_Event_DO_CLICK:
			DoClick(in_data, 
					out_data, 
					params, 
					output, 
					event_extraP);
			break;
			
		case PF_Event_DRAG:
			DoDrag(	in_data, 
					out_data, 
					params, 
					output, 
					event_extraP);
			break;
			
		case PF_Event_DRAW:
			DrawEvent(	in_data, 
						out_data, 
						params, 
						output, 
						event_extraP);
			break;
			
		case PF_Event_DEACTIVATE:
			break;
			
		case PF_Event_CLOSE_CONTEXT:
			break;
			
		default:
		case PF_Event_IDLE:
			break;
			
	}
	return err;
}
