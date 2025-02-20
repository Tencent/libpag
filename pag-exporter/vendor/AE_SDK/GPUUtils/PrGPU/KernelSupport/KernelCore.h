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


#ifndef PRGPU_KERNELSUPPORT_KERNELCORE_H
#	define PRGPU_KERNELSUPPORT_KERNELCORE_H

/**
**	Device target. Use these defines to #if/#endif API specific code
**  For OpenCL or Metal, you'll need to define the corresponding 
**  GF_DEVICE_TARGET_* macro before including this file
*/
#if defined(__CUDACC__)
#	define GF_DEVICE_TARGET_CUDA 1
#	define GF_DEVICE_TARGET_OPENCL 0
#	define GF_DEVICE_TARGET_HOST 0
#	define GF_DEVICE_TARGET_METAL	 0
#	define GF_DEVICE_TARGET_DEVICE 1

#elif defined(GF_DEVICE_TARGET_OPENCL)
#	define GF_DEVICE_TARGET_CUDA 0
#	define GF_DEVICE_TARGET_HOST 0
#	define GF_DEVICE_TARGET_METAL 0
#	define GF_DEVICE_TARGET_DEVICE 1

#elif defined(GF_DEVICE_TARGET_METAL)
#	define GF_DEVICE_TARGET_CUDA 0
#	define GF_DEVICE_TARGET_OPENCL 0
#	define GF_DEVICE_TARGET_HOST 0
#	define GF_DEVICE_TARGET_DEVICE 1

#else
#	define GF_DEVICE_TARGET_CUDA 0
#	define GF_DEVICE_TARGET_OPENCL 0
#	define GF_DEVICE_TARGET_METAL 0
#	define GF_DEVICE_TARGET_HOST 1
#	define GF_DEVICE_TARGET_DEVICE 0
#endif

#if GF_DEVICE_TARGET_METAL
//#	include <metal_common>
//#	include <metal_compute>
//#	include <metal_integer>
//#	include <metal_texture>
using namespace metal;
#endif

/**
** Device sections - one-liner target dependent code insertions
** Example:
**		GF_CUDA_SECTION(width=16;)
** The section macros have a hidden superpower: they can defer processing of preprocessor directives until second time the text is preprocessed.
** Example:
**		GF_OPENCL_SECTION(#pragma unroll 32)
** This is useful for when source code is preprocessed twice, once on the host during host compilation, and then again at runtime in the device compiler.
** This allows us to filter out non-kernel code from the source sent to the device compiler and pull in the required includes, while still keeping line numbers correct.
** Line numbers are important for locating compiler messages in the kernel source and for debugging tools to work correctly.
** Double preprocessing means special treatment is needed for preprocessor directives aimed at the device, such as "pragma"
**
** Don't use section macros for more than one liners or structures. Line number tracking doesn't work within them.
*/
#ifndef GF_HOST_SECTION
#	if GF_DEVICE_TARGET_HOST
#		define GF_HOST_SECTION(...) __VA_ARGS__ 
#		define GF_SHARED_SECTION(NS, ...) namespace NS { __VA_ARGS__ }
#		define GF_DEVICE_SECTION(...)
#	else
#		define GF_HOST_SECTION(...)
#		define GF_SHARED_SECTION(NS, ...) __VA_ARGS__
#		define GF_DEVICE_SECTION(...) __VA_ARGS__
#	endif

#	if GF_DEVICE_TARGET_CUDA
#		define GF_CUDA_SECTION(...) __VA_ARGS__
#	else
#		define GF_CUDA_SECTION(...)
#	endif

#	if GF_DEVICE_TARGET_OPENCL
#		define GF_OPENCL_SECTION(...) __VA_ARGS__
#	else
#		define GF_OPENCL_SECTION(...)
#	endif

#	if GF_DEVICE_TARGET_METAL
#		define GF_METAL_SECTION(...)  __VA_ARGS__
#	else
#		define GF_METAL_SECTION(...)
#	endif
#endif

/*
** The GF_KERNEL_FUNCTION macro is used to declare a kernel and pass values as parameters (OpenCL/CUDA) or in a struct (metal).
** The macro will create an API-specific kernel entry point which will call a device function that it defines, leaving you to fill in the body.
** The macro uses Boost preprocessor sequences to express a type/name pair: 
**		(float)(inValue)
** These pairs are then nested into a sequence of parameters:  
**		((float)(inAge))((int)(inMarbles))
** There are different categories of parameters, such as buffers, values, and kernel position.
** Each category sequence is a separate macro parameter.
** Example usage:
**	GF_KERNEL_FUNCTION(RemoveFlicker,				//kernel name, then comma,
**      ((GF_PTR(float4))(inSrc))					//all buffers and textures go after the first comma
**		((GF_PTR(float4))(outDest)),
**		((int)(inDestPitch))						//After the second comma, all values to be passed
**		((DevicePixelFormat)(inDeviceFormat))
**		((int)(inWidth))
**		((int)(inHeight)),
**		((uint2)(inXY)(KERNEL_XY))					//After the third comma, the position arguments.
**		((uint2)(inBlockID)(BLOCK_ID)))
**	{
**		<do something interesting here>
**	}
** In the example above, the host does not pass the position values when invoking the kernel. 
** Position values are filled in automatically by the unmarshalling code.
** See the example plugins for usage.
*/
#define GF_KERNEL_FUNCTION(name, buffers, values, positions) \
	GF_UNMARSHAL(name, buffers, values, positions, BOOST_PP_NIL)

/**
** Declare kernels that use shared memory slightly differently, depending in whether it is statically or dynamically sized.
** Statically sized shared memory:
**  GF_KERNEL_FUNCTION_SHARED is used for kernels that need statically sized shared memory.
**  The GF_KERNEL_FUNCTION_SHARED macro takes a fifth argument that is a sequence:
**		((sharedMemDeclarationInKernel)(sharedMemForFunctionSignature)(argumentToPassToFunction))
**  In practice, it looks something like:
**		((GF_SHARED(float) inMoo[42])(GF_SHARED_PTR(float)inMoo)(&inMoo[0]))
**  or
**		((GF_SHARED(float4) block2D[17][18])(GF_SHARED_PTR_QUALIFIER float4 (*block2D)[18])(&block2D[0])))
**  Shared memory parameters to functions must be pointers in OpenCL, so arrays must be passed by pointer.
**
** Dynamically sized shared memory:
**  Use GF_KERNEL_FUNCTION, not GF_KERNEL_FUNCTION_SHARED.
**  Dynamic shared memory is a setting on the kernel for CUDA, and passed through the parameter list for non-CUDA.
**  To support both, there's an overlap in the APIs below.
**  Host:
**		Declare kernel parameter as shared dynamic. (This parameter will be skipped for CUDA):
**  		GF_NONCUDA_SHARED_DYNAMIC(float,smem),
**		Before calling kernel, set shared size (for Cuda):
**			cuFuncSetSharedSize((CUfunction)inKernel, inSize);
**		Call kernel, passing size of shared buffer in parameter list (skip parameter for CUDA, due to the declaration above):
**  Kernel:
**		Declare parameter in buffer section of GF_KERNEL_FUNCTION parameter list (no-op for CUDA):
**			GF_NONCUDA_SHARED_DYNAMIC(float,smem)
**		Declare external pointer in body (no-op for non-CUDA)
**			GF_CUDA_SHARED_DYNAMIC(float, smem);
**	 A kernel may have at most one dynamically shared section.
*/
#define GF_KERNEL_FUNCTION_SHARED(name, buffers, values, positions, shares) \
	GF_UNMARSHAL(name, buffers, values, positions, BOOST_PP_SEQ_TO_LIST(shares))

/**
**	Device function declaration.
**		GF_DEVICE_FUNCTION float Average(float a, float b) {...
*/
#ifdef __NVCC__  /* CUDA compiler for device or host */
#	define GF_DEVICE_FUNCTION __inline__ __device__

#elif GF_DEVICE_TARGET_HOST
#	define GF_DEVICE_FUNCTION __inline__

#elif GF_DEVICE_TARGET_OPENCL
#	define GF_DEVICE_FUNCTION static __inline__

#elif GF_DEVICE_TARGET_METAL
#	define GF_DEVICE_FUNCTION
#endif

/*
**	GF_HOST_FUNCTION declaration is mostly useful (along with GF_DEVICE_TARGET_* and __NVCC__) 
**	for CUDA style programming that mixes host and device code in the same file.
*/
#ifdef __NVCC__
#	define GF_HOST_FUNCTION __host__

#else
#	define GF_HOST_FUNCTION
#endif

/**
**	Memory - pointers and alignment
**
**  GF_PTR declares a pointer to global memory:
**		GF_PTR(float4) myImage;
**
**	GF_THREAD_PTR declares a pointer to memory shared between threads in a block:
**		GF_THREAD_PTR(float) mySharedMemoryPtr;
**
**  GF_ALIGN structure alignment example:
**		struct GF_ALIGN(8)
**		{
**			float a, b;
**		}
*/
#if GF_DEVICE_TARGET_CUDA || GF_DEVICE_TARGET_HOST
#	define GF_PTR(Type) Type*
#	define GF_THREAD_PTR(Type) Type*
#	define GF_ALIGN(inSize) __align__(inSize)

#elif GF_DEVICE_TARGET_OPENCL
#	define GF_PTR(Type) __global Type*
#	define GF_THREAD_PTR(Type) Type*
#	define GF_ALIGN(inSize) __attribute__ ((aligned (inSize)))

#elif GF_DEVICE_TARGET_METAL
#	define GF_PTR(Type) device Type*
#	define GF_THREAD_PTR(Type) Type thread *
#	define GF_ALIGN(inSize) alignas(inSize)
#endif

/**
**	Unchanging values declared in global scope on GPU
**	Example:
**		GF_CONSTANT(float) kConversionFactors[9] = ...
**	This may be accessed as an array:
**		x = y * kConversionFactors[9];
** 	It has a special pointer type using GF_STATIC_CONSTANT_PTR
*/
#ifdef __NVCC__
#	define GF_CONSTANT(Type) __constant__ Type
#	define GF_STATIC_CONSTANT_PTR(Type) GF_CONSTANT_PTR(Type)

#elif GF_DEVICE_TARGET_HOST
#	define GF_CONSTANT(Type) Type

#elif GF_DEVICE_TARGET_OPENCL
#	define GF_CONSTANT(Type) __constant Type
#	define GF_STATIC_CONSTANT_PTR(Type) GF_CONSTANT(Type*)

#elif GF_DEVICE_TARGET_METAL
#	define GF_CONSTANT(Type) constant Type
#	define GF_STATIC_CONSTANT_PTR(Type) GF_CONSTANT_PTR(Type)
#endif

/**
**	Uniform buffer - constant for a given kernel launch
**	The compute APIs treat these differently. CUDA passes these as a global, while the others receive them as a buffer.
**  Portable kernel code needs to set up for both, and the macros will stub out the inappropriate path
**  The kernel file declares a global:
**		GF_CUDA_CONSTANT(Curve, inCurve)
**  It also declares the buffer as a kernel buffer parameter:
**		GF_KERNEL_FUNCTION(CurveKernel,
**			((const GF_PTR(float4))(inImage))
**			((GF_NONCUDA_CONSTANT_TYPE(Curve))(GF_NONCUDA_CONSTANT_NAME(inCurve))), ...
**	The body of the function accesses it using GF_GET_CONSTANT, which returns a GF_pTR:
**		GF_PTR(Curve const) curveBuffer = GF_GET_CONSTANT(inCurve);
**	If the host is using CUDA, it uses cuModuleGetGlobal and friends to set up the constant, 
**	and passes NULL as the kernel buffer parameter.
**	For other APIs, pass the the buffer as the kernel buffer parameter.
*/
#if GF_DEVICE_TARGET_CUDA || GF_DEVICE_TARGET_HOST
#	ifdef __NVCC__
#		define GF_CUDA_CONSTANT(Type, Name) __device__ GF_CONSTANT(Type) Name;
#	else
#		define GF_CUDA_CONSTANT(Type, Name) GF_CONSTANT(Type) Name;
#	endif
#   define GF_NONCUDA_CONSTANT_TYPE(Type) const void*
#   define GF_NONCUDA_CONSTANT_NAME(Name) Constant##Name
#	define GF_GET_CONSTANT(inDeclarator) (&inDeclarator)
#	define GF_CONSTANT_PTR(Type) const Type*

#elif GF_DEVICE_TARGET_OPENCL
#	define GF_CUDA_CONSTANT(Type, Name)
#   define GF_NONCUDA_CONSTANT_TYPE(Type) const GF_PTR(Type)
#   define GF_NONCUDA_CONSTANT_NAME(Name) (Name)
#	define GF_GET_CONSTANT(inDeclarator) inDeclarator
#	define GF_CONSTANT_PTR(Type) GF_PTR(Type)

#elif GF_DEVICE_TARGET_METAL
#	define GF_CUDA_CONSTANT(Type, Name)
#   define GF_NONCUDA_CONSTANT_TYPE(Type) const GF_PTR(Type)
#   define GF_NONCUDA_CONSTANT_NAME(Name) (Name)
#	define GF_GET_CONSTANT(inDeclarator) inDeclarator
#	define GF_CONSTANT_PTR(Type) const constant Type*
#endif

/**
**	Memory shared within a thread group.
**  See comments above GF_KERNEL_FUNCTION_SHARED for usage examples
*/
#if GF_DEVICE_TARGET_CUDA || GF_DEVICE_TARGET_HOST
#	ifdef __NVCC__
#		define GF_SHARED(Type) __shared__ Type
#	else
#		define GF_SHARED(Type) Type
#	endif
#   define GF_SHARED_PTR(Type) Type*
#	define GF_SHARED_PTR_QUALIFIER
#	define GF_CUDA_SHARED_DYNAMIC(Type, Name) extern GF_SHARED(float) Name[]
#	if GF_DEVICE_TARGET_CUDA
#		define GF_NONCUDA_SHARED_DYNAMIC(Type, Name)
#	endif

#elif GF_DEVICE_TARGET_OPENCL
#	define GF_SHARED(Type) __local Type
#	define GF_SHARED_PTR(Type) __local Type*
#	define GF_SHARED_PTR_QUALIFIER __local
#	define GF_CUDA_SHARED_DYNAMIC(Type, Name)
#	define GF_NONCUDA_SHARED_DYNAMIC(Type, Name) ((GF_SHARED_PTR(Type))(Name))

#elif GF_DEVICE_TARGET_METAL
#	define GF_SHARED(Type) threadgroup Type
#	define GF_SHARED_PTR(Type) threadgroup Type*
#	define GF_SHARED_PTR_QUALIFIER threadgroup
#	define GF_CUDA_SHARED_DYNAMIC(Type, Name)
#	define GF_NONCUDA_SHARED_DYNAMIC(Type, Name) ((GF_SHARED_PTR(Type))(Name))
#endif


/**
**	Optimized 2D array access
*/
#if GF_DEVICE_TARGET_CUDA || GF_DEVICE_TARGET_HOST
#	define GF_FASTMULTIPLY(inX, inY) ((inX)*(inY))
#	define GF_READ2D(inPtr, inRowStride, inX, inY) (inPtr)[GF_FASTMULTIPLY((inY), (inRowStride)) + (inX)]
#	define GF_WRITE2D(inValue, outPtr, inRowStride, inX, inY) (outPtr)[GF_FASTMULTIPLY((inY), (inRowStride)) + (inX)] = (inValue)

#elif GF_DEVICE_TARGET_OPENCL
#	define GF_FASTMULTIPLY(inX, inY) mul24((inX), (inY))
#	define GF_READ2D(inPtr, inRowStride, inX, inY) (inPtr)[GF_FASTMULTIPLY((inY), (inRowStride)) + (inX)]
#	define GF_WRITE2D(inValue, outPtr, inRowStride, inX, inY) (outPtr)[GF_FASTMULTIPLY(inY, inRowStride) + (inX)] = (inValue)

#elif GF_DEVICE_TARGET_METAL
#	define GF_FASTMULTIPLY(inX, inY) ((inX)*(inY))
#	define GF_READ2D(inPtr, inRowStride, inX, inY) (inPtr)[GF_FASTMULTIPLY((inY), (inRowStride)) + (inX)]
#	define GF_WRITE2D(inValue, outPtr, inRowStride, inX, inY) (outPtr)[GF_FASTMULTIPLY((inY), (inRowStride)) + (inX)] = (inValue)
#endif


/**
**	Textures
**  Textures have different usage patterns in the different APIs, so we need to follow both patterns and let the macros stub the unneeded path for the current API.
**  A typical texturing kernel file uses this pattern:
**		Declare the texture at global scope:
**			GF_TEXTURE_GLOBAL(float4, inSrcTexture, GF_DOMAIN_NATURAL, GF_RANGE_NATURAL_CUDA, GF_EDGE_CLAMP, GF_FILTER_LINEAR)
**		Declare the texture as a kernel parameter:
**			GF_KERNEL_FUNCTION(ApplyCowSpotsKernel,
**				((GF_TEXTURE_TYPE(float))(GF_TEXTURE_NAME(inSrcTexture)))...
**		Sample the texture inside the body of the kernel:
**			float4 color = GF_READTEXTUREPIXEL(inSrcTexture, srcX + 0.5, srcY + 0.5);
**	The macros:
**		GF_TEXTURE_GLOBAL(
**			PixelType,	//The type of the pixel, e.g. float4 
**			Name,		//A name for the texture. Must match that used in GF_TEXTURE_NAME
**			inDomain,	//The position type used - GF_DOMAIN_UNIT for 0-1, GF_DOMAIN_NATURAL for 0-width (or height)
**			inRange,	//The output range for integer textures - GF_RANGE_UNIT for 0-1
**			inEdge,		//The edge treatment such as GF_EDGE_CLAMP or GF_EDGE_BORDER
**			inFilter)	//The interpolation filter such as GF_FILTER_NEAREST or GF_FILTER_LINEAR
**		For CUDA, these properties are set by the host, and the GF_TEXTURE_GLOBAL properties are ignored. 
**		You'll want to ensure the two sets match, so that your results match across APIs!
** 
**		GF_TEXTURE_TYPE(ChannelType) - the type of a single channel in the texture. This can be used in declaring kernels and device functions.
**
**		GF_TEXTURE_NAME(Name) - the name must match that used in GF_TEXTURE_GLOBAL
**		
**		GF_READTEXTURE(Name, inX, inY) - sample texture of given name at given position. Name must match that used in GF_TEXTURE_GLOBAL.
*/
#if GF_DEVICE_TARGET_CUDA || GF_DEVICE_TARGET_HOST
#	define GF_TEXTURE_TYPE(ChannelType) void *
#	define GF_TEXTURE_NAME(Name) Texture##Name
#	define GF_GET_TEXTURE(Name) 0
#	define GF_READTEXTURE(Name, inX, inY) tex2D(Name, (inX), (inY))
#	define GF_TEXTURE_GLOBAL(PixelType, Name, inDomain, inRange, inEdge, inFilter) static texture<PixelType, cudaTextureType2D, inRange> Name;

#	define GF_DOMAIN_NATURAL							 // positions in pixels
#	define GF_DOMAIN_UNIT							     // position normalized

#	define GF_RANGE_NATURAL_CUDA cudaReadModeElementType // unnormalized result (for CUDA only)
#	define GF_RANGE_UNIT cudaReadModeNormalizedFloat	 // non-float texture values normalized

#	define GF_EDGE_CLAMP								 // clamp to edge pixel
#	define GF_EDGE_BORDER								 // clamp to border value (black)

#	define GF_FILTER_NEAREST
#	define GF_FILTER_LINEAR

#elif GF_DEVICE_TARGET_OPENCL
#	define GF_TEXTURE_TYPE(ChannelType) __read_only image2d_t
#	define GF_TEXTURE_NAME(Name) Name
#	define GF_GET_TEXTURE(Name) Name
#	define GF_READTEXTURE(Name, inX, inY) read_imagef(Name, kSampler_##Name, (float2)((inX), (inY)))
#	define GF_TEXTURE_GLOBAL(PixelType, Name, inDomain, inRange, inEdge, inFilter)	\
		constant sampler_t kSampler_##Name = inDomain | inEdge | inFilter;

#	define GF_DOMAIN_NATURAL		CLK_NORMALIZED_COORDS_FALSE
#	define GF_DOMAIN_UNIT			CLK_NORMALIZED_COORDS_TRUE

#	define GF_RANGE_NATURAL_CUDA	GF_RANGE_UNIT
#	define GF_RANGE_UNIT			// non-float texture values normalized

#	define GF_EDGE_CLAMP			CLK_ADDRESS_CLAMP_TO_EDGE
#	define GF_EDGE_BORDER			CLK_ADDRESS_CLAMP

#	define GF_FILTER_NEAREST		CLK_FILTER_NEAREST
#	define GF_FILTER_LINEAR			CLK_FILTER_LINEAR

#elif GF_DEVICE_TARGET_METAL
template<typename ChannelType> using GFMetalTextureType = texture2d<ChannelType, access::sample>;
#	define GF_TEXTURE_TYPE(ChannelType) GFMetalTextureType<ChannelType> //boost preprocessor sequences don't like commas
#	define GF_TEXTURE_NAME(Name) Name
#	define GF_GET_TEXTURE(Name) Name
#	define GF_READTEXTURE(Name, inX, inY) Name.sample(kSampler_##Name, make_float2((inX),(inY)))
#	define GF_TEXTURE_GLOBAL(PixelType, Name, inDomain, inRange, inEdge, inFilter)	\
		constexpr sampler kSampler_##Name(inDomain, s_address::inEdge, t_address::inEdge, inFilter);

#	define GF_DOMAIN_NATURAL	coord::pixel
#	define GF_DOMAIN_UNIT		coord::normalized

#	define GF_RANGE_NATURAL		GF_RANGE_UNIT
#	define GF_RANGE_UNIT		// non-float texture values normalized

#	define GF_EDGE_CLAMP		clamp_to_edge
#	define GF_EDGE_BORDER		clamp_to_zero

#	define GF_FILTER_NEAREST	filter::nearest
#	define GF_FILTER_LINEAR		filter::linear
#endif

/**
**	Vector constructors
*/
#if GF_DEVICE_TARGET_OPENCL
#	ifndef make_int2
#		define make_int2(inX, inY) (int2)(inX, inY)
#	endif
#	ifndef make_uint2
#		define make_uint2(inX, inY) (uint2)(inX, inY)
#	endif
#	ifndef make_float4
#		define make_float4(inX, inY, inZ, inW) (float4)(inX, inY, inZ, inW)
#	endif
#	ifndef make_float3
#		define make_float3(inX, inY, inZ) (float3)(inX, inY, inZ)
#	endif
#	ifndef make_float2
#		define make_float2(inX, inY) (float2)(inX, inY)
#	endif
#	ifndef make_uchar4
#		define make_uchar4(inX, inY, inZ, inW) (uchar4)(inX, inY, inZ, inW)
#	endif
#	ifndef make_ushort4
#		define make_ushort4(inX, inY, inZ, inW) (ushort4)(inX, inY, inZ, inW)
#	endif
#	ifndef make_uint4
#		define make_uint4(inX, inY, inZ, inW) (uint4)(inX, inY, inZ, inW)
#	endif

#elif GF_DEVICE_TARGET_METAL
#	define make_int2(inX, inY) (int2)(inX, inY)
#	define make_float4(inX, inY, inZ, inW)  (float4)(static_cast<float>(inX), static_cast<float>(inY), static_cast<float>(inZ), static_cast<float>(inW))
#	define make_float3(inX, inY, inZ)       (float3)(static_cast<float>(inX), static_cast<float>(inY), static_cast<float>(inZ))
#	define make_float2(inX, inY)            (float2)(static_cast<float>(inX), static_cast<float>(inY))
#	define make_half4(inX, inY, inZ, inW)   (half4)(static_cast<half>(inX), static_cast<half>(inY), static_cast<half>(inZ), static_cast<half>(inW))
#	define make_half2(inX, inY)		        (half2)(static_cast<half>(inX), static_cast<half>(inY))
#	define make_uchar4(inX, inY, inZ, inW)  (uchar4)(inX, inY, inZ, inW)
#	define make_ushort4(inX, inY, inZ, inW) (ushort4)(inX, inY, inZ, inW)
#	define make_uint2(inX, inY)             (uint2)(inX, inY)
#	define make_uint4(inX, inY, inZ, inW)   (uint4)(inX, inY, inZ, inW)
#endif

/**
** Memory transaction synchronization within thread group
**
** SyncThreadsShared = block until all thread block transactions with shared memory are complete
** SyncThreadsDevice = block until all thread block transactions with device memory are complete
** SyncThreads       = block until all thread block transactions with shared memory and device memory are complete
*/
#if GF_DEVICE_TARGET_CUDA
GF_DEVICE_FUNCTION void SyncThreads()
{
	__syncthreads();
}
#	define SyncThreadsShared()			SyncThreads();
#	define SyncThreadsDevice()			SyncThreads();

#elif GF_DEVICE_TARGET_OPENCL
GF_DEVICE_FUNCTION void SyncThreads()
{
	barrier(CLK_LOCAL_MEM_FENCE);
}
#	define SyncThreadsShared()			SyncThreads();
#	define SyncThreadsDevice()			SyncThreads();

#elif GF_DEVICE_TARGET_METAL
GF_DEVICE_FUNCTION void SyncThreads()
{
	threadgroup_barrier(metal::mem_flags::mem_device_and_threadgroup);
}

GF_DEVICE_FUNCTION void SyncThreadsShared()
{
	threadgroup_barrier(metal::mem_flags::mem_threadgroup);
}

GF_DEVICE_FUNCTION void SyncThreadsDevice()
{
	threadgroup_barrier(metal::mem_flags::mem_device);
}
#endif

/**
**	various ported functions
*/
#if GF_DEVICE_TARGET_CUDA
template<
	typename Type>
	GF_DEVICE_FUNCTION Type clamp(Type inX, Type inY, Type inZ)
{
	return min(max(inX, inY), inZ);
}
#endif

#if GF_DEVICE_TARGET_OPENCL
GF_DEVICE_FUNCTION float saturate(float inX)
{
	return clamp(inX, 0.0f, 1.0f);
}
#endif


#include "PrGPU/KernelSupport/KernelWrapper.h"	//depends on some of the above #defines

#endif
