#ifndef _PR_Public_H
#define _PR_Public_H

/** PR_Public.h
 ** (C)2005 by Adobe Systems Inc.
 **	Public header that defines the API for plug-in renderers
 **
 ** Plugin renderers are known as artisans. They are AEGP modules which register
 ** themselves with the plug-in render manager when they are loaded at 
 ** program startup. 
 ** Very little is passed to these functions, just an opaque context 
 ** and a handle to previously allocated data. The artisan must query 
 ** for other rendering parameters via AEGP suites. The appropriate 
 ** suites will take one of the opaque rendering contexts as one of their 
 ** parameters. 
 **
 ** The entry points are :
 ** PR_GlobalSetupFunc	
 **		Called at artisan load. If necessary allocate global data that will be shared
 **		among all instances of this type of artisan.
 **
 **	PR_GlobalSetdownFunc 
 **		Called at program termination. Delete the global data if any allocated.
 **
 **	PR_GlobalDoAboutFunc 
 **		Display an about box with revelant information about the artisan
 **
 **		
 **	PR_InstanceSetupFunc
 **		An instance of the artisan is associated with each comp. Each instance has its
 **		own data. It should be allocated within this function.
 **
 **	PR_InstanceSetdownFunc
 **		Delete allocated data if needed.
 **		
 **	PR_FlattenInstanceFunc
 **		Return a flattened platform independent version of the instance data.
 **		This is called when the artisan is being written to disk or a copy of the 
 **		artisan is being made. Do not disturb the src instance handle.
 **		
 **	PR_DoInstanceDialogFunc	
 **		Some artisans may have parameters. They may be set here.
 **	
 **	PR_FrameSetupFunc
 **		Called just berfore render. Allocate the render handle, if needed.
 **
 **	PR_RenderFunc
 **		This is the main rendering function. 
 **
 **	PR_FrameSetdownFunc	
 **		Called just after render. Deallocate the render handle if needed.
 **
 **	PR_Query	
 **		This can be called at any time after Instance Setup. It is used by AE
 **		to inquire about geomertic transforms used by the artisan. AE uses this information
 **		to draw layer handles, and manipulate the layers with a mouse better. 
 **
 **/


#include <A.h>
#include <SPBasic.h>
#include <AE_EffectSuites.h>

#pragma pack( push, 4 )




#ifdef __cplusplus
	extern "C" {
#endif



		
#define PR_FileType_ARTISAN			'ARt'
#define PR_ARTISAN_EXTENSION		".aex"

#define PR_ARTISAN_API_VERSION_MAJOR		1
#define PR_ARTISAN_API_VERSION_MINOR		0


#define PR_PUBLIC_MATCH_NAME_LEN			31
#define PR_PUBLIC_ARTISAN_NAME_LEN			31


/** $$$ move to aegp.h ***/
typedef struct _Up_OpaqueMem			**PR_Handle;
typedef PR_Handle						PR_FlatHandle;
									
typedef PR_Handle						PR_GlobalDataH;				// holds data private to the plug-in
typedef PR_Handle						PR_InstanceDataH;
typedef PR_Handle						PR_RenderDataH;

typedef struct PR_GlobalContext			**PR_GlobalContextH;	// opaque until PR.h
typedef struct PR_InstanceContext		**PR_InstanceContextH;	// opaque until PR.h
typedef struct PR_RenderContext			**PR_RenderContextH;	// opaque until PR.h
typedef struct PR_QueryContext			**PR_QueryContextH;		// opaque until PR.h

typedef struct PF_LayerDef	*PF_EffectWorldPtr;
/**
 ** in data is passed to every pr entry function
 **/
typedef void (*PR_MessageFunc) (A_Err err_number, const A_char *msgA);


typedef struct PR_InData {
	PR_MessageFunc				msg_func;
	const struct SPBasicSuite	*pica_basicP;
	A_long						aegp_plug_id;
	void						*aegp_refconPV;
} PR_InData;





/**
 ** response from dialog box function
 **/
enum {
	PR_DialogResult_NO_CHANGE,
	PR_DialogResult_CHANGE_MADE
};
typedef A_long PR_DialogResult;



/**
 ** The types of queries that will be made.
 **
 **/
enum { 
	PR_QueryType_NONE			= 0,
	PR_QueryType_TRANSFORM,
	PR_QueryType_INTERACTIVE_WINDOW_DISPOSE,
	PR_QueryType_INTERACTIVE_WINDOW_CLEAR,
	PR_QueryType_INTERACTIVE_WINDOW_FROZEN_PROXY,
	PR_QueryType_INTERACTIVE_SWAP_BUFFER,
	PR_QueryType_INTERACTIVE_DRAW_PROCS,
	PR_QueryType_PREPARE_FOR_LINE_DRAWING,
	PR_QueryType_UNPREPARE_FOR_LINE_DRAWING,
	PR_QueryType_GET_CURRENT_CONTEXT_SAFE_FOR_LINE_DRAWING,
	PR_QueryType_GET_ARTISAN_QUALITY,
};

typedef A_u_long	PR_QueryType;


// If this sounds interesting, talk to us
enum {
	PR_ArtisanFeature_NONE  = 0
};
typedef A_long PR_ArtisanFeature_Flags;


/**
 ** PR_InstanceSetupFunc flags
 **/
enum {
	PR_InstanceFlags_NONE = 0x0,
	PR_InstanceFlags_DUPLICATE
};

typedef A_u_long PR_InstanceFlags;


/***********************   plugin entry points  *****************************
 ** the main routine of each plugin fills in these function pointers
 ** AE will call them as appropriate
 ****************************************************************************/



/**
 ** called after main. This happens just once, after the plugin is loaded.
 ** The global data is common across all instances of the plugin
 **/
typedef	A_Err	(*PR_GlobalSetupFunc)(	const PR_InData				*in_dataP,				/* >> */
										PR_GlobalContextH			global_contextH,		/* >> */
										PR_GlobalDataH				*global_dataPH);		/* << */

/**
 ** dispose of the global data
 **/
typedef	A_Err	(*PR_GlobalSetdownFunc)(	const PR_InData			*in_dataP,			/* >> */
											PR_GlobalContextH		global_contextH,	/* >> */
											PR_GlobalDataH			global_dataH);		/* <> */ // must be disposed by plugin


/**
 ** display an about box
 **/
typedef A_Err	(*PR_GlobalDoAboutFunc)(	const PR_InData			*in_dataP,			/* >> */
											PR_GlobalContextH		global_contextH,	/* >> */
											PR_GlobalDataH			global_dataH);		/* <> */



/**
 ** Analogous to an Effect's Sequence setup call. This sets up the renderer's
 ** instance data. 
 **/
typedef	A_Err	(*PR_InstanceSetupFunc)(
										const PR_InData			*in_dataP,					/* >> */
										PR_GlobalContextH		global_contextH,			/* >> */
										PR_InstanceContextH		instance_contextH,			/* >> */
										PR_GlobalDataH			global_dataH,				/* >> */
										PR_InstanceFlags		flags,
										PR_FlatHandle			flat_dataH0,				/* >> */
										PR_InstanceDataH		*instance_dataPH);			/* << */


/**
 ** dispose of the instance data
 **/
typedef	A_Err	(*PR_InstanceSetdownFunc)(
										const PR_InData				*in_dataP,			/* >> */
										const PR_GlobalContextH		global_contextH,		/* >> */
										const PR_InstanceContextH	instance_contextH,		/* >> */
										PR_GlobalDataH				global_dataH,			/* >> */
										PR_InstanceDataH			instance_dataH);		/* >> */ // must be disposed by plugin



/**
 ** flatten your data in preparation to being written to disk.
 ** Make sure its OS independent
 **/
typedef	A_Err	(*PR_FlattenInstanceFunc)(
										const PR_InData			*in_dataP,			/* >> */
										PR_GlobalContextH		global_contextH,		/* >> */
										PR_InstanceContextH		instance_contextH,		/* >> */
										PR_GlobalDataH			global_dataH,			/* <> */
										PR_InstanceDataH		instance_dataH,			/* <> */
										PR_FlatHandle			*flatH);				/* << */





/**
 ** if the renderer has parameters, this is where they get set or changed.
 **/


typedef	A_Err	(*PR_DoInstanceDialogFunc)(		const PR_InData		*in_dataP,			/* >> */
												PR_GlobalContextH	global_contextH,		/* >> */
												PR_InstanceContextH	instance_contextH,		/* >> */
												PR_GlobalDataH		global_dataH,			/* <> */
												PR_InstanceDataH	instance_dataH,			/* <> */
												PR_DialogResult		*resultP);				/* << */
			


/** 
 ** allocate render data if needed
 **/
typedef A_Err	(*PR_FrameSetupFunc)(
										const PR_InData			*in_dataP,				/* >> */
										PR_GlobalContextH		global_contextH,			/* >> */
										PR_InstanceContextH		instance_contextH,			/* >> */
										PR_RenderContextH		render_contextH,			/* >> */
										PR_GlobalDataH			global_dataH,				/* <> */
										PR_InstanceDataH		instance_dataH,				/* <> */
										PR_RenderDataH			*render_dataPH);			/* << */


/** 
 ** deallocate render data
 **/
typedef A_Err	(*PR_FrameSetdownFunc)(
										const PR_InData			*in_dataP,				/* >> */
										PR_GlobalContextH		global_contextH,			/* >> */
										PR_InstanceContextH		instance_contextH,			/* >> */
										PR_RenderContextH		render_contextH,			/* >> */
										PR_GlobalDataH			global_dataH,				/* <> */
										PR_InstanceDataH		instance_dataH,				/* <> */
										PR_RenderDataH			render_dataH);	


/** 
 ** the main drawing routine 
 **/
typedef A_Err	(*PR_RenderFunc)(
										const PR_InData			*in_dataP,					/* >> */
										PR_GlobalContextH		global_contextH,			/* >> */
										PR_InstanceContextH		instance_contextH,			/* >> */
										PR_RenderContextH		render_contextH,			/* >> */
										PR_GlobalDataH			global_dataH,				/* <> */
										PR_InstanceDataH		instance_dataH,				/* <> */
										PR_RenderDataH			render_dataH);




/**
 ** AE will need to have the artisan process data on its behalf such as 
 ** projecting points to the screen, transforming axis, etc. This routine will handle
 ** it all
 **/
typedef A_Err	(*PR_QueryFunc)(	const PR_InData			*in_dataP,			/* >> */
									PR_GlobalContextH		global_contextH,	/* >> */
									PR_InstanceContextH		instance_contextH,  /* >> */
									PR_QueryContextH		query_contextH,		/* <> */
									PR_QueryType			query_type,			/* >> */
									PR_GlobalDataH			global_data,		/* >> */
									PR_InstanceDataH		instance_dataH);	/* >> */
									



/**
 ** main fills this in, just once at plugin load time
 ** These are the entry points that AE calls to use an artisan.
 **/

typedef struct {

	PR_GlobalSetupFunc					global_setup_func0;
	PR_GlobalSetdownFunc				global_setdown_func0;
	PR_GlobalDoAboutFunc				global_do_about_func0;

	PR_InstanceSetupFunc				setup_instance_func0;
	PR_InstanceSetdownFunc				setdown_instance_func0;
	PR_FlattenInstanceFunc				flatten_instance_func0;
	PR_DoInstanceDialogFunc				do_instance_dialog_func0;
	
	PR_FrameSetupFunc					frame_setup_func0;
	PR_RenderFunc						render_func;			// must have at least this one function
	PR_FrameSetdownFunc					frame_setdown_func0;

	PR_QueryFunc						query_func0;
	
} PR_ArtisanEntryPoints;




/** 
 ** line drawing routines for interactive artisans
 **/
typedef void	(*PR_Draw_MoveToFunc)(A_FpLong x, A_FpLong y);

typedef void	(*PR_Draw_LineToFunc)(A_FpLong x, A_FpLong y);

typedef void	(*PR_Draw_LineRelFunc)(A_FpLong dx, A_FpLong dy);

typedef void	(*PR_Draw_ForeColorFunc)(const A_Color *fore_color);

typedef void	(*PR_Draw_BackColorFunc)(const A_Color *fore_color);

typedef void	(*PR_Draw_FrameRectFunc)(const A_FloatRect *rectPR );

typedef void	(*PR_Draw_PaintRectFunc)(const A_FloatRect *rectPR );

typedef void	(*PR_Draw_FrameOvalFunc)(const A_FloatRect *rectPR );

typedef void	(*PR_Draw_PaintOvalFunc)(const A_FloatRect *rectPR );

typedef void	(*PR_Draw_InvertRectFunc)(const A_FloatRect *rectPR );

typedef void	(*PR_Draw_SetClipFunc)(const A_FloatRect *rectPR, A_Boolean invertB );

typedef void	(*PR_Draw_PenNormal)(void);

typedef void	(*PR_Draw_PenSize)(A_FpLong widthS, A_FpLong heightS);

typedef void	(*PR_Draw_PenPat)(A_u_char pattern);

typedef void	(*PR_Draw_Invert)(A_Boolean);

typedef void	(*PR_CacheIconFunc)(PF_EffectWorldPtr iconP);

typedef void	(*PR_DrawCachedIconFunc)(A_long x, A_long y);

typedef void	(*PR_DrawStringFunc)(const A_UTF16Char *nameZ, PF_FontStyleSheet style, const A_Color *fore_colorP, const A_Color *shadow_colorP, const A_FloatPoint *originP, const A_FloatPoint *shadow_offsetP);

typedef void	(*PR_StrokePolyFunc) (A_long nptsL, A_FloatPoint *ptsA);

typedef void	(*PR_PaintPolyFunc) (A_long nptsL, A_FloatPoint *ptsA);



typedef struct {

	PR_Draw_MoveToFunc				move_to_func;
	PR_Draw_LineToFunc				line_to_func;
	PR_Draw_LineRelFunc				line_rel_func;
	PR_Draw_ForeColorFunc			fore_color_func;
	PR_Draw_BackColorFunc			back_color_func;
	PR_Draw_FrameRectFunc			frame_rect_func;
	PR_Draw_PaintRectFunc			paint_rect_func;
	PR_Draw_FrameOvalFunc			frame_oval_func;
	PR_Draw_PaintOvalFunc			paint_oval_func;
	PR_Draw_InvertRectFunc			invert_rect_func;
	PR_Draw_SetClipFunc				set_clip_func;
	PR_Draw_PenNormal				pen_normal_func;
	PR_Draw_PenSize					pen_size_func;
	PR_Draw_PenPat					pen_pat_func;
	PR_Draw_Invert					invert_func;
	PR_CacheIconFunc				cache_icon_func;
	PR_DrawCachedIconFunc			draw_cached_icon_func;
	PR_DrawStringFunc				draw_string_func;
	PR_StrokePolyFunc				stroke_poly_func;
	PR_PaintPolyFunc				paint_poly_func;

} PR_InteractiveDrawProcs;





#pragma pack( pop )



#ifdef __cplusplus
	}
#endif

							
#endif
