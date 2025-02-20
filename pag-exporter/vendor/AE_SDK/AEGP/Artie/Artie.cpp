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

#include "Artie.h"

static AEGP_PluginID		S_aegp_plugin_id;

static SPBasicSuite			*S_sp_basic_suiteP;

static A_Err
Artie_DisposeLights(
	const PR_InData				*in_dataP,	
	AEGP_MemHandle				lightsH)
{
	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	
	return suites.MemorySuite1()->AEGP_FreeMemHandle(lightsH);
}

static A_Err
Artie_GetPlaneEquation(
	Artie_Poly		*polyP,
	A_FpLong		*aFP,
	A_FpLong		*bFP,
	A_FpLong		*cFP,
	A_FpLong		*dFP)
{
	A_Err			err = A_Err_NONE;
	A_FpLong		sum_x = 0, 
					sum_y = 0, 
					sum_z = 0;

	for (A_long iL = 0; iL < 4; iL++) {
		sum_x += polyP->vertsA[iL].coord.P[0];
		sum_y += polyP->vertsA[iL].coord.P[1];
		sum_z += polyP->vertsA[iL].coord.P[2];
	}
	sum_x *= 0.25;
	sum_y *= 0.25;
	sum_z *= 0.25;

	*aFP = polyP->normal.V[0];
	*bFP = polyP->normal.V[1];
	*cFP = polyP->normal.V[2];

	*dFP = -(sum_x * (*aFP) + sum_y * (*bFP) + sum_z * (*cFP));
	
	return err;
}

static A_Err
Artie_RayIntersectPlane(
	Artie_Poly		*polyP,
	Ray				*rayP,
	A_FpLong		*tPF)
{
	A_Err			err 	= A_Err_NONE;
	A_FpLong		topF 	= 0.0, 
					bottomF = 0.0,
					aFP 	= 0.0, 
					bFP 	= 0.0, 
					cFP		= 0.0,
					dFP 	= 0.0;
					
	VectorType4D	dir;

	ERR(Artie_GetPlaneEquation(polyP, &aFP, &bFP, &cFP, &dFP));
	if (!err) {
		dir		= Pdiff(rayP->P1, rayP->P0);
		bottomF = aFP * dir.V[0] +bFP * dir.V[1] + cFP * dir.V[2];

		if (bottomF){
			topF = -(dFP + aFP*rayP->P0.P[0] + bFP*rayP->P0.P[1] + cFP*rayP->P0.P[2]);
			*tPF = topF / bottomF;
		} else {
			*tPF = Artie_MISS;
		}
	}
	return err;
}

static A_Err
Artie_RayIntersectLayer(
	Artie_Poly  	*polyP,
	Ray				*rayP,
	A_FpLong		*tPF,
	A_FpLong		*uPF,
	A_FpLong		*vPF)
{
	A_Err		err 		= A_Err_NONE;
	A_FpLong	u0F 		= 0.0, 
				v0F			= 0.0,
				u1F			= 0.0,
				v1F			= 0.0, 
				u2F			= 0.0, 
				v2F			= 0.0,
				alphaF		= 0.0,
				betaF		= 0.0,
				gammaF		= 0.0;	
	A_Boolean	insideB 	= FALSE;
	A_long		i1L			= 0,
			 	i2			= 0,
			 	kL			= 0;
	PointType4D		P;
	VectorType4D	dir;

	*uPF = *vPF = -1;

	ERR(Artie_RayIntersectPlane(polyP, rayP, tPF));

	if ( *tPF != Artie_MISS) {
		// find uv-coords
		dir = Pdiff(rayP->P1, rayP->P0);
		P.P[0] = rayP->P0.P[0] + dir.V[0] * *tPF;
		P.P[1] = rayP->P0.P[1] + dir.V[1] * *tPF;
		P.P[2] = rayP->P0.P[2] + dir.V[2] * *tPF;

		if (0 == polyP->dominant) {
			i1L = 1;
			i2 = 2;
		} else if ( 1 == polyP->dominant) {
			i1L = 0;
			i2 = 2;
		} else {
			i1L = 0;
			i2 = 1;
		}

		u0F = P.P[i1L] - polyP->vertsA[0].coord.P[i1L];
		v0F = P.P[i2] - polyP->vertsA[0].coord.P[i2];
		
		kL = 2;
		do{
			u1F = polyP->vertsA[kL-1].coord.P[i1L] - polyP->vertsA[0].coord.P[i1L];
			v1F = polyP->vertsA[kL-1].coord.P[i2] - polyP->vertsA[0].coord.P[i2];
			u2F = polyP->vertsA[kL  ].coord.P[i1L] - polyP->vertsA[0].coord.P[i1L];
			v2F = polyP->vertsA[kL  ].coord.P[i2] - polyP->vertsA[0].coord.P[i2];
			
			if (u1F == 0){
				betaF = u0F/u2F;
				if (betaF >= 0.0 && betaF <= 1.0){
					alphaF 	= (v0F - betaF*v2F)/v1F;
					insideB = ((alphaF>=0.) && (alphaF + betaF) <= 1);
				}
			} else {
				betaF = (v0F * u1F - u0F * v1F)/(v2F * u1F - u2F * v1F);
				if (betaF >= 0. && betaF <= 1.) {
					alphaF 	= (u0F - betaF*u2F) / u1F;
					insideB = ((alphaF >= 0.) && (alphaF + betaF) <= 1);
				}
			}
		} while (!insideB && (++kL < 4));

		if (insideB){
			gammaF = 1.0 - alphaF - betaF;
			*uPF = gammaF * polyP->vertsA[0].txtur[0] + alphaF * polyP->vertsA[kL-1].txtur[0] + betaF * polyP->vertsA[kL].txtur[0];
			*vPF = gammaF * polyP->vertsA[0].txtur[1] + alphaF * polyP->vertsA[kL-1].txtur[1] + betaF * polyP->vertsA[kL].txtur[1];
		} else {
			*tPF = Artie_MISS;
		}
	}
	return err;
}

static A_Err
Artie_FillIntersection(
	Ray				*rayP,
	Artie_Poly		*polyP,
	A_FpLong		t,
	A_FpLong		u,
	A_FpLong		v,
	Intersection	*intersectionP)
{
	A_Err err = A_Err_NONE;
	
	AEGP_SuiteHandler suites(S_sp_basic_suiteP);	
	AEGP_WorldSuite2 *wsP = suites.WorldSuite2();
	
	A_long	widthL	=	0,
			heightL	=	0;
			
	A_u_long	rowbytesLu = 0;
	PF_Pixel8*	baseP = NULL;
	
	ERR(wsP->AEGP_GetSize(polyP->texture, &widthL, &heightL));
	ERR(wsP->AEGP_GetBaseAddr8(polyP->texture, &baseP));
	ERR(wsP->AEGP_GetRowBytes(polyP->texture, &rowbytesLu));

	if ( t != Artie_MISS) {
		intersectionP->hitB 						= TRUE;
		intersectionP->intersect_point_coord_on_ray = t;

		// build reflected ray
		VectorType4D	dir, reflected_dir;
		A_FpLong		dotF;

		dir								= Pdiff(rayP->P1, rayP->P0);
		dotF							= Dot4D(dir, polyP->normal);
		reflected_dir					= Vadd(polyP->normal, Vscale(-2.0, dir));
		intersectionP->ray.P0			= PVadd(rayP->P0, Vscale(t, dir));
		intersectionP->ray.P0			= PVadd(intersectionP->ray.P0, Vscale(0.0001, reflected_dir));
		intersectionP->ray.P1			= PVadd(intersectionP->ray.P0, reflected_dir);
		intersectionP->ray.depth		= rayP->depth + 1;
		intersectionP->ray.contribution	= rayP->contribution * polyP->material.ksF;
		
		intersectionP->reflectance_percentF = polyP->material.ksF;
		
		if ( u >= 0 && v >= 0 && u <= 1 && v <= 1) {
			A_long iL = static_cast<A_long>(u * (widthL-1));
			A_long j = static_cast<A_long>(v * (heightL-1));
			PF_Pixel8 *pP = baseP + iL;
			intersectionP->color_struck_surface = *(PF_Pixel *)((char *)pP + j * rowbytesLu);
		}
	}
	return err;
}

static 
Intersection
SceneIntersection(
	Artie_Scene		*sceneP, 
	Ray				*rayP)
{
	A_Err			err			= A_Err_NONE;
	Intersection	returned_I, 
					I[Artie_MAX_POLYGONS];
	A_FpLong		u = 0.0, 
					v = 0.0;

	returned_I.hitB = FALSE;
	returned_I.intersect_point_coord_on_ray = Artie_MISS;
	returned_I.reflectance_percentF = 0.0;

	for(A_long iL = 0; iL < sceneP->num_polysL; ++iL){
		I[iL].hitB = 0;
		err = Artie_RayIntersectLayer(&sceneP->polygons[iL], rayP, &I[iL].intersect_point_coord_on_ray, &u, &v);
		if (!err && I[iL].intersect_point_coord_on_ray != Artie_MISS){
			I[iL].hitB = 1;
		}
	}
	
	if (!err) {
		for(A_long iL = 0; iL < sceneP->num_polysL; iL++) {
			if (I[iL].hitB) {
				if (I[iL].intersect_point_coord_on_ray < returned_I.intersect_point_coord_on_ray && I[iL].intersect_point_coord_on_ray > 0){
					err = Artie_FillIntersection(	rayP, 
													&sceneP->polygons[iL], 
													I[iL].intersect_point_coord_on_ray, 
													u, 
													v, 
													&returned_I);
				}
			}
		}
	}
	return returned_I;
}

static A_Err 
Artie_DoInstanceDialog(	
	const PR_InData		*in_dataP,				/* >> */
	PR_GlobalContextH	global_contextH,		/* >> */
	PR_InstanceContextH	instance_contextH,		/* >> */
	PR_GlobalDataH		global_dataH,			/* <> */
	PR_InstanceDataH	instance_dataH,			/* <> */
	PR_DialogResult		*resultP)				/* << */
{
	A_Err err = A_Err_NONE;
	in_dataP->msg_func(0, "Add any options for your Artisan here.");
	*resultP = PR_DialogResult_NO_CHANGE;
	return err;
}

/*	allocate flat handle and write flatten instance data into it */
 
static A_Err 
Artie_FlattenInstance(
	const PR_InData			*in_dataP,				/* >> */
	PR_GlobalContextH		global_contextH,		/* >> */
	PR_InstanceContextH		instance_contextH,		/* >> */
	PR_GlobalDataH			global_dataH,			/* <> */
	PR_InstanceDataH		instance_dataH,			/* <> */
	PR_FlatHandle			*flatPH)				/* << */
{
	return A_Err_NONE;
}

/*	dispose of render data */
 
static A_Err 
Artie_FrameSetdown(
	const PR_InData			*in_dataP,				/* >> */
	PR_GlobalContextH		global_contextH,		/* >> */
	PR_InstanceContextH		instance_contextH,		/* >> */
	PR_RenderContextH		render_contextH,		/* >> */
	PR_GlobalDataH			global_dataH,			/* <> */
	PR_InstanceDataH		instance_dataH,			/* <> */
	PR_RenderDataH			render_dataH)			/* << */

{
	return A_Err_NONE;
}

/*	Called immediately before rendering; allocate any render data here */

static A_Err 
Artie_FrameSetup(
	const PR_InData			*in_dataP,				/* >> */
	PR_GlobalContextH		global_contextH,		/* >> */
	PR_InstanceContextH		instance_contextH,		/* >> */
	PR_RenderContextH		render_contextH,		/* >> */
	PR_GlobalDataH			global_dataH,			/* <> */
	PR_InstanceDataH		instance_dataH,			/* <> */
	PR_RenderDataH			*render_dataPH)			/* << */
{
	return A_Err_NONE;
}

static A_Err 
Artie_GlobalDoAbout(
	const PR_InData			*in_dataP,			/* >> */
	PR_GlobalContextH		global_contextH,	/* >> */
	PR_GlobalDataH			global_dataH)		/* <> */
{
	A_Err err = A_Err_NONE;
	in_dataP->msg_func(err, "Artie the Artisan.");
	return err;
}

static A_Err 
Artie_GlobalSetdown(
	const PR_InData			*in_dataP,			/* >> */
	PR_GlobalContextH		global_contextH,	/* >> */
	PR_GlobalDataH			global_dataH)		/* <> */ // must be disposed by plugin

{
	return A_Err_NONE;
}

/*	allocate global data here.  */

static A_Err 
Artie_GlobalSetup(
	const PR_InData				*in_dataP,				/* >> */
	PR_GlobalContextH			global_contextH,		/* >> */
	PR_GlobalDataH				*global_dataPH)			/* << */

{
	return A_Err_NONE;
}


/*	create instance data, using flat_data if supplied */

static A_Err 
Artie_SetupInstance(
	const PR_InData			*in_dataP,					/* >> */
	PR_GlobalContextH		global_contextH,			/* >> */
	PR_InstanceContextH		instance_contextH,			/* >> */
	PR_GlobalDataH			global_dataH,				/* >> */
	PR_InstanceFlags		flags,
	PR_FlatHandle			flat_dataH0,				/* >> */
	PR_InstanceDataH		*instance_dataPH)			/* << */
{
	return A_Err_NONE;
}

/* deallocate your instance data */
 
static A_Err 
Artie_SetdownInstance(
	const PR_InData				*in_dataP,				/* >> */
	const PR_GlobalContextH		global_contextH,		/* >> */
	const PR_InstanceContextH	instance_contextH,		/* >> */
	PR_GlobalDataH				global_dataH,			/* >> */
	PR_InstanceDataH			instance_dataH)			/* >> */ // must be disposed by plugin

{
	return A_Err_NONE;
}

static A_Err 
Artie_Query(
	const PR_InData			*in_dataP,			/* >> */
	PR_GlobalContextH		global_contextH,	/* >> */
	PR_InstanceContextH		instance_contextH,  /* >> */
	PR_QueryContextH		query_contextH,		/* <> */
	PR_QueryType			query_type,			/* >> */
	PR_GlobalDataH			global_data,		/* >> */
	PR_InstanceDataH		instance_dataH)	/* >> */
{
	return A_Err_NONE;
}

static A_Err 
Artie_DeathHook(
	AEGP_GlobalRefcon	plugin_refconP,			
	AEGP_DeathRefcon	refconP)
{
	return A_Err_NONE;
}

static A_Err
Artie_BuildLight(
	const PR_InData			*in_dataP,			/* >> */
	PR_RenderContextH		render_contextH,	/* >> */
	AEGP_LayerH				layerH,				/* >> */
	Artie_Light				*lightP)			/* <> */
{
	A_Err				err	= A_Err_NONE;
	A_Time				comp_time, comp_time_step;
	AEGP_StreamVal		stream_val;
	Artie_Light			light;

	AEGP_SuiteHandler	suites(in_dataP->pica_basicP);

	ERR(suites.CanvasSuite5()->AEGP_GetCompRenderTime(render_contextH, &comp_time, &comp_time_step));
	ERR(suites.LightSuite2()->AEGP_GetLightType(layerH, &lightP->type));
	ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, 
														AEGP_LayerStream_COLOR,
														AEGP_LTimeMode_CompTime,
														&comp_time,
														FALSE,
														&stream_val,
														NULL));

	light.color.alpha 	= (A_u_char)(PF_MAX_CHAN8 * stream_val.color.alphaF 	+ 0.5);
	light.color.red 	= (A_u_char)(PF_MAX_CHAN8 * stream_val.color.redF 		+ 0.5);
	light.color.green 	= (A_u_char)(PF_MAX_CHAN8 * stream_val.color.greenF 	+ 0.5);
	light.color.blue 	= (A_u_char)(PF_MAX_CHAN8 * stream_val.color.blueF 		+ 0.5);

	ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue(	layerH, 
															AEGP_LayerStream_INTENSITY,
															AEGP_LTimeMode_CompTime,
															&comp_time,
															FALSE,
															&stream_val,
															NULL));
	if (!err){
		light.intensityF = stream_val.one_d;
		err = suites.StreamSuite2()->AEGP_GetLayerStreamValue( 	layerH, 
																AEGP_LayerStream_CASTS_SHADOWS,
																AEGP_LTimeMode_CompTime,
																&comp_time,
																FALSE,
																&stream_val,
																NULL);
		light.casts_shadowsB = (stream_val.one_d != 0.0);
	}
	
	// Normalize intensity

	if (!err){
		lightP->intensityF *= 0.01;

		light.direction.x 	= 0;
		light.direction.y 	= 0;
		light.direction.z 	= 1;
		light.position.x	= 0;
		light.position.y 	= 0;
		light.position.z 	= 0;

		// GetLightAttenuation was also unimplemented and only returned this constant.
		light.attenuation.x = 1;
		light.attenuation.y = 0;
		light.attenuation.z = 0;
	
		err = suites.StreamSuite2()->AEGP_GetLayerStreamValue( layerH, 
																AEGP_LayerStream_CONE_ANGLE,
																AEGP_LTimeMode_CompTime,
																&comp_time,
																FALSE,
																&stream_val,
																NULL);
		light.cone_angleF = stream_val.one_d;

		ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue( layerH, 
											AEGP_LayerStream_CONE_FEATHER,
											AEGP_LTimeMode_CompTime,
											&comp_time,
											FALSE,
											&stream_val,
											NULL));
		light.spot_exponentF = stream_val.one_d;
		
		ERR(suites.LayerSuite5()->AEGP_GetLayerToWorldXform(	layerH, 
											&comp_time, 
											&light.light_to_world_matrix));
		ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue(	layerH, 
															AEGP_LayerStream_SHADOW_DARKNESS,
															AEGP_LTimeMode_CompTime,
															&comp_time,
															FALSE,
															&stream_val,
															NULL));
		light.shadow_densityF = stream_val.one_d;
		if (!err) {	
			light.shadow_densityF *= 0.01;
		}

		if (!err && light.type != AEGP_LightType_PARALLEL) 	{
			err = suites.StreamSuite2()->AEGP_GetLayerStreamValue(	layerH, 
																	AEGP_LayerStream_SHADOW_DIFFUSION,
																	AEGP_LTimeMode_CompTime,
																	&comp_time,
																	FALSE,
																	&stream_val,
																	NULL);
			light.radiusF = stream_val.one_d;
		}
	}
	return err;
}	

static A_Err
Artie_BuildLights(
	const PR_InData				*in_dataP,	
	PR_RenderContextH			render_contextH,		
	AEGP_MemHandle				*lightsPH)
{
	A_Err				err			=	A_Err_NONE, 
						err2		= A_Err_NONE;
	A_long				layers_to_renderL	=	0;
	Artie_Light			*lightP 	= 	NULL;
	A_Boolean			is_activeB	=	FALSE;
	AEGP_LayerH			layerH		=	NULL;;
	A_Time				comp_time		=	{0, 1}, 
						comp_time_step	=	{0, 1};
	AEGP_CompH			compH 		= 	NULL;
	AEGP_ObjectType		layer_type	=	AEGP_ObjectType_NONE;
	
	AEGP_SuiteHandler	suites(in_dataP->pica_basicP);

	ERR(suites.MemorySuite1()->AEGP_NewMemHandle(	S_aegp_plugin_id, 
													"Light Data", 
													sizeof(Artie_Light), 
													AEGP_MemFlag_CLEAR, 
													lightsPH));

	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(*lightsPH, reinterpret_cast<void**>(&lightP)));
	ERR(suites.CanvasSuite5()->AEGP_GetCompToRender(render_contextH, &compH));
	ERR(suites.LayerSuite5()->AEGP_GetCompNumLayers(compH, &layers_to_renderL));
	
	for (A_long iL = 0; !err && iL < layers_to_renderL && iL < Artie_MAX_LIGHTS; iL++){		
		ERR(suites.LayerSuite5()->AEGP_GetCompLayerByIndex(compH, iL, &layerH));
		ERR(suites.CanvasSuite5()->AEGP_GetCompRenderTime(render_contextH, &comp_time, &comp_time_step));
		ERR(suites.LayerSuite5()->AEGP_IsVideoActive(layerH, 
													AEGP_LTimeMode_CompTime,
													&comp_time, 
													&is_activeB));
		ERR(suites.LayerSuite5()->AEGP_GetLayerObjectType(layerH, &layer_type));
		
		if (!err && (AEGP_ObjectType_LIGHT == layer_type)  && is_activeB){
			err = Artie_BuildLight(in_dataP,  render_contextH, layerH,  lightP);
		}
		if (!err){
			lightP->num_lightsL++;
		}
	}
	if (lightP){
		ERR(suites.MemorySuite1()->AEGP_UnlockMemHandle(*lightsPH));
	}	
	if (lightsPH){	
		ERR2(Artie_DisposeLights(in_dataP,  *lightsPH));
	}
	return err;
}

static A_Err
Artie_CompositeImage(
	const PR_InData				*in_dataP,	
	PR_RenderContextH			render_contextH,
	AEGP_LayerQuality			quality,
	AEGP_WorldH					*srcP,
	AEGP_WorldH					*dstP)
{
	A_Err				err				= 	A_Err_NONE;
	PF_CompositeMode	comp_mode;
	PF_Quality			pf_quality		= 	PF_Quality_HI;
	PF_Pixel8			*src_pixelsP	= 	NULL;
	PF_Pixel8			*dst_pixelsP	= 	NULL;	
	PF_EffectWorld		src_effect_world;
	PF_EffectWorld		dst_effect_world;

	A_long				src_widthL			= 	0,
						src_heightL			= 	0,
						dst_widthL			=	0,
						dst_heightL			=	0;
	A_Rect				src_rect			=	{0, 0, 0, 0};

	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	
	
	A_u_long			src_rowbytesLu 	=	0,
						dst_rowbytesLu 	=	0;
	PF_Pixel8			*src_baseP		=	0,
						*dst_baseP		=	0;						
	
	ERR(suites.WorldSuite3()->AEGP_GetSize(*srcP, &src_widthL, &src_heightL));
	ERR(suites.WorldSuite3()->AEGP_GetSize(*dstP, &dst_widthL, &dst_heightL));
	src_rect.bottom = static_cast<A_short>(src_heightL);
	src_rect.right = static_cast<A_short>(src_widthL);

	comp_mode.xfer 		= PF_Xfer_IN_FRONT;
    comp_mode.rand_seed = 3141;  
    comp_mode.opacity 	= 255;
    comp_mode.rgb_only 	= FALSE;

	ERR(suites.WorldSuite3()->AEGP_FillOutPFEffectWorld(*srcP, &src_effect_world));
	ERR(suites.WorldSuite3()->AEGP_FillOutPFEffectWorld(*dstP, &dst_effect_world));

	ERR(suites.CompositeSuite2()->AEGP_TransferRect(pf_quality,
													PF_MF_Alpha_STRAIGHT,
													PF_Field_FRAME,
													&src_rect,
													&src_effect_world,
													&comp_mode,
													NULL,
													NULL,
													dst_widthL,
													dst_heightL,
													&dst_effect_world));
	return err;
}

static PF_Pixel 
Raytrace(
	const PR_InData				*in_dataP,	
		  Artie_Scene			*sceneP,
		  Ray					*rayP)
{
	Intersection    I;
	VectorType4D    Nhat,Rvec, Rrefl;
	Ray             RefRay;
	PF_Pixel        cFP, crefl;
	A_FpLong        cosThetaI;
	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	
 
	if (rayP->contribution < Artie_RAYTRACE_THRESHOLD){
		cFP.red 	= 0;
		cFP.green	= 0;
		cFP.blue 	= 0;
		cFP.alpha	= 0;
		return cFP;
	}

	if (rayP->depth > Artie_MAX_RAY_DEPTH) {
		cFP.red 	= 0;
		cFP.green	= 0;
		cFP.blue 	= 0;
		cFP.alpha	= 0;
		return cFP;
	}

	I = SceneIntersection(sceneP, rayP);

	if (I.hitB){
		cFP.red	= I.color_struck_surface.red;
		cFP.green	= I.color_struck_surface.green;
		cFP.blue	= I.color_struck_surface.blue;
		cFP.alpha	= I.color_struck_surface.alpha;

		Nhat = Normalize(suites.ANSICallbacksSuite1()->sqrt, Pdiff(I.ray.P1, I.ray.P0));
		Rvec = Normalize(suites.ANSICallbacksSuite1()->sqrt, Pdiff(rayP->P1, rayP->P0));

		/* Compute the direction of the reflected ray */
		cosThetaI = -Dot4D(Rvec, Nhat);
		
		Rrefl = Vadd(Rvec, Vscale(2.0 * cosThetaI, Nhat));

		if (I.reflectance_percentF){

			/* Cast aFP reflected ray */

			RefRay = CreateRay(I.ray.P0, PVadd(I.ray.P0, Rrefl));
			RefRay.contribution = rayP->contribution * I.reflectance_percentF;
			RefRay.depth = rayP->depth + 1;
			crefl = Raytrace(in_dataP,  sceneP, &RefRay);

			cFP.red 	+= (A_u_char)(I.reflectance_percentF * crefl.red);
			cFP.green	+= (A_u_char)(I.reflectance_percentF * crefl.green);
			cFP.blue 	+= (A_u_char)(I.reflectance_percentF * crefl.blue);
		}
	} else {
		cFP.red		= 0;
		cFP.green	= 0;
		cFP.blue	= 0;
		cFP.alpha	= 0;
	}
	return cFP;
}

static 
PF_Pixel ThrowRay(
	const	PR_InData				*in_dataP,	
			Artie_Scene				*sceneP,
			A_FpLong				xF, 
			A_FpLong				yF,
			A_FpLong				zF)
{
  Ray		R;
  PointType4D P0, P1;

  P0.P[X] = 0;
  P0.P[Y] = 0;
  P0.P[Z] = 0; 
  P0.P[W] = 1;

  P1.P[X] = xF;
  P1.P[Y] = yF; 
  P1.P[Z] = zF; 
  P1.P[W] = 1;

  R = CreateRay(P0, P1);
  R.contribution 	= 1.0;
  R.depth 			= 0;
  return Raytrace(in_dataP,  sceneP, &R);
}

static A_Err
Artie_SampleImage(
	const PR_InData				*in_dataP,	
	PR_RenderContextH			render_contextH, 
	Artie_Scene					*sceneP,
	Artie_Camera				*cameraP,
	AEGP_WorldH					*rwP)
{
	A_Err				err		= A_Err_NONE;
	A_FpLong			x		= 0, 
						y		= 0, 
						z		= cameraP->focal_lengthF;
	PF_Pixel8			*pixelP	= NULL,
						*baseP	= NULL;
	
	A_long				widthL	= 0,
						heightL	= 0;
						
	A_u_long			rowbytesLu = 0;
						
	
	AEGP_SuiteHandler	suites(in_dataP->pica_basicP);
	
	ERR(suites.WorldSuite2()->AEGP_GetSize(*rwP, &widthL, &heightL));
	ERR(suites.WorldSuite2()->AEGP_GetBaseAddr8(*rwP, &baseP));
	ERR(suites.WorldSuite2()->AEGP_GetRowBytes(*rwP, &rowbytesLu));
	
	if (z < 0){
		z *= -1;
	}
	
	for (A_long iL = 0; iL < heightL; iL++) {
		y	= -heightL/2.0 + iL + 0.5;

		pixelP = (PF_Pixel8*)( (char *)baseP + iL * rowbytesLu);	
			
		for (A_long jL = 0; jL < widthL; jL++){
			x = -widthL / 2.0 + jL + 0.5;
			*pixelP++ = ThrowRay(	in_dataP, sceneP, x, y, z);
		}
	}
	return err;
}

static A_Err 
Artie_TransformPolygon(
	Artie_Poly			*polyP,	
	const A_Matrix4		*xformP)
{
	A_Err			err		= A_Err_NONE;
	
	A_Matrix4		inverse, 
					normal_xformP;

	for(A_long iL = 0; iL < 4; iL++) {
		ERR(TransformPoint4(&polyP->vertsA[iL].coord, xformP, &polyP->vertsA[iL].coord));
	}

	ERR(InverseMatrix4(xformP, &inverse));
	ERR(TransposeMatrix4(&inverse, &normal_xformP));
	ERR(TransformVector4(&polyP->normal, &normal_xformP, &polyP->normal));

	if (!err){
		// find dominant axis;
		polyP->dominant = 2;
	
		if (ABS(polyP->normal.V[0]) > ABS(polyP->normal.V[1])) {
			if (ABS(polyP->normal.V[0]) > ABS(polyP->normal.V[2])) 			{
				polyP->dominant = 0;
			} else {
				polyP->dominant = 2;
			}
		} else if (ABS(polyP->normal.V[1]) > ABS(polyP->normal.V[2])) {
				polyP->dominant = 1;
			} else {
				polyP->dominant = 2;
		}
	}
	return err;
}

static A_Err
Artie_TransformScene(
	const PR_InData			*in_dataP,	
	PR_RenderContextH		render_contextH, 
	Artie_Scene				*sceneP,
	Artie_Camera			*cameraP)
{
	A_Err		err			= A_Err_NONE;
	A_Matrix4	composite, 
				up_sample, 
				world, 
				view, 
				down_sample;

	ERR(ScaleMatrix4( cameraP->dsf.xS, cameraP->dsf.yS, 1.0, &up_sample));
	ERR(ScaleMatrix4( 1.0 / cameraP->dsf.xS, 1.0 / cameraP->dsf.yS, 1.0, &down_sample));
	if (!err){
		view = cameraP->view_matrix;
	}

	for(A_long iL = 0; iL < sceneP->num_polysL; iL++) {
		world = sceneP->polygons[iL].world_matrix;
		ERR(MultiplyMatrix4(&up_sample, &world, &composite));
		ERR(MultiplyMatrix4(&composite, &view, &composite));
		ERR(MultiplyMatrix4(&composite, &down_sample, &composite));	
		ERR(Artie_TransformPolygon(&sceneP->polygons[iL], &composite));
	}
	return err;
}

static A_Err
Artie_Paint(
	const PR_InData				*in_dataP,	
	PR_RenderContextH			render_contextH,		
	AEGP_MemHandle				sceneH,
	AEGP_MemHandle				cameraH)
{
	A_Err				err				= A_Err_NONE,
						err2			= A_Err_NONE;
	Artie_Scene			*sceneP			= NULL;
	Artie_Camera		*cameraP		= NULL;
	AEGP_WorldH			render_world; 
	
	AEGP_WorldH			dst;
	AEGP_CompH			compH			= NULL;
	AEGP_SuiteHandler	suites(in_dataP->pica_basicP);

	A_long				widthL			=	0,
						heightL			=	0;
						
	AEGP_ItemH			comp_itemH		=	NULL;						

	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(sceneH, reinterpret_cast<void**>(&sceneP)));	
	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(cameraH, reinterpret_cast<void**>(&cameraP)));	
	ERR(suites.CanvasSuite5()->AEGP_GetCompToRender(render_contextH, &compH));	
	ERR(suites.CompSuite4()->AEGP_GetItemFromComp(compH, &comp_itemH));
	ERR(suites.ItemSuite6()->AEGP_GetItemDimensions(comp_itemH, &widthL, &heightL));
	ERR(suites.CanvasSuite5()->AEGP_GetCompDestinationBuffer(render_contextH, compH, &dst));
	
	ERR(suites.WorldSuite2()->AEGP_New(	S_aegp_plugin_id,
										AEGP_WorldType_8,
										widthL, 
										heightL, 
										&render_world));
						
	ERR(Artie_TransformScene(	in_dataP,  
								render_contextH, 
								sceneP, 
								cameraP));

	ERR(Artie_SampleImage(	in_dataP,  
							render_contextH,
							sceneP, 
							cameraP, 
							&render_world));

	ERR(Artie_CompositeImage(	in_dataP,  
								render_contextH, 
								0,
								&render_world, 
								&dst));

	if (dst){
		ERR2(suites.WorldSuite2()->AEGP_Dispose(render_world));
	}
	if (sceneP){	
		ERR2(suites.MemorySuite1()->AEGP_UnlockMemHandle(sceneH));	
	}
	if (cameraP){
		ERR2(suites.MemorySuite1()->AEGP_UnlockMemHandle(cameraH));
	}
	return err;
}

static A_Err
Artie_CreateListOfLayerContexts(
	const PR_InData				*in_dataP,
	PR_RenderContextH			render_contextH,
	A_long						*num_layersPL,
	Artie_LayerContexts			*layer_contexts)	
{
	A_Err						err 					= A_Err_NONE;
	AEGP_RenderLayerContextH	layer_contextH 			= NULL;
	A_long						local_layers_to_renderL	= 0;

	AEGP_SuiteHandler			suites(in_dataP->pica_basicP);
	AEGP_CanvasSuite5			*csP 	=	 suites.CanvasSuite5();	

	*num_layersPL =	0;
	
	err = csP->AEGP_GetNumLayersToRender(render_contextH, &local_layers_to_renderL);
	
	if (!err) {
		for(A_long iL = 0; (!err && (iL < local_layers_to_renderL) && (iL < Artie_MAX_POLYGONS)); iL++){
			ERR(csP->AEGP_GetNthLayerContextToRender(render_contextH, iL, &layer_contextH));
			if (!err){
				layer_contexts->layer_contexts[iL] = layer_contextH;
				layer_contexts->count++;
			}
		}
		*num_layersPL = local_layers_to_renderL;
	}
	return err;
}

static A_Err
Artie_GetSourceLayerSize(
	const PR_InData				*in_dataP,				/* >> */
	PR_RenderContextH			render_contextH,		/* >> */
	AEGP_LayerH					layerH,					/* >> */
	A_FpLong					*widthPF,				/* << */
	A_FpLong					*heightPF,				/* << */
	AEGP_DownsampleFactor		*dsfP)
{
	A_Err 					err 			= 	A_Err_NONE;
	AEGP_ItemH				source_itemH 	= 	NULL;
	AEGP_DownsampleFactor	dsf				=	{1, 1};
	A_long					widthL			=	0, 
							heightL			=	0;

	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	

	if (widthPF		==	NULL	||
		heightPF	==	NULL	||
		dsfP		==	NULL){
		err = A_Err_PARAMETER;
	}
	if (!err){
		if (widthPF){
			*widthPF	= 0.0;
		}
		if (heightPF){
			*heightPF	= 0.0;
		}
		if (dsfP) {
			dsfP->xS	=
			dsfP->yS	= 1;
		}

		// This doesn't return a valid item if the layer is a text layer
		// Use AEGP_GetCompRenderTime, AEGP_GetRenderLayerBounds, AEGP_GetRenderDownsampleFactor instead?
		ERR(suites.LayerSuite5()->AEGP_GetLayerSourceItem(layerH, &source_itemH));
		
		if (!err && source_itemH){
			err = suites.ItemSuite6()->AEGP_GetItemDimensions(source_itemH, &widthL, &heightL);	
			
			if (!err){
				*widthPF	= widthL 	/ (A_FpLong)dsf.xS;
				*heightPF	= heightL 	/ (A_FpLong)dsf.yS;
				
				ERR(suites.CanvasSuite5()->AEGP_GetRenderDownsampleFactor(render_contextH, &dsf));
			
				if (!err && dsfP){
					*dsfP = dsf;
				}
			}
		}
	}
	return err;
}

static A_Err
Artie_DisposePolygonTexture(
	const PR_InData			*in_dataP,
	PR_RenderContextH		render_contextH,
	Artie_Poly				*polygonP)
{
	A_Err		err	= A_Err_NONE;
	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	
	AEGP_CanvasSuite5	*canP	= suites.CanvasSuite5();

	if (polygonP->texture){
		err = canP->AEGP_DisposeTexture(render_contextH, polygonP->layer_contextH, polygonP->texture);
	}
	return err;
}



/**
 ** get the pixels and the vertices
 ** if quality = wireframe, just get the dimensions
 ** 
 ** we wait until this routine to get the layer dimension as its height and width
 ** may change with buffer expanding effects applied.
 **/

static A_Err
Artie_GetPolygonTexture(
	const PR_InData			*in_dataP,
	PR_RenderContextH		render_contextH,
	AEGP_RenderHints		render_hints,
	A_Boolean				*is_visiblePB,
	Artie_Poly				*polygonP)
{
	A_Err					err	= A_Err_NONE;
	A_FpLong				widthF = 0.0, heightF = 0.0;
	AEGP_DownsampleFactor	dsf = {1, 1};
	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	

	*is_visiblePB = FALSE;
	
	if (!polygonP->texture){
		// no texture map yet

		// if we're in wireframe, there is no texture.
		// we still need the dimensions, so we'll get the layers source dimensions and use that.
		// we'll also need to correct for the comp down sampling as all textures have this correction

		if ( polygonP->aegp_quality == AEGP_LayerQual_WIREFRAME){
		
			ERR(Artie_GetSourceLayerSize(	in_dataP, 
											render_contextH, 
											polygonP->layerH, 
											&widthF, 
											&heightF, 
											&dsf));
			if (!err){
				polygonP->origin.x			= 0;
				polygonP->origin.y			= 0;
			}
				
		} else {
			ERR(suites.CanvasSuite5()->AEGP_RenderTexture(	render_contextH,
															polygonP->layer_contextH,
															AEGP_RenderHints_NONE,
															NULL, 
															NULL,
															NULL,
															&polygonP->texture));

		}
		if (!err && polygonP->texture) {
			*is_visiblePB = TRUE;
		}
		if (!err && *is_visiblePB){
			A_FpLong	widthL 	= 0, 
						heightL = 0;
			
			err = Artie_GetSourceLayerSize(	in_dataP, 
											render_contextH, 
											polygonP->layerH, 
											&widthL, 
											&heightL, 
											&dsf);
			if (!err){
				polygonP->origin.x /= (A_FpLong)dsf.xS;	
				polygonP->origin.y /= (A_FpLong)dsf.yS;
			}
		}

		// construct the polygon vertices in local ( pixel) space.
		if (!err && *is_visiblePB) 	{
		
			A_long	widthL = 0, heightL = 0;
			
			ERR(suites.WorldSuite2()->AEGP_GetSize(polygonP->texture, &widthL, &heightL));
			
			widthF 	= static_cast<A_FpLong>(widthL);
			heightF = static_cast<A_FpLong>(heightL);

			// counterclockwise specification -- for texture map
			//vertex 0
			polygonP->vertsA[0].coord.P[0] = -polygonP->origin.x;				
			polygonP->vertsA[0].coord.P[1] = -polygonP->origin.y;   	
			polygonP->vertsA[0].coord.P[2] = 0;
			polygonP->vertsA[0].coord.P[3] = 1;

			polygonP->vertsA[0].txtur[0] = 0.0;
			polygonP->vertsA[0].txtur[1] = 0.0;
			polygonP->vertsA[0].txtur[2] = 1;


			// vertex 1
			polygonP->vertsA[1].coord.P[0] = -polygonP->origin.x;				
			polygonP->vertsA[1].coord.P[1] = heightF - polygonP->origin.y;				
			polygonP->vertsA[1].coord.P[2] = 0.0;
			polygonP->vertsA[1].coord.P[3] = 1;

			polygonP->vertsA[1].txtur[0] = 0.0;
			polygonP->vertsA[1].txtur[1] = 1.0;
			polygonP->vertsA[1].txtur[2] = 1;


			//vertex 2
			polygonP->vertsA[2].coord.P[0] = widthF  - polygonP->origin.x ;	
			polygonP->vertsA[2].coord.P[1] = heightF - polygonP->origin.y ;				
			polygonP->vertsA[2].coord.P[2] = 0;
			polygonP->vertsA[2].coord.P[3] = 1;

			polygonP->vertsA[2].txtur[0] = 1.0;
			polygonP->vertsA[2].txtur[1] = 1.0;
			polygonP->vertsA[2].txtur[2] = 1;

		
			// vertex 3
			polygonP->vertsA[3].coord.P[0] = (widthF  - polygonP->origin.x);	
			polygonP->vertsA[3].coord.P[1] = -polygonP->origin.y;
			polygonP->vertsA[3].coord.P[2] = 0;
			polygonP->vertsA[3].coord.P[3] = 1;


			polygonP->vertsA[3].txtur[0] = 1.0;
			polygonP->vertsA[3].txtur[1] = 0.0;
			polygonP->vertsA[3].txtur[2] = 1;

		}
	}
	return err;
}

static A_Err
Artie_BuildPolygon(
	const PR_InData				*in_dataP,
	PR_RenderContextH			render_contextH,
	AEGP_RenderLayerContextH	layer_contextH,
	AEGP_LayerH					layerH,
	A_Matrix4					*xform,
	A_FpLong					widthF,
	A_FpLong					heightF,
	PF_CompositeMode			*composite_mode,
	Artie_Material				*material,
	AEGP_TrackMatte				*track_matte,
	AEGP_LayerQuality			*aegp_quality,
	Artie_Poly					*polygonP)			/* <> */
{
	A_Err			err	   = A_Err_NONE;
	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	
	
	AEFX_CLR_STRUCT(*polygonP);

	polygonP->aegp_quality		= *aegp_quality;
	polygonP->layerH			= layerH;
	polygonP->layer_contextH	= layer_contextH;
	polygonP->normal.V[X]		= 0;
	polygonP->normal.V[Y]		= 0;
	polygonP->normal.V[Z]		= -1;
	polygonP->normal.V[W]		= 0;
	polygonP->material			= *material;	
	polygonP->world_matrix		= *xform;

	
	// fill in vertices and texture map
	err = Artie_GetPolygonTexture(	in_dataP,  
									render_contextH,
									AEGP_RenderHints_NONE,
									&polygonP->is_visibleB, 
									polygonP);
	return err;
}

static A_Err
Artie_AddPolygonToScene(
	const PR_InData				*in_dataP,
	PR_RenderContextH			render_contextH,
	A_long						indexL,
	AEGP_RenderLayerContextH	layer_contextH,
	AEGP_LayerH					layerH, 
	AEGP_MemHandle				sceneH)
{
	
	A_Err				err				= A_Err_NONE, 
						err2			= A_Err_NONE;
	Artie_Scene			*sceneP			= NULL;
	PF_CompositeMode	composite_mode;
	AEGP_TrackMatte		track_matte;
	A_Matrix4			xform;
	AEGP_LayerQuality	aegp_quality;
	AEGP_LayerFlags		layer_flags;
	AEGP_LayerTransferMode layer_transfer_mode;
	Artie_Poly			poly;
	A_FpLong			opacityF		= 0.0,
						widthF 			= 0.0, 
						heightF 		= 0.0;
	A_long				seedL 			= 0;
	Artie_Material		material;
	A_Time				comp_time		= {0,1},
						comp_time_step	= {0,1};
						
	AEGP_StreamVal		stream_val;
	AEGP_SuiteHandler 	suites(in_dataP->pica_basicP);	

	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(sceneH, reinterpret_cast<void**>(&sceneP)));
	ERR(suites.LayerSuite5()->AEGP_GetLayerFlags(layerH, &layer_flags));
	ERR(suites.LayerSuite5()->AEGP_GetLayerTransferMode(layerH, &layer_transfer_mode));
	ERR(suites.LayerSuite5()->AEGP_GetLayerQuality(layerH, &aegp_quality));
	ERR(suites.CanvasSuite5()->AEGP_GetCompRenderTime(render_contextH, &comp_time, &comp_time_step));
	ERR(suites.LayerSuite5()->AEGP_GetLayerToWorldXform(layerH, &comp_time, &xform));
	ERR(suites.CanvasSuite5()->AEGP_GetCompRenderTime(render_contextH, &comp_time, &comp_time_step));
	ERR(suites.CanvasSuite5()->AEGP_GetRenderOpacity(render_contextH, layer_contextH, &comp_time, &opacityF));
	ERR(suites.LayerSuite5()->AEGP_GetLayerDancingRandValue(layerH, &comp_time, &seedL));
	if (!err) {
		composite_mode.xfer			= layer_transfer_mode.mode;
		composite_mode.rand_seed	= seedL;
		composite_mode.opacity		= (unsigned char) (255.0 * opacityF / 100.0 + 0.5);
		composite_mode.rgb_only		= (layer_transfer_mode.flags & AEGP_TransferFlag_PRESERVE_ALPHA) != 0;
		track_matte					= layer_transfer_mode.track_matte;
	}
	// get polygon material
	ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, 
														AEGP_LayerStream_AMBIENT_COEFF, 
														AEGP_LTimeMode_CompTime,
														&comp_time,	
														FALSE,	
														&stream_val, 
														NULL));
	if (!err){
		material.kaF = stream_val.one_d;
		err = suites.StreamSuite2()->AEGP_GetLayerStreamValue(	layerH, 
																AEGP_LayerStream_DIFFUSE_COEFF, 
																AEGP_LTimeMode_CompTime,
																&comp_time,	
																FALSE,	
																&stream_val, 
																NULL);
		material.kdF = stream_val.one_d;

	}
	ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue(	layerH, 
															AEGP_LayerStream_SPECULAR_INTENSITY,	
															AEGP_LTimeMode_CompTime,
															&comp_time,	
															FALSE,	
															&stream_val, 
															NULL));

	// Normalize coeffs
	if (!err) {
		material.ksF = stream_val.one_d;
		material.kaF *= 0.01;	
		material.kdF *= 0.01;	
		material.ksF *= 0.01;	
	}
	ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue(layerH, 
														AEGP_LayerStream_SPECULAR_SHININESS,
														AEGP_LTimeMode_CompTime,
														&comp_time,	
														FALSE,	
														&stream_val, 
														NULL));

	if (!err) {
		material.specularF = stream_val.one_d;

		AEGP_DownsampleFactor	dsf	= {1,1};
		
		err = Artie_GetSourceLayerSize(	in_dataP, 
										render_contextH, 
										layerH, 
										&widthF, 
										&heightF, 
										&dsf);
	}

	ERR(Artie_BuildPolygon(	in_dataP, 
							render_contextH,
							layer_contextH,
							layerH,  
							&xform, 
							widthF, 
							heightF, 
							&composite_mode, 
							&material, 
							&track_matte, 
							&aegp_quality,  
							&poly));
	if (!err) {
		sceneP->polygons[indexL] = poly;
		sceneP->num_polysL++;
	}
	if (sceneP) {
		ERR2(suites.MemorySuite1()->AEGP_UnlockMemHandle(sceneH));
	}
	return err;
}

static A_Err
Artie_DisposeScene(
	const PR_InData				*in_dataP,	
	PR_RenderContextH			render_contextH,
	AEGP_MemHandle				sceneH)
{
	A_Err			err 	= A_Err_NONE, 
					err2 	= A_Err_NONE;
	Artie_Scene		*sceneP = NULL;	
	
	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	

	if (sceneH) {
		ERR(suites.MemorySuite1()->AEGP_LockMemHandle(sceneH, reinterpret_cast<void**>(&sceneP)));
		if (!err) {
			for(A_long iL = 0; iL< sceneP->num_polysL; iL++){
				ERR(Artie_DisposePolygonTexture(in_dataP, render_contextH, &sceneP->polygons[iL]));
			}
		}
		ERR2(suites.MemorySuite1()->AEGP_FreeMemHandle(sceneH));
	}
	return err;
}

static A_Err
Artie_BuildScene(
	const PR_InData				*in_dataP,	
	Artie_LayerContexts			layer_contexts,
	PR_RenderContextH			render_contextH,
	AEGP_MemHandle				*scenePH)
{
	A_Err			err					= A_Err_NONE, 
					err2				= A_Err_NONE;

	Artie_Scene		*sceneP				= NULL;
	AEGP_LayerH		layerH				= NULL;
	AEGP_CompH		compH				= NULL;
	AEGP_ItemH		source_itemH		= NULL;
	AEGP_ItemFlags	item_flags			= 0;
	A_long			layers_to_renderL	= 0;

	AEGP_MemHandle	lightsH				= NULL;
	
	AEGP_SuiteHandler	suites(in_dataP->pica_basicP);

	ERR(suites.MemorySuite1()->AEGP_NewMemHandle(	in_dataP->aegp_plug_id, 
													"Scene Data", 
													sizeof(Artie_Scene), 
													AEGP_MemFlag_CLEAR,  
													scenePH)); 

	ERR(suites.CanvasSuite5()->AEGP_GetCompToRender(render_contextH, &compH));

	ERR(Artie_CreateListOfLayerContexts(in_dataP, 
										render_contextH,
										&layers_to_renderL, 
										&layer_contexts));

	ERR(Artie_BuildLights(in_dataP, render_contextH, &lightsH));
	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(*scenePH, reinterpret_cast<void**>(&sceneP)));

	for(A_long iL = 0; iL < layers_to_renderL; iL++){			
		ERR(suites.CanvasSuite5()->AEGP_GetLayerFromLayerContext(	render_contextH, 
																	layer_contexts.layer_contexts[iL], 
																	&layerH));
			
		ERR(suites.LayerSuite5()->AEGP_GetLayerSourceItem(layerH, &source_itemH));
		ERR(suites.ItemSuite6()->AEGP_GetItemFlags(source_itemH, &item_flags));
	
		if (item_flags & AEGP_ItemFlag_HAS_VIDEO){ 
			ERR(Artie_AddPolygonToScene(	in_dataP, 
											render_contextH,
											iL,
											layer_contexts.layer_contexts[iL], 
											layerH, 
											*scenePH));
		}
	}
	ERR2(suites.MemorySuite1()->AEGP_UnlockMemHandle(*scenePH));
	return err;
}

static A_Err
Artie_DisposeCamera(
	const PR_InData			*in_dataP,	
	AEGP_MemHandle			cameraH)
{
	A_Err				err		 = A_Err_NONE;
	Artie_Camera		*cameraP = NULL;
	AEGP_SuiteHandler 	suites(in_dataP->pica_basicP);	

	if (!cameraH) {
		err = PF_Err_UNRECOGNIZED_PARAM_TYPE;
		in_dataP->msg_func(err, "Trying to dispose NULL camera");
	}
	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(cameraH, reinterpret_cast<void**>(&cameraP)));
	ERR(suites.MemorySuite1()->AEGP_FreeMemHandle(cameraH));

	return err;
}

static A_Err
Artie_CreateDefaultCamera(
	const PR_InData				*in_dataP,
	AEGP_CompH					compH,
	AEGP_DownsampleFactor		*dsfP,
	AEGP_MemHandle				*cameraPH)
{
	A_Err					err				= A_Err_NONE;
	Artie_Camera			*cameraP		= NULL;
	AEGP_ItemH				itemH			= NULL;
	A_long					widthL 			= 0, 
							heightL			= 0;
	A_FpLong				comp_origin_xF 	= 0.0, 
							comp_origin_yF 	= 0.0, 
							min_dimensionF 	= 0.0;

	A_Matrix4				matrix;

	AEGP_SuiteHandler 	suites(in_dataP->pica_basicP);	

	*cameraPH = NULL;

	ERR(suites.MemorySuite1()->AEGP_NewMemHandle(	in_dataP->aegp_plug_id, 
													"Camera Data", 
													sizeof(Artie_Camera), 
													AEGP_MemFlag_CLEAR, 
													cameraPH));

	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(*cameraPH, reinterpret_cast<void**>(&cameraP)));
	ERR(suites.CompSuite4()->AEGP_GetItemFromComp(compH, &itemH));
	ERR(suites.ItemSuite6()->AEGP_GetItemDimensions(itemH, &widthL, &heightL));

	//  comp origin is the middle of the comp in x and y, and z = 0.

	if (!err){
		comp_origin_xF = widthL / 2.0;
		comp_origin_yF = heightL / 2.0;
		min_dimensionF = MIN(comp_origin_xF, comp_origin_yF);
		A_FpLong tanF			= suites.ANSICallbacksSuite1()->tan(0.5 * Artie_DEFAULT_FIELD_OF_VIEW);
		cameraP->type			= AEGP_CameraType_PERSPECTIVE;
		cameraP->res_xLu		= static_cast<A_u_long>(widthL);
		cameraP->res_yLu		= static_cast<A_u_long>(heightL);
		cameraP->focal_lengthF	= min_dimensionF / tanF;			
		cameraP->dsf			= *dsfP;

		ERR(IdentityMatrix4(&matrix));
		ERR(TranslateMatrix4(comp_origin_xF, 
							comp_origin_yF, 
							-cameraP->focal_lengthF, 
							&matrix));
		ERR(InverseMatrix4(&matrix, &cameraP->view_matrix));
		ERR(suites.MemorySuite1()->AEGP_UnlockMemHandle(*cameraPH));
	}
	return err;
}

static A_Err
Artie_CreateLayerCamera(
	const PR_InData				*in_dataP,
	AEGP_CompH					compH,
	AEGP_DownsampleFactor		*dsfP,
	A_Time						comp_time,
	A_Rect						*roiRP0,
	AEGP_LayerH					camera_layerH,
	AEGP_MemHandle				*cameraPH)
{
	A_Err				err				= A_Err_NONE,
						err2			= A_Err_NONE;
	Artie_Camera		*cameraP		= NULL;
	AEGP_ItemH			itemH			= NULL;
	A_long				widthL			= 0, 
						heightL			= 0;	
	A_Ratio				pix_aspectR		= {1,1};
	A_FpLong			comp_origin_xF	= 0.0, 
						comp_origin_yF	= 0.0;
	A_Matrix4			matrix;
	
	AEGP_SuiteHandler suites(in_dataP->pica_basicP);	

	*cameraPH = NULL;

	ERR(suites.MemorySuite1()->AEGP_NewMemHandle(	in_dataP->aegp_plug_id, 
													"Camera Data", 
													sizeof(Artie_Camera), 
													AEGP_MemFlag_CLEAR,  
													cameraPH));	

	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(*cameraPH, reinterpret_cast<void**>(&cameraP)));
	ERR(suites.CompSuite4()->AEGP_GetItemFromComp(compH, &itemH));
	ERR(suites.ItemSuite6()->AEGP_GetItemDimensions(itemH, &widthL, &heightL));
	ERR(suites.ItemSuite6()->AEGP_GetItemPixelAspectRatio(itemH, &pix_aspectR));

	//  comp origin is the middle of the comp in x and y, and z = 0.
	if (!err) {
		comp_origin_xF					=	widthL / 2.0;
		comp_origin_yF					=	heightL / 2.0;
		cameraP->view_aspect_ratioF		=	widthL / heightL;
		cameraP->pixel_aspect_ratioF	=	pix_aspectR.num / pix_aspectR.den;
		cameraP->dsf					=	*dsfP;
	}

	ERR(suites.LayerSuite5()->AEGP_GetLayerToWorldXform(camera_layerH, &comp_time, &matrix));
	ERR(InverseMatrix4(&matrix, &cameraP->view_matrix));
	ERR(suites.CameraSuite2()->AEGP_GetCameraType(camera_layerH, &cameraP->type));

	if (!err) {
		cameraP->res_xLu	= (unsigned long) widthL;				
		cameraP->res_yLu	= (unsigned long) heightL;
		cameraP->dsf		= *dsfP;
		
		if (roiRP0){
			cameraP->roiR		=	*roiRP0;
		}
	}
	if (cameraP){
		ERR2(suites.MemorySuite1()->AEGP_UnlockMemHandle(*cameraPH));
	}
	if (*cameraPH){
		ERR2(Artie_DisposeCamera(in_dataP, *cameraPH));
		*cameraPH = NULL;
	}
	return err;
}

static A_Err
Artie_BuildCamera(
	const PR_InData				*in_dataP,	
	PR_RenderContextH			render_contextH,		
	AEGP_MemHandle				*cameraPH)
{
	A_Err					err				= A_Err_NONE;
	AEGP_LayerH				camera_layerH	= NULL;
	A_Time					render_time		= {0,1}, 
							time_step		= {0,1};
	AEGP_DownsampleFactor	dsf				= {1,1};
	AEGP_CompH				compH 			= NULL;
	A_LegacyRect			roiLR			= {0,0,0,0};
	A_Rect					roiR			= {0,0,0,0};

	*cameraPH = NULL;

	AEGP_SuiteHandler suites(in_dataP->pica_basicP);

	ERR(suites.CanvasSuite5()->AEGP_GetCompToRender(render_contextH, &compH));
	ERR(suites.CanvasSuite5()->AEGP_GetRenderDownsampleFactor(render_contextH, &dsf));
	ERR(suites.CanvasSuite5()->AEGP_GetCompRenderTime(render_contextH, &render_time, &time_step));
	ERR(suites.CameraSuite2()->AEGP_GetCamera(render_contextH, &render_time, &camera_layerH));
	ERR(suites.CanvasSuite5()->AEGP_GetROI(render_contextH, &roiLR));
	if (!err && !camera_layerH){
		ERR(Artie_CreateDefaultCamera(in_dataP,  compH, &dsf, cameraPH));
	} else 	{ 
		roiR.top	= roiLR.top;
		roiR.left	= roiLR.left;
		roiR.right	= roiLR.right;
		roiR.bottom	= roiLR.bottom;
		ERR(Artie_CreateLayerCamera(	in_dataP,  
										compH, 
										&dsf, 
										render_time, 
										&roiR,
										camera_layerH, 
										cameraPH));
	}
	return err;
}

static A_Err 
Artie_Render(
	const PR_InData			*in_dataP,					/* >> */
	PR_GlobalContextH		global_contextH,			/* >> */
	PR_InstanceContextH		instance_contextH,			/* >> */
	PR_RenderContextH		render_contextH,			/* >> */
	PR_GlobalDataH			global_dataH,				/* <> */
	PR_InstanceDataH		instance_dataH,				/* <> */
	PR_RenderDataH			render_dataH)
{
	A_Err				err	= A_Err_NONE,
						err2 = A_Err_NONE;

	AEGP_MemHandle		cameraH		= NULL;
	AEGP_MemHandle		sceneH		= NULL;
	Artie_LayerContexts	layer_contexts;

	AEFX_CLR_STRUCT(layer_contexts);

	ERR(Artie_BuildCamera(in_dataP, render_contextH, &cameraH));
	
	if (cameraH){
		ERR(Artie_BuildScene(in_dataP, layer_contexts, render_contextH, &sceneH));
		if (sceneH){
			ERR(Artie_Paint(in_dataP, render_contextH, sceneH, cameraH));
			ERR2(Artie_DisposeScene(in_dataP, render_contextH, sceneH)); 	
			ERR2(Artie_DisposeCamera(in_dataP,  cameraH));
		}
	}
	return err;
}

A_Err
EntryPointFunc(
	struct SPBasicSuite		*pica_basicP,			/* >> */
	A_long				 	major_versionL,			/* >> */		
	A_long					minor_versionL,			/* >> */		
	AEGP_PluginID			aegp_plugin_id,			/* >> */
	AEGP_GlobalRefcon		*plugin_refconP)		/* << */
{
	A_Err 						err					= A_Err_NONE;

	A_Version					api_version;
	A_Version					artisan_version;
	char						match_nameA[PR_PUBLIC_MATCH_NAME_LEN];
	char						artisan_nameA[PR_PUBLIC_ARTISAN_NAME_LEN];
	PR_ArtisanEntryPoints		entry_funcs;
	AEGP_SuiteHandler			suites(pica_basicP);
	
	api_version.majorS = PR_ARTISAN_API_VERSION_MAJOR;
	api_version.minorS = PR_ARTISAN_API_VERSION_MINOR;

	artisan_version.majorS	= Artie_MAJOR_VERSION;
	artisan_version.minorS	= Artie_MAJOR_VERSION;

	S_aegp_plugin_id		= aegp_plugin_id;
	S_sp_basic_suiteP		= pica_basicP;

	suites.ANSICallbacksSuite1()->strcpy(match_nameA,	Artie_ARTISAN_MATCH_NAME);
	suites.ANSICallbacksSuite1()->strcpy(artisan_nameA,	Artie_ARTISAN_NAME);

	// 0 at the end of the name means optional function
	// only render_func is not optional.
	
	try {
		entry_funcs.do_instance_dialog_func0		= Artie_DoInstanceDialog;
		entry_funcs.flatten_instance_func0			= Artie_FlattenInstance;
		entry_funcs.frame_setdown_func0				= Artie_FrameSetdown;
		entry_funcs.frame_setup_func0				= Artie_FrameSetup;
		entry_funcs.global_do_about_func0			= Artie_GlobalDoAbout;
		entry_funcs.global_setdown_func0			= Artie_GlobalSetdown;
		entry_funcs.global_setup_func0				= Artie_GlobalSetup;
		entry_funcs.render_func						= Artie_Render;
		entry_funcs.setup_instance_func0			= Artie_SetupInstance;
		entry_funcs.setdown_instance_func0			= Artie_SetdownInstance;
		entry_funcs.query_func0						= Artie_Query;

		#ifdef DEBUG
			ERR(suites.MemorySuite1()->AEGP_SetMemReportingOn(TRUE));
		#endif
		
		ERR(suites.RegisterSuite5()->AEGP_RegisterArtisan(	api_version, 
										artisan_version, 
										aegp_plugin_id,
										plugin_refconP,
										match_nameA,
										artisan_nameA,
										&entry_funcs));

		ERR(suites.RegisterSuite5()->AEGP_RegisterDeathHook(	S_aegp_plugin_id,		
											Artie_DeathHook,		
											NULL));
	} catch (A_Err &thrown_err){
		err = 	thrown_err;
	}						
	return err;
}


/*	Everything below this line is specific to Artie's implementation, and 
	has little to do with the Artisan interface. Hopefully, your plug-in
	does something more exciting than basic raytracing.

*/

static void 
NormalizePoint(
	PointType4D *P1)
{
  double *p = P1->P;
  double  w = p[W];

  if (w != 1.0){
    /* We are assuming that the order in P is:  X, Y, Z, W */
    *p++ /= w;  
    *p++ /= w; 
    *p++ /= w;
    *p = 1.0;
  }
}

static VectorType4D 
Normalize(
	A_FpLong	(*sqrt)(A_FpLong),  
	VectorType4D v)
{
  VectorType4D vout;
  A_FpLong l = sqrt( v.V[0]*v.V[0] + v.V[1]*v.V[1] + v.V[2]*v.V[2]);
 
  if (l < Artie_EPSILON){
    vout = v;
  } else {
    vout.V[0] = v.V[0]/l;
    vout.V[1] = v.V[1]/l;
    vout.V[2] = v.V[2]/l;
    vout.V[3] = 0.0;
  }
  return vout;
}

/*	Return the vector V=P1-P0. */

static VectorType4D 
Pdiff(
	PointType4D P1,
	PointType4D P0)
{
  VectorType4D V;

  NormalizePoint(&P0);
  NormalizePoint(&P1);

  V.V[0] = P1.P[0] - P0.P[0];
  V.V[1] = P1.P[1] - P0.P[1];
  V.V[2] = P1.P[2] - P0.P[2];
  V.V[3] = 0.0;

  return V;
}

/*	Return the vector s*V. */

static VectorType4D 
Vscale(
	PF_FpLong sF,
	VectorType4D V)
{
  V.V[X] *= sF;  
  V.V[Y] *= sF;  
  V.V[Z] *= sF; 
  V.V[W] = 0.0;

  return V;
}

/*	Return the vector V1+V1. */

VectorType4D 
Vadd(
	VectorType4D 	V1, 
	VectorType4D 	V2)
{
  VectorType4D V;

  V.V[X] = V1.V[X] + V2.V[X];
  V.V[Y] = V1.V[Y] + V2.V[Y];
  V.V[Z] = V1.V[Z] + V2.V[Z];
  V.V[W] = 0.0;

  return V;
}

/*	Return the point P+V. */

PointType4D 
PVadd(
	PointType4D 	P, 
	VectorType4D 	V)
{
  PointType4D p;

  NormalizePoint(&P);
  p.P[X] = P.P[X] + V.V[X];
  p.P[Y] = P.P[Y] + V.V[Y];
  p.P[Z] = P.P[Z] + V.V[Z];
  p.P[W] = 1.0;

  return p;
}

/* Return the negative of vector V */

VectorType4D 
Vneg(
VectorType4D 	V)
{
	V.V[X] = -V.V[X];
	V.V[Y] = -V.V[Y];
	V.V[Z] = -V.V[Z];
	
	/* since V is aFP vector, V.V[W] is always 0. */
	return V;
}

A_FpLong 
Dot4D(
	VectorType4D 	v1F, 
	VectorType4D 	v2F)
{
  return v1F.V[0] * v2F.V[0] + v1F.V[1] * v2F.V[1] + v1F.V[2] * v2F.V[2];
}

A_Err
IdentityMatrix4(
	A_Matrix4 	*matrixP)
{
	AEFX_CLR_STRUCT(*matrixP);

	matrixP->mat[0][0] = 1.0;
	matrixP->mat[1][1] = 1.0;
	matrixP->mat[2][2] = 1.0;
	matrixP->mat[3][3] = 1.0;

  return A_Err_NONE; 
}

A_Err
TranslateMatrix4(
	A_FpLong 	x,
	A_FpLong 	y,
	A_FpLong 	z,
	A_Matrix4 	*resultP)
{
	IdentityMatrix4(resultP);

	resultP->mat[3][0] = x;
	resultP->mat[3][1] = y;
	resultP->mat[3][2] = z;
	resultP->mat[3][3] = 1.0;

	return A_Err_NONE;
}

A_Err
ScaleMatrix4(
	A_FpLong x,
	A_FpLong y,
	A_FpLong z,
	A_Matrix4 *resultP)
{
	IdentityMatrix4(resultP);

	resultP->mat[0][0] = x;
	resultP->mat[1][1] = y;
	resultP->mat[2][2] = z;
	resultP->mat[3][3] = 1.0;

	return A_Err_NONE;
}

A_Err 
TransposeMatrix4(
	const A_Matrix4 *matrix1,		
	 	  A_Matrix4 *resultP) 		

{
	A_u_long iLu, jLu;
	A_Matrix4	tmp;

	for (iLu = 0 ; iLu < 4 ; iLu++){
		for (jLu = 0 ; jLu < 4 ; jLu++) {
			tmp.mat[iLu][jLu] = matrix1->mat[jLu][iLu];
		}
	}

	AEFX_COPY_STRUCT(tmp, *resultP);
	
	return A_Err_NONE;
}

/* compute inverse, if no inverse, return adjoint */ 

A_Err 
InverseMatrix4(
	const	A_Matrix4	*m,
			A_Matrix4 *resultP)
{
	A_Err err = A_Err_NONE;

	A_FpLong d00, d01, d02, d03,
			 d10, d11, d12, d13,
			 d20, d21, d22, d23,
			 d30, d31, d32, d33,
			 m00, m01, m02, m03,
			 m10, m11, m12, m13,
			 m20, m21, m22, m23,
			 m30, m31, m32, m33,
			 D;

	m00 = m->mat[0][0];  m01 = m->mat[0][1];  m02 = m->mat[0][2];  m03 = m->mat[0][3];
	m10 = m->mat[1][0];  m11 = m->mat[1][1];  m12 = m->mat[1][2];  m13 = m->mat[1][3];
	m20 = m->mat[2][0];  m21 = m->mat[2][1];  m22 = m->mat[2][2];  m23 = m->mat[2][3];
	m30 = m->mat[3][0];  m31 = m->mat[3][1];  m32 = m->mat[3][2];  m33 = m->mat[3][3];

	d00 = m11*m22*m33 + m12*m23*m31 + m13*m21*m32 - m31*m22*m13 - m32*m23*m11 - m33*m21*m12;
	d01 = m10*m22*m33 + m12*m23*m30 + m13*m20*m32 - m30*m22*m13 - m32*m23*m10 - m33*m20*m12;
	d02 = m10*m21*m33 + m11*m23*m30 + m13*m20*m31 - m30*m21*m13 - m31*m23*m10 - m33*m20*m11;
	d03 = m10*m21*m32 + m11*m22*m30 + m12*m20*m31 - m30*m21*m12 - m31*m22*m10 - m32*m20*m11;

	d10 = m01*m22*m33 + m02*m23*m31 + m03*m21*m32 - m31*m22*m03 - m32*m23*m01 - m33*m21*m02;
	d11 = m00*m22*m33 + m02*m23*m30 + m03*m20*m32 - m30*m22*m03 - m32*m23*m00 - m33*m20*m02;
	d12 = m00*m21*m33 + m01*m23*m30 + m03*m20*m31 - m30*m21*m03 - m31*m23*m00 - m33*m20*m01;
	d13 = m00*m21*m32 + m01*m22*m30 + m02*m20*m31 - m30*m21*m02 - m31*m22*m00 - m32*m20*m01;

	d20 = m01*m12*m33 + m02*m13*m31 + m03*m11*m32 - m31*m12*m03 - m32*m13*m01 - m33*m11*m02;
	d21 = m00*m12*m33 + m02*m13*m30 + m03*m10*m32 - m30*m12*m03 - m32*m13*m00 - m33*m10*m02;
	d22 = m00*m11*m33 + m01*m13*m30 + m03*m10*m31 - m30*m11*m03 - m31*m13*m00 - m33*m10*m01;
	d23 = m00*m11*m32 + m01*m12*m30 + m02*m10*m31 - m30*m11*m02 - m31*m12*m00 - m32*m10*m01;

	d30 = m01*m12*m23 + m02*m13*m21 + m03*m11*m22 - m21*m12*m03 - m22*m13*m01 - m23*m11*m02;
	d31 = m00*m12*m23 + m02*m13*m20 + m03*m10*m22 - m20*m12*m03 - m22*m13*m00 - m23*m10*m02;
	d32 = m00*m11*m23 + m01*m13*m20 + m03*m10*m21 - m20*m11*m03 - m21*m13*m00 - m23*m10*m01;
	d33 = m00*m11*m22 + m01*m12*m20 + m02*m10*m21 - m20*m11*m02 - m21*m12*m00 - m22*m10*m01;

	D = m00*d00 - m01*d01 + m02*d02 - m03*d03;

	if (D) {
		resultP->mat[0][0] =  d00/D; resultP->mat[0][1] = -d10/D;  resultP->mat[0][2] =  d20/D; resultP->mat[0][3] = -d30/D;
		resultP->mat[1][0] = -d01/D; resultP->mat[1][1] =  d11/D;  resultP->mat[1][2] = -d21/D; resultP->mat[1][3] =  d31/D;
		resultP->mat[2][0] =  d02/D; resultP->mat[2][1] = -d12/D;  resultP->mat[2][2] =  d22/D; resultP->mat[2][3] = -d32/D;
		resultP->mat[3][0] = -d03/D; resultP->mat[3][1] =  d13/D;  resultP->mat[3][2] = -d23/D; resultP->mat[3][3] =  d33/D;
	} else {
		resultP->mat[0][0] =  d00; resultP->mat[0][1] = -d10;  resultP->mat[0][2] =  d20; resultP->mat[0][3] = -d30;
		resultP->mat[1][0] = -d01; resultP->mat[1][1] =  d11;  resultP->mat[1][2] = -d21; resultP->mat[1][3] =  d31;
		resultP->mat[2][0] =  d02; resultP->mat[2][1] = -d12;  resultP->mat[2][2] =  d22; resultP->mat[2][3] = -d32;
		resultP->mat[3][0] = -d03; resultP->mat[3][1] =  d13;  resultP->mat[3][2] = -d23; resultP->mat[3][3] =  d33;
	}
	return err;
}

A_Err
MultiplyMatrix4(
	const A_Matrix4	*A,
	const A_Matrix4	*B,
		  A_Matrix4	*resultP)
{
	A_Err err = A_Err_NONE;

	A_Matrix4	tmp;

	for (A_u_long iLu = 0; iLu < 4; iLu++){
		for (int jLu = 0; jLu < 4; jLu++) {
			tmp.mat[iLu][jLu] = A->mat[iLu][0] * B->mat[0][jLu] + 
								A->mat[iLu][1] * B->mat[1][jLu] +
								A->mat[iLu][2] * B->mat[2][jLu] +
								A->mat[iLu][3] * B->mat[3][jLu];
		}
	}
	AEFX_COPY_STRUCT(tmp, *resultP);
	return err;
}

A_Err
TransformPoint4(
	const PointType4D	*pointP,
	const A_Matrix4		*transformP,
		  PointType4D	*resultP)
{
	A_Err err = A_Err_NONE;
	PointType4D	tmp;

	Artie_TRANSFORM_POINT4(*pointP, *transformP, tmp);

	*resultP  = tmp;

	return err;
}

A_Err
TransformVector4(
	const	VectorType4D *vectorP,
	const	A_Matrix4	 *matrixP,
			VectorType4D *resultP)
{
	A_Err err = A_Err_NONE;
	VectorType4D tmp;

	Artie_TRANSFORM_VECTOR4(*vectorP, *matrixP, tmp);

	*resultP = tmp;

	return err;
}

Ray CreateRay(PointType4D P0, PointType4D P1)
{
  Ray R;

  R.P0 = P0;
  R.P1 = P1;
  return R;
}

Ray
TransformRay(
	const Ray		*rayP,
	const A_Matrix4	*xformP)
{
	Ray	R;

	TransformPoint4(&rayP->P0, xformP, &R.P0);
	TransformPoint4(&rayP->P1, xformP, &R.P1);

	return R;
}

