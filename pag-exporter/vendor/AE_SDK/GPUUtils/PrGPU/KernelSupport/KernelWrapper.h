/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2016 Adobe Systems Incorporated                       */
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

#ifndef GFKERNELSUPPORT_KERNELWRAPPER_H
#	define GFKERNELSUPPORT_KERNELWRAPPER_H

#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/to_list.hpp>

/*
** Implementation detail for kernel wrappers
*/

//Operators used by boost Preprocessor metaprogramming sequence operations
//((a)(b)) -> (b)
#define OP_NTH(r, nth, elem) (BOOST_PP_SEQ_ELEM(nth,elem))
//((a)(b)) -> (data b)
#define OP_SECOND_PREFIX(r, data, elem) (data BOOST_PP_SEQ_ELEM(1,elem))
//((a)(b)) -> a,b data
#define OP_PAIR(r, data, elem) BOOST_PP_SEQ_ELEM(0, elem) BOOST_PP_SEQ_ELEM(1, elem) data
//((a)(b)) -> a b
#define OP_PAIR2(r, data, elem) (OP_PAIR(r, , elem))
//((a)(b)(c)) -> a b c
#define OP_TRIAD(r, data, elem) (BOOST_PP_SEQ_ELEM(0, elem) BOOST_PP_SEQ_ELEM(1, elem) BOOST_PP_SEQ_ELEM(2, elem))
//((a)(b)(c)) -> a;
#define OP_1st_SEMI(r, data, elem) BOOST_PP_SEQ_ELEM(0, elem);
//((a)(b)(c)) -> b,
#define OP_NTH_COMMA(r, nth, elem) BOOST_PP_COMMA() BOOST_PP_SEQ_ELEM(nth, elem)

//Transformations for boost preprocessing sequences
//((a)(b))((c)(d)) -> a b inSep c d inSep
#define SEQ_PAIRS(inSeq, inSep) BOOST_PP_SEQ_FOR_EACH(OP_PAIR, inSep, inSeq)
//((a)(b))((c)(d)) -> a b, c d
#define SEQ_PAIRS_ENUM(inSeq) BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(OP_PAIR2, ., inSeq))
//((a)(b)(c))((d)(e)(f)) -> a b c, d e f
#define SEQ_TRIADS_ENUM(inSeq) BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(OP_TRIAD, ., inSeq))
//((a)(b))((c)(d)) -> c, d
#define SEQ_NTH(nth, inSeq) BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(OP_NTH, nth, inSeq))
//((a)(b))((c)(d)) -> inPrefix c, inPrefix d
#define SEQ_SECONDS_PREFIX(inSeq, inPrefix) BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(OP_SECOND_PREFIX, inPrefix,inSeq))

#define KERNEL_DECLARE_SHARES(inSeq) BOOST_PP_LIST_FOR_EACH(OP_1st_SEMI, , inSeq)
#define FUNCTION_DECLARE_SHARES(inSeq) BOOST_PP_LIST_FOR_EACH(OP_NTH_COMMA, 1, inSeq)
#define PASS_SHARES(inSeq) BOOST_PP_LIST_FOR_EACH(OP_NTH_COMMA, 2, inSeq)

#define KERNEL_WRAPPER_CAT(a,b) a##b
#define KERNEL_DELEGATE_NAME(name) KERNEL_WRAPPER_CAT(name,_Delegate)

#define GF_SIGNATURE(name, buffers, values, positions, shares)  \
    KERNEL_DELEGATE_NAME(name)(						\
		SEQ_PAIRS_ENUM(buffers),					\
        SEQ_PAIRS_ENUM(values),						\
        SEQ_PAIRS_ENUM(positions)					\
		FUNCTION_DECLARE_SHARES(shares))

#define GF_UNMARSHAL_USING_STRUCT(name, buffers, values, positions, shares)				\
    GF_DEVICE_FUNCTION void GF_SIGNATURE(name, buffers, values, positions, shares);		\
																						\
    typedef struct { SEQ_PAIRS(values, ;)} KERNEL_WRAPPER_CAT(name,Values);				\
																						\
	GF_KERNEL name(																		\
		SEQ_PAIRS_ENUM(buffers),														\
		device KERNEL_WRAPPER_CAT(name,Values) *inValues,								\
		SEQ_TRIADS_ENUM(positions))					\
	{												\
		KERNEL_DECLARE_SHARES(shares)				\
													\
		KERNEL_DELEGATE_NAME(name)(					\
			SEQ_NTH(1, buffers),					\
			SEQ_SECONDS_PREFIX(values, inValues->),	\
			SEQ_NTH(1, positions)					\
			PASS_SHARES(shares));					\
	}												\
													\
    GF_DEVICE_FUNCTION void GF_SIGNATURE(name, buffers, values, positions, shares)

#define GF_UNMARSHAL_USING_PARAMETERS(name, buffers, values, positions, shares)			\
    GF_DEVICE_FUNCTION void GF_SIGNATURE(name, buffers, values, positions, shares);		\
											\
	GF_KERNEL name(							\
        SEQ_PAIRS_ENUM(buffers),			\
        SEQ_PAIRS_ENUM(values))				\
	{										\
		KERNEL_DECLARE_SHARES(shares)		\
											\
        KERNEL_DELEGATE_NAME(name)(			\
            SEQ_NTH(1, buffers),			\
            SEQ_NTH(1, values),				\
            SEQ_NTH(2, positions)			\
			PASS_SHARES(shares));			\
	}										\
											\
    GF_DEVICE_FUNCTION void GF_SIGNATURE(name, buffers, values, positions, shares)

#if GF_DEVICE_TARGET_METAL
#	define GF_UNMARSHAL GF_UNMARSHAL_USING_STRUCT
#else
#	define GF_UNMARSHAL GF_UNMARSHAL_USING_PARAMETERS
#endif

/**
** Kernel identity 
*/
#if GF_DEVICE_TARGET_METAL
#	define KERNEL_XY	[[ thread_position_in_grid ]]
#	define THREAD_ID	[[ thread_position_in_threadgroup ]]
#	define BLOCK_ID 	[[ threadgroup_position_in_grid ]]
#	define BLOCK_SIZE 	[[ threads_per_threadgroup ]]
#else
#	define KERNEL_XY	KernelXYUnsigned()
#	define THREAD_ID	ThreadIDUnsigned()
#	define BLOCK_ID		BlockIDUnsigned()
#	define BLOCK_SIZE	BlockSizeUnsigned()

#	if GF_DEVICE_TARGET_CUDA
		GF_DEVICE_FUNCTION uint2 KernelXYUnsigned()
		{
			return make_uint2(
				GF_FASTMULTIPLY(blockIdx.x, blockDim.x) + threadIdx.x,
				GF_FASTMULTIPLY(blockIdx.y, blockDim.y) + threadIdx.y);
		}

		GF_DEVICE_FUNCTION uint2 ThreadIDUnsigned()
		{
			return make_uint2(threadIdx.x, threadIdx.y);
		}

		GF_DEVICE_FUNCTION uint2 BlockIDUnsigned()
		{
			return make_uint2(blockIdx.x, blockIdx.y);
		}

		GF_DEVICE_FUNCTION uint2 BlockSizeUnsigned()
		{
			return make_uint2(blockDim.x, blockDim.y);
		}

#	elif GF_DEVICE_TARGET_OPENCL
		GF_DEVICE_FUNCTION uint2 KernelXYUnsigned()
		{
			return make_uint2(get_global_id(0), get_global_id(1));
		}

		GF_DEVICE_FUNCTION uint2 ThreadIDUnsigned()
		{
			return make_uint2(get_local_id(0), get_local_id(1));
		}

		GF_DEVICE_FUNCTION uint2 BlockIDUnsigned()
		{
			return make_uint2(get_group_id(0), get_group_id(1));
		}

		GF_DEVICE_FUNCTION uint2 BlockSizeUnsigned()
		{
			return make_uint2(get_local_size(0), get_local_size(1));
		}
#	endif
#endif	

/* 
** Kernel entry keyword abstraction
*/
#ifdef __NVCC__
#	define GF_KERNEL extern "C" __global__ void

#elif GF_DEVICE_TARGET_HOST
#	define GF_KERNEL extern "C" void

#elif GF_DEVICE_TARGET_OPENCL
#	define GF_KERNEL __kernel void

#elif GF_DEVICE_TARGET_METAL
#	define GF_KERNEL kernel void
#endif

#endif
