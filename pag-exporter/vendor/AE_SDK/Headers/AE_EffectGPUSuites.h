/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2018 Adobe Systems Incorporated                       */
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

#ifndef _H_AE_EffectGPUSuites
#define _H_AE_EffectGPUSuites

#include <AE_Effect.h>
#include <SPBasic.h>

#include <adobesdk/config/PreConfig.h>

#ifdef __cplusplus
	extern "C" {
#endif


#define PF_CUDAVersion						10010


#define kPFGPUDeviceSuite			"PF GPU Device Suite"
#define kPFGPUDeviceSuiteVersion1	1	/* frozen in AE 16.0 */	

typedef struct
{
	PF_GPU_Framework device_framework;
	PF_Boolean compatibleB;	// device meets minimum requriement for acceleration
	
	void* platformPV; // cl_platform_id
	void* devicePV; // CUdevice or cl_device_id or MTLDevice
	void* contextPV; // CUcontext or cl_context
	void* command_queuePV; // CUstream or cl_command_queue or MTLCommandQueue
	void* offscreen_opengl_contextPV; // CGLContextObj or HGLRC - only available on the primary device
	void* offscreen_opengl_devicePV; // HDC - only available on the primary device

} PF_GPUDeviceInfo;



typedef struct PF_GPUDeviceSuite1 {

	/**
	**	This will return the number of gpu devices the host supports.
	**
	**	@param	effect_ref								Comes with PF_InData. 
	**	@param	device_countP							Return number of devices available.
	*/
	SPAPI PF_Err	(*GetDeviceCount)( 					PF_ProgPtr			effect_ref,
														A_u_long			*device_countP);	/* << */

	/**
	**	This will return the device info with given device index, which includes necessary context/queue information
	**	needed to dispatch task to the device. Refer PF_GPUDeviceInfo for details.
	**
	**	@param	effect_ref								Comes with PF_InData. 
	**	@param	device_index							The device index for the requested device.
	**	@param	PF_GPUDeviceInfo						The device info will to be filled.
	*/
	SPAPI PF_Err	(*GetDeviceInfo)( 					PF_ProgPtr			effect_ref,
														A_u_long			device_index,
									  					PF_GPUDeviceInfo	*device_infoP);	/* << */


	/**
	**	Acquire/release exclusive access to inDeviceIndex. All calls below this point generally require access be held.
	**	For full GPU plugins (those that use a separate entry point for GPU rendering) exclusive access is always held.
	**	These calls do not need to be made in that case.
	**		For CUDA calls cuCtxPushCurrent/cuCtxPopCurrent on the current thread to manage the devices context.
	*/
	SPAPI PF_Err	(*AcquireExclusiveDeviceAccess)( 	PF_ProgPtr			effect_ref,
														A_u_long			device_index);

	SPAPI PF_Err	(*ReleaseExclusiveDeviceAccess)( 	PF_ProgPtr			effect_ref,
														A_u_long			device_index);

	/**
	**	All device memory must be allocated through this suite.
	**		Purge should be called only in emergency situations when working with GPU memory
	**			that cannot be allocated through this suite (eg OpenGL memory).
	**		Returned pointer value represents memory allocated through cuMemAlloc or clCreateBuffer.
	*/

	SPAPI PF_Err	(*AllocateDeviceMemory)(	PF_ProgPtr			effect_ref,
	 											A_u_long			device_index,
											 	size_t				size_bytes,
												void 				**memoryPP);	/* << */

	SPAPI PF_Err	(*FreeDeviceMemory)( 		PF_ProgPtr			effect_ref,
												A_u_long			device_index,
										 		void         	  	*memoryP);

	SPAPI PF_Err	(*PurgeDeviceMemory)( 		PF_ProgPtr			effect_ref,
												A_u_long			device_index,
										  		size_t				size_bytes,
										 	 	size_t 				*bytes_purgedP0);	/* << */

	/**
	**	All host (pinned) memory must be allocated through this suite.
	**		Purge should be called only in emergency situations when working with GPU memory
	**			that cannot be allocated through this suite (eg OpenGL memory).
	**		Returned pointer value represents memory allocated through cuMemHostAlloc or malloc.
	*/
	SPAPI PF_Err	(*AllocateHostMemory)(	PF_ProgPtr			effect_ref,
											A_u_long			device_index,
											size_t				size_bytes,
											void 				**memoryPP);	/* << */


	SPAPI PF_Err	(*FreeHostMemory)( 		PF_ProgPtr			effect_ref,
											A_u_long			device_index,
											void				*memoryP);


	SPAPI PF_Err	(*PurgeHostMemory)( 	PF_ProgPtr			effect_ref,	
											A_u_long			device_index,
											size_t				bytes_to_purge,
											size_t 				*bytes_purgedP0);	/* << */

	/**
	**	This will allocate a gpu effect world. Caller is responsible for deallocating the buffer with
	**	PF_GPUDeviceSuite1::DisposeGPUWorld.
	**
	**	@param	effect_ref					Comes with PF_InData. 
	**	@param	device_index				The device you want your gpu effect world allocated with.
	**	@param	width						Width of the effect world.
	**	@param	height						Height of the effect world.
	**	@param	pixel_aspect_ratio			Pixel Aspect Ratio of the effect world.
	**	@param	field_type					The field of the effect world.
	**	@param	pixel_format				The pixel format of the effect world, only gpu formats are accepted.
	**	@param	clear_pixB					Pass in 'true' for a transparent black frame.
	**	@param	worldPP						The handle to the effect world to be created.
	*/
	SPAPI PF_Err	(*CreateGPUWorld)(		PF_ProgPtr			effect_ref,
											A_u_long			device_index,
											A_long				width,
											A_long				height,
											PF_RationalScale	pixel_aspect_ratio,
											PF_Field 			field_type,
											PF_PixelFormat		pixel_format,
											PF_Boolean			clear_pixB,
											PF_EffectWorld		**worldPP);	/* << */


	/**
	**	This will free this effect world. The effect world is no longer valid after this function is called.
	**	Plugin module is only allowed to dispose of effect worlds they create.
	**
	**	@param	effect_ref					Comes with PF_InData. 
	**	@param	worldP						The effect world you want to dispose.
	*/
	SPAPI PF_Err	(*DisposeGPUWorld)(		PF_ProgPtr			effect_ref,
											PF_EffectWorld		*worldP);


	/**
	**	This will return the gpu buffer address of the given effect world.
	**
	**	@param	effect_ref						Comes with PF_InData. 
	**	@param	worldP							The effect world you want to operate on, has to be a gpu effect world.
	**	@param	pixPP							Returns the gpu buffer address.
	*/
	SPAPI PF_Err	(*GetGPUWorldData)(			PF_ProgPtr			effect_ref,
												PF_EffectWorld		*worldP,
												void				**pixPP);	/* << */

	/**
	**	This will return the size of the total data in the effect world.
	**
	**	@param	effect_ref						Comes with PF_InData.
	**	@param	worldP							The effect world you want to operate on, has to be a gpu effect world.
	**	@param	device_indexP					Returns the size of the total data in the effect world.
	*/
	SPAPI PF_Err	(*GetGPUWorldSize)( 		PF_ProgPtr			effect_ref,
												PF_EffectWorld		*worldP,
												size_t				*size_in_bytesP);	/* << */


	/**
	**	This will return device index the gpu effect world is associated with.
	**
	**	@param	effect_ref						Comes with PF_InData. 
	**	@param	worldP							The effect world you want to operate on, has to be a gpu effect world.
	**	@param	device_indexP					Returns the device index of the given effect world.
	*/
	SPAPI PF_Err	(*GetGPUWorldDeviceIndex)( 	PF_ProgPtr			effect_ref,
												PF_EffectWorld		*worldP,
												A_u_long			*device_indexP);	/* << */


} PF_GPUDeviceSuite1;



/**********************************************************/
/**********************************************************/

#ifdef __cplusplus
	}
#endif


#include <adobesdk/config/PostConfig.h>

#endif
