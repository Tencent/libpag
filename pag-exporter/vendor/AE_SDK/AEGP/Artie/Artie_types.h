#ifndef _Artie_TYPES_H_
#define _Artie_TYPES_H_

#include <AE_GeneralPlug.h>

#ifndef __MACH__
	#include <stdio.h>
	#include <string.h>
#endif
#include <AE_EffectUI.h>
#include <AE_EffectCBSuites.h>
#include <AE_AdvEffectSuites.h>
#include <AE_Macros.h>
#include <PR_Public.h>
#include "Artie_vector.h"



/**
 ** All the typedefs for the Artie ray tracer
 **/
#define Artie_PROB_S(_ERR, _MSG)	in_dataP->msg_func(_ERR, _MSG)


typedef struct  {
	AEGP_CameraType			type;					// type of lens
	A_FpLong				focal_lengthF;			// distance from eye to image plane. 
	A_u_long				res_xLu;				// x_resolution in pixels
	A_u_long				res_yLu;				// y_resolution in pixels
	AEGP_DownsampleFactor	dsf;					// of comp
	A_Matrix4				view_matrix;
	A_FpLong				view_aspect_ratioF;
	A_FpLong				pixel_aspect_ratioF;
	A_Rect					roiR;					// region of interest rectangle
} Artie_Camera;

typedef struct {
	A_long				magic;
	A_long				idL;
	AEGP_LightType		type;
	PF_Pixel			color;
	A_FpLong			intensityF;

	A_FpLong			radiusF;			// soft shadow blur radius
	A_FpLong			shadow_densityF;	// density of shadow  (0 - 1)

	A_FloatPoint3		position;			// location of local or spotlight - always (0, 0, 0) in layer coords
	A_FloatPoint3		direction;			// direction of spot or infinite 
	A_FloatPoint3		attenuation;		// 1/(kc + kl * dist + kq * dist * dist)
	A_FpLong			cone_angleF;		// in degrees, > 90 represents represents a point light
	A_FpLong			spot_exponentF;		// cosine falloff

	A_Boolean			transformedB;		// has position, etc been transformed by matrix
	A_Boolean			casts_shadowsB;
	A_Matrix4			light_to_world_matrix;		
	A_long				num_lightsL;	

} Artie_Light;



typedef struct {
	A_FpLong		kaF, kdF, ksF;
	A_FpLong		specularF;
} Artie_Material;


typedef struct {
   PointType4D		coord;		/* homogeneous coords */
   A_FpLong			txtur[3];   /* homegeneous (u,v)-coordinates */
} Artie_PolyVert;



typedef struct {
	A_Boolean							is_visibleB;
	AEGP_LayerQuality					aegp_quality;
	Artie_Material						material;						/* shading info */
	AEGP_RenderLayerContextH			layer_contextH;							
	AEGP_LayerH							layerH;							/* associated layer */
	VectorType4D						normal;							/* normal vector */
	long								dominant;						/* index of biggest normal component */
	A_FloatPoint						origin;							/* offset */
    Artie_PolyVert						vertsA[4];						/* vertices */
	A_Matrix4							world_matrix;					/* layer to world */
	AEGP_WorldH							texture;						/* pixels */
} Artie_Poly;


#define Artie_MAX_POLYGONS 10

typedef struct {
	A_long			num_polysL;
	Artie_Poly		polygons[Artie_MAX_POLYGONS];
} Artie_Scene;


#define Artie_MISS 1e10


typedef struct {		
  A_Boolean hitB;							/* True if an actual intersection occured */
  Ray		ray;							/* A ray from the point of intersection in the normal direction. */
  double	intersect_point_coord_on_ray;	/* Coordinate along the ray where intersection point occurred. */
  PF_Pixel  color_struck_surface;			/* color of struck surface */
  A_FpLong	reflectance_percentF;			/* reflectance percent */
}Intersection;




#endif