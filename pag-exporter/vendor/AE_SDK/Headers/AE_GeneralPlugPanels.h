#ifndef _AEGP_PANELS_H_
#define _AEGP_PANELS_H_

#include "AE_GeneralPlug.h"

#if defined(__GNUC__) && defined(__MACH__)
	#if defined(__LP64__)
		#include <Cocoa/Cocoa.h>
		typedef NSView *AEGP_PlatformViewRef;
	#else
		typedef HIViewRef AEGP_PlatformViewRef;
	#endif
#endif

#if defined(WIN32)
	#include <windows.h>
	typedef HWND AEGP_PlatformViewRef;
#endif

typedef struct _AEGP_CreatePanelRefcon			*AEGP_CreatePanelRefcon;
typedef struct _AEGP_PanelRefcon			*AEGP_PanelRefcon;

#ifndef AEGP_INTERNAL
	// opaque for everyone else
	typedef struct _AEGP_PanelH						*AEGP_PanelH;
#endif



#include <AE_GeneralPlugPre.h>


enum {AEGP_FlyoutMenuCmdID_NONE = 0};
typedef A_long AEGP_FlyoutMenuCmdID;


enum {
	AEGP_FlyoutMenuMarkType_NORMAL,
	AEGP_FlyoutMenuMarkType_CHECKED,
	AEGP_FlyoutMenuMarkType_RADIO_BULLET,
	AEGP_FlyoutMenuMarkType_SEPARATOR
};
typedef A_long AEGP_FlyoutMenuMarkType;

typedef A_long AEGP_FlyoutMenuIndent;


typedef struct 
{
	AEGP_FlyoutMenuIndent	indent;
	AEGP_FlyoutMenuMarkType	type;
	A_Boolean				enabledB;
	AEGP_FlyoutMenuCmdID	cmdID;		// limited to MAX(A_long) - 201;
	const A_u_char*			utf8NameZ;
}AEGP_FlyoutMenuItem;

/*
flyout menu's are a simple declarative structure

AEGP_FlyoutMenuItem	myMenu[] = { 
	{1, AEGP_FlyoutMenuMarkType_NORMAL,		FALSE,	AEGP_FlyoutMenuCmdID_NONE, "Hi!"  },
	{1, AEGP_FlyoutMenuMarkType_SEPARATOR,	TRUE,	AEGP_FlyoutMenuCmdID_NONE, NULL  },
	{1, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	AEGP_FlyoutMenuCmdID_NONE, "Set BG Color"  },
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_RED,				"Red"  },
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_GREEN,			"Green"  },
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_BLUE,			"Blue"  },
	{1, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_STANDARD,		"Normal Fill Color"  },
	{1, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	AEGP_FlyoutMenuCmdID_NONE,	"Set Title"  },
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_TITLE_LONGER,	"Longer"  },
		{2, AEGP_FlyoutMenuMarkType_NORMAL,		TRUE,	PT_MenuCmd_TITLE_SHORTER,	"Shorter"  }
};


*/

typedef struct {
	// no more than 5 snap sizes. Our algo can't really cope and it confuses
	// the user.
	A_Err	(*GetSnapSizes)(AEGP_PanelRefcon refcon, A_LPoint*	snapSizes, A_long * numSizesP);


	A_Err	(*PopulateFlyout)(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuItem* itemsP, A_long * in_out_numItemsP);
	A_Err	(*DoFlyoutCommand)(AEGP_PanelRefcon refcon, AEGP_FlyoutMenuCmdID commandID);
} AEGP_PanelFunctions1;

typedef A_Err		(*AEGP_CreatePanelHook)(							
									 AEGP_GlobalRefcon		plugin_refconP,			
									 AEGP_CreatePanelRefcon		refconP,
									 AEGP_PlatformViewRef container,
									 AEGP_PanelH		panelH,
									 AEGP_PanelFunctions1* outFunctionTable,
									 AEGP_PanelRefcon*		outRefcon);			
#define	kAEGPPanelSuite			"AEGP Workspace Panel Suite"
#define	kAEGPPanelSuiteVersion1	1 /* frozen in AE 8.0 */

typedef struct {
	SPAPI A_Err	(*AEGP_RegisterCreatePanelHook)(
		AEGP_PluginID			in_plugin_id,				
		const A_u_char*				in_utf8_match_nameZ, // do not localize
		AEGP_CreatePanelHook	in_update_menu_hook_func,	
		AEGP_CreatePanelRefcon	in_refconP,
		A_Boolean				in_paint_backgroundB);				


	SPAPI A_Err	(*AEGP_UnRegisterCreatePanelHook)(			
		const A_u_char*				in_utf8_match_nameZ);	// do not localize match name

	SPAPI A_Err (*AEGP_SetTitle)(
		AEGP_PanelH in_panelH,
		const A_u_char*		in_utf8_nameZ		// use this to localize the user visible name of the panel
		);

	/** Does the standard 'Window' menu operation
	If tab is not in workspace, it creates it.
	Otherwise, if the tab is not the front most tab in it's frame, it is made frontmost
	Else, if it is visible and frontmost, it is closed.
	*/
	SPAPI A_Err (*AEGP_ToggleVisibility)(
		const A_u_char*				in_utf8_match_nameZ
		);

	SPAPI A_Err (*AEGP_IsShown)(
		const A_u_char*	in_utf8_match_nameZ, 
		A_Boolean*	out_tab_is_shownB,
		A_Boolean*	out_panel_is_frontmostB
		);

} AEGP_PanelSuite1;


#include <AE_GeneralPlugPost.h>


#endif