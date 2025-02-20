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

#include "ColorGrid.h"

void	PFToDrawbotRect(const PF_Rect		*in_rectR,
						DRAWBOT_RectF32		*out_rectR)
{
	out_rectR->left = in_rectR->left + 0.5;
	out_rectR->top = in_rectR->top + 0.5;
	out_rectR->width = (float)(in_rectR->right - in_rectR->left);
	out_rectR->height = (float)(in_rectR->bottom - in_rectR->top);
}


#define		kMaxShortColor		65535

DRAWBOT_ColorRGBA QDtoDRAWBOTColor(const PF_App_Color *c)
{
	const float inv_sixty_five_kF = 1.0f / (float)(kMaxShortColor);
	
	DRAWBOT_ColorRGBA	color;
	
	color.red = c->red * inv_sixty_five_kF;
	color.green = c->green * inv_sixty_five_kF;
	color.blue = c->blue	* inv_sixty_five_kF;
	color.alpha = 1.0f;
	
	return color;
}


PF_Err	
AcquireBackgroundColor(	
					   PF_InData			*in_data,
					   PF_OutData			*out_data,
					   PF_ParamDef			*params[],
					   PF_EventExtra		*event_extra,
					   DRAWBOT_ColorRGBA	*drawbot_color)
{

	PF_Err			err		= PF_Err_NONE, err2	= PF_Err_NONE;

	PFAppSuite4		*app_suiteP = NULL;
	PF_App_Color	local_color = {0,0,0};

	ERR(AEFX_AcquireSuite(	in_data,
		out_data,
		kPFAppSuite,
		kPFAppSuiteVersion4,
		NULL,
		(void**)&app_suiteP));

	if (app_suiteP) {
		ERR(app_suiteP->PF_AppGetBgColor(&local_color));
		if (!err && drawbot_color) {
			*drawbot_color = QDtoDRAWBOTColor(&local_color);
		}
	}
	
	ERR2(AEFX_ReleaseSuite(	in_data,
		out_data,
		kPFAppSuite,
		kPFAppSuiteVersion4,
		NULL));
	
	return err;
}


PF_Boolean
ColorGrid_PointInRect(
	const PF_Point			*point,			/*>>*/ 
	const PF_Rect			*rectR)			/*>>*/
{
	PF_Boolean		result = FALSE;

	if(	point->h > rectR->left		&&
		point->h <= rectR->right		&&
		point->v > rectR->top		&&
		point->v <= rectR->bottom) {	
		
		result = TRUE; 
	}

	return result;
}


PF_Err
ColorGrid_Get_Box_In_Grid(
	const PF_Point		*origin,		/*>>*/
	A_long				grid_width,		/*>>*/
	A_long				grid_height,	/*>>*/
	A_long				box_across,		/*>>*/
	A_long				box_down,		/*>>*/
	PF_Rect				*box_in_rect)	/*<<*/
{
	PF_Err			err			= PF_Err_NONE;
	A_short			box_width	= 0,
					box_height	= 0;

	// Given the grid h+w and the box coord (0,0 through BOXES_ACROSS,BOXES_DOWN)
	// return the rect of the box

	box_width	= (A_short)(grid_width 	/ BOXES_ACROSS);
	box_height	= (A_short)(grid_height	/ BOXES_DOWN);

	box_in_rect->left	= (A_short)(box_width 	* box_across) 	+ origin->h;
	box_in_rect->top	= (A_short)(box_height 	* box_down) 	+ origin->v;
	box_in_rect->right	= (A_short)box_in_rect->left 			+ box_width;
	box_in_rect->bottom	= (A_short)box_in_rect->top 			+ box_height;

	return err;
}


PF_Err
ColorGrid_GetNewColor(
	PF_InData		*in_data,
	A_long			hposL,
	A_long			vposL,
	CG_ArbData		*arbP)
{
	PF_Err			err				= PF_Err_NONE;
	PF_PixelFloat	*box_colorP		= NULL;
	A_long			posL			= vposL * (BOXES_ACROSS) + hposL;

	if (hposL >= 0 && hposL < BOXES_ACROSS && vposL >= 0 && vposL < BOXES_DOWN)
	{
		box_colorP	= arbP->colorA;
		box_colorP	+= posL;

		AEGP_SuiteHandler suites(in_data->pica_basicP);
		
		// Premiere Pro/Elements don't support PF_AppColorPickerDialog
		if (in_data->appl_id != 'PrMr') {
			ERR(suites.AppSuite4()->PF_AppColorPickerDialog("ColorGrid!",
															box_colorP,
															TRUE,
															box_colorP));
		} else 	{
			box_colorP->red = (PF_FpShort)(rand() % PF_MAX_CHAN8) / 255.0;
			box_colorP->green = (PF_FpShort)(rand() % PF_MAX_CHAN8) / 255.0;
			box_colorP->blue = (PF_FpShort)(rand() % PF_MAX_CHAN8) / 255.0;
		}
	}
	return err;
}


PF_Err
DoClick(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*extra)
{
	PF_Err				err					= PF_Err_NONE;
	CG_ArbData			*arbP				= NULL;
	PF_Handle			arbH				= params[CG_GRID_UI]->u.arb_d.value;
	PF_Rect				box_rectR;
	PF_Rect				grid_rectR;

	PF_Point			mouse_pt, origin;
	A_char				count				= 0;
	A_long				box_across			= 0,
						box_down			= 0;
	PF_Boolean			point_in_rectB		= FALSE;
	
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	// Get the AdvAppSuite1 PICA suite to do Info window text in the 4.x and later style
	arbP	= reinterpret_cast<CG_ArbData*>(PF_LOCK_HANDLE(arbH));

	/*
		Do the hit detection of where in the UI the user clicked, 
		determine which of the custom UI grids needs updating, 
		update and refresh UI appropriately
	*/
	
	if(!err){
		mouse_pt.v	= extra->u.do_click.screen_point.v;
		mouse_pt.h	= extra->u.do_click.screen_point.h;
		origin.h	= extra->effect_win.current_frame.left;
		origin.v	= extra->effect_win.current_frame.top;

		grid_rectR.top	= origin.v;
		grid_rectR.left	= origin.h;
		grid_rectR.right 	= grid_rectR.left 	+ UI_GRID_WIDTH;
		grid_rectR.bottom 	= grid_rectR.top 	+ UI_GRID_HEIGHT;
		
		if (extra->effect_win.area == PF_EA_CONTROL){	
			// Is the click in the grid?
			if(ColorGrid_PointInRect(&mouse_pt, &grid_rectR)){
				for(count = 0; count < CG_ARBDATA_ELEMENTS; count++)
				{
					ColorGrid_Get_Box_In_Grid(	&origin,
												UI_GRID_WIDTH,
												UI_GRID_HEIGHT,
												box_across,
												box_down,
												&box_rectR);

					point_in_rectB = ColorGrid_PointInRect(&mouse_pt, &box_rectR);
					
					if(point_in_rectB){
						count = CG_ARBDATA_ELEMENTS;
					} else {
						++box_across;
						if(box_across == BOXES_ACROSS){
							++box_down;
							box_across = 0;
						}
					}
				}

				err = ColorGrid_GetNewColor(in_data, 
											box_across,
											box_down,
											arbP);

				// Specify the area to redraw
				PF_Rect inval(extra->effect_win.current_frame);
				ERR(suites.AppSuite4()->PF_InvalidateRect(extra->contextH, &inval));

				extra->evt_out_flags					|= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
				params[CG_GRID_UI]->uu.change_flags 	|= PF_ChangeFlag_CHANGED_VALUE;
			}
			PF_UNLOCK_HANDLE(arbH);
		}
	}

	// Premiere Pro/Elements does not support this suite
	if (in_data->appl_id != 'PrMr')
	{
		ERR(suites.AdvAppSuite2()->PF_InfoDrawText("ColorGrid - DoClick Event","Adobe Inc"));
	}

	return err;
}


PF_Err
Deactivate(
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	PF_EventExtra	*extra)
{
	PF_Err 				err 	= PF_Err_NONE;

	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	// Premiere Pro/Elements does not support this suite
	if (in_data->appl_id != 'PrMr')
	{
		ERR(suites.AdvAppSuite2()->PF_InfoDrawText("ColorGrid - Deactivate Event","Adobe Inc"));
	}

	return err;
}


PF_Err 
DrawEvent(	
		PF_InData			*in_data,
		PF_OutData			*out_data,
		PF_ParamDef			*params[],
		PF_LayerDef			*output,
		PF_EventExtra		*event_extra,
		PF_Pixel			some_color)
{
	PF_Err					err			= PF_Err_NONE,
							err2		= PF_Err_NONE;

	// Setup the arb data handling
	PF_Handle				arbH		= params[CG_GRID_UI]->u.arb_d.value;
	CG_ArbData				*arbP;
	PF_Point				origin;
	PF_Rect					box_rectR;
	A_long					box_across	= 0,
							box_down	= 0;
	float					grid_width	= 0,
							grid_height = 0;
	int						i			= 0;
	PF_PixelFloat			*pixP		= NULL;	
	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	DRAWBOT_DrawRef			drawing_ref = NULL;
	DRAWBOT_SurfaceRef		surface_ref = NULL;
	DRAWBOT_SupplierRef		supplier_ref = NULL;
	DRAWBOT_PathRef			path_ref = NULL;
	DRAWBOT_BrushRef		brush_ref = NULL;
	DRAWBOT_Suites			drawbotSuites;
	DRAWBOT_RectF32			onscreen_rectR;
	DRAWBOT_ColorRGBA		background_color;
	DRAWBOT_ColorRGBA		pixel_colr[CG_ARBDATA_ELEMENTS];
	
	// Acquire all the drawbot suites in one go; it should be matched with release routine.
	// You can also use C++ style AEFX_DrawbotSuitesScoper which doesn't need release routine.
	ERR(AEFX_AcquireDrawbotSuites(in_data, out_data, &drawbotSuites));
	
	// Get the drawing reference by passing context to this new api
	PF_EffectCustomUISuite1	*effectCustomUISuiteP;
	err = AEFX_AcquireSuite(in_data,
							out_data,
							kPFEffectCustomUISuite,
							kPFEffectCustomUISuiteVersion1,
							NULL,
							(void**)&effectCustomUISuiteP);
	if (!err && effectCustomUISuiteP) {
		err = (*effectCustomUISuiteP->PF_GetDrawingReference)(event_extra->contextH, &drawing_ref);
		AEFX_ReleaseSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, NULL);
	}

	// Get the drawbot supplier from drawing reference; it shouldn't be released like pen or brush (see below)
	ERR(drawbotSuites.drawbot_suiteP->GetSupplier(drawing_ref, &supplier_ref));
	
	// Get the drawbot surface from drawing reference; it shouldn't be released like pen or brush (see below)
	ERR(drawbotSuites.drawbot_suiteP->GetSurface(drawing_ref, &surface_ref));
	
	ERR(AcquireBackgroundColor(	in_data,
								out_data,
								params,
								event_extra,
								&background_color));
	
	if (!err && event_extra->effect_win.area == PF_EA_CONTROL) {
		// Use to fill background with AE's BG color
		onscreen_rectR.left 	= (float)event_extra->effect_win.current_frame.left;
		onscreen_rectR.top		= (float)event_extra->effect_win.current_frame.top;
		onscreen_rectR.width 	= (float)(event_extra->effect_win.current_frame.right - event_extra->effect_win.current_frame.left);
		onscreen_rectR.height 	= (float)(event_extra->effect_win.current_frame.bottom + 1 - event_extra->effect_win.current_frame.top);

		origin.v	= (short)onscreen_rectR.top;
		origin.h	= (short)onscreen_rectR.left;

		// Calculate the space taken up by the grid
		// Allow the width to scale horizontally, but not too much
		grid_width = onscreen_rectR.width;
		grid_height = onscreen_rectR.height;
		if (grid_width < UI_GRID_WIDTH / 1.5) {
			grid_width = UI_GRID_WIDTH / 1.5f;
		} else if (grid_width > UI_GRID_WIDTH * 1.5) {
			grid_width = UI_GRID_WIDTH * 1.5f;
		}
	}
	
	ERR(drawbotSuites.surface_suiteP->PaintRect(surface_ref, &background_color, &onscreen_rectR));
	
	// Get the arb data to fill out the grid colors
	arbP = (CG_ArbData*)PF_LOCK_HANDLE(arbH);

	if(arbP) {
		pixP = reinterpret_cast<PF_PixelFloat*>(arbP);	

		for(i = 0; i < CG_ARBDATA_ELEMENTS; i++){
			pixel_colr[i].red = pixP->red;
			pixel_colr[i].green = pixP->green;
			pixel_colr[i].blue = pixP->blue;
			pixel_colr[i].alpha = 1.0;
			pixP++;
		}
		
		for(i = 0; i < BOXES_PER_GRID && !err; i++){
			// Fill in our grid
			
			ERR(ColorGrid_Get_Box_In_Grid(&origin,
										  static_cast<A_long>(grid_width),
										  static_cast<A_long>(grid_height),
										  box_across,
										  box_down,
										  &box_rectR));
			
			// Create a new path; it should be matched with release routine.
			// You can also use C++ style DRAWBOT_PenP which doesn't need release routine.
			ERR(drawbotSuites.supplier_suiteP->NewPath(supplier_ref, &path_ref));
			
			DRAWBOT_RectF32		rectR;
			PFToDrawbotRect(&box_rectR, &rectR);
			
			// Add a rect to the path
			ERR(drawbotSuites.path_suiteP->AddRect(path_ref, &rectR));
			
			// Create a new brush taking color as input; it should be matched with release routine.
			// You can also use C++ style DRAWBOT_BrushP which doesn't need release routine.
			ERR(drawbotSuites.supplier_suiteP->NewBrush(supplier_ref, &(pixel_colr[i]), &brush_ref));
			
			// Fill the path with the brush.
			ERR(drawbotSuites.surface_suiteP->FillPath(surface_ref, brush_ref, path_ref, kDRAWBOT_FillType_Default));
			
			// Release/destroy the brush. It will lead to memory leak otherwise. 
			if (brush_ref) {
				ERR2(drawbotSuites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)brush_ref));
			}
			
			// Release/destroy the path. It will lead to memory leak otherwise.
			if (path_ref) {
				ERR2(drawbotSuites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path_ref));
			}
			
			box_across++;
			
			if(box_across == BOXES_ACROSS)	{
				box_across = 0;
				box_down++;
			}
		}
	}
	
	DRAWBOT_ColorRGBA	black_color;
	
	black_color.red = black_color.green = black_color.blue = 0.0;
	black_color.alpha = 1.0;

	DRAWBOT_PenRef	pen_ref = NULL;
	
	ERR(drawbotSuites.supplier_suiteP->NewPen(supplier_ref, &black_color, 1.0, &pen_ref));
	
	// Draw the grid - outline
	for(box_down = 0; box_down < BOXES_DOWN && !err; ++box_down) {
		for(box_across = 0; box_across < BOXES_ACROSS && !err; ++box_across) {
			
			ERR(ColorGrid_Get_Box_In_Grid(	&origin,
											static_cast<A_long>(grid_width),
											static_cast<A_long>(grid_height),
											box_across,
											box_down,
											&box_rectR));
			
			ERR(drawbotSuites.supplier_suiteP->NewPath(supplier_ref, &path_ref));
			
			DRAWBOT_RectF32		rectR;
			PFToDrawbotRect(&box_rectR, &rectR);

			ERR(drawbotSuites.path_suiteP->AddRect(path_ref, &rectR));
			
			ERR(drawbotSuites.surface_suiteP->StrokePath(surface_ref, pen_ref, path_ref));
			
			if (path_ref) {
				ERR2(drawbotSuites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path_ref));
			}
		}
	}
	
	if (pen_ref) {
		ERR2(drawbotSuites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)pen_ref));
	}
	
	PF_UNLOCK_HANDLE(arbH);
	
	// Release the earlier acquired drawbot suites
	ERR2(AEFX_ReleaseDrawbotSuites(in_data, out_data));

	event_extra->evt_out_flags	= PF_EO_HANDLED_EVENT;

	return err;
}

PF_Err 
ChangeCursor(	
		PF_InData			*in_data,
		PF_OutData			*out_data,
		PF_ParamDef			*params[],
		PF_LayerDef			*output,
		PF_EventExtra		*extra)
{
	PF_Err				err					= PF_Err_NONE;

	PF_Point			origin;	
	PF_Point			mouse_pt;
	PF_Rect				grid_rectR;

	AEGP_SuiteHandler		suites(in_data->pica_basicP);

	//	Determine where the mouse is, and sent the info window the appropriate color
	//	and change the cursor only when over the ColorGrid
	if(!err) {
		mouse_pt.v	= extra->u.adjust_cursor.screen_point.v;
		mouse_pt.h	= extra->u.adjust_cursor.screen_point.h;
		origin.h	= extra->effect_win.current_frame.left;
		origin.v	= extra->effect_win.current_frame.top;

		grid_rectR.top		= origin.v;
		grid_rectR.left		= origin.h;
		grid_rectR.right 	= grid_rectR.left 	+ UI_GRID_WIDTH;
		grid_rectR.bottom 	= grid_rectR.top 	+ UI_GRID_HEIGHT;

		if (extra->effect_win.area == PF_EA_CONTROL){	
			// Is the click in the grid?
			if(ColorGrid_PointInRect(&mouse_pt, &grid_rectR)){
				extra->u.adjust_cursor.set_cursor = PF_Cursor_EYEDROPPER;
			}
		}
	}

	// Premiere Pro/Elements does not support this suite
	if (in_data->appl_id != 'PrMr')
	{
		ERR(suites.AdvAppSuite2()->PF_InfoDrawText("ColorGrid - ChangeCursor Event","Adobe Inc"));
	}

	return err;
}


