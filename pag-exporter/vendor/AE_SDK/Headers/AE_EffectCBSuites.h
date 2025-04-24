/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
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


#ifndef _H_AE_EffectCBSuites
#define _H_AE_EffectCBSuites


#include <AE_EffectCB.h>
#include <AE_EffectPixelFormat.h>
#include <SPBasic.h>

#include <adobesdk/config/PreConfig.h>

#ifdef __cplusplus
	extern "C" {
#endif

// note: many of these suites are not SPAPI because they are shared with the
// old-style PF_UtilCallback definitions and we want calls to them to
// be object compatible

#define kPFHandleSuite			"PF Handle Suite"
#define kPFHandleSuiteVersion1	2	/* frozen in AE 10.0 */

//Keeping the same version for compatibility reasons but bumping the actual value for 64-bit SDK. Please define A_HandleSize
//as A_u_long to get the same suite working with 32-bit SDK.

typedef struct PF_HandleSuite1 {
	PF_Handle (*host_new_handle)(
		A_HandleSize			size);

	void * (*host_lock_handle)(
		PF_Handle				pf_handle);

	void (*host_unlock_handle)(
		PF_Handle				pf_handle);

	void (*host_dispose_handle)(
		PF_Handle				pf_handle);

	A_HandleSize (*host_get_handle_size)(
		PF_Handle				pf_handle);

	PF_Err (*host_resize_handle)(
		A_HandleSize			new_sizeL,		/* >> */
		PF_Handle				*handlePH);		/* <> Handle Value May Change */
	
} PF_HandleSuite1;


#define kPFANSISuite			"PF ANSI Suite"
#define kPFANSISuiteVersion1	1	/* frozen in AE 5.0 */

typedef struct PF_ANSICallbacksSuite1 {
		A_FpLong	(*atan)(A_FpLong);
		A_FpLong	(*atan2)(A_FpLong y, A_FpLong x);	/* returns atan(y/x) - note param order! */
		A_FpLong	(*ceil)(A_FpLong);				/* returns next int above x */
		A_FpLong	(*cos)(A_FpLong);
		A_FpLong	(*exp)(A_FpLong);					/* returns e to the x power */
		A_FpLong	(*fabs)(A_FpLong);				/* returns absolute value of x */
		A_FpLong	(*floor)(A_FpLong);				/* returns closest int below x */
		A_FpLong	(*fmod)(A_FpLong x, A_FpLong y);	/* returns x mod y */
		A_FpLong	(*hypot)(A_FpLong x, A_FpLong y);	/* returns sqrt(x*x + y*y) */
		A_FpLong	(*log)(A_FpLong);					/* returns natural log of x */
		A_FpLong	(*log10)(A_FpLong);				/* returns log base 10 of x */
		A_FpLong	(*pow)(A_FpLong x, A_FpLong y);		/* returns x to the y power */
		A_FpLong	(*sin)(A_FpLong);
		A_FpLong	(*sqrt)(A_FpLong);
		A_FpLong	(*tan)(A_FpLong);

		int		(*sprintf)(A_char *, const A_char *, ...);
		A_char *	(*strcpy)(A_char *, const A_char *);

		A_FpLong (*asin)(A_FpLong);
		A_FpLong (*acos)(A_FpLong);
	
} PF_ANSICallbacksSuite1;


#define kPFPixelDataSuite				"PF Pixel Data Suite"
#define kPFPixelDataSuiteVersion1	1	/* frozen in AE 7.0 */	

typedef struct PF_PixelDataSuite1 {

	PF_Err	(*get_pixel_data8)(
		PF_EffectWorld	*worldP,
		PF_PixelPtr		pixelsP0,		// NULL to use data in PF_EffectWorld
		PF_Pixel8		**pixPP);		// will return NULL if depth mismatch
		
	PF_Err	(*get_pixel_data16)(
		PF_EffectWorld	*worldP,
		PF_PixelPtr		pixelsP0,		// NULL to use data in PF_EffectWorld
		PF_Pixel16		**pixPP);		// will return NULL if depth mismatch
	
	PF_Err	(*get_pixel_data_float)(
		PF_EffectWorld	*worldP,
		PF_PixelPtr		pixelsP0,		// NULL to use data in PF_EffectWorld
		PF_PixelFloat	**pixPP);		// will return NULL if depth mismatch
	
	
} PF_PixelDataSuite1;
		

#define kPFPixelDataSuiteVersion2	2	/* frozen in AE 16.0 */
		
typedef struct PF_PixelDataSuite2 {
	
	PF_Err	(*get_pixel_data8)(
		PF_EffectWorld	*worldP,
		PF_PixelPtr		pixelsP0,		// NULL to use data in PF_EffectWorld
		PF_Pixel8		**pixPP);		// will return NULL if depth mismatch
	
	PF_Err	(*get_pixel_data16)(
		PF_EffectWorld	*worldP,
		PF_PixelPtr		pixelsP0,		// NULL to use data in PF_EffectWorld
		PF_Pixel16		**pixPP);		// will return NULL if depth mismatch
	
	PF_Err	(*get_pixel_data_float)(
		PF_EffectWorld	*worldP,
		PF_PixelPtr		pixelsP0,		// NULL to use data in PF_EffectWorld
		PF_PixelFloat	**pixPP);		// will return NULL if depth mismatch
	
	PF_Err	(*get_pixel_data_float_gpu)(
		PF_EffectWorld	*worldP,
		void			**pixPP);		// will return NULL if depth mismatch
		
} PF_PixelDataSuite2;


#define kPFColorCallbacksSuite			"PF Color Suite"
#define kPFColorCallbacksSuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct PF_ColorCallbacksSuite1 {
		PF_Err (*RGBtoHLS)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel		*rgb,
		PF_HLS_Pixel	hls);

		PF_Err (*HLStoRGB)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_HLS_Pixel	hls,
		PF_Pixel		*rgb);
		
		PF_Err (*RGBtoYIQ)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel		*rgb,
		PF_YIQ_Pixel	yiq);

		PF_Err (*YIQtoRGB)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_HLS_Pixel	yiq,
		PF_Pixel		*rgb);
		
		PF_Err (*Luminance)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel		*rgb,
		A_long			*lum100);		/* << 100 * luminance */
		
		PF_Err (*Hue)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel		*rgb,
		A_long			*hue);			/* << 0-255 maps to 0-360  */
		
		PF_Err (*Lightness)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel		*rgb,
		A_long			*lightness);		/* <<  goes from 0-255 */
		
		PF_Err (*Saturation)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel		*rgb,
		A_long			*saturation);		/* <<  goes from 0-255 */
		
} PF_ColorCallbacksSuite1;

#define kPFColorCallbacks16Suite			"PF Color16 Suite"
#define kPFColorCallbacks16SuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct PF_ColorCallbacks16Suite1 {
		PF_Err (*RGBtoHLS)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel16		*rgb,
		PF_HLS_Pixel	hls);

		PF_Err (*HLStoRGB)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_HLS_Pixel	hls,
		PF_Pixel16		*rgb);
		
		PF_Err (*RGBtoYIQ)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel16		*rgb,
		PF_YIQ_Pixel	yiq);

		PF_Err (*YIQtoRGB)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_HLS_Pixel	yiq,
		PF_Pixel16		*rgb);
		
		PF_Err (*Luminance)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel16		*rgb,
		A_long			*lum100);		/* << 100 * luminance */
		
		PF_Err (*Hue)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel16		*rgb,
		A_long			*hue);			/* << 0-255 maps to 0-360  */
		
		PF_Err (*Lightness)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel16		*rgb,
		A_long			*lightness);		/* <<  goes from 0-32768 */
		
		PF_Err (*Saturation)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Pixel16		*rgb,
		A_long			*saturation);		/* <<  goes from 0-32768 */
		
} PF_ColorCallbacks16Suite1;




#define kPFColorCallbacksFloatSuite				"PF ColorFloat Suite"
#define kPFColorCallbacksFloatSuiteVersion1	1	/* frozen in AE 7.0 */	


typedef struct PF_ColorCallbacksFloatSuite1 {
		PF_Err (*RGBtoHLS)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_PixelFloat		*rgb,
		PF_HLS_Pixel		hls);

		PF_Err (*HLStoRGB)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_HLS_Pixel	hls,
		PF_PixelFloat	*rgb);
		
		PF_Err (*RGBtoYIQ)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_PixelFloat	*rgb,
		PF_YIQ_Pixel	yiq);

		PF_Err (*YIQtoRGB)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_HLS_Pixel	yiq,
		PF_PixelFloat	*rgb);
		
		PF_Err (*Luminance)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_PixelFloat	*rgb,
		float			*lumP);		/* <<  luminance -- note *not* 100*lum */
		
		PF_Err (*Hue)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_PixelFloat	*rgb,
		float			*hue);			/* 0..360 float  */
		
		PF_Err (*Lightness)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_PixelFloat	*rgb,
		float			*lightness);		/* <<  */
		
		PF_Err (*Saturation)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_PixelFloat	*rgb,
		float			*saturation);		/* <<  */
		
} PF_ColorCallbacksFloatSuite1;



#define kPFBatchSamplingSuite			"PF Batch Sampling Suite"
#define kPFBatchSamplingSuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct PF_BatchSamplingSuite1 {
	 PF_Err (*begin_sampling)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Quality		qual,
		PF_ModeFlags	mf,
		PF_SampPB		*params);
		
	 PF_Err (*end_sampling)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Quality		qual,
		PF_ModeFlags	mf,
		PF_SampPB		*params);
		
	 PF_Err (*get_batch_func)(
		PF_ProgPtr			effect_ref,		/* reference from in_data */
		PF_Quality			quality,
		PF_ModeFlags		mode_flags,
		const PF_SampPB		*params,
		PF_BatchSampleFunc	*batch);

	 PF_Err (*get_batch_func16)(
		PF_ProgPtr				effect_ref,		/* reference from in_data */
		PF_Quality				quality,
		PF_ModeFlags			mode_flags,
		const PF_SampPB			*params,
		PF_BatchSample16Func	*batch);
		
} PF_BatchSamplingSuite1;


#define kPFSampling8Suite			"PF Sampling8 Suite"
#define kPFSampling8SuiteVersion1	1	/* frozen in AE 5.0 */	

typedef struct PF_Sampling8Suite1 {

	 PF_Err (*nn_sample)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_Pixel		*dst_pixel);

	 PF_Err (*subpixel_sample)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_Pixel		*dst_pixel);

	 PF_Err (*area_sample)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_Pixel		*dst_pixel);
		

} PF_Sampling8Suite1;

#define kPFSampling16Suite			"PF Sampling16 Suite"
#define kPFSampling16SuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct PF_Sampling16Suite1 {

	 PF_Err (*nn_sample16)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_Pixel16		*dst_pixel);
		
	 PF_Err (*subpixel_sample16)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_Pixel16		*dst_pixel);

	 PF_Err (*area_sample16)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_Pixel16		*dst_pixel);
		
} PF_Sampling16Suite1;


#define kPFSamplingFloatSuite			"PF SamplingFloat Suite"
#define kPFSamplingFloatSuiteVersion1	1	/* frozen in AE 7.0 */	


typedef struct PF_SamplingFloatSuite1 {

	 PF_Err (*nn_sample_float)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_PixelFloat	*dst_pixel);
		
	 PF_Err (*subpixel_sample_float)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_PixelFloat	*dst_pixel);

	 PF_Err (*area_sample_float)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Fixed		x,
		PF_Fixed		y,
		const PF_SampPB	*params,
		PF_PixelFloat	*dst_pixel);
		
} PF_SamplingFloatSuite1;




#define kPFWorldSuite				"PF World Suite"
#define kPFWorldSuiteVersion2	2	/* frozen in AE 7.0 */	


typedef struct PF_WorldSuite2 {

	PF_Err (*PF_NewWorld)(
		PF_ProgPtr			effect_ref,				/* reference from in_data */
		A_long				widthL,
		A_long				heightL,
		PF_Boolean			clear_pixB,
		PF_PixelFormat		pixel_format,
		PF_EffectWorld		*worldP);		

	PF_Err (*PF_DisposeWorld)(
		PF_ProgPtr			effect_ref,				/* reference from in_data */
		PF_EffectWorld		*worldP);


	PF_Err (*PF_GetPixelFormat)(
		const PF_EffectWorld		*worldP,				/* the pixel buffer of interest */
		PF_PixelFormat				*pixel_formatP);		/* << OUT. one of the above PF_PixelFormat types */


} PF_WorldSuite2;





#define kPFPixelFormatSuite			"PF Pixel Format Suite"
#define kPFPixelFormatSuiteVersion2	2


// call during global setup

typedef struct PF_PixelFormatSuite2 {

	PF_Err	(*PF_AddSupportedPixelFormat)(
						PF_ProgPtr			effect_ref,					/* reference from in_data */
						PF_PixelFormat		pixel_format);				/* add a supported pixel format */


	PF_Err	(*PF_ClearSupportedPixelFormats)(
						PF_ProgPtr			effect_ref);				/* reference from in_data */

} PF_PixelFormatSuite2;





#define kPFWorldSuite			"PF World Suite"
#define kPFWorldSuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct PF_WorldSuite1 {

	 PF_Err (*new_world)(
		PF_ProgPtr			effect_ref,		/* reference from in_data */
		A_long				width,
		A_long				height,
		PF_NewWorldFlags	flags,			/* should would be pre-cleared to zeroes */
		PF_EffectWorld		*world);		/* always 32 bit */

	 PF_Err (*dispose_world)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_EffectWorld	*world);


} PF_WorldSuite1;



#define kPFIterate8Suite			"PF Iterate8 Suite"
#define kPFIterate8SuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct PF_Iterate8Suite1 {
	 PF_Err (*iterate)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		void*		refcon,
		 PF_Err			(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel *in, PF_Pixel *out),
		PF_EffectWorld	*dst);


	 PF_Err (*iterate_origin)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		const PF_Point	*origin,
		void*		refcon,
		 PF_Err			(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel *in, PF_Pixel *out),
		PF_EffectWorld	*dst);

	 PF_Err (*iterate_lut)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		A_u_char	*a_lut0,		/* pass NULL for identity */
		A_u_char	*r_lut0,		/* pass NULL for identity */
		A_u_char	*g_lut0,		/* pass NULL for identity */
		A_u_char	*b_lut0,		/* pass NULL for identity */
		PF_EffectWorld	*dst);

	 PF_Err	(*iterate_origin_non_clip_src)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,
		const PF_Point	*origin,
		void*		refcon,
		PF_Err	(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel *in, PF_Pixel *out),
		PF_EffectWorld	*dst);

	 PF_Err (*iterate_generic)(
		A_long			iterationsL,						/* >> */		// can be PF_Iterations_ONCE_PER_PROCESSOR
		void			*refconPV,							/* >> */
		PF_Err			(*fn_func)(	void *refconPV,			/* >> */
									A_long thread_indexL,		// only call abort and progress from thread_indexL == 0.
									A_long i,
									A_long iterationsL));		// never sends PF_Iterations_ONCE_PER_PROCESSOR

} PF_Iterate8Suite1;

#define kPFIterate8SuiteVersion2	2	/* frozen in AE 22.0 */	

typedef struct PF_Iterate8Suite2 {
	PF_Err(*iterate)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		void			*refcon,
		PF_Err(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel* in, PF_Pixel* out),
		PF_EffectWorld	*dst);


	PF_Err(*iterate_origin)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		const PF_Point	*origin,
		void			*refcon,
		PF_Err(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel* in, PF_Pixel* out),
		PF_EffectWorld	*dst);

	PF_Err(*iterate_lut)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		A_u_char		*a_lut0,		/* pass NULL for identity */
		A_u_char		*r_lut0,		/* pass NULL for identity */
		A_u_char		*g_lut0,		/* pass NULL for identity */
		A_u_char		*b_lut0,		/* pass NULL for identity */
		PF_EffectWorld	*dst);

	PF_Err(*iterate_origin_non_clip_src)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,
		const PF_Point	*origin,
		void			*refcon,
		PF_Err(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel* in, PF_Pixel* out),
		PF_EffectWorld	*dst);

	PF_Err(*iterate_generic)(
		A_long			iterationsL,	/* >> */		// can be PF_Iterations_ONCE_PER_PROCESSOR
		void			*refconPV,		/* >> */
		PF_Err(*fn_func)(
			void	*refconPV,			/* >> */
			A_long	thread_indexL,		// only call abort and progress from thread_indexL == 0.
			A_long	i,
			A_long	iterationsL));		// never sends PF_Iterations_ONCE_PER_PROCESSOR

} PF_Iterate8Suite2;

#define kPFIterate16Suite			"PF iterate16 Suite"
#define kPFIterate16SuiteVersion1	1	/* frozen in AE 5.0 */	


typedef struct PF_iterate16Suite1 {
	 PF_Err (*iterate)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		void*		refcon,
		 PF_Err			(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel16 *in, PF_Pixel16 *out),
		PF_EffectWorld	*dst);


	 PF_Err (*iterate_origin)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		const PF_Point	*origin,
		void*			refcon,
		 PF_Err			(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel16 *in, PF_Pixel16 *out),
		PF_EffectWorld	*dst);

	 PF_Err	(*iterate_origin_non_clip_src)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,
		const PF_Point	*origin,
		void*		refcon,
		PF_Err	(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel16 *in, PF_Pixel16 *out),
		PF_EffectWorld	*dst);
		
} PF_Iterate16Suite1;

#define kPFIterate16SuiteVersion2	2	/* frozen in AE 22.0 */	

typedef struct PF_iterate16Suite2 {
	PF_Err(*iterate)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		void			*refcon,
		PF_Err(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel16* in, PF_Pixel16* out),
		PF_EffectWorld	*dst);


	PF_Err(*iterate_origin)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		const PF_Point	*origin,
		void			*refcon,
		PF_Err(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel16* in, PF_Pixel16* out),
		PF_EffectWorld	*dst);

	PF_Err(*iterate_origin_non_clip_src)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,
		const PF_Point	*origin,
		void			*refcon,
		PF_Err(*pix_fn)(void* refcon, A_long x, A_long y, PF_Pixel16* in, PF_Pixel16* out),
		PF_EffectWorld	*dst);

} PF_Iterate16Suite2;

#define kPFIterateFloatSuite			"PF iterateFloat Suite"
#define kPFIterateFloatSuiteVersion1	1	/* frozen in AE 7.0 */	


typedef struct PF_iterateFloatSuite1 {
	 PF_Err (*iterate)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		void*		refcon,
		PF_IteratePixelFloatFunc			pix_fn,
		PF_EffectWorld	*dst);


	 PF_Err (*iterate_origin)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		const PF_Point	*origin,
		void*		refcon,
		PF_IteratePixelFloatFunc			pix_fn,
		PF_EffectWorld	*dst);

	 PF_Err	(*iterate_origin_non_clip_src)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld	*src,
		const PF_Rect	*area,
		const PF_Point	*origin,
		void*		refcon,
		PF_IteratePixelFloatFunc			pix_fn,
		PF_EffectWorld	*dst);
		
} PF_IterateFloatSuite1;

#define kPFIterateFloatSuiteVersion2	2	/* frozen in AE 22.0 */	

typedef struct PF_iterateFloatSuite2 {
	 PF_Err (*iterate)(
		PF_InData					*in_data,
		A_long						progress_base,
		A_long						progress_final,
		PF_EffectWorld				*src,
		const PF_Rect				*area,			/* pass NULL for all pixels */
		void*						refcon,
		PF_IteratePixelFloatFunc	pix_fn,
		PF_EffectWorld				*dst);


	 PF_Err (*iterate_origin)(
		PF_InData					*in_data,
		A_long						progress_base,
		A_long						progress_final,
		PF_EffectWorld				*src,
		const PF_Rect				*area,			/* pass NULL for all pixels */
		const PF_Point				*origin,
		void*						refcon,
		PF_IteratePixelFloatFunc	pix_fn,
		PF_EffectWorld				*dst);

	 PF_Err	(*iterate_origin_non_clip_src)(
		PF_InData					*in_data,
		A_long						progress_base,
		A_long						progress_final,
		PF_EffectWorld				*src,
		const PF_Rect				*area,
		const PF_Point				*origin,
		void*						refcon,
		PF_IteratePixelFloatFunc	pix_fn,
		PF_EffectWorld				*dst);
		
} PF_IterateFloatSuite2;

#define kPFWorldTransformSuite			"PF World Transform Suite"
#define kPFWorldTransformSuiteVersion1	1	/* frozen in AE 5.0 */	

typedef struct PF_WorldTransformSuite1 {

	 PF_Err (*composite_rect)(
		PF_ProgPtr		effect_ref,		/* from in_data */
		PF_Rect			*src_rect,		/* rectangle in source image */
		A_long			src_opacity,	/* opacity of src */
		PF_EffectWorld	*source_wld,	/* src PF world */
		A_long			dest_x,			/* upper left-hand corner of src rect...*/
		A_long			dest_y,			/* ... in composite image */
		PF_Field		field_rdr,		/* which scanlines to render (all, upper, lower) */
		PF_XferMode		xfer_mode,		/* Copy, Composite Behind, Composite In Front */
		PF_EffectWorld	*dest_wld);		/* Destination buffer. Already filled */

	 PF_Err (*blend)(
		PF_ProgPtr				effect_ref,		/* reference from in_data */
		const PF_EffectWorld	*src1,
		const PF_EffectWorld	*src2,
		PF_Fixed				ratio,			/* 0 == full src1, 0x00010000 == full src2 */
		PF_EffectWorld			*dst);

	 PF_Err (*convolve)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_EffectWorld	*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		PF_KernelFlags	flags,
		A_long			kernel_size,
		void			*a_kernel,
		void			*r_kernel,
		void			*g_kernel,
		void			*b_kernel,
		PF_EffectWorld	*dst);

	 PF_Err (*copy)(
		PF_ProgPtr		effect_ref,		/* reference from in_data	*/
		PF_EffectWorld	*src,
		PF_EffectWorld	*dst,
		PF_Rect 		*src_r,			/* pass NULL for whole world */
		PF_Rect			*dst_r);		/* pass NULL for whole world */

	 PF_Err (*copy_hq)(
		PF_ProgPtr		effect_ref,		/* reference from in_data	*/
		PF_EffectWorld	*src,
		PF_EffectWorld	*dst,
		PF_Rect 		*src_r,			/* pass NULL for whole world */
		PF_Rect			*dst_r);		/* pass NULL for whole world */


	 PF_Err	(*transfer_rect)(
		PF_ProgPtr				effect_ref,
		PF_Quality				quality,
		PF_ModeFlags			m_flags,
		PF_Field				field,
		const PF_Rect			*src_rec,
		const PF_EffectWorld	*src_world,
		const PF_CompositeMode	*comp_mode,
		const PF_MaskWorld		*mask_world0,
		A_long					dest_x,
		A_long					dest_y,
		PF_EffectWorld			*dst_world);

	 PF_Err	(*transform_world)(
		PF_ProgPtr				effect_ref,
		PF_Quality				quality,
		PF_ModeFlags			m_flags,
		PF_Field				field,
		const PF_EffectWorld	*src_world,
		const PF_CompositeMode	*comp_mode,
		const PF_MaskWorld		*mask_world0,
		const PF_FloatMatrix	*matrices,
		A_long					num_matrices,
		PF_Boolean				src2dst_matrix,
		const PF_Rect			*dest_rect,
		PF_EffectWorld			*dst_world);


} PF_WorldTransformSuite1;


#define kPFFillMatteSuite			"PF Fill Matte Suite"


#define kPFFillMatteSuiteVersion2	2	/* frozen in AE 7.0 */	
typedef struct PF_FillMatteSuite2 {

	 PF_Err (*fill)(
		PF_ProgPtr		effect_ref,		/* reference from in_data	*/
		const PF_Pixel	*color,
		const PF_Rect	*dst_rect,		/* pass NULL for whole world */
		PF_EffectWorld	*world);

	 PF_Err (*fill16)(
		PF_ProgPtr			effect_ref,		/* reference from in_data	*/
		const PF_Pixel16	*color,
		const PF_Rect		*dst_rect,		/* pass NULL for whole world */
		PF_EffectWorld		*world);

	 PF_Err (*fill_float)(
		PF_ProgPtr			effect_ref,		/* reference from in_data	*/
		const PF_PixelFloat	*color,
		const PF_Rect		*dst_rect,		/* pass NULL for whole world */
		PF_EffectWorld		*world);

	 PF_Err (*premultiply)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		A_long			forward,		/* TRUE means convert non-premul to premul, FALSE mean reverse */
		PF_EffectWorld	*dst);
		
	 PF_Err (*premultiply_color)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_EffectWorld	*src,
		const PF_Pixel	*color,			/* color to premultiply/unmultiply with */
		A_long			forward,		/* TRUE means convert non-premul to premul, FALSE mean reverse */
		PF_EffectWorld	*dst);
		
	 PF_Err (*premultiply_color16)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_EffectWorld	*src,
		const PF_Pixel16 *color,			/* color to premultiply/unmultiply with */
		A_long			forward,		/* TRUE means convert non-premul to premul, FALSE mean reverse */
		PF_EffectWorld	*dst);

	 PF_Err (*premultiply_color_float)(
		PF_ProgPtr			effect_ref,		/* reference from in_data */
		PF_EffectWorld		*src,
		const PF_PixelFloat *color,			/* color to premultiply/unmultiply with */
		A_long				forward,		/* TRUE means convert non-premul to premul, FALSE mean reverse */
		PF_EffectWorld		*dst);

} PF_FillMatteSuite2;



#ifdef __cplusplus
	}		// end extern "C"
#endif



#include <adobesdk/config/PostConfig.h>


#endif

