//
//	MODULE:		TextView.cpp
//
//	PURPOSE:	Implementation of the TextView control
//
//	NOTES:		www.catch22.net
//
#define _WIN32_WINNT 0x400
#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

//
//	Constructor for TextView class
//
TEXTVIEW *TextView__new(HWND hwnd)
{
    TEXTVIEW *ptv = malloc(sizeof(TEXTVIEW));
    if (ptv == NULL)
        return NULL;
    ptv->m_hWnd = hwnd;

    // Set the default font
    TextView__OnSetFont(ptv, (HFONT)GetStockObject(ANSI_FIXED_FONT));

    // Scrollbar related data
    ptv->m_nVScrollPos = 0;
    ptv->m_nHScrollPos = 0;
    ptv->m_nVScrollMax = 0;
    ptv->m_nHScrollMax = 0;

    // File-related data
    ptv->m_nLineCount = 0;
    ptv->m_nLongestLine = 0;

    ptv->m_pTextDoc = TextDocument__new();
    if (ptv->m_pTextDoc == NULL) {
        TextView__delete(ptv);
        return NULL;
    }

    TextView__SetupScrollbars(ptv);
    return ptv;
}

//
//	Destructor for TextView class
//
void TextView__delete(TEXTVIEW *ptv)
{
    if (ptv->m_pTextDoc) {
        TextDocument__delete(ptv->m_pTextDoc);
        free(ptv->m_pTextDoc);
    }
}

VOID TextView__UpdateMetrics(TEXTVIEW *ptv)
{
    RECT rect;
    GetClientRect(ptv->m_hWnd, &rect);

    TextView__OnSize(ptv, 0, rect.right, rect.bottom);
    TextView__RefreshWindow(ptv);
}
//
//	Set a new font
//
LONG TextView__OnSetFont(TEXTVIEW *ptv, HFONT hFont)
{
    HDC hdc;
    TEXTMETRIC tm;
    HANDLE hOld;

    ptv->m_hFont = hFont;

    hdc = GetDC(ptv->m_hWnd);
    hOld = SelectObject(hdc, hFont);

    GetTextMetrics(hdc, &tm);

    ptv->m_nFontHeight = tm.tmHeight;
    ptv->m_nFontWidth = tm.tmAveCharWidth;

    SelectObject(hdc, hOld);
    ReleaseDC(ptv->m_hWnd, hdc);

    TextView__UpdateMetrics(ptv);

    return 0;
}

//
//	Win32 TextView window procedure.
//
LRESULT WINAPI TextViewWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                               LPARAM lParam)
{
    TEXTVIEW *ptv = (TEXTVIEW *)GetWindowLong(hwnd, 0);

    switch (msg) {
    // First message received by any window - make a new TextView object
    // and store pointer to it in our extra-window-bytes
    case WM_NCCREATE:

        if ((ptv = TextView__new(hwnd)) == 0)
            return FALSE;

        SetWindowLong(hwnd, 0, (LONG)ptv);
        return TRUE;

    // Last message received by any window - delete the TextView object
    case WM_NCDESTROY:

        TextView__delete(ptv);
        return 0;

    // Draw contents of TextView whenever window needs updating
    case WM_PAINT:
        return TextView__OnPaint(ptv);

    // Set a new font
    case WM_SETFONT:
        return TextView__OnSetFont(ptv, (HFONT)wParam);

    case WM_SIZE:
        return TextView__OnSize(ptv, wParam, LOWORD(lParam), HIWORD(lParam));

    case WM_VSCROLL:
        return TextView__OnVScroll(ptv, LOWORD(wParam), HIWORD(wParam));

    case WM_HSCROLL:
        return TextView__OnHScroll(ptv, LOWORD(wParam), HIWORD(wParam));

    case WM_MOUSEWHEEL:
        return TextView__OnMouseWheel(ptv, (short)HIWORD(wParam));

    //
    case TXM_OPENFILE:
        return TextView__OpenFile(ptv, (TCHAR *)lParam);

    case TXM_CLEAR:
        return TextView__ClearFile(ptv);

    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//
//	Register the TextView window class
//
BOOL InitTextView(void)
{
    WNDCLASSEX wcx;

    // Window class for the main application parent window
    wcx.cbSize = sizeof(wcx);
    wcx.style = 0;
    wcx.lpfnWndProc = TextViewWndProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = sizeof(TEXTVIEW *);
    wcx.hInstance = GetModuleHandle(0);
    wcx.hIcon = 0;
    wcx.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wcx.hbrBackground = (HBRUSH)0; // NO FLICKERING FOR US!!
    wcx.lpszMenuName = 0;
    wcx.lpszClassName = TEXTVIEW_CLASS;
    wcx.hIconSm = 0;

    return RegisterClassEx(&wcx) ? TRUE : FALSE;
}

//
//	Create a TextView control!
//
HWND CreateTextView(HWND hwndParent)
{
    return CreateWindowEx(WS_EX_CLIENTEDGE, TEXTVIEW_CLASS, _T(""),
                          WS_VSCROLL | WS_HSCROLL | WS_CHILD | WS_VISIBLE, 0, 0,
                          0, 0, hwndParent, 0, GetModuleHandle(0), 0);
}
