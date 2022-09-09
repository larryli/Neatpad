//
//	ScriptString.c
//
//	ScriptString demo
//
//	Written by J Brown 8/3/2003	Freeware
//
//
#define _WIN32_WINNT 0x501
#define UNICODE
#define _UNICODE

#include <tchar.h>
#include <windows.h>

#include "resource.h"

#include "usplib\usplib.h"

#define UNIVIEWCLASS _T("UNIVIEW")

// selection and cursor positions
int nSelStart = 0;
int nSelEnd = 1;
int nCurPos = 0;
BOOL fMouseDown = FALSE;

#define XBORDER 10
#define YBORDER 10

// extern  HFONT g_hFont;
// extern  HFONT g_textMetric;
extern int g_nLineHeight;
extern USPFONT g_uspFontList[];
extern WCHAR g_szCurrentString[];

USPDATA *g_uspData;
ATTR g_attrRunList[100];

int ApplyFormatting(WCHAR *wstr, int wlen, ATTR *attrList)
{
    int i;
    int s1 = min(nSelStart, nSelEnd);
    int s2 = max(nSelStart, nSelEnd);

    for (i = 0; i < wlen; i++) {
        attrList[i].sel = (i >= s1 && i < s2) ? 1 : 0;

        attrList[i].fg = 0;
        attrList[i].bg = GetSysColor(COLOR_WINDOW);

        if (i >= 2 && i <= 5)
            attrList[i].bg = RGB(220, 220, 220);

        if (i % 2)
            attrList[i].fg = RGB(200, 0, 50);

        attrList[i].len = 1;
        attrList[i].font = 0;
    }

    return 0;
}

USPDATA *GetUspData(HDC hdc)
{
    SCRIPT_CONTROL sc = {0};
    SCRIPT_STATE ss = {0};

    WCHAR *wstr = g_szCurrentString;
    int wlen = lstrlen(g_szCurrentString);

    ApplyFormatting(wstr, wlen, g_attrRunList);

    UspAnalyze(g_uspData, hdc, wstr, wlen, g_attrRunList, 0, g_uspFontList, &sc,
               &ss, 0);
    UspApplySelection(g_uspData, nSelStart, nSelEnd);

    return g_uspData;
}

//
//	Map mouse-x coordinate to a character-offset and return x-coord
//	of selected character
//
void Uniscribe_MouseXToOffset(HWND hwnd, int mouseX, int *charpos,
                              int *snappedToX)
{
    HDC hdc = GetDC(hwnd);
    UspSnapXToOffset(GetUspData(hdc), mouseX, snappedToX, charpos, 0);
    ReleaseDC(hwnd, hdc);
}

//
//	Display the string
//
static void PaintWnd(HWND hwnd)
{
    PAINTSTRUCT ps;
    RECT rect;
    HDC hdcMem;
    HBITMAP hbmMem;
    USPDATA *uspData;

    BeginPaint(hwnd, &ps);

    GetClientRect(hwnd, &rect);

    //
    //	Create memory-dc for double-buffering
    //	(StringStringOut flickers badly!!)
    //
    hdcMem = CreateCompatibleDC(ps.hdc);
    hbmMem = CreateCompatibleBitmap(ps.hdc, rect.right, rect.bottom);
    SelectObject(hdcMem, hbmMem);

    //	paint the text
    FillRect(hdcMem, &rect, GetSysColorBrush(COLOR_WINDOW));

    uspData = GetUspData(hdcMem);

    UspTextOut(uspData, hdcMem, XBORDER, YBORDER, g_nLineHeight, 0, &rect);

    // copy to window-dc
    BitBlt(ps.hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);

    // cleanup
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    EndPaint(hwnd, &ps);
}

//
//	Left mouse-down handler
//
void MouseDown(HWND hwnd, int x, int y)
{
    Uniscribe_MouseXToOffset(hwnd, x - XBORDER, &nCurPos, &x);

    nSelStart = nCurPos;
    nSelEnd = nCurPos;

    InvalidateRect(hwnd, 0, 0);
    SetCaretPos(x + XBORDER, YBORDER / 2);

    fMouseDown = TRUE;
    SetCapture(hwnd);

    PostMessage(GetParent(hwnd), WM_USER, 0, 0);
}

//
//	mouse-move handler
//
void MouseMove(HWND hwnd, int x, int y)
{
    if (fMouseDown) {
        Uniscribe_MouseXToOffset(hwnd, x - XBORDER, &nCurPos, &x);

        nSelEnd = nCurPos;

        InvalidateRect(hwnd, 0, 0);
        SetCaretPos(x + XBORDER, YBORDER / 2);

        PostMessage(GetParent(hwnd), WM_USER, 0, 0);
    }
}

//
//	mouse-up handler
//
void MouseUp(HWND hwnd, int x, int y)
{
    ReleaseCapture();
    fMouseDown = FALSE;
}

//
//	Main window procedure
//
static LRESULT WINAPI UniViewWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_uspData = UspAllocate();
        return 0;

    case WM_DESTROY:
        UspFree(g_uspData);
        return 0;

    //	Paint the display using Uniscribe
    case WM_PAINT:

        PaintWnd(hwnd);
        return 0;

    //
    //	Mouse handling
    //
    case WM_RBUTTONDOWN:
        InvalidateRect(hwnd, 0, 0);
        return 0;

    case WM_LBUTTONDOWN:
        MouseDown(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
        return 0;

    case WM_MOUSEMOVE:
        MouseMove(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
        return 0;

    case WM_LBUTTONUP:
        MouseUp(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
        return 0;

    //
    // Show/Hide caret with focus change
    //
    case WM_KILLFOCUS:
        HideCaret(hwnd);
        DestroyCaret();
        return 0;

    case WM_SETFOCUS:
        CreateCaret(hwnd, NULL, 2, g_nLineHeight + YBORDER);
        ShowCaret(hwnd);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//
//	Initialize the window-class
//
ATOM InitUniView(void)
{
    WNDCLASSEX wc;

    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = UniViewWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(0);
    wc.hIcon = LoadIcon(0, MAKEINTRESOURCE(IDI_APPLICATION));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
    wc.lpszClassName = UNIVIEWCLASS;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    return RegisterClassEx(&wc);
}

//
//	Create the unicode viewer
//
HWND CreateUniView(HWND hwndParent)
{
    InitUniView();

    return CreateWindowEx(WS_EX_CLIENTEDGE, UNIVIEWCLASS, 0,
                          WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwndParent, 0, 0,
                          0);
}
