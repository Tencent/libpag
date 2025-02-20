/*
** ADOBE CONFIDENTIAL
**
** Copyright 2017 Adobe
** All Rights Reserved.
**
** NOTICE: Adobe permits you to use, modify, and distribute this file in
** accordance with the terms of the Adobe license agreement accompanying
** it. If you have received this file from a source other than Adobe,
** then your use, modification, or distribution of it requires the prior
** written permission of Adobe.
*/


#ifndef GFKERNELSUPPORT_KERNELHALF_H
#define GFKERNELSUPPORT_KERNELHALF_H

/**
** Half4ToFloat* and Float4ToHalf* are implementation details and are not portable to OpenCL.
** These exist as an aid to KernelMemory.h
*/
#if GF_DEVICE_TARGET_CUDA
#include "cuda_fp16.h"

GF_DEVICE_FUNCTION float HalfToFloat(
	unsigned short inV)
{
	return 	__half2float(inV);
}

GF_DEVICE_FUNCTION unsigned short FloatToHalf(
	float inV)
{
	return __float2half_rn(inV);
}

GF_DEVICE_FUNCTION float2 Half2ToFloat2(
	ushort2 inV)
{
	return make_float2(
		HalfToFloat(inV.x),
		HalfToFloat(inV.y));
}

GF_DEVICE_FUNCTION ushort2 Float2ToHalf2(
	float2 inV)
{
	return make_ushort2(
		FloatToHalf(inV.x),
		FloatToHalf(inV.y));
}

GF_DEVICE_FUNCTION float4 Half4ToFloat4(
	ushort4 inV)
{
	return make_float4(
		HalfToFloat(inV.x),
		HalfToFloat(inV.y),
		HalfToFloat(inV.z),
		HalfToFloat(inV.w));
}

GF_DEVICE_FUNCTION ushort4 Float4ToHalf4(
	float4 inV)
{
	return make_ushort4(
		FloatToHalf(inV.x),
		FloatToHalf(inV.y),
		FloatToHalf(inV.z),
		FloatToHalf(inV.w));
}

#elif GF_DEVICE_TARGET_METAL

float HalfToFloat(
	half inV)
{
	return inV;
}

half FloatToHalf(
	float inV)
{
	return inV;
}

half2 Float2ToHalf2(
	float2 inV)
{
	return make_half2(inV.x, inV.y);
}

float2 Half2ToFloat2(
	half2 inV)
{
	return make_float2(inV.x, inV.y);
}

float4 Half4ToFloat4(
	half4 inV)
{
	return make_float4(inV.x, inV.y, inV.z, inV.w);
}

half4 Float4ToHalf4(
	float4 inV)
{
	return make_half4(inV.x, inV.y, inV.z, inV.w);
}

#endif

#endif //GFKERNELSUPPORT_KERNELHALF_H