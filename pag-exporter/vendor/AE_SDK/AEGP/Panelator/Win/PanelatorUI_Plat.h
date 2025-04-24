#ifndef _PANEL_TESTER_UI_PLAT_H_
#define _PANEL_TESTER_UI_PLAT_H_

#include "..\PanelatorUI.h"
#include "SPBasic.h"
#include "AEConfig.h"

class PanelatorUI_Plat : public PanelatorUI
{
public:
	explicit PanelatorUI_Plat(	SPBasicSuite* spbP,
								AEGP_PanelH panelH, 
								AEGP_PlatformViewRef platformWindowRef,
								AEGP_PanelFunctions1* outFunctionTable);


protected:
	virtual void InvalidateAll();

private:
	void operator=(const PanelatorUI&);
	PanelatorUI_Plat(const PanelatorUI_Plat&); // private, unimplemented


	typedef LRESULT (CALLBACK* WindowProc)(	HWND	hWnd, 
		UINT	message, 
		WPARAM	wParam, 
		LPARAM	lParam);

	WindowProc							i_prevWindowProc;


	static LRESULT CALLBACK				StaticOSWindowWndProc(	HWND	hWnd, 
		UINT	message, 
		WPARAM	wParam, 
		LPARAM	lParam);


	LRESULT OSWindowWndProc(	HWND	hWnd, 
		UINT	message, 
		WPARAM	wParam, 
		LPARAM	lParam);

};

#endif