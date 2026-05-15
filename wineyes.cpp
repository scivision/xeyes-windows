/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Xeyes for Windows
 *
 * (C) 2022 Yutaka Hirata(YOULAB)
 *
 * This software is based on WinEyes 1.2.
 * Special thanks to Robert W. Buccigrossi.
 */

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include "wineyes.h"

#define LEYE 0
#define REYE 1
#define NUM_EYES 2

static HINSTANCE hInst;
static int    reset_clipping_region = 1;
static POINT  eyecenter[NUM_EYES];
static POINT  eyesize;
static POINT  eyeballsize;
static RECT   prevloc[NUM_EYES] = {{0,0,0,0},{0,0,0,0}};
//
// Displaying menu bar by default.
//
static int show_menu = 1;
//
// Deprecated:
// Original version was not clipping the client area
// when menu is enabled.
//
static bool g_legacyShowMenu = false;

//
// Low level handler for mouse motion.
//
static HHOOK g_hMouseHook;
static HWND g_hWnd;
//
// Setup the window to be topmost by default.
//
static bool g_showTopMost = true;

//
// Command line option.
//
// Usage:
//   xeyes.exe -geometry WIDTHxHEIGHT+XOFF+YOFF
//   xeyes.exe -geometry WIDTHxHEIGHT
//   xeyes.exe -geometry +XOFF+YOFF
//   xeyes.exe -monitor screen_no
//     screen_no: 1, 2, ...
//
static int g_geometryXoff;
static int g_geometryYoff;
static int g_geometryWidth;
static int g_geometryHeight;
static int g_monitorNumber;

enum commandOption {
	OPT_NONE,       // No argument.
	OPT_GEOMETRY,   // -geometry
	OPT_MONITOR,    // -monitor
};

//
// Default value for command line option
//
#define DEFAULT_X 0
#define DEFAULT_Y 0
#define DEFAULT_W 150
#define DEFAULT_H 100
#define DEFAULT_SCREEN_NO 1

//
// Maximum number of monitors to be retrieved.
// Change this value if you want to increase the maximum value.
//
#define MAX_SCREEN_NO 32

//
// Multi monitor information
//
struct MonitorInfoData
{
	MONITORINFOEX entry;
};
static struct MonitorInfoData g_monitorInfo[MAX_SCREEN_NO];
static int g_monitorInfoCount;


enum WinEyesMouseState { WEMS_NONE, WEMS_IN_MOVE };
static WinEyesMouseState mouse_state = WEMS_NONE;


//
// Setup the clipping region which includes left eye,
// right eye and window caption.
//
void setClippingRegion(HWND hWnd)
{
	if (g_legacyShowMenu && show_menu) {
		SetWindowRgn(hWnd, nullptr, 1);
	}
	else {
		POINT size;
		RECT forEllipse1, forEllipse2, winrect, rect;
		HRGN leye, reye;
		int loff, toff;
		POINT client_origin;

		// Get window rectangle in screen coordinates.
		GetWindowRect(hWnd, &winrect);
		// Get client rectangle.
		// The client coordinates specify the upper-left and
		// lower-right corners of the client area.
		GetClientRect(hWnd, &rect);

		// Get the client origin in window coordinates
		client_origin.x = 0; client_origin.y = 0;
		ClientToScreen(hWnd, &client_origin);
		client_origin.x -= winrect.left;
		client_origin.y -= winrect.top;

		size.x = rect.right - rect.left;
		size.y = rect.bottom - rect.top;
		forEllipse1.left = rect.left + 1;
		forEllipse1.right = (long)(size.x / 2 - size.x * 0.025 + 1);
		forEllipse1.top = rect.top + 1;
		forEllipse1.bottom = size.y;
		forEllipse2.right = rect.right - 1;
		forEllipse2.left = (long)(forEllipse1.right + size.x * 0.05) - 1;
		forEllipse2.top = rect.top + 1;
		forEllipse2.bottom = size.y;
		loff = client_origin.x;
		toff = client_origin.y;
		leye = CreateEllipticRgn(forEllipse1.left + loff, forEllipse1.top + toff,
			forEllipse1.right + loff, forEllipse1.bottom + toff);
		reye = CreateEllipticRgn(forEllipse2.left + loff, forEllipse2.top + toff,
			forEllipse2.right + loff, forEllipse2.bottom + toff);
		CombineRgn(leye, leye, reye, RGN_OR);

		//
		// Adding the window title bar to the region.
		//
		if (show_menu) {
			int width = winrect.right - winrect.left;
			int height = toff;
			HRGN topbar = CreateRectRgn(0, 0, width, height);
			CombineRgn(leye, leye, topbar, RGN_OR);
		}

		SetWindowRgn(hWnd, leye, 1);
	}
}

//
// Change the line of sight of left and right eyes
// to the mouse cursor position.
//
void WinEyesUpdate(HWND hWnd, int ForceRedrawEyes)
{
	RECT  forEllipse1, forEllipse2;
	POINT newmouseloc, current1, current2, relmouse[2];
	POINT win_origin;
	HDC   hDc;
	double len;
	double eyecos[2], eyesin[2];
	static POINT mouseloc;

	GetCursorPos((LPPOINT)&newmouseloc);
	if ((mouseloc.x == newmouseloc.x) && (mouseloc.y == newmouseloc.y) && (!ForceRedrawEyes))
		return;

	mouseloc.x = newmouseloc.x, mouseloc.y = newmouseloc.y;

	hDc = GetDC(hWnd);

	GetDCOrgEx(hDc, &win_origin);

	relmouse[LEYE].x = mouseloc.x - win_origin.x - eyecenter[LEYE].x;
	relmouse[LEYE].y = mouseloc.y - win_origin.y - eyecenter[LEYE].y;
	relmouse[REYE].x = mouseloc.x - win_origin.x - eyecenter[REYE].x;
	relmouse[REYE].y = mouseloc.y - win_origin.y - eyecenter[REYE].y;

	if ((relmouse[LEYE].x != 0) || (relmouse[LEYE].y != 0)) {
		len = sqrt((double)(relmouse[LEYE].x * relmouse[LEYE].x + relmouse[LEYE].y * relmouse[LEYE].y));
		eyecos[LEYE] = relmouse[LEYE].x / len;
		eyesin[LEYE] = relmouse[LEYE].y / len;
	}
	else {
		eyecos[LEYE] = 0; eyesin[LEYE] = 0;
	}
	if ((relmouse[REYE].x != 0) || (relmouse[REYE].y != 0)) {
		len = sqrt((double)(relmouse[REYE].x * relmouse[REYE].x + relmouse[REYE].y * relmouse[REYE].y));
		eyecos[REYE] = relmouse[REYE].x / len;
		eyesin[REYE] = relmouse[REYE].y / len;
	}
	else {
		eyecos[REYE] = 0; eyesin[REYE] = 0;
	}

	current1.x = (int)(eyecos[LEYE] * eyesize.x); current1.y = (int)(eyesin[LEYE] * eyesize.y);
	current2.x = (int)(eyecos[REYE] * eyesize.x); current2.y = (int)(eyesin[REYE] * eyesize.y);
	if (current1.x * current1.x + current1.y * current1.y >
		relmouse[LEYE].x * relmouse[LEYE].x + relmouse[LEYE].y * relmouse[LEYE].y) {
		current1.x = relmouse[LEYE].x;
		current1.y = relmouse[LEYE].y;
	}
	if (current2.x * current2.x + current2.y * current2.y >
		relmouse[REYE].x * relmouse[REYE].x + relmouse[REYE].y * relmouse[REYE].y) {
		current2.x = relmouse[REYE].x;
		current2.y = relmouse[REYE].y;
	}
	current1.x += eyecenter[LEYE].x;
	current1.y += eyecenter[LEYE].y;
	current2.x += eyecenter[REYE].x;
	current2.y += eyecenter[REYE].y;

	forEllipse1.left = current1.x - eyeballsize.x, forEllipse1.top = current1.y - eyeballsize.y;
	forEllipse1.right = current1.x + eyeballsize.x, forEllipse1.bottom = current1.y + eyeballsize.y;

	forEllipse2.left = current2.x - eyeballsize.x, forEllipse2.top = current2.y - eyeballsize.y;
	forEllipse2.right = current2.x + eyeballsize.x, forEllipse2.bottom = current2.y + eyeballsize.y;

	if (prevloc[LEYE].left < prevloc[LEYE].right)
	{
		SelectObject(hDc, GetStockObject(WHITE_PEN));
		SelectObject(hDc, GetStockObject(WHITE_BRUSH));
		Ellipse(hDc, prevloc[LEYE].left, prevloc[LEYE].top, prevloc[LEYE].right, prevloc[LEYE].bottom);
		Ellipse(hDc, prevloc[REYE].left, prevloc[REYE].top, prevloc[REYE].right, prevloc[REYE].bottom);
	}

	SelectObject(hDc, GetStockObject(BLACK_BRUSH));
	SelectObject(hDc, GetStockObject(BLACK_PEN));
	Ellipse(hDc, forEllipse1.left, forEllipse1.top, forEllipse1.right, forEllipse1.bottom);
	Ellipse(hDc, forEllipse2.left, forEllipse2.top, forEllipse2.right, forEllipse2.bottom);

	prevloc[LEYE].left = forEllipse1.left, prevloc[LEYE].top = forEllipse1.top;
	prevloc[LEYE].right = forEllipse1.right, prevloc[LEYE].bottom = forEllipse1.bottom;
	prevloc[REYE].left = forEllipse2.left, prevloc[REYE].top = forEllipse2.top;
	prevloc[REYE].right = forEllipse2.right, prevloc[REYE].bottom = forEllipse2.bottom;

	ReleaseDC(hWnd, hDc);
}

void WinEyesPaint(HWND hWnd)
{
	PAINTSTRUCT ps;
	POINT size;
	RECT  forEllipse1, forEllipse2, rect;

	if (reset_clipping_region){
		setClippingRegion(hWnd);
		reset_clipping_region = 0;
	}
	GetClientRect( hWnd, &rect );
	size.x = rect.right-rect.left;
	size.y = rect.bottom-rect.top;
	forEllipse1.left   = rect.left + 1;
	forEllipse1.right  = (long)(size.x/2 - size.x*0.025);
	forEllipse1.top    = rect.top + 1;
	forEllipse1.bottom = size.y;
	forEllipse2.right  = rect.right - 1;
	forEllipse2.left   = (long)(forEllipse1.right + size.x*0.05);
	forEllipse2.top    = rect.top + 1;
	forEllipse2.bottom = size.y;

	BeginPaint(hWnd, (LPPAINTSTRUCT)&ps);

	if (g_legacyShowMenu && show_menu){
		FillRect(ps.hdc, & rect, (HBRUSH) GetStockObject(WHITE_BRUSH));
	}


	SelectObject(ps.hdc, GetStockObject(BLACK_BRUSH) );
	SelectObject(ps.hdc, GetStockObject(BLACK_PEN) );
	Ellipse(ps.hdc, forEllipse1.left, forEllipse1.top, forEllipse1.right, forEllipse1.bottom);
	Ellipse(ps.hdc, forEllipse2.left, forEllipse2.top, forEllipse2.right, forEllipse2.bottom);

	forEllipse1.left +=size.x/2/10; 	forEllipse1.right -=size.x/2/10;
	forEllipse1.top +=size.y/10;    forEllipse1.bottom -= size.y/10;
	forEllipse2.left +=size.x/2/10; 	forEllipse2.right -=size.x/2/10;
	forEllipse2.top +=size.y/10;    forEllipse2.bottom -= size.y/10;

	SelectObject(ps.hdc, GetStockObject(WHITE_BRUSH) );
	SelectObject(ps.hdc, GetStockObject(WHITE_PEN) );
	Ellipse(ps.hdc, forEllipse1.left, forEllipse1.top, forEllipse1.right, forEllipse1.bottom);
	Ellipse(ps.hdc, forEllipse2.left, forEllipse2.top, forEllipse2.right, forEllipse2.bottom);

	eyecenter[LEYE].x=(forEllipse1.right+forEllipse1.left)/2+1;
	eyecenter[LEYE].y=(forEllipse1.top+forEllipse1.bottom)/2+1;

	eyecenter[REYE].x=(forEllipse2.right+forEllipse2.left)/2+1;
	eyecenter[REYE].y=eyecenter[LEYE].y;

	eyesize.x = (long)((forEllipse1.right - forEllipse1.left)/2/1.7);
	eyesize.y = (long)((forEllipse1.bottom - forEllipse1.top)/2/1.7);
	eyeballsize.x = (long)(eyesize.x/2.5);
	eyeballsize.y = (long)(eyesize.y/2.5);
	WinEyesUpdate(hWnd, TRUE);
	EndPaint(hWnd, (LPPAINTSTRUCT)&ps);
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return (TRUE);
	case WM_COMMAND:
		if (wParam == IDOK)
		{
			EndDialog(hDlg, 0);
			return (TRUE);
		}
		break;
	}
	return(FALSE);
}

void ShowTopMost(void)
{
	HMENU hMenu;
	HWND hWnd = g_hWnd;

	hMenu = GetSystemMenu(hWnd, FALSE);

	if (g_showTopMost) {
		CheckMenuItem(hMenu, ID_ALWAYS_ON_TOP, MF_BYCOMMAND | MF_CHECKED);
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else {
		CheckMenuItem(hMenu, ID_ALWAYS_ON_TOP, MF_BYCOMMAND | MF_UNCHECKED);
		SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	}
}

void TerminateAllApplications(void)
{
	HWND hd, rootHd, nextHd;
	char className[128];
	int ret;

	rootHd = GetDesktopWindow();
	hd = GetTopWindow(rootHd);
	while (hd) {
		ZeroMemory(className, sizeof(className));
		ret = GetClassNameA(hd, className, sizeof(className));
		if (ret > 0) {
			if (strcmp(className, WINEYES_APPNAME) == 0) { // found my app
				PostMessage(hd, WM_CLOSE, 0, 0);

			}
		}

		nextHd = GetWindow(hd, GW_HWNDNEXT);
		if (nextHd == nullptr)
			break;
		hd = nextHd;
	}
}

LRESULT CALLBACK PASCAL WinEyesWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HMENU hMenu;
	static int dragx = 0, dragy = 0;

	switch (message)
	{
	case WM_PAINT:
		WinEyesPaint(hWnd);
		break;

	case WM_MOVE:
		WinEyesPaint(hWnd);
		break;

	case WM_SIZE:
		reset_clipping_region = 1;
		RedrawWindow(hWnd, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE);
		break;

	case WM_LBUTTONDOWN:
		mouse_state = WEMS_IN_MOVE;
		SetCapture(hWnd);
		dragx = (short)LOWORD(lParam);
		dragy = (short)HIWORD(lParam);
		break;

	case WM_LBUTTONUP:
		if (mouse_state == WEMS_IN_MOVE) {
			mouse_state = WEMS_NONE;
			ReleaseCapture();
		}
		break;

	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONUP:
	{
		show_menu = show_menu ^ 1;
		reset_clipping_region = 1;
		RedrawWindow(hWnd, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE);
		break;
	}

	case WM_MOUSEMOVE:
	{
		//
		// Changes the mouse cursor type while the cursor is
		// moving over my application.
		//
		HCURSOR cursor = LoadCursor(nullptr, IDC_HAND);
		SetCursor(cursor);

		//
		// The entire application window is moved not just the frame
		// while you are dragging.
		//
		if (mouse_state == WEMS_IN_MOVE) {
			static POINT mouseloc;
			POINT newmouseloc;
			RECT r;
			GetWindowRect(hWnd, &r);
			newmouseloc.x = r.left + (short)LOWORD(lParam);
			newmouseloc.y = r.top + (short)HIWORD(lParam);
			if ((newmouseloc.x != mouseloc.x) || (newmouseloc.y != mouseloc.y)) {
				mouseloc.x = newmouseloc.x;
				mouseloc.y = newmouseloc.y;
				MoveWindow(hWnd, r.left + (short)LOWORD(lParam) - dragx,
					r.top + (short)HIWORD(lParam) - dragy, r.right - r.left, r.bottom - r.top, 1);
			}
		}
		else {
			return (DefWindowProc(hWnd, message, wParam, lParam));
		}
	}
	break;

	case WM_SYSCOMMAND:
		if (wParam == ID_ABOUT) {
			DialogBoxA(hInst, "AboutBox", hWnd, About);
			break;
		}
		else if (wParam == ID_DEFAULT_SIZE) {
			RECT r;
			r.left = DEFAULT_X;
			r.top = DEFAULT_Y;
			r.right = DEFAULT_W;
			r.bottom = DEFAULT_H;
			AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, 0, WS_EX_TOOLWINDOW);
			SetWindowPos(hWnd, HWND_TOP, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE);

			//
			// Bug fix in original version:
			// The eyeball is incorrectly rendered after resizing.
			// So, the clipping area is to be reset and redrawn.
			//
			WinEyesPaint(hWnd);
			reset_clipping_region = 1;
			RedrawWindow(hWnd, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE);
			break;
		}
		else if (wParam == ID_ALWAYS_ON_TOP) {
			hMenu = GetSystemMenu(hWnd, FALSE);
			if (GetMenuState(hMenu, ID_ALWAYS_ON_TOP, MF_BYCOMMAND) & MF_CHECKED) {
				g_showTopMost = false;
				ShowTopMost();
			}
			else {
				g_showTopMost = true;
				ShowTopMost();
			}
		}
		else if (wParam == ID_TERMINATE_ALL) {
			TerminateAllApplications();

		}
		else {
			return(DefWindowProc(hWnd, message, wParam, lParam));
		}
		break;

	case WM_CREATE:
		hMenu = GetSystemMenu(hWnd, FALSE);
		DeleteMenu(hMenu, SC_RESTORE, MF_BYCOMMAND);
		DeleteMenu(hMenu, SC_MINIMIZE, MF_BYCOMMAND);
		DeleteMenu(hMenu, SC_MAXIMIZE, MF_BYCOMMAND);
		InsertMenu(hMenu, 2, MF_STRING | MF_BYPOSITION, ID_DEFAULT_SIZE, TEXT("&Default Size"));
		InsertMenu(hMenu, 3, MF_STRING | MF_BYPOSITION, ID_ALWAYS_ON_TOP, TEXT("&Always on Top"));
		InsertMenu(hMenu, 4, MF_STRING | MF_BYPOSITION, ID_TERMINATE_ALL, TEXT("&Terminate all xeyes"));
		AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenu(hMenu, MF_STRING, ID_ABOUT, TEXT("A&bout Xeyes for Windows..."));
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return (DefWindowProc(hWnd, message, wParam, lParam));
	}

	return (FALSE);
}


BOOL WinEyesInit(HINSTANCE hInstance)
{
	HLOCAL hMemory;
	PWNDCLASS pWndClass;
	BOOL bSuccess = FALSE;

	hMemory = LocalAlloc(LPTR, sizeof(WNDCLASS));
	if (hMemory != nullptr) {
		pWndClass = (PWNDCLASS)LocalLock(hMemory);

		if (pWndClass != nullptr) {
			pWndClass->style = CS_DBLCLKS; // Accept double click events
			pWndClass->lpfnWndProc = WinEyesWndProc;
			pWndClass->hInstance = hInstance;
			pWndClass->hIcon = LoadIcon(hInstance, TEXT("WINEYES"));
			pWndClass->hCursor = LoadCursor(nullptr, IDC_ARROW);
			pWndClass->hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
			pWndClass->lpszMenuName = nullptr;
			pWndClass->lpszClassName = (LPSTR)WINEYES_APPNAME;
			bSuccess = RegisterClass(pWndClass);

			LocalUnlock(hMemory);
		}

		LocalFree(hMemory);
	}

	return(bSuccess);
}

LRESULT CALLBACK GlobalMouseHandler(int nCode, WPARAM wParam, LPARAM lParam)
{
	MOUSEHOOKSTRUCT* pMouseStruct = (MOUSEHOOKSTRUCT*)lParam;

	if (pMouseStruct != nullptr) {
		if (wParam == WM_MOUSEMOVE) {
			WinEyesUpdate(g_hWnd, FALSE);

			//DEBUG_PRINT("wParam %x Mouse position X = %d  Mouse Position Y = %d\n", wParam, pMouseStruct->pt.x, pMouseStruct->pt.y);
		}
	}

	return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

BOOL CALLBACK AllMonitorInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	MONITORINFOEX iMonitor;

	ZeroMemory(&iMonitor, sizeof(iMonitor));
	iMonitor.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(hMonitor, &iMonitor);

	if (iMonitor.dwFlags == DISPLAY_DEVICE_MIRRORING_DRIVER)
	{
		return true;
	}
	else
	{
		if (g_monitorInfoCount >= MAX_SCREEN_NO)
			return false;
		g_monitorInfo[g_monitorInfoCount].entry = iMonitor;
		g_monitorInfoCount++;

		DEBUG_PRINT("scr %d: %d %d, %d %d\n", g_monitorInfoCount,
			iMonitor.rcMonitor.left,
			iMonitor.rcMonitor.top,
			iMonitor.rcMonitor.right,
			iMonitor.rcMonitor.bottom
			);
	}

	return true;
}

void MoveApplWindow(void)
{
	bool outside = false;
	int x, y, w, h;
	int nx, ny, nw, nh;

	nx = g_geometryXoff;
	ny = g_geometryYoff;
	nw = g_geometryWidth;
	nh = g_geometryHeight;

	g_monitorInfoCount = 0;
	EnumDisplayMonitors(nullptr, nullptr, AllMonitorInfoEnumProc, 0);
	if (g_monitorNumber <= g_monitorInfoCount) {
		int mx, my, mw, mh;
		int index = g_monitorNumber - 1;

		if (index >= 0) {
			MONITORINFOEX ent = g_monitorInfo[index].entry;
			mx = ent.rcMonitor.left;
			my = ent.rcMonitor.top;
			mw = ent.rcMonitor.right - ent.rcMonitor.left;
			mh = ent.rcMonitor.bottom - ent.rcMonitor.top;

			DEBUG_PRINT("add: %d %d, %d, %d\n", mx, my, mw, mh);

			nx += mx;
			ny += my;

			DEBUG_PRINT("added: %d %d, %d, %d\n", nx, ny, nw, nh);
		}

	}

	//
	// Checking the coordinate whether it is not out of range of all screens.
	//
	x = GetSystemMetrics(SM_XVIRTUALSCREEN);
	y = GetSystemMetrics(SM_YVIRTUALSCREEN);
	w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	if (nx < x) {
		outside = true;
	}
	else if (nx > x + w) {
		outside = true;
	}

	if (ny < y) {
		outside = true;
	}
	else if (ny > y + h) {
		outside = true;
	}

	if (outside) {
		nx = DEFAULT_X;
		ny = DEFAULT_Y;
		nw = DEFAULT_W;
		nh = DEFAULT_H;
	}

	RECT r;
	r.left = 0;
	r.top = 0;
	r.right = nw;
	r.bottom = nh;
	AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, 0, WS_EX_TOOLWINDOW);

	MoveWindow(g_hWnd, nx, ny, r.right - r.left, r.bottom - r.top, TRUE);
}

void AnalyzeCommandOption(void)
{
	WCHAR *cmdLine;
	WCHAR** argv, ** argv_org;
	int argc;
	enum commandOption optType;
	bool nextSecondParam = false;

	cmdLine = GetCommandLineW();
	argv = CommandLineToArgvW(cmdLine, &argc);
	argv_org = argv;

	argc--;
	argv++;

	g_geometryXoff = DEFAULT_X;
	g_geometryYoff = DEFAULT_Y;
	g_geometryWidth = DEFAULT_W;
	g_geometryHeight = DEFAULT_H;
	g_monitorNumber = DEFAULT_SCREEN_NO;

	for (int i = 0; i < argc; i++) {
		if (nextSecondParam) {
			nextSecondParam = false;

			switch (optType) {
			case OPT_GEOMETRY:
			{
				int w, h, x, y;
				int n = swscanf_s(argv[i], L"%dx%d+%d+%d", &w, &h, &x, &y);
				DEBUG_PRINT("%d %ws %d\n", i, argv[i], n);
				if (n == 4) {
					g_geometryXoff = x;
					g_geometryYoff = y;
					g_geometryWidth = w;
					g_geometryHeight = h;
					break;
				}

				n = swscanf_s(argv[i], L"%dx%d", &w, &h);
				DEBUG_PRINT("%d %ws %d\n", i, argv[i], n);
				if (n == 2) {
					g_geometryWidth = w;
					g_geometryHeight = h;
					break;
				}

				n = swscanf_s(argv[i], L"+%d+%d", &x, &y);
				DEBUG_PRINT("%d %ws %d\n", i, argv[i], n);
				if (n == 2) {
					g_geometryXoff = x;
					g_geometryYoff = y;
					break;
				}
				break;
			}

			case OPT_MONITOR:
			{
				int val;
				int n = swscanf_s(argv[i], L"%d", &val);
				DEBUG_PRINT("%d %ws %d\n", i, argv[i], n);
				if (n == 1) {
					if (!(val >= DEFAULT_SCREEN_NO && val <= MAX_SCREEN_NO))
						val = 1;
					g_monitorNumber = val;
				}
				break;
			}

			default:
				break;
			}

		}
		else {
			if (lstrcmpW(argv[i], L"-geometry") == 0) {
				optType = OPT_GEOMETRY;
				nextSecondParam = true;
			} else if (lstrcmpW(argv[i], L"-monitor") == 0) {
				optType = OPT_MONITOR;
				nextSecondParam = true;
			}
		}
	}

	LocalFree(argv_org);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	HWND hWnd;
	MSG msg;
	RECT r;

	//
	// Paser command line options.
	//
	AnalyzeCommandOption();


	if (!WinEyesInit(hInstance))
	{
		MessageBoxA(nullptr, "Class registration failed", "Error", MB_OK);
		return 0;
	}

	hInst = hInstance;

	r.left = DEFAULT_X;
	r.top = DEFAULT_Y;
	r.right = DEFAULT_W;
	r.bottom = DEFAULT_H;
	AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, 0, WS_EX_TOOLWINDOW);

	hWnd = CreateWindowExA(
		WS_EX_TOOLWINDOW, // Make a tool window so that it doesn't appear in the taskbar
		WINEYES_APPNAME,
		WINEYES_TITLE,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		r.right - r.left,
		r.bottom - r.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	if (!hWnd)
	{
		char buf[80];
		snprintf(buf, sizeof(buf), "Could not create window %lu", GetLastError());
		MessageBoxA(nullptr, buf, "Error", MB_OK);
		return(0);
	}
	//
	// Save the window handle.
	//
	g_hWnd = hWnd;

	//
	// Add low level handler of mouse motion.
	//
	g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, GlobalMouseHandler, hInstance, 0);

	MoveApplWindow();

	setClippingRegion(hWnd);
	ShowWindow(hWnd, SW_RESTORE);
	UpdateWindow(hWnd);

	ShowTopMost();

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	//
	// Remove low level handler of mouse motion.
	//
	UnhookWindowsHookEx(g_hMouseHook);

	return(msg.wParam);
}
