/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*******************************************************************/
/*																   */
/* this file contains the old or deprecated versions of the suites */
/* listed in AE_GeneralPlug.h									   */
/*******************************************************************/


#include <AE_GeneralPlugPre.h>

typedef struct {	// note: unused values are still stored in settings and used when cycling through
					//	the 3 types using cmd/ctrl-click on timecode
	AEGP_TimeDisplayType	time_display_type;
	A_char					timebaseC;			// only used for AEGP_TimeDisplayType_TIMECODE, 1 to 100
	A_Boolean				non_drop_30B;		// only used for AEGP_TimeDisplayType_TIMECODE,
												//	when timebaseC == 30 && item framerate == 29.97, use drop frame or non-drop?
	A_char					frames_per_footC;	// only used for AEGP_TimeDisplayType_FEET_AND_FRAMES
	A_long					starting_frameL;	// usually 0 or 1, not used for AEGP_TimeDisplayType_TIMECODE
} AEGP_TimeDisplay;

/**
 ** Canvas Suite
 ** Used by artisans to render layers
 **/

#define kAEGPCanvasSuiteVersion1		4	/* frozen in AE 5.0 */

typedef struct AEGP_CanvasSuite1 {

	SPAPI A_Err	(*AEGP_GetCompToRender)(
						PR_RenderContextH 		render_contextH,   	/* >> */
						AEGP_CompH				*compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumLayersToRender)(
				const	PR_RenderContextH	render_contextH,
						A_long				*num_to_renderPL);


	SPAPI A_Err	(*AEGP_GetNthLayerContextToRender)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n,					/* >> */
						AEGP_RenderLayerContextH	*layer_contextPH);	/* << */

	SPAPI A_Err	(*AEGP_GetLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,
						AEGP_RenderLayerContextH	layer_contextH,
						AEGP_LayerH					*layerPH);

	SPAPI A_Err	(*AEGP_GetCompRenderTime)(
						PR_RenderContextH	render_contextH,	/* >> */
						A_Time				*time,				/* << */
						A_Time				*time_step);

	SPAPI A_Err	(*AEGP_GetCompDestinationBuffer)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_CompH					compH,				/* >> */
						PF_EffectWorld					*dst);				/* <> */

	SPAPI A_Err	(*AEGP_GetROI)(
						PR_RenderContextH			render_contextH,	/* <> */
						A_LegacyRect						*roiPR);			/* << */

	/// for rendering track mattes
	SPAPI A_Err	(*AEGP_RenderLayer)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH					layerH,				/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						PF_EffectWorld					*render_bufferP);	/* >> */

	// for rendering the texture map of a layer
	SPAPI A_Err	(*AEGP_RenderTexture)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						A_FloatPoint				*suggested_scaleP0,	/* >> */
						A_FloatRect					*suggested_src_rectP0, /* >> */
						A_Matrix3					*src_matrixP0,		/* << */
						PF_EffectWorld					*dst);				/* <> */


	SPAPI A_Err	(*AEGP_DisposeTexture)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						PF_EffectWorld					*dst0);				/* <> */

	SPAPI A_Err	(*AEGP_GetFieldRender)(
						PR_RenderContextH 			render_contextH,   	/* >> */
						PF_Field					*field);			/* << */

	// not thread safe on MacOS
	// only call when thread ID = 0
	SPAPI A_Err	(*AEGP_ReportArtisanProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							totalL);			/* >> */

	SPAPI A_Err	(*AEGP_GetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_IsBlankCanvas)(
						PR_RenderContextH			render_contextH,	/* >> */
						A_Boolean					*is_blankPB);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Matrix4						*transform);			/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerBounds)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_LegacyRect							*boundsP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderOpacity)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_FpLong						*opacityPF);		/* << */

	SPAPI A_Err (*AEGP_IsRenderLayerActive)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_Boolean						*activePB);			/* << */

	// set the layer index. If total > 0, set it too.
	SPAPI A_Err	(*AEGP_SetArtisanLayerProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							num_layersL);

} AEGP_CanvasSuite1;

/********************************************************************/

#define kAEGPRQItemSuiteVersion3		4	/* frozen in AE 7.0 */

typedef struct AEGP_RQItemSuite3 {

	SPAPI A_Err	(*AEGP_GetNumRQItems)(
						A_long				*num_itemsPL);		/* << */

	/*	NOTE: 	All AEGP_RQItemRefH are invalidated by ANY
				re-ordering, addition or removal of render
				items. DO NOT CACHE THEM.
	*/

	SPAPI A_Err (*AEGP_GetRQItemByIndex)(
						A_long				rq_item_index,		/* >> */
						AEGP_RQItemRefH		*rq_item_refPH);	/* << */

	SPAPI A_Err (*AEGP_GetNextRQItem)(							/* Pass RQ_ITEM_INDEX_NONE for current_rq_itemH to get first RQItemH. */
						AEGP_RQItemRefH		current_rq_itemH,	/* >> */
						AEGP_RQItemRefH		*next_rq_itemH);	/* << */

	SPAPI A_Err (*AEGP_GetNumOutputModulesForRQItem)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_long				*num_outmodsPL);	/* << */

	SPAPI A_Err (*AEGP_GetRenderState)(
						AEGP_RQItemRefH				rq_itemH,	/* >> */
						AEGP_RenderItemStatusType	*statusP);	/* << */

	/*
		the following now returns:
		Err_PARAMETER if you try to call while AEGP_RenderQueueState != AEGP_RenderQueueState_STOPPED

		if that's okay then:
		Err_RANGE if you pass a status that is illegal in any case
		Err_PARAMETER if you try to pass a status that doesn't make sense right now (eg: trying to Que something for which you haven't set the output path)
	*/
	SPAPI A_Err (*AEGP_SetRenderState)(
						AEGP_RQItemRefH				rq_itemH,	/* >> */
						AEGP_RenderItemStatusType	status);	/* >> */

	SPAPI A_Err (*AEGP_GetStartedTime)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_Time				*started_timePT);	/* <<	Returns {0,1} if not started. */

	SPAPI A_Err (*AEGP_GetElapsedTime)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_Time				*render_timePT);	/* << 	Returns {0,1} if not rendered. */

	SPAPI A_Err	(*AEGP_GetLogType)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						AEGP_LogType		*logtypeP);			/* << */

	SPAPI A_Err	(*AEGP_SetLogType)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						AEGP_LogType		logtype);			/* << */

	SPAPI A_Err	(*AEGP_RemoveOutputModule)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						AEGP_OutputModuleRefH	outmodH);		/* >> */

	SPAPI A_Err (*AEGP_GetComment)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						A_char					*commentZ);		/* << 		up to A_char[AEGP_MAX_RQITEM_COMMENT_SIZE] */

	SPAPI A_Err (*AEGP_SetComment)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						const A_char			*commentZ);		/* >> 		up to A_char[AEGP_MAX_RQITEM_COMMENT_SIZE] */

	SPAPI A_Err (*AEGP_GetCompFromRQItem)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						AEGP_CompH				*compPH);		/* << */

	SPAPI A_Err (*AEGP_DeleteRQItem)(
						AEGP_RQItemRefH			rq_itemH);		/* <> 	   UNDOABLE */

} AEGP_RQItemSuite3;

#define kAEGPRQItemSuiteVersion2		3	/* frozen in AE 6.5 */

typedef struct AEGP_RQItemSuite2 {

	SPAPI A_Err	(*AEGP_GetNumRQItems)(
						A_long				*num_itemsPL);		/* << */

	/*	NOTE: 	All AEGP_RQItemRefH are invalidated by ANY
				re-ordering, addition or removal of render
				items. DO NOT CACHE THEM.
	*/

	SPAPI A_Err (*AEGP_GetRQItemByIndex)(
						A_long				rq_item_index,		/* >> */
						AEGP_RQItemRefH		*rq_item_refPH);	/* << */

	SPAPI A_Err (*AEGP_GetNextRQItem)(							/* Pass NULL for current_rq_itemH to get first RQItemH. */
						AEGP_RQItemRefH		current_rq_itemH,	/* >> */
						AEGP_RQItemRefH		*next_rq_itemH);	/* << */

	SPAPI A_Err (*AEGP_GetNumOutputModulesForRQItem)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_long				*num_outmodsPL);	/* << */

	SPAPI A_Err (*AEGP_GetRenderState)(
						AEGP_RQItemRefH				rq_itemH,	/* >> */
						AEGP_RenderItemStatusType	*statusP);	/* << */

	/*
		the following now returns:
		Err_PARAMETER if you try to call while AEGP_RenderQueueState != AEGP_RenderQueueState_STOPPED

		if that's okay then:
		Err_RANGE if you pass a status that is illegal in any case
		Err_PARAMETER if you try to pass a status that doesn't make sense right now (eg: trying to Que something for which you haven't set the output path)
	*/
	SPAPI A_Err (*AEGP_SetRenderState)(
						AEGP_RQItemRefH				rq_itemH,	/* >> */
						AEGP_RenderItemStatusType	status);	/* >> */

	SPAPI A_Err (*AEGP_GetStartedTime)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_Time				*started_timePT);	/* <<	Returns {0,1} if not started. */

	SPAPI A_Err (*AEGP_GetElapsedTime)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_Time				*render_timePT);	/* << 	Returns {0,1} if not rendered. */

	SPAPI A_Err	(*AEGP_GetLogType)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						AEGP_LogType		*logtypeP);			/* << */

	SPAPI A_Err	(*AEGP_SetLogType)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						AEGP_LogType		logtype);			/* << */

	SPAPI A_Err	(*AEGP_RemoveOutputModule)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						AEGP_OutputModuleRefH	outmodH);		/* >> */

	SPAPI A_Err (*AEGP_GetComment)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						A_char					*commentZ);		/* << 		up to A_char[AEGP_MAX_RQITEM_COMMENT_SIZE] */

	SPAPI A_Err (*AEGP_SetComment)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						const A_char			*commentZ);		/* >> 		up to A_char[AEGP_MAX_RQITEM_COMMENT_SIZE] */

	SPAPI A_Err (*AEGP_GetCompFromRQItem)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						AEGP_CompH				*compPH);		/* << */

} AEGP_RQItemSuite2;

#define kAEGPRQItemSuiteVersion1		1	/* frozen in AE 6.0 */

typedef struct AEGP_RQItemSuite1 {

	SPAPI A_Err	(*AEGP_GetNumRQItems)(
						A_long				*num_itemsPL);		/* << */

	/*	NOTE: 	All AEGP_RQItemRefH are invalidated by ANY
				re-ordering, addition or removal of render
				items. DO NOT CACHE THEM.
	*/

	SPAPI A_Err (*AEGP_GetRQItemByIndex)(
						A_long				rq_item_index,		/* >> */
						AEGP_RQItemRefH		*rq_item_refPH);	/* << */

	SPAPI A_Err (*AEGP_GetNextRQItem)(							/* Pass NULL for current_rq_itemH to get first RQItemH. */
						AEGP_RQItemRefH		current_rq_itemH,	/* >> */
						AEGP_RQItemRefH		*next_rq_itemH);	/* << */

	SPAPI A_Err (*AEGP_GetNumOutputModulesForRQItem)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_long				*num_outmodsPL);	/* << */

	SPAPI A_Err (*AEGP_GetRenderState)(
						AEGP_RQItemRefH				rq_itemH,	/* >> */
						AEGP_RenderItemStatusType	*statusP);	/* << */

	SPAPI A_Err (*AEGP_SetRenderState)(							/* Will return error if called while (AEGP_RenderQueueState != AEGP_RenderQueueState_STOPPED). */
						AEGP_RQItemRefH				rq_itemH,	/* >> */
						AEGP_RenderItemStatusType	status);	/* >> */

	SPAPI A_Err (*AEGP_GetStartedTime)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_Time				*started_timePT);	/* <<	Returns {0,0} if not started. */

	SPAPI A_Err (*AEGP_GetElapsedTime)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						A_Time				*render_timePT);	/* << 	Returns {0,0} if not rendered. */

	SPAPI A_Err	(*AEGP_GetLogType)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						AEGP_LogType		*logtypeP);			/* << */

	SPAPI A_Err	(*AEGP_SetLogType)(
						AEGP_RQItemRefH		rq_itemH,			/* >> */
						AEGP_LogType		logtype);			/* << */

	SPAPI A_Err	(*AEGP_RemoveOutputModule)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						AEGP_OutputModuleRefH	outmodH);		/* >> */

	SPAPI A_Err (*AEGP_GetComment)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						A_char					*commentZ);		/* << 		up to A_char[AEGP_MAX_RQITEM_COMMENT_SIZE] */

	SPAPI A_Err (*AEGP_SetComment)(
						AEGP_RQItemRefH			rq_itemH,		/* >> */
						const A_char			*commentZ);		/* >> 		up to A_char[AEGP_MAX_RQITEM_COMMENT_SIZE] */

} AEGP_RQItemSuite1;



#define kAEGPCanvasSuiteVersion2		6 /* frozen in AE 5.5 */

typedef struct AEGP_CanvasSuite2 {

	SPAPI A_Err	(*AEGP_GetCompToRender)(
						PR_RenderContextH 		render_contextH,   	/* >> */
						AEGP_CompH				*compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumLayersToRender)(
				const	PR_RenderContextH	render_contextH,
						A_long				*num_to_renderPL);


	SPAPI A_Err	(*AEGP_GetNthLayerContextToRender)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n,					/* >> */
						AEGP_RenderLayerContextH	*layer_contextPH);	/* << */

	SPAPI A_Err	(*AEGP_GetLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,
						AEGP_RenderLayerContextH	layer_contextH,
						AEGP_LayerH					*layerPH);

	/**
	 ** With collapsed geometrics "on" this gives the layer in the root comp
	 ** contining the layer context. With collapsed geometrics off
	 ** this is the same as AEGP_GetLayerFromLayerContext.
	 **
	 **/
	SPAPI A_Err	(*AEGP_GetTopLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,
						AEGP_RenderLayerContextH	layer_contextH,
						AEGP_LayerH					*layerPH);

	SPAPI A_Err	(*AEGP_GetCompRenderTime)(
						PR_RenderContextH	render_contextH,	/* >> */
						A_Time				*time,				/* << */
						A_Time				*time_step);

	SPAPI A_Err	(*AEGP_GetCompDestinationBuffer)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_CompH					compH,				/* >> */
						PF_EffectWorld					*dst);				/* <> */

	SPAPI A_Err	(*AEGP_GetROI)(
						PR_RenderContextH			render_contextH,	/* <> */
						A_LegacyRect						*roiPR);			/* << */

	/// for rendering track mattes
	SPAPI A_Err	(*AEGP_RenderLayer)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH					layerH,				/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						PF_EffectWorld					*render_bufferP);	/* >> */

	// for rendering the texture map of a layer
	SPAPI A_Err	(*AEGP_RenderTexture)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						A_FloatPoint				*suggested_scaleP0,	/* >> */
						A_FloatRect					*suggested_src_rectP0, /* >> */
						A_Matrix3					*src_matrixP0,		/* << */
						PF_EffectWorld					*dst);				/* <> */


	SPAPI A_Err	(*AEGP_DisposeTexture)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						PF_EffectWorld					*dst0);				/* <> */

	SPAPI A_Err	(*AEGP_GetFieldRender)(
						PR_RenderContextH 			render_contextH,   	/* >> */
						PF_Field					*field);			/* << */

	// not thread safe on MacOS
	// only call when thread ID = 0
	SPAPI A_Err	(*AEGP_ReportArtisanProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							totalL);			/* >> */

	SPAPI A_Err	(*AEGP_GetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_IsBlankCanvas)(
						PR_RenderContextH			render_contextH,	/* >> */
						A_Boolean					*is_blankPB);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Matrix4						*transform);			/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerBounds)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_LegacyRect							*boundsP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderOpacity)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_FpLong						*opacityPF);		/* << */

	SPAPI A_Err (*AEGP_IsRenderLayerActive)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_Boolean						*activePB);			/* << */

	// set the layer index. If total > 0, set it too.
	SPAPI A_Err	(*AEGP_SetArtisanLayerProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							num_layersL);

	SPAPI A_Err	(*AEGP_RenderLayerPlus)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						PF_EffectWorld					*render_bufferP);	/* << */


	SPAPI A_Err	(*AEGP_GetTrackMatteContext)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH 	fill_contextH,		/* << */
						AEGP_RenderLayerContextH	*matte_contextPH);	/* >> */

} AEGP_CanvasSuite2;



#define kAEGPCanvasSuiteVersion4		9	/* frozen AE 6.0 */

typedef struct AEGP_CanvasSuite4 {

	SPAPI A_Err	(*AEGP_GetCompToRender)(
						PR_RenderContextH 		render_contextH,   	/* >> */
						AEGP_CompH				*compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumLayersToRender)(
				const	PR_RenderContextH	render_contextH,		/* >> */
						A_long				*num_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_GetNthLayerContextToRender)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n,					/* >> */
						AEGP_RenderLayerContextH	*layer_contextPH);	/* << */

	SPAPI A_Err	(*AEGP_GetLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_LayerH					*layerPH);			/* << */

	/**
	 ** With collapsed geometrics "on" this gives the layer in the root comp
	 ** contining the layer context. With collapsed geometrics off
	 ** this is the same as AEGP_GetLayerFromLayerContext.
	 **
	 **/
	SPAPI A_Err	(*AEGP_GetTopLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,
						AEGP_RenderLayerContextH	layer_contextH,
						AEGP_LayerH					*layerPH);

	SPAPI A_Err	(*AEGP_GetCompRenderTime)(
						PR_RenderContextH	render_contextH,	/* >> */
						A_Time				*time,				/* << */
						A_Time				*time_step);

	SPAPI A_Err	(*AEGP_GetCompDestinationBuffer)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_CompH					compH,				/* >> */
						AEGP_WorldH					*dst);				/* << */

	SPAPI A_Err	(*AEGP_GetROI)(
						PR_RenderContextH			render_contextH,	/* <> */
						A_LegacyRect						*roiPR);			/* << */

	// for rendering the texture map of a layer
	SPAPI A_Err	(*AEGP_RenderTexture)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						AEGP_RenderHints			render_hints,			/* >> */
						A_FloatPoint				*suggested_scaleP0,		/* >> */
						A_FloatRect					*suggested_src_rectP0, 	/* >> */
						A_Matrix3					*src_matrixP0,			/* << */
						AEGP_WorldH					*dstPH);				/* <> */


	SPAPI A_Err	(*AEGP_DisposeTexture)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_WorldH					dstH0);				/* <> */

	SPAPI A_Err	(*AEGP_GetFieldRender)(
						PR_RenderContextH 			render_contextH,   	/* >> */
						PF_Field					*field);			/* << */

	// not thread safe on MacOS
	// only call when thread ID = 0
	SPAPI A_Err	(*AEGP_ReportArtisanProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							totalL);			/* >> */

	SPAPI A_Err	(*AEGP_GetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* >> */

	SPAPI A_Err	(*AEGP_IsBlankCanvas)(
						PR_RenderContextH			render_contextH,	/* >> */
						A_Boolean					*is_blankPB);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Matrix4						*transform);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerBounds)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_LegacyRect							*boundsP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderOpacity)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_FpLong						*opacityPF);		/* << */

	SPAPI A_Err (*AEGP_IsRenderLayerActive)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_Boolean						*activePB);			/* << */

	// set the layer index. If total > 0, set it too.
	SPAPI A_Err	(*AEGP_SetArtisanLayerProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							num_layersL);

	// for track mattes.
	// Returns a comp-size buffer, which must be disposed thru AEGP_Dispose in World suite
	SPAPI A_Err	(*AEGP_RenderLayerPlus)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						AEGP_WorldH					*render_bufferPH);	/* <<  must be disposed with AEGP_DisposeWorld */


	SPAPI A_Err	(*AEGP_GetTrackMatteContext)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH 	fill_contextH,		/* << */
						AEGP_RenderLayerContextH	*matte_contextPH);	/* >> */

	// new for 6.0 --get receipt with the returned texture
	// use receipt to determine if a subsequent call to render
	// this layer can be skipped (because the artisan cached it)
	SPAPI A_Err	(*AEGP_RenderTextureWithReceipt)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						AEGP_RenderHints			render_hints,			/* >> */
						A_FloatPoint				*suggested_scaleP0,		/* >> */
						A_FloatRect					*suggested_src_rectP0, 	/* >> */
						A_Matrix3					*src_matrixP0,			/* << */
						AEGP_RenderReceiptH			*render_receiptPH, 	 	/* << must be disposed with AEGP_DisposeRenderReceipt */
						AEGP_WorldH					*dstPH);				/* << */

	SPAPI A_Err	(*AEGP_RenderLayerPlusWithReceipt)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						AEGP_RenderReceiptH			*render_receiptPH,  /* << must be disposed with AEGP_DisposeRenderReceipt */
						AEGP_WorldH					*render_bufferPH);	/* << */

	SPAPI A_Err	(*AEGP_DisposeRenderReceipt)(
						AEGP_RenderReceiptH			render_receiptH);	/* >> */


	SPAPI A_Err	(*AEGP_CheckRenderReceipt)(
						PR_RenderContextH			current_render_contextH,	/* >> */
						AEGP_RenderLayerContextH	current_layer_contextH,		/* >> */
						AEGP_RenderReceiptH			old_render_receiptH,		/* >> */
						A_Boolean					check_aceB,					/* >> */
						AEGP_RenderReceiptStatus	*receipt_statusP);			/* << */


	SPAPI A_Err	(*AEGP_GetNumBinsToRender)(
				const	PR_RenderContextH	render_contextH,			/* >> */
						A_long				*num_bins_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_SetNthBin)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n);					/* >> */

	SPAPI A_Err	(*AEGP_GetBinType)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_BinType				*bin_typeP);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform2D3D)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Boolean						only_2dB,			/* >> */
					A_Matrix4						*transform);		/* << */


	// interactive artisan information
	// handle to the on-screen window
	SPAPI A_Err (*AEGP_GetPlatformWindowRef)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_PlatformWindowRef				*window_refP);		/* << */


	// the dsf src to frame scale factors
	SPAPI A_Err (*AEGP_GetViewportScale)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_FpLong							*scale_xPF,			/* << */
				A_FpLong							*scale_yPF);		/* << */


	// the dsf src to frame translate
	SPAPI A_Err (*AEGP_GetViewportOrigin)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_long								*origin_xPL,		/* << */
				A_long								*origin_yPL);		/* << */


	SPAPI A_Err (*AEGP_GetViewportRect)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_LegacyRect								*viewport_rectPR);	/* << */


	SPAPI A_Err (*AEGP_GetFallowColor)(
				const PR_RenderContextH				render_contextH,	/* >> */
				PF_Pixel8							*fallow_colorP);	/* << */

	SPAPI A_Err (*AEGP_GetInteractiveBuffer)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_WorldH							*buffer);			/* << */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboard)(
				const PR_RenderContextH				render_contextH,	/* in */
				A_Boolean							*checkerboard_onPB);/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardColors)(
				const PR_RenderContextH				render_contextH,			/* in */
				PF_Pixel							*checkerboard_color1P,		/* out */
				PF_Pixel							*checkerboard_color2P);		/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardSize)(
				const PR_RenderContextH				render_contextH,			/* in */
				A_u_long							*checkerboard_widthPLu,		/* out - width of square*/
				A_u_long							*checkerboard_heightPLu);	/* out - height of square*/

	SPAPI A_Err (*AEGP_GetInteractiveCachedBuffer)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_WorldH							*buffer);			/* << */


	// should we call AEGP_RenderLayer or AEGP_RenderTexture
	SPAPI A_Err (*AEGP_ArtisanMustRenderAsLayer)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_RenderLayerContextH			layer_contextH,
				A_Boolean							*use_render_texturePB);

} AEGP_CanvasSuite4;



#define kAEGPCanvasSuiteVersion5		10	/* frozen in AE 6.5 */

typedef struct AEGP_CanvasSuite5 {

	SPAPI A_Err	(*AEGP_GetCompToRender)(
						PR_RenderContextH 		render_contextH,   	/* >> */
						AEGP_CompH				*compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumLayersToRender)(
				const	PR_RenderContextH	render_contextH,		/* >> */
						A_long				*num_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_GetNthLayerContextToRender)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n,					/* >> */
						AEGP_RenderLayerContextH	*layer_contextPH);	/* << */

	SPAPI A_Err	(*AEGP_GetLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_LayerH					*layerPH);			/* << */

	/**
	 ** With collapsed geometrics "on" this gives the layer in the root comp
	 ** contining the layer context. With collapsed geometrics off
	 ** this is the same as AEGP_GetLayerFromLayerContext.
	 **
	 **/
	SPAPI A_Err	(*AEGP_GetTopLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,
						AEGP_RenderLayerContextH	layer_contextH,
						AEGP_LayerH					*layerPH);

	SPAPI A_Err	(*AEGP_GetCompRenderTime)(
						PR_RenderContextH	render_contextH,	/* >> */
						A_Time				*time,				/* << */
						A_Time				*time_step);

	SPAPI A_Err	(*AEGP_GetCompDestinationBuffer)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_CompH					compH,				/* >> */
						AEGP_WorldH					*dst);				/* << */

	SPAPI A_Err	(*AEGP_GetROI)(
						PR_RenderContextH			render_contextH,	/* <> */
						A_LegacyRect						*roiPR);			/* << */

	// for rendering the texture map of a layer
	SPAPI A_Err	(*AEGP_RenderTexture)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						AEGP_RenderHints			render_hints,			/* >> */
						A_FloatPoint				*suggested_scaleP0,		/* >> */
						A_FloatRect					*suggested_src_rectP0, 	/* >> */
						A_Matrix3					*src_matrixP0,			/* << */
						AEGP_WorldH					*dstPH);				/* <> */


	SPAPI A_Err	(*AEGP_DisposeTexture)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_WorldH					dstH0);				/* <> */

	SPAPI A_Err	(*AEGP_GetFieldRender)(
						PR_RenderContextH 			render_contextH,   	/* >> */
						PF_Field					*field);			/* << */

	// not thread safe on MacOS
	// only call when thread ID = 0
	SPAPI A_Err	(*AEGP_ReportArtisanProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							totalL);			/* >> */

	SPAPI A_Err	(*AEGP_GetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* >> */

	SPAPI A_Err	(*AEGP_IsBlankCanvas)(
						PR_RenderContextH			render_contextH,	/* >> */
						A_Boolean					*is_blankPB);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Matrix4						*transform);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerBounds)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_LegacyRect							*boundsP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderOpacity)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_FpLong						*opacityPF);		/* << */

	SPAPI A_Err (*AEGP_IsRenderLayerActive)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_Boolean						*activePB);			/* << */

	// set the layer index. If total > 0, set it too.
	SPAPI A_Err	(*AEGP_SetArtisanLayerProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							num_layersL);

	// for track mattes.
	// Returns a comp-size buffer, which must be disposed thru AEGP_Dispose in World suite
	SPAPI A_Err	(*AEGP_RenderLayerPlus)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						AEGP_WorldH					*render_bufferPH);	/* <<  must be disposed with AEGP_DisposeWorld */


	SPAPI A_Err	(*AEGP_GetTrackMatteContext)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH 	fill_contextH,		/* << */
						AEGP_RenderLayerContextH	*matte_contextPH);	/* >> */

	// new for 6.0 --get receipt with the returned texture
	// use receipt to determine if a subsequent call to render
	// this layer can be skipped (because the artisan cached it)
	SPAPI A_Err	(*AEGP_RenderTextureWithReceipt)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						AEGP_RenderHints			render_hints,			/* >> */
						A_FloatPoint				*suggested_scaleP0,		/* >> */
						A_FloatRect					*suggested_src_rectP0, 	/* >> */
						A_Matrix3					*src_matrixP0,			/* << */
						AEGP_RenderReceiptH			*render_receiptPH, 	 	/* << must be disposed with AEGP_DisposeRenderReceipt */
						AEGP_WorldH					*dstPH);				/* << */

	SPAPI A_Err	(*AEGP_RenderLayerPlusWithReceipt)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						AEGP_RenderReceiptH			*render_receiptPH,  /* << must be disposed with AEGP_DisposeRenderReceipt */
						AEGP_WorldH					*render_bufferPH);	/* << */

	SPAPI A_Err	(*AEGP_DisposeRenderReceipt)(
						AEGP_RenderReceiptH			render_receiptH);	/* >> */


	SPAPI A_Err	(*AEGP_CheckRenderReceipt)(
						PR_RenderContextH			current_render_contextH,	/* >> */
						AEGP_RenderLayerContextH	current_layer_contextH,		/* >> */
						AEGP_RenderReceiptH			old_render_receiptH,		/* >> */
						A_Boolean					check_geometricsB,					/* >> */
						AEGP_RenderReceiptStatus	*receipt_statusP);			/* << */


	SPAPI A_Err	(*AEGP_GetNumBinsToRender)(
				const	PR_RenderContextH	render_contextH,			/* >> */
						A_long				*num_bins_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_SetNthBin)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n);					/* >> */

	SPAPI A_Err	(*AEGP_GetBinType)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_BinType				*bin_typeP);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform2D3D)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Boolean						only_2dB,			/* >> */
					A_Matrix4						*transform);		/* << */


	// interactive artisan information
	// handle to the on-screen window
	SPAPI A_Err (*AEGP_GetPlatformWindowRef)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_PlatformWindowRef				*window_refP);		/* << */


	// the dsf src to frame scale factors
	SPAPI A_Err (*AEGP_GetViewportScale)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_FpLong							*scale_xPF,			/* << */
				A_FpLong							*scale_yPF);		/* << */


	// the dsf src to frame translate
	SPAPI A_Err (*AEGP_GetViewportOrigin)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_long								*origin_xPL,		/* << */
				A_long								*origin_yPL);		/* << */


	SPAPI A_Err (*AEGP_GetViewportRect)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_LegacyRect								*viewport_rectPR);	/* << */


	SPAPI A_Err (*AEGP_GetFallowColor)(
				const PR_RenderContextH				render_contextH,	/* >> */
				PF_Pixel8							*fallow_colorP);	/* << */

	SPAPI A_Err (*AEGP_GetInteractiveBuffer)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_WorldH							*buffer);			/* << */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboard)(
				const PR_RenderContextH				render_contextH,	/* in */
				A_Boolean							*checkerboard_onPB);/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardColors)(
				const PR_RenderContextH				render_contextH,			/* in */
				PF_Pixel							*checkerboard_color1P,		/* out */
				PF_Pixel							*checkerboard_color2P);		/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardSize)(
				const PR_RenderContextH				render_contextH,			/* in */
				A_u_long							*checkerboard_widthPLu,		/* out - width of square*/
				A_u_long							*checkerboard_heightPLu);	/* out - height of square*/

	SPAPI A_Err (*AEGP_GetInteractiveCachedBuffer)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_WorldH							*buffer);			/* << */


	// should we call AEGP_RenderLayer or AEGP_RenderTexture
	SPAPI A_Err (*AEGP_ArtisanMustRenderAsLayer)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_RenderLayerContextH			layer_contextH,
				A_Boolean							*use_render_texturePB);


	SPAPI A_Err (*AEGP_GetInteractiveDisplayChannel)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_DisplayChannelType				*display_channelP);			/* << */

} AEGP_CanvasSuite5;


#define kAEGPCanvasSuiteVersion6		11	/* frozen in AE 7.0 */

typedef struct AEGP_CanvasSuite6 {

	SPAPI A_Err	(*AEGP_GetCompToRender)(
		PR_RenderContextH 		render_contextH,   	/* >> */
		AEGP_CompH				*compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumLayersToRender)(
		const	PR_RenderContextH	render_contextH,		/* >> */
		A_long				*num_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_GetNthLayerContextToRender)(
		const	PR_RenderContextH			render_contextH,	/* >> */
		A_long						n,					/* >> */
		AEGP_RenderLayerContextH	*layer_contextPH);	/* << */

	SPAPI A_Err	(*AEGP_GetLayerFromLayerContext)(
		const	PR_RenderContextH			render_contextH,	/* >> */
		AEGP_RenderLayerContextH	layer_contextH,		/* >> */
		AEGP_LayerH					*layerPH);			/* << */

	/**
	** With collapsed geometrics "on" this gives the layer in the root comp
	** contining the layer context. With collapsed geometrics off
	** this is the same as AEGP_GetLayerFromLayerContext.
	**
	**/
	SPAPI A_Err	(*AEGP_GetTopLayerFromLayerContext)(
		const	PR_RenderContextH			render_contextH,
		AEGP_RenderLayerContextH	layer_contextH,
		AEGP_LayerH					*layerPH);

	SPAPI A_Err	(*AEGP_GetCompRenderTime)(
		PR_RenderContextH	render_contextH,	/* >> */
		A_Time				*time,				/* << */
		A_Time				*time_step);

	SPAPI A_Err	(*AEGP_GetCompDestinationBuffer)(
		PR_RenderContextH			render_contextH,	/* <> */
		AEGP_CompH					compH,				/* >> */
		AEGP_WorldH					*dst);				/* << */

	SPAPI A_Err	(*AEGP_GetROI)(
		PR_RenderContextH			render_contextH,	/* <> */
		A_LegacyRect						*roiPR);			/* << */

	// for rendering the texture map of a layer
	SPAPI A_Err	(*AEGP_RenderTexture)(
		PR_RenderContextH			render_contextH,		/* <> */
		AEGP_RenderLayerContextH	layer_contextH,			/* >> */
		AEGP_RenderHints			render_hints,			/* >> */
		A_FloatPoint				*suggested_scaleP0,		/* >> */
		A_FloatRect					*suggested_src_rectP0, 	/* >> */
		A_Matrix3					*src_matrixP0,			/* << */
		AEGP_WorldH					*dstPH);				/* <> */


	SPAPI A_Err	(*AEGP_DisposeTexture)(
		PR_RenderContextH			render_contextH,	/* <> */
		AEGP_RenderLayerContextH	layer_contextH,		/* >> */
		AEGP_WorldH					dstH0);				/* <> */

	SPAPI A_Err	(*AEGP_GetFieldRender)(
		PR_RenderContextH 			render_contextH,   	/* >> */
		PF_Field					*field);			/* << */

	// not thread safe on MacOS
	// only call when thread ID = 0
	SPAPI A_Err	(*AEGP_ReportArtisanProgress)(
		PR_RenderContextH 				render_contextH,   	/* >> */
		A_long							countL,				/* >> */
		A_long							totalL);			/* >> */

	SPAPI A_Err	(*AEGP_GetRenderDownsampleFactor)(
		PR_RenderContextH			render_contextH,	/* >> */
		AEGP_DownsampleFactor		*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetRenderDownsampleFactor)(
		PR_RenderContextH			render_contextH,	/* >> */
		AEGP_DownsampleFactor		*dsfP);				/* >> */

	SPAPI A_Err	(*AEGP_IsBlankCanvas)(
		PR_RenderContextH			render_contextH,	/* >> */
		A_Boolean					*is_blankPB);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform)(
		PR_RenderContextH				render_contextH,	/* >> */
		AEGP_RenderLayerContextH		layer_contextH,		/* >> */
		const A_Time					*comp_timeP,		/* >> */
		A_Matrix4						*transform);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerBounds)(
		PR_RenderContextH				render_contextH,	/* >> */
		AEGP_RenderLayerContextH		layer_contextH,		/* >> */
		const A_Time					*comp_timeP,		/* >> */
		A_LegacyRect							*boundsP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderOpacity)(
		PR_RenderContextH				render_contextH,	/* >> */
		AEGP_RenderLayerContextH		layer_contextH,		/* >> */
		const A_Time					*comp_timePT,		/* >> */
		A_FpLong						*opacityPF);		/* << */

	SPAPI A_Err (*AEGP_IsRenderLayerActive)(
		PR_RenderContextH				render_contextH,	/* >> */
		AEGP_RenderLayerContextH		layer_contextH,		/* >> */
		const A_Time					*comp_timePT,		/* >> */
		A_Boolean						*activePB);			/* << */

	// set the layer index. If total > 0, set it too.
	SPAPI A_Err	(*AEGP_SetArtisanLayerProgress)(
		PR_RenderContextH 				render_contextH,   	/* >> */
		A_long							countL,				/* >> */
		A_long							num_layersL);

	// for track mattes.
	// Returns a comp-size buffer, which must be disposed thru AEGP_Dispose in World suite
	SPAPI A_Err	(*AEGP_RenderLayerPlus)(
		PR_RenderContextH			render_contextH,	/* <> */
		AEGP_LayerH 				layerH,				/* >> */
		AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
		AEGP_RenderHints			render_hints,		/* >> */
		AEGP_WorldH					*render_bufferPH);	/* <<  must be disposed with AEGP_DisposeWorld */


	SPAPI A_Err	(*AEGP_GetTrackMatteContext)(
		PR_RenderContextH			render_contextH,	/* <> */
		AEGP_RenderLayerContextH 	fill_contextH,		/* << */
		AEGP_RenderLayerContextH	*matte_contextPH);	/* >> */

	// new for 6.0 --get receipt with the returned texture
	// use receipt to determine if a subsequent call to render
	// this layer can be skipped (because the artisan cached it)
	SPAPI A_Err	(*AEGP_RenderTextureWithReceipt)(
		PR_RenderContextH			render_contextH,		/* <> */
		AEGP_RenderLayerContextH	layer_contextH,			/* >> */
		AEGP_RenderHints			render_hints,			/* >> */
		AEGP_NumEffectsToRenderType	num_effectsS,			/* >>  number of effect to render, -1 for all */
		A_FloatPoint				*suggested_scaleP0,		/* >> */
		A_FloatRect					*suggested_src_rectP0, 	/* >> */
		A_Matrix3					*src_matrixP0,			/* << */
		AEGP_RenderReceiptH			*render_receiptPH, 	 	/* << must be disposed with AEGP_DisposeRenderReceipt */
		AEGP_WorldH					*dstPH);				/* << */



	SPAPI A_Err	(*AEGP_GetNumberOfSoftwareEffects)(
		PR_RenderContextH			render_contextH,		/* <> */
		AEGP_RenderLayerContextH	layer_contextH,			/* >> */
		A_short						*num_software_effectsPS);

	SPAPI A_Err	(*AEGP_RenderLayerPlusWithReceipt)(
		PR_RenderContextH			render_contextH,	/* <> */
		AEGP_LayerH 				layerH,				/* >> */
		AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
		AEGP_RenderHints			render_hints,		/* >> */
		AEGP_RenderReceiptH			*render_receiptPH,  /* << must be disposed with AEGP_DisposeRenderReceipt */
		AEGP_WorldH					*render_bufferPH);	/* << */

	SPAPI A_Err	(*AEGP_DisposeRenderReceipt)(
		AEGP_RenderReceiptH			render_receiptH);	/* >> */


	/* modified for 7.0 - added num_effects to check against */
	SPAPI A_Err	(*AEGP_CheckRenderReceipt)(
		PR_RenderContextH			current_render_contextH,	/* in */
		AEGP_RenderLayerContextH	current_layer_contextH,		/* in */
		AEGP_RenderReceiptH			old_render_receiptH,		/* in */
		A_Boolean					check_geometricsB,			/* in */
		AEGP_NumEffectsToRenderType	num_effectsS,				/* in */
		AEGP_RenderReceiptStatus	*receipt_statusP);			/* out */


	/* new in 7.0 generate a receipt for a layer as asd if the first num_effectsS have been rendered */
	SPAPI A_Err	(*AEGP_GenerateRenderReceipt)(
		PR_RenderContextH			current_render_contextH,	/* >> */
		AEGP_RenderLayerContextH	current_layer_contextH,		/* >> */
		AEGP_NumEffectsToRenderType	num_effectsS,				/* in */
		AEGP_RenderReceiptH			*render_receiptPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumBinsToRender)(
		const	PR_RenderContextH	render_contextH,			/* >> */
		A_long				*num_bins_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_SetNthBin)(
		const	PR_RenderContextH			render_contextH,	/* >> */
		A_long						n);					/* >> */

	SPAPI A_Err	(*AEGP_GetBinType)(
		const	PR_RenderContextH			render_contextH,	/* >> */
		AEGP_BinType				*bin_typeP);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform2D3D)(
		PR_RenderContextH				render_contextH,	/* >> */
		AEGP_RenderLayerContextH		layer_contextH,		/* >> */
		const A_Time					*comp_timeP,		/* >> */
		A_Boolean						only_2dB,			/* >> */
		A_Matrix4						*transform);		/* << */


	// interactive artisan information
	// handle to the on-screen window
	SPAPI A_Err (*AEGP_GetPlatformWindowRef)(
		const PR_RenderContextH				render_contextH,	/* >> */
		AEGP_PlatformWindowRef				*window_refP);		/* << */


	// the dsf src to frame scale factors
	SPAPI A_Err (*AEGP_GetViewportScale)(
		const PR_RenderContextH				render_contextH,	/* >> */
		A_FpLong							*scale_xPF,			/* << */
		A_FpLong							*scale_yPF);		/* << */


	// the dsf src to frame translate
	SPAPI A_Err (*AEGP_GetViewportOrigin)(
		const PR_RenderContextH				render_contextH,	/* >> */
		A_long								*origin_xPL,		/* << */
		A_long								*origin_yPL);		/* << */


	SPAPI A_Err (*AEGP_GetViewportRect)(
		const PR_RenderContextH				render_contextH,	/* >> */
		A_LegacyRect								*viewport_rectPR);	/* << */


	SPAPI A_Err (*AEGP_GetFallowColor)(
		const PR_RenderContextH				render_contextH,	/* >> */
		PF_Pixel8							*fallow_colorP);	/* << */

	SPAPI A_Err (*AEGP_GetInteractiveBuffer)(
		const PR_RenderContextH				render_contextH,	/* >> */
		AEGP_WorldH							*buffer);			/* << */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboard)(
		const PR_RenderContextH				render_contextH,	/* in */
		A_Boolean							*checkerboard_onPB);/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardColors)(
		const PR_RenderContextH				render_contextH,			/* in */
		PF_Pixel							*checkerboard_color1P,		/* out */
		PF_Pixel							*checkerboard_color2P);		/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardSize)(
		const PR_RenderContextH				render_contextH,			/* in */
		A_u_long							*checkerboard_widthPLu,		/* out - width of square*/
		A_u_long							*checkerboard_heightPLu);	/* out - height of square*/

	SPAPI A_Err (*AEGP_GetInteractiveCachedBuffer)(
		const PR_RenderContextH				render_contextH,	/* >> */
		AEGP_WorldH							*buffer);			/* << */


	// should we call AEGP_RenderLayer or AEGP_RenderTexture
	SPAPI A_Err (*AEGP_ArtisanMustRenderAsLayer)(
		const PR_RenderContextH				render_contextH,	/* >> */
		AEGP_RenderLayerContextH			layer_contextH,
		A_Boolean							*use_render_texturePB);


	SPAPI A_Err (*AEGP_GetInteractiveDisplayChannel)(
		const PR_RenderContextH				render_contextH,	/* >> */
		AEGP_DisplayChannelType				*display_channelP);			/* << */


} AEGP_CanvasSuite6;



#define kAEGPCanvasSuiteVersion7		13	/* frozen in AE 8.0, source code-compatible additions in 64-bit AE 10.0*/

typedef struct AEGP_CanvasSuite7 {

	SPAPI A_Err	(*AEGP_GetCompToRender)(
						PR_RenderContextH 		render_contextH,   	/* >> */
						AEGP_CompH				*compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumLayersToRender)(
				const	PR_RenderContextH	render_contextH,		/* >> */
						A_long				*num_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_GetNthLayerContextToRender)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n,					/* >> */
						AEGP_RenderLayerContextH	*layer_contextPH);	/* << */

	SPAPI A_Err	(*AEGP_GetLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_LayerH					*layerPH);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerAndSubLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_LayerH					*layerPH,			/* << */
						AEGP_SubLayerIndex			*sublayerP);		/* << */

	/**
	 ** With collapsed geometrics "on" this gives the layer in the root comp
	 ** contining the layer context. With collapsed geometrics off
	 ** this is the same as AEGP_GetLayerFromLayerContext.
	 **
	 **/
	SPAPI A_Err	(*AEGP_GetTopLayerFromLayerContext)(
				const	PR_RenderContextH			render_contextH,
						AEGP_RenderLayerContextH	layer_contextH,
						AEGP_LayerH					*layerPH);

	SPAPI A_Err	(*AEGP_GetCompRenderTime)(
						PR_RenderContextH	render_contextH,	/* >> */
						A_Time				*time,				/* << */
						A_Time				*time_step);

	SPAPI A_Err	(*AEGP_GetCompDestinationBuffer)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_CompH					compH,				/* >> */
						AEGP_WorldH					*dst);				/* << */

	SPAPI A_Err	(*AEGP_GetROI)(
						PR_RenderContextH			render_contextH,	/* <> */
						A_LegacyRect						*roiPR);			/* << */

	// for rendering the texture map of a layer
	SPAPI A_Err	(*AEGP_RenderTexture)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						AEGP_RenderHints			render_hints,			/* >> */
						A_FloatPoint				*suggested_scaleP0,		/* >> */
						A_FloatRect					*suggested_src_rectP0, 	/* >> */
						A_Matrix3					*src_matrixP0,			/* << */
						AEGP_WorldH					*dstPH);				/* <> */


	SPAPI A_Err	(*AEGP_DisposeTexture)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH	layer_contextH,		/* >> */
						AEGP_WorldH					dstH0);				/* <> */

	SPAPI A_Err	(*AEGP_GetFieldRender)(
						PR_RenderContextH 			render_contextH,   	/* >> */
						PF_Field					*field);			/* << */

	// not thread safe on MacOS
	// only call when thread ID = 0
	SPAPI A_Err	(*AEGP_ReportArtisanProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							totalL);			/* >> */

	SPAPI A_Err	(*AEGP_GetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetRenderDownsampleFactor)(
						PR_RenderContextH			render_contextH,	/* >> */
						AEGP_DownsampleFactor		*dsfP);				/* >> */

	SPAPI A_Err	(*AEGP_IsBlankCanvas)(
						PR_RenderContextH			render_contextH,	/* >> */
						A_Boolean					*is_blankPB);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Matrix4						*transform);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerBounds)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_LegacyRect							*boundsP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderOpacity)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_FpLong						*opacityPF);		/* << */

	SPAPI A_Err (*AEGP_IsRenderLayerActive)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timePT,		/* >> */
					A_Boolean						*activePB);			/* << */

	// set the layer index. If total > 0, set it too.
	SPAPI A_Err	(*AEGP_SetArtisanLayerProgress)(
					PR_RenderContextH 				render_contextH,   	/* >> */
					A_long							countL,				/* >> */
					A_long							num_layersL);

	// for track mattes.
	// Returns a comp-size buffer, which must be disposed thru AEGP_Dispose in World suite
	SPAPI A_Err	(*AEGP_RenderLayerPlus)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						AEGP_WorldH					*render_bufferPH);	/* <<  must be disposed with AEGP_DisposeWorld */


	SPAPI A_Err	(*AEGP_GetTrackMatteContext)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_RenderLayerContextH 	fill_contextH,		/* << */
						AEGP_RenderLayerContextH	*matte_contextPH);	/* >> */

	// new for 6.0 --get receipt with the returned texture
	// use receipt to determine if a subsequent call to render
	// this layer can be skipped (because the artisan cached it)
	SPAPI A_Err	(*AEGP_RenderTextureWithReceipt)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						AEGP_RenderHints			render_hints,			/* >> */
						AEGP_NumEffectsToRenderType	num_effectsS,			/* >>  number of effect to render, -1 for all */
						A_FloatPoint				*suggested_scaleP0,		/* >> */
						A_FloatRect					*suggested_src_rectP0, 	/* >> */
						A_Matrix3					*src_matrixP0,			/* << */
						AEGP_RenderReceiptH			*render_receiptPH, 	 	/* << must be disposed with AEGP_DisposeRenderReceipt */
						AEGP_WorldH					*dstPH);				/* << */



	SPAPI A_Err	(*AEGP_GetNumberOfSoftwareEffects)(
						PR_RenderContextH			render_contextH,		/* <> */
						AEGP_RenderLayerContextH	layer_contextH,			/* >> */
						A_short						*num_software_effectsPS);

	SPAPI A_Err	(*AEGP_RenderLayerPlusWithReceipt)(
						PR_RenderContextH			render_contextH,	/* <> */
						AEGP_LayerH 				layerH,				/* >> */
						AEGP_RenderLayerContextH 	layer_contextH,		/* >> */
						AEGP_RenderHints			render_hints,		/* >> */
						AEGP_RenderReceiptH			*render_receiptPH,  /* << must be disposed with AEGP_DisposeRenderReceipt */
						AEGP_WorldH					*render_bufferPH);	/* << */

	SPAPI A_Err	(*AEGP_DisposeRenderReceipt)(
						AEGP_RenderReceiptH			render_receiptH);	/* >> */


	/* modified for 7.0 - added num_effects to check against */
	SPAPI A_Err	(*AEGP_CheckRenderReceipt)(
						PR_RenderContextH			current_render_contextH,	/* in */
						AEGP_RenderLayerContextH	current_layer_contextH,		/* in */
						AEGP_RenderReceiptH			old_render_receiptH,		/* in */
						A_Boolean					check_geometricsB,			/* in */
						AEGP_NumEffectsToRenderType	num_effectsS,				/* in */
						AEGP_RenderReceiptStatus	*receipt_statusP);			/* out */


	/* new in 7.0 generate a receipt for a layer as asd if the first num_effectsS have been rendered */
	SPAPI A_Err	(*AEGP_GenerateRenderReceipt)(
						PR_RenderContextH			current_render_contextH,	/* >> */
						AEGP_RenderLayerContextH	current_layer_contextH,		/* >> */
						AEGP_NumEffectsToRenderType	num_effectsS,				/* in */
						AEGP_RenderReceiptH			*render_receiptPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNumBinsToRender)(
				const	PR_RenderContextH	render_contextH,			/* >> */
						A_long				*num_bins_to_renderPL);		/* << */


	SPAPI A_Err	(*AEGP_SetNthBin)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						A_long						n);					/* >> */

	SPAPI A_Err	(*AEGP_GetBinType)(
				const	PR_RenderContextH			render_contextH,	/* >> */
						AEGP_BinType				*bin_typeP);		/* << */

	SPAPI A_Err (*AEGP_GetRenderLayerToWorldXform2D3D)(
					PR_RenderContextH				render_contextH,	/* >> */
					AEGP_RenderLayerContextH		layer_contextH,		/* >> */
					const A_Time					*comp_timeP,		/* >> */
					A_Boolean						only_2dB,			/* >> */
					A_Matrix4						*transform);		/* << */


	// interactive artisan information
	// handle to the on-screen window
	SPAPI A_Err (*AEGP_GetPlatformWindowRef)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_PlatformWindowRef				*window_refP);		/* << */


	// the dsf src to frame scale factors
	SPAPI A_Err (*AEGP_GetViewportScale)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_FpLong							*scale_xPF,			/* << */
				A_FpLong							*scale_yPF);		/* << */


	// the dsf src to frame translate
	SPAPI A_Err (*AEGP_GetViewportOrigin)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_long								*origin_xPL,		/* << */
				A_long								*origin_yPL);		/* << */


	SPAPI A_Err (*AEGP_GetViewportRect)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_LegacyRect								*viewport_rectPR);	/* << */


	SPAPI A_Err (*AEGP_GetFallowColor)(
				const PR_RenderContextH				render_contextH,	/* >> */
				PF_Pixel8							*fallow_colorP);	/* << */

	SPAPI A_Err (*AEGP_GetInteractiveBuffer)(
				const PR_RenderContextH				render_contextH,	/* >> */
				AEGP_WorldH							*buffer);			/* << */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboard)(
				const PR_RenderContextH				render_contextH,	/* in */
				A_Boolean							*checkerboard_onPB);/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardColors)(
				const PR_RenderContextH				render_contextH,			/* in */
				PF_Pixel							*checkerboard_color1P,		/* out */
				PF_Pixel							*checkerboard_color2P);		/* out */

	SPAPI A_Err (*AEGP_GetInteractiveCheckerboardSize)(
				const PR_RenderContextH				render_contextH,			/* in */
				A_u_long							*checkerboard_widthPLu,		/* out - width of square*/
				A_u_long							*checkerboard_heightPLu);	/* out - height of square*/

	SPAPI A_Err (*AEGP_GetInteractiveCachedBuffer)(
				const PR_RenderContextH				render_contextH,			/* >> */
				AEGP_WorldH							*buffer);					/* << */


	// should we call AEGP_RenderLayer or AEGP_RenderTexture
	SPAPI A_Err (*AEGP_ArtisanMustRenderAsLayer)(
				const PR_RenderContextH				render_contextH,			/* >> */
				AEGP_RenderLayerContextH			layer_contextH,
				A_Boolean							*use_render_texturePB);


	SPAPI A_Err (*AEGP_GetInteractiveDisplayChannel)(
				const PR_RenderContextH				render_contextH,			/* >> */
				AEGP_DisplayChannelType				*display_channelP);			/* << */

	SPAPI A_Err (*AEGP_GetInteractiveExposure)(
				const PR_RenderContextH				render_contextH,			/* >> */
				A_FpLong							*exposurePF);				/* << */


	SPAPI A_Err (*AEGP_GetColorTransform)(
				const PR_RenderContextH				render_contextH,	/* >> */
				A_Boolean							*cms_onB,
				A_u_long							*xform_keyLu,
				void								*xformP);


	SPAPI A_Err	(*AEGP_GetCompShutterTime)(
						PR_RenderContextH	render_contextH,	/* >> */
						A_Time				*shutter_time,		/* << */
						A_Time				*shutter_dur);

} AEGP_CanvasSuite7;




#define kAEGPMaskOutlineSuite				"AEGP Mask Outline Suite"
#define kAEGPMaskOutlineSuiteVersion2		3 /* frozen in AE 5.5 */

typedef struct AEGP_MaskOutlineSuite2 {

	SPAPI A_Err	(*AEGP_IsMaskOutlineOpen)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_Boolean					*openPB);			/* << */

	SPAPI A_Err (*AEGP_SetMaskOutlineOpen)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_Boolean					openB);				/* >> */

	// N segments means there are segments [0..N-1]; segment J is defined by vertex J & J+1
	SPAPI A_Err	(*AEGP_GetMaskOutlineNumSegments)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_long						*num_segmentsPL);	/* << */

	// which_pointL range: [0..num_segments]; for closed masks vertex[0] == vertex[num_segments]
	SPAPI A_Err	(*AEGP_GetMaskOutlineVertexInfo)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			which_pointL,		/* >> */
						AEGP_MaskVertex				*vertexP);			/* << tangents are relative to position */

	// Setting vertex 0 is special. Its in tangent will actually set the out tangent
	// of the last vertex in the outline.
	SPAPI A_Err	(*AEGP_SetMaskOutlineVertexInfo)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			which_pointL,		/* >> must already exists (use Create) */
						const AEGP_MaskVertex		*vertexP);			/* >> tangents are relative to position */

	SPAPI A_Err (*AEGP_CreateVertex)(
						AEGP_MaskOutlineValH	mask_outlineH,			/* >> */
						AEGP_VertexIndex		insert_position);		/* >> will insert at this index. moving other verticies index++*/

	SPAPI A_Err (*AEGP_DeleteVertex)(
						AEGP_MaskOutlineValH	mask_outlineH,			/* >> */
						AEGP_VertexIndex		index);					/* >> */

} AEGP_MaskOutlineSuite2;

#define kAEGPMaskOutlineSuiteVersion1		2 /* frozen in AE 5.0 */

typedef struct AEGP_MaskOutlineSuite1 {

	SPAPI A_Err	(*AEGP_IsMaskOutlineOpen)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_Boolean					*openPB);			/* << */

	// N segments means there are segments [0..N-1]; segment J is defined by vertex J & J+1
	SPAPI A_Err	(*AEGP_GetMaskOutlineNumSegments)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						A_long						*num_segmentsPL);	/* << */

	// which_pointL range: [0..num_segments]; for closed masks vertex[0] == vertex[num_segments]
	SPAPI A_Err	(*AEGP_GetMaskOutlineVertexInfo)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			which_pointL,		/* >> */
						AEGP_MaskVertex				*vertexP);			/* << tangents are relative to position */

	// Setting vertex 0 is special. Its in tangent will actually set the out tangent
	// of the last vertex in the outline.
	SPAPI A_Err	(*AEGP_SetMaskOutlineVertexInfo)(
						AEGP_MaskOutlineValH		mask_outlineH,		/* >> */
						AEGP_VertexIndex			which_pointL,		/* >> must already exists (use Create) */
						const AEGP_MaskVertex				*vertexP);	/* >> tangents are relative to position */

	SPAPI A_Err (*AEGP_CreateVertex)(
						AEGP_MaskOutlineValH	mask_outlineH,			/* >> */
						AEGP_VertexIndex		insert_position);		/* >> will insert at this index. moving other verticies index++*/

	SPAPI A_Err (*AEGP_DeleteVertex)(
						AEGP_MaskOutlineValH	mask_outlineH,			/* >> */
						AEGP_VertexIndex		index);					/* >> */

} AEGP_MaskOutlineSuite1;


#define kAEGPCompSuite				"AEGP Comp Suite"
#define kAEGPCompSuiteVersion10		21 /* frozen in AE 12 */

typedef struct AEGP_CompSuite10 {
	
	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
										AEGP_ItemH 			itemH,					/* >> */
										AEGP_CompH			*compPH);				/* << */
	
	SPAPI A_Err	(*AEGP_GetItemFromComp)(
										AEGP_CompH			compH,					/* >> */
										AEGP_ItemH 			*itemPH);				/* << */
	
	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
												AEGP_CompH				compH,				/* >> */
												AEGP_DownsampleFactor	*dsfP);				/* << */
	
	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
												AEGP_CompH					compH,			/* <> */
												const AEGP_DownsampleFactor	*dsfP);			/* >> */
	
	SPAPI A_Err	(*AEGP_GetCompBGColor)(
									   AEGP_CompH			compH,					/* >> */
									   AEGP_ColorVal		*bg_colorP);			/* << */
	
	SPAPI A_Err (*AEGP_SetCompBGColor)(
									   AEGP_CompH				compH,				/* >> */
									   const AEGP_ColorVal		*bg_colorP);		/* >> */
	
	SPAPI A_Err	(*AEGP_GetCompFlags)(
									 AEGP_CompH			compH,					/* >> */
									 AEGP_CompFlags		*comp_flagsP);			/* << */
	
	/*Opens the comp*/
	SPAPI A_Err	(*AEGP_GetShowLayerNameOrSourceName)(
													 AEGP_CompH 				compH,					/* >> */
													 A_Boolean				*layer_names_shownPB);	/* << true if layer names, false if source names */
	
	/*Opens the comp*/
	SPAPI A_Err	(*AEGP_SetShowLayerNameOrSourceName)(
													 AEGP_CompH 				compH,					/* >> */
													 A_Boolean				show_layer_namesB);		/* >> true to show layer names, false to show source names */
	
	/*Opens the comp*/
	SPAPI A_Err	(*AEGP_GetShowBlendModes)(
										  AEGP_CompH 				compH,					/* >> */
										  A_Boolean				*blend_modes_shownPB);	/* << */
	
	/*Opens the comp*/
	SPAPI A_Err	(*AEGP_SetShowBlendModes)(
										  AEGP_CompH 				compH,					/* >> */
										  A_Boolean				show_blend_modesB);		/* << */
	
	SPAPI A_Err	(*AEGP_GetCompFramerate)(
										 AEGP_CompH			compH,					/* >> */
										 A_FpLong			*fpsPF);				/* << */
	
	SPAPI A_Err (*AEGP_SetCompFrameRate)(
										 AEGP_CompH			compH,					/* >> */
										 const A_FpLong		*fpsPF);				/* >> */
	
	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
												 AEGP_CompH			compH,					/* >> */
												 A_Ratio				*angle,					/* << */
												 A_Ratio				*phase);				/* << */
	
	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
												 AEGP_CompH			compH,					/* >> */
												 const A_Time		*comp_timeP,			/* >> */
												 A_Time				*start,					/* << */
												 A_Time				*duration);				/* << */
	
	SPAPI A_Err	(*AEGP_GetCompSuggestedMotionBlurSamples)(
														  AEGP_CompH			compH,					/* >> */
														  A_long				*samplesPL);			/* << */
	
	SPAPI A_Err	(*AEGP_SetCompSuggestedMotionBlurSamples)(			/* UNDOABLE */
														  AEGP_CompH			compH,					/* >> */
														  A_long				samplesL);				/* >> */
	
	SPAPI A_Err	(*AEGP_GetCompMotionBlurAdaptiveSampleLimit)(
															 AEGP_CompH			compH,					/* >> */
															 A_long				*samplesPL);			/* << */
	
	SPAPI A_Err	(*AEGP_SetCompMotionBlurAdaptiveSampleLimit)(			/* UNDOABLE */
															 AEGP_CompH			compH,					/* >> */
															 A_long				samplesL);				/* >> */
	
	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
											 AEGP_CompH			compH,					/* >> */
											 A_Time 				*work_area_startPT);	/* << */
	
	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
												AEGP_CompH			compH,					/* >> */
												A_Time 				*work_area_durationPT);	/* << */
	
	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
														AEGP_CompH			compH,					/* >> */
														const A_Time 		*work_area_startPT,		/* >> */
														const A_Time 		*work_area_durationPT);	/* >> */
	
	SPAPI A_Err (*AEGP_CreateSolidInComp)(
										  const A_UTF16Char		*utf_nameZ,			/* >> */
										  A_long				width,					/* >> */
										  A_long				height,					/* >> */
										  const AEGP_ColorVal	*color,					/* >> */
										  AEGP_CompH			parent_compH,			/* >> */
										  const A_Time		*durationPT0,			/* >> */
										  AEGP_LayerH			*new_solidPH);			/* << */
	
	SPAPI A_Err (*AEGP_CreateCameraInComp)(
										   const A_UTF16Char		*utf_nameZ,			/* >> */
										   A_FloatPoint		center_point,			/* >> */
										   AEGP_CompH			parent_compH,			/* >> */
										   AEGP_LayerH			*new_cameraPH);			/* << */
	
	SPAPI A_Err (*AEGP_CreateLightInComp)(
										  const A_UTF16Char		*utf_nameZ,			/* >> */
										  A_FloatPoint		center_point,			/* >> */
										  AEGP_CompH			parent_compH,			/* >> */
										  AEGP_LayerH			*new_lightPH);			/* << */
	
	SPAPI A_Err (*AEGP_CreateComp)(
								   AEGP_ItemH			parent_folderH0,		/* >> */
								   const A_UTF16Char		*utf_nameZ,			/* >> */
								   A_long				widthL,					/* >> */
								   A_long				heightL,				/* >> */
								   const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
								   const A_Time		*durationPT,			/* >> */
								   const A_Ratio		*frameratePRt,			/* >> */
								   AEGP_CompH			*new_compPH);			/* << */
	
	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
														  AEGP_PluginID		plugin_id,				/* >> */
														  AEGP_CompH			compH,					/* >> */
														  AEGP_Collection2H	*collectionPH);			/* << */
	
	SPAPI A_Err (*AEGP_SetSelection)(
									 AEGP_CompH			compH,					/* >> */
									 AEGP_Collection2H	collectionH);			/* >> not adopted */
	
	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
												AEGP_CompH			compH,					/* >> */
												A_Time				*start_timePT);			/* << */
	
	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
												AEGP_CompH			compH,					/* >> */
												const A_Time		*start_timePT);			/* >> */
	
	SPAPI A_Err (*AEGP_SetCompDuration)(
										AEGP_CompH			compH,					/* >> */
										const A_Time		*durationPT);			/* >> */
	
	SPAPI A_Err (*AEGP_CreateNullInComp)(
										 const A_UTF16Char		*utf_nameZ,			/* >> */
										 AEGP_CompH			parent_compH,			/* >> */
										 const A_Time		*durationPT0,			/* >> */
										 AEGP_LayerH			*new_null_solidPH);		/* << */
	
	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
												AEGP_CompH			compH,					/* >> */
												const A_Ratio		*pix_aspectratioPRt);	/* >> */
	
	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
											  AEGP_CompH			parent_compH,			/* >> */
											  A_Boolean			select_new_layerB,		/* >> */
											  AEGP_LayerH			*new_text_layerPH);		/* << */
	
	SPAPI A_Err (*AEGP_CreateBoxTextLayerInComp)(
												 AEGP_CompH			parent_compH,			/* >> */
												 A_Boolean			select_new_layerB,		/* >> */
												 A_FloatPoint		box_dimensions,			/* >> */ // (width and height)
												 AEGP_LayerH			*new_text_layerPH);		/* << */
	
	SPAPI A_Err (*AEGP_SetCompDimensions)(
										  AEGP_CompH			compH,					/* >> */
										  A_long				widthL,					/* >> */
										  A_long				heightL);				/* >> */
	
	SPAPI A_Err (*AEGP_DuplicateComp)(
									  AEGP_CompH			compH,					/* >> */
									  AEGP_CompH			*new_compPH);			/* << */
	
	SPAPI A_Err	(*AEGP_GetCompFrameDuration)(
											 AEGP_CompH			compH,    				/* >> */
											 A_Time				*timeP);				/* << */
	
	SPAPI A_Err (*AEGP_GetMostRecentlyUsedComp)(
												AEGP_CompH			*compPH);				/* << If compPH returns NULL, there's no available comp */
	
	SPAPI A_Err (*AEGP_CreateVectorLayerInComp)(
												AEGP_CompH			parent_compH,			/* >> */
												AEGP_LayerH			*new_vector_layerPH);	/* << */
	
	SPAPI A_Err (*AEGP_GetNewCompMarkerStream)(
											   AEGP_PluginID		aegp_plugin_id,			/* >> */
											   AEGP_CompH			parent_compH,			/* >> */
											   AEGP_StreamRefH		*streamPH);				/* << must be disposed by caller */
	
	SPAPI A_Err (*AEGP_GetCompDisplayDropFrame)(
												AEGP_CompH			compH,					/* >> */
												A_Boolean			*dropFramePB);			/* << */
	
	SPAPI A_Err (*AEGP_SetCompDisplayDropFrame)(
												AEGP_CompH			compH,					/* >> */
												A_Boolean			dropFrameB);			/* << */
	
} AEGP_CompSuite10;


#define kAEGPCompSuiteVersion9		19 /* frozen in AE 11 */

typedef struct AEGP_CompSuite9 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
						AEGP_CompH				compH,				/* >> */
						AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
						AEGP_CompH					compH,			/* <> */
						const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err (*AEGP_SetCompBGColor)(
						AEGP_CompH				compH,				/* >> */
						const AEGP_ColorVal		*bg_colorP);		/* >> */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
						AEGP_CompH			compH,					/* >> */
						A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err (*AEGP_SetCompFrameRate)(
						AEGP_CompH			compH,					/* >> */
						const A_FpLong		*fpsPF);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
						AEGP_CompH			compH,					/* >> */
						A_Ratio				*angle,					/* << */
						A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*comp_timeP,			/* >> */
						A_Time				*start,					/* << */
						A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompSuggestedMotionBlurSamples)(
						AEGP_CompH			compH,					/* >> */
						A_long				*samplesPL);			/* << */

	SPAPI A_Err	(*AEGP_SetCompSuggestedMotionBlurSamples)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						A_long				samplesL);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						const A_Time 		*work_area_startPT,		/* >> */
						const A_Time 		*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const AEGP_ColorVal	*color,					/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
						AEGP_ItemH			parent_folderH0,		/* >> */
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL,				/* >> */
						const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
						const A_Time		*durationPT,			/* >> */
						const A_Ratio		*frameratePRt,			/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
						AEGP_PluginID		plugin_id,				/* >> */
						AEGP_CompH			compH,					/* >> */
						AEGP_Collection2H	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
						AEGP_CompH			compH,					/* >> */
						AEGP_Collection2H	collectionH);			/* >> not adopted */

	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
						AEGP_CompH			compH,					/* >> */
						A_Time				*start_timePT);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
						AEGP_CompH			compH,					/* >> */
						const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*durationPT);			/* >> */

	SPAPI A_Err (*AEGP_CreateNullInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_null_solidPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
						AEGP_CompH			compH,					/* >> */
						const A_Ratio		*pix_aspectratioPRt);	/* >> */

	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						A_Boolean			select_new_layerB,		/* >> */
						AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_CreateBoxTextLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						A_Boolean			select_new_layerB,		/* >> */
						A_FloatPoint		box_dimensions,			/* >> */ // (width and height)
						AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompDimensions)(
						AEGP_CompH			compH,					/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL);				/* >> */

	SPAPI A_Err (*AEGP_DuplicateComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFrameDuration)(
						AEGP_CompH			compH,    				/* >> */
						A_Time				*timeP);				/* << */

	SPAPI A_Err (*AEGP_GetMostRecentlyUsedComp)(
						AEGP_CompH			*compPH);				/* << If compPH returns NULL, there's no available comp */

	SPAPI A_Err (*AEGP_CreateVectorLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_vector_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetNewCompMarkerStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_StreamRefH		*streamPH);				/* << must be disposed by caller */

	SPAPI A_Err (*AEGP_GetCompDisplayDropFrame)(
						AEGP_CompH			compH,					/* >> */
						A_Boolean			*dropFramePB);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayDropFrame)(
						AEGP_CompH			compH,					/* >> */
						A_Boolean			dropFrameB);			/* << */


} AEGP_CompSuite9;


#define kAEGPCompSuiteVersion8		18 /* frozen in AE 10.5 */

typedef struct AEGP_CompSuite8 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
		AEGP_ItemH 			itemH,					/* >> */
		AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
		AEGP_CompH			compH,					/* >> */
		AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
		AEGP_CompH				compH,				/* >> */
		AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
		AEGP_CompH					compH,			/* <> */
		const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
		AEGP_CompH			compH,					/* >> */
		AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err (*AEGP_SetCompBGColor)(
		AEGP_CompH				compH,				/* >> */
		const AEGP_ColorVal		*bg_colorP);		/* >> */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
		AEGP_CompH			compH,					/* >> */
		AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
		AEGP_CompH			compH,					/* >> */
		A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err (*AEGP_SetCompFrameRate)(
		AEGP_CompH			compH,					/* >> */
		const A_FpLong		*fpsPF);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
		AEGP_CompH			compH,					/* >> */
		A_Ratio				*angle,					/* << */
		A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
		AEGP_CompH			compH,					/* >> */
		const A_Time		*comp_timeP,			/* >> */
		A_Time				*start,					/* << */
		A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompSuggestedMotionBlurSamples)(
		AEGP_CompH			compH,					/* >> */
		A_long				*samplesPL);			/* << */

	SPAPI A_Err	(*AEGP_SetCompSuggestedMotionBlurSamples)(			/* UNDOABLE */
		AEGP_CompH			compH,					/* >> */
		A_long				samplesL);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
		AEGP_CompH			compH,					/* >> */
		A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
		AEGP_CompH			compH,					/* >> */
		A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
		AEGP_CompH			compH,					/* >> */
		const A_Time 		*work_area_startPT,		/* >> */
		const A_Time 		*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
		const A_UTF16Char		*utf_nameZ,			/* >> */
		A_long				width,					/* >> */
		A_long				height,					/* >> */
		const AEGP_ColorVal	*color,					/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		const A_Time		*durationPT0,			/* >> */
		AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
		const A_UTF16Char		*utf_nameZ,			/* >> */
		A_FloatPoint		center_point,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
		const A_UTF16Char		*utf_nameZ,			/* >> */
		A_FloatPoint		center_point,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
		AEGP_ItemH			parent_folderH0,		/* >> */
		const A_UTF16Char		*utf_nameZ,			/* >> */
		A_long				widthL,					/* >> */
		A_long				heightL,				/* >> */
		const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
		const A_Time		*durationPT,			/* >> */
		const A_Ratio		*frameratePRt,			/* >> */
		AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
		AEGP_PluginID		plugin_id,				/* >> */
		AEGP_CompH			compH,					/* >> */
		AEGP_Collection2H	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
		AEGP_CompH			compH,					/* >> */
		AEGP_Collection2H	collectionH);			/* >> not adopted */

	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
		AEGP_CompH			compH,					/* >> */
		A_Time				*start_timePT);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
		AEGP_CompH			compH,					/* >> */
		const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
		AEGP_CompH			compH,					/* >> */
		const A_Time		*durationPT);			/* >> */

	SPAPI A_Err (*AEGP_CreateNullInComp)(
		const A_UTF16Char		*utf_nameZ,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		const A_Time		*durationPT0,			/* >> */
		AEGP_LayerH			*new_null_solidPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
		AEGP_CompH			compH,					/* >> */
		const A_Ratio		*pix_aspectratioPRt);	/* >> */

	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_CreateBoxTextLayerInComp)(
		AEGP_CompH			parent_compH,			/* >> */
		A_FloatPoint		box_dimensions,			/* >> */ // (width and height)
		AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompDimensions)(
		AEGP_CompH			compH,					/* >> */
		A_long				widthL,					/* >> */
		A_long				heightL);				/* >> */

	SPAPI A_Err (*AEGP_DuplicateComp)(
		AEGP_CompH			compH,					/* >> */
		AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFrameDuration)(
		AEGP_CompH			compH,    				/* >> */
		A_Time				*timeP);				/* << */

	SPAPI A_Err (*AEGP_GetMostRecentlyUsedComp)(
		AEGP_CompH			*compPH);				/* << If compPH returns NULL, there's no available comp */

	SPAPI A_Err (*AEGP_CreateVectorLayerInComp)(
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_vector_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetNewCompMarkerStream)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_StreamRefH		*streamPH);				/* << must be disposed by caller */

	SPAPI A_Err (*AEGP_GetCompDisplayDropFrame)(
		AEGP_CompH			compH,					/* >> */
		A_Boolean			*dropFramePB);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayDropFrame)(
		AEGP_CompH			compH,					/* >> */
		A_Boolean			dropFrameB);			/* << */


} AEGP_CompSuite8;



#define kAEGPCompSuiteVersion7		15 /* frozen in AE 9.0 */

typedef struct AEGP_CompSuite7 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
						AEGP_CompH				compH,				/* >> */
						AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
						AEGP_CompH					compH,			/* <> */
						const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err (*AEGP_SetCompBGColor)(
						AEGP_CompH				compH,				/* >> */
						const AEGP_ColorVal		*bg_colorP);		/* >> */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
						AEGP_CompH			compH,					/* >> */
						A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err (*AEGP_SetCompFrameRate)(
						AEGP_CompH			compH,					/* >> */
						const A_FpLong		*fpsPF);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
						AEGP_CompH			compH,					/* >> */
						A_Ratio				*angle,					/* << */
						A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*comp_timeP,			/* >> */
						A_Time				*start,					/* << */
						A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompSuggestedMotionBlurSamples)(
						AEGP_CompH			compH,					/* >> */
						A_long				*samplesPL);			/* << */

	SPAPI A_Err	(*AEGP_SetCompSuggestedMotionBlurSamples)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						A_long				samplesL);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						const A_Time 		*work_area_startPT,		/* >> */
						const A_Time 		*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const AEGP_ColorVal	*color,					/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
						AEGP_ItemH			parent_folderH0,		/* >> */
						const A_UTF16Char		*utf_nameZ,			/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL,				/* >> */
						const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
						const A_Time		*durationPT,			/* >> */
						const A_Ratio		*frameratePRt,			/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
						AEGP_PluginID		plugin_id,				/* >> */
						AEGP_CompH			compH,					/* >> */
						AEGP_Collection2H	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
						AEGP_CompH			compH,					/* >> */
						AEGP_Collection2H	collectionH);			/* >> not adopted */

	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
						AEGP_CompH			compH,					/* >> */
						A_Time				*start_timePT);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
						AEGP_CompH			compH,					/* >> */
						const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*durationPT);			/* >> */

	SPAPI A_Err (*AEGP_CreateNullInComp)(
						const A_UTF16Char		*utf_nameZ,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_null_solidPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
						AEGP_CompH			compH,					/* >> */
						const A_Ratio		*pix_aspectratioPRt);	/* >> */

	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompDimensions)(
						AEGP_CompH			compH,					/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL);				/* >> */

	SPAPI A_Err (*AEGP_DuplicateComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFrameDuration)(
						AEGP_CompH			compH,    				/* >> */
						A_Time				*timeP);				/* << */

	SPAPI A_Err (*AEGP_GetMostRecentlyUsedComp)(
						AEGP_CompH			*compPH);				/* << If compPH returns NULL, there's no available comp */

	SPAPI A_Err (*AEGP_CreateVectorLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_vector_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetNewCompMarkerStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_StreamRefH		*streamPH);				/* << must be disposed by caller */

} AEGP_CompSuite7;


#define kAEGPCompSuiteVersion6		14 /* frozen in AE 8.0 */

typedef struct AEGP_CompSuite6 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
		AEGP_ItemH 			itemH,					/* >> */
		AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
		AEGP_CompH			compH,					/* >> */
		AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
		AEGP_CompH				compH,				/* >> */
		AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
		AEGP_CompH					compH,			/* <> */
		const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
		AEGP_CompH			compH,					/* >> */
		AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err (*AEGP_SetCompBGColor)(
		AEGP_CompH				compH,				/* >> */
		const AEGP_ColorVal		*bg_colorP);		/* >> */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
		AEGP_CompH			compH,					/* >> */
		AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
		AEGP_CompH			compH,					/* >> */
		A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err (*AEGP_SetCompFrameRate)(
		AEGP_CompH			compH,					/* >> */
		const A_FpLong		*fpsPF);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
		AEGP_CompH			compH,					/* >> */
		A_Ratio				*angle,					/* << */
		A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
		AEGP_CompH			compH,					/* >> */
		const A_Time		*comp_timeP,			/* >> */
		A_Time				*start,					/* << */
		A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompSuggestedMotionBlurSamples)(
		AEGP_CompH			compH,					/* >> */
		A_long				*samplesPL);			/* << */

	SPAPI A_Err	(*AEGP_SetCompSuggestedMotionBlurSamples)(			/* UNDOABLE */
		AEGP_CompH			compH,					/* >> */
		A_long				samplesL);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
		AEGP_CompH			compH,					/* >> */
		A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
		AEGP_CompH			compH,					/* >> */
		A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
		AEGP_CompH			compH,					/* >> */
		const A_Time 		*work_area_startPT,		/* >> */
		const A_Time 		*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
		const A_char		*nameZ,					/* >> */
		A_long				width,					/* >> */
		A_long				height,					/* >> */
		const AEGP_ColorVal	*color,					/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		const A_Time		*durationPT0,			/* >> */
		AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
		const A_char		*nameZ,					/* >> */
		A_FloatPoint		center_point,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
		const A_char		*nameZ,					/* >> */
		A_FloatPoint		center_point,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
		AEGP_ItemH			parent_folderH0,		/* >> */
		const A_char		*nameZ,					/* >> */
		A_long				widthL,					/* >> */
		A_long				heightL,				/* >> */
		const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
		const A_Time		*durationPT,			/* >> */
		const A_Ratio		*frameratePRt,			/* >> */
		AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
		AEGP_PluginID		plugin_id,				/* >> */
		AEGP_CompH			compH,					/* >> */
		AEGP_Collection2H	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
		AEGP_CompH			compH,					/* >> */
		AEGP_Collection2H	collectionH);			/* >> not adopted */

	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
		AEGP_CompH			compH,					/* >> */
		A_Time				*start_timePT);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
		AEGP_CompH			compH,					/* >> */
		const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
		AEGP_CompH			compH,					/* >> */
		const A_Time		*durationPT);			/* >> */

	SPAPI A_Err (*AEGP_CreateNullInComp)(
		const A_char		*nameZ,					/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		const A_Time		*durationPT0,			/* >> */
		AEGP_LayerH			*new_null_solidPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
		AEGP_CompH			compH,					/* >> */
		const A_Ratio		*pix_aspectratioPRt);	/* >> */

	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompDimensions)(
		AEGP_CompH			compH,					/* >> */
		A_long				widthL,					/* >> */
		A_long				heightL);				/* >> */

	SPAPI A_Err (*AEGP_DuplicateComp)(
		AEGP_CompH			compH,					/* >> */
		AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFrameDuration)(
		AEGP_CompH			compH,    				/* >> */
		A_Time				*timeP);				/* << */

	SPAPI A_Err (*AEGP_GetMostRecentlyUsedComp)(
		AEGP_CompH			*compPH);				/* << If compPH returns NULL, there's no available comp */

	SPAPI A_Err (*AEGP_CreateVectorLayerInComp)(
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_vector_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetNewCompMarkerStream)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_StreamRefH		*streamPH);				/* << must be disposed by caller */

} AEGP_CompSuite6;


#define kAEGPCompSuiteVersion5		11 /* frozen AE 7.0 */

typedef struct AEGP_CompSuite5 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
		AEGP_ItemH 			itemH,					/* >> */
		AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
		AEGP_CompH			compH,					/* >> */
		AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
		AEGP_CompH				compH,				/* >> */
		AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
		AEGP_CompH					compH,			/* <> */
		const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
		AEGP_CompH			compH,					/* >> */
		AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err (*AEGP_SetCompBGColor)(
		AEGP_CompH				compH,				/* >> */
		const AEGP_ColorVal		*bg_colorP);		/* >> */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
		AEGP_CompH			compH,					/* >> */
		AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
		AEGP_CompH			compH,					/* >> */
		A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err (*AEGP_SetCompFrameRate)(
		AEGP_CompH			compH,					/* >> */
		const A_FpLong		*fpsPF);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
		AEGP_CompH			compH,					/* >> */
		A_Ratio				*angle,					/* << */
		A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
		AEGP_CompH			compH,					/* >> */
		const A_Time		*comp_timeP,			/* >> */
		A_Time				*start,					/* << */
		A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
		AEGP_CompH			compH,					/* >> */
		A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
		AEGP_CompH			compH,					/* >> */
		A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
		AEGP_CompH			compH,					/* >> */
		A_Time 				*work_area_startPT,		/* >> */
		A_Time 				*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
		const A_char		*nameZ,					/* >> */
		A_long				width,					/* >> */
		A_long				height,					/* >> */
		const AEGP_ColorVal	*color,					/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		const A_Time		*durationPT0,			/* >> */
		AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
		const A_char		*nameZ,					/* >> */
		A_FloatPoint		center_point,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
		const A_char		*nameZ,					/* >> */
		A_FloatPoint		center_point,			/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
		AEGP_ItemH			parent_folderH0,		/* >> */
		const A_char		*nameZ,					/* >> */
		A_long				widthL,					/* >> */
		A_long				heightL,				/* >> */
		const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
		const A_Time		*durationPT,			/* >> */
		const A_Ratio		*frameratePRt,			/* >> */
		AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
		AEGP_PluginID		plugin_id,				/* >> */
		AEGP_CompH			compH,					/* >> */
		AEGP_Collection2H	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
		AEGP_CompH	compH,							/* >> */
		AEGP_Collection2H collectionH);				/* >> not adopted */

	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
		AEGP_CompH			compH,					/* >> */
		A_Time				*start_timePT);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
		AEGP_CompH			compH,					/* >> */
		const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
		AEGP_CompH			compH,					/* >> */
		const A_Time		*durationPT);			/* >> */

	SPAPI A_Err (*AEGP_CreateNullInComp)(
		const A_char		*nameZ,					/* >> */
		AEGP_CompH			parent_compH,			/* >> */
		const A_Time		*durationPT0,			/* >> */
		AEGP_LayerH			*new_null_solidPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
		AEGP_CompH		compH,						/* >> */
		const A_Ratio	*pix_aspectratioPRt);		/* >> */

	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
		AEGP_CompH			parent_compH,			/* >> */
		AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompDimensions)(
		AEGP_CompH		compH,						/* >> */
		A_long			widthL,						/* >> */
		A_long			heightL);					/* >> */

	SPAPI A_Err (*AEGP_DuplicateComp)(
		AEGP_CompH		compH,						/* >> */
		AEGP_CompH		*new_compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompFrameDuration)(AEGP_CompH compH,    	/* >> */
		A_Time*		timeP);							/* << */

	SPAPI A_Err (*AEGP_GetMostRecentlyUsedComp)(
		AEGP_CompH		*compPH);					/* << If compPH returns NULL, there's no available comp */
} AEGP_CompSuite5;


#define kAEGPCompSuiteVersion4		9 /* frozen AE 6.5 */

typedef struct AEGP_CompSuite4 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
						AEGP_CompH				compH,				/* >> */
						AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
						AEGP_CompH					compH,			/* <> */
						const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
						AEGP_CompH			compH,					/* >> */
						A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err (*AEGP_SetCompFrameRate)(
						AEGP_CompH			compH,					/* >> */
						const A_FpLong		*fpsPF);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
						AEGP_CompH			compH,					/* >> */
						A_Ratio				*angle,					/* << */
						A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*comp_timeP,			/* >> */
						A_Time				*start,					/* << */
						A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT,		/* >> */
						A_Time 				*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
						const A_char		*nameZ,					/* >> */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const AEGP_ColorVal	*color,					/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
						const A_char		*nameZ,					/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
						const A_char		*nameZ,					/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
						AEGP_ItemH			parent_folderH0,		/* >> */
						const A_char		*nameZ,					/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL,				/* >> */
						const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
						const A_Time		*durationPT,			/* >> */
						const A_Ratio		*frameratePRt,			/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
						AEGP_PluginID		plugin_id,				/* >> */
						AEGP_CompH			compH,					/* >> */
						AEGP_Collection2H	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
						AEGP_CompH	compH,							/* >> */
						AEGP_Collection2H collectionH);				/* >> not adopted */

	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
						AEGP_CompH			compH,					/* >> */
						A_Time				*start_timePT);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
						AEGP_CompH			compH,					/* >> */
						const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*durationPT);			/* >> */

	SPAPI A_Err (*AEGP_CreateNullInComp)(
						const A_char		*nameZ,					/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_null_solidPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
						AEGP_CompH		compH,						/* >> */
						const A_Ratio	*pix_aspectratioPRt);		/* >> */

	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_text_layerPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompDimensions)(
						AEGP_CompH		compH,						/* >> */
						A_long			widthL,						/* >> */
						A_long			heightL);					/* >> */

	SPAPI A_Err (*AEGP_DuplicateComp)(
						AEGP_CompH		compH,						/* >> */
						AEGP_CompH		*new_compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompFrameDuration)(AEGP_CompH compH,    // in
		A_Time*		timeP);   // out

} AEGP_CompSuite4;


#define kAEGPCompSuiteVersion3		7 /* frozen AE 6.0 */

typedef struct AEGP_CompSuite3 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
						AEGP_CompH				compH,				/* >> */
						AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
						AEGP_CompH					compH,			/* <> */
						const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
						AEGP_CompH			compH,					/* >> */
						A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err (*AEGP_SetCompFrameRate)(
						AEGP_CompH			compH,					/* >> */
						const A_FpLong		*fpsPF);				/* >> */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
						AEGP_CompH			compH,					/* >> */
						A_Ratio				*angle,					/* << */
						A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*comp_timeP,			/* >> */
						A_Time				*start,					/* << */
						A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT,		/* >> */
						A_Time 				*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
						const A_char		*nameZ,					/* >> */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const AEGP_ColorVal	*color,					/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
						const A_char		*nameZ,					/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
						const A_char		*nameZ,					/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
						AEGP_ItemH			parent_folderH0,		/* >> */
						const A_char		*nameZ,					/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL,				/* >> */
						const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
						const A_Time		*durationPT,			/* >> */
						const A_Ratio		*frameratePRt,			/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
						AEGP_PluginID		plugin_id,				/* >> */
						AEGP_CompH			compH,					/* >> */
						AEGP_CollectionH	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
						AEGP_CompH	compH,							/* >> */
						AEGP_CollectionH collectionH);				/* >> not adopted */

	SPAPI A_Err (*AEGP_GetCompDisplayStartTime)(
						AEGP_CompH			compH,					/* >> */
						A_Time				*start_timePT);			/* << */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
						AEGP_CompH			compH,					/* >> */
						const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*durationPT);			/* >> */

	SPAPI A_Err (*AEGP_CreateNullInComp)(
						const A_char		*nameZ,					/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_null_solidPH);		/* << */

	SPAPI A_Err (*AEGP_SetCompPixelAspectRatio)(
						AEGP_CompH		compH,						/* >> */
						const A_Ratio	*pix_aspectratioPRt);		/* >> */

	SPAPI A_Err (*AEGP_CreateTextLayerInComp)(
						AEGP_CompH			parent_compH,			/* >> */
						AEGP_LayerH			*new_text_layerPH);		/* << */

} AEGP_CompSuite3;


#define kAEGPCompSuiteVersion2		6 /* frozen in AE 5.5 */

typedef struct AEGP_CompSuite2 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
						AEGP_CompH				compH,				/* >> */
						AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_SetCompDownsampleFactor)(
						AEGP_CompH					compH,			/* <> */
						const AEGP_DownsampleFactor	*dsfP);			/* >> */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
						AEGP_CompH			compH,					/* >> */
						A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
						AEGP_CompH			compH,					/* >> */
						A_Ratio				*angle,					/* << */
						A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*comp_timeP,			/* >> */
						A_Time				*start,					/* << */
						A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT,		/* >> */
						A_Time 				*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
						const A_char		*nameZ,					/* >> */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const AEGP_ColorVal	*color,					/* >> */
						AEGP_ItemH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateCameraInComp)(
						const A_char		*nameZ,					/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_ItemH			parent_compH,			/* >> */
						AEGP_LayerH			*new_cameraPH);			/* << */

	SPAPI A_Err (*AEGP_CreateLightInComp)(
						const A_char		*nameZ,					/* >> */
						A_FloatPoint		center_point,			/* >> */
						AEGP_ItemH			parent_compH,			/* >> */
						AEGP_LayerH			*new_lightPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
						AEGP_ItemH			parent_folderH0,		/* >> */
						const A_char		*nameZ,					/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL,				/* >> */
						const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
						const A_Time		*durationPT,			/* >> */
						const A_Ratio		*frameratePRt,			/* >> */
						AEGP_CompH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
						AEGP_PluginID		plugin_id,				/* >> */
						AEGP_CompH			compH,					/* >> */
						AEGP_CollectionH	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
						AEGP_CompH	compH,							/* >> */
						AEGP_CollectionH collectionH);				/* >> not adopted */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
						AEGP_CompH			compH,					/* >> */
						const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*durationPT);			/* >> */


	SPAPI A_Err (*AEGP_CreateNullInComp)(
						const A_char		*nameZ,					/* >> */
						AEGP_CompH			parent_compH,			/* >> */
						const A_Time		*durationPT0,			/* >> */
						AEGP_LayerH			*new_null_solidPH);		/* << */

} AEGP_CompSuite2;


#define kAEGPCompSuiteVersion1		4 /* frozen in AE 5.0 */

typedef struct AEGP_CompSuite1 {

	SPAPI A_Err	(*AEGP_GetCompFromItem)(							// error if item isn't AEGP_ItemType_COMP!
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetItemFromComp)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ItemH 			*itemPH);				/* << */

	SPAPI A_Err	(*AEGP_GetCompDownsampleFactor)(
						AEGP_CompH				compH,				/* >> */
						AEGP_DownsampleFactor	*dsfP);				/* << */

	SPAPI A_Err	(*AEGP_GetCompBGColor)(
						AEGP_CompH			compH,					/* >> */
						AEGP_ColorVal		*bg_colorP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFlags)(
						AEGP_CompH			compH,					/* >> */
						AEGP_CompFlags		*comp_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_GetCompFramerate)(
						AEGP_CompH			compH,					/* >> */
						A_FpLong			*fpsPF);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterAnglePhase)(
						AEGP_CompH			compH,					/* >> */
						A_Ratio				*angle,					/* << */
						A_Ratio				*phase);				/* << */

	SPAPI A_Err	(*AEGP_GetCompShutterFrameRange)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*comp_timeP,			/* >> */
						A_Time				*start,					/* << */
						A_Time				*duration);				/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaStart)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT);	/* << */

	SPAPI A_Err	(*AEGP_GetCompWorkAreaDuration)(
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_durationPT);	/* << */

	SPAPI A_Err	(*AEGP_SetCompWorkAreaStartAndDuration)(			/* UNDOABLE */
						AEGP_CompH			compH,					/* >> */
						A_Time 				*work_area_startPT,		/* >> */
						A_Time 				*work_area_durationPT);	/* >> */

	SPAPI A_Err (*AEGP_CreateSolidInComp)(
						const A_char		*nameZ,					/* >> */
						A_long				width,					/* >> */
						A_long				height,					/* >> */
						const AEGP_ColorVal	*color,					/* >> */
						AEGP_ItemH			parent_compH,			/* >> */
						AEGP_LayerH			*new_solidPH);			/* << */

	SPAPI A_Err (*AEGP_CreateComp)(
						AEGP_ItemH			parent_folderH0,		/* >> */
						const A_char		*nameZ,					/* >> */
						A_long				widthL,					/* >> */
						A_long				heightL,				/* >> */
						const A_Ratio		*pixel_aspect_ratioPRt,	/* >> */
						const A_Time		*durationPT,			/* >> */
						const A_Ratio		*frameratePRt,			/* >> */
						AEGP_ItemH			*new_compPH);			/* << */

	SPAPI A_Err	(*AEGP_GetNewCollectionFromCompSelection)(
						AEGP_PluginID		plugin_id,				/* >> */
						AEGP_CompH			compH,					/* >> */
						AEGP_CollectionH	*collectionPH);			/* << */

	SPAPI A_Err (*AEGP_SetSelection)(
						AEGP_CompH	compH,							/* >> */
						AEGP_CollectionH collectionH);				/* >> not adopted */

	SPAPI A_Err (*AEGP_SetCompDisplayStartTime)(					/*	NOT Undoable! */
						AEGP_CompH			compH,					/* >> */
						const A_Time		*start_timePT);			/* >> */

	SPAPI A_Err (*AEGP_SetCompDuration)(
						AEGP_CompH			compH,					/* >> */
						const A_Time		*durationPT);			/* >> */
} AEGP_CompSuite1;



#define kAEGPLayerSuite				"AEGP Layer Suite"

#define kAEGPLayerSuiteVersion8		14	/* frozen AE 12.0 x300 */

typedef struct AEGP_LayerSuite8 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
						AEGP_CompH			compH,					/* >> */
						A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
						AEGP_CompH			compH,					/* >> */
						A_long				layer_indexL,			/* >> */
						AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
						AEGP_LayerH			*layerPH);				/* << returns non null only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err (*AEGP_GetLayerSourceItemID)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*source_item_idPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
						AEGP_PluginID			pluginID,				// in
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_MemHandle		*utf_layer_namePH0,		// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
						AEGP_MemHandle		*utf_source_namePH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		single_flag,			/* >> */
						A_Boolean			valueB);				/* >> */

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time 		*in_pointPT,			/* >> */
						const A_Time 		*durationPT);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Time 		*offsetPT);				/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Ratio 		*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
						AEGP_LayerH 			layerH,				/* >> */
						AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(								/* UNDOABLE */
						AEGP_LayerH 					layerH,				/* >> */
						const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
						AEGP_ItemH 			item_to_addH,			/* >> */
						AEGP_CompH 			into_compH,				/* >> */
						A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
						AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
						AEGP_CompH 			into_compH,				/* >> */
						AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
						AEGP_LayerH 		layerH,			/* >> */
						AEGP_LTimeMode		time_mode,		/* >> */
						const A_Time	 	*timePT,		/* >> */
						A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_LTimeMode		time_mode,			/* >> */
						const A_Time	 	*timePT,			/* >> */
						A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			fill_must_be_activeB,	/* >> */
						A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_Time			*layer_timePT);					/* << */

	SPAPI A_Err	(*AEGP_ConvertLayerToCompTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*layer_timePT,					/* >> */
					A_Time			*comp_timePT) ;					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
					AEGP_LayerH				layerH,				/* >> */
					AEGP_LayerIDVal			*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*transform);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*view_timeP,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*transform);		/* << */

	SPAPI A_Err (*AEGP_SetLayerName)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_UTF16Char		*new_nameZ);				/* >> null terminated UTF16 */

	SPAPI A_Err	(*AEGP_GetLayerParent)(
				const AEGP_LayerH		layerH,				/* >> */
				AEGP_LayerH				*parent_layerPH);	/* << NULL if no parent */

	SPAPI A_Err (*AEGP_SetLayerParent)(
				AEGP_LayerH				layerH,				/* >> */
				const AEGP_LayerH		parent_layerH0);	/* >> */

	SPAPI A_Err (*AEGP_DeleteLayer)(
				AEGP_LayerH				layerH);			/* >>  	UNDOABLE */

	SPAPI A_Err (*AEGP_DuplicateLayer)(
				AEGP_LayerH				orig_layerH,			/* >> */
				AEGP_LayerH				*duplicate_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetLayerFromLayerID)(
				AEGP_CompH				parent_compH,			/* >> */
				AEGP_LayerIDVal			id,						/* >> */
				AEGP_LayerH				*layerPH);				/* << */

	SPAPI A_Err (*AEGP_GetLayerLabel)(
				AEGP_LayerH				layerH,					/* >> */
				AEGP_LabelID			*labelP);				/* << */

	SPAPI A_Err (*AEGP_SetLayerLabel)(		/* UNDOABLE */
				AEGP_LayerH				layerH,					/* >> */
				AEGP_LabelID			label);					/* >> */

	SPAPI A_Err	(*AEGP_GetLayerSamplingQuality)(
				AEGP_LayerH					layerH,					/* >> */
				AEGP_LayerSamplingQuality	*qualityP);				/* << */

	/* Option is explicitly set on the layer independent of layer quality. 
	If you want to force it on you must also set the layer quality to 
	AEGP_LayerQual_BEST with AEGP_SetLayerQuality. Otherwise it will only be using
	the specified layer sampling quality whenever the layer quality is set to AEGP_LayerQual_BEST*/
	SPAPI A_Err	(*AEGP_SetLayerSamplingQuality)(			/* UNDOABLE */
				AEGP_LayerH					layerH,					/* >> */
				AEGP_LayerSamplingQuality	quality);				/* >> */

} AEGP_LayerSuite8;


#define kAEGPLayerSuiteVersion7		13	/* frozen AE 10.0 build 396 */

typedef struct AEGP_LayerSuite7 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
						AEGP_CompH			compH,					/* >> */
						A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
						AEGP_CompH			compH,					/* >> */
						A_long				layer_indexL,			/* >> */
						AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
						AEGP_LayerH			*layerPH);				/* << returns non null only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err (*AEGP_GetLayerSourceItemID)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*source_item_idPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
						AEGP_PluginID			pluginID,				// in
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_MemHandle		*utf_layer_namePH0,		// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
						AEGP_MemHandle		*utf_source_namePH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		single_flag,			/* >> */
						A_Boolean			valueB);				/* >> */

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time 		*in_pointPT,			/* >> */
						const A_Time 		*durationPT);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Time 		*offsetPT);				/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Ratio 		*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
						AEGP_LayerH 			layerH,				/* >> */
						AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(								/* UNDOABLE */
						AEGP_LayerH 					layerH,				/* >> */
						const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
						AEGP_ItemH 			item_to_addH,			/* >> */
						AEGP_CompH 			into_compH,				/* >> */
						A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
						AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
						AEGP_CompH 			into_compH,				/* >> */
						AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
						AEGP_LayerH 		layerH,			/* >> */
						AEGP_LTimeMode		time_mode,		/* >> */
						const A_Time	 	*timePT,		/* >> */
						A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_LTimeMode		time_mode,			/* >> */
						const A_Time	 	*timePT,			/* >> */
						A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			fill_must_be_activeB,	/* >> */
						A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_Time			*layer_timePT);					/* << */

	SPAPI A_Err	(*AEGP_ConvertLayerToCompTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*layer_timePT,					/* >> */
					A_Time			*comp_timePT) ;					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
					AEGP_LayerH				layerH,				/* >> */
					AEGP_LayerIDVal			*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*transform);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*view_timeP,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*transform);		/* << */

	SPAPI A_Err (*AEGP_SetLayerName)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_UTF16Char		*new_nameZ);				/* >> null terminated UTF16 */

	SPAPI A_Err	(*AEGP_GetLayerParent)(
				const AEGP_LayerH		layerH,				/* >> */
				AEGP_LayerH				*parent_layerPH);	/* << NULL if no parent */

	SPAPI A_Err (*AEGP_SetLayerParent)(
				AEGP_LayerH				layerH,				/* >> */
				const AEGP_LayerH		parent_layerH0);	/* >> */

	SPAPI A_Err (*AEGP_DeleteLayer)(
				AEGP_LayerH				layerH);			/* >>  	UNDOABLE */

	SPAPI A_Err (*AEGP_DuplicateLayer)(
				AEGP_LayerH				orig_layerH,			/* >> */
				AEGP_LayerH				*duplicate_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetLayerFromLayerID)(
				AEGP_CompH				parent_compH,			/* >> */
				AEGP_LayerIDVal			id,						/* >> */
				AEGP_LayerH				*layerPH);				/* << */

	SPAPI A_Err (*AEGP_GetLayerLabel)(
				AEGP_LayerH				layerH,					/* >> */
				AEGP_LabelID			*labelP);				/* << */

	SPAPI A_Err (*AEGP_SetLayerLabel)(		/* UNDOABLE */
				AEGP_LayerH				layerH,					/* >> */
				AEGP_LabelID			label);					/* >> */

} AEGP_LayerSuite7;


#define kAEGPLayerSuiteVersion6		12	/* frozen AE 9.0 */

typedef struct AEGP_LayerSuite6 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
		AEGP_CompH			compH,					/* >> */
		A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
		AEGP_CompH			compH,					/* >> */
		A_long				layer_indexL,			/* >> */
		AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
		AEGP_LayerH			*layerPH);				/* << returns non null only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
		AEGP_LayerH			layerH,					/* >> */
		A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err (*AEGP_GetLayerSourceItemID)(
		AEGP_LayerH			layerH,					/* >> */
		A_long				*source_item_idPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
		AEGP_PluginID			pluginID,				// in
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_MemHandle		*utf_layer_namePH0,		// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
		AEGP_MemHandle		*utf_source_namePH0);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerFlags		single_flag,			/* >> */
		A_Boolean			valueB);				/* >> */

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
		AEGP_LayerH			layerH,					/* >> */
		A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
		AEGP_LayerH			layerH,					/* >> */
		A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		const A_Time 		*in_pointPT,			/* >> */
		const A_Time 		*durationPT);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
		AEGP_LayerH 		layerH,					/* >> */
		A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
		AEGP_LayerH 		layerH,					/* >> */
		const A_Time 		*offsetPT);				/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
		AEGP_LayerH 		layerH,					/* >> */
		A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
		AEGP_LayerH 		layerH,					/* >> */
		const A_Ratio 		*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
		AEGP_LayerH 			layerH,				/* >> */
		AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(								/* UNDOABLE */
		AEGP_LayerH 					layerH,				/* >> */
		const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
		AEGP_ItemH 			item_to_addH,			/* >> */
		AEGP_CompH 			into_compH,				/* >> */
		A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
		AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
		AEGP_CompH 			into_compH,				/* >> */
		AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
		AEGP_LayerH 		layerH,					/* >> */
		A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
		AEGP_LayerH 		layerH,			/* >> */
		AEGP_LTimeMode		time_mode,		/* >> */
		const A_Time	 	*timePT,		/* >> */
		A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
		AEGP_LayerH 		layerH,				/* >> */
		AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
		AEGP_LayerH 		layerH,				/* >> */
		A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
		AEGP_LayerH 		layerH,				/* >> */
		A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
		AEGP_LayerH 		layerH,				/* >> */
		AEGP_LTimeMode		time_mode,			/* >> */
		const A_Time	 	*timePT,			/* >> */
		A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
		AEGP_LayerH			layerH,					/* >> */
		A_Boolean			fill_must_be_activeB,	/* >> */
		A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
		AEGP_LayerH			layerH,					/* >> */
		A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
		AEGP_LayerH		layerH,							/* >> */
		const A_Time	*comp_timePT,					/* >> */
		A_Time			*layer_timePT);					/* << */

	SPAPI A_Err	(*AEGP_ConvertLayerToCompTime)(
		AEGP_LayerH		layerH,							/* >> */
		const A_Time	*layer_timePT,					/* >> */
		A_Time			*comp_timePT) ;					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
		AEGP_LayerH		layerH,							/* >> */
		const A_Time	*comp_timePT,					/* >> */
		A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
		AEGP_LayerH				layerH,				/* >> */
		AEGP_LayerIDVal			*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
		AEGP_LayerH				aegp_layerH,		/* >> */
		const A_Time			*comp_timeP,		/* >> */
		A_Matrix4				*transform);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
		AEGP_LayerH				aegp_layerH,		/* >> */
		const A_Time			*view_timeP,		/* >> */
		const A_Time			*comp_timeP,		/* >> */
		A_Matrix4				*transform);		/* << */

	SPAPI A_Err (*AEGP_SetLayerName)(
		AEGP_LayerH				aegp_layerH,		/* >> */
		const A_UTF16Char		*new_nameZ);				/* >> null terminated UTF16 */

	SPAPI A_Err	(*AEGP_GetLayerParent)(
		const AEGP_LayerH		layerH,				/* >> */
		AEGP_LayerH				*parent_layerPH);	/* << NULL if no parent */

	SPAPI A_Err (*AEGP_SetLayerParent)(
		AEGP_LayerH				layerH,				/* >> */
		const AEGP_LayerH		parent_layerH0);	/* >> */

	SPAPI A_Err (*AEGP_DeleteLayer)(
		AEGP_LayerH				layerH);			/* >>  	UNDOABLE */

	SPAPI A_Err (*AEGP_DuplicateLayer)(
		AEGP_LayerH				orig_layerH,			/* >> */
		AEGP_LayerH				*duplicate_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetLayerFromLayerID)(
		AEGP_CompH				parent_compH,			/* >> */
		AEGP_LayerIDVal			id,						/* >> */
		AEGP_LayerH				*layerPH);				/* << */

} AEGP_LayerSuite6;


#define kAEGPLayerSuiteVersion5		11	/* frozen AE 7.0 */

typedef struct AEGP_LayerSuite5 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
		AEGP_CompH			compH,					/* >> */
		A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
		AEGP_CompH			compH,					/* >> */
		A_long				layer_indexL,			/* >> */
		AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
		AEGP_LayerH			*layerPH);				/* << returns non null only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
		AEGP_LayerH			layerH,					/* >> */
		A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err (*AEGP_GetLayerSourceItemID)(
		AEGP_LayerH			layerH,					/* >> */
		A_long				*source_item_idPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
		AEGP_LayerH 		layerH,					/* >> */
		A_char				*layer_nameZ0,			/* << space for A_char[AEGP_MAX_LAYER_NAME_MB_SIZE] */
		A_char				*source_nameZ0);		/* << space for A_char[AEGP_MAX_LAYER_NAME_MB_SIZE] */

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerFlags		single_flag,			/* >> */
		A_Boolean			valueB);				/* >> */

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
		AEGP_LayerH			layerH,					/* >> */
		A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
		AEGP_LayerH			layerH,					/* >> */
		A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
		AEGP_LayerH 		layerH,					/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		const A_Time 		*in_pointPT,			/* >> */
		const A_Time 		*durationPT);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
		AEGP_LayerH 		layerH,					/* >> */
		A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
		AEGP_LayerH 		layerH,					/* >> */
		const A_Time 		*offsetPT);				/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
		AEGP_LayerH 		layerH,					/* >> */
		A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
		AEGP_LayerH 		layerH,					/* >> */
		const A_Ratio 		*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
		AEGP_LayerH 			layerH,				/* >> */
		AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(								/* UNDOABLE */
		AEGP_LayerH 					layerH,				/* >> */
		const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
		AEGP_ItemH 			item_to_addH,			/* >> */
		AEGP_CompH 			into_compH,				/* >> */
		A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
		AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
		AEGP_CompH 			into_compH,				/* >> */
		AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
		AEGP_LayerH 		layerH,					/* >> */
		A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
		AEGP_LayerH 		layerH,			/* >> */
		AEGP_LTimeMode		time_mode,		/* >> */
		const A_Time	 	*timePT,		/* >> */
		A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
		AEGP_LayerH 		layerH,				/* >> */
		AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
		AEGP_LayerH 		layerH,				/* >> */
		A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
		AEGP_LayerH 		layerH,				/* >> */
		A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
		AEGP_LayerH 		layerH,				/* >> */
		AEGP_LTimeMode		time_mode,			/* >> */
		const A_Time	 	*timePT,			/* >> */
		A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
		AEGP_LayerH			layerH,					/* >> */
		A_Boolean			fill_must_be_activeB,	/* >> */
		A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
		AEGP_LayerH			layerH,					/* >> */
		A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
		AEGP_LayerH		layerH,							/* >> */
		const A_Time	*comp_timePT,					/* >> */
		A_Time			*layer_timePT);					/* << */

	SPAPI A_Err	(*AEGP_ConvertLayerToCompTime)(
		AEGP_LayerH		layerH,							/* >> */
		const A_Time	*layer_timePT,					/* >> */
		A_Time			*comp_timePT) ;					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
		AEGP_LayerH		layerH,							/* >> */
		const A_Time	*comp_timePT,					/* >> */
		A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
		AEGP_LayerH				layerH,				/* >> */
		AEGP_LayerIDVal			*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
		AEGP_LayerH				aegp_layerH,		/* >> */
		const A_Time			*comp_timeP,		/* >> */
		A_Matrix4				*transform);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
		AEGP_LayerH				aegp_layerH,		/* >> */
		const A_Time			*view_timeP,		/* >> */
		const A_Time			*comp_timeP,		/* >> */
		A_Matrix4				*transform);		/* << */

	SPAPI A_Err (*AEGP_SetLayerName)(
		AEGP_LayerH				aegp_layerH,		/* >> */
		const A_char			*new_nameZ);		/* >> */

	SPAPI A_Err	(*AEGP_GetLayerParent)(
		const AEGP_LayerH		layerH,				/* >> */
		AEGP_LayerH				*parent_layerPH);	/* << NULL if no parent */

	SPAPI A_Err (*AEGP_SetLayerParent)(
		AEGP_LayerH				layerH,				/* >> */
		const AEGP_LayerH		parent_layerH0);	/* >> */

	SPAPI A_Err (*AEGP_DeleteLayer)(
		AEGP_LayerH				layerH);			/* >>  	UNDOABLE */

	SPAPI A_Err (*AEGP_DuplicateLayer)(
		AEGP_LayerH				orig_layerH,			/* >> */
		AEGP_LayerH				*duplicate_layerPH);	/* << */

	SPAPI A_Err (*AEGP_GetLayerFromLayerID)(
		AEGP_CompH				parent_compH,			/* >> */
		AEGP_LayerIDVal			id,						/* >> */
		AEGP_LayerH				*layerPH);				/* << */

} AEGP_LayerSuite5;



#define kAEGPLayerSuiteVersion4		10	/* frozen AE 6.5 */

typedef struct AEGP_LayerSuite4 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
						AEGP_CompH			compH,					/* >> */
						A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
						AEGP_CompH			compH,					/* >> */
						A_long				layer_indexL,			/* >> */
						AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
						AEGP_LayerH			*layerPH);				/* << returns non null only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err (*AEGP_GetLayerSourceItemID)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*source_item_idPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
						AEGP_LayerH 		layerH,					/* >> */
						A_char				*layer_nameZ0,			/* << space for A_char[AEGP_MAX_LAYER_NAME_SIZE] */
						A_char				*source_nameZ0);		/* << space for A_char[AEGP_MAX_LAYER_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		single_flag,			/* >> */
						A_Boolean			valueB);				/* >> */

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time 		*in_pointPT,			/* >> */
						const A_Time 		*durationPT);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Time 		*offsetPT);				/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Ratio 		*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
						AEGP_LayerH 			layerH,				/* >> */
						AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(								/* UNDOABLE */
						AEGP_LayerH 					layerH,				/* >> */
						const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
						AEGP_ItemH 			item_to_addH,			/* >> */
						AEGP_CompH 			into_compH,				/* >> */
						A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
						AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
						AEGP_CompH 			into_compH,				/* >> */
						AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
						AEGP_LayerH 		layerH,			/* >> */
						AEGP_LTimeMode		time_mode,		/* >> */
						const A_Time	 	*timePT,		/* >> */
						A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_LTimeMode		time_mode,			/* >> */
						const A_Time	 	*timePT,			/* >> */
						A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			fill_must_be_activeB,	/* >> */
						A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_Time			*layer_timePT);					/* << */

	SPAPI A_Err	(*AEGP_ConvertLayerToCompTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*layer_timePT,					/* >> */
					A_Time			*comp_timePT) ;					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
					AEGP_LayerH				layerH,				/* >> */
					AEGP_LayerIDVal			*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*transform);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*view_timeP,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*transform);		/* << */

	SPAPI A_Err (*AEGP_SetLayerName)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_char			*new_nameZ);		/* >> */

	SPAPI A_Err	(*AEGP_GetLayerParent)(
				const AEGP_LayerH		layerH,				/* >> */
				AEGP_LayerH				*parent_layerPH);	/* << NULL if no parent */

	SPAPI A_Err (*AEGP_SetLayerParent)(
				AEGP_LayerH				layerH,				/* >> */
				const AEGP_LayerH		parent_layerH0);	/* >> */

	SPAPI A_Err (*AEGP_DeleteLayer)(
				AEGP_LayerH				layerH);			/* >>  	UNDOABLE */

	SPAPI A_Err (*AEGP_DuplicateLayer)(
				AEGP_LayerH				orig_layerH,			/* >> */
				AEGP_LayerH				*duplicate_layerPH);	/* << */

} AEGP_LayerSuite4;

#define kAEGPLayerSuiteVersion3		8	/* frozen AE 6.0 */

typedef struct AEGP_LayerSuite3 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
						AEGP_CompH			compH,					/* >> */
						A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
						AEGP_CompH			compH,					/* >> */
						A_long				layer_indexL,			/* >> */
						AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
						AEGP_LayerH			*layerPH);				/* << only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
						AEGP_LayerH 		layerH,					/* >> */
						A_char				*layer_nameZ0,			/* << space for A_char[AEGP_MAX_LAYER_NAME_SIZE] */
						A_char				*source_nameZ0);		/* << space for A_char[AEGP_MAX_LAYER_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		single_flag,			/* >> */
						A_Boolean			valueB);				/* >> */

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time 		*in_pointPT,			/* >> */
						const A_Time 		*durationPT);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Time 		*offsetPT);				/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Ratio 		*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
						AEGP_LayerH 			layerH,				/* >> */
						AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(								/* UNDOABLE */
						AEGP_LayerH 					layerH,				/* >> */
						const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
						AEGP_ItemH 			item_to_addH,			/* >> */
						AEGP_CompH 			into_compH,				/* >> */
						A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
						AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
						AEGP_CompH 			into_compH,				/* >> */
						AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
						AEGP_LayerH 		layerH,			/* >> */
						AEGP_LTimeMode		time_mode,		/* >> */
						const A_Time	 	*timePT,		/* >> */
						A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_LTimeMode		time_mode,			/* >> */
						const A_Time	 	*timePT,			/* >> */
						A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			fill_must_be_activeB,	/* >> */
						A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_Time			*layer_timePT);					/* << */

	SPAPI A_Err	(*AEGP_ConvertLayerToCompTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*layer_timePT,					/* >> */
					A_Time			*comp_timePT) ;					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
					AEGP_LayerH				layerH,				/* >> */
					AEGP_LayerIDVal			*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*tranform);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*view_timeP,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*tranform);			/* >> */

	SPAPI A_Err (*AEGP_SetLayerName)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_char			*new_nameZ);		/* >> */

	SPAPI A_Err	(*AEGP_GetLayerParent)(
				const AEGP_LayerH		layerH,				/* >> */
				AEGP_LayerH				*parent_layerPH);	/* << NULL if no parent */

	SPAPI A_Err (*AEGP_SetLayerParent)(
				AEGP_LayerH				layerH,				/* >> */
				const AEGP_LayerH		parent_layerH0);	/* >> */

	SPAPI A_Err (*AEGP_DeleteLayer)(
				AEGP_LayerH				layerH);			/* >>  	UNDOABLE */


} AEGP_LayerSuite3;

/*******************************************************************************/

typedef struct {
	A_char				nameAC		[AEGP_MAX_MARKER_NAME_SIZE];
	A_char				urlAC		[AEGP_MAX_MARKER_URL_SIZE];
	A_char				targetAC	[AEGP_MAX_MARKER_TARGET_SIZE];
	A_char				chapterAC	[AEGP_MAX_MARKER_CHAPTER_SIZE];
} AEGP_MarkerVal;

typedef AEGP_MarkerVal**		AEGP_MarkerValH;

typedef union {
	AEGP_FourDVal			four_d;
	AEGP_ThreeDVal			three_d;
	AEGP_TwoDVal			two_d;
	AEGP_OneDVal			one_d;
	AEGP_ColorVal			color;
	AEGP_ArbBlockVal		arbH;
	AEGP_MarkerValH			markerH;
	AEGP_LayerIDVal			layer_id;
	AEGP_MaskIDVal			mask_id;
	AEGP_MaskOutlineValH	mask;
	AEGP_TextDocumentH		text_documentH;
} AEGP_StreamVal;

/* 	Metrowerks 2.4.6, a.k.a. Codewarrior Pro 7.1, changed PowerPC struct
	alignment. The pragma ensures the same alignment as with 2.4.5 and
	previous since plug-ins are built against this structure. See Codewarrior
	release notes for for details. Bug# WB1-27922.
*/

#if (__MWERKS__ >= 0x2406)
#pragma options align=mac68k4byte
#endif

typedef struct {

	AEGP_StreamRefH 	streamH;
	/* CW 7.1 was adding 4 padding bytes here. See pragma above for comments. */
	AEGP_StreamVal	val;
} AEGP_StreamValue;

#define kAEGPStreamSuite				"AEGP Stream Suite"

#define kAEGPStreamSuiteVersion5        10 /* frozen in AE 15 */
typedef struct AEGP_StreamSuite5 {
    //    the only diff from this vs. last rev is that routines that pass AEGP_StreamValue2, when referring to a marker,
    //    (comp or layer) the struct now contains the NEW markerP type, which is compatible with the new Marker Suite

    SPAPI A_Err (*AEGP_IsStreamLegal)(
                        AEGP_LayerH            layerH,                    /* >> */
                        AEGP_LayerStream    which_stream,            /* >> */
                        A_Boolean*            is_legalP);                /* << */


    SPAPI A_Err (*AEGP_CanVaryOverTime)(
                        AEGP_StreamRefH streamH,                    /* >> */
                        A_Boolean* can_varyPB);                        /* << */

    SPAPI A_Err (*AEGP_GetValidInterpolations)(
                        AEGP_StreamRefH                streamH,                    /* >> */
                        AEGP_KeyInterpolationMask*    valid_interpolationsP);        /* << */

    SPAPI A_Err    (*AEGP_GetNewLayerStream)(
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_LayerH            layerH,                    /* >> */
                        AEGP_LayerStream    which_stream,            /* >> */
                        AEGP_StreamRefH     *streamPH);                /* << must be disposed by caller! */

    SPAPI A_Err    (*AEGP_GetEffectNumParamStreams)(
                        AEGP_EffectRefH        effect_refH,            /* >> */
                        A_long                *num_paramsPL);            /* << */

    SPAPI A_Err    (*AEGP_GetNewEffectStreamByIndex)(
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_EffectRefH        effect_refH,            /* >> */
                        PF_ParamIndex        param_index,            /* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
                        AEGP_StreamRefH     *streamPH);                /* << must be disposed by caller! */

    SPAPI A_Err    (*AEGP_GetNewMaskStream)(
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_MaskRefH        mask_refH,                /* >> */
                        AEGP_MaskStream        which_stream,            /* >> */
                        AEGP_StreamRefH     *mask_streamPH);        /* << must be disposed by caller! */

    SPAPI A_Err    (*AEGP_DisposeStream)(
                        AEGP_StreamRefH     streamH);                /* >> */

    SPAPI A_Err    (*AEGP_GetStreamName)(
                        AEGP_PluginID            pluginID,                // in
                        AEGP_StreamRefH     streamH,                /* >> */
                        A_Boolean            force_englishB,            /* >> */
                        AEGP_MemHandle        *utf_stream_namePH);        // <<    handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

    SPAPI A_Err    (*AEGP_GetStreamUnitsText)(
                        AEGP_StreamRefH     streamH,                /* >> */
                        A_Boolean            force_englishB,            /* >> */
                        A_char                *unitsZ);                /* << space for A_char[AEGP_MAX_STREAM_UNITS_SIZE] */

    SPAPI A_Err    (*AEGP_GetStreamProperties)(
                        AEGP_StreamRefH     streamH,                /* >> */
                        AEGP_StreamFlags     *flagsP,                /* << */
                        A_FpLong             *minP0,                    /* << */
                        A_FpLong             *maxP0);                /* << */

    SPAPI A_Err    (*AEGP_IsStreamTimevarying)(                        /* takes expressions into account */
                        AEGP_StreamRefH     streamH,                /* >> */
                        A_Boolean            *is_timevaryingPB);        /* << */

    SPAPI A_Err    (*AEGP_GetStreamType)(
                        AEGP_StreamRefH     streamH,                /* >> */
                        AEGP_StreamType        *stream_typeP);            /* << */

    SPAPI A_Err    (*AEGP_GetNewStreamValue)(
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_StreamRefH     streamH,                /* >> */
                        AEGP_LTimeMode        time_mode,                /* >> */
                        const A_Time        *timePT,                /* >> */
                        A_Boolean            pre_expressionB,        /* >> sample the stream before evaluating the expression */
                        AEGP_StreamValue2    *valueP);                /* << must be disposed */

    SPAPI A_Err    (*AEGP_DisposeStreamValue)(
                        AEGP_StreamValue2    *valueP);                /* <> */


    SPAPI A_Err    (*AEGP_SetStreamValue)(                                // only legal to call when AEGP_GetStreamNumKFs==0 or NO_DATA
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_StreamRefH     streamH,                /* >> */
                        AEGP_StreamValue2    *valueP);                /* << */

    // this is only valid on streams with primitive types. It is illegal on
    // AEGP_ArbBlockVal || AEGP_MarkerValP || AEGP_MaskOutlineValH

    SPAPI A_Err    (*AEGP_GetLayerStreamValue)(
                        AEGP_LayerH            layerH,                    /* >> */
                        AEGP_LayerStream    which_stream,            /* >> */
                        AEGP_LTimeMode        time_mode,                /* >> */
                        const A_Time        *timePT,                /* >> */
                        A_Boolean            pre_expressionB,        /* >> sample the stream before evaluating the expression */
                        AEGP_StreamVal2        *stream_valP,            /* << */
                        AEGP_StreamType     *stream_typeP0);        /* << */

    SPAPI A_Err    (*AEGP_GetExpressionState)(                            /* expressions can be disabled automatically by the parser on playback */
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_StreamRefH     streamH,                /* >> */
                        A_Boolean            *enabledPB);            /* >> */

    SPAPI A_Err    (*AEGP_SetExpressionState)(                            /* expressions can be disabled automatically by the parser on playback */
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_StreamRefH     streamH,                /* >> */
                        A_Boolean            enabledB);                /* >> */

    SPAPI A_Err(*AEGP_GetExpression)(
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_StreamRefH     streamH,                /* >> */
                        AEGP_MemHandle        *unicodeHZ);            /* << must be disposed with AEGP_FreeMemHandle */

    SPAPI A_Err(*AEGP_SetExpression)(
                        AEGP_PluginID        aegp_plugin_id,            /* >> */
                        AEGP_StreamRefH     streamH,                /* >> */
                        const A_UTF16Char*    expressionP);            /* >> not adopted */

    SPAPI A_Err (*AEGP_DuplicateStreamRef)(                    // must dispose yourself
                        AEGP_PluginID        aegp_plugin_id,            // in
                        AEGP_StreamRefH        streamH,                // in
                        AEGP_StreamRefH        *dup_streamPH);            // out
} AEGP_StreamSuite5;

#define kAEGPStreamSuiteVersion4		9 /* frozen in AE 9 */
typedef struct AEGP_StreamSuite4 {
	//	the only diff from this vs. last rev is that routines that pass AEGP_StreamValue2, when referring to a marker,
	//	(comp or layer) the struct now contains the NEW markerP type, which is compatible with the new Marker Suite

	SPAPI A_Err(*AEGP_IsStreamLegal)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerStream	which_stream,			/* >> */
		A_Boolean*			is_legalP);				/* << */


	SPAPI A_Err(*AEGP_CanVaryOverTime)(
		AEGP_StreamRefH streamH,					/* >> */
		A_Boolean* can_varyPB);						/* << */

	SPAPI A_Err(*AEGP_GetValidInterpolations)(
		AEGP_StreamRefH				streamH,					/* >> */
		AEGP_KeyInterpolationMask*	valid_interpolationsP);		/* << */

	SPAPI A_Err(*AEGP_GetNewLayerStream)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerStream	which_stream,			/* >> */
		AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err(*AEGP_GetEffectNumParamStreams)(
		AEGP_EffectRefH		effect_refH,			/* >> */
		A_long				*num_paramsPL);			/* << */

	SPAPI A_Err(*AEGP_GetNewEffectStreamByIndex)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_EffectRefH		effect_refH,			/* >> */
		PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
		AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err(*AEGP_GetNewMaskStream)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_MaskRefH		mask_refH,				/* >> */
		AEGP_MaskStream		which_stream,			/* >> */
		AEGP_StreamRefH 	*mask_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err(*AEGP_DisposeStream)(
		AEGP_StreamRefH 	streamH);				/* >> */

	SPAPI A_Err(*AEGP_GetStreamName)(
		AEGP_PluginID			pluginID,				// in
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			force_englishB,			/* >> */
		AEGP_MemHandle		*utf_stream_namePH);		// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err(*AEGP_GetStreamUnitsText)(
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			force_englishB,			/* >> */
		A_char				*unitsZ);				/* << space for A_char[AEGP_MAX_STREAM_UNITS_SIZE] */

	SPAPI A_Err(*AEGP_GetStreamProperties)(
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_StreamFlags 	*flagsP,				/* << */
		A_FpLong 			*minP0,					/* << */
		A_FpLong 			*maxP0);				/* << */

	SPAPI A_Err(*AEGP_IsStreamTimevarying)(						/* takes expressions into account */
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			*is_timevaryingPB);		/* << */

	SPAPI A_Err(*AEGP_GetStreamType)(
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_StreamType		*stream_typeP);			/* << */

	SPAPI A_Err(*AEGP_GetNewStreamValue)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		const A_Time		*timePT,				/* >> */
		A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
		AEGP_StreamValue2	*valueP);				/* << must be disposed */

	SPAPI A_Err(*AEGP_DisposeStreamValue)(
		AEGP_StreamValue2	*valueP);				/* <> */


	SPAPI A_Err(*AEGP_SetStreamValue)(								// only legal to call when AEGP_GetStreamNumKFs==0 or NO_DATA
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_StreamValue2	*valueP);				/* << */

	// this is only valid on streams with primitive types. It is illegal on
	// AEGP_ArbBlockVal || AEGP_MarkerValP || AEGP_MaskOutlineValH

	SPAPI A_Err(*AEGP_GetLayerStreamValue)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerStream	which_stream,			/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		const A_Time		*timePT,				/* >> */
		A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
		AEGP_StreamVal2		*stream_valP,			/* << */
		AEGP_StreamType 	*stream_typeP0);		/* << */

	SPAPI A_Err(*AEGP_GetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			*enabledPB);			/* >> */

	SPAPI A_Err(*AEGP_SetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			enabledB);				/* >> */

	SPAPI A_Err(*AEGP_GetExpression)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_MemHandle		*expressionHZ);			/* << must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err(*AEGP_SetExpression)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		const A_char*			expressionP);			/* >> not adopted */

	SPAPI A_Err(*AEGP_DuplicateStreamRef)(					// must dispose yourself
		AEGP_PluginID		aegp_plugin_id,			// in
		AEGP_StreamRefH		streamH,				// in
		AEGP_StreamRefH		*dup_streamPH);			// out
} AEGP_StreamSuite4;



#define kAEGPStreamSuiteVersion3		8 /* frozen in AE 8 */
typedef struct AEGP_StreamSuite3 {
	//	the only diff from this vs. last rev is that routines that pass AEGP_StreamValue2, when referring to a marker,
	//	(comp or layer) the struct now contains the NEW markerP type, which is compatible with the new Marker Suite

	SPAPI A_Err (*AEGP_IsStreamLegal)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerStream	which_stream,			/* >> */
		A_Boolean*			is_legalP);				/* << */


	SPAPI A_Err (*AEGP_CanVaryOverTime)(
		AEGP_StreamRefH streamH,					/* >> */
		A_Boolean* can_varyPB);						/* << */

	SPAPI A_Err (*AEGP_GetValidInterpolations)(
		AEGP_StreamRefH				streamH,					/* >> */
		AEGP_KeyInterpolationMask*	valid_interpolationsP);		/* << */

	SPAPI A_Err	(*AEGP_GetNewLayerStream)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerStream	which_stream,			/* >> */
		AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetEffectNumParamStreams)(
		AEGP_EffectRefH		effect_refH,			/* >> */
		A_long				*num_paramsPL);			/* << */

	SPAPI A_Err	(*AEGP_GetNewEffectStreamByIndex)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_EffectRefH		effect_refH,			/* >> */
		PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
		AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewMaskStream)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_MaskRefH		mask_refH,				/* >> */
		AEGP_MaskStream		which_stream,			/* >> */
		AEGP_StreamRefH 	*mask_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_DisposeStream)(
		AEGP_StreamRefH 	streamH);				/* >> */

	SPAPI A_Err	(*AEGP_GetStreamName)(
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			force_englishB,			/* >> */
		A_char				*nameZ);				/* << space for A_char[AEGP_MAX_STREAM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetStreamUnitsText)(
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			force_englishB,			/* >> */
		A_char				*unitsZ);				/* << space for A_char[AEGP_MAX_STREAM_UNITS_SIZE] */

	SPAPI A_Err	(*AEGP_GetStreamProperties)(
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_StreamFlags 	*flagsP,				/* << */
		A_FpLong 			*minP0,					/* << */
		A_FpLong 			*maxP0);				/* << */

	SPAPI A_Err	(*AEGP_IsStreamTimevarying)(						/* takes expressions into account */
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			*is_timevaryingPB);		/* << */

	SPAPI A_Err	(*AEGP_GetStreamType)(
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_StreamType		*stream_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetNewStreamValue)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		const A_Time		*timePT,				/* >> */
		A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
		AEGP_StreamValue2	*valueP);				/* << must be disposed */

	SPAPI A_Err	(*AEGP_DisposeStreamValue)(
		AEGP_StreamValue2	*valueP);				/* <> */


	SPAPI A_Err	(*AEGP_SetStreamValue)(								// only legal to call when AEGP_GetStreamNumKFs==0 or NO_DATA
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_StreamValue2	*valueP);				/* << */

	// this is only valid on streams with primitive types. It is illegal on
	// AEGP_ArbBlockVal || AEGP_MarkerValP || AEGP_MaskOutlineValH

	SPAPI A_Err	(*AEGP_GetLayerStreamValue)(
		AEGP_LayerH			layerH,					/* >> */
		AEGP_LayerStream	which_stream,			/* >> */
		AEGP_LTimeMode		time_mode,				/* >> */
		const A_Time		*timePT,				/* >> */
		A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
		AEGP_StreamVal2		*stream_valP,			/* << */
		AEGP_StreamType 	*stream_typeP0);		/* << */

	SPAPI A_Err	(*AEGP_GetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			*enabledPB);			/* >> */

	SPAPI A_Err	(*AEGP_SetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		A_Boolean			enabledB);				/* >> */

	SPAPI A_Err	(*AEGP_GetExpression)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		AEGP_MemHandle		*expressionHZ);			/* << must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err	(*AEGP_SetExpression)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_StreamRefH 	streamH,				/* >> */
		const A_char*			expressionP);			/* >> not adopted */

	SPAPI A_Err (*AEGP_DuplicateStreamRef)(					// must dispose yourself
		AEGP_PluginID		aegp_plugin_id,			// in
		AEGP_StreamRefH		streamH,				// in
		AEGP_StreamRefH		*dup_streamPH);			// out
} AEGP_StreamSuite3;


#define kAEGPStreamSuiteVersion2		7 /* frozen in AE 6.5 */

typedef struct AEGP_StreamSuite2 {

	SPAPI A_Err (*AEGP_IsStreamLegal)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						A_Boolean*			is_legalP);				/* << */


	SPAPI A_Err (*AEGP_CanVaryOverTime)(
						AEGP_StreamRefH streamH,					/* >> */
						A_Boolean* can_varyPB);						/* << */

	SPAPI A_Err (*AEGP_GetValidInterpolations)(
						AEGP_StreamRefH				streamH,					/* >> */
						AEGP_KeyInterpolationMask*	valid_interpolationsP);		/* << */

	SPAPI A_Err	(*AEGP_GetNewLayerStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetEffectNumParamStreams)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						A_long				*num_paramsPL);			/* << */

	SPAPI A_Err	(*AEGP_GetNewEffectStreamByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
						AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewMaskStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_MaskRefH		mask_refH,				/* >> */
						AEGP_MaskStream		which_stream,			/* >> */
						AEGP_StreamRefH 	*mask_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_DisposeStream)(
						AEGP_StreamRefH 	streamH);				/* >> */

	SPAPI A_Err	(*AEGP_GetStreamName)(
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			force_englishB,			/* >> */
						A_char				*nameZ);				/* << space for A_char[AEGP_MAX_STREAM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetStreamUnitsText)(
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			force_englishB,			/* >> */
						A_char				*unitsZ);				/* << space for A_char[AEGP_MAX_STREAM_UNITS_SIZE] */

	SPAPI A_Err	(*AEGP_GetStreamProperties)(
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamFlags 	*flagsP,				/* << */
						A_FpLong 			*minP0,					/* << */
						A_FpLong 			*maxP0);				/* << */

	SPAPI A_Err	(*AEGP_IsStreamTimevarying)(						/* takes expressions into account */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			*is_timevaryingPB);		/* << */

	SPAPI A_Err	(*AEGP_GetStreamType)(
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamType		*stream_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetNewStreamValue)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time		*timePT,				/* >> */
						A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
						AEGP_StreamValue	*valueP);				/* << must be disposed */

	SPAPI A_Err	(*AEGP_DisposeStreamValue)(
						AEGP_StreamValue	*valueP);				/* <> */


	SPAPI A_Err	(*AEGP_SetStreamValue)(								// only legal to call when AEGP_GetStreamNumKFs==0 or NO_DATA
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamValue	*valueP);				/* << */

	// this is only valid on streams with primitive types. It is illegal on
	// AEGP_ArbBlockVal || AEGP_MarkerValP || AEGP_MaskOutlineValH

	SPAPI A_Err	(*AEGP_GetLayerStreamValue)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time		*timePT,				/* >> */
						A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
						AEGP_StreamVal	*stream_valP,			/* << */
						AEGP_StreamType 	*stream_typeP0);		/* << */

	SPAPI A_Err	(*AEGP_GetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			*enabledPB);			/* >> */

	SPAPI A_Err	(*AEGP_SetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			enabledB);				/* >> */

	SPAPI A_Err	(*AEGP_GetExpression)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_MemHandle		*expressionHZ);			/* << must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err	(*AEGP_SetExpression)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						const A_char*			expressionP);			/* >> not adopted */

	SPAPI A_Err (*AEGP_DuplicateStreamRef)(					// must dispose yourself
						AEGP_PluginID		aegp_plugin_id,			// in
						AEGP_StreamRefH		streamH,				// in
						AEGP_StreamRefH		*dup_streamPH);			// out
} AEGP_StreamSuite2;

#define kAEGPStreamSuite				"AEGP Stream Suite"
#define kAEGPStreamSuiteVersion1		4 /* frozen in AE 5.0 */

typedef struct AEGP_StreamSuite1 {

	SPAPI A_Err (*AEGP_IsStreamLegal)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						A_Boolean*			is_legalP);				/* << */


	SPAPI A_Err (*AEGP_CanVaryOverTime)(
						AEGP_StreamRefH streamH,					/* >> */
						A_Boolean* can_varyPB);						/* << */


	SPAPI A_Err	(*AEGP_GetNewLayerStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetEffectNumParamStreams)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						A_long				*num_paramsPL);			/* << */

	SPAPI A_Err	(*AEGP_GetNewEffectStreamByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1],  where 0 is the effect's input layer */
						AEGP_StreamRefH 	*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewMaskStream)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_MaskRefH		mask_refH,				/* >> */
						AEGP_MaskStream		which_stream,			/* >> */
						AEGP_StreamRefH 	*mask_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_DisposeStream)(
						AEGP_StreamRefH 	streamH);				/* >> */

	SPAPI A_Err	(*AEGP_GetStreamName)(
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			force_englishB,			/* >> */
						A_char				*nameZ);				/* << space for A_char[AEGP_MAX_STREAM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetStreamUnitsText)(
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			force_englishB,			/* >> */
						A_char				*unitsZ);				/* << space for A_char[AEGP_MAX_STREAM_UNITS_SIZE] */

	SPAPI A_Err	(*AEGP_GetStreamProperties)(
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamFlags 	*flagsP,				/* << */
						A_FpLong 			*minP0,					/* << */
						A_FpLong 			*maxP0);				/* << */

	SPAPI A_Err	(*AEGP_IsStreamTimevarying)(						/* takes expressions into account */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			*is_timevaryingPB);		/* << */

	SPAPI A_Err	(*AEGP_GetStreamType)(
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamType		*stream_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetNewStreamValue)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time		*timePT,				/* >> */
						A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
						AEGP_StreamValue	*valueP);				/* << must be disposed */

	SPAPI A_Err	(*AEGP_DisposeStreamValue)(
						AEGP_StreamValue	*valueP);				/* <> */


	SPAPI A_Err	(*AEGP_SetStreamValue)(								// only legal to call when AEGP_GetStreamNumKFs==0 or NO_DATA
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_StreamValue	*valueP);				/* << */

	// this is only valid on streams with primitive types. It is illegal on
	// AEGP_ArbBlockVal || AEGP_MarkerValH || AEGP_MaskOutlineValH

	SPAPI A_Err	(*AEGP_GetLayerStreamValue)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerStream	which_stream,			/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time		*timePT,				/* >> */
						A_Boolean			pre_expressionB,		/* >> sample the stream before evaluating the expression */
						AEGP_StreamVal	*stream_valP,			/* << */
						AEGP_StreamType 	*stream_typeP0);		/* << */

	SPAPI A_Err	(*AEGP_GetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			*enabledPB);			/* >> */

	SPAPI A_Err	(*AEGP_SetExpressionState)(							/* expressions can be disabled automatically by the parser on playback */
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						A_Boolean			enabledB);				/* >> */

	SPAPI A_Err	(*AEGP_GetExpression)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						AEGP_MemHandle		*expressionHZ);			/* << must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err	(*AEGP_SetExpression)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_StreamRefH 	streamH,				/* >> */
						const A_char*			expressionP);			/* >> not adopted */


} AEGP_StreamSuite1;


#define kAEGPLayerSuiteVersion1		5		/* frozen in AE 5.0 */

typedef struct AEGP_LayerSuite1 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
						AEGP_CompH			compH,					/* >> */
						A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
						AEGP_CompH			compH,					/* >> */
						A_long				layer_indexL,			/* >> */
						AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
						AEGP_LayerH			*layerPH);				/* << only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
						AEGP_LayerH 		layerH,					/* >> */
						A_char				*layer_nameZ0,			/* << space for A_char[AEGP_MAX_LAYER_NAME_SIZE] */
						A_char				*source_nameZ0);		/* << space for A_char[AEGP_MAX_LAYER_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
						AEGP_LayerH			layerH,					// >>
						AEGP_LayerFlags		single_flag,			// >>
						A_Boolean			valueB);				// >>

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time 				*in_pointPT,	/* >> */
						const A_Time 				*durationPT);	/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Time 				*offsetPT);		/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Ratio 			*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
						AEGP_LayerH 			layerH,				/* >> */
						AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(						/* UNDOABLE */
						AEGP_LayerH 			layerH,				/* >> */
						const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
						AEGP_ItemH 			item_to_addH,			/* >> */
						AEGP_CompH 			into_compH,				/* >> */
						A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
						AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
						AEGP_CompH 			into_compH,				/* >> */
						AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
						AEGP_LayerH 		layerH,			/* >> */
						AEGP_LTimeMode		time_mode,		/* >> */
						const A_Time	 	*timePT,		/* >> */
						A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_LTimeMode		time_mode,			/* >> */
						const A_Time	 	*timePT,			/* >> */
						A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			fill_must_be_activeB,	/* >> */
						A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timeP,					/* >> */
					A_Time			*layer_timeP); 					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
						AEGP_LayerH				layerH,				/* >> */
						AEGP_LayerIDVal				*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*tranform);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*view_timeP,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*tranform);			/* >> */

	SPAPI A_Err (*AEGP_SetLayerName)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_char			*new_nameZ);		/* >> */

} AEGP_LayerSuite1;

#define kAEGPLayerSuiteVersion2		7	/* frozen in AE 5.5 */

typedef struct AEGP_LayerSuite2 {

	SPAPI A_Err	(*AEGP_GetCompNumLayers)(
						AEGP_CompH			compH,					/* >> */
						A_long				*num_layersPL);			/* << */

	SPAPI A_Err	(*AEGP_GetCompLayerByIndex)(
						AEGP_CompH			compH,					/* >> */
						A_long				layer_indexL,			/* >> */
						AEGP_LayerH			*layerPH);				/* << */

	SPAPI A_Err	(*AEGP_GetActiveLayer)(
						AEGP_LayerH			*layerPH);				/* << only if one layer is selected */

	SPAPI A_Err	(*AEGP_GetLayerIndex)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*layer_indexPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerSourceItem)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_ItemH			*source_itemPH);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerParentComp)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_CompH			*compPH);				/* << */

	SPAPI A_Err	(*AEGP_GetLayerName)(
						AEGP_LayerH 		layerH,					/* >> */
						A_char				*layer_nameZ0,			/* << space for A_char[AEGP_MAX_LAYER_NAME_SIZE] */
						A_char				*source_nameZ0);		/* << space for A_char[AEGP_MAX_LAYER_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetLayerQuality)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	*qualityP);				/* << */

	SPAPI A_Err	(*AEGP_SetLayerQuality)(							/* UNDOABLE */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerQuality	quality);				/* >> */

	SPAPI A_Err	(*AEGP_GetLayerFlags)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		*layer_flagsP);			/* << */

	SPAPI A_Err (*AEGP_SetLayerFlag)(
						AEGP_LayerH			layerH,					/* >> */
						AEGP_LayerFlags		single_flag,			/* >> */
						A_Boolean			valueB);				/* >> */

	SPAPI A_Err	(*AEGP_IsLayerVideoReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_IsLayerAudioReallyOn)(						// accounts for solo status of other layers in comp
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*onPB);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerCurrentTime)(						// not updated while rendering
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*curr_timePT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerInPoint)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*in_pointPT);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerDuration)(
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						A_Time 				*durationPT);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerInPointAndDuration)(					/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						AEGP_LTimeMode		time_mode,				/* >> */
						const A_Time 		*in_pointPT,			/* >> */
						const A_Time 		*durationPT);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerOffset)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Time 				*offsetPT);				/* << always in comp time */

	SPAPI A_Err	(*AEGP_SetLayerOffset)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Time 		*offsetPT);				/* >> always in comp time */

	SPAPI A_Err	(*AEGP_GetLayerStretch)(
						AEGP_LayerH 		layerH,					/* >> */
						A_Ratio 			*stretchPRt);			/* << */

	SPAPI A_Err	(*AEGP_SetLayerStretch)(							/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						const A_Ratio 		*stretchPRt);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerTransferMode)(
						AEGP_LayerH 			layerH,				/* >> */
						AEGP_LayerTransferMode	*transfer_modeP);	/* << */

	SPAPI A_Err	(*AEGP_SetLayerTransferMode)(								/* UNDOABLE */
						AEGP_LayerH 					layerH,				/* >> */
						const AEGP_LayerTransferMode	*transfer_modeP);	/* >> */

	SPAPI A_Err	(*AEGP_IsAddLayerValid)(
						AEGP_ItemH 			item_to_addH,			/* >> */
						AEGP_CompH 			into_compH,				/* >> */
						A_Boolean			*validPB);				/* << */

	SPAPI A_Err	(*AEGP_AddLayer)(									/* UNDOABLE */
						AEGP_ItemH 			item_to_addH,			/* >> check AEGP_IsAddLayerValid() before using */
						AEGP_CompH 			into_compH,				/* >> */
						AEGP_LayerH			*added_layerPH0);		/* << */

	SPAPI A_Err	(*AEGP_ReorderLayer)(								/* UNDOABLE */
						AEGP_LayerH 		layerH,					/* >> */
						A_long 				layer_indexL);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerMaskedBounds)(
						AEGP_LayerH 		layerH,			/* >> */
						AEGP_LTimeMode		time_mode,		/* >> */
						const A_Time	 	*timePT,		/* >> */
						A_FloatRect			*boundsPR);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerObjectType)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_ObjectType 	*object_type);		/* << */

	SPAPI A_Err	(*AEGP_IsLayer3D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_3DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsLayer2D)(
						AEGP_LayerH 		layerH,				/* >> */
						A_Boolean 			*is_2DPB);			/* << */

	SPAPI A_Err	(*AEGP_IsVideoActive)(
						AEGP_LayerH 		layerH,				/* >> */
						AEGP_LTimeMode		time_mode,			/* >> */
						const A_Time	 	*timePT,			/* >> */
						A_Boolean			*is_activePB);		/* << */

	SPAPI A_Err	(*AEGP_IsLayerUsedAsTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			fill_must_be_activeB,	/* >> */
						A_Boolean			*is_track_mattePB);		/* << */

	SPAPI A_Err	(*AEGP_DoesLayerHaveTrackMatte)(
						AEGP_LayerH			layerH,					/* >> */
						A_Boolean			*has_track_mattePB);	/* << */

	SPAPI A_Err	(*AEGP_ConvertCompToLayerTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_Time			*layer_timePT);					/* << */

	SPAPI A_Err	(*AEGP_ConvertLayerToCompTime)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*layer_timePT,					/* >> */
					A_Time			*comp_timePT) ;					/* << */

	SPAPI A_Err	(*AEGP_GetLayerDancingRandValue)(
					AEGP_LayerH		layerH,							/* >> */
					const A_Time	*comp_timePT,					/* >> */
					A_long			*rand_valuePL);					/* << */

	SPAPI A_Err	(*AEGP_GetLayerID)(
						AEGP_LayerH				layerH,				/* >> */
						AEGP_LayerIDVal			*id_valP);			/* << */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXform)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*tranform);			/* >> */

	SPAPI A_Err	(*AEGP_GetLayerToWorldXformFromView)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_Time			*view_timeP,		/* >> */
				const A_Time			*comp_timeP,		/* >> */
				A_Matrix4				*tranform);			/* >> */

	SPAPI A_Err (*AEGP_SetLayerName)(
				AEGP_LayerH				aegp_layerH,		/* >> */
				const A_char			*new_nameZ);		/* >> */

	SPAPI A_Err	(*AEGP_GetLayerParent)(
				const AEGP_LayerH		layerH,				/* >> */
				AEGP_LayerH				*parent_layerPH);	/* << NULL if no parent */

	SPAPI A_Err (*AEGP_SetLayerParent)(
				AEGP_LayerH				layerH,				/* >> */
				const AEGP_LayerH		parent_layerH);		/* >> */


} AEGP_LayerSuite2;




#define kAEGPEffectSuite				"AEGP Effect Suite"
#define kAEGPEffectSuiteVersion1		1 /* frozen in AE 5.5 */

typedef struct AEGP_EffectSuite1 {

	SPAPI A_Err	(*AEGP_GetLayerNumEffects)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*num_effectsPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerEffectByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_EffectIndex	layer_effect_indexL,	/* >> */
						AEGP_EffectRefH		*effectPH);				/* <<  MUST dispose with DisposeEffect*/

	SPAPI A_Err (*AEGP_GetInstalledKeyFromLayerEffect)(
						AEGP_EffectRefH		effect_refH,					/* >> */
						AEGP_InstalledEffectKey	*installed_effect_keyP);	/* << */

	SPAPI A_Err (*AEGP_GetEffectParamUnionByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
						PF_ParamType		*param_typeP,			/* << */
						PF_ParamDefUnion	*uP0);					/* << DO NOT USE THE VALUE FROM THIS PARAMDEF! */

	SPAPI A_Err (*AEGP_GetEffectFlags)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						AEGP_EffectFlags	*effect_flagsP);		/* << */

	SPAPI A_Err (*AEGP_SetEffectFlags)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						AEGP_EffectFlags	effect_flags_set_mask,	/* >> */
						AEGP_EffectFlags	effect_flags);			/* >> */

	SPAPI A_Err	(*AEGP_ReorderEffect)(								/* UNDOABLE */
						AEGP_EffectRefH 	effect_refH,			/* >> */
						A_long 				effect_indexL);			/* >> */

	SPAPI A_Err (*AEGP_EffectCallGeneric)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						const A_Time		*timePT,				/* >> Use the timebase of the layer to which effect is applied. */
						void				*effect_extraPV);		/* <> */

	SPAPI A_Err (*AEGP_DisposeEffect)(
						AEGP_EffectRefH	effect_refH );				/* >> */

	SPAPI A_Err (*AEGP_ApplyEffect)(
						AEGP_PluginID		aegp_plugin_id,				/* >> */
						AEGP_LayerH			layerH,						/* >> */
						AEGP_InstalledEffectKey	installed_effect_key,	/* >> */
						AEGP_EffectRefH		*effect_refPH);				/* <<  MUST BE DISPOSED with AEGP_DisposeEffect */

	SPAPI A_Err	(*AEGP_DeleteLayerEffect)(
						AEGP_EffectRefH 	effect_refH);				/* >>  undoable */

	SPAPI A_Err (*AEGP_GetNumInstalledEffects)(
						A_long 				*num_installed_effectsPL);	/* << */

	// pass in AEGP_InstalledEffectKey_NONE for installed_effect_key to get first effect key

	SPAPI A_Err (*AEGP_GetNextInstalledEffect)(
									AEGP_InstalledEffectKey	installed_effect_key,	/* >> */
									AEGP_InstalledEffectKey	*next_effectPH);		/* << */

	SPAPI A_Err	(*AEGP_GetEffectName)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*nameZ);				/* << space for A_char[AEGP_MAX_EFFECT_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetEffectMatchName)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*match_nameZ);			/* << space for A_char[AEGP_MAX_EFFECT_MATCH_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetEffectCategory)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*categoryZ);			/* << space for A_char[AEGP_MAX_EFFECT_CATEGORY_NAME_SIZE] */


} AEGP_EffectSuite1;




#define kAEGPEffectSuiteVersion2		2 /* frozen in AE 6.5 */

typedef struct AEGP_EffectSuite2 {

	SPAPI A_Err	(*AEGP_GetLayerNumEffects)(
						AEGP_LayerH			layerH,					/* >> */
						A_long				*num_effectsPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerEffectByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_LayerH			layerH,					/* >> */
						AEGP_EffectIndex	layer_effect_indexL,	/* >> */
						AEGP_EffectRefH		*effectPH);				/* <<  MUST dispose with DisposeEffect*/

	SPAPI A_Err (*AEGP_GetInstalledKeyFromLayerEffect)(
						AEGP_EffectRefH		effect_refH,					/* >> */
						AEGP_InstalledEffectKey	*installed_effect_keyP);	/* << */

	SPAPI A_Err (*AEGP_GetEffectParamUnionByIndex)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
						PF_ParamType		*param_typeP,			/* << */
						PF_ParamDefUnion	*uP0);					/* << DO NOT USE THE VALUE FROM THIS PARAMDEF! */

	SPAPI A_Err (*AEGP_GetEffectFlags)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						AEGP_EffectFlags	*effect_flagsP);		/* << */

	SPAPI A_Err (*AEGP_SetEffectFlags)(
						AEGP_EffectRefH		effect_refH,			/* >> */
						AEGP_EffectFlags	effect_flags_set_mask,	/* >> */
						AEGP_EffectFlags	effect_flags);			/* >> */

	SPAPI A_Err	(*AEGP_ReorderEffect)(								/* UNDOABLE */
						AEGP_EffectRefH 	effect_refH,			/* >> */
						A_long 				effect_indexL);			/* >> */

	SPAPI A_Err (*AEGP_EffectCallGeneric)(
						AEGP_PluginID		aegp_plugin_id,			/* >> */
						AEGP_EffectRefH		effect_refH,			/* >> */
						const A_Time		*timePT,				/* >> Use the timebase of the layer to which effect is applied. */
						void				*effect_extraPV);		/* <> */

	SPAPI A_Err (*AEGP_DisposeEffect)(
						AEGP_EffectRefH	effect_refH );				/* >> */

	SPAPI A_Err (*AEGP_ApplyEffect)(
						AEGP_PluginID		aegp_plugin_id,				/* >> */
						AEGP_LayerH			layerH,						/* >> */
						AEGP_InstalledEffectKey	installed_effect_key,	/* >> */
						AEGP_EffectRefH		*effect_refPH);				/* <<  MUST BE DISPOSED with AEGP_DisposeEffect */

	SPAPI A_Err	(*AEGP_DeleteLayerEffect)(
						AEGP_EffectRefH 	effect_refH);				/* >>  undoable */

	SPAPI A_Err (*AEGP_GetNumInstalledEffects)(
						A_long 				*num_installed_effectsPL);	/* << */

	// pass in AEGP_InstalledEffectKey_NONE for installed_effect_key to get first effect key

	SPAPI A_Err (*AEGP_GetNextInstalledEffect)(
									AEGP_InstalledEffectKey	installed_effect_key,	/* >> */
									AEGP_InstalledEffectKey	*next_effectPH);		/* << */

	SPAPI A_Err	(*AEGP_GetEffectName)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*nameZ);				/* << space for A_char[AEGP_MAX_EFFECT_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetEffectMatchName)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*match_nameZ);			/* << space for A_char[AEGP_MAX_EFFECT_MATCH_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetEffectCategory)(
						AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
						A_char						*categoryZ);			/* << space for A_char[AEGP_MAX_EFFECT_CATEGORY_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_DuplicateEffect)(
						AEGP_EffectRefH		original_effect_refH,			/* >> */
						AEGP_EffectRefH		*duplicate_effect_refPH);		/* << */

} AEGP_EffectSuite2;


#define kAEGPEffectSuiteVersion3		3 /* frozen in AE 7.0 */

typedef struct AEGP_EffectSuite3 {

	SPAPI A_Err	(*AEGP_GetLayerNumEffects)(
		AEGP_LayerH			layerH,					/* >> */
		A_long				*num_effectsPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerEffectByIndex)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_LayerH			layerH,					/* >> */
		AEGP_EffectIndex	layer_effect_indexL,	/* >> */
		AEGP_EffectRefH		*effectPH);				/* <<  MUST dispose with DisposeEffect*/

	SPAPI A_Err (*AEGP_GetInstalledKeyFromLayerEffect)(
		AEGP_EffectRefH		effect_refH,					/* >> */
		AEGP_InstalledEffectKey	*installed_effect_keyP);	/* << */

	SPAPI A_Err (*AEGP_GetEffectParamUnionByIndex)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_EffectRefH		effect_refH,			/* >> */
		PF_ParamIndex		param_index,			/* >> valid in range [0 to AEGP_GetEffectNumParamStreams - 1], where 0 is the effect's input layer */
		PF_ParamType		*param_typeP,			/* << */
		PF_ParamDefUnion	*uP0);					/* << DO NOT USE THE VALUE FROM THIS PARAMDEF! */

	SPAPI A_Err (*AEGP_GetEffectFlags)(
		AEGP_EffectRefH		effect_refH,			/* >> */
		AEGP_EffectFlags	*effect_flagsP);		/* << */

	SPAPI A_Err (*AEGP_SetEffectFlags)(
		AEGP_EffectRefH		effect_refH,			/* >> */
		AEGP_EffectFlags	effect_flags_set_mask,	/* >> */
		AEGP_EffectFlags	effect_flags);			/* >> */

	SPAPI A_Err	(*AEGP_ReorderEffect)(								/* UNDOABLE */
		AEGP_EffectRefH 	effect_refH,			/* >> */
		A_long 				effect_indexL);			/* >> */

	/** new command parameter addded. To get old behaviour pass in PF_Cmd_COMPLETELY_GENERAL for effect_command **/
	SPAPI A_Err (*AEGP_EffectCallGeneric)(
		AEGP_PluginID		aegp_plugin_id,			/* >> */
		AEGP_EffectRefH		effect_refH,			/* >> */
		const A_Time		*timePT,				/* >> Use the timebase of the layer to which effect is applied. */
		PF_Cmd				effect_cmd,				/* >> new parameter from version 2 */
		void				*effect_extraPV);		/* <> */

	SPAPI A_Err (*AEGP_DisposeEffect)(
		AEGP_EffectRefH	effect_refH );				/* >> */

	SPAPI A_Err (*AEGP_ApplyEffect)(
		AEGP_PluginID		aegp_plugin_id,				/* >> */
		AEGP_LayerH			layerH,						/* >> */
		AEGP_InstalledEffectKey	installed_effect_key,	/* >> */
		AEGP_EffectRefH		*effect_refPH);				/* <<  MUST BE DISPOSED with AEGP_DisposeEffect */

	SPAPI A_Err	(*AEGP_DeleteLayerEffect)(
		AEGP_EffectRefH 	effect_refH);				/* >>  undoable */

	SPAPI A_Err (*AEGP_GetNumInstalledEffects)(
		A_long 				*num_installed_effectsPL);	/* << */

	// pass in AEGP_InstalledEffectKey_NONE for installed_effect_key to get first effect key

	SPAPI A_Err (*AEGP_GetNextInstalledEffect)(
		AEGP_InstalledEffectKey	installed_effect_key,	/* >> */
		AEGP_InstalledEffectKey	*next_effectPH);		/* << */

	SPAPI A_Err	(*AEGP_GetEffectName)(
		AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
		A_char						*nameZ);				/* << space for A_char[AEGP_MAX_EFFECT_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetEffectMatchName)(
		AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
		A_char						*match_nameZ);			/* << space for A_char[AEGP_MAX_EFFECT_MATCH_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetEffectCategory)(
		AEGP_InstalledEffectKey		installed_effect_key,	/* >> */
		A_char						*categoryZ);			/* << space for A_char[AEGP_MAX_EFFECT_CATEGORY_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_DuplicateEffect)(
		AEGP_EffectRefH		original_effect_refH,			/* >> */
		AEGP_EffectRefH		*duplicate_effect_refPH);		/* << */
} AEGP_EffectSuite3;

#define kAEGPLightSuite				"AEGP Light Suite"
#define kAEGPLightSuiteVersion1		1 /* frozen in AE 5.0 */



typedef struct AEGP_LightSuite1 {

	SPAPI A_Err	(*AEGP_GetLightType)(
						AEGP_LayerH				light_layerH,		/* >> */
						AEGP_LightType			*light_typeP);		/* << */


} AEGP_LightSuite1;



#define kAEGPMaskSuite					"AEGP Layer Mask Suite"
#define kAEGPMaskSuiteVersion5			6 /* frozen AE 10 */

typedef struct AEGP_MaskSuite5 {

	SPAPI A_Err	(*AEGP_GetLayerNumMasks)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						A_long					*num_masksPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerMaskByIndex)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						AEGP_MaskIndex			mask_indexL,		/* >> */
						AEGP_MaskRefH			*maskPH);			/* << must be disposed by calling AEGP_DisposeMask() */

	SPAPI A_Err	(*AEGP_DisposeMask)(
						AEGP_MaskRefH			mask_refH);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskInvert)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_Boolean				*invertPB);			/* << */

	SPAPI A_Err (*AEGP_SetMaskInvert)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_Boolean				invertB);			/* << */

	SPAPI A_Err	(*AEGP_GetMaskMode)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						PF_MaskMode				*modeP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskMode)(
						AEGP_MaskRefH			maskH,				/* >> */
						PF_MaskMode				mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskMotionBlurState)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskMBlur			*blur_stateP);		/* << */

	SPAPI A_Err (*AEGP_SetMaskMotionBlurState)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskMBlur			blur_state);		/* >> */

	// AEGP_GetMaskName/SetMaskName are obsoleted. Use AEGP_GetNewDynamicStreamForMask
	// and the name functions there

	SPAPI A_Err	(*AEGP_GetMaskID)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskIDVal			*id_valP);			/* << */

	SPAPI A_Err (*AEGP_CreateNewMask)(								/* UNDOABLE */
						AEGP_LayerH				layerH,				/* >> */
						AEGP_MaskRefH			*mask_refPH,		/* << */
						A_long					*mask_indexPL0);	/* << */

	SPAPI A_Err (*AEGP_DeleteMaskFromLayer)(						/* UNDOABLE */
						AEGP_MaskRefH			mask_refH);			/* >> still need to Dispose MaskRefH */

	SPAPI A_Err (*AEGP_GetMaskColor)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_ColorVal			*colorP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskColor)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						const AEGP_ColorVal		*colorP);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskLockState)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				*is_lockedPB);		/* >> */

	SPAPI A_Err	(*AEGP_SetMaskLockState)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				lockB);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskIsRotoBezier)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				*is_roto_bezierPB);	/* << */

	SPAPI A_Err (*AEGP_SetMaskIsRotoBezier)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				is_roto_bezierB);	/* >> */

	SPAPI A_Err (*AEGP_DuplicateMask)(
						AEGP_MaskRefH			orig_mask_refH,			/* >> */
						AEGP_MaskRefH			*duplicate_mask_refPH);	/* << */

} AEGP_MaskSuite5;

#define kAEGPMaskSuiteVersion4			5 /* frozen AE 6.5 */

typedef struct AEGP_MaskSuite4 {

	SPAPI A_Err	(*AEGP_GetLayerNumMasks)(
		AEGP_LayerH				aegp_layerH,		/* >> */
		A_long					*num_masksPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerMaskByIndex)(
		AEGP_LayerH				aegp_layerH,		/* >> */
		AEGP_MaskIndex			mask_indexL,		/* >> */
		AEGP_MaskRefH			*maskPH);			/* << must be disposed by calling AEGP_DisposeMask() */

	SPAPI A_Err	(*AEGP_DisposeMask)(
		AEGP_MaskRefH			mask_refH);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskInvert)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		A_Boolean				*invertPB);			/* << */

	SPAPI A_Err (*AEGP_SetMaskInvert)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		A_Boolean				invertB);			/* << */

	SPAPI A_Err	(*AEGP_GetMaskMode)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		PF_MaskMode				*modeP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskMode)(
		AEGP_MaskRefH			maskH,				/* >> */
		PF_MaskMode				mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskMotionBlurState)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		AEGP_MaskMBlur			*blur_stateP);		/* << */

	SPAPI A_Err (*AEGP_SetMaskMotionBlurState)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		AEGP_MaskMBlur			blur_state);		/* >> */

	SPAPI A_Err	(*AEGP_GetMaskName)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		A_char					*nameZ);			/* << space for A_char[AEGP_MAX_MASK_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_SetMaskName)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		A_char					*nameZ);			/* >> space for A_char[AEGP_MAX_MASK_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetMaskID)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		AEGP_MaskIDVal			*id_valP);			/* << */

	SPAPI A_Err (*AEGP_CreateNewMask)(								/* UNDOABLE */
		AEGP_LayerH				layerH,				/* >> */
		AEGP_MaskRefH			*mask_refPH,		/* << */
		A_long					*mask_indexPL0);	/* << */

	SPAPI A_Err (*AEGP_DeleteMaskFromLayer)(						/* UNDOABLE */
		AEGP_MaskRefH			mask_refH);			/* >> still need to Dispose MaskRefH */

	SPAPI A_Err (*AEGP_GetMaskColor)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		AEGP_ColorVal			*colorP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskColor)(
		AEGP_MaskRefH			mask_refH,			/* >> */
		const AEGP_ColorVal		*colorP);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskLockState)(
		AEGP_MaskRefH			mask_refH,			/* <> */
		A_Boolean				*is_lockedPB);		/* >> */

	SPAPI A_Err	(*AEGP_SetMaskLockState)(
		AEGP_MaskRefH			mask_refH,			/* <> */
		A_Boolean				lockB);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskIsRotoBezier)(
		AEGP_MaskRefH			mask_refH,			/* <> */
		A_Boolean				*is_roto_bezierPB);	/* << */

	SPAPI A_Err (*AEGP_SetMaskIsRotoBezier)(
		AEGP_MaskRefH			mask_refH,			/* <> */
		A_Boolean				is_roto_bezierB);	/* >> */

	SPAPI A_Err (*AEGP_DuplicateMask)(
		AEGP_MaskRefH			orig_mask_refH,			/* >> */
		AEGP_MaskRefH			*duplicate_mask_refPH);	/* << */

} AEGP_MaskSuite4;

#define kAEGPMaskSuiteVersion3			4 /* frozen AE 6.0 */

typedef struct AEGP_MaskSuite3 {

	SPAPI A_Err	(*AEGP_GetLayerNumMasks)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						A_long					*num_masksPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerMaskByIndex)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						AEGP_MaskIndex			mask_indexL,		/* >> */
						AEGP_MaskRefH			*maskPH);			/* << must be disposed by calling AEGP_DisposeMask() */

	SPAPI A_Err	(*AEGP_DisposeMask)(
						AEGP_MaskRefH			mask_refH);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskInvert)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_Boolean				*invertPB);			/* << */

	SPAPI A_Err (*AEGP_SetMaskInvert)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_Boolean				invertB);			/* << */

	SPAPI A_Err	(*AEGP_GetMaskMode)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						PF_MaskMode				*modeP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskMode)(
						AEGP_MaskRefH			maskH,				/* >> */
						PF_MaskMode				mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskMotionBlurState)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskMBlur			*blur_stateP);		/* << */

	SPAPI A_Err (*AEGP_SetMaskMotionBlurState)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskMBlur			blur_state);		/* >> */

	SPAPI A_Err	(*AEGP_GetMaskName)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_char					*nameZ);			/* << space for A_char[AEGP_MAX_MASK_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_SetMaskName)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_char					*nameZ);			/* >> space for A_char[AEGP_MAX_MASK_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetMaskID)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskIDVal			*id_valP);			/* << */

	SPAPI A_Err (*AEGP_CreateNewMask)(								/* UNDOABLE */
						AEGP_LayerH				layerH,				/* >> */
						AEGP_MaskRefH			*mask_refPH,		/* << */
						A_long					*mask_indexPL0);	/* << */

	SPAPI A_Err (*AEGP_DeleteMaskFromLayer)(						/* UNDOABLE */
						AEGP_MaskRefH			mask_refH);			/* >> still need to Dispose MaskRefH */

	SPAPI A_Err (*AEGP_GetMaskColor)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_ColorVal			*colorP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskColor)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						const AEGP_ColorVal		*colorP);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskLockState)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				*is_lockedPB);		/* >> */

	SPAPI A_Err	(*AEGP_SetMaskLockState)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				lockB);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskIsRotoBezier)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				*is_roto_bezierPB);	/* << */

	SPAPI A_Err (*AEGP_SetMaskIsRotoBezier)(
						AEGP_MaskRefH			mask_refH,			/* <> */
						A_Boolean				is_roto_bezierB);	/* >> */

} AEGP_MaskSuite3;

#define kAEGPMaskSuiteVersion1			2 /* frozen in AE 5.0 */

typedef struct AEGP_MaskSuite1 {

	SPAPI A_Err	(*AEGP_GetLayerNumMasks)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						A_long					*num_masksPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerMaskByIndex)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						AEGP_MaskIndex			mask_indexL,		/* >> */
						AEGP_MaskRefH			*maskPH);			/* << must be disposed by calling AEGP_DisposeMask() */

	SPAPI A_Err	(*AEGP_DisposeMask)(
						AEGP_MaskRefH			mask_refH);				/* >> */

	SPAPI A_Err	(*AEGP_GetMaskInvert)(
						AEGP_MaskRefH			mask_refH,				/* >> */
						A_Boolean				*invertPB);			/* << */

	SPAPI A_Err	(*AEGP_GetMaskMode)(
						AEGP_MaskRefH			mask_refH,				/* >> */
						PF_MaskMode				*modeP);			/* << */

	SPAPI A_Err	(*AEGP_GetMaskName)(
						AEGP_MaskRefH			mask_refH,				/* >> */
						A_char					*nameZ);			/* << space for A_char[AEGP_MAX_MASK_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetMaskID)(
						AEGP_MaskRefH			mask_refH,				/* >> */
						AEGP_MaskIDVal			*id_valP);			/* << */

	SPAPI A_Err (*AEGP_CreateNewMask)(			//undoable
						AEGP_LayerH				layerH,			/* >> */
						AEGP_MaskRefH			*mask_refPH,	/* << */
						A_long					*mask_indexPL0);/* << */

	SPAPI A_Err (*AEGP_DeleteMaskFromLayer)(	//undoable
						AEGP_MaskRefH			mask_refH);		/* >> still need to Dispose MaskRefH */

} AEGP_MaskSuite1;

#define kAEGPMaskSuiteVersion2			3 /* frozen in AE 5.5 */

typedef struct AEGP_MaskSuite2 {

	SPAPI A_Err	(*AEGP_GetLayerNumMasks)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						A_long					*num_masksPL);		/* << */

	SPAPI A_Err	(*AEGP_GetLayerMaskByIndex)(
						AEGP_LayerH				aegp_layerH,		/* >> */
						AEGP_MaskIndex			mask_indexL,		/* >> */
						AEGP_MaskRefH			*maskPH);			/* << must be disposed by calling AEGP_DisposeMask() */

	SPAPI A_Err	(*AEGP_DisposeMask)(
						AEGP_MaskRefH			mask_refH);			/* >> */

	SPAPI A_Err	(*AEGP_GetMaskInvert)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_Boolean				*invertPB);			/* << */

	SPAPI A_Err (*AEGP_SetMaskInvert)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_Boolean				invertB);			/* << */

	SPAPI A_Err	(*AEGP_GetMaskMode)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						PF_MaskMode				*modeP);			/* << */

	SPAPI A_Err (*AEGP_SetMaskMode)(
						AEGP_MaskRefH			maskH,				/* >> */
						PF_MaskMode				mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMaskMotionBlurState)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskMBlur			*blur_stateP);		/* << */

	SPAPI A_Err (*AEGP_SetMaskMotionBlurState)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskMBlur			blur_state);		/* >> */

	SPAPI A_Err	(*AEGP_GetMaskName)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						A_char					*nameZ);			/* << space for A_char[AEGP_MAX_MASK_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetMaskID)(
						AEGP_MaskRefH			mask_refH,			/* >> */
						AEGP_MaskIDVal			*id_valP);			/* << */

	SPAPI A_Err (*AEGP_CreateNewMask)(								//undoable
						AEGP_LayerH				layerH,				/* >> */
						AEGP_MaskRefH			*mask_refPH,		/* << */
						A_long					*mask_indexPL0);	/* << */

	SPAPI A_Err (*AEGP_DeleteMaskFromLayer)(						//undoable
						AEGP_MaskRefH			mask_refH);			/* >> still need to Dispose MaskRefH */

} AEGP_MaskSuite2;
/**
 ** Camera Suite
 **
 **/
#define kAEGPCameraSuite				"AEGP Camera Suite"
#define kAEGPCameraSuiteVersion1		1 /* frozen in AE 5.0 */



typedef struct AEGP_CameraSuite1 {

		SPAPI A_Err	(*AEGP_GetCamera)(
						PR_RenderContextH 		render_contextH,   	/* >> */
						const A_Time			*comp_timeP,		/* >> */
						AEGP_LayerH				*camera_layerPH);	/* << */

		SPAPI A_Err	(*AEGP_GetCameraType)(
						AEGP_LayerH				camera_layerH,		/* >> */
						AEGP_CameraType			*camera_typeP);		/* << */


		SPAPI A_Err (*AEGP_GetDefaultCameraDistanceToImagePlane)(
					AEGP_CompH				compH,					/* >> */
					A_FpLong				*dist_to_planePF);		/* << */

} AEGP_CameraSuite1;


#define kAEGPItemSuite				"AEGP Item Suite"

#define kAEGPItemSuiteVersion8		13	/* frozen in AE 9.0 */

typedef struct AEGP_ItemSuite8 {

	SPAPI A_Err	(*AEGP_GetFirstProjItem)(
						AEGP_ProjectH		projectH,				/* >> */
						AEGP_ItemH			*itemPH);					/* << */

	SPAPI A_Err	(*AEGP_GetNextProjItem)(
						AEGP_ProjectH		projectH,				/* >> */
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
						AEGP_ItemH			*itemPH);				/* << NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			selectB,				/* >>	allows to select or deselect the item */
						A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
						AEGP_ItemType		item_type,				/* << */
						A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
						AEGP_PluginID			pluginID,				// in
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_MemHandle		*unicode_namePH);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_SetItemName)(								/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> */
						const A_UTF16Char	*nameZ);				/* >> null terminated UTF16 */

	SPAPI A_Err	(*AEGP_GetItemID)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
						A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*parent_folder_itemPH);	/* << */

	SPAPI A_Err	(*AEGP_SetItemParentFolder)(
						AEGP_ItemH			itemH,					/* <> */
						AEGP_ItemH			parent_folder_itemH);	/* >> */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
						AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
						A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
																	/*			Folder			-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
						AEGP_ItemH 			itemH,					/* >>	*/
						A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long	 			*widthPL,				/* << */
						A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
						AEGP_ItemH 			itemH,					/* >> */
						A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
						AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
						const A_UTF16Char	*nameZ,				/* >> null terminated UTF16 */
						AEGP_ItemH			parent_folderH0,		/* >> */
						AEGP_ItemH			*new_folderPH);			/* << allocated and owned by AE */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* UNDOABLE. Use the item's native timespace */
						AEGP_ItemH 			itemH,					/* >> */
						const A_Time		*new_timePT);			/* >> */

	SPAPI A_Err (*AEGP_GetItemCommentLength)(
						AEGP_ItemH			itemH, 					/* >> */
						A_u_long			*buf_sizePLu);			/* << */

	SPAPI A_Err	(*AEGP_GetItemComment)(
						AEGP_ItemH			itemH,					/* >> */
						A_u_long			buf_sizeLu,				/* >> */
						A_char				*commentZ);				/* << */

	SPAPI A_Err (*AEGP_SetItemComment)(
						AEGP_ItemH			itemH, 					/* >> UNDOABLE */
						const A_char			*commentZ);				/* >> */

	SPAPI A_Err (*AEGP_GetItemLabel)(
						AEGP_ItemH			itemH, 					/* >> */
						AEGP_LabelID		*labelP);				/* << */

	SPAPI A_Err (*AEGP_SetItemLabel)(
						AEGP_ItemH			itemH, 					/* >> UNDOABLE */
						AEGP_LabelID		label);					/* >> */

	SPAPI A_Err (*AEGP_GetItemMRUView)(
						AEGP_ItemH			itemH,					// >>
						AEGP_ItemViewP		*mru_viewP);			// <<

} AEGP_ItemSuite8;

#define kAEGPItemSuiteVersion7		11	/* frozen in AE 8.0 */

typedef struct AEGP_ItemSuite7 {

	SPAPI A_Err	(*AEGP_GetFirstProjItem)(
		AEGP_ProjectH		projectH,				/* >> */
		AEGP_ItemH			*itemPH);					/* << */

	SPAPI A_Err	(*AEGP_GetNextProjItem)(
		AEGP_ProjectH		projectH,				/* >> */
		AEGP_ItemH			itemH,					/* >> */
		AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
		AEGP_ItemH			*itemPH);				/* << NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
		AEGP_ItemH			itemH,					/* >> */
		A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
		AEGP_ItemH			itemH,					/* >> */
		A_Boolean			selectB,				/* >>	allows to select or deselect the item */
		A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
		AEGP_ItemH 			itemH,					/* >> */
		AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
		AEGP_ItemType		item_type,				/* << */
		A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
		AEGP_ItemH 			itemH,					/* >> */
		A_char				*nameZ);				/* << space for A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_SetItemName)(								/* UNDOABLE */
		AEGP_ItemH 			itemH,					/* >> */
		const A_char		*nameZ);				/* >> up to  A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemID)(
		AEGP_ItemH 			itemH,					/* >> */
		A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
		AEGP_ItemH 			itemH,					/* >> */
		AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
		AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
		A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
		AEGP_ItemH			itemH,					/* >> */
		AEGP_ItemH			*parent_folder_itemPH);	/* << */

	SPAPI A_Err	(*AEGP_SetItemParentFolder)(
		AEGP_ItemH			itemH,					/* <> */
		AEGP_ItemH			parent_folder_itemH);	/* >> */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
		AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
		A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
	/*			Folder			-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
		AEGP_ItemH 			itemH,					/* >>	*/
		A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
		AEGP_ItemH 			itemH,					/* >> */
		A_long	 			*widthPL,				/* << */
		A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
		AEGP_ItemH 			itemH,					/* >> */
		A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
		AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
		const A_char		*nameZ,					/* >> */
		AEGP_ItemH			parent_folderH0,		/* >> */
		AEGP_ItemH			*new_folderPH);			/* << allocated and owned by AE */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* UNDOABLE. Use the item's native timespace */
		AEGP_ItemH 			itemH,					/* >> */
		const A_Time		*new_timePT);			/* >> */

	SPAPI A_Err (*AEGP_GetItemCommentLength)(
		AEGP_ItemH			itemH, 					/* >> */
		A_u_long			*buf_sizePLu);			/* << */

	SPAPI A_Err	(*AEGP_GetItemComment)(
		AEGP_ItemH			itemH,					/* >> */
		A_u_long			buf_sizeLu,				/* >> */
		A_char				*commentZ);				/* << */

	SPAPI A_Err (*AEGP_SetItemComment)(
		AEGP_ItemH			itemH, 					/* >> UNDOABLE */
		const A_char			*commentZ);				/* >> */

	SPAPI A_Err (*AEGP_GetItemLabel)(
		AEGP_ItemH			itemH, 					/* >> */
		AEGP_LabelID		*labelP);				/* << */

	SPAPI A_Err (*AEGP_SetItemLabel)(
		AEGP_ItemH			itemH, 					/* >> UNDOABLE */
		AEGP_LabelID		label);					/* >> */

	SPAPI A_Err (*AEGP_GetItemMRUView)(
		AEGP_ItemH			itemH,					// >>
		AEGP_ItemViewP		*mru_viewP);			// <<

} AEGP_ItemSuite7;


#define kAEGPItemSuiteVersion6		10	/* frozen in AE 7.0 */

typedef struct AEGP_ItemSuite6 {

	SPAPI A_Err	(*AEGP_GetFirstProjItem)(
						AEGP_ProjectH		projectH,				/* >> */
						AEGP_ItemH			*itemPH);					/* << */

	SPAPI A_Err	(*AEGP_GetNextProjItem)(
						AEGP_ProjectH		projectH,				/* >> */
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
						AEGP_ItemH			*itemPH);				/* << NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			selectB,				/* >>	allows to select or deselect the item */
						A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
						AEGP_ItemType		item_type,				/* << */
						A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
						AEGP_ItemH 			itemH,					/* >> */
						A_char				*nameZ);				/* << space for A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_SetItemName)(								/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> */
						const A_char		*nameZ);				/* >> up to  A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemID)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
						A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*parent_folder_itemPH);	/* << */

	SPAPI A_Err	(*AEGP_SetItemParentFolder)(
						AEGP_ItemH			itemH,					/* <> */
						AEGP_ItemH			parent_folder_itemH);	/* >> */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
						AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
						A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
																	/*			Folder			-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
						AEGP_ItemH 			itemH,					/* >>	*/
						A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long	 			*widthPL,				/* << */
						A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
						AEGP_ItemH 			itemH,					/* >> */
						A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
						AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
						const A_char		*nameZ,					/* >> */
						AEGP_ItemH			parent_folderH0,		/* >> */
						AEGP_ItemH			*new_folderPH);			/* << allocated and owned by AE */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* UNDOABLE. Use the item's native timespace */
						AEGP_ItemH 			itemH,					/* >> */
						const A_Time		*new_timePT);			/* >> */

	SPAPI A_Err (*AEGP_GetItemCommentLength)(
						AEGP_ItemH			itemH, 					/* >> */
						A_u_long			*buf_sizePLu);			/* << */

	SPAPI A_Err	(*AEGP_GetItemComment)(
						AEGP_ItemH			itemH,					/* >> */
						A_u_long			buf_sizeLu,				/* >> */
						A_char				*commentZ);				/* << */

	SPAPI A_Err (*AEGP_SetItemComment)(
						AEGP_ItemH			itemH, 					/* >> UNDOABLE */
						const A_char			*commentZ);				/* >> */

	SPAPI A_Err (*AEGP_GetItemLabel)(
						AEGP_ItemH			itemH, 					/* >> */
						AEGP_LabelID		*labelP);				/* << */

	SPAPI A_Err (*AEGP_SetItemLabel)(
						AEGP_ItemH			itemH, 					/* >> UNDOABLE */
						AEGP_LabelID		label);					/* >> */

} AEGP_ItemSuite6;


#define kAEGPItemSuiteVersion5		7	/* frozen in AE 6.5 */

typedef struct AEGP_ItemSuite5 {

	SPAPI A_Err	(*AEGP_GetNextItem)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
						AEGP_ItemH			*itemPH);				/* << NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			selectB,				/* >>	allows to select or deselect the item */
						A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
						AEGP_ItemType		item_type,				/* << */
						A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
						AEGP_ItemH 			itemH,					/* >> */
						A_char				*nameZ);				/* << space for A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemID)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
						A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*parent_folder_itemPH);	/* << */

	SPAPI A_Err	(*AEGP_SetItemParentFolder)(
						AEGP_ItemH			itemH,					/* <> */
						AEGP_ItemH			parent_folder_itemH);	/* >> */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
						AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
						A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
																	/*			Folder			-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
						AEGP_ItemH 			itemH,					/* >>	*/
						A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long	 			*widthPL,				/* << */
						A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
						AEGP_ItemH 			itemH,					/* >> */
						A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
						AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
						const A_char		*nameZ,					/* >> */
						AEGP_ItemH			parent_folderH0,		/* >> */
						AEGP_ItemH			*new_folderPH);			/* << allocated and owned by AE */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* UNDOABLE. Use the item's native timespace */
						AEGP_ItemH 			itemH,					/* >> */
						const A_Time		*new_timePT);			/* >> */

	SPAPI A_Err (*AEGP_GetItemCommentLength)(
						AEGP_ItemH			itemH, 					/* >> */
						A_u_long			*buf_sizePLu);			/* << */

	SPAPI A_Err	(*AEGP_GetItemComment)(
						AEGP_ItemH			itemH,					/* >> */
						A_u_long			buf_sizeLu,				/* >> */
						char				*commentZ);				/* << */

	SPAPI A_Err (*AEGP_SetItemComment)(
						AEGP_ItemH			itemH, 					/* >> UNDOABLE */
						const char			*commentZ);				/* >> */

} AEGP_ItemSuite5;


#define kAEGPItemSuiteVersion4		6	/* frozen in AE 6.0 */

typedef struct AEGP_ItemSuite4 {

	SPAPI A_Err	(*AEGP_GetNextItem)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
						AEGP_ItemH			*itemPH);				/* << NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			selectB,				/* >>	allows to select or deselect the item */
						A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
						AEGP_ItemType		item_type,				/* << */
						A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
						AEGP_ItemH 			itemH,					/* >> */
						A_char				*nameZ);				/* << space for A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemID)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
						A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*parent_folder_itemPH);	/* << */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
						AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
						A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
																	/*			Folder			-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
						AEGP_ItemH 			itemH,					/* >>	*/
						A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long	 			*widthPL,				/* << */
						A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
						AEGP_ItemH 			itemH,					/* >> */
						A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
						AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
						const A_char		*nameZ,					/* >> */
						AEGP_ItemH			parent_folderH0,		/* >> */
						AEGP_ItemH			*new_folderPH);			/* << allocated and owned by AE */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* UNDOABLE. Use the item's native timespace */
						AEGP_ItemH 			itemH,					/* >> */
						const A_Time		*new_timePT);			/* >> */

	SPAPI A_Err (*AEGP_GetItemCommentLength)(
						AEGP_ItemH			itemH, 					/* >> */
						A_u_long			*buf_sizePLu);			/* << */

	SPAPI A_Err	(*AEGP_GetItemComment)(
						AEGP_ItemH			itemH,					/* >> */
						A_u_long			buf_sizeLu,				/* >> */
						char				*commentZ);				/* << */

	SPAPI A_Err (*AEGP_SetItemComment)(
						AEGP_ItemH			itemH, 					/* >> UNDOABLE */
						const char			*commentZ);				/* >> */

} AEGP_ItemSuite4;


#define kAEGPItemSuite				"AEGP Item Suite"
#define kAEGPItemSuiteVersion3		5	/* frozen in 5.5.1 */

typedef struct AEGP_ItemSuite3 {

	SPAPI A_Err	(*AEGP_GetNextItem)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
						AEGP_ItemH			*itemPH);				/* << could be NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			selectB,				/* >>	allows to select or deselect the item */
						A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
						AEGP_ItemType		item_type,				/* << */
						A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
						AEGP_ItemH 			itemH,					/* >> */
						A_char				*nameZ);				/* << space for A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemID)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
						A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
						AEGP_ItemH			itemH,						/* >> */
						AEGP_ItemH			*parent_folder_itemPH);		/* << */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
						AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
						A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
																	/*			Solid/Folder	-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
						AEGP_ItemH 			itemH,					/* >>	*/
						A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long	 			*widthPL,				/* << */
						A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
						AEGP_ItemH 			itemH,					/* >> */
						A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
						AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err	(*AEGP_GetItemSolidColor)(							/* error if item isn't AEGP_ItemType_SOLID! */
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ColorVal		*colorP);				/* << */

	SPAPI A_Err	(*AEGP_SetSolidColor)(								/* error if item isn't AEGP_ItemType_SOLID! */
						AEGP_ItemH			itemH,					/* <> */
						AEGP_ColorVal		color);					/* >> */

	SPAPI A_Err (*AEGP_SetSolidDimensions)(							/* error if item isn't AEGP_ItemType_SOLID! */
						AEGP_ItemH			itemH,					/* <> */
						A_short				widthS,					/* >> */
						A_short				heightS);				/* >> */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
						const A_char		*nameZ,					/* >> */
						AEGP_ItemH			parent_folderH0,		/* >> */
						AEGP_ItemH			*new_folderPH);			/* << */  /* allocated and owned by project (AE) */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* Undoable. Use the item's native timespace */
						AEGP_ItemH 			itemH,					/* >> */
						const A_Time		*new_timePT);			/* >> */


} AEGP_ItemSuite3;

/**************************************************************************************************/
#define kAEGPKeyframeSuite				"AEGP Keyframe Suite"
#define kAEGPKeyframeSuiteVersion4		4 	/* frozen in 8 */

typedef struct AEGP_KeyframeSuite4 {
	//	the only diff from this vs. last rev is that routines that pass AEGP_StreamValue2, when referring to a marker,
	//	(comp or layer) the struct now contains the NEW markerP type, which is compatible with the new Marker Suite

	// returns AEGP_NumKF_NO_DATA if it's a AEGP_StreamType_NO_DATA, and you can't retrieve any values
	// returns zero if no keyframes (but might have an expression, so not necessarily constant)
	SPAPI A_Err(*AEGP_GetStreamNumKFs)(
		AEGP_StreamRefH 		streamH,				/* >> */
		A_long* num_kfsPL);			/* << */


	SPAPI A_Err(*AEGP_GetKeyframeTime)(
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		AEGP_LTimeMode			time_mode,				/* >> */
		A_Time* timePT);				/* << */

// leaves stream unchanged if a keyframe already exists at specified time
	SPAPI A_Err(*AEGP_InsertKeyframe)(									/* UNDOABLE */
		AEGP_StreamRefH			streamH,				/* <> */
		AEGP_LTimeMode			time_mode,				/* >> */
		const A_Time* timePT,				/* >> */
		AEGP_KeyframeIndex* key_indexP);			/* << */

	SPAPI A_Err(*AEGP_DeleteKeyframe)(									/* UNDOABLE */
		AEGP_StreamRefH			streamH,				/* <> */
		AEGP_KeyframeIndex		key_index);				/* >> */

	SPAPI A_Err(*AEGP_GetNewKeyframeValue)(							// dispose using AEGP_DisposeStreamValue()
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		AEGP_StreamValue2* valueP);				/* << */

	SPAPI A_Err(*AEGP_SetKeyframeValue)(								/* UNDOABLE */
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		const AEGP_StreamValue2* valueP);				/* >>  not adopted */

	SPAPI A_Err(*AEGP_GetStreamValueDimensionality)(
		AEGP_StreamRefH			streamH,				/* >> */
		A_short* value_dimPS);			/* << */

	SPAPI A_Err(*AEGP_GetStreamTemporalDimensionality)(
		AEGP_StreamRefH			streamH,				/* >> */
		A_short* temporal_dimPS);		/* << */

	SPAPI A_Err(*AEGP_GetNewKeyframeSpatialTangents)(					// dispose using AEGP_DisposeStreamValue()
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		AEGP_StreamValue2* in_tanP0,				/* << */
		AEGP_StreamValue2* out_tanP0);			/* << */

	SPAPI A_Err(*AEGP_SetKeyframeSpatialTangents)(						/* UNDOABLE */
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		const AEGP_StreamValue2* in_tanP0,				/* >>  not adopted */
		const AEGP_StreamValue2* out_tanP0);			/* >>  not adopted */

	SPAPI A_Err(*AEGP_GetKeyframeTemporalEase)(
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		A_long					dimensionL,				/* >> ranges from 0..TemporalDimensionality-1 */
		AEGP_KeyframeEase* in_easeP0,				/* << */
		AEGP_KeyframeEase* out_easeP0);			/* << */

	SPAPI A_Err(*AEGP_SetKeyframeTemporalEase)(						/* UNDOABLE */
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		A_long					dimensionL,				/* >> ranges from 0..TemporalDimensionality-1 */
		const AEGP_KeyframeEase* in_easeP0,				/* >> not adopted */
		const AEGP_KeyframeEase* out_easeP0);			/* >> not adopted */

	SPAPI A_Err(*AEGP_GetKeyframeFlags)(
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		AEGP_KeyframeFlags* flagsP);				/* << */

	SPAPI A_Err(*AEGP_SetKeyframeFlag)(								/* UNDOABLE */
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_KeyframeIndex		key_index,				/* >> */
		AEGP_KeyframeFlags		flag,					/* >> set one flag at a time */
		A_Boolean				true_falseB);			/* >> */

	SPAPI A_Err(*AEGP_GetKeyframeInterpolation)(
		AEGP_StreamRefH					streamH,		/* >> */
		AEGP_KeyframeIndex				key_index,		/* >> */
		AEGP_KeyframeInterpolationType* in_interpP0,	/* << */
		AEGP_KeyframeInterpolationType* out_interpP0);	/* << */

	SPAPI A_Err(*AEGP_SetKeyframeInterpolation)(						/* UNDOABLE */
		AEGP_StreamRefH					streamH,		/* >> */
		AEGP_KeyframeIndex				key_index,		/* >> */
		AEGP_KeyframeInterpolationType	in_interp,		/* >> */
		AEGP_KeyframeInterpolationType	out_interp);	/* >> */

	SPAPI A_Err(*AEGP_StartAddKeyframes)(
		AEGP_StreamRefH			streamH,
		AEGP_AddKeyframesInfoH* akPH);			/* << */


	SPAPI A_Err(*AEGP_AddKeyframes)(
		AEGP_AddKeyframesInfoH	akH,			/* <> */
		AEGP_LTimeMode          time_mode,		/* >> */
		const A_Time* timePT,		/* >> */
		A_long* key_indexPL);	/* >> */

	SPAPI A_Err(*AEGP_SetAddKeyframe)(
		AEGP_AddKeyframesInfoH	akH,			/* <> */
		A_long					key_indexL,		/* >> */
		const AEGP_StreamValue2* valueP);		/* >> */

	SPAPI A_Err(*AEGP_EndAddKeyframes)(						/* UNDOABLE */
		A_Boolean				addB,
		AEGP_AddKeyframesInfoH	akH);			/* >> */

} AEGP_KeyframeSuite4;

#define kAEGPKeyframeSuiteVersion3		3 	/* frozen in 6.5 */

typedef struct AEGP_KeyframeSuite3 {

	// returns AEGP_NumKF_NO_DATA if it's a AEGP_StreamType_NO_DATA, and you can't retrieve any values
	// returns zero if no keyframes (but might have an expression, so not necessarily constant)


	SPAPI A_Err	(*AEGP_GetStreamNumKFs)(
						AEGP_StreamRefH 		streamH,				/* >> */
						A_long					*num_kfsPL);			/* << */


	SPAPI A_Err (*AEGP_GetKeyframeTime)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_LTimeMode			time_mode,				/* >> */
						A_Time					*timePT);				/* << */

	// leaves stream unchanged if a keyframe already exists at specified time
	SPAPI A_Err (*AEGP_InsertKeyframe)(									/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* <> */
						AEGP_LTimeMode			time_mode,				/* >> */
						const A_Time			*timePT,				/* >> */
						AEGP_KeyframeIndex		*key_indexP);			/* << */

	SPAPI A_Err (*AEGP_DeleteKeyframe)(									/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* <> */
						AEGP_KeyframeIndex		key_index);				/* >> */

	SPAPI A_Err (*AEGP_GetNewKeyframeValue)(							// dispose using AEGP_DisposeStreamValue()
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_StreamValue		*valueP);				/* << */

	SPAPI A_Err (*AEGP_SetKeyframeValue)(								/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						const AEGP_StreamValue	*valueP);				/* >>  not adopted */

	SPAPI A_Err (*AEGP_GetStreamValueDimensionality)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_short					*value_dimPS);			/* << */

	SPAPI A_Err (*AEGP_GetStreamTemporalDimensionality)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_short					*temporal_dimPS);		/* << */

	SPAPI A_Err (*AEGP_GetNewKeyframeSpatialTangents)(					// dispose using AEGP_DisposeStreamValue()
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_StreamValue		*in_tanP0,				/* << */
						AEGP_StreamValue		*out_tanP0);			/* << */

	// In AEGP_KeyframeSuite2 and prior versions, the values returned from
	// this function were wrong when called on an effect point control stream or
	// anchor point. They were not multiplied by the layer size. Now they are.
	SPAPI A_Err (*AEGP_SetKeyframeSpatialTangents)(						/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						const AEGP_StreamValue	*in_tanP0,				/* >>  not adopted */
						const AEGP_StreamValue	*out_tanP0);			/* >>  not adopted */

	SPAPI A_Err (*AEGP_GetKeyframeTemporalEase)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						A_long					dimensionL,				/* >> ranges from 0..TemporalDimensionality-1 */
						AEGP_KeyframeEase		*in_easeP0,				/* << */
						AEGP_KeyframeEase		*out_easeP0);			/* << */

	SPAPI A_Err (*AEGP_SetKeyframeTemporalEase)(						/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						A_long					dimensionL,				/* >> ranges from 0..TemporalDimensionality-1 */
						const AEGP_KeyframeEase	*in_easeP0,				/* >> not adopted */
						const AEGP_KeyframeEase	*out_easeP0);			/* >> not adopted */

	SPAPI A_Err (*AEGP_GetKeyframeFlags)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_KeyframeFlags		*flagsP);				/* << */

	SPAPI A_Err (*AEGP_SetKeyframeFlag)(								/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_KeyframeFlags		flag,					/* >> set one flag at a time */
						A_Boolean				true_falseB);			/* >> */

	SPAPI A_Err (*AEGP_GetKeyframeInterpolation)(
						AEGP_StreamRefH					streamH,		/* >> */
						AEGP_KeyframeIndex				key_index,		/* >> */
						AEGP_KeyframeInterpolationType	*in_interpP0,	/* << */
						AEGP_KeyframeInterpolationType	*out_interpP0);	/* << */

	SPAPI A_Err (*AEGP_SetKeyframeInterpolation)(						/* UNDOABLE */
						AEGP_StreamRefH					streamH,		/* >> */
						AEGP_KeyframeIndex				key_index,		/* >> */
						AEGP_KeyframeInterpolationType	in_interp,		/* >> */
						AEGP_KeyframeInterpolationType	out_interp);	/* >> */

	SPAPI A_Err (*AEGP_StartAddKeyframes)(
						AEGP_StreamRefH			streamH,
						AEGP_AddKeyframesInfoH	*akPH);			/* << */


	SPAPI A_Err (*AEGP_AddKeyframes)(
						AEGP_AddKeyframesInfoH	akH,			/* <> */
						AEGP_LTimeMode          time_mode,		/* >> */
						const A_Time            *timePT,		/* >> */
						A_long					*key_indexPL);	/* >> */

	SPAPI A_Err (*AEGP_SetAddKeyframe)(
						AEGP_AddKeyframesInfoH	akH,			/* <> */
						A_long					key_indexL,		/* >> */
						const AEGP_StreamValue	*valueP);		/* >> */

	SPAPI A_Err (*AEGP_EndAddKeyframes)(						/* UNDOABLE */
						A_Boolean				addB,
						AEGP_AddKeyframesInfoH	akH);			/* >> */

} AEGP_KeyframeSuite3;

#define kAEGPKeyframeSuiteVersion2		2 	/* frozen in 5.5 */

typedef struct AEGP_KeyframeSuite2 {

	// returns AEGP_NumKF_NO_DATA if it's a AEGP_StreamType_NO_DATA, and you can't retrieve any values
	// returns zero if no keyframes (but might have an expression, so not necessarily constant)


	SPAPI A_Err	(*AEGP_GetStreamNumKFs)(
						AEGP_StreamRefH 		streamH,				/* >> */
						A_long					*num_kfsPL);			/* << */


	SPAPI A_Err (*AEGP_GetKeyframeTime)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_LTimeMode			time_mode,				/* >> */
						A_Time					*timePT);				/* << */

	// leaves stream unchanged if a keyframe already exists at specified time
	SPAPI A_Err (*AEGP_InsertKeyframe)(									/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* <> */
						AEGP_LTimeMode			time_mode,				/* >> */
						const A_Time			*timePT,				/* >> */
						AEGP_KeyframeIndex		*key_indexP);			/* << */

	SPAPI A_Err (*AEGP_DeleteKeyframe)(									/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* <> */
						AEGP_KeyframeIndex		key_index);				/* >> */

	SPAPI A_Err (*AEGP_GetNewKeyframeValue)(							// dispose using AEGP_DisposeStreamValue()
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_StreamValue		*valueP);				/* << */

	SPAPI A_Err (*AEGP_SetKeyframeValue)(								/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						const AEGP_StreamValue	*valueP);				/* >>  not adopted */

	SPAPI A_Err (*AEGP_GetStreamValueDimensionality)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_short					*value_dimPS);			/* << */

	SPAPI A_Err (*AEGP_GetStreamTemporalDimensionality)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_short					*temporal_dimPS);		/* << */

	SPAPI A_Err (*AEGP_GetNewKeyframeSpatialTangents)(					// dispose using AEGP_DisposeStreamValue()
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_StreamValue		*in_tanP0,				/* << */
						AEGP_StreamValue		*out_tanP0);			/* << */

	SPAPI A_Err (*AEGP_SetKeyframeSpatialTangents)(						/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						const AEGP_StreamValue	*in_tanP0,				/* >>  not adopted */
						const AEGP_StreamValue	*out_tanP0);			/* >>  not adopted */

	SPAPI A_Err (*AEGP_GetKeyframeTemporalEase)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						A_long					dimensionL,				/* >> ranges from 0..TemporalDimensionality-1 */
						AEGP_KeyframeEase		*in_easeP0,				/* << */
						AEGP_KeyframeEase		*out_easeP0);			/* << */

	SPAPI A_Err (*AEGP_SetKeyframeTemporalEase)(						/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						A_long					dimensionL,				/* >> ranges from 0..TemporalDimensionality-1 */
						const AEGP_KeyframeEase	*in_easeP0,				/* >> not adopted */
						const AEGP_KeyframeEase	*out_easeP0);			/* >> not adopted */

	SPAPI A_Err (*AEGP_GetKeyframeFlags)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_KeyframeFlags		*flagsP);				/* << */

	SPAPI A_Err (*AEGP_SetKeyframeFlag)(								/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_KeyframeFlags		flag,					/* >> set one flag at a time */
						A_Boolean				true_falseB);			/* >> */

	SPAPI A_Err (*AEGP_GetKeyframeInterpolation)(
						AEGP_StreamRefH					streamH,		/* >> */
						AEGP_KeyframeIndex				key_index,		/* >> */
						AEGP_KeyframeInterpolationType	*in_interpP0,	/* << */
						AEGP_KeyframeInterpolationType	*out_interpP0);	/* << */

	SPAPI A_Err (*AEGP_SetKeyframeInterpolation)(						/* UNDOABLE */
						AEGP_StreamRefH					streamH,		/* >> */
						AEGP_KeyframeIndex				key_index,		/* >> */
						AEGP_KeyframeInterpolationType	in_interp,		/* >> */
						AEGP_KeyframeInterpolationType	out_interp);	/* >> */

	SPAPI A_Err (*AEGP_StartAddKeyframes)(
						AEGP_StreamRefH			streamH,
						AEGP_AddKeyframesInfoH	*akPH);			/* << */


	SPAPI A_Err (*AEGP_AddKeyframes)(
						AEGP_AddKeyframesInfoH	akH,			/* <> */
						AEGP_LTimeMode          time_mode,		/* >> */
						const A_Time            *timePT,		/* >> */
						A_long					*key_indexPL);	/* >> */

	SPAPI A_Err (*AEGP_SetAddKeyframe)(
						AEGP_AddKeyframesInfoH	akH,			/* <> */
						A_long					key_indexL,		/* >> */
						const AEGP_StreamValue	*valueP);		/* >> */

	SPAPI A_Err (*AEGP_EndAddKeyframes)(						/* UNDOABLE */
						A_Boolean				addB,
						AEGP_AddKeyframesInfoH	akH);			/* >> */

} AEGP_KeyframeSuite2;

#define kAEGPKeyframeSuiteVersion1		1 /* frozen in AE 5.0 */

typedef struct AEGP_KeyframeSuite1 {

	// returns AEGP_NumKF_NO_DATA if it's a AEGP_StreamType_NO_DATA, and you can't retrieve any values
	// returns zero if no keyframes (but might have an expression, so not necessarily constant)


	SPAPI A_Err	(*AEGP_GetStreamNumKFs)(
						AEGP_StreamRefH 			streamH,				/* >> */
						A_long					*num_kfsPL);			/* << */


	SPAPI A_Err (*AEGP_GetKeyframeTime)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_LTimeMode			time_mode,				/* >> */
						A_Time					*timePT);				/* << */

	// leaves stream unchanged if a keyframe already exists at specified time
	SPAPI A_Err (*AEGP_InsertKeyframe)(									/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* <> */
						AEGP_LTimeMode			time_mode,				/* >> */
						const A_Time			*timePT,				/* >> */
						AEGP_KeyframeIndex		*key_indexP);			/* << */

	SPAPI A_Err (*AEGP_DeleteKeyframe)(									/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* <> */
						AEGP_KeyframeIndex		key_index);				/* >> */

	SPAPI A_Err (*AEGP_GetNewKeyframeValue)(							// dispose using AEGP_DisposeStreamValue()
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_StreamValue		*valueP);				/* << */

	SPAPI A_Err (*AEGP_SetKeyframeValue)(								/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						const AEGP_StreamValue	*valueP);				/* >> */	// not adopted

	SPAPI A_Err (*AEGP_GetStreamValueDimensionality)(
						AEGP_StreamRefH			streamH,				/* >> */
						short					*value_dimPS);			/* << */

	SPAPI A_Err (*AEGP_GetStreamTemporalDimensionality)(
						AEGP_StreamRefH			streamH,				/* >> */
						short					*temporal_dimPS);		/* << */

	SPAPI A_Err (*AEGP_GetNewKeyframeSpatialTangents)(					// dispose using AEGP_DisposeStreamValue()
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_StreamValue		*in_tanP0,				/* << */
						AEGP_StreamValue		*out_tanP0);			/* << */

	SPAPI A_Err (*AEGP_SetKeyframeSpatialTangents)(						/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						const AEGP_StreamValue	*in_tanP0,				/* >> */	// not adopted
						const AEGP_StreamValue	*out_tanP0);			/* >> */	// not adopted

	SPAPI A_Err (*AEGP_GetKeyframeTemporalEase)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						A_long					dimensionL,				/* >> */	// ranges from 0..TemporalDimensionality-1
						AEGP_KeyframeEase		*in_easeP0,				/* << */
						AEGP_KeyframeEase		*out_easeP0);			/* << */

	SPAPI A_Err (*AEGP_SetKeyframeTemporalEase)(						/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						A_long					dimensionL,				/* >> */	// ranges from 0..TemporalDimensionality-1
						const AEGP_KeyframeEase	*in_easeP0,				/* >> */	// not adopted
						const AEGP_KeyframeEase	*out_easeP0);			/* >> */	// not adopted

	SPAPI A_Err (*AEGP_GetKeyframeFlags)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_KeyframeFlags		*flagsP);				/* << */

	SPAPI A_Err (*AEGP_SetKeyframeFlag)(								/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_KeyframeIndex		key_index,				/* >> */
						AEGP_KeyframeFlags		flag,					/* >> */	// set one at a time
						A_Boolean				true_falseB);			/* >> */

	SPAPI A_Err (*AEGP_GetKeyframeInterpolation)(
						AEGP_StreamRefH					streamH,		/* >> */
						AEGP_KeyframeIndex				key_index,		/* >> */
						AEGP_KeyframeInterpolationType	*in_interpP0,	/* << */
						AEGP_KeyframeInterpolationType	*out_interpP0);	/* << */

	SPAPI A_Err (*AEGP_SetKeyframeInterpolation)(						/* UNDOABLE */
						AEGP_StreamRefH					streamH,		/* >> */
						AEGP_KeyframeIndex				key_index,		/* >> */
						AEGP_KeyframeInterpolationType	in_interp,		/* >> */
						AEGP_KeyframeInterpolationType	out_interp);	/* >> */

} AEGP_KeyframeSuite1;

/* frozen AE 5.5 */
#define kAEGPItemSuiteVersion2		4

typedef struct AEGP_ItemSuite2 {

	SPAPI A_Err	(*AEGP_GetNextItem)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
						AEGP_ItemH			*itemPH);				/* << could be NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			selectB,				/* >>	allows to select or deselect the item */
						A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
						AEGP_ItemType		item_type,				/* << */
						A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
						AEGP_ItemH 			itemH,					/* >> */
						A_char				*nameZ);				/* << space for A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemID)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
						A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
						AEGP_ItemH			itemH,						/* >> */
						AEGP_ItemH			*parent_folder_itemPH);		/* << */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
						AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
						A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
																	/*			Solid/Folder	-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
						AEGP_ItemH 			itemH,					/* >>	*/
						A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long	 			*widthPL,				/* << */
						A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
						AEGP_ItemH 			itemH,					/* >> */
						A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
						AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err	(*AEGP_GetItemSolidColor)(							/* error if item isn't AEGP_ItemType_SOLID! */
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ColorVal		*colorP);				/* << */

	SPAPI A_Err	(*AEGP_SetSolidColor)(								/* error if item isn't AEGP_ItemType_SOLID! */
						AEGP_ItemH			itemH,					/* <> */
						AEGP_ColorVal		color);					/* >> */

	SPAPI A_Err (*AEGP_SetSolidDimensions)(							/* error if item isn't AEGP_ItemType_SOLID! */
						AEGP_ItemH			itemH,					/* <> */
						A_short				widthS,					/* >> */
						A_short				heightS);				/* >> */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
						const A_char		*nameZ,					/* >> */
						AEGP_ItemH			parent_folderH0,		/* >> */
						AEGP_ItemH			*new_folderPH);			/* << */  /* allocated and owned by project (AE) */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* Undoable. Use the item's native timespace */
						AEGP_ItemH 			itemH,					/* >> */
						const A_Time		*new_timePT);			/* >> */


	//  work on Comps and Footage items.
	SPAPI A_Err (*AEGP_RenderNewItemSoundData)( // AEGP_SoundDataH must be disposed.
					AEGP_ItemH	itemH,								// >>
					const A_Time			*start_timePT,			// >>
					const A_Time			*durationPT,			// >>
					const AEGP_SoundDataFormat* sound_formatP,		// >>
					AEGP_SoundDataH			*new_sound_dataPH);		// << can return NULL if no audio

} AEGP_ItemSuite2;


#define kAEGPItemSuiteVersion1		3 /* frozen in AE 5.0 */

typedef struct AEGP_ItemSuite1 {

	SPAPI A_Err	(*AEGP_GetNextItem)(
						AEGP_ItemH			itemH,					/* >> */
						AEGP_ItemH			*next_itemPH);			/* << NULL after last item */

	SPAPI A_Err	(*AEGP_GetActiveItem)(
						AEGP_ItemH			*itemPH);				/* << could be NULL if none is active */

	SPAPI A_Err	(*AEGP_IsItemSelected)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			*selectedPB);			/* << */

	SPAPI A_Err	(*AEGP_SelectItem)(
						AEGP_ItemH			itemH,					/* >> */
						A_Boolean			selectB,				/* >>	allows to select or deselect the item */
						A_Boolean			deselect_othersB);		/* >> */

	SPAPI A_Err	(*AEGP_GetItemType)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemType		*item_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetTypeName)(
						AEGP_ItemType		item_type,				/* << */
						A_char 				*nameZ);				/* << space for A_char[AEGP_MAX_TYPE_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemName)(
						AEGP_ItemH 			itemH,					/* >> */
						A_char				*nameZ);				/* << space for A_char[AEGP_MAX_ITEM_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetItemID)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long				*item_idPL);			/* << */

	SPAPI A_Err	(*AEGP_GetItemFlags)(
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ItemFlags		*item_flagsP);			/* << */

	SPAPI A_Err	(*AEGP_SetItemUseProxy)(							/* UNDOABLE */
						AEGP_ItemH 			itemH,					/* >> error if has_proxy is FALSE! */
						A_Boolean			use_proxyB);			/* >> */

	SPAPI A_Err	(*AEGP_GetItemParentFolder)(
						AEGP_ItemH			itemH,						/* >> */
						AEGP_ItemH			*parent_folder_itemPH);		/* << */

	SPAPI A_Err	(*AEGP_GetItemDuration)(							/* Returns the result in the item's native timespace:	*/
						AEGP_ItemH 			itemH,					/* >>		Comp			-> comp time,				*/
						A_Time 				*durationPT);			/* <<		Footage			-> footage time,			*/
																	/*			Solid/Folder	-> 0 (no duration)			*/

	SPAPI A_Err	(*AEGP_GetItemCurrentTime)(							/* Returns the result in the item's native timespace (not updated while rendering)*/
						AEGP_ItemH 			itemH,					/* >>	*/
						A_Time 				*curr_timePT);			/* <<	*/

	SPAPI A_Err	(*AEGP_GetItemDimensions)(
						AEGP_ItemH 			itemH,					/* >> */
						A_long	 			*widthPL,				/* << */
						A_long				*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetItemPixelAspectRatio)(
						AEGP_ItemH 			itemH,					/* >> */
						A_Ratio 			*pix_aspect_ratioPRt);	/* << */

	SPAPI A_Err	(*AEGP_DeleteItem)(									/* UNDOABLE */
						AEGP_ItemH 			itemH);					/* >> removes item from all comps */

	SPAPI A_Err	(*AEGP_GetItemSolidColor)(							/* error if item isn't AEGP_ItemType_SOLID! */
						AEGP_ItemH 			itemH,					/* >> */
						AEGP_ColorVal		*colorP);				/* << */

	SPAPI A_Err (*AEGP_CreateNewFolder)(
						const A_char		*nameZ,					/* >> */
						AEGP_ItemH			parent_folderH0,		/* >> */
						AEGP_ItemH			*new_folderPH);			/* << */  /* allocated and owned by project (AE) */

	SPAPI A_Err (*AEGP_SetItemCurrentTime)(							/* Undoable. Use the item's native timespace */
						AEGP_ItemH 			itemH,					/* >> */
						const A_Time		*new_timePT);			/* >> */


	//  work on Comps and Footage items.
	SPAPI A_Err (*AEGP_RenderNewItemSoundData)( // AEGP_SoundDataH must be disposed.
					AEGP_ItemH	itemH,								// >>
					const A_Time			*start_timePT,			// >>
					const A_Time			*durationPT,			// >>
					const AEGP_SoundDataFormat* sound_formatP,		// >>
					AEGP_SoundDataH			*new_sound_dataPH);		// << can return NULL if no audio

} AEGP_ItemSuite1;

/*********************************************************/

#define kAEGPUtilitySuiteVersion5		11 /* frozen in AE 8.0 */

typedef struct AEGP_UtilitySuite5 {

	SPAPI A_Err	(*AEGP_ReportInfo)(										/* displays dialog with name of plugin followed by info string */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						const A_char			*info_stringZ);			/* >> */


	SPAPI A_Err	(*AEGP_GetDriverPluginInitFuncVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_GetDriverImplementationVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_StartQuietErrors)(
						AEGP_ErrReportState		*err_stateP);			/* << */

	SPAPI A_Err	(*AEGP_EndQuietErrors)(
						A_Boolean				report_quieted_errorsB,	/* >> currently reports last quieted error */
						AEGP_ErrReportState		*err_stateP);			/* >> */

	SPAPI A_Err (*AEGP_GetLastErrorMessage)(
						A_long					buffer_size,			/* >> size of character buffer */
						A_char					*error_string,					/* << */
						A_Err					*error_num);				/* << */

	SPAPI A_Err	(*AEGP_StartUndoGroup)(									/* MUST be balanced with call to AEGP_EndUndoGroup() */
						const A_char			*undo_nameZ);			/* >> */

	SPAPI A_Err	(*AEGP_EndUndoGroup)(void);

	SPAPI A_Err (*AEGP_RegisterWithAEGP)(
						AEGP_GlobalRefcon 		global_refcon,			/* >> global refcon passed in command handlers */
						const A_char			*plugin_nameZ,			/* >> name of this plugin. AEGP_MAX_PLUGIN_NAME_SIZE */
						AEGP_PluginID			*plugin_id);			/* << id for plugin to use in other AEGP calls */

	SPAPI A_Err (*AEGP_GetMainHWND)(
						void					*main_hwnd);			/* << */

	SPAPI A_Err (*AEGP_ShowHideAllFloaters)(
						A_Boolean				include_tool_palB);		/* >> */

	SPAPI A_Err (*AEGP_PaintPalGetForeColor)(
						AEGP_ColorVal			*fore_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalGetBackColor)(
						AEGP_ColorVal			*back_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalSetForeColor)(
						const AEGP_ColorVal		*fore_colorP);			/* >> */

	SPAPI A_Err (*AEGP_PaintPalSetBackColor)(
						const AEGP_ColorVal		*back_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalGetFillColor)(
						A_Boolean				*is_fill_color_definedPB,	/*  << */
                        AEGP_ColorVal			*fill_colorP); 				/* << only valid if is_fill_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalGetStrokeColor)(
						A_Boolean				*is_stroke_color_definedPB, /*  << */
                        AEGP_ColorVal			*stroke_colorP); 			/* << only valid if is_stroke_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalSetFillColor)(
						const AEGP_ColorVal		*fill_colorP);				/* >> */

	SPAPI A_Err (*AEGP_CharPalSetStrokeColor)(
						const AEGP_ColorVal		*stroke_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalIsFillColorUIFrontmost)(						/* Otherwise, StrokeColor is frontmost */
						A_Boolean				*is_fill_color_selectedPB);	/* << */

	SPAPI A_Err (*AEGP_ConvertFpLongToHSFRatio)(
						A_FpLong				numberF,					/* >> */
						A_Ratio					*ratioPR);					/* << */

	SPAPI A_Err (*AEGP_ConvertHSFRatioToFpLong)(
						A_Ratio					ratioR,						/* << */
						A_FpLong				*numberPF);					/* >> */

	// this routine is safe to call from the non-main
	// thread. It is asynchronous and will return before the idle handler is called.
	// The Suite routines to get this pointer are not
	// thread safe, therefore you need to save it off
	// in the main thread for use by the child thread.
	SPAPI A_Err (*AEGP_CauseIdleRoutinesToBeCalled)(void);


	// Determine if after effects is running in a mode where there is no
	// user interface, and attempting to interact with the user (via a modal dialog)
	// will hang the application.
	// This will not change during a run. Use it to optimize your plugin at startup
	// to not create a user interface and make AE launch faster, and not break
	// when running multiple instances of a service.
	SPAPI	A_Err  (*AEGP_GetSuppressInteractiveUI)(A_Boolean* ui_is_suppressedPB); // out

	// this call writes text to the console if one is available.  One is guaranteed to be available
	// if ui_is_suppressedB == true.
	// In general use the call AEGP_ReportInfo() as it will write to the console in
	// non-interactive modes, and use a dialog in interactive modes.
	SPAPI	A_Err  (*AEGP_WriteToOSConsole)(const A_char* textZ);				// in

	// this writes an entry into the debug log, or to the command line if launched
	// with the -debug flag.
	SPAPI	A_Err  (*AEGP_WriteToDebugLog)(const A_char* subsystemZ,	// in
								const A_char* event_typeZ,				// in
								const A_char * infoZ);					// in


	SPAPI	A_Err  (*AEGP_IsScriptingAvailable)(A_Boolean* outAvailablePB);

	// Execute a script.
	// The script text can either be in UTF-8, or the current
	// application encoding.
	// The result is the result string if OK. It is optional.
	// The error is the error string if an error occurred. It is optional.
	// the result and error are in the encoding specified by platform_encodingB
	SPAPI	A_Err  (*AEGP_ExecuteScript)(AEGP_PluginID			inPlugin_id,
										const A_char* inScriptZ,		// in
										const A_Boolean platform_encodingB,	// in
										AEGP_MemHandle* outResultPH0,
										AEGP_MemHandle* outErrorStringPH0);

	SPAPI A_Err (*AEGP_HostIsActivated)(A_Boolean	*is_activatedPB);

	SPAPI A_Err (*AEGP_GetPluginPlatformRef)(AEGP_PluginID	plug_id, void** plat_refPPV); // on the Mac, it is a CFBundleRef to your mach-o plugin or NULL for a CFM plug-in; on Windows it is set to NULL for now

	SPAPI A_Err (*AEGP_UpdateFontList)(void); // Rescan the system font list.  This will return quickly if the font list hasn't changed.

} AEGP_UtilitySuite5;



#define kAEGPUtilitySuiteVersion4		10 /* frozen in AE 7.0 */

typedef struct AEGP_UtilitySuite4 {

	SPAPI A_Err	(*AEGP_ReportInfo)(										/* displays dialog with name of plugin followed by info string */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						const A_char			*info_stringZ);			/* >> */


	SPAPI A_Err	(*AEGP_GetDriverPluginInitFuncVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_GetDriverImplementationVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_StartQuietErrors)(
						AEGP_ErrReportState		*err_stateP);			/* << */

	SPAPI A_Err	(*AEGP_EndQuietErrors)(
						A_Boolean				report_quieted_errorsB,	/* >> currently reports last quieted error */
						AEGP_ErrReportState		*err_stateP);			/* >> */

	SPAPI A_Err (*AEGP_GetLastErrorMessage)(
						A_long					buffer_size,			/* >> size of character buffer */
						A_char					*error_string,					/* << */
						A_Err					*error_num);				/* << */

	SPAPI A_Err	(*AEGP_StartUndoGroup)(									/* MUST be balanced with call to AEGP_EndUndoGroup() */
						const A_char			*undo_nameZ);			/* >> */

	SPAPI A_Err	(*AEGP_EndUndoGroup)(void);

	SPAPI A_Err (*AEGP_RegisterWithAEGP)(
						AEGP_GlobalRefcon 		global_refcon,			/* >> global refcon passed in command handlers */
						const A_char			*plugin_nameZ,			/* >> name of this plugin. AEGP_MAX_PLUGIN_NAME_SIZE */
						AEGP_PluginID			*plugin_id);			/* << id for plugin to use in other AEGP calls */

	SPAPI A_Err (*AEGP_GetMainHWND)(
						void					*main_hwnd);			/* << */

	SPAPI A_Err (*AEGP_ShowHideAllFloaters)(
						A_Boolean				include_tool_palB);		/* >> */

	SPAPI A_Err (*AEGP_PaintPalGetForeColor)(
						AEGP_ColorVal			*fore_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalGetBackColor)(
						AEGP_ColorVal			*back_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalSetForeColor)(
						const AEGP_ColorVal		*fore_colorP);			/* >> */

	SPAPI A_Err (*AEGP_PaintPalSetBackColor)(
						const AEGP_ColorVal		*back_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalGetFillColor)(
						A_Boolean				*is_fill_color_definedPB,	/*  << */
                        AEGP_ColorVal			*fill_colorP); 				/* << only valid if is_fill_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalGetStrokeColor)(
						A_Boolean				*is_stroke_color_definedPB, /*  << */
                        AEGP_ColorVal			*stroke_colorP); 			/* << only valid if is_stroke_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalSetFillColor)(
						const AEGP_ColorVal		*fill_colorP);				/* >> */

	SPAPI A_Err (*AEGP_CharPalSetStrokeColor)(
						const AEGP_ColorVal		*stroke_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalIsFillColorUIFrontmost)(						/* Otherwise, StrokeColor is frontmost */
						A_Boolean				*is_fill_color_selectedPB);	/* << */

	SPAPI A_Err (*AEGP_ConvertFpLongToHSFRatio)(
						A_FpLong				numberF,					/* >> */
						A_Ratio					*ratioPR);					/* << */

	SPAPI A_Err (*AEGP_ConvertHSFRatioToFpLong)(
						A_Ratio					ratioR,						/* << */
						A_FpLong				*numberPF);					/* >> */

	// this routine is safe to call from the non-main
	// thread. It is asynchronous and will return before the idle handler is called.
	// The Suite routines to get this pointer are not
	// thread safe, therefore you need to save it off
	// in the main thread for use by the child thread.
	SPAPI A_Err (*AEGP_CauseIdleRoutinesToBeCalled)(void);


	// Determine if after effects is running in a mode where there is no
	// user interface, and attempting to interact with the user (via a modal dialog)
	// will hang the application.
	// This will not change during a run. Use it to optimize your plugin at startup
	// to not create a user interface and make AE launch faster, and not break
	// when running multiple instances of a service.
	SPAPI	A_Err  (*AEGP_GetSuppressInteractiveUI)(A_Boolean* ui_is_suppressedPB); // out

	// this call writes text to the console if one is available.  One is guaranteed to be available
	// if ui_is_suppressedB == true.
	// In general use the call AEGP_ReportInfo() as it will write to the console in
	// non-interactive modes, and use a dialog in interactive modes.
	SPAPI	A_Err  (*AEGP_WriteToOSConsole)(const A_char* textZ);				// in

	// this writes an entry into the debug log, or to the command line if launched
	// with the -debug flag.
	SPAPI	A_Err  (*AEGP_WriteToDebugLog)(const A_char* subsystemZ,	// in
								const A_char* event_typeZ,				// in
								const A_char * infoZ);					// in


	SPAPI	A_Err  (*AEGP_IsScriptingAvailable)(A_Boolean* outAvailablePB);

	// Execute a script.
	// The script text can either be in UTF-8, or the current
	// application encoding.
	// The result is the result string if OK. It is optional.
	// The error is the error string if an error occurred. It is optional.
	// the result and error are in the encoding specified by platform_encodingB
	SPAPI	A_Err  (*AEGP_ExecuteScript)(AEGP_PluginID			inPlugin_id,
										const A_char* inScriptZ,		// in
										const A_Boolean platform_encodingB,	// in
										AEGP_MemHandle* outResultPH0,
										AEGP_MemHandle* outErrorStringPH0);

	SPAPI A_Err (*AEGP_HostIsActivated)(A_Boolean	*is_activatedPB);

	SPAPI A_Err (*AEGP_GetPluginPlatformRef)(AEGP_PluginID	plug_id, void** plat_refPPV); // on the Mac, it is a CFBundleRef to your mach-o plugin or NULL for a CFM plug-in; on Windows it is set to NULL for now

} AEGP_UtilitySuite4;



#define kAEGPUtilitySuiteVersion3		7 /* frozen in AE 6.5 */

typedef struct AEGP_UtilitySuite3 {

	SPAPI A_Err	(*AEGP_ReportInfo)(										/* displays dialog with name of plugin followed by info string */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						const A_char			*info_stringZ);			/* >> */


	SPAPI A_Err	(*AEGP_GetDriverPluginInitFuncVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_GetDriverImplementationVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_StartQuietErrors)(
						AEGP_ErrReportState		*err_stateP);			/* << */

	SPAPI A_Err	(*AEGP_EndQuietErrors)(
						A_Boolean				report_quieted_errorsB,	/* >> currently reports last quieted error */
						AEGP_ErrReportState		*err_stateP);			/* >> */

	SPAPI A_Err	(*AEGP_StartUndoGroup)(									/* MUST be balanced with call to AEGP_EndUndoGroup() */
						const A_char			*undo_nameZ);			/* >> */

	SPAPI A_Err	(*AEGP_EndUndoGroup)(void);

	SPAPI A_Err (*AEGP_RegisterWithAEGP)(
						AEGP_GlobalRefcon 		global_refcon,			/* >> global refcon passed in command handlers */
						const A_char			*plugin_nameZ,			/* >> name of this plugin. AEGP_MAX_PLUGIN_NAME_SIZE */
						AEGP_PluginID			*plugin_id);			/* << id for plugin to use in other AEGP calls */

	SPAPI A_Err (*AEGP_GetMainHWND)(
						void					*main_hwnd);			/* << */

	SPAPI A_Err (*AEGP_ShowHideAllFloaters)(
						A_Boolean				include_tool_palB);		/* >> */

	SPAPI A_Err (*AEGP_PaintPalGetForeColor)(
						AEGP_ColorVal			*fore_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalGetBackColor)(
						AEGP_ColorVal			*back_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalSetForeColor)(
						const AEGP_ColorVal		*fore_colorP);			/* >> */

	SPAPI A_Err (*AEGP_PaintPalSetBackColor)(
						const AEGP_ColorVal		*back_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalGetFillColor)(
						A_Boolean				*is_fill_color_definedPB,	/*  << */
                        AEGP_ColorVal			*fill_colorP); 				/* << only valid if is_fill_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalGetStrokeColor)(
						A_Boolean				*is_stroke_color_definedPB, /*  << */
                        AEGP_ColorVal			*stroke_colorP); 			/* << only valid if is_stroke_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalSetFillColor)(
						const AEGP_ColorVal		*fill_colorP);				/* >> */

	SPAPI A_Err (*AEGP_CharPalSetStrokeColor)(
						const AEGP_ColorVal		*stroke_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalIsFillColorUIFrontmost)(						/* Otherwise, StrokeColor is frontmost */
						A_Boolean				*is_fill_color_selectedPB);	/* << */

	SPAPI A_Err (*AEGP_ConvertFpLongToHSFRatio)(
						A_FpLong				numberF,					/* >> */
						A_Ratio					*ratioPR);					/* << */

	SPAPI A_Err (*AEGP_ConvertHSFRatioToFpLong)(
						A_Ratio					ratioR,						/* << */
						A_FpLong				*numberPF);					/* >> */

	// this routine is safe to call from the non-main
	// thread. It is asynchronous and will return before the idle handler is called.
	// The Suite routines to get this pointer are not
	// thread safe, therefore you need to save it off
	// in the main thread for use by the child thread.
	SPAPI A_Err (*AEGP_CauseIdleRoutinesToBeCalled)(void);


	// Determine if after effects is running in a mode where there is no
	// user interface, and attempting to interact with the user (via a modal dialog)
	// will hang the application.
	// This will not change during a run. Use it to optimize your plugin at startup
	// to not create a user interface and make AE launch faster, and not break
	// when running multiple instances of a service.
	SPAPI	A_Err  (*AEGP_GetSuppressInteractiveUI)(A_Boolean* ui_is_suppressedPB); // out

	// this call writes text to the console if one is available.  One is guaranteed to be available
	// if ui_is_suppressedB == true.
	// In general use the call AEGP_ReportInfo() as it will write to the console in
	// non-interactive modes, and use a dialog in interactive modes.
	SPAPI	A_Err  (*AEGP_WriteToOSConsole)(const A_char* textZ);				// in

	// this writes an entry into the debug log, or to the command line if launched
	// with the -debug flag.
	SPAPI	A_Err  (*AEGP_WriteToDebugLog)(const A_char* subsystemZ,	// in
								const A_char* event_typeZ,				// in
								const A_char * infoZ);					// in
} AEGP_UtilitySuite3;

#define kAEGPUtilitySuiteVersion2		5 /* frozen in AE 6.0 */

typedef struct AEGP_UtilitySuite2 {

	SPAPI A_Err	(*AEGP_ReportInfo)(										/* displays dialog with name of plugin followed by info string */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						const A_char			*info_stringZ);			/* >> */


	SPAPI A_Err	(*AEGP_GetDriverPluginInitFuncVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_GetDriverImplementationVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_StartQuietErrors)(
						AEGP_ErrReportState		*err_stateP);			/* << */

	SPAPI A_Err	(*AEGP_EndQuietErrors)(
						A_Boolean				report_quieted_errorsB,	/* >> currently reports last quieted error */
						AEGP_ErrReportState		*err_stateP);			/* >> */

	SPAPI A_Err	(*AEGP_StartUndoGroup)(									/* MUST be balanced with call to AEGP_EndUndoGroup() */
						const A_char			*undo_nameZ);			/* >> */

	SPAPI A_Err	(*AEGP_EndUndoGroup)(void);

	SPAPI A_Err (*AEGP_RegisterWithAEGP)(
						AEGP_GlobalRefcon 		global_refcon,			/* >> global refcon passed in command handlers */
						const A_char			*plugin_nameZ,			/* >> name of this plugin. AEGP_MAX_PLUGIN_NAME_SIZE */
						AEGP_PluginID			*plugin_id);			/* << id for plugin to use in other AEGP calls */

	SPAPI A_Err (*AEGP_GetMainHWND)(
						void					*main_hwnd);			/* << */

	SPAPI A_Err (*AEGP_ShowHideAllFloaters)(
						A_Boolean				include_tool_palB);		/* >> */

	SPAPI A_Err (*AEGP_PaintPalGetForeColor)(
						AEGP_ColorVal			*fore_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalGetBackColor)(
						AEGP_ColorVal			*back_colorP);			/* << */

	SPAPI A_Err (*AEGP_PaintPalSetForeColor)(
						const AEGP_ColorVal		*fore_colorP);			/* >> */

	SPAPI A_Err (*AEGP_PaintPalSetBackColor)(
						const AEGP_ColorVal		*back_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalGetFillColor)(
						A_Boolean				*is_fill_color_definedPB,	/*  << */
                        AEGP_ColorVal			*fill_colorP); 				/* << only valid if is_fill_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalGetStrokeColor)(
						A_Boolean				*is_stroke_color_definedPB, /*  << */
                        AEGP_ColorVal			*stroke_colorP); 			/* << only valid if is_stroke_color_definedPB == TRUE */

	SPAPI A_Err (*AEGP_CharPalSetFillColor)(
						const AEGP_ColorVal		*fill_colorP);				/* >> */

	SPAPI A_Err (*AEGP_CharPalSetStrokeColor)(
						const AEGP_ColorVal		*stroke_colorP);			/* >> */

	SPAPI A_Err (*AEGP_CharPalIsFillColorUIFrontmost)(						/* Otherwise, StrokeColor is frontmost */
						A_Boolean				*is_fill_color_selectedPB);	/* << */

} AEGP_UtilitySuite2;

#define kAEGPUtilitySuiteVersion1		3 /* frozen in AE 5.0 */

typedef struct AEGP_UtilitySuite1 {

	SPAPI A_Err	(*AEGP_ReportInfo)(										/* displays dialog with name of plugin followed by info string */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						const A_char			*info_stringZ);			/* >> */


	SPAPI A_Err	(*AEGP_GetDriverPluginInitFuncVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_GetDriverImplementationVersion)(
						A_short					*major_versionPS,		/* << */
						A_short					*minor_versionPS);		/* << */

	SPAPI A_Err	(*AEGP_StartQuietErrors)(
						AEGP_ErrReportState		*err_stateP);			/* << */

	SPAPI A_Err	(*AEGP_EndQuietErrors)(
						A_Boolean				report_quieted_errorsB,	/* >> currently reports last quieted error */
						AEGP_ErrReportState		*err_stateP);			/* >> */

	SPAPI A_Err	(*AEGP_StartUndoGroup)(									/* MUST be balanced with call to AEGP_EndUndoGroup() */
						const A_char			*undo_nameZ);			/* >> */

	SPAPI A_Err	(*AEGP_EndUndoGroup)(void);

	SPAPI A_Err (*AEGP_RegisterWithAEGP)(
						AEGP_GlobalRefcon 		global_refcon,			/* >> global refcon passed in command handlers */
						const A_char			*plugin_nameZ,			/* >> name of this plugin. AEGP_MAX_PLUGIN_NAME_SIZE */
						AEGP_PluginID			*plugin_id);			/* << id for plugin to use in other AEGP calls */

	SPAPI A_Err (*AEGP_GetMainHWND)(
						void					*main_hwnd);			/* << */

} AEGP_UtilitySuite1;




#define kAEGPQueryXformSuiteVersion1		1 /* frozen in AE 5.0 */



typedef struct AEGP_QueryXformSuite1 {

	SPAPI A_Err	(*AEGP_QueryXformGetSrcType)(
					PR_QueryContextH		query_contextH,		/* <> */
					AEGP_QueryXformType		*src_type);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetDstType)(
					PR_QueryContextH		query_contextH,		/* <> */
					AEGP_QueryXformType		*dst_type);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetLayer)(
						PR_QueryContextH	query_contextH,		/* <> */
						AEGP_LayerH			*layerPH);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetComp)(
						PR_QueryContextH	query_contextH,		/* <> */
						AEGP_CompH			*compPH);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetTransformTime)(
						PR_QueryContextH	query_contextH,		/* <> */
						A_Time				*time);				/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetViewTime)(
						PR_QueryContextH	query_contextH,		/* <> */
						A_Time				*time);				/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetCamera)(
						PR_QueryContextH	query_contextH,		/* <> */
						AEGP_LayerH			*camera_layerPH);	/* << */

	SPAPI A_Err	(*AEGP_QueryXformGetXform)(
						PR_QueryContextH	query_contextH,		/* <> */
						A_Matrix4			*xform);			/* << */

	SPAPI A_Err	(*AEGP_QueryXformSetXform)(
						PR_QueryContextH	query_contextH,		/* <> */
						A_Matrix4			*xform);			/* >> */

}AEGP_QueryXformSuite1;




#define	kAEGPRenderSuiteVersion1			1 /* frozen in AE 5.5.1 */


typedef struct {
	SPAPI A_Err (*AEGP_RenderAndCheckoutFrame)(
					AEGP_RenderOptionsH				optionsH,					/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_FrameReceiptH				*receiptPH);				/* << check in using AEGP_CheckinFrame to release memory */

	SPAPI A_Err (*AEGP_CheckinFrame)(
					AEGP_FrameReceiptH		receiptH);							/* >> */

	/*	This returns a read only world that is not-owned by the plugin.
		Call CheckinFrame to release the world when you are done reading from it.
	*/

	SPAPI A_Err (*AEGP_GetReceiptWorld)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					AEGP_WorldH				*worldPH);							/* << */

	SPAPI A_Err (*AEGP_GetRenderedRegion)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					A_LRect					*rendered_regionP);					/* << */

	SPAPI A_Err (*AEGP_IsRenderedFrameSufficient)(
						AEGP_RenderOptionsH rendered_optionsH,					/* >> */
						AEGP_RenderOptionsH proposed_optionsH,					/* >> */
						A_Boolean			*rendered_is_sufficientPB);			/* << */

	SPAPI A_Err (*AEGP_RenderNewItemSoundData)( 								/* Works on Compositions and Footage items. */
					AEGP_ItemH						itemH,						/* >> */
					const A_Time					*start_timePT,				/* >> */
					const A_Time					*durationPT,				/* >> */
					const AEGP_SoundDataFormat		*sound_formatP,				/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_SoundDataH					*new_sound_dataPH);			/* << AEGP_SoundDataH must be disposed. Returns NULL if no audio */

} AEGP_RenderSuite1;


#define	kAEGPRenderSuiteVersion2			2 /* frozen in 6.5 */


typedef A_Err (*AEGP_RenderSuiteCheckForCancelv1)(
					void 		*refcon,
					A_Boolean 	*cancelPB);

typedef struct {
	SPAPI A_Err (*AEGP_RenderAndCheckoutFrame)(
					AEGP_RenderOptionsH				optionsH,					/* >> */
					AEGP_RenderSuiteCheckForCancelv1	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_FrameReceiptH				*receiptPH);				/* << check in using AEGP_CheckinFrame to release memory */

	SPAPI A_Err (*AEGP_CheckinFrame)(
					AEGP_FrameReceiptH		receiptH);							/* >> */

	/*	This returns a read only world that is not-owned by the plugin.
		Call CheckinFrame to release the world when you are done reading from it.
	*/

	SPAPI A_Err (*AEGP_GetReceiptWorld)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					AEGP_WorldH				*worldPH);							/* << */

	SPAPI A_Err (*AEGP_GetRenderedRegion)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					A_LRect					*rendered_regionP);					/* << */

	SPAPI A_Err (*AEGP_IsRenderedFrameSufficient)(
						AEGP_RenderOptionsH rendered_optionsH,					/* >> */
						AEGP_RenderOptionsH proposed_optionsH,					/* >> */
						A_Boolean			*rendered_is_sufficientPB);			/* << */

	SPAPI A_Err (*AEGP_RenderNewItemSoundData)( 								/* Works on Compositions and Footage items. */
					AEGP_ItemH						itemH,						/* >> */
					const A_Time					*start_timePT,				/* >> */
					const A_Time					*durationPT,				/* >> */
					const AEGP_SoundDataFormat		*sound_formatP,				/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_SoundDataH					*new_sound_dataPH);			/* << AEGP_SoundDataH must be disposed. Returns NULL if no audio */


	// returns the current timestamp of the project.this is increased any time something is touched in the project
	// that affects rendering
	SPAPI A_Err (*AEGP_GetCurrentTimestamp)(
							AEGP_TimeStamp * time_stampP); // out

	// Lets you know if the video of the item has changed since the input time stamp.
	// Is not affected by audio.
	SPAPI A_Err (*AEGP_HasItemChangedSinceTimestamp)(AEGP_ItemH itemH,					// in
													const A_Time * start_timeP,			// in
													const A_Time* durationP,			//in
													const AEGP_TimeStamp * time_stampP, //in
													A_Boolean * item_has_changedPB);	//out

	// checks whether this frame would be worth rendering externally and
	// checking in to the cache. a speculative renderer should check this twice:
	// (1) before sending the frame out to render
	// (2) when it is complete, before calling AEGP_NewPlatformWorld and checking in.
	// (don't forget to call AEGP_HasItemChangedSinceTimestamp also!)
	SPAPI A_Err (*AEGP_IsItemWorthwhileToRender)(	AEGP_RenderOptionsH roH,					// in
												const AEGP_TimeStamp* time_stampP,		// in
												A_Boolean *worthwhile_to_renderPB);	// out

	// ticks_to_render is the approximate amount of time needed to render the frame
	// on this machine. it is 60Hz.
	SPAPI A_Err (*AEGP_CheckinRenderedFrame)( AEGP_RenderOptionsH roH,					// in
												const AEGP_TimeStamp* time_stampP,		// in
												A_u_long ticks_to_renderL,				// in
												AEGP_PlatformWorldH	imageH);			// in (adopted)
} AEGP_RenderSuite2;


#define	kAEGPRenderSuiteVersion3			3 /* frozen in 11.0 */

typedef struct {
	SPAPI A_Err (*AEGP_RenderAndCheckoutFrame)(
					AEGP_RenderOptionsH				optionsH,					/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_FrameReceiptH				*receiptPH);				/* << check in using AEGP_CheckinFrame to release memory */

	SPAPI A_Err (*AEGP_CheckinFrame)(
					AEGP_FrameReceiptH		receiptH);							/* >> */

	/*	This returns a read only world that is not-owned by the plugin.
		Call CheckinFrame to release the world when you are done reading from it.
	*/

	SPAPI A_Err (*AEGP_GetReceiptWorld)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					AEGP_WorldH				*worldPH);							/* << */

	SPAPI A_Err (*AEGP_GetRenderedRegion)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					A_LRect					*rendered_regionP);					/* << */

	SPAPI A_Err (*AEGP_IsRenderedFrameSufficient)(
						AEGP_RenderOptionsH rendered_optionsH,					/* >> */
						AEGP_RenderOptionsH proposed_optionsH,					/* >> */
						A_Boolean			*rendered_is_sufficientPB);			/* << */

	SPAPI A_Err (*AEGP_RenderNewItemSoundData)( 								/* Works on Compositions and Footage items. */
					AEGP_ItemH						itemH,						/* >> */
					const A_Time					*start_timePT,				/* >> */
					const A_Time					*durationPT,				/* >> */
					const AEGP_SoundDataFormat		*sound_formatP,				/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_SoundDataH					*new_sound_dataPH);			/* << AEGP_SoundDataH must be disposed. Returns NULL if no audio */


	// returns the current timestamp of the project.this is increased any time something is touched in the project
	// that affects rendering
	SPAPI A_Err (*AEGP_GetCurrentTimestamp)(
							AEGP_TimeStamp * time_stampP); // out

	// Lets you know if the video of the item has changed since the input time stamp.
	// Is not affected by audio.
	SPAPI A_Err (*AEGP_HasItemChangedSinceTimestamp)(AEGP_ItemH itemH,					// in
													const A_Time * start_timeP,			// in
													const A_Time* durationP,			//in
													const AEGP_TimeStamp * time_stampP, //in
													A_Boolean * item_has_changedPB);	//out

	// checks whether this frame would be worth rendering externally and
	// checking in to the cache. a speculative renderer should check this twice:
	// (1) before sending the frame out to render
	// (2) when it is complete, before calling AEGP_NewPlatformWorld and checking in.
	// (don't forget to call AEGP_HasItemChangedSinceTimestamp also!)
	SPAPI A_Err (*AEGP_IsItemWorthwhileToRender)(	AEGP_RenderOptionsH roH,					// in
												const AEGP_TimeStamp* time_stampP,		// in
												A_Boolean *worthwhile_to_renderPB);	// out

	// ticks_to_render is the approximate amount of time needed to render the frame
	// on this machine. it is 60Hz.
	SPAPI A_Err (*AEGP_CheckinRenderedFrame)( AEGP_RenderOptionsH roH,					// in
												const AEGP_TimeStamp* time_stampP,		// in
												A_u_long ticks_to_renderL,				// in
												AEGP_PlatformWorldH	imageH);			// in (adopted)
												
	SPAPI A_Err (*AEGP_GetReceiptGuid) (AEGP_FrameReceiptH receiptH, 	// in
											AEGP_MemHandle *guidMH);	// out, must be disposed
} AEGP_RenderSuite3;


#define	kAEGPRenderSuiteVersion4			5 /* frozen in 13.0 */

typedef struct {
	SPAPI A_Err (*AEGP_RenderAndCheckoutFrame)(
					AEGP_RenderOptionsH				optionsH,					/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_FrameReceiptH				*receiptPH);				/* << check in using AEGP_CheckinFrame to release memory */

	SPAPI A_Err (*AEGP_RenderAndCheckoutLayerFrame)(
					AEGP_LayerRenderOptionsH			optionsH,					/* >> */
					A_Boolean							render_plain_layer_frameB,	/* >> true to render layer frame, false to render effect input frame */
					AEGP_RenderSuiteCheckForCancel		cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon					cancel_function_refconP0,	/* >> optional */
					AEGP_FrameReceiptH					*receiptPH);				/* << check in using AEGP_CheckinFrame to release memory */

	SPAPI A_Err (*AEGP_CheckinFrame)(
					AEGP_FrameReceiptH		receiptH);							/* >> */

	/*	This returns a read only world that is not-owned by the plugin.
		Call CheckinFrame to release the world when you are done reading from it.
	*/

	SPAPI A_Err (*AEGP_GetReceiptWorld)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					AEGP_WorldH				*worldPH);							/* << */

	SPAPI A_Err (*AEGP_GetRenderedRegion)(
					AEGP_FrameReceiptH		receiptH,							/* >> */
					A_LRect					*rendered_regionP);					/* << */

	SPAPI A_Err (*AEGP_IsRenderedFrameSufficient)(
						AEGP_RenderOptionsH rendered_optionsH,					/* >> */
						AEGP_RenderOptionsH proposed_optionsH,					/* >> */
						A_Boolean			*rendered_is_sufficientPB);			/* << */

	SPAPI A_Err (*AEGP_RenderNewItemSoundData)( 								/* Works on Compositions and Footage items. */
					AEGP_ItemH						itemH,						/* >> */
					const A_Time					*start_timePT,				/* >> */
					const A_Time					*durationPT,				/* >> */
					const AEGP_SoundDataFormat		*sound_formatP,				/* >> */
					AEGP_RenderSuiteCheckForCancel	cancel_functionP0,			/* >> optional*/
					AEGP_CancelRefcon				cancel_function_refconP0, 	/* >> optional */
					AEGP_SoundDataH					*new_sound_dataPH);			/* << AEGP_SoundDataH must be disposed. Returns NULL if no audio */


	// returns the current timestamp of the project.this is increased any time something is touched in the project
	// that affects rendering
	SPAPI A_Err (*AEGP_GetCurrentTimestamp)(
							AEGP_TimeStamp * time_stampP); // out

	// Lets you know if the video of the item has changed since the input time stamp.
	// Is not affected by audio.
	SPAPI A_Err (*AEGP_HasItemChangedSinceTimestamp)(AEGP_ItemH itemH,					// in
													const A_Time * start_timeP,			// in
													const A_Time* durationP,			//in
													const AEGP_TimeStamp * time_stampP, //in
													A_Boolean * item_has_changedPB);	//out

	// checks whether this frame would be worth rendering externally and
	// checking in to the cache. a speculative renderer should check this twice:
	// (1) before sending the frame out to render
	// (2) when it is complete, before calling AEGP_NewPlatformWorld and checking in.
	// (don't forget to call AEGP_HasItemChangedSinceTimestamp also!)
	SPAPI A_Err (*AEGP_IsItemWorthwhileToRender)(	AEGP_RenderOptionsH roH,					// in
												const AEGP_TimeStamp* time_stampP,		// in
												A_Boolean *worthwhile_to_renderPB);	// out

	// ticks_to_render is the approximate amount of time needed to render the frame
	// on this machine. it is 60Hz.
	SPAPI A_Err (*AEGP_CheckinRenderedFrame)( AEGP_RenderOptionsH roH,					// in
												const AEGP_TimeStamp* time_stampP,		// in
												A_u_long ticks_to_renderL,				// in
												AEGP_PlatformWorldH	imageH);			// in (adopted)
												
	SPAPI A_Err (*AEGP_GetReceiptGuid) (AEGP_FrameReceiptH receiptH, 	// in
											AEGP_MemHandle *guidMH);	// out, must be disposed
} AEGP_RenderSuite4;



#define	kAEGPWorldSuiteVersion2			2 /* frozen in AE 6.5 */

typedef struct {
	SPAPI A_Err (*AEGP_New)(
		AEGP_PluginID	plugin_id,			/* >> */
		AEGP_WorldType	type,				/* >> */
		A_long			widthL,				/* >> */
		A_long			heightL,			/* >> */
		AEGP_WorldH		*worldPH);			/* << */

	SPAPI A_Err (*AEGP_Dispose)(
		AEGP_WorldH		worldH);			/* >> */

	SPAPI A_Err (*AEGP_GetType)(
		AEGP_WorldH		worldH,				/* >> */
		AEGP_WorldType	*typeP);			/* << */

	SPAPI A_Err (*AEGP_GetSize)(
		AEGP_WorldH		worldH,				/* >> */
		A_long			*widthPL,			/* << */
		A_long			*heightPL);			/* << */

	SPAPI A_Err (*AEGP_GetRowBytes)(
		AEGP_WorldH		worldH,				/* >> */
		A_u_long			*row_bytesPL);	/* << */

	SPAPI A_Err (*AEGP_GetBaseAddr8)(
		AEGP_WorldH		worldH,				/* >> error if the worldH is not AEGP_WorldType_8 */
		PF_Pixel8		**base_addrP);		/* << */

	SPAPI A_Err (*AEGP_GetBaseAddr16)(
		AEGP_WorldH		worldH,				/* >> error if the worldH is not AEGP_WorldType_16 */
		PF_Pixel16		**base_addrP);		/* << */

	SPAPI A_Err (*AEGP_FillOutPFEffectWorld)(				/*	Provided so you can use some of the PF routines with an AEGPWorld. Pass NULL as the ProgPtr to the PF routines.*/
		AEGP_WorldH		worldH,				/* >> */
		PF_EffectWorld	*pf_worldP);		/* << */

	SPAPI A_Err (*AEGP_FastBlur)(
		A_FpLong		radiusF,			/* >> */
		PF_ModeFlags	mode,				/* >> */
		PF_Quality		quality,			/* >> */
		AEGP_WorldH		worldH);			/* <>  only for user allocated worlds; not for checked-out frames which are read only */

	SPAPI A_Err (*AEGP_NewPlatformWorld)(
		AEGP_PluginID	plugin_id,			/* >> */
		AEGP_WorldType	type,				/* >> */
		A_long			widthL,				/* >> */
		A_long			heightL,			/* >> */
		AEGP_PlatformWorldH		*worldPH);			/* << */

	SPAPI A_Err (*AEGP_DisposePlatformWorld)(
		AEGP_PlatformWorldH		worldH);			/* >> */

	SPAPI A_Err (*AEGP_NewReferenceFromPlatformWorld)(
		AEGP_PluginID	plugin_id,			/* >> */
		AEGP_PlatformWorldH platform_worldH, // >>
		AEGP_WorldH		*worldPH);			/* << */


} AEGP_WorldSuite2;


#define	kAEGPWorldSuite					"AEGP World Suite"
#define	kAEGPWorldSuiteVersion1			1 /* frozen AE 6.0 */

typedef struct {
	SPAPI A_Err (*AEGP_New)(
		AEGP_PluginID	plugin_id,			/* >> */
		AEGP_WorldType	type,				/* >> */
		A_long			widthL,				/* >> */
		A_long			heightL,			/* >> */
		AEGP_WorldH		*worldPH);			/* << */

	SPAPI A_Err (*AEGP_Dispose)(
		AEGP_WorldH		worldH);			/* >> */

	SPAPI A_Err (*AEGP_GetType)(
		AEGP_WorldH		worldH,				/* >> */
		AEGP_WorldType	*typeP);			/* << */

	SPAPI A_Err (*AEGP_GetSize)(
		AEGP_WorldH		worldH,				/* >> */
		A_long			*widthPL,			/* << */
		A_long			*heightPL);			/* << */

	SPAPI A_Err (*AEGP_GetRowBytes)(
		AEGP_WorldH		worldH,				/* >> */
		A_u_long			*row_bytesPL);	/* << */

	SPAPI A_Err (*AEGP_GetBaseAddr8)(
		AEGP_WorldH		worldH,				/* >> error if the worldH is not AEGP_WorldType_8 */
		PF_Pixel8		**base_addrP);		/* << */

	SPAPI A_Err (*AEGP_GetBaseAddr16)(
		AEGP_WorldH		worldH,				/* >> error if the worldH is not AEGP_WorldType_16 */
		PF_Pixel16		**base_addrP);		/* << */

	SPAPI A_Err (*AEGP_FillOutPFEffectWorld)(				/*	Provided so you can use some of the PF routines with an AEGPWorld. Pass NULL as the ProgPtr to the PF routines.*/
		AEGP_WorldH		worldH,				/* >> */
		PF_EffectWorld	*pf_worldP);		/* << */

	SPAPI A_Err (*AEGP_FastBlur)(
		A_FpLong		radiusF,			/* >> */
		PF_ModeFlags	mode,				/* >> */
		PF_Quality		quality,			/* >> */
		AEGP_WorldH		worldH);			/* <>  only for user allocated worlds; not for checked-out frames which are read only */
} AEGP_WorldSuite1;


typedef struct {
	AEGP_CollectionItemType	type;
	union {
		AEGP_LayerCollectionItem		layer;
		AEGP_MaskCollectionItem			mask;
		AEGP_EffectCollectionItem		effect;
		AEGP_StreamCollectionItem		stream;
		AEGP_MaskVertexCollectionItem	mask_vertex;
		AEGP_KeyframeCollectionItem		keyframe;
	} u;
} AEGP_CollectionItem;


#define	kAEGPCollectionSuiteVersion1			1 /* frozen in AE 5.0 */

typedef struct {
	SPAPI A_Err (*AEGP_NewCollection)(									/* dispose with dispose collection */
		AEGP_PluginID	plugin_id,						/* >> */
		AEGP_CollectionH *collectionPH);				/* << */

	SPAPI A_Err (*AEGP_DisposeCollection)(
		AEGP_CollectionH collectionH);					/* >> */

	SPAPI A_Err	(*AEGP_GetCollectionNumItems)(							/* constant time */
		AEGP_CollectionH	collectionH,				/* >> */
		A_u_long		*num_itemsPL);					/* << */

	SPAPI A_Err	(*AEGP_GetCollectionItemByIndex)(						/* constant time */
		AEGP_CollectionH	collectionH,				/* >> */
		A_u_long			indexL,						/* >> */
		AEGP_CollectionItem	*collection_itemP);			/* << */

	SPAPI A_Err (*AEGP_CollectionPushBack)(								/* constant time */
		AEGP_CollectionH			collectionH,		/* <> */
		const AEGP_CollectionItem	*collection_itemP);	/* >> */

	SPAPI A_Err (*AEGP_CollectionErase)(								/* O(n) */
		AEGP_CollectionH	collectionH,				/* <> */
		A_u_long			index_firstL,				/* >> */
		A_u_long			index_lastL);				/* >> */

} AEGP_CollectionSuite1;

#define kAEGPDynamicStreamSuiteVersion1		1	/* frozen in AE 6.0 */

typedef struct AEGP_DynamicStreamSuite1 {

	SPAPI A_Err	(*AEGP_GetNewStreamRefForLayer)(						// used to start recursive walk of layer,
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_LayerH				layerH,					/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetStreamDepth)(									// layer is depth 0
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*depthPL);				/* << */


	SPAPI A_Err	(*AEGP_GetStreamGroupingType)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_StreamGroupingType	*group_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetNumStreamsInGroup)(	// error on leaf
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*num_streamsPL);		/* << */


	SPAPI A_Err	(*AEGP_GetDynamicStreamFlags)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_DynStreamFlags		*stream_flagsP);		/* << */

	SPAPI A_Err	(*AEGP_SetDynamicStreamFlag)(							/* UNDOABLE */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_DynStreamFlags		one_flag,				/* >> */
						A_Boolean				setB);					/* >> */


	SPAPI A_Err	(*AEGP_GetNewStreamRefByIndex)(		// legal for namedgroup, indexedgroup, not leaf
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			parent_groupH,			/* >> */
						A_long					indexL,					/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewStreamRefByMatchname)(	// legal for namedgroup
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			parent_groupH,			/* >> */
						const A_char			*match_nameZ,			/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_DeleteStream)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH);			/* >> */ // must still dispose the streamref later

	SPAPI A_Err	(*AEGP_ReorderStream)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH,			/* <> updated to refer to newly ordered stream */
						A_long 					new_indexL);		/* >> */

	SPAPI A_Err (*AEGP_DuplicateStream)(	/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*new_indexPL0);			/* << */

	/* GetStreamName is in main kAEGPStreamSuite, and works on dynamic streams including groups */

	SPAPI A_Err	(*AEGP_SetStreamName)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH,				/* >> */
						const A_char			*nameZ);				/* >> */

	SPAPI A_Err (*AEGP_CanAddStream)(
						AEGP_StreamRefH			group_streamH,			/* >> */
						const A_char			*match_nameZ,			/* >> */
						A_Boolean				*can_addPB);			/* << */

	SPAPI A_Err (*AEGP_AddStream)(				/* UNDOABLE, only valid for AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			indexed_group_streamH,	/* >> */
						const A_char			*match_nameZ,
						AEGP_StreamRefH			*streamPH0);			/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetMatchName)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_char					*nameZ);				/* << use A_char[AEGP_MAX_STREAM_MATCH_NAME_SIZE] for buffer */

	SPAPI A_Err (*AEGP_GetNewParentStreamRef)(
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_StreamRefH			*parent_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetStreamIsModified)(	// i.e. changed from defaults, like the UU key
						AEGP_StreamRefH			streamH,				/* >> */
						A_Boolean				*modifiedPB);			/* << */

} AEGP_DynamicStreamSuite1;

#define kAEGPDynamicStreamSuiteVersion2		2	/* frozen in AE 6.5 */

typedef struct AEGP_DynamicStreamSuite2 {

	SPAPI A_Err	(*AEGP_GetNewStreamRefForLayer)(						// used to start recursive walk of layer,
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_LayerH				layerH,					/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetStreamDepth)(									// layer is depth 0
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*depthPL);				/* << */


	SPAPI A_Err	(*AEGP_GetStreamGroupingType)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_StreamGroupingType	*group_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetNumStreamsInGroup)(	// error on leaf
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*num_streamsPL);		/* << */


	SPAPI A_Err	(*AEGP_GetDynamicStreamFlags)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_DynStreamFlags		*stream_flagsP);		/* << */

	SPAPI A_Err	(*AEGP_SetDynamicStreamFlag)(
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_DynStreamFlags		one_flag,				/* >> */
						A_Boolean				undoableB,				/* true if you want this to be an undoable change */
																		/* if false, the only legal flag is AEGP_DynStreamFlag_HIDDEN */
						A_Boolean				setB);					/* >> */


	SPAPI A_Err	(*AEGP_GetNewStreamRefByIndex)(		// legal for namedgroup, indexedgroup, not leaf
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			parent_groupH,			/* >> */
						A_long					indexL,					/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewStreamRefByMatchname)(	// legal for namedgroup
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			parent_groupH,			/* >> */
						const A_char			*match_nameZ,			/* >> */
						AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_DeleteStream)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH);			/* >> */ // must still dispose the streamref later

	SPAPI A_Err	(*AEGP_ReorderStream)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH,			/* <> updated to refer to newly ordered stream */
						A_long 					new_indexL);		/* >> */

	SPAPI A_Err (*AEGP_DuplicateStream)(	/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						A_long					*new_indexPL0);			/* << */

	/* GetStreamName is in main kAEGPStreamSuite, and works on dynamic streams including groups */

	SPAPI A_Err	(*AEGP_SetStreamName)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_StreamRefH			streamH,				/* >> */
						const A_char			*nameZ);				/* >> */

	SPAPI A_Err (*AEGP_CanAddStream)(
						AEGP_StreamRefH			group_streamH,			/* >> */
						const A_char			*match_nameZ,			/* >> */
						A_Boolean				*can_addPB);			/* << */

	SPAPI A_Err (*AEGP_AddStream)(				/* UNDOABLE, only valid for AEGP_StreamGroupingType_INDEXED_GROUP */
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			indexed_group_streamH,	/* >> */
						const A_char			*match_nameZ,
						AEGP_StreamRefH			*streamPH0);			/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetMatchName)(
						AEGP_StreamRefH			streamH,				/* >> */
						A_char					*nameZ);				/* << use A_char[AEGP_MAX_STREAM_MATCH_NAME_SIZE] for buffer */

	SPAPI A_Err (*AEGP_GetNewParentStreamRef)(
						AEGP_PluginID			aegp_plugin_id,			/* >> */
						AEGP_StreamRefH			streamH,				/* >> */
						AEGP_StreamRefH			*parent_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetStreamIsModified)(	// i.e. changed from defaults, like the UU key
						AEGP_StreamRefH			streamH,				/* >> */
						A_Boolean				*modifiedPB);			/* << */

} AEGP_DynamicStreamSuite2;

#define kAEGPDynamicStreamSuiteVersion3		3	/* frozen in AE 7.0 */

typedef struct AEGP_DynamicStreamSuite3 {

	SPAPI A_Err	(*AEGP_GetNewStreamRefForLayer)(						// used to start recursive walk of layer,
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_LayerH				layerH,					/* >> */
		AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetStreamDepth)(									// layer is depth 0
		AEGP_StreamRefH			streamH,				/* >> */
		A_long					*depthPL);				/* << */


	SPAPI A_Err	(*AEGP_GetStreamGroupingType)(
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_StreamGroupingType	*group_typeP);			/* << */

	SPAPI A_Err	(*AEGP_GetNumStreamsInGroup)(	// error on leaf
		AEGP_StreamRefH			streamH,				/* >> */
		A_long					*num_streamsPL);		/* << */


	SPAPI A_Err	(*AEGP_GetDynamicStreamFlags)(
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_DynStreamFlags		*stream_flagsP);		/* << */

	SPAPI A_Err	(*AEGP_SetDynamicStreamFlag)(
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_DynStreamFlags		one_flag,				/* >> */
		A_Boolean				undoableB,				/* true if you want this to be an undoable change */
		/* if false, the only legal flag is AEGP_DynStreamFlag_HIDDEN */
		A_Boolean				setB);					/* >> */


	SPAPI A_Err	(*AEGP_GetNewStreamRefByIndex)(		// legal for namedgroup, indexedgroup, not leaf
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_StreamRefH			parent_groupH,			/* >> */
		A_long					indexL,					/* >> */
		AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_GetNewStreamRefByMatchname)(	// legal for namedgroup
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_StreamRefH			parent_groupH,			/* >> */
		const A_char			*match_nameZ,			/* >> */
		AEGP_StreamRefH 		*streamPH);				/* << must be disposed by caller! */

	SPAPI A_Err	(*AEGP_DeleteStream)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
		AEGP_StreamRefH			streamH);			/* >> */ // must still dispose the streamref later

	SPAPI A_Err	(*AEGP_ReorderStream)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
		AEGP_StreamRefH			streamH,			/* <> updated to refer to newly ordered stream */
		A_long 					new_indexL);		/* >> */

	SPAPI A_Err (*AEGP_DuplicateStream)(	/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_StreamRefH			streamH,				/* >> */
		A_long					*new_indexPL0);			/* << */

	/* GetStreamName is in main kAEGPStreamSuite, and works on dynamic streams including groups */

	SPAPI A_Err	(*AEGP_SetStreamName)(		/* UNDOABLE, only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP */
		AEGP_StreamRefH			streamH,				/* >> */
		const A_char			*nameZ);				/* >> */

	SPAPI A_Err (*AEGP_CanAddStream)(
		AEGP_StreamRefH			group_streamH,			/* >> */
		const A_char			*match_nameZ,			/* >> */
		A_Boolean				*can_addPB);			/* << */

	SPAPI A_Err (*AEGP_AddStream)(				/* UNDOABLE, only valid for AEGP_StreamGroupingType_INDEXED_GROUP */
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_StreamRefH			indexed_group_streamH,	/* >> */
		const A_char			*match_nameZ,
		AEGP_StreamRefH			*streamPH0);			/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetMatchName)(
		AEGP_StreamRefH			streamH,				/* >> */
		A_char					*nameZ);				/* << use A_char[AEGP_MAX_STREAM_MATCH_NAME_SIZE] for buffer */

	SPAPI A_Err (*AEGP_GetNewParentStreamRef)(
		AEGP_PluginID			aegp_plugin_id,			/* >> */
		AEGP_StreamRefH			streamH,				/* >> */
		AEGP_StreamRefH			*parent_streamPH);		/* << must be disposed by caller! */

	SPAPI A_Err (*AEGP_GetStreamIsModified)(	// i.e. changed from defaults, like the UU key
		AEGP_StreamRefH			streamH,				/* >> */
		A_Boolean				*modifiedPB);			/* << */

	SPAPI A_Err (*AEGP_GetStreamIndexInParent)(	// only valid for children of AEGP_StreamGroupingType_INDEXED_GROUP
		AEGP_StreamRefH			streamH,				/* >> */
		A_long 					*indexPL);				/* << */



} AEGP_DynamicStreamSuite3;


#define	kAEGPRenderOptionsSuite					"AEGP Render Options Suite"
#define	kAEGPRenderOptionsSuiteVersion3			3 /* frozen in AE 7.01 */

typedef struct {
	// fills out
	// Time to 0
	// Time step to the frame duration
	// field render to none
	// depth is best resolution of item
	SPAPI A_Err (*AEGP_NewFromItem)(
					AEGP_PluginID		plugin_id,			/* >> */
					AEGP_ItemH			itemH,				/* >> */
					AEGP_RenderOptionsH	*optionsPH);		/* << */

	SPAPI A_Err (*AEGP_Duplicate)(
					AEGP_PluginID		plugin_id,			/* >> */
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_RenderOptionsH	*copyPH);			/* << */

	SPAPI A_Err (*AEGP_Dispose)(
					AEGP_RenderOptionsH	optionsH);			/* >> */

	SPAPI A_Err (*AEGP_SetTime)(							/* the render time */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_Time				time);				/* >> */

	SPAPI A_Err (*AEGP_GetTime)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Time				*timeP);			/* << */

	SPAPI A_Err (*AEGP_SetTimeStep)(						/* duration of the frame; important for motion blur. */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_Time				time_step);			/* >> */

	SPAPI A_Err (*AEGP_GetTimeStep)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Time				*timePT);			/* << */

	SPAPI A_Err (*AEGP_SetFieldRender)(						/* How fields are to be handled. */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					PF_Field			field_render);		/* >> */

	SPAPI A_Err (*AEGP_GetFieldRender)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					PF_Field			*field_renderP);	/* << */


	SPAPI A_Err (*AEGP_SetWorldType)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					AEGP_WorldType		type);				/* >> */

	SPAPI A_Err (*AEGP_GetWorldType)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_WorldType		*typeP);			/* << */


	// 1 == 100%
	// 2 == 50%
	// ...
	SPAPI A_Err (*AEGP_SetDownsampleFactor)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_short		x,							/* >> */
					A_short		y);							/* >> */

	SPAPI A_Err (*AEGP_GetDownsampleFactor)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_short				*xP,				/* >> */
					A_short				*yP);				/* << */

	SPAPI A_Err	(*AEGP_SetRegionOfInterest)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					const A_LRect		*roiP);				/* >>  {0,0,0,0} for all*/

	SPAPI A_Err (*AEGP_GetRegionOfInterest)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_LRect				*roiP);				/* << */

	SPAPI A_Err	(*AEGP_SetMatteMode)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					AEGP_MatteMode		mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMatteMode)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_MatteMode		*modeP);			/* << */

	SPAPI A_Err	(*AEGP_SetChannelOrder)(
					AEGP_RenderOptionsH optionsH,			/* <> */
					AEGP_ChannelOrder	channel_order);		/* >> */

	SPAPI A_Err (*AEGP_GetChannelOrder)(
					AEGP_RenderOptionsH optionsH,			/* >> */
					AEGP_ChannelOrder	*channelP);			/* << */

	SPAPI A_Err (*AEGP_GetRenderGuideLayers)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Boolean			*will_renderPB);	/* << */

	SPAPI A_Err (*AEGP_SetRenderGuideLayers)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Boolean			render_themB);		/* >> */

} AEGP_RenderOptionsSuite3;


#define	kAEGPRenderOptionsSuite					"AEGP Render Options Suite"
#define	kAEGPRenderOptionsSuiteVersion2			2 /* frozen in AE 7.0 */

typedef struct {
	// fills out
	// Time to 0
	// Time step to the frame duration
	// field render to none
	// depth is best resolution of item
	SPAPI A_Err (*AEGP_NewFromItem)(
					AEGP_PluginID		plugin_id,			/* >> */
					AEGP_ItemH			itemH,				/* >> */
					AEGP_RenderOptionsH	*optionsPH);		/* << */

	SPAPI A_Err (*AEGP_Duplicate)(
					AEGP_PluginID		plugin_id,			/* >> */
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_RenderOptionsH	*copyPH);			/* << */

	SPAPI A_Err (*AEGP_Dispose)(
					AEGP_RenderOptionsH	optionsH);			/* >> */

	SPAPI A_Err (*AEGP_SetTime)(							/* the render time */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_Time				time);				/* >> */

	SPAPI A_Err (*AEGP_GetTime)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Time				*timeP);			/* << */

	SPAPI A_Err (*AEGP_SetTimeStep)(						/* duration of the frame; important for motion blur. */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_Time				time_step);			/* >> */

	SPAPI A_Err (*AEGP_GetTimeStep)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Time				*timePT);			/* << */

	SPAPI A_Err (*AEGP_SetFieldRender)(						/* How fields are to be handled. */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					PF_Field			field_render);		/* >> */

	SPAPI A_Err (*AEGP_GetFieldRender)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					PF_Field			*field_renderP);	/* << */


	SPAPI A_Err (*AEGP_SetWorldType)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					AEGP_WorldType		type);				/* >> */

	SPAPI A_Err (*AEGP_GetWorldType)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_WorldType		*typeP);			/* << */


	// 1 == 100%
	// 2 == 50%
	// ...
	SPAPI A_Err (*AEGP_SetDownsampleFactor)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_short		x,							/* >> */
					A_short		y);							/* >> */

	SPAPI A_Err (*AEGP_GetDownsampleFactor)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_short				*xP,				/* >> */
					A_short				*yP);				/* << */

	SPAPI A_Err	(*AEGP_SetRegionOfInterest)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					const A_LRect		*roiP);				/* >>  {0,0,0,0} for all*/

	SPAPI A_Err (*AEGP_GetRegionOfInterest)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_LRect				*roiP);				/* << */

	SPAPI A_Err	(*AEGP_SetMatteMode)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					AEGP_MatteMode		mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMatteMode)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_MatteMode		*modeP);			/* << */

	SPAPI A_Err	(*AEGP_SetChannelOrder)(
					AEGP_RenderOptionsH optionsH,			/* <> */
					AEGP_ChannelOrder	channel_order);		/* >> */

	SPAPI A_Err (*AEGP_GetChannelOrder)(
					AEGP_RenderOptionsH optionsH,			/* >> */
					AEGP_ChannelOrder	*channelP);			/* << */
} AEGP_RenderOptionsSuite2;


#define	kAEGPRenderOptionsSuite					"AEGP Render Options Suite"
#define	kAEGPRenderOptionsSuiteVersion1			1 /* frozen in AE 5.5.1 */

typedef struct {
	// fills out
	// Time to 0
	// Time step to the frame duration
	// field render to none
	// depth is best resolution of item
	SPAPI A_Err (*AEGP_NewFromItem)(
					AEGP_PluginID		plugin_id,			/* >> */
					AEGP_ItemH			itemH,				/* >> */
					AEGP_RenderOptionsH	*optionsPH);		/* << */

	SPAPI A_Err (*AEGP_Duplicate)(
					AEGP_PluginID		plugin_id,			/* >> */
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_RenderOptionsH	*copyPH);			/* << */

	SPAPI A_Err (*AEGP_Dispose)(
					AEGP_RenderOptionsH	optionsH);			/* >> */

	SPAPI A_Err (*AEGP_SetTime)(							/* the render time */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_Time				time);				/* >> */

	SPAPI A_Err (*AEGP_GetTime)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Time				*timeP);			/* << */

	SPAPI A_Err (*AEGP_SetTimeStep)(						/* duration of the frame; important for motion blur. */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_Time				time_step);			/* >> */

	SPAPI A_Err (*AEGP_GetTimeStep)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_Time				*timePT);			/* << */

	SPAPI A_Err (*AEGP_SetFieldRender)(						/* How fields are to be handled. */
					AEGP_RenderOptionsH	optionsH,			/* <> */
					PF_Field			field_render);		/* >> */

	SPAPI A_Err (*AEGP_GetFieldRender)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					PF_Field			*field_renderP);	/* << */


	SPAPI A_Err (*AEGP_SetWorldType)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					AEGP_WorldType		type);				/* >> */

	SPAPI A_Err (*AEGP_GetWorldType)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_WorldType		*typeP);			/* << */


	// 1 == 100%
	// 2 == 50%
	// ...
	SPAPI A_Err (*AEGP_SetDownsampleFactor)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					A_short		x,							/* >> */
					A_short		y);							/* >> */

	SPAPI A_Err (*AEGP_GetDownsampleFactor)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_short				*xP,				/* >> */
					A_short				*yP);				/* << */

	SPAPI A_Err	(*AEGP_SetRegionOfInterest)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					const A_LRect		*roiP);				/* >>  {0,0,0,0} for all*/

	SPAPI A_Err (*AEGP_GetRegionOfInterest)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					A_LRect				*roiP);				/* << */

	SPAPI A_Err	(*AEGP_SetMatteMode)(
					AEGP_RenderOptionsH	optionsH,			/* <> */
					AEGP_MatteMode		mode);				/* >> */

	SPAPI A_Err (*AEGP_GetMatteMode)(
					AEGP_RenderOptionsH	optionsH,			/* >> */
					AEGP_MatteMode		*modeP);			/* << */

} AEGP_RenderOptionsSuite1;




#define	kAEGPLayerRenderOptionsSuite					"AEGP Layer Render Options Suite"
#define	kAEGPLayerRenderOptionsSuiteVersion1			1 /* frozen in 13.0 */

typedef struct {
	// optionsPH must be disposed by calling code
	//
	// fills out
	// Time to the layer's current time
	// Time step to layer's frame duration
	// ROI to the layer's nominal bounds
	// EffectsToRender to "all"
	SPAPI A_Err (*AEGP_NewFromLayer)(
					AEGP_PluginID				plugin_id,		/* >> */
					AEGP_LayerH					layerH,			/* >> */
					AEGP_LayerRenderOptionsH	*optionsPH);	/* << */

	// optionsPH must be disposed by calling code
	// like AEGP_NewFromLayer, but sets EffectsToRender to be the index fof effectH
	SPAPI A_Err (*AEGP_NewFromUpstreamOfEffect)(
					AEGP_PluginID				plugin_id,		/* >> */
					AEGP_EffectRefH				effectH,		/* >> */
					AEGP_LayerRenderOptionsH	*optionsPH);	/* << */

	// copyPH must be disposed by calling code
	SPAPI A_Err (*AEGP_Duplicate)(
					AEGP_PluginID				plugin_id,		/* >> */
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					AEGP_LayerRenderOptionsH	*copyPH);		/* << */

	SPAPI A_Err (*AEGP_Dispose)(
					AEGP_LayerRenderOptionsH	optionsH);		/* >> */

	SPAPI A_Err (*AEGP_SetTime)(								/* the render time */
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					A_Time						time);			/* >> */

	SPAPI A_Err (*AEGP_GetTime)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					A_Time				*timeP);				/* << */

	SPAPI A_Err (*AEGP_SetTimeStep)(							/* duration of the frame; important for motion blur. */
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					A_Time						time_step);		/* >> */

	SPAPI A_Err (*AEGP_GetTimeStep)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					A_Time						*timePT);		/* << */

	SPAPI A_Err (*AEGP_SetWorldType)(
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					AEGP_WorldType				type);			/* >> */

	SPAPI A_Err (*AEGP_GetWorldType)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					AEGP_WorldType				*typeP);		/* << */

	// 1 == 100%
	// 2 == 50%
	// ...
	SPAPI A_Err (*AEGP_SetDownsampleFactor)(
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					A_short						x,				/* >> */
					A_short						y);				/* >> */

	SPAPI A_Err (*AEGP_GetDownsampleFactor)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					A_short						*xP,			/* >> */
					A_short						*yP);			/* << */

	SPAPI A_Err	(*AEGP_SetMatteMode)(
					AEGP_LayerRenderOptionsH	optionsH,		/* <> */
					AEGP_MatteMode				mode);			/* >> */

	SPAPI A_Err (*AEGP_GetMatteMode)(
					AEGP_LayerRenderOptionsH	optionsH,		/* >> */
					AEGP_MatteMode				*modeP);		/* << */
} AEGP_LayerRenderOptionsSuite1;




#define kAEGPColorSettingsSuiteVersion1	1	// frozen in AE 7.0

typedef struct AEGP_ColorSettingsSuite1 {

	 SPAPI A_Err (*AEGP_GetBlendingTables)(
		PR_RenderContextH 		render_contextH,
		PF_EffectBlendingTables *blending_tables);

} AEGP_ColorSettingsSuite1;

#define kAEGPColorSettingsSuiteVersion2	3	// frozen in AE 8.0

typedef struct AEGP_ColorSettingsSuite2 {

	 SPAPI	A_Err (*AEGP_GetBlendingTables)(
		PR_RenderContextH 		render_contextH,
		PF_EffectBlendingTables *blending_tables);

	SPAPI	A_Err (*AEGP_DoesViewHaveColorSpaceXform)(
		AEGP_ItemViewP	viewP,			// >>
		A_Boolean		*has_xformPB);	// <<

	SPAPI	A_Err (*AEGP_XformWorkingToViewColorSpace)(
		AEGP_ItemViewP	viewP,		// >>
		AEGP_WorldH		srcH,		// in
		AEGP_WorldH		dstH);		// out; must be the same size (can be the same as source)

	SPAPI	A_Err (*AEGP_GetNewWorkingSpaceColorProfile)(
		AEGP_PluginID		aegp_plugin_id,					// >>
		AEGP_CompH			compH,							// >>
		AEGP_ColorProfileP	*color_profilePP);				// << caller must dispose with AEGP_DisposeColorProfile

	SPAPI	A_Err (*AEGP_GetNewColorProfileFromICCProfile)(
		AEGP_PluginID		aegp_plugin_id,		// >>
		A_long				icc_sizeL,			// >>	icc profile size
		const	void		*icc_dataPV,		// >>	icc profile
		AEGP_ColorProfileP	*color_profilePP);	// <<	builds AEGP_ColorProfile from icc profile; caller must dispose with AEGP_DisposeColorProfile

	SPAPI	A_Err (*AEGP_GetNewICCProfileFromColorProfile)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstColorProfileP	color_profileP,		// >>
		AEGP_MemHandle			*icc_profilePH);	// <<	extract icc profile from AEGP_ColorProfile; caller must dispose with AEGP_FreeMemHandle

	SPAPI	A_Err (*AEGP_GetNewColorProfileDescription)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstColorProfileP	color_profileP,		// >>
		AEGP_MemHandle			*unicode_descPH);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI	A_Err (*AEGP_DisposeColorProfile)(
		AEGP_ColorProfileP	color_profileP);	// >>

	SPAPI	A_Err (*AEGP_GetColorProfileApproximateGamma)(
		AEGP_ConstColorProfileP	color_profileP,		// >>
		A_FpShort				*approx_gammaP);	// <<

	SPAPI	A_Err (*AEGP_IsRGBColorProfile)(
		AEGP_ConstColorProfileP	color_profileP,		// <<
		A_Boolean				*is_rgbPB);			// >>


} AEGP_ColorSettingsSuite2;

#define kAEGPColorSettingsSuiteVersion3	4	// frozen in AE 16.1; adding an API to set working color space

typedef struct AEGP_ColorSettingsSuite3 {

	 SPAPI	A_Err (*AEGP_GetBlendingTables)(
		PR_RenderContextH 		render_contextH,
		PF_EffectBlendingTables *blending_tables);
		
	SPAPI	A_Err (*AEGP_DoesViewHaveColorSpaceXform)(
		AEGP_ItemViewP	viewP,			// >>
		A_Boolean		*has_xformPB);	// <<
		
	SPAPI	A_Err (*AEGP_XformWorkingToViewColorSpace)(
		AEGP_ItemViewP	viewP,		// >>
		AEGP_WorldH		srcH,		// in
		AEGP_WorldH		dstH);		// out; must be the same size (can be the same as source)
		
	SPAPI	A_Err (*AEGP_GetNewWorkingSpaceColorProfile)(
		AEGP_PluginID		aegp_plugin_id,					// >>
		AEGP_CompH			compH,							// >>
		AEGP_ColorProfileP	*color_profilePP);				// << caller must dispose with AEGP_DisposeColorProfile
		
	SPAPI	A_Err (*AEGP_GetNewColorProfileFromICCProfile)(
		AEGP_PluginID		aegp_plugin_id,		// >>
		A_long				icc_sizeL,			// >>	icc profile size
		const	void		*icc_dataPV,		// >>	icc profile
		AEGP_ColorProfileP	*color_profilePP);	// <<	builds AEGP_ColorProfile from icc profile; caller must dispose with AEGP_DisposeColorProfile
		
	SPAPI	A_Err (*AEGP_GetNewICCProfileFromColorProfile)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstColorProfileP	color_profileP,		// >>
		AEGP_MemHandle			*icc_profilePH);	// <<	extract icc profile from AEGP_ColorProfile; caller must dispose with AEGP_FreeMemHandle
		
	SPAPI	A_Err (*AEGP_GetNewColorProfileDescription)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstColorProfileP	color_profileP,		// >>
		AEGP_MemHandle			*unicode_descPH);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
		
	SPAPI	A_Err (*AEGP_DisposeColorProfile)(
		AEGP_ColorProfileP	color_profileP);	// >>
		
	SPAPI	A_Err (*AEGP_GetColorProfileApproximateGamma)(
		AEGP_ConstColorProfileP	color_profileP,		// >>
		A_FpShort				*approx_gammaP);	// <<
		
	SPAPI	A_Err (*AEGP_IsRGBColorProfile)(
		AEGP_ConstColorProfileP	color_profileP,		// <<
		A_Boolean				*is_rgbPB);			// >>
	
	SPAPI	A_Err (*AEGP_SetWorkingColorSpace)(
		AEGP_PluginID			aegp_plugin_id,
		AEGP_CompH				compH,				// >>
		AEGP_ConstColorProfileP	color_profileP);	// >>
} AEGP_ColorSettingsSuite3;

#define kAEGPColorSettingsSuiteVersion4	5	// frozen in AE 22.6; get OCIO Color information
typedef struct AEGP_ColorSettingsSuite4 {

	 SPAPI	A_Err (*AEGP_GetBlendingTables)(
		PR_RenderContextH 		render_contextH,
		PF_EffectBlendingTables *blending_tables);
		
	SPAPI	A_Err (*AEGP_DoesViewHaveColorSpaceXform)(
		AEGP_ItemViewP	viewP,			// >>
		A_Boolean		*has_xformPB);	// <<
		
	SPAPI	A_Err (*AEGP_XformWorkingToViewColorSpace)(
		AEGP_ItemViewP	viewP,		// >>
		AEGP_WorldH		srcH,		// in
		AEGP_WorldH		dstH);		// out; must be the same size (can be the same as source)
		
	SPAPI	A_Err (*AEGP_GetNewWorkingSpaceColorProfile)(
		AEGP_PluginID		aegp_plugin_id,					// >>
		AEGP_CompH			compH,							// >>
		AEGP_ColorProfileP	*color_profilePP);				// << caller must dispose with AEGP_DisposeColorProfile
		
	SPAPI	A_Err (*AEGP_GetNewColorProfileFromICCProfile)(
		AEGP_PluginID		aegp_plugin_id,		// >>
		A_long				icc_sizeL,			// >>	icc profile size
		const	void		*icc_dataPV,		// >>	icc profile
		AEGP_ColorProfileP	*color_profilePP);	// <<	builds AEGP_ColorProfile from icc profile; caller must dispose with AEGP_DisposeColorProfile
		
	SPAPI	A_Err (*AEGP_GetNewICCProfileFromColorProfile)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstColorProfileP	color_profileP,		// >>
		AEGP_MemHandle			*icc_profilePH);	// <<	extract icc profile from AEGP_ColorProfile; caller must dispose with AEGP_FreeMemHandle
		
	SPAPI	A_Err (*AEGP_GetNewColorProfileDescription)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstColorProfileP	color_profileP,		// >>
		AEGP_MemHandle			*unicode_descPH);	// <<	handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
		
	SPAPI	A_Err (*AEGP_DisposeColorProfile)(
		AEGP_ColorProfileP	color_profileP);	// >>
		
	SPAPI	A_Err (*AEGP_GetColorProfileApproximateGamma)(
		AEGP_ConstColorProfileP	color_profileP,		// >>
		A_FpShort				*approx_gammaP);	// <<
		
	SPAPI	A_Err (*AEGP_IsRGBColorProfile)(
		AEGP_ConstColorProfileP	color_profileP,		// <<
		A_Boolean				*is_rgbPB);			// >>
	
	SPAPI	A_Err (*AEGP_SetWorkingColorSpace)(
		AEGP_PluginID			aegp_plugin_id,
		AEGP_CompH				compH,				// >>
		AEGP_ConstColorProfileP	color_profileP);	// >>
		
	SPAPI	A_Err (*AEGP_IsOCIOColorManagementUsed)(
		AEGP_PluginID			aegp_plugin_id,					// >>
		A_Boolean				*is_OCIOColorManagementUsedPB);	// <<

	SPAPI	A_Err (*AEGP_GetOCIOConfigurationFile)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_MemHandle			*config_filePH);	// <<
} AEGP_ColorSettingsSuite4;

#define kAEGPMarkerSuiteVersion1		1 /* frozen in AE 8.0 */

typedef struct AEGP_MarkerSuite1 {

	SPAPI A_Err	(*AEGP_NewMarker)(
		AEGP_MarkerValP			*markerPP);

	SPAPI A_Err	(*AEGP_DisposeMarker)(
		AEGP_MarkerValP			markerP);

	SPAPI A_Err	(*AEGP_DuplicateMarker)(
		AEGP_MarkerValP			markerP, 			// >>
		AEGP_MarkerValP			*new_markerP);		// <<

	SPAPI A_Err	(*AEGP_SetMarkerFlag)(
		AEGP_MarkerValP			markerP,			// >>
		AEGP_MarkerFlagType		flagType,			// >>
		A_Boolean				valueB);			// >>

	SPAPI A_Err	(*AEGP_GetMarkerFlag)(
		AEGP_ConstMarkerValP	markerP,			// >>
		AEGP_MarkerFlagType		flagType,			// >>
		A_Boolean				*valueBP);			// <<

	SPAPI A_Err	(*AEGP_GetMarkerString)(
		AEGP_PluginID			aegp_plugin_id,		/* >> */
		AEGP_ConstMarkerValP	markerP,			// >>
		AEGP_MarkerStringType	strType,			// >>
		AEGP_MemHandle			*unicodePH);		/* << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err	(*AEGP_SetMarkerString)(
		AEGP_MarkerValP			markerP,			// <<>>
		AEGP_MarkerStringType	strType,			// >>
		const A_u_short			*unicodeP,			// >>
		A_long					lengthL);			// >> number of characters

	SPAPI A_Err	(*AEGP_CountCuePointParams)(
		AEGP_ConstMarkerValP	markerP,			// >>
		A_long					*paramsLP);			// <<

	SPAPI A_Err	(*AEGP_GetIndCuePointParam)(
		AEGP_PluginID			aegp_plugin_id,		// >>
		AEGP_ConstMarkerValP	markerP,			// >>
		A_long					param_indexL,		// >> must be between 0 and count - 1. else error
		AEGP_MemHandle			*unicodeKeyPH,		// << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle
		AEGP_MemHandle			*unicodeValuePH);	// << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_SetIndCuePointParam)(
		AEGP_MarkerValP			markerP,			// >>
		A_long					param_indexL,		// must be between 0 and count - 1. else error
		const A_u_short			*unicodeKeyP,		// >> UTF16
		A_long					key_lengthL,		// >> number of characters
		const A_u_short			*unicodeValueP,		// >> UTF16
		A_long					value_lengthL);		// >> number of characters

	//	this call is followed by AEGP_SetIndCuePointParam() to actually set the data
	//	the ONLY thing this function does is reserve the space for the param, at the provided index
	SPAPI A_Err	(*AEGP_InsertCuePointParam)(
		AEGP_MarkerValP			markerP,			// >>
		A_long					param_indexL);		// must be between 0 and count. else error

	SPAPI A_Err	(*AEGP_DeleteIndCuePointParam)(
		AEGP_MarkerValP			markerP,			// >>
		A_long					param_indexL);		// must be between 0 and count - 1. else error

} AEGP_MarkerSuite1;

#define kAEGPMarkerSuiteVersion2		2 /* frozen in AE 9.0 */

typedef struct AEGP_MarkerSuite2 {

	SPAPI A_Err	(*AEGP_NewMarker)(
						AEGP_MarkerValP			*markerPP);

	SPAPI A_Err	(*AEGP_DisposeMarker)(
						AEGP_MarkerValP			markerP);

	SPAPI A_Err	(*AEGP_DuplicateMarker)(
						AEGP_MarkerValP			markerP, 			// >>
						AEGP_MarkerValP			*new_markerP);		// <<

	SPAPI A_Err	(*AEGP_SetMarkerFlag)(
						AEGP_MarkerValP			markerP,			// >>
						AEGP_MarkerFlagType		flagType,			// >>
						A_Boolean				valueB);			// >>

	SPAPI A_Err	(*AEGP_GetMarkerFlag)(
						AEGP_ConstMarkerValP	markerP,			// >>
						AEGP_MarkerFlagType		flagType,			// >>
							A_Boolean				*valueBP);			// <<

	SPAPI A_Err	(*AEGP_GetMarkerString)(
						AEGP_PluginID			aegp_plugin_id,		/* >> */
						AEGP_ConstMarkerValP	markerP,			// >>
						AEGP_MarkerStringType	strType,			// >>
						AEGP_MemHandle			*unicodePH);		/* << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle */

	SPAPI A_Err	(*AEGP_SetMarkerString)(
						AEGP_MarkerValP			markerP,			// <<>>
						AEGP_MarkerStringType	strType,			// >>
						const A_u_short			*unicodeP,			// >>
						A_long					lengthL);			// >> number of characters

	SPAPI A_Err	(*AEGP_CountCuePointParams)(
						AEGP_ConstMarkerValP	markerP,			// >>
						A_long					*paramsLP);			// <<

	SPAPI A_Err	(*AEGP_GetIndCuePointParam)(
						AEGP_PluginID			aegp_plugin_id,		// >>
						AEGP_ConstMarkerValP	markerP,			// >>
						A_long					param_indexL,		// >> must be between 0 and count - 1. else error
						AEGP_MemHandle			*unicodeKeyPH,		// << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle
						AEGP_MemHandle			*unicodeValuePH);	// << handle of A_u_short, UTF16, NULL terminated, must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_SetIndCuePointParam)(
						AEGP_MarkerValP			markerP,			// >>
						A_long					param_indexL,		// must be between 0 and count - 1. else error
						const A_u_short			*unicodeKeyP,		// >> UTF16
						A_long					key_lengthL,		// >> number of characters
						const A_u_short			*unicodeValueP,		// >> UTF16
						A_long					value_lengthL);		// >> number of characters

	//	this call is followed by AEGP_SetIndCuePointParam() to actually set the data
	//	the ONLY thing this function does is reserve the space for the param, at the provided index
	SPAPI A_Err	(*AEGP_InsertCuePointParam)(
						AEGP_MarkerValP			markerP,			// >>
						A_long					param_indexL);		// must be between 0 and count. else error

	SPAPI A_Err	(*AEGP_DeleteIndCuePointParam)(
						AEGP_MarkerValP			markerP,			// >>
						A_long					param_indexL);		// must be between 0 and count - 1. else error

	SPAPI A_Err	(*AEGP_SetMarkerDuration)(
						AEGP_MarkerValP			markerP,			// >>
						const A_Time			*durationPT);		// >>


	SPAPI A_Err	(*AEGP_GetMarkerDuration)(
						AEGP_ConstMarkerValP	markerP,			// >>
						A_Time					*durationPT);		// <<


} AEGP_MarkerSuite2;

#define kAEGPProjSuiteVersion5		8	/* frozen in AE 10.0 */

typedef struct AEGP_ProjSuite5 {

	SPAPI A_Err	(*AEGP_GetNumProjects)(									/* will always (in 5.0) return 1 if project is open */
		A_long				*num_projPL);				/* << */

	SPAPI A_Err	(*AEGP_GetProjectByIndex)(
		A_long				proj_indexL,				/* >> */
		AEGP_ProjectH		*projPH);					/* << */

	SPAPI A_Err	(*AEGP_GetProjectName)(
		AEGP_ProjectH		projH,						/* >> */
		A_char 				*nameZ);					/* << space for A_char[AEGP_MAX_PROJ_NAME_SIZE] */

	SPAPI A_Err	(*AEGP_GetProjectPath)(
		AEGP_ProjectH		projH,						/* >> */
		AEGP_MemHandle		*unicode_pathPH);			// << empty string if no file. handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

	SPAPI A_Err	(*AEGP_GetProjectRootFolder)(
		AEGP_ProjectH		projH,						/* >> */
		AEGP_ItemH			*root_folderPH);			/* << */

	SPAPI A_Err (*AEGP_SaveProjectToPath)(
		AEGP_ProjectH		projH,						/* >> */
		const A_UTF16Char	*pathZ);					// >> null terminated unicode path with platform separators

	SPAPI A_Err (*AEGP_GetProjectTimeDisplay)(
		AEGP_ProjectH		projH,						/* >> */
		AEGP_TimeDisplay2	*time_displayP);			/* << */

	SPAPI A_Err (*AEGP_SetProjectTimeDisplay)(							/* UNDOABLE */
		AEGP_ProjectH			projH,					/* <> */
		const AEGP_TimeDisplay2	*time_displayP);		/* >> */

	SPAPI A_Err (*AEGP_ProjectIsDirty)(
		AEGP_ProjectH		projH,						/* >> */
		A_Boolean			*is_dirtyPB);				/* << */

	SPAPI A_Err (*AEGP_SaveProjectAs)(
		AEGP_ProjectH		projH,						/* >> */
		const A_UTF16Char	*pathZ);					// >> null terminated unicode path with platform separators

	SPAPI A_Err (*AEGP_NewProject)(
		AEGP_ProjectH	*new_projectPH);				/* << WARNING: Will close any open projects! */

	// WARNING: Will close any open projects!
	SPAPI A_Err (*AEGP_OpenProjectFromPath)(
		const A_UTF16Char	*pathZ,						// >> null terminated unicode path with platform separators
		AEGP_ProjectH		*projectPH);				/* << */

	SPAPI A_Err	(*AEGP_GetProjectBitDepth)(
		AEGP_ProjectH		projectH,					/* >> */
		AEGP_ProjBitDepth	*bit_depthP);				/* << */

	SPAPI A_Err	(*AEGP_SetProjectBitDepth)(								/* UNDOABLE */
		AEGP_ProjectH		projectH,					/* >> */
		AEGP_ProjBitDepth	bit_depth);					/* >> */

} AEGP_ProjSuite5;

#define	kAEGPPersistentDataSuiteVersion3	3 /* frozen in AE 10.0 */

typedef struct {
	// get a handle of the application blob,
	// modifying this will modify the application
	SPAPI A_Err (*AEGP_GetApplicationBlob)(
		AEGP_PersistentBlobH	*blobPH);			/* >> */

	// section and value key management
	SPAPI A_Err (*AEGP_GetNumSections)(
		AEGP_PersistentBlobH	blobH,				/* >> */
		A_long					*num_sectionPL);	/* << */

	SPAPI A_Err (*AEGP_GetSectionKeyByIndex)(
		AEGP_PersistentBlobH	blobH,				/* >> */
		A_long					section_index,		/* >> */
		A_long					max_section_size,	/* >> */
		A_char					*section_keyZ);		/* << */

	SPAPI A_Err	(*AEGP_DoesKeyExist)(
		AEGP_PersistentBlobH	blobH,				/* >> */
		const A_char			*section_keyZ,		/* >> */
		const A_char			*value_keyZ,		/* >> */
		A_Boolean				*existsPB);			/* << */

	SPAPI A_Err (*AEGP_GetNumKeys)(
		AEGP_PersistentBlobH	blobH,				/* >> */
		const A_char			*section_keyZ,		/* >> */
		A_long					*num_keysPL);		/* << */

	SPAPI A_Err (*AEGP_GetValueKeyByIndex)(
		AEGP_PersistentBlobH	blobH,				/* >> */
		const A_char			*section_keyZ,		/* >> */
		A_long					key_index,			/* >> */
		A_long					max_key_size,		/* >> */
		A_char					*value_keyZ);		/* << */

	// data access and manipulation

	// For the entry points below, if a given key is not found,
	//	the default value is both written to the blobH and
	//	returned as the value; if no default is provided, a blank value will be written
	//	and returned

	SPAPI A_Err (*AEGP_GetDataHandle)(
		AEGP_PluginID			plugin_id,
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		AEGP_MemHandle			defaultH0,		/* >> never adopted, NULL means no default data */
		AEGP_MemHandle			*valuePH);		/* << newly allocated, owned by caller, NULL if would be zero sized handle */

	SPAPI A_Err (*AEGP_GetData)(
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		A_u_long				data_sizeLu,	/* >> bufPV & default must be this big, if pref isn't then the default will be used */
		const void				*defaultPV0,	/* >> NULL means all zeros for default */
		void					*bufPV);		/* << */

	SPAPI A_Err (*AEGP_GetString)(
		AEGP_PersistentBlobH	blobH,					/* >> */
		const A_char			*section_keyZ,			/* >> */
		const A_char			*value_keyZ,			/* >> */
		const A_char			*defaultZ0,				/* >> NULL means '\0' is the default */
		A_u_long				buf_sizeLu,				/* >> size of buffer. Behavior dependent on actual_buf_sizeLu0 */
		A_char					*bufZ,					/* << will be "" if buf_size is too small */
		A_u_long				*actual_buf_sizeLu0);	/* << actual size needed to store the buffer (includes terminating NULL). Pass NULL for error reporting if size mismatch.*/

	SPAPI A_Err (*AEGP_GetLong)(
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		A_long					defaultL,		/* >> */
		A_long					*valuePL);		/* << */

	SPAPI A_Err	(*AEGP_GetFpLong)(
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		A_FpLong				defaultF,		/* >> */
		A_FpLong				*valuePF);		/* << */

	// setters
	SPAPI A_Err	(*AEGP_SetDataHandle)(
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		const AEGP_MemHandle	valueH);		/* >> not adopted */

	SPAPI A_Err	(*AEGP_SetData)(
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		A_u_long				data_sizeLu,	/* >> */
		const void				*dataPV);		/* >> */

	SPAPI A_Err	(*AEGP_SetString)(
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		const A_char			*strZ);			/* >> */

	SPAPI A_Err	(*AEGP_SetLong)(
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		A_long					valueL);		/* >> */


	SPAPI A_Err	(*AEGP_SetFpLong)(
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ,	/* >> */
		A_FpLong				valueF);		/* >> */

	SPAPI A_Err	(*AEGP_DeleteEntry)(							/* no error if entry not found */
		AEGP_PersistentBlobH	blobH,			/* >> */
		const A_char			*section_keyZ,	/* >> */
		const A_char			*value_keyZ);	/* >> */

	SPAPI A_Err	(*AEGP_GetPrefsDirectory)(
		AEGP_MemHandle			*unicode_pathPH);		// << empty string if no file. handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle

} AEGP_PersistentDataSuite3;

#define kAEGPIterateSuite				"AEGP Iterate Suite"
#define kAEGPIterateSuiteVersion1		1 /* frozen in AE 5.0 */

typedef struct AEGP_IterateSuite1 {

	SPAPI A_Err(*AEGP_GetNumThreads)(
		A_long* num_threadsPL);


	SPAPI A_Err(*AEGP_IterateGeneric)(
		A_long			iterationsL,						/* >> */		// can be PF_Iterations_ONCE_PER_PROCESSOR
		void* refconPV,							/* >> */
		A_Err(*fn_func)(void* refconPV,		/* >> */
			A_long 	thread_indexL,	/* >> */
			A_long 	i,				/* >> */
			A_long 	iterationsL));	/* >> */

} AEGP_IterateSuite1;

#define kAEGPIOInSuite			"AEGP IO In Suite"
#define kAEGPIOInSuiteVersion4	5 /* frozen in AE 10 */

typedef struct AEGP_IOInSuite4 {
	
	SPAPI A_Err	(*AEGP_GetInSpecOptionsHandle)(
											   AEIO_InSpecH	inH,					/* >> */
											   void			**optionsPPV);			/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecOptionsHandle)(
											   AEIO_InSpecH	inH,					/* >> */
											   void			*optionsPV,				/* >> */
											   void			**old_optionsPPV);		/* << */
	
	SPAPI A_Err	(*AEGP_GetInSpecFilePath)(
										  AEIO_InSpecH	inH,					/* >> */
										  AEGP_MemHandle	*unicode_pathPH);		// << handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle
	
	SPAPI A_Err	(*AEGP_GetInSpecNativeFPS)(
										   AEIO_InSpecH	inH,					/* >> */
										   A_Fixed			*native_fpsP);			/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecNativeFPS)(
										   AEIO_InSpecH	inH,					/* >> */
										   A_Fixed			native_fps);			/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecDepth)(
									   AEIO_InSpecH	inH,					/* >> */
									   A_short			*depthPS);				/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecDepth)(
									   AEIO_InSpecH	inH,					/* >> */
									   A_short			depthS);				/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecSize)(
									  AEIO_InSpecH	inH,					/* >> */
									  AEIO_FileSize	*sizePL);				/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecSize)(
									  AEIO_InSpecH	inH,					/* >> */
									  AEIO_FileSize	sizeL);					/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecInterlaceLabel)(
												AEIO_InSpecH		inH,				/* >> */
												FIEL_Label	 		*interlaceP);		/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecInterlaceLabel)(
												AEIO_InSpecH		inH,				/* >> */
												const FIEL_Label	*interlaceP);		/* << */
	
	SPAPI A_Err	(*AEGP_GetInSpecAlphaLabel)(
											AEIO_InSpecH			inH,			/* >> */
											AEIO_AlphaLabel			*alphaP);		/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecAlphaLabel)(
											AEIO_InSpecH			inH,			/* >> */
											const AEIO_AlphaLabel	*alphaP);		/* << */
	
	SPAPI A_Err	(*AEGP_GetInSpecDuration)(
										  AEIO_InSpecH	inH,					/* >> */
										  A_Time			*durationP);			/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecDuration)(
										  AEIO_InSpecH	inH,					/* >> */
										  const A_Time	*durationP);			/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecDimensions)(
											AEIO_InSpecH	inH,					/* >> */
											A_long			*widthPL0,				/* << */
											A_long			*heightPL0);
	
	SPAPI A_Err	(*AEGP_SetInSpecDimensions)(
											AEIO_InSpecH	inH,					/* >> */
											A_long			widthL,					/* >> */
											A_long			heightL);				/* >> */
	
	SPAPI A_Err	(*AEGP_InSpecGetRationalDimensions)(
													AEIO_InSpecH				inH,		/* >> */
													const AEIO_RationalScale	*rs0,		/* << */
													A_long						*width0,	/* << */
													A_long						*height0,	/* << */
													A_Rect						*r0);		/* << */
	
	SPAPI A_Err	(*AEGP_GetInSpecHSF)(
									 AEIO_InSpecH	inH,					/* >> */
									 A_Ratio			*hsfP);					/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecHSF)(
									 AEIO_InSpecH	inH,					/* >> */
									 const A_Ratio	*hsfP);					/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecSoundRate)(
										   AEIO_InSpecH	inH,					/* >> */
										   A_FpLong		*ratePF);				/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecSoundRate)(
										   AEIO_InSpecH	inH,					/* >> */
										   A_FpLong		rateF);					/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecSoundEncoding)(
											   AEIO_InSpecH		inH,				/* >> */
											   AEIO_SndEncoding	*encodingP);		/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecSoundEncoding)(
											   AEIO_InSpecH		inH,				/* >> */
											   AEIO_SndEncoding	encoding);			/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecSoundSampleSize)(
												 AEIO_InSpecH		inH,				/* >> */
												 AEIO_SndSampleSize	*bytes_per_sampleP);/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecSoundSampleSize)(
												 AEIO_InSpecH		inH,				/* >> */
												 AEIO_SndSampleSize	bytes_per_sample);	/* >> */
	
	SPAPI A_Err	(*AEGP_GetInSpecSoundChannels)(
											   AEIO_InSpecH		inH,				/* >> */
											   AEIO_SndChannels	*num_channelsP);	/* << */
	
	SPAPI A_Err	(*AEGP_SetInSpecSoundChannels)(
											   AEIO_InSpecH		inH,				/* >> */
											   AEIO_SndChannels	num_channels);		/* >> */
	
	SPAPI A_Err	(*AEGP_AddAuxExtMap)(
									 const A_char		*extension,			/* >> */
									 A_long				file_type,			/* >> */
									 A_long				creator);			/* >> */
	
	// In case of RGB data, if there is an embedded icc profile, build AEGP_ColorProfile out of this icc profile using AEGP_GetNewColorProfileFromICCProfile and pass it to
	// AEGP_SetInSpecEmbeddedColorProfile, with profile description set to NULL.
	//
	// In case of non-RGB data, if there is an embedded non-RGB icc profile or you know the color space the data is in, pass its description as a null-terminated unicode string
	// to AEGP_SetInSpecEmbeddedColorProfile, with color profile set to NULL. Doing this disables color management UI that allows user to affect
	// profile choice in the application UI.
	//
	// If you are unpacking non-RGB data directly into working space (to get working space use AEGP_GetNewWorkingSpaceColorProfile), you are done.
	//
	// If you are unpacking non-RGB data into specific RGB color space, you must pass the profile describing this space to AEGP_SetInSpecAssignedColorProfile.
	// Otherwise, your RGB data will be incorrectly interpreted as being in working space.
	//
	// Either color profile or profile description should be NULL in AEGP_SetInSpecEmbeddedColorProfile. You cannot use both.
	SPAPI A_Err (*AEGP_SetInSpecEmbeddedColorProfile)(
													  AEIO_InSpecH			inH,				// <<
													  AEGP_ConstColorProfileP	color_profileP0,	// <<
													  const	A_UTF16Char		*profile_descP0);	// <<	pointer to a null-terminated unicode string
	
	// Assign valid RGB profile to the footage
	SPAPI A_Err (*AEGP_SetInSpecAssignedColorProfile)(
													  AEIO_InSpecH			inH,				// <<
													  AEGP_ConstColorProfileP	color_profileP);	// <<
	
} AEGP_IOInSuite4;

#define kAEGPIOOutSuiteVersion4	7 /* frozen in AE 10.0 */

typedef struct AEGP_IOOutSuite4 {
	SPAPI A_Err	(*AEGP_GetOutSpecOptionsHandle)(
						AEIO_OutSpecH	outH,					/* >> */
						void			**optionsPPV);			/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecOptionsHandle)(
						AEIO_OutSpecH	outH,					/* >> */
						void			*optionsPV,				/* >> */
						void			**old_optionsPPVO);		/* <> */

	SPAPI A_Err	(*AEGP_GetOutSpecFilePath)(
						AEIO_OutSpecH	outH,					/* >> */
						AEGP_MemHandle	*unicode_pathPH,		// << handle of A_UTF16Char (contains null terminated UTF16 string); must be disposed with AEGP_FreeMemHandle 
						A_Boolean		*file_reservedPB);		/* << If the file is reserved, do not create the file.
																	  Otherwise, multi-machine rendering can fail. 
																	  If true, an empty file has already been created. */

	SPAPI A_Err	(*AEGP_GetOutSpecFPS)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Fixed			*native_fpsP);			/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecNativeFPS)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Fixed			native_fpsP);			/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecDepth)(
						AEIO_OutSpecH	outH,					/* >> */
						A_short			*depthPS);				/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecDepth)(
						AEIO_OutSpecH	outH,					/* >> */
						A_short			depthPS);				/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecInterlaceLabel)(
						AEIO_OutSpecH		outH,				/* >> */
						FIEL_Label			*interlaceP);		/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecInterlaceLabel)(
						AEIO_OutSpecH		outH,				/* >> */
						const FIEL_Label	*interlaceP);		/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecAlphaLabel)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_AlphaLabel		*alphaP);			/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecAlphaLabel)(
						AEIO_OutSpecH			outH,			/* >> */
						const AEIO_AlphaLabel	*alphaP);		/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecDuration)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Time			*durationP);			/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecDuration)(
						AEIO_OutSpecH	outH,					/* >> */
						const A_Time	*durationP);			/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecDimensions)(
						AEIO_OutSpecH	outH,					/* >> */
						A_long			*widthPL,				/* << */
						A_long			*heightPL);				/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecHSF)(
						AEIO_OutSpecH	outH,					/* >> */
						A_Ratio			*hsfP);					/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecHSF)(
						AEIO_OutSpecH	outH,					/* >> */
						const A_Ratio	*hsfP);					/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecSoundRate)(
						AEIO_OutSpecH	outH,					/* >> */
						A_FpLong		*ratePF);				/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecSoundRate)(
						AEIO_OutSpecH	outH,					/* >> */
						A_FpLong		rateF);					/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecSoundEncoding)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndEncoding	*encodingP);		/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecSoundEncoding)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndEncoding	encoding);			/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecSoundSampleSize)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndSampleSize	*bytes_per_sampleP);/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecSoundSampleSize)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndSampleSize	bytes_per_sample);	/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecSoundChannels)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndChannels	*num_channelsP);	/* << */

	SPAPI A_Err	(*AEGP_SetOutSpecSoundChannels)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_SndChannels	num_channels);		/* >> */

	SPAPI A_Err	(*AEGP_GetOutSpecIsStill)(
						AEIO_OutSpecH		outH,				/* >> */
						A_Boolean			*is_stillPB);		/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecPosterTime)(
						AEIO_OutSpecH		outH,				/* >> */
						A_Time				*poster_timeP);		/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecStartFrame)(
						AEIO_OutSpecH		outH,				/* >> */
						A_long				*start_frameP);		/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecPullDown)(
						AEIO_OutSpecH		outH,				/* >> */
						AEIO_Pulldown		*pulldownP);		/* << */

	SPAPI A_Err	(*AEGP_GetOutSpecIsMissing)(
						AEIO_OutSpecH		outH,				/* >> */
						A_Boolean			*missingPB);		/* << */
	
	// see if you need to embed outspec's color profile as an icc profile
	SPAPI A_Err (*AEGP_GetOutSpecShouldEmbedICCProfile)(
						AEIO_OutSpecH		outH,				// >>
						A_Boolean			*embedPB);			// <<
	
	// query outspec's color profile					
	SPAPI A_Err (*AEGP_GetNewOutSpecColorProfile)(
						AEGP_PluginID		aegp_plugin_id,		// >>
						AEIO_OutSpecH		outH,				// >>
						AEGP_ColorProfileP	*color_profilePP);	// <<	output color space; caller must dispose with AEGP_DisposeColorProfile

	// Fails if rq_itemP is not found.
	// This API would also fail if the outH is not a confirmed outH and is a copy.
	// e.g. if the Output Module settings dialog is Open.
	SPAPI A_Err	(*AEGP_GetOutSpecOutputModule)(
						AEIO_OutSpecH		outH,				/* >> */
						AEGP_RQItemRefH		*rq_itemP,			/* << */
						AEGP_OutputModuleRefH *om_refP);		/* << */

	
} AEGP_IOOutSuite4;

#define	kAEGPFIMSuiteVersion3	3 /* frozen in AE 10.0 */
typedef struct {
	SPAPI A_Err (*AEGP_RegisterImportFlavor)(
				const A_char 				*nameZ,		// format name you'd like to appear
													// in AE's Import Dialog Format pop-up
													// menu.
													// Limited to AE_FIM_MAX_IMPORT_FLAVOR_NAME_LEN.
													// Everything after that will be truncated.
				AE_FIM_ImportFlavorRef	*imp_refP);	// On return it is set to a valid opaque ref.
													// If error occured, it will be returned to
													// the caller and ref will be set to a special
													// value - AE_FIM_ImportFlavorRef_NONE.
													
	SPAPI A_Err (*AEGP_RegisterImportFlavorFileTypes)(
				AE_FIM_ImportFlavorRef	imp_ref,		// Received from AEGP_RegisterImportFlavor
				A_long					num_filekindsL,	// number of supported file types for this format
				const	AEIO_FileKind	*kindsAP,		// Array of supported file types for this format
				A_long					num_fileextsL,	// number of supported file exts for this format
				const	AEIO_FileKind	*extsAP);		// Array of supported file exts for this format
				
				
	SPAPI A_Err (*AEGP_RegisterImportFlavorImportCallbacks)(
				AE_FIM_ImportFlavorRef	imp_ref,			// Received from AEGP_RegisterImportFlavor
				AE_FIM_ImportFlags		single_flag,		// You can register callbacks only per single flag
															// this also registers the flag with the import flavor		
				const	AE_FIM_ImportCallbacks	*imp_cbsP);	// Callbacks your format installs per each flag
				
	// optionally call once from AE_FIM_ImportFileCB.  This is used by the application when re-importing
	// from the render queue and replacing an existing item.
	SPAPI A_Err (*AEGP_SetImportedItem)(
			AE_FIM_ImportOptions		imp_options,		/* <> */
			AEGP_ItemH					imported_itemH);	/* >> */
				
} AEGP_FIMSuite3;


#include <AE_GeneralPlugPost.h>
