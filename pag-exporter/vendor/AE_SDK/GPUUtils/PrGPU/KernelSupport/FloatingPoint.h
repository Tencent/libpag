/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2004 Adobe Systems Incorporated                       */
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


#ifndef PRGPUKERNELSUPPORT_FLOATINGPOINT_H
#	define PRGPUKERNELSUPPORT_FLOATINGPOINT_H

#include "PrGPU/KernelSupport/KernelCore.h"

#if GF_DEVICE_TARGET_METAL
#	include <metal_math>
#endif

#if GF_DEVICE_TARGET_CUDA

	GF_DEVICE_FUNCTION float Power(float inX, float inP)
	{
		return inX >= 0 ? __powf(inX, inP) : -__powf(-inX, inP);
	}

#elif GF_DEVICE_TARGET_OPENCL

	GF_DEVICE_FUNCTION float Power(float inX, float inP)
	{
		return inX >= 0 ? native_powr(inX, inP) : -native_powr(-inX, inP);
	}

	GF_DEVICE_FUNCTION float fdividef(float inX, float inY)
	{
		return native_divide(inX, inY);
	}

	GF_DEVICE_FUNCTION float fabsf(float inX)
	{
		return fabs(inX);
	}

	GF_DEVICE_FUNCTION float rsqrtf(float inX)
	{
		return native_rsqrt(inX);
	}

	GF_DEVICE_FUNCTION float sqrtf(float inX)
	{
		return native_sqrt(inX);
	}

	GF_DEVICE_FUNCTION float cosf(float inX)
	{
		return native_cos(inX);
	}

	GF_DEVICE_FUNCTION float sinf(float inX)
	{
		return native_sin(inX);
	}

	GF_DEVICE_FUNCTION float atan2f(float inX, float inY)
	{
		return atan2(inX, inY);
	}

	GF_DEVICE_FUNCTION float logf(float inX)
	{
		return native_log(inX);
	}

#elif GF_DEVICE_TARGET_METAL

	#define saturate precise::saturate
	#define rsqrtf rsqrt

    GF_DEVICE_FUNCTION float Power(float inX, float inP)
    {
        return inX >= 0 ? metal::powr(inX, inP) : -metal::powr(-inX, inP);
    }
                 
    GF_DEVICE_FUNCTION float fdividef(float inX, float inY)
    {
       return inX/inY;
    }
                 
    GF_DEVICE_FUNCTION float fabsf(float inX)
    {
       return metal::abs(inX);
    }
                     
    GF_DEVICE_FUNCTION float sqrtf(float inX)
	{
		return metal::sqrt(inX);
	}
	
	GF_DEVICE_FUNCTION float cosf(float inX)
	{
		return metal::cos(inX);
	}
	
	GF_DEVICE_FUNCTION float sinf(float inX)
	{
		return metal::sin(inX);
	}

	GF_DEVICE_FUNCTION float atan2f(float inX, float inY)
	{
		return metal::atan2(inX, inY);
	}

	GF_DEVICE_FUNCTION float logf(float inX)
	{
		return metal::log(inX);
	}

#endif
	
GF_HOST_SECTION(namespace GF { )

	GF_DEVICE_FUNCTION float LERP(float inA, float inB, float inS)
	{
		return inA + inS * (inB - inA);
	}

	GF_DEVICE_FUNCTION GF_HOST_FUNCTION bool fIsEqual(
		float inValue0,
		float inValue1)
	{
		return fabsf(inValue0 - inValue1) < 0.000008f;
	}
	
	GF_DEVICE_FUNCTION GF_HOST_FUNCTION bool fIsNotEqual(
		float inValue0,
		float inValue1)
	{
		return !fIsEqual(inValue0, inValue1);
	}
	
	GF_DEVICE_FUNCTION GF_HOST_FUNCTION bool fIsLessThan(
		float inValue0,
		float inValue1)
	{
		return (inValue0 - 0.000008f) < inValue1;
	}
	
	GF_DEVICE_FUNCTION GF_HOST_FUNCTION bool fIsLessThanOrEqual(
		float inValue0,
		float inValue1)
	{
		return (inValue0 - 0.000008f) <= inValue1;
	}
	
	GF_DEVICE_FUNCTION GF_HOST_FUNCTION bool fIsGreaterThan(
		float inValue0,
		float inValue1)
	{
		return (inValue0 + 0.000008f) > inValue1;
	}
	
	GF_DEVICE_FUNCTION GF_HOST_FUNCTION bool fIsGreaterThanOrEqual(
		float inValue0,
		float inValue1)
	{
		return (inValue0 + 0.000008f) >= inValue1;
	}

GF_HOST_SECTION( } )
	
#endif