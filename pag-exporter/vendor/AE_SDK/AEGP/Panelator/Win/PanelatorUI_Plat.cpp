

#include "PanelatorUI_Plat.h"
#include <stdio.h>

const char OSWndObjectProperty[] = "PanelatorUI_PlatPtr";

const int BtnID = 444;

PanelatorUI_Plat::PanelatorUI_Plat(SPBasicSuite* spbP, AEGP_PanelH panelH, 
									AEGP_PlatformViewRef platformWindowRef,
									AEGP_PanelFunctions1* outFunctionTable)
							: PanelatorUI(spbP, panelH, platformWindowRef, outFunctionTable),
							i_prevWindowProc(NULL)
{

	// hook the main window
	i_prevWindowProc = (WindowProc)GetWindowLongPtr(platformWindowRef, GWLP_WNDPROC);
	// blasting the client's wndproc, it's required that they provide an adapter.

	SetWindowLongPtrA(platformWindowRef, GWLP_WNDPROC, (LONG_PTR)PanelatorUI_Plat::StaticOSWindowWndProc);
	::SetProp(platformWindowRef, OSWndObjectProperty, (HANDLE)this);


	HWND btn = CreateWindow("BUTTON", 
							"Click Me", 
							WS_CHILD | WS_VISIBLE| BS_CENTER | BS_VCENTER | BS_TEXT, 
							10, 200, 100, 30, 
							platformWindowRef, (HMENU)BtnID, NULL, NULL);
}

LRESULT CALLBACK	PanelatorUI_Plat::StaticOSWindowWndProc(	HWND	hWnd, 
														  UINT	message, 
														  WPARAM	wParam, 
														  LPARAM	lParam)
{
	// suck out our window ptr
	PanelatorUI_Plat* platPtr = reinterpret_cast<PanelatorUI_Plat*>(::GetProp(hWnd, OSWndObjectProperty));
	if(platPtr){
		return platPtr->OSWindowWndProc(hWnd, message, wParam, lParam);
	} else {
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

HBRUSH CreateBrushFromSelector(PFAppSuite4* appSuiteP, PF_App_ColorType sel)
{
	PF_App_Color	appColor = {0};
	appSuiteP->PF_AppGetColor(sel, &appColor);
	return CreateSolidBrush( RGB(appColor.red/255,appColor.green/255, appColor.blue/255));
}

LRESULT PanelatorUI_Plat::OSWindowWndProc(	HWND	hWnd, 
															  UINT	message, 
															  WPARAM	wParam, 
															  LPARAM	lParam)
{
	LRESULT result = 0;
	// do I want to do something
	bool handledB = false;
	switch(message){
		case WM_PAINT: {
			RECT clientArea;
			GetClientRect(hWnd, &clientArea);
			HBRUSH brush = NULL;
			if(i_use_bg) {
				brush = CreateBrushFromSelector(i_appSuite.get(), PF_App_Color_PANEL_BACKGROUND);
			} else {
				brush = CreateSolidBrush( RGB(red,green, blue));
			}
			HDC hdc = GetDC(hWnd);
			FillRect(hdc, &clientArea, brush);
			DeleteObject(brush);	


			clientArea.right = clientArea.left + 200;
			clientArea.bottom = clientArea.top + 30;
			brush = CreateBrushFromSelector(i_appSuite.get(), PF_App_Color_LIST_BOX_FILL);
			FillRect(hdc, &clientArea, brush);
			DeleteObject(brush);	

			clientArea.top = clientArea.bottom;
			clientArea.bottom = clientArea.top + 30;
			brush = CreateBrushFromSelector(i_appSuite.get(), PF_App_Color_TEXT_ON_LIGHTER_BG);
			FillRect(hdc, &clientArea, brush);
			DeleteObject(brush);	
	
			clientArea.top = clientArea.bottom;
			clientArea.bottom = clientArea.top + 30;
			brush = CreateBrushFromSelector(i_appSuite.get(), PF_App_Color_DARK_CAPTION_FILL);
			FillRect(hdc, &clientArea, brush);
			DeleteObject(brush);	

			clientArea.top = clientArea.bottom;
			clientArea.bottom = clientArea.top + 30;
			brush = CreateBrushFromSelector(i_appSuite.get(), PF_App_Color_DARK_CAPTION_TEXT);
			FillRect(hdc, &clientArea, brush);
			DeleteObject(brush);	

			char textToDraw[200];
			RECT origClientArea;
			GetClientRect(hWnd, &origClientArea);

			#ifdef AE_OS_WIN
				sprintf_s(textToDraw, "Size: %d, %d", origClientArea.right - origClientArea.left, origClientArea.bottom - origClientArea.top);
			#else
				sprintf(textToDraw, "Size: %d, %d", origClientArea.right - origClientArea.left, origClientArea.bottom - origClientArea.top);
			#endif
			clientArea.top = clientArea.bottom;
			clientArea.bottom = clientArea.top + 30;
			
			DrawText(hdc, textToDraw, (uint32_t) strlen(textToDraw), &clientArea, DT_SINGLELINE | DT_LEFT );
			
#ifdef AE_OS_WIN
			sprintf_s(textToDraw, "NumClicks: %d", i_numClicks);
#else
			sprintf(textToDraw, "NumClicks: %d", i_numClicks);
#endif
			clientArea.top = clientArea.bottom;
			clientArea.bottom = clientArea.top + 30;

			DrawText(hdc, textToDraw, (uint32_t) strlen(textToDraw), &clientArea, DT_SINGLELINE | DT_LEFT );


			handledB = true;
					   }
		case WM_SIZING:
			InvalidateAll();
			break;

		case WM_COMMAND:
			if(HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == BtnID){
				i_numClicks++;
				InvalidateAll();
				handledB = true;
			}	
		break;
	}


	if(i_prevWindowProc && !handledB){
		result = CallWindowProc(i_prevWindowProc,	hWnd, 
			message,
			wParam, 
			lParam);
	} else {
		result = DefWindowProc(hWnd, message, wParam, lParam);
	}
		return result;

}


void PanelatorUI_Plat::InvalidateAll()
{
	RECT clientArea;
	GetClientRect(i_refH, &clientArea);

	InvalidateRect(i_refH, &clientArea, FALSE);
}
