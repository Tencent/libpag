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

#ifndef _ARTIE_H_
#define _Artie_H_

#include "AEConfig.h"
#include "entry.h"
#include "AE_GeneralPlug.h"
#include "Artie_types.h"
#include "Artie_vector.h"
#include "AEGP_SuiteHandler.h"

#define Artie_MAJOR_VERSION 1
#define Artie_MINOR_VERSION 0

#define Artie_ARTISAN_MATCH_NAME	"ADBE SDK Artie"
#define Artie_ARTISAN_NAME			"Artie"

#define Artie_MAX_LIGHTS				10
#define Artie_DEFAULT_FIELD_OF_VIEW		1.0
#define Artie_RAYTRACE_THRESHOLD		0.1
#define Artie_MAX_RAY_DEPTH				3

#ifndef ABS
#define ABS(_A) ((_A) < 0 ? -(_A) : (_A))
#endif

typedef struct {
	AEGP_RenderLayerContextH			layer_contexts[Artie_MAX_POLYGONS];
	A_long								count;
} Artie_LayerContexts, *Artie_LayerContextsP, **Artie_LayerContextsH;

// This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;

#define	Artie_TRANSFORM_POINT4(_IN, _MATRIX, _OUT)													\
		do {																							\
																										\
			(_OUT).P[0] =	(_IN).P[0] * (_MATRIX).mat[0][0] + (_IN).P[1] * (_MATRIX).mat[1][0] +			\
							(_IN).P[2] * (_MATRIX).mat[2][0] + (_IN).P[3] * (_MATRIX).mat[3][0];			\
																										\
			(_OUT).P[1] =	(_IN).P[0] * (_MATRIX).mat[0][1] + (_IN).P[1] * (_MATRIX).mat[1][1] +			\
							(_IN).P[2] * (_MATRIX).mat[2][1] + (_IN).P[3] * (_MATRIX).mat[3][1];			\
																										\
			(_OUT).P[2] =	(_IN).P[0] * (_MATRIX).mat[0][2] + (_IN).P[1] * (_MATRIX).mat[1][2] +			\
							(_IN).P[2] * (_MATRIX).mat[2][2] + (_IN).P[3] * (_MATRIX).mat[3][2];			\
																										\
			(_OUT).P[3] =	(_IN).P[0] * (_MATRIX).mat[0][3] + (_IN).P[1] * (_MATRIX).mat[1][3] +			\
							(_IN).P[2] * (_MATRIX).mat[2][3] + (_IN).P[3] * (_MATRIX).mat[3][3];			\
																										\
			if ((_OUT).P[3] != 0) {																	\
				A_FpLong	inv_homoF = 1.0 / (_OUT).P[3];												\
				(_OUT).P[0] *=  inv_homoF;																\
				(_OUT).P[1] *=  inv_homoF;																\
				(_OUT).P[2] *=  inv_homoF;																\
				(_OUT).P[3] =   1.0;																	\
			}																							\
		} while (0)

#define	Artie_TRANSFORM_VECTOR4(_IN, _MATRIX, _OUT)													\
		do {																							\
																										\
			(_OUT).V[0] =	(_IN).V[0] * (_MATRIX).mat[0][0] + (_IN).V[1] * (_MATRIX).mat[1][0] +		\
							(_IN).V[2] * (_MATRIX).mat[2][0] + (_IN).V[3] * (_MATRIX).mat[3][0];		\
																										\
			(_OUT).V[1] =	(_IN).V[0] * (_MATRIX).mat[0][1] + (_IN).V[1] * (_MATRIX).mat[1][1] +		\
							(_IN).V[2] * (_MATRIX).mat[2][1] + (_IN).V[3] * (_MATRIX).mat[3][1];		\
																										\
			(_OUT).V[2] =	(_IN).V[0] * (_MATRIX).mat[0][2] + (_IN).V[1] * (_MATRIX).mat[1][2] +		\
							(_IN).V[2] * (_MATRIX).mat[2][2] + (_IN).V[3] * (_MATRIX).mat[3][2];		\
																										\
			(_OUT).V[3] =	(_IN).V[0] * (_MATRIX).mat[0][3] + (_IN).V[1] * (_MATRIX).mat[1][3] +		\
							(_IN).V[2] * (_MATRIX).mat[2][3] + (_IN).V[3] * (_MATRIX).mat[3][3];		\
																										\
			if ((_OUT).V[3] != 0) {																		\
				A_FpLong	inv_homoF = 1.0 / (_OUT).V[3];												\
				(_OUT).V[0] *=  inv_homoF;																\
				(_OUT).V[1] *=  inv_homoF;																\
				(_OUT).V[2] *=  inv_homoF;																\
				(_OUT).V[3] =   1.0;																	\
			}																							\
		} while (0)



#define			AEFX_COPY_STRUCT(FROM, TO)	\
	do {									\
		long _t = sizeof(FROM);				\
		char *_p = (char*)&(FROM);			\
		char *_q = (char*)&(TO);			\
		while (_t--) {						\
			*_q++ = *_p++;					\
		}									\
	} while (0);										

#endif