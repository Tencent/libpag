/**************************************************************************
*
* ADOBE CONFIDENTIAL
* ___________________
*
*  Copyright 2009 Adobe Systems Incorporated
*  All Rights Reserved.
*
* NOTICE:  All information contained herein is, and remains the property of
* Adobe Systems Incorporated  and its suppliers,  if any.  The intellectual 
* and technical concepts contained herein are proprietary to  Adobe Systems 
* Incorporated  and its suppliers  and may be  covered by U.S.  and Foreign 
* Patents,patents in process,and are protected by trade secret or copyright 
* law.  Dissemination of this  information or reproduction of this material
* is strictly  forbidden  unless prior written permission is  obtained from 
* Adobe Systems Incorporated.
**************************************************************************/

#ifndef DRAWBOT_SUITE_H
#define DRAWBOT_SUITE_H

#include <adobesdk/config/PreConfig.h>

#ifdef __cplusplus
	#include <exception> // for std::exception
#endif
//Sweet pea header
#include <SPBasic.h>

#ifdef ADOBE_SDK_INTERNAL		
	#include <adobesdk/private/drawbotsuite/DrawbotSuiteTypes.h>
#else
	#include <adobesdk/drawbotsuite/DrawbotSuiteTypes.h>
#endif

#include <adobesdk/config/AdobesdkTypes.h>

#ifdef __cplusplus
	extern "C" {
#endif
	
/*
C STYLE
--------
Drawbot suites can be used to draw paths, strings, images using application-provided DrawbotRef. Use C++ style (mentioned later in the file)
if you are creating cpp files as it is more elegant and you don't have to deal with retain/release of objects.

Below function takes DrawbotRef and strokes a circle & a rectangle.

void DrawSomething(DRAWBOT_DrawRef drawbot_ref)
{
	//Acquire drawbot (drawbot_suiteP), supplier (supplier_suiteP), surface (surface_suiteP) & path (path_suiteP) suites
	...

	//Get the supplier and surface reference from drawbot_ref
	DRAWBOT_SupplierRef		supplier_ref;
	DRAWBOT_SurfaceRef		surface_ref;

	drawbot_suiteP->GetSupplier(drawbot_ref, &supplier_ref);
	drawbot_suiteP->GetSurface(drawbot_ref, &surface_ref);

	//Save the surface state by pushing it in stack. It is required to restore state if you are going to clip/transform surface
	//or change interpolation/anti-aliasing policy.
	surface_suiteP->PushStateStack(surface_ref);

	//Create a new red-color brush.
	DRAWBOT_BrushRef	brush_ref;

	supplier_suiteP->NewBrush(supplier_ref, DRAWBOT_ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), &brush_ref);

	//Create a new path
	DRAWBOT_PathRef		path_ref;

	supplier_suiteP->NewPath(supplier_ref, &path_ref);

	//Add a rectangle to the path
	DRAWBOT_RectF32		rect1(0.0f, 0.0f, 50.0f, 50.0f);

	path_suiteP->AddRect(path_ref, rect1);

	//Add a circle to the path
	path_suiteP->AddArc(path_ref, DRAWBOT_PointF32(25.0f, 25.0f), 10.0f, 0.0f, 360.0f);

	//Fill the path using red-colored brush.
	surface_suiteP->FillPath(surface_ref, brush_ref, path_ref, kDRAWBOT_FillType_EvenOdd);

	//We are done. Release (delete) all the objects. There will be memory leak if you don't release.
	surface_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path_ref);
	surface_suiteP->ReleaseObject((DRAWBOT_ObjectRef)brush_ref);

	//Don't forget to pop surface state (if pushed earlier)
	surface_suiteP->PopStateStack(surface_ref);

	//DO NOT CALL ReleaseObject on surface_ref and supplier_ref as you did not create them.

	//Release drawbot (drawbot_suiteP), supplier (supplier_suiteP), surface (surface_suiteP) & path (path_suiteP) suites
}
*/


#define kDRAWBOT_DrawSuite					"DRAWBOT Draw Suite"
#define kDRAWBOT_DrawSuite_Version1			1

#define kDRAWBOT_DrawSuite_VersionCurrent	kDRAWBOT_DrawSuite_Version1
#define	DRAWBOT_DrawbotSuiteCurrent			DRAWBOT_DrawbotSuite1

typedef struct DRAWBOT_DrawbotSuite1 {
	
	//Get the supplier from the drawbot_ref
	SPAPI SPErr	(*GetSupplier)(	DRAWBOT_DrawRef			in_drawbot_ref,
								DRAWBOT_SupplierRef		*out_supplierP);

	//Get the surface from the drawbot_ref
	SPAPI SPErr	(*GetSurface)(	DRAWBOT_DrawRef			in_drawbot_ref,
								DRAWBOT_SurfaceRef		*out_surfaceP);

} DRAWBOT_DrawbotSuite1;



#define kDRAWBOT_SupplierSuite						"DRAWBOT Supplier Suite"
#define kDRAWBOT_SupplierSuite_Version1				1

#define kDRAWBOT_SupplierSuite_VersionCurrent		kDRAWBOT_SupplierSuite_Version1
#define DRAWBOT_SupplierSuiteCurrent				DRAWBOT_SupplierSuite1

typedef struct DRAWBOT_SupplierSuite1 {

	//Create a new pen.
	//It should be released with ReleaseObject api.
	SPErr	(*NewPen)(	DRAWBOT_SupplierRef				in_supplier_ref,
						const DRAWBOT_ColorRGBA			*in_colorP,
						float							in_size,
						DRAWBOT_PenRef					*out_penP);

	//Create a new brush. 
	//It should be released with ReleaseObject api.
	SPErr	(*NewBrush)(DRAWBOT_SupplierRef				in_supplier_ref,
						const DRAWBOT_ColorRGBA			*in_colorP,
						DRAWBOT_BrushRef				*out_brushP);
	
	//Check if current supplier supports text.
	SPErr	(*SupportsText)(DRAWBOT_SupplierRef			in_supplier_ref,
							DRAWBOT_Boolean				*out_supports_textPB);
	
	//Get default font size.
	SPErr	(*GetDefaultFontSize)(	DRAWBOT_SupplierRef			in_supplier_ref,
									float						*out_font_sizeF);

	//Create a new font with default settings.
	//You can pass default font size from GetDefaultFontSize.
	//It should be released with ReleaseObject api.
	SPErr	(*NewDefaultFont)(	DRAWBOT_SupplierRef				in_supplier_ref,
								float							in_font_sizeF,
								DRAWBOT_FontRef					*out_fontP);

	//Create a new image from buffer. 
	//It should be released with ReleaseObject api.
	SPErr	(*NewImageFromBuffer)(	DRAWBOT_SupplierRef		in_supplier_ref,
									int						in_width,
									int						in_height,
									int						in_row_bytes,
									DRAWBOT_PixelLayout		in_pl,
									const void				*in_dataP,
									DRAWBOT_ImageRef		*out_imageP);

	//Create a new path. 
	//It should be released with ReleaseObject api.
	SPErr	(*NewPath)(	DRAWBOT_SupplierRef				in_supplier_ref,
						DRAWBOT_PathRef					*out_pathP);


	//A given drawbot implementation can support multiple channel orders, but will likely
	//prefer one over the other. Use below apis to get the preferred layout for any api that 
	//takes DRAWBOT_PixelLayout (ex- NewImageFromBuffer).
		
	SPErr	(*SupportsPixelLayoutBGRA)(	DRAWBOT_SupplierRef			in_supplier_ref,
										DRAWBOT_Boolean				*out_supports_bgraPB);

	SPErr	(*PrefersPixelLayoutBGRA) (	DRAWBOT_SupplierRef			in_supplier_ref,
										DRAWBOT_Boolean				*out_prefers_bgraPB);

	SPErr	(*SupportsPixelLayoutARGB)(	DRAWBOT_SupplierRef			in_supplier_ref,
										DRAWBOT_Boolean				*out_supports_argbPB);
	
	SPErr	(*PrefersPixelLayoutARGB) (	DRAWBOT_SupplierRef			in_supplier_ref,
										DRAWBOT_Boolean				*out_prefers_argbPB);

	//Retain (increase reference count) any object (pen, brush, path etc)
	//Ex: It should be used when any object is copied and the copied object should be retained.
	SPErr	(*RetainObject)(DRAWBOT_ObjectRef			in_obj_ref);

	//Release (decrease reference count) any object (pen, brush, path etc)
	//This function MUST be called for any object created using NewXYZ() from this suite.
	SPErr	(*ReleaseObject)(DRAWBOT_ObjectRef			in_obj_ref);

} DRAWBOT_SupplierSuite1;


#define kDRAWBOT_SurfaceSuite					"DRAWBOT Surface Suite"
#define kDRAWBOT_SurfaceSuite_Version1			1
#define kDRAWBOT_SurfaceSuite_Version2			2

#define kDRAWBOT_SurfaceSuite_VersionCurrent	kDRAWBOT_SurfaceSuite_Version2
#define DRAWBOT_SurfaceSuiteCurrent				DRAWBOT_SurfaceSuite2

typedef struct DRAWBOT_SurfaceSuite1 {

	//Push the current surface state in stack. It should be popped to retrieve old state.
	SPErr	(*PushStateStack)( DRAWBOT_SurfaceRef		in_surface_ref);

	//Pop the last pushed surface state.
	SPErr	(*PopStateStack)( DRAWBOT_SurfaceRef		in_surface_ref);

	//Paint a rectangle with a color on the surface.
	SPErr	(*PaintRect)(	DRAWBOT_SurfaceRef			in_surface_ref,
							const DRAWBOT_ColorRGBA		*in_colorP,	
							const DRAWBOT_RectF32		*in_rectPR);

	//Fill a path using a brush and fill type.
	SPErr	(*FillPath)(	DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_BrushRef			in_brush_ref,
							DRAWBOT_PathRef				in_path_ref,
							DRAWBOT_FillType			in_fill_type);

	//Stroke a path using a pen.
	SPErr	(*StrokePath)(	DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_PenRef				in_pen_ref,
							DRAWBOT_PathRef				in_path_ref);

	//Clip the surface
	SPErr	(*Clip)(		DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_SupplierRef			in_supplier_ref,
							const DRAWBOT_Rect32		*in_rectPR);

	//Get clip bounds
	SPErr	(*GetClipBounds)(
							DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_Rect32				*out_rectPR);

	//Checks whether a rect is within the clip bounds.
	SPErr	(*IsWithinClipBounds)(
							DRAWBOT_SurfaceRef			in_surface_ref,
							const DRAWBOT_Rect32		*in_rectPR,
							DRAWBOT_Boolean				*out_withinPB);

	//Transform the last surface state.
	SPErr	(*Transform)(	DRAWBOT_SurfaceRef			in_surface_ref,
							const DRAWBOT_MatrixF32		*in_matrixP);

	//Draw a string
	SPErr	(*DrawString)(	DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_BrushRef			in_brush_ref,
							DRAWBOT_FontRef				in_font_ref,
							const DRAWBOT_UTF16Char		*in_stringP,
							const DRAWBOT_PointF32		*in_originP,
							DRAWBOT_TextAlignment		in_alignment_style, 
							DRAWBOT_TextTruncation		in_truncation_style,
							float						in_truncation_width);

	//Draw an image (created using NewImageFromBuffer())on the surface
	//alpha = [0.0f, 1.0f]
	SPErr	(*DrawImage)(	DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_ImageRef			in_image_ref,
							const DRAWBOT_PointF32		*in_originP,
							float						in_alpha);

	SPErr	(*SetInterpolationPolicy)(	
							DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_InterpolationPolicy in_interp);

	SPErr	(*GetInterpolationPolicy)(	
							DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_InterpolationPolicy *out_interpP);

	SPErr	(*SetAntiAliasPolicy)(	
							DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_AntiAliasPolicy		in_policy);

	SPErr	(*GetAntiAliasPolicy)(	
							DRAWBOT_SurfaceRef			in_surface_ref,
							DRAWBOT_AntiAliasPolicy		*out_policyP);

	//Flush drawing
	SPErr	(*Flush)(		DRAWBOT_SurfaceRef			in_surface_ref);

	//Get the transform of the surface state.
	SPErr	(*GetTransformToScreenScale)(DRAWBOT_SurfaceRef	in_surface_ref,
										 float* out_scale);
} DRAWBOT_SurfaceSuite2;

typedef DRAWBOT_SurfaceSuite2 DRAWBOT_SurfaceSuite1; // At the time, the last function was Flush.


#define kDRAWBOT_PathSuite					"DRAWBOT Path Suite"
#define kDRAWBOT_PathSuite_Version1			1

#define kDRAWBOT_PathSuite_VersionCurrent	kDRAWBOT_PathSuite_Version1
#define DRAWBOT_PathSuiteCurrent			DRAWBOT_PathSuite1

typedef struct DRAWBOT_PathSuite1 {

	//Move to a point
	SPErr	(*MoveTo)(	DRAWBOT_PathRef				in_path_ref,
						float						in_x,
						float						in_y);
	
	//Add a line to the path
	SPErr	(*LineTo)(	DRAWBOT_PathRef				in_path_ref,
						float						in_x,
						float						in_y);
	
	//Add a cubic bezier to the path
	SPErr	(*BezierTo)(DRAWBOT_PathRef				in_path_ref,
						const DRAWBOT_PointF32		*in_pt1P,
						const DRAWBOT_PointF32		*in_pt2P,
						const DRAWBOT_PointF32		*in_pt3P);
	
	//Add a rect to the path 
	SPErr	(*AddRect)(	DRAWBOT_PathRef				in_path_ref,
						const DRAWBOT_RectF32		*in_rectPR);

	//Add a arc to the path
	//zero start degrees == 3 o'clock
	//sweep is clockwise
	//units in degrees
	SPErr	(*AddArc)(	DRAWBOT_PathRef				in_path_ref,
						const DRAWBOT_PointF32		*in_centerP,
						float						in_radius,
						float						in_start_angle,
						float						in_sweep);
	
	SPErr	(*Close)(	DRAWBOT_PathRef				in_path_ref);

} DRAWBOT_PathSuite1;


#define kDRAWBOT_PenSuite					"DRAWBOT Pen Suite"
#define kDRAWBOT_PenSuite_Version1			1

#define kDRAWBOT_PenSuite_VersionCurrent	kDRAWBOT_PenSuite_Version1
#define DRAWBOT_PenSuiteCurrent				DRAWBOT_PenSuite1

typedef struct DRAWBOT_PenSuite1 {

	//Make the line dashed
	SPErr	(*SetDashPattern)(	DRAWBOT_PenRef		in_pen_ref,
								const float			*in_dashesP,
								int					in_pattern_size);

} DRAWBOT_PenSuite1;

#define kDRAWBOT_ImageSuite					"DRAWBOT Image Suite"
#define kDRAWBOT_ImageSuite_Version1		1

#define kDRAWBOT_ImageSuite_VersionCurrent	kDRAWBOT_ImageSuite_Version1
#define DRAWBOT_ImageSuiteCurrent			DRAWBOT_ImageSuite1

typedef struct DRAWBOT_ImageSuite1 {

	//Make the line dashed
	SPErr	(*SetScaleFactor)(	DRAWBOT_ImageRef	in_image_ref,
								float				in_scale_factor);

} DRAWBOT_ImageSuite1;

//Collection of latest drawbot suites
typedef struct {
	DRAWBOT_DrawbotSuiteCurrent				*drawbot_suiteP;
	DRAWBOT_SupplierSuiteCurrent			*supplier_suiteP;
	DRAWBOT_SurfaceSuiteCurrent				*surface_suiteP;
	DRAWBOT_PathSuiteCurrent				*path_suiteP;
	DRAWBOT_PenSuiteCurrent					*pen_suiteP;
	DRAWBOT_ImageSuiteCurrent				*image_suiteP;
} DRAWBOT_Suites;


#ifdef __cplusplus
	}		// end extern "C"
#endif



#ifdef __cplusplus	//C++ Style

/*
Use C++ style as it will automatically deal with memory management of objects. Below example will make it clear.

Below function takes DrawbotRef and strokes a circle & a rectangle.

void DrawSomething(DRAWBOT_DrawRef drawbot_ref)
{
	//Acquire drawbot (drawbot_suiteP), supplier (supplier_suiteP), surface (surface_suiteP) & path (path_suiteP) suites
	...

	//Get the supplier and surface reference from drawbot_ref
	DRAWBOT_SupplierRef		supplier_ref;
	DRAWBOT_SurfaceRef		surface_ref;

	drawbot_suiteP->GetSupplier(drawbot_ref, &supplier_ref);
	drawbot_suiteP->GetSurface(drawbot_ref, &surface_ref);

	//Save the surface state by pushing it in stack. It is required to restore if you are going to clip or transform surface.
	//Below call acts like a a scoper: it will push the current surface state and automatically pop at the end of scope.
	DRAWBOT_SaveAndRestoreStateStack		sc_save_state(surface_suiteP, surface_ref);

	//Create a new red-color brush.
	//Compare below call with C-style apis. It is compact and brush will be automatically released at the end of scope.
	DRAWBOT_BrushP		brushP(supplier_suiteP, supplier_ref, DRAWBOT_ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
	
	//Create a new path
	DRAWBOT_PathP		pathP(supplier_suiteP, supplier_ref);

	//Add a rectangle to the path
	DRAWBOT_RectF32		rect1(0.0f, 0.0f, 50.0f, 50.0f);

	path_suiteP->AddRect(pathP, rect1);

	//Add a circle to the path
	path_suiteP->AddArc(pathP, DRAWBOT_PointF32(25.0f, 25.0f), 10.0f, 0.0f, 360.0f);

	//Fill the path using red-colored brush.
	surface_suiteP->FillPath(surface_ref, brushP, pathP, kDRAWBOT_FillType_EvenOdd);

	//Release drawbot (drawbot_suiteP), supplier (supplier_suiteP), surface (surface_suiteP) & path (path_suiteP) suites

	//We are DONE. You don't have to release brush & path objects as you did in C-style.
}
*/
class DRAWBOT_Exception : public std::exception {
public:
		DRAWBOT_Exception(SPErr err): mErr(err)
		{
		}
		virtual ~DRAWBOT_Exception() throw () {}

		SPErr mErr;
};

#define DRAWBOT_ErrorToException(EXPR)		{ SPErr _err = (EXPR); if (_err) throw DRAWBOT_Exception(_err); }


//SharedRefImpl declaration

template<typename REF_T>
class SharedRefImpl {
public:
	SharedRefImpl(DRAWBOT_SupplierSuiteCurrent *suiteP, REF_T r = 0, bool retainB = true)
		:
		mRef(r),
		mSuiteP(suiteP)
	{
		if (retainB) {
			RetainObject();
		}
	}

	virtual ~SharedRefImpl()
	{
		ReleaseObject();
	}

	//copy constructor
	SharedRefImpl(SharedRefImpl const & rhs)
	{
		*this = rhs;
	}

	//assignment operator
	SharedRefImpl & operator=(SharedRefImpl const & rhs)
	{
		mSuiteP = rhs.mSuiteP;
		mRef = rhs.mRef;

		RetainObject();

		return *this;
	}

	inline REF_T Get() const
	{
		return mRef;
	}

	inline operator REF_T() const
	{ 
		return mRef;
	}

	inline void Reset(REF_T rhs = 0, bool retainB = true)
	{
		if (mRef != rhs) {
			ReleaseObject();

			mRef = rhs;

			if (retainB) {
				RetainObject();
			}
		}
	}

private:
	void ReleaseObject()
	{
		if (mRef) {
			DRAWBOT_ErrorToException(mSuiteP->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(mRef)));
			mRef = NULL;
		}
	}

	void RetainObject()
	{
		if (mRef) {
			DRAWBOT_ErrorToException(mSuiteP->RetainObject(reinterpret_cast<DRAWBOT_ObjectRef>(mRef)));
		}
	}

	REF_T								mRef;
	DRAWBOT_SupplierSuiteCurrent		*mSuiteP;
};


//DRAWBOT_PenP declaration

class DRAWBOT_PenP : public SharedRefImpl<DRAWBOT_PenRef> {
	typedef SharedRefImpl<DRAWBOT_PenRef>	_inherited;
public:
	
	DRAWBOT_PenP(	DRAWBOT_SupplierSuiteCurrent	*suiteP, 
					const DRAWBOT_SupplierRef		supplier_ref, 
					const DRAWBOT_ColorRGBA			*colorP, 
					float							sizeF)
		:
		_inherited(suiteP)
	{
		New(suiteP, supplier_ref, colorP, sizeF);
	}

	void New(	DRAWBOT_SupplierSuiteCurrent	*suiteP, 
				const DRAWBOT_SupplierRef		supplier_ref, 
				const DRAWBOT_ColorRGBA			*colorP, 
				float							sizeF)
	{
		DRAWBOT_PenRef	pen_ref;

		DRAWBOT_ErrorToException(suiteP->NewPen(supplier_ref, colorP, sizeF, &pen_ref));

		Reset(pen_ref, false);
	}
};


//DRAWBOT_PathP declaration

class DRAWBOT_PathP : public SharedRefImpl<DRAWBOT_PathRef> {
	typedef SharedRefImpl<DRAWBOT_PathRef>	_inherited;
public:
	DRAWBOT_PathP(	DRAWBOT_SupplierSuiteCurrent	*suiteP, 
					const DRAWBOT_SupplierRef		supplier_ref)
		:
		_inherited(suiteP)
	{
		New(suiteP, supplier_ref);
	}

	void New(	DRAWBOT_SupplierSuiteCurrent	*suiteP, 
				const DRAWBOT_SupplierRef		supplier_ref)
	{
		DRAWBOT_PathRef	path_ref;

		DRAWBOT_ErrorToException(suiteP->NewPath(supplier_ref, &path_ref));

		Reset(path_ref, false);
	}
};


//DRAWBOT_BrushP declaration

class DRAWBOT_BrushP : public SharedRefImpl<DRAWBOT_BrushRef> {
	typedef SharedRefImpl<DRAWBOT_BrushRef>	_inherited;
public:
	DRAWBOT_BrushP(	DRAWBOT_SupplierSuiteCurrent	*suiteP, 
					const DRAWBOT_SupplierRef		supplier_ref, 
					const DRAWBOT_ColorRGBA			*colorP)
		:
		_inherited(suiteP)
	{
		New(suiteP, supplier_ref, colorP);
	}

	void New(	DRAWBOT_SupplierSuiteCurrent	*suiteP, 
				const DRAWBOT_SupplierRef		supplier_ref, 
				const DRAWBOT_ColorRGBA			*colorP)
	{
		DRAWBOT_BrushRef	brush_ref;

		DRAWBOT_ErrorToException(suiteP->NewBrush(supplier_ref, colorP, &brush_ref));

		Reset(brush_ref, false);
	}
};


//DRAWBOT_FontP declaration

class DRAWBOT_FontP : public SharedRefImpl<DRAWBOT_FontRef> {
	typedef SharedRefImpl<DRAWBOT_FontRef>	_inherited;
public:
	DRAWBOT_FontP(	DRAWBOT_SupplierSuiteCurrent	*suiteP, 
					const DRAWBOT_SupplierRef		supplier_ref, 
					float							in_font_size)
		:
	_inherited(suiteP)
	{
		New(suiteP, supplier_ref, in_font_size);
	}

	void New(	DRAWBOT_SupplierSuiteCurrent	*suiteP, 
				const DRAWBOT_SupplierRef		supplier_ref, 
				float							in_font_size)
	{
		DRAWBOT_FontRef	font_ref;

		DRAWBOT_ErrorToException(suiteP->NewDefaultFont(supplier_ref, in_font_size, &font_ref));

		Reset(font_ref, false);
	}
};


class DRAWBOT_SaveAndRestoreStateStack {
public:
	DRAWBOT_SaveAndRestoreStateStack(	DRAWBOT_SurfaceSuiteCurrent		*suiteP, 
										const DRAWBOT_SurfaceRef		surface_ref) 
			: 
			mSurfaceRef(surface_ref),
			mSuiteP(suiteP) 
	{
		DRAWBOT_ErrorToException(mSuiteP->PushStateStack(mSurfaceRef));
	}

	~DRAWBOT_SaveAndRestoreStateStack() 
	{ 
		(void)mSuiteP->PopStateStack(mSurfaceRef); 
	}

private:
	DRAWBOT_SurfaceRef				mSurfaceRef;
	DRAWBOT_SurfaceSuiteCurrent		*mSuiteP;
};


#endif	//C++ Style


#include <adobesdk/config/PostConfig.h>

#endif //DRAWBOT_SUITE_H
