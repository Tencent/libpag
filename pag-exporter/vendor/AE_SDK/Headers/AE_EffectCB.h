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


/** AE_EffectCB.h

	Part of the After Effects SDK
	
	NOTES
		This file describes utility functions and macros for accessing
		those utilities that are provided to every filter.  These functions
		provide graphical tools, mathematical utilities, and other basic
		library functions.

		An effect will want to use these callbacks for three primary reasons:

		1)	The mathematical and graphics callbacks will be efficiently
			implemented, and will adaptively take advantage of any hardware
			acceleration transparent to the effects module.
		2)	The callbacks will maximize portability of the effect code
			and consistency of results from platform to platform and from
			effect to effect.
		3)	The callbacks will simplify construction of complex filters,
			both making filters easier to write and resulting in smaller
			code for each filter.

		The After Effects standard for routine parameters is to list input
		parameters first, then list parameters whose contents will be modified,
		and then pass output parameters whose old value will be completely
		replaced. Large or significant params tend to be listed earlier within
		their segment.  Most callbacks we provide follow this standard, so with a
		convolution callback, we generally would order parameters:
			convolve(src_world, convolution_details, dst_world)
		However, not all Mac routines follow this, so to be easily
		brain-compatible with Mac programmers, we structure some routines
		(actually just CopyBits) that we provide to be as much like the
		corresponding Mac routine as possible.

**/

#ifndef _H_AE_EffectCB
#define _H_AE_EffectCB


#include <AE_Effect.h>

#include <adobesdk/config/PreConfig.h>

#ifdef __cplusplus
	extern "C" {
#endif


/** ---------- Useful Constants ---------- **/

#define	PF_PI				3.14159265358979323846
#define PF_2PI				6.28318530717958647692
#define PF_HALF_PI			1.57079632679489661923
#define PF_E				2.7182818284590452354
#define PF_SQRT2			1.41421356237309504880
#define PF_RAD_PER_DEGREE	0.01745329251994329576



/** ---------- PF_KernelFlags ----------

	Many functions work with "kernels" or matrices of values.  These matrices
	can be of different types, of different arrangements, and can be generated
	or treated in different ways.  The KernelFlags are used in a variety of
	functions to determine how the matrices should be created and used.  You
	should OR together any flags you need.  Which flags are relevant for a
	given routine are documented along with the prototype for the routine below.

	The most important information to consider is the type of data.
	You will have to choose whether to use Fixeds, Chars, or Longs.
	See the information on the USE_... flags below.

	Note, the default for each flag is listed first and passing zero for
	the flags parameter will automatically get you the defaults.

	??? Some of the non-default parameters may not be implemented.  Where
		a flag is not implemented it will be commented with ??? beside it.

**/

/* pass bottom flag for 1 dimensional kernel, or top for 2D kernel */
#define	PF_KernelFlag_2D				0
#define	PF_KernelFlag_1D				(1L << 0)

/* pass bottom flag to equalize kernel, forcing the volume under the
 * kernel surface to be the same as the volume under the covered area
 * of pixels.  Otherwise, it will be unnormalized.
 */
#define	PF_KernelFlag_UNNORMALIZED		0
#define	PF_KernelFlag_NORMALIZED		(1L << 1)

/* use the first flag to force values to be clamped into their valid
 * range (that is determined by the type of item (A_char, fixed, A_long).
 */
#define	PF_KernelFlag_CLAMP				0
#define PF_KernelFlag_NO_CLAMP			(1L << 2)

/* pass the first flag to treat kernel as an array of longs valued from 0 to 255.
 * pass the second to treat kernel as an array of unsigned chars from 0 to 255,
 * pass the third to treat kernel as an array of Fixeds from 0 to 1.
 * ??? NOTE!  For now, only USE_LONG is implemented! ???
 */
#define	PF_KernelFlag_USE_LONG			0
#define	PF_KernelFlag_USE_CHAR			(1L << 3)
#define PF_KernelFlag_USE_FIXED			(1L << 4)
#define PF_KernelFlag_USE_UNDEFINED		((1L << 4) | (1L << 3))

/* pass the top flag to apply a 1D convolution horizontally,
 * the second to apply it vertically.
 */
#define PF_KernelFlag_HORIZONTAL		0
#define	PF_KernelFlag_VERTICAL			(1L << 5)

/* pass the second flag to replicate border pixels when sampling
 * off the edge;  pass the first flag to treat pixels off the
 * edge as alpha zero (black).  ??? NOTE! The replicate borders
 * flag is unimplemented and this will be ignored. ???
 */
#define PF_KernelFlag_TRANSPARENT_BORDERS	0
#define	PF_KernelFlag_REPLICATE_BORDERS		(1L << 6)

/* top flag indicates straight convolution, second tells the
 * convolution code to alpha-weight the contributions of pixels
 * to the resulting convolved output.  ??? NOTE! The alpha weighted
 * convolve is not implemented and this will be ignored. ???
 */
#define PF_KernelFlag_STRAIGHT_CONVOLVE		0
#define PF_KernelFlag_ALPHA_WEIGHT_CONVOLVE	(1L << 7)

typedef A_u_long PF_KernelFlags;



/** ---------- PF_SampleEdgeBehav ----------

	The sampling routines always deal with 32 bit images, and thus
	need to compute properly alpha-weighted samples. An issue arises
	when an attempt is made to sample outside of the image content area.
	
	Before PF_PLUG_IN_VERSION 2, After Effects always treated pixels
	outside of the image content area as having alpha = 0, which is desirable
	in many cases. Distortions and other effects may want different sampling
	behaviors, however, hence the PF_SampleEdgeBehav.
	
	
**/

enum {
	PF_SampleEdgeBehav_ZERO = 0L		/* Treat pixels outside image as alpha 0;
										 * Default behavior in After Effects 1.x */
/* Sorry, not supported!
	PF_SampleEdgeBehav_REPEAT = 1L,		// Samples are clamped to nearest edge
	PF_SampleEdgeBehav_WRAP = 2L		// Image wraps around horizontally and vertically
*/

};

typedef	A_u_long PF_SampleEdgeBehav;

enum {
    PF_Xfer_NONE = -1,
    PF_Xfer_COPY,
    PF_Xfer_BEHIND,
    PF_Xfer_IN_FRONT,
    PF_Xfer_DISSOLVE,
    PF_Xfer_ADD,
    PF_Xfer_MULTIPLY,
    PF_Xfer_SCREEN,
    PF_Xfer_OVERLAY,
    PF_Xfer_SOFT_LIGHT,
    PF_Xfer_HARD_LIGHT,
    PF_Xfer_DARKEN,
    PF_Xfer_LIGHTEN,
    PF_Xfer_DIFFERENCE,							// original < PS5.5 Difference
    PF_Xfer_HUE,
    PF_Xfer_SATURATION,
    PF_Xfer_COLOR,
    PF_Xfer_LUMINOSITY,
    PF_Xfer_MULTIPLY_ALPHA,                     // dest alpha *= src alpha
    PF_Xfer_MULTIPLY_ALPHA_LUMA,                // dest alpha *= src luminance
    PF_Xfer_MULTIPLY_NOT_ALPHA,					// dest alpha *= ~(src alpha)
    PF_Xfer_MULTIPLY_NOT_ALPHA_LUMA,			// dest alpha *= ~(src luminance)
    PF_Xfer_ADDITIVE_PREMUL,
    PF_Xfer_ALPHA_ADD,
	PF_Xfer_COLOR_DODGE,						// original < PS5.5 Color Dodge
	PF_Xfer_COLOR_BURN,							// original < PS5.5 Color Burn
	PF_Xfer_EXCLUSION,

	PF_Xfer_DIFFERENCE2,							// PS >= 6.0, PDF 1.4 Difference
	PF_Xfer_COLOR_DODGE2,							// PS >= 6.0, PDF 1.4 Color Dodge
	PF_Xfer_COLOR_BURN2,							// PS >= 6.0, PDF 1.4 Color Burn
	
	PF_Xfer_LINEAR_DODGE,									
	PF_Xfer_LINEAR_BURN,
	PF_Xfer_LINEAR_LIGHT,
	PF_Xfer_VIVID_LIGHT,
	PF_Xfer_PIN_LIGHT,
	
	PF_Xfer_HARD_MIX,
	
	PF_Xfer_LIGHTER_COLOR,							// new in AE8
	PF_Xfer_DARKER_COLOR,
	
	PF_Xfer_SUBTRACT,								// new in AE10
	PF_Xfer_DIVIDE,							
	
	PF_Xfer_RESERVED0,								// private/useless
	PF_Xfer_RESERVED1,								// ditto

	PF_Xfer_NUM_MODES
};

// obsolete xfer mode names
enum {
	PF_Xfer_TINT  = PF_Xfer_LINEAR_DODGE,
	PF_Xfer_SHADE = PF_Xfer_LINEAR_BURN,
	PF_Xfer_INTENSE_LIGHT = PF_Xfer_VIVID_LIGHT
};

#define PF_TransferMode_ZERO_SRC_ALPHA_CLEARS_DST_ALPHA(TMODE) \
	((TMODE) == PF_Xfer_MULTIPLY_ALPHA ||		\
	 (TMODE) == PF_Xfer_MULTIPLY_ALPHA_LUMA)


// PF_TransferMode_ZERO_ALPHA_NOP is deprecated because it was
// confusing -- you probably want PF_TransferMode_ZERO_SRC_ALPHA_CLEARS_DST_ALPHA
// instead

// WARNING: this macro is incorrect for PF_Xfer_COPY (returns true), but it's been like this for so long
//			that we are leaving it unchanged so as not to create bugs by changing it.
#define PF_TransferMode_ZERO_SRC_ALPHA_LEAVES_DST_UNCHANGED(TMODE) \
	(((TMODE) == PF_Xfer_MULTIPLY_ALPHA ||		\
	 (TMODE) == PF_Xfer_MULTIPLY_ALPHA_LUMA ||	\
	 (TMODE) == PF_Xfer_ADDITIVE_PREMUL) == 0)

enum {
	PF_MaskFlag_NONE = 0,					/* just use the alpha, thank you */
	PF_MaskFlag_INVERTED = 1L << 0,			/* invert the result of the mask */
	PF_MaskFlag_LUMINANCE = 1L << 1			/* use the luminance values	*/
	
};


typedef A_long PF_MaskFlags;


typedef struct {

	PF_EffectWorld		mask;
	PF_Point			offset;	
	PF_MaskFlags		what_is_mask;

} PF_MaskWorld;


/** ---------- PF_SampPB ----------

	There are calls to sample an a non-integral point in an image,
	and to sample an area of an image.  This parameter block describes
	some information needed for these image resampling routines.

**/
typedef struct {
	
	/* parameters needed for single point or area sample */
	
	PF_Fixed				x_radius;		/* radii are used for area sample, 0 for point sample */
	PF_Fixed				y_radius;
	PF_Fixed				area;			/* must fit in a Fixed; must be correct */
	PF_EffectWorld			*src;			/* the world to sample from */
	PF_SampleEdgeBehav		samp_behave;
	A_long					allow_asynch;	/* It's okay if I don't get the result until end_sampling */


	/* parameters needed for batch sampling & compositing, motion blur, etc. */

	A_long					motion_blur;	/* requires pointer to 2 starting points and 2 dxdy's */
	PF_CompositeMode		comp_mode;		/* compositing mode info */
    PF_PixelPtr           	mask0;     		/* per-pixel extra masking, before xfer mode */
	
	A_u_char			*fcm_table;
	A_u_char			*fcd_table;
	A_long 					reserved[8];	/* Set to zero at beginsampling */	

} PF_SampPB;


/** ---------- Callback Selectors ----------

	Some callbacks have different high and low quality versions.
	The parameter block of function pointers will automatically
	be filled with the appropriate versions for the current quality
	setting, but some filters may wish to override this and access
	a callback of different quality.  To do this, a get_callback_addr
	callback is provided which will take a callback selector and a
	desired quality and return the callback of that quality.  The
	selectors for the various callbacks are listed here.  Also, a
	typedef for the function pointer that will be returned is given.

**/

enum {
	PF_Callback_NONE = 0,
	PF_Callback_BEGIN_SAMPLING,
	PF_Callback_SUBPIXEL_SAMPLE,
	PF_Callback_AREA_SAMPLE,
	PF_Callback_OBSOLETE0,
	PF_Callback_END_SAMPLING,
	PF_Callback_COMPOSITE_RECT,
	PF_Callback_BLEND,
	PF_Callback_CONVOLVE,
	PF_Callback_COPY,
	PF_Callback_FILL,
	PF_Callback_GAUSSIAN,
	PF_Callback_ITERATE,
	PF_Callback_PREMUL,
	PF_Callback_PREMUL_COLOR,
	PF_Callback_RGB_TO_HLS,
	PF_Callback_HLS_TO_RGB,
	PF_Callback_RGB_TO_YIQ,
	PF_Callback_YIQ_TO_RGB,
	PF_Callback_LUMINANCE,
	PF_Callback_HUE,
	PF_Callback_LIGHTNESS,
	PF_Callback_SATURATION,
	PF_Callback_NEW_WORLD,
	PF_Callback_DISPOSE_WORLD,
	PF_Callback_ITERATE_ORIGIN,
	PF_Callback_ITERATE_LUT,
	PF_Callback_TRANSFER_RECT,
	PF_Callback_TRANSFORM_WORLD,
	PF_Callback_ITERATE_ORIGIN_NON_CLIP_SRC,
	PF_Callback_ITERATE_GENERIC,
	PF_Callback_SUBPIXEL_SAMPLE16,
	PF_Callback_AREA_SAMPLE16,
	PF_Callback_FILL16,
	PF_Callback_PREMUL_COLOR16,
	PF_Callback_ITERATE16,
	PF_Callback_ITERATE_ORIGIN16,
	PF_Callback_ITERATE_ORIGIN_NON_CLIP_SRC16,
	PF_Callback_ITERATE_GENERIC_NO_MAX_THREADS,
	PF_Callback_ITERATE_NO_MAX_THREADS,
	PF_Callback_ITERATE_ORIGIN_NO_MAX_THREADS,
	PF_Callback_ITERATE_ORIGIN_NON_CLIP_SRC_NO_MAX_THREADS,
	PF_Callback_ITERATE16_NO_MAX_THREADS,
	PF_Callback_ITERATE_ORIGIN16_NO_MAX_THREADS,
	PF_Callback_ITERATE_ORIGIN_NON_CLIP_SRC16_NO_MAX_THREADS,
};
typedef A_long PF_CallbackID;

/**
	 Previous versions of PF_CallbackFunc were defined using varargs in attempt to enforce
	 the effect_ref passed as the first argument to all callbacks. The actual host
	 implementations are generally *not* varargs, and on some platforms the calling
	 conventions between normal C ABI and C varargs ABI are not compatible. If your
	 code is trying to call through a PF_CallbackFunc it is unsafe; with this definition
	 it will now no longer compile. To fix, cast to the proper API signature before
	 calling. Or better, use the extensive Suite implementations which are explicitly typed
**/

typedef struct PF_YouMustCastThisToActualFunctionType  *PF_CallbackFunc;

#define		AEFX_MAX_PATH		260

enum {
	PF_PlatData_MAIN_WND = 0,			// windows only		(output data => HWND)
	PF_PlatData_EXE_FILE_PATH_DEPRECATED,			// deprecated in CS6. Use _W versions below
	PF_PlatData_RES_FILE_PATH_DEPRECATED,			// deprecated in CS6. Use _W versions below
	PF_PlatData_RES_REFNUM,				// deprecated in AE 2015. Use PF_PlatData_BUNDLE_REF instead
	PF_PlatData_RES_DLLINSTANCE,		// windows only		(output data => HANDLE)
	PF_PlatData_SP_PLUG_REF,			// unimplemented
	PF_PlatData_BUNDLE_REF,				// mac only			(output data => CFBundleRef)
	PF_PlatData_EXE_FILE_PATH_W,		// mac and windows	(output data => A_UTF16Char[AEFX_MAX_PATH])
	PF_PlatData_RES_FILE_PATH_W			//// mac and windows	(output data => A_UTF16Char[AEFX_MAX_PATH])
};

typedef A_long PF_PlatDataID;


/** ---------- Image Plane Selectors ----------

	These constants can be used to specify a subset
	of the planes of the 32-bit image.

**/

enum {
	PF_Plane_ALPHA = 1,
	PF_Plane_RED = 2,
	PF_Plane_GREEN = 4,
	PF_Plane_BLUE = 8
};
typedef A_u_long PF_Plane;


/** ---------- ANSI Routines Block ----------

	Within the callback routines block there is a block of ANSI
	routines, so that the filter will not need to link with the
	ANSI library.  The following structure describes that block
	and is included in the larger Callback Routines block below.

	All angles are expressed in radians; use PF_RAD_PER_DEGREE
	to convert from degrees to radians, if necessary.  Be aware
	that angle parameter types use degrees (in fixed point).

	Sprintf and strcpy are provided to facilitate string usage,
	such as printing for names and supervised controls.

	None of these callbacks vary based on the Quality setting.

**/

typedef struct {
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
	
	A_long	ansi_procs[1];
} PF_ANSICallbacks;



/** ---------- Colorspace Conversion Callbacks
 **
 **/

typedef struct {
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
		PF_YIQ_Pixel	yiq,
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
		
} PF_ColorCallbacks;



typedef void * PF_BatchSampleFunc;		/*	<<<Batch Sampling is deprecated in AE 7.0, sorry */
typedef void * PF_BatchSample16Func;	/*	<<<Batch Sampling is deprecated in AE 7.0, sorry */


#define PF_Iterations_ONCE_PER_PROCESSOR			(-1L)

// misspelling provided for source compatibility
#define PF_Iteratations_ONCE_PER_PROCESSOR			PF_Iterations_ONCE_PER_PROCESSOR

enum {
	PF_NewWorldFlag_NONE			= 0,		
	PF_NewWorldFlag_CLEAR_PIXELS	= 1L << 0,		/* clear pixels to zero on creation */
	PF_NewWorldFlag_DEEP_PIXELS		= 1L << 1,		/* create deep world */
	PF_NewWorldFlag_RESERVED0		= 1L << 2,		/* now in use internally -- 5/14/2002 */
	PF_NewWorldFlag_RESERVED1		= 1L << 3
};

typedef A_long PF_NewWorldFlags;


/** ---------- Callback Routines Block ----------

	This structure is pointed to by the utils pointer in the in_params
	structure described in PF_Public.h.  The functions in this block are
	described here, but are more easily accessed through the macros which
	follow at the bottom of this file.

	The functions defined in this block are:

	begin_sampling
		Call this routine before you plan to perform a large number of
		image resamplings. Depending on platform, this routine could
		start up the DSP chip, compute an index table to each scanline
		of the buffer, or whatever might be needed to speed up image
		resampling.
		
	subpixel_sample
		Use this to interpolate the appropriate alpha weighted mix of
		colors at a non-integral point in a source image, in high quality.
		Nearest neighbor sample is used in low quality.

	area_sample
		Use this to calculate the appropriate alpha weighted average
		of an axis-aligned non-integral rectangle of color in a source
		image, in high quality.  Nearest neighbor in low quality.
		Because of overflow issues, this can only average a maximum of
		a 256 pixel by 256 pixel area (ie. x and y range < 128 pixels).
	
	get_batch_func -- returns a pointer to the "batch_sample" function given
		a sampling parameter block and a quality level. A batch
		sample routine is used to perform a large number of image resamples at once.
		This routine could do a number of things to improve sampling performance,
		depending on platform. One obvious possible speedup is the elimination
		of function calls for repeated samples. Another is pipelining the sample
		requests into a DSP, and another is using context to avoid recalculation
		of sample weights, etc.
		
	end_sampling
		Call this routine when you're done sampling. It should undo
		whatever begin_sampling did.

	blend
		To blend two images with one another...  This is provided
		because ALL effects should have a default state in which there
		is no visual change to the source image.  This can often be
		realized by providing a "blend-with-source" slider.  It is
		possible that this will have different high and low qual versions.

	convolve
		Convolve an image with an arbitrary size kernel on each of the
		a, r, g, and b channels separately.  You can specify a rectangle to
		convolve (for instance, the extent_hint), or pass NULL to convolve
		the entire image.  This looks for kernel flags:
			1D or 2D
			Clamp or No Clamp
			Use longs-chars-fixeds
			straight convolve vs. alpha-weighted
		plus, if 1D is specified:
			Horizontal or Vertical
		See the comments about the kernel flags above.
		Note: some 2D convolutions are seperable and can be implemented
		with a horizontal 1D convolve and a vertical 1D convolve.
		This filter may have different high and low quality versions.

	copy
		This blits a region from one PF_EffectWorld to another.  This is an alpha-
		preserving (unlike CopyBits), 32-bit only, non-antialiased stretch blit.
		The high qual version does an anti-aliased blit (ie. it interpolates).

	fill
		This fills a rectangle in the image with the given color.  Setting
		the color pointer to NULL will fill the rectangle with black.
		Quality setting doesn't matter.

	gaussian_kernel
		Generate a kernel with a Gaussian distribution of values.
		This looks for kernel flags:
			1D or 2D
			Normalized or Unnormalized
			Use longs-chars-fixeds
		This filter will be the same high and low quality.
		Parameter Notes:
			multiplier:  this value is multiplied by every value generated;
				in general, you should pass 1.0, but this lets you adjust
				the "fuzziness" of the kernel.
			diameter:  actual integral width of generated kernel;  this will
				always currently be (int)ceil(kRadius) * 2 + 1;  you need to
				know this because the "kernel" array must be already allocated
				upon entry to this routine.
			kernel:  kernel is a "diameter" by "diameter" array of values
				allocated by you, of longs, chars, or Fixeds.  It points to
				the kernel upper left corner.

	iterate
		This invokes a function you specify on a region of pixels in the source
		and dest images.  You give a refcon, and the function is invoked with
		that refcon, plus the x and y coordinates of the current pixel, plus
		pointer to that pixel in the src and dest images.  You can specify a
		rectangle to iterate over (for instance, the extent_hint), or pass NULL
		for the rect param to iterate over every pixel where the worlds overlap.
		If you pass the src world as NULL, this will just iterate over the dst.
		NOTE: This function takes the PF_InData pointer, not just the effect_ref.
		This function will automatically make the progress bar go as it iterates.
		To allow your effect to have the progress bar go across just once and
		still perform multiple iterations, "iterate" starts progress at a base
		number you specify, and goes to that number + the height of the image,
		reporting the progress out of a possible maximum that you also specify.
		Pass the max number as zero to turn off progress reporting.
		This is quality independent.

	premultiply
		To convert to and from having r, g, and b color values
		premultiplied by the pixel alpha value.  High qual same as low qual.

	premultiply_color
		To convert between premul and straight pixel buffers where the
		color channels were matted with a color other than black.
		
	new_world
		This creates a new PF_EffectWorld for scratch for you.
		You must dispose of it.  This is quality independent.
		
	dispose_world
		This disposes a PF_EffectWorld, deallocating pixels, etc.
		Only call it on worlds you have created.  Quality independent.

	get_callback_addr
		Chances are, you will never use this callback.  This is the
		callback to get addresses of callback functions at different
		qualities. See the large comment in the Callback Selectors section.
		You would use this to circumvent the NearestNeighbor behaviour
		of the sampling functions at low quality, if you really needed to.
		Here you can also override the alpha mode, if necessary.
		
		You set the quality here, so this is sort of quality independent.

**/


/**
	prototypes for PF_ITERATE8 and PF_ITERATE16 pixel functions
 	
 	the user of PF_ITERATE provides a refcon; After Effects may distribute iteration
 	across multiple threads, so any data pointed to by refconL should be read-only or
 	properly synchronized
 	
 	inP is writable only if the world you pass to PF_ITERATE() is PF_WorldFlag_WRITEABLE
 	

 **/

typedef PF_Err (*PF_IteratePixel8Func)	(	void* refconP,					/* >> see comment above !! */
											A_long xL,						/* >> */
											A_long yL,						/* >> */
											PF_Pixel *inP,					/* <> */
											PF_Pixel *outP);				/* <> */
											
typedef PF_Err (*PF_IteratePixel16Func)	(	void* refconP,					/* >> see comment above !! */
											A_long xL,						/* >> */
											A_long yL,						/* >> */
											PF_Pixel16 *inP,				/* <> */
											PF_Pixel16 *outP);				/* <> */

typedef PF_Err (*PF_IteratePixelFloatFunc)	(	void* refconP,					/* >> see comment above !! */
												A_long xL,						/* >> */
												A_long yL,						/* >> */
												PF_PixelFloat *inP,				/* <> */
												PF_PixelFloat *outP);				/* <> */


typedef struct _PF_UtilCallbacks {
	PF_Err (*begin_sampling)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Quality		qual,
		PF_ModeFlags	mf,
		PF_SampPB		*params);
		
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

	void *get_batch_func_is_deprecated;	

	PF_Err (*end_sampling)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Quality		qual,
		PF_ModeFlags	mf,
		PF_SampPB		*params);

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
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		const PF_EffectWorld	*src1,
		const PF_EffectWorld	*src2,
		PF_Fixed		ratio,			/* 0 == full src1, 0x00010000 == full src2 */
		PF_EffectWorld	*dst);

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

	PF_Err (*fill)(
		PF_ProgPtr		effect_ref,		/* reference from in_data	*/
		const PF_Pixel	*color,
		const PF_Rect	*dst_rect,		/* pass NULL for whole world */
		PF_EffectWorld	*world);

	PF_Err (*gaussian_kernel)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		A_FpLong			kRadius,		/* desired gaussian radius */
		PF_KernelFlags	flags,			/* see kernel flags commented above */
		A_FpLong			multiplier,
		A_long			*diameter,
		void			*kernel);

	PF_Err (*iterate)(
		PF_InData				*in_data,
		A_long					progress_base,
		A_long					progress_final,
		PF_EffectWorld			*src,
		const PF_Rect			*area,			/* pass NULL for all pixels */
		void*					refcon,
		PF_IteratePixel8Func	pix_fn,
		PF_EffectWorld			*dst);

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

	PF_Err (*new_world)(
		PF_ProgPtr			effect_ref,		/* reference from in_data */
		A_long				width,
		A_long				height,
		PF_NewWorldFlags	flags,			/* should would be pre-cleared to zeroes */
		PF_EffectWorld		*world);		/* always 32 bit */

	PF_Err (*dispose_world)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_EffectWorld		*world);

	PF_Err (*iterate_origin)(
		PF_InData				*in_data,
		A_long					progress_base,
		A_long					progress_final,
		PF_EffectWorld			*src,
		const PF_Rect			*area,			/* pass NULL for all pixels */
		const PF_Point			*origin,
		void*					refcon,
		PF_IteratePixel8Func	pix_fn,
		PF_EffectWorld			*dst);

	PF_Err (*iterate_lut)(
		PF_InData		*in_data,
		A_long			progress_base,
		A_long			progress_final,
		PF_EffectWorld		*src,
		const PF_Rect	*area,			/* pass NULL for all pixels */
		A_u_char	*a_lut0,		/* pass NULL for identity */
		A_u_char	*r_lut0,		/* pass NULL for identity */
		A_u_char	*g_lut0,		/* pass NULL for identity */
		A_u_char	*b_lut0,		/* pass NULL for identity */
		PF_EffectWorld		*dst);

	
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
		
	PF_Handle (*host_new_handle)(
		A_u_longlong			size);

	void * (*host_lock_handle)(
		PF_Handle				pf_handle);

	void (*host_unlock_handle)(
		PF_Handle				pf_handle);

	void (*host_dispose_handle)(
		PF_Handle				pf_handle);

	PF_Err (*get_callback_addr)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_Quality		quality,
		PF_ModeFlags	mode_flags,
		PF_CallbackID	which_callback,
		PF_CallbackFunc	*fn_ptr);

	PF_Err (*app)(PF_ProgPtr, A_long, ...);	/* application specific callback */

	PF_ANSICallbacks	ansi;			/* ANSI callback block, see above */
	PF_ColorCallbacks	colorCB;		/* colorspace conversion callbacks */

	PF_Err (*get_platform_data)(
		PF_ProgPtr		effect_ref,
		PF_PlatDataID	which,
		void			*data);
	
	A_u_longlong (*host_get_handle_size)(
		PF_Handle				pf_handle);

	PF_Err	(*iterate_origin_non_clip_src)(
		PF_InData				*in_data,
		A_long					progress_base,
		A_long					progress_final,
		PF_EffectWorld			*src,
		const PF_Rect			*area,
		const PF_Point			*origin,
		void*					refcon,
		PF_IteratePixel8Func	pix_fn,
		PF_EffectWorld			*dst);

	PF_Err (*iterate_generic)(
		A_long			iterationsL,						/* >> */		// can be PF_Iterations_ONCE_PER_PROCESSOR
		void			*refconPV,							/* >> */
		PF_Err			(*fn_func)(	void *refconPV,			/* >> */
									A_long thread_indexL,		// only call abort and progress from thread_indexL == 0.
									A_long i,
									A_long iterationsL));		// never sends PF_Iterations_ONCE_PER_PROCESSOR

	PF_Err (*host_resize_handle)(
		A_u_longlong		new_sizeL,		/* >> */
		PF_Handle			*handlePH);		/* <> Handle Value May Change */


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

	PF_Err (*fill16)(
		PF_ProgPtr			effect_ref,		/* reference from in_data	*/
		const PF_Pixel16	*color,
		const PF_Rect		*dst_rect,		/* pass NULL for whole world */
		PF_EffectWorld		*world);

	PF_Err (*premultiply_color16)(
		PF_ProgPtr		effect_ref,		/* reference from in_data */
		PF_EffectWorld	*src,
		const PF_Pixel16 *color,			/* color to premultiply/unmultiply with */
		A_long			forward,		/* TRUE means convert non-premul to premul, FALSE mean reverse */
		PF_EffectWorld	*dst);

	PF_Err (*iterate16)(
		PF_InData				*in_data,
		A_long					progress_base,
		A_long					progress_final,
		PF_EffectWorld			*src,
		const PF_Rect			*area,			/* pass NULL for all pixels */
		void*					refcon,
		PF_IteratePixel16Func	pix_fn,
		PF_EffectWorld			*dst);
		
	PF_Err (*iterate_origin16)(
		PF_InData				*in_data,
		A_long					progress_base,
		A_long					progress_final,
		PF_EffectWorld			*src,
		const PF_Rect			*area,			/* pass NULL for all pixels */
		const PF_Point			*origin,
		void*					refcon,
		PF_IteratePixel16Func	pix_fn,
		PF_EffectWorld			*dst);

	PF_Err	(*iterate_origin_non_clip_src16)(
		PF_InData				*in_data,
		A_long					progress_base,
		A_long					progress_final,
		PF_EffectWorld				*src,
		const PF_Rect			*area,
		const PF_Point			*origin,
		void*				refcon,
		PF_IteratePixel16Func	pix_fn,
		PF_EffectWorld				*dst);

	PF_Err	(*get_pixel_data8)(
		PF_EffectWorld	*worldP,
		PF_PixelPtr		pixelsP0,		// NULL to use data in PF_EffectWorld
		PF_Pixel8		**pixPP);		// will return NULL if depth mismatch
		
	PF_Err	(*get_pixel_data16)(
		PF_EffectWorld	*worldP,
		PF_PixelPtr		pixelsP0,		// NULL to use data in PF_EffectWorld
		PF_Pixel16		**pixPP);		// will return NULL if depth mismatch
	
	
										// and we're done. all future expansion
										// is thru suite mechanism. (for example,
										// this is where you'll find the floating
										// pt pixel callbacks)	
	A_long				reserved[1];

} PF_UtilCallbacks;


/** ---------- Callback Access Macros ----------

	Each of these macros _ASSUMES_ that the (PF_InData *) parameter to
	the effects module was passed as a parameter named "in_data".  I know
	this is a heinous assumption, but the template code all declares the
	parameter like that, and by making that assumption, I can simplify
	all these macros very much.  If you absolutely need to change the
	name of that parameter, you will have to pick apart these macros and
	invoke the callbacks by yourself.  It's not too hard...

	For efficiency, most notably with the image resampling functions (i.e.
	subpixel_sample and area_sample), you may wish to declare a local function
	pointer and bypass these macros to avoid the multiple dereferences in
	your inner loop.  The sample code will show how to do this.

	The prototypes and comments about each function are given above in
	the PF_UtilCallbacks structure definition.

**/
#define PF_BEGIN_SAMPLING(QUALITY, PARAMS)	\
	(*in_data->utils->begin_sampling)(	\
		in_data->effect_ref, (QUALITY), PF_MF_Alpha_STRAIGHT, (PARAMS))

#define PF_SUBPIXEL_SAMPLE(X, Y, PARAMS, DST_PXL)	\
	(*in_data->utils->subpixel_sample)(	\
		in_data->effect_ref, (X), (Y), (PARAMS), (DST_PXL))

#define PF_AREA_SAMPLE(X, Y, PARAMS, DST_PXL)	\
	(*in_data->utils->area_sample)(	\
		in_data->effect_ref, (X), (Y), (PARAMS), (DST_PXL))

#define PF_END_SAMPLING(QUALITY, PARAMS)	\
	(*in_data->utils->end_sampling)(	\
		in_data->effect_ref, (QUALITY), PF_MF_Alpha_STRAIGHT, (PARAMS))

#define PF_BLEND(SRC1, SRC2, RATIO, DST)	\
	(*in_data->utils->blend)(	\
		in_data->effect_ref, (SRC1), (SRC2), (RATIO), (DST))

#define PF_CONVOLVE(SRC, RCT_P, FLAGS, KRNL_SZ, AK, RK, GK, BK, DST)	\
	(*in_data->utils->convolve)(	\
		in_data->effect_ref, (SRC), (RCT_P), (FLAGS), (KRNL_SZ), (AK), (RK), (GK), (BK), (DST))

#define PF_COPY(SRC, DST, SRC_RECT, DST_RECT)	\
	(*in_data->utils->copy)(	\
		in_data->effect_ref, (SRC), (DST), (SRC_RECT), (DST_RECT))

#define PF_FILL(COLOR, DST_RECT, DST)	\
	(*in_data->utils->fill)(	\
		in_data->effect_ref, (COLOR), (DST_RECT), (DST))

#define PF_GAUSSIAN_KERNEL(K_RAD, FLAGS, MULT, DIAM, KERNEL)	\
	(*in_data->utils->gaussian_kernel)(	\
		in_data->effect_ref, (K_RAD), (FLAGS), (MULT), (DIAM), (KERNEL))

#define PF_ITERATE(PROG_BASE, PROG_FINAL, SRC, RCT_P, REFCON, PIX_FUNC, DST)	\
	(*in_data->utils->iterate)(	\
		in_data, (PROG_BASE), (PROG_FINAL), (SRC), (RCT_P), (REFCON), (PIX_FUNC), (DST))

#define PF_ITERATE16(PROG_BASE, PROG_FINAL, SRC, RCT_P, REFCON, PIX_FUNC, DST)	\
	(*in_data->utils->iterate16)(	\
		in_data, (PROG_BASE), (PROG_FINAL), (SRC), (RCT_P), (REFCON), (PIX_FUNC), (DST))

#define PF_PREMUL(FORWARD, DST)	\
	(*in_data->utils->premultiply)(	\
		in_data->effect_ref, (FORWARD), (DST))

#define PF_PREMUL_COLOR(SRC, COLOR, FORWARD, DST)	\
	(*in_data->utils->premultiply_color)(	\
		in_data->effect_ref, (SRC), (COLOR), (FORWARD), (DST))

#define PF_NEW_WORLD(WIDTH, HEIGHT, FLAGS, WORLD)	\
	(*in_data->utils->new_world)(	\
		in_data->effect_ref, (WIDTH), (HEIGHT), (FLAGS), (WORLD))

#define PF_DISPOSE_WORLD(WORLD)	\
	(*in_data->utils->dispose_world)(	\
		in_data->effect_ref, (WORLD))

#define PF_ITERATE_ORIGIN(PROG_BASE, PROG_FINAL, SRC, RCT_P, OR, REFCON, PIX_FUNC, DST)	\
	(*in_data->utils->iterate_origin)(	\
		in_data, (PROG_BASE), (PROG_FINAL), (SRC), (RCT_P), (OR), (REFCON), (PIX_FUNC), (DST))
		
#define PF_ITERATE_ORIGIN16(PROG_BASE, PROG_FINAL, SRC, RCT_P, OR, REFCON, PIX_FUNC, DST)	\
	(*in_data->utils->iterate_origin16)(	\
		in_data, (PROG_BASE), (PROG_FINAL), (SRC), (RCT_P), (OR), (REFCON), (PIX_FUNC), (DST))

#define PF_ITERATE_LUT(PROG_BASE, PROG_FINAL, SRC, RCT_P, A_LUT, R_LUT, G_LUT, B_LUT, DST)	\
	(*in_data->utils->iterate_lut)(			\
		in_data, (PROG_BASE), (PROG_FINAL), (SRC), (RCT_P), (A_LUT), 	\
		(R_LUT), (G_LUT), (B_LUT), (DST))

#define PF_TRANSFER_RECT(QUALITY, M_FLAGS, FIELD, SRC_REC, SRC_WORLD, COMP_MODE,	\
						MASK_WORLD_0, DST_X, DST_Y, DST)	\
	(*in_data->utils->transfer_rect)(			\
		in_data->effect_ref, (QUALITY), (M_FLAGS), (FIELD), (SRC_REC), (SRC_WORLD), 	\
		(COMP_MODE), (MASK_WORLD_0), (DST_X), (DST_Y), (DST))

#define PF_TRANSFORM_WORLD(QUALITY, M_FLAGS, FIELD, SRC_WORLD, COMP_MODE,	\
						MASK_WORLD_0, MATRICES, NUM_MATRICES, SRC2DST_MATRIX,	\
						DST_RECT, DST)	\
	(*in_data->utils->transform_world)(			\
		in_data->effect_ref, (QUALITY), (M_FLAGS), (FIELD), (SRC_WORLD), 	\
		(COMP_MODE), (MASK_WORLD_0), (MATRICES), (NUM_MATRICES), (SRC2DST_MATRIX),	\
		(DST_RECT), (DST))

#define PF_ITERATE_ORIGIN_NON_SRC_CLIP(PROG_BASE, PROG_FINAL, SRC, RCT_P, OR, REFCON, PIX_FUNC, DST)	\
	(*in_data->utils->iterate_origin_non_clip_src)(	\
		in_data, (PROG_BASE), (PROG_FINAL), (SRC), (RCT_P), (OR), (REFCON), (PIX_FUNC), (DST))


#define PF_NEW_HANDLE(SIZE) \
    (*in_data->utils->host_new_handle)((SIZE))

#define PF_DISPOSE_HANDLE(PF_HANDLE) \
    (*in_data->utils->host_dispose_handle)((PF_Handle)(PF_HANDLE))

#define PF_LOCK_HANDLE(PF_HANDLE) \
    (*in_data->utils->host_lock_handle)((PF_Handle)(PF_HANDLE))

#define PF_UNLOCK_HANDLE(PF_HANDLE) \
    (*in_data->utils->host_unlock_handle)((PF_Handle)(PF_HANDLE))

#define PF_GET_HANDLE_SIZE(PF_HANDLE) \
    (*in_data->utils->host_get_handle_size)((PF_Handle)(PF_HANDLE))


// Takes a pointer to a handle.  Handle may change.  4.1 and later ONLY.
#define PF_RESIZE_HANDLE(NEW_SIZE, PF_HANDLE_P) \
	(*in_data->utils->host_resize_handle)((NEW_SIZE), (PF_Handle*)(PF_HANDLE_P))



#define PF_GET_PLATFORM_DATA(ID, DATA) \
	(*in_data->utils->get_platform_data)(in_data->effect_ref, (ID), (DATA))

#define PF_GET_PIXEL_DATA8(WORLDP, PIXELPTR0, PIXEL8PP) \
	(*in_data->utils->get_pixel_data8)((WORLDP), (PIXELPTR0), (PIXEL8PP))

#define PF_GET_PIXEL_DATA16(WORLDP, PIXELPTR0, PIXEL16PP) \
	(*in_data->utils->get_pixel_data16)((WORLDP), (PIXELPTR0), (PIXEL16PP))




#define PF_ACOS(X)		(*in_data->utils->ansi.acos)(X)
#define PF_ASIN(X)		(*in_data->utils->ansi.asin)(X)
#define PF_ATAN(X)		(*in_data->utils->ansi.atan)(X)
#define PF_ATAN2(Y, X)	(*in_data->utils->ansi.atan2)((Y), (X))
#define PF_CEIL(X)		(*in_data->utils->ansi.ceil)(X)
#define PF_COS(X)		(*in_data->utils->ansi.cos)(X)
#define PF_EXP(X)		(*in_data->utils->ansi.exp)(X)
#define PF_FABS(X)		(*in_data->utils->ansi.fabs)(X)
#define PF_FLOOR(X)		(*in_data->utils->ansi.floor)(X)
#define PF_FMOD(X, Y)	(*in_data->utils->ansi.fmod)((X), (Y))
#define PF_HYPOT(X, Y)	(*in_data->utils->ansi.hypot)((X), (Y))
#define PF_LOG(X)		(*in_data->utils->ansi.log)(X)
#define PF_LOG10(X)		(*in_data->utils->ansi.log10)(X)
#define PF_POW(X, Y)	(*in_data->utils->ansi.pow)((X), (Y))
#define PF_SIN(X)		(*in_data->utils->ansi.sin)(X)
#define PF_SQRT(X)		(*in_data->utils->ansi.sqrt)(X)
#define PF_TAN(X)		(*in_data->utils->ansi.tan)(X)

/* This is kind of a hack to deal with the varargs params to sprintf */

#define PF_SPRINTF		(*in_data->utils->ansi.sprintf)

#define	PF_STRCPY(DST, SRC)	\
		(*in_data->utils->ansi.strcpy)((DST), (SRC))
		

		
#define PF_RGB_TO_HLS(RGB, HLS) \
	(*in_data->utils->colorCB.RGBtoHLS)(in_data->effect_ref, (RGB), (HLS))
	
#define PF_HLS_TO_RGB(HLS, RGB) \
	(*in_data->utils->colorCB.HLStoRGB)(in_data->effect_ref, (HLS), (RGB))
	
#define PF_RGB_TO_YIQ(RGB, YIQ) \
	(*in_data->utils->colorCB.RGBtoYIQ)(in_data->effect_ref, (RGB), (YIQ))
	
#define PF_YIQ_TO_RGB(YIQ, RGB) \
	(*in_data->utils->colorCB.YIQtoRGB)(in_data->effect_ref, (YIQ), (RGB))
	
#define PF_LUMINANCE(RGB, LUM100) \
	(*in_data->utils->colorCB.Luminance)(in_data->effect_ref, (RGB), (LUM100))
	
#define PF_HUE(RGB, HUE) \
	(*in_data->utils->colorCB.Hue)(in_data->effect_ref, (RGB), (HUE))

#define PF_LIGHTNESS(RGB, LIGHTNESS) \
	(*in_data->utils->colorCB.Lightness)(in_data->effect_ref, (RGB), (LIGHTNESS))
	
#define PF_SATURATION(RGB, SATURATION) \
	(*in_data->utils->colorCB.Saturation)(in_data->effect_ref, (RGB), (SATURATION))
	

	

#ifdef __cplusplus
	}		// end extern "C"
#endif



#include <adobesdk/config/PostConfig.h>


#endif /* _H_AE_EffectCB */
