#ifndef SDK_INVERT_PROC_AMP
#	define SDK_INVERT_PROC_AMP

#	include "PrGPU/KernelSupport/KernelCore.h" //includes KernelWrapper.h
#	include "PrGPU/KernelSupport/KernelMemory.h"

#	if GF_DEVICE_TARGET_DEVICE
		GF_KERNEL_FUNCTION(ProcAmp2Kernel,
			((const GF_PTR(float4))(inSrc))
			((GF_PTR(float4))(outDst)),
			((int)(inSrcPitch))
			((int)(inDstPitch))
			((int)(in16f))
			((unsigned int)(inWidth))
			((unsigned int)(inHeight))
			((float)(inBrightness))
			((float)(inContrast))
			((float)(inHueCosSaturation))
			((float)(inHueSinSaturation)),
			((uint2)(inXY)(KERNEL_XY)))
		{
			if (inXY.x < inWidth && inXY.y < inHeight)
			{
				/* We'll operate in-place on 16f/32f data */
				float4 pixel = ReadFloat4(inSrc, inXY.y * inSrcPitch + inXY.x, !!in16f);

				/* RGB -> YUV */
				float srcY = pixel.z * 0.299f     + pixel.y * 0.587f     + pixel.x * 0.114f;
				float srcU = pixel.z * -0.168736f + pixel.y * -0.331264f + pixel.x * 0.5f;
				float srcV = pixel.z * 0.5f       + pixel.y * -0.418688f + pixel.x * -0.081312f;

				/* Render ProcAmp */
				float dstY = (inContrast * srcY) + inBrightness;
				float dstU = (srcU * inHueCosSaturation) + (srcV * -inHueSinSaturation);
				float dstV = (srcV * inHueCosSaturation) + (srcU *  inHueSinSaturation);

				/* YUV -> RGB */
				pixel.z = dstY * 1.0f + dstU * 0.0f       + dstV * 1.402f;
				pixel.y = dstY * 1.0f + dstU * -0.344136f + dstV * -0.714136f;
				pixel.x = dstY * 1.0f + dstU * 1.772f     + dstV * 0.0f;

				WriteFloat4(pixel, outDst, inXY.y * inDstPitch + inXY.x, !!in16f);
			}
		}

		GF_KERNEL_FUNCTION(InvertColorKernel,
			((const GF_PTR(float4))(inSrc))
			((GF_PTR(float4))(outDst)),
			((int)(inSrcPitch))
			((int)(inDstPitch))
			((int)(in16f))
			((unsigned int)(inWidth))
			((unsigned int)(inHeight)),
			((uint2)(inXY)(KERNEL_XY)))
		{
			if (inXY.x < inWidth && inXY.y < inHeight)
			{
				float4 pixel = ReadFloat4(inSrc, inXY.y * inSrcPitch + inXY.x, !!in16f);

				pixel.x = fmax(fmin(1.0f, pixel.x), 0.0f);
				pixel.y = fmax(fmin(1.0f, pixel.y), 0.0f);
				pixel.z = fmax(fmin(1.0f, pixel.z), 0.0f);
				pixel.w = fmax(fmin(1.0f, pixel.w), 0.0f);

				pixel.x = 1.0 - pixel.x;
				pixel.y = 1.0 - pixel.y;
				pixel.z = 1.0 - pixel.z;

				WriteFloat4(pixel, outDst, inXY.y * inDstPitch + inXY.x, !!in16f);
			}
		}
#	endif

#	if __NVCC__

		void Invert_Color_CUDA (
			float const *src,
			float *dst,
			unsigned int srcPitch,
			unsigned int dstPitch,
			int is16f,
			unsigned int width,
			unsigned int height)
		{
			dim3 blockDim(16, 16, 1);
			dim3 gridDim((width + blockDim.x - 1) / blockDim.x, (height + blockDim.y - 1) / blockDim.y, 1);

			InvertColorKernel <<< gridDim, blockDim, 0 >>> ((float4 const*)src, (float4*)dst, srcPitch, dstPitch, is16f, width, height);

			cudaDeviceSynchronize();
		}

		void ProcAmp_CUDA (
			float const *src,
			float *dst,
			unsigned int srcPitch,
			unsigned int dstPitch,
			int	is16f,
			unsigned int width,
			unsigned int height,
			float inBrightness,
			float inContrast,
			float inHueCosSaturation,
			float inHueSinSaturation)
		{
			dim3 blockDim (16, 16, 1);
			dim3 gridDim ( (width + blockDim.x - 1)/ blockDim.x, (height + blockDim.y - 1) / blockDim.y, 1 );

			ProcAmp2Kernel <<< gridDim, blockDim, 0 >>> ((float4 const*) src, (float4*) dst, srcPitch, dstPitch, is16f, width, height, inBrightness, inContrast, inHueCosSaturation, inHueSinSaturation );

			cudaDeviceSynchronize();
		}
#	endif //GF_DEVICE_TARGET_HOST

#endif
