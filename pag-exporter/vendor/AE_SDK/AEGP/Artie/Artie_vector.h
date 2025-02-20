#ifndef _Artie_VECTOR_H_
#define _Artie_VECTOR_H_

#include <AE_GeneralPlug.h>
#include <AE_EffectUI.h>
#include <AE_EffectCBSuites.h>
#include <AE_AdvEffectSuites.h>
#include <AE_Macros.h>
#include <PR_Public.h>

/**
 ** All the typedefs for the Artie ray tracer
 **/

#define X 0
#define Y 1
#define Z 2
#define W 3

#define Artie_EPSILON 1e-7



#define MIN(A,B)			((A) < (B) ? (A) : (B))



typedef struct P4D { A_FpLong P[4];} PointType4D;
typedef struct V4D { A_FpLong V[4];} VectorType4D;



typedef struct {
  PointType4D	P0, P1;
  A_FpLong		contribution;
  A_long		depth;         /* Distance from root in ray tree */
} Ray;



static void NormalizePoint(PointType4D *P1);

static VectorType4D Pdiff(PointType4D P1,PointType4D P0);

static VectorType4D Vscale(A_FpLong s, VectorType4D V);

VectorType4D Vadd(VectorType4D V1, VectorType4D V2);

PointType4D PVadd(PointType4D P, VectorType4D V);

VectorType4D Vneg(VectorType4D V);

A_FpLong Dot4D(VectorType4D v1, VectorType4D v2);

static VectorType4D Normalize(A_FpLong	(*sqrt)(A_FpLong), VectorType4D v);


A_Err
IdentityMatrix4(
	A_Matrix4 *matrixP);



A_Err
TranslateMatrix4(
	A_FpLong x,
	A_FpLong y,
	A_FpLong z,
	A_Matrix4 *result);


A_Err
ScaleMatrix4(
	A_FpLong x,
	A_FpLong y,
	A_FpLong z,
	A_Matrix4 *result);


A_Err 
TransposeMatrix4(
	const A_Matrix4 *matrix1,		
	 	  A_Matrix4 *result);


A_Err 
InverseMatrix4(
	const	A_Matrix4	*m,
			A_Matrix4 *result);


A_Err
MultiplyMatrix4(
	const A_Matrix4	*A,
	const A_Matrix4	*B,
	A_Matrix4	*result);


A_Err
TransformPoint4(
	const	PointType4D *P,
	const	A_Matrix4	*T,
			PointType4D *result);


A_Err
TransformVector4(
	const	VectorType4D *vectorP,
	const	A_Matrix4	 *matrixP,
			VectorType4D *resultP);


Ray 
CreateRay(PointType4D P0, PointType4D P1);



Ray 
TransformRay(
	const Ray			*rayP,
	const A_Matrix4		*xformP);


#endif

