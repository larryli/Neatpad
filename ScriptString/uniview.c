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
#include <usp10.h>

#pragma comment(lib, "usp10.lib")

#define UNIVIEWCLASS _T("UNIVIEW")

// selection and cursor positions
int nSelStart = 0;
int nSelEnd = 1;
int nCurPos = 0;
BOOL fMouseDown = FALSE;

#define XBORDER 10
#define YBORDER 10

extern HFONT g_hFont;
extern HFONT g_textMetric;
extern WCHAR g_szCurrentString[];

//
//	Wrapper around ScriptStringAnalyze
//
SCRIPT_STRING_ANALYSIS GetStringAnalysis(HDC hdc, WCHAR *wstr, UINT wlen)
{
    SCRIPT_CONTROL scriptControl = {0};
    SCRIPT_STATE scriptState = {0};
    SCRIPT_STRING_ANALYSIS scriptStringAnalysis;

    ScriptStringAnalyse(hdc, wstr, wlen, wlen * 2,
                        -1, // Unicode string
                        SSA_GLYPHS | SSA_FALLBACK,
                        0, // no clipping
                        &scriptControl, &scriptState, 0, 0, 0,
                        &scriptStringAnalysis);

    return scriptStringAnalysis;
}

//
//	Uniscribe version of ExtTextOut - takes the same parameters
//
void Uniscribe_TextOut(HDC hdc, int xpos, int ypos, int selstart, int selend)
{
    SCRIPT_STRING_ANALYSIS ssa =
        GetStringAnalysis(hdc, g_szCurrentString, lstrlen(g_szCurrentString));

    if (selend < selstart) {
        int t = selstart;
        selstart = selend;
        selend = t;
    }

    ScriptStringOut(ssa, xpos, ypos, 0, NULL, selstart, selend, FALSE);

    ScriptStringFree(&ssa);
}

//
//	Map mouse-x coordinate to a character-offset and return x-coord
//	of selected character
//
void Uniscribe_MouseXToOffset(HWND hwnd, int mouseX, int *charpos,
                              int *snappedToX)
{
    int trailing = 0;
    int pos;
    HDC hdc = GetDC(hwnd);

    SCRIPT_STRING_ANALYSIS ssa;

    SelectObject(hdc, g_hFont);

    ssa = GetStringAnalysis(hdc, g_szCurrentString, lstrlen(g_szCurrentString));

    ScriptStringXtoCP(ssa, mouseX, &pos, &trailing);
    *charpos = pos + trailing;

    if (ScriptStringCPtoX(ssa, pos, trailing, snappedToX) != S_OK) {
        const SIZE *psz = ScriptString_pSize(ssa);
        *snappedToX = psz->cx;
    }

    ReleaseDC(hwnd, hdc);
    ScriptStringFree(&ssa);
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

    BeginPaint(hwnd, &ps);

    GetClientRect(hwnd, &rect);

    //
    //	Create memory-dc for double-buffering
    //	(StringStringOut flickers badly!!)
    //
    hdcMem = CreateCompatibleDC(ps.hdc);
    hbmMem = CreateCompatibleBitmap(ps.hdc, rect.right, rect.bottom);
    SelectObject(hdcMem, hbmMem);
    SelectObject(hdcMem, g_hFont);

    //	paint the text
    FillRect(hdcMem, &rect, GetSysColorBrush(COLOR_WINDOW));
    Uniscribe_TextOut(hdcMem, XBORDER, YBORDER, nSelStart, nSelEnd);

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

void OnSetFocus(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    TEXTMETRIC tm;

    SelectObject(hdc, g_hFont);
    GetTextMetrics(hdc, &tm);
    CreateCaret(hwnd, NULL, 2, tm.tmHeight + tm.tmExternalLeading + XBORDER);

    ReleaseDC(hwnd, hdc);
    ShowCaret(hwnd);
}

//
//	Main window procedure
//
static LRESULT WINAPI UniViewWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam)
{
    switch (msg) {
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
        OnSetFocus(hwnd);
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
