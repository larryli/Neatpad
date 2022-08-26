//
//	MODULE:		TextView.cpp
//
//	PURPOSE:	Implementation of the TextView control
//
//	NOTES:		www.catch22.net
//

#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

TEXTVIEW *TextView__new(HWND hwnd)
{
    TEXTVIEW *ptv = malloc(sizeof(TEXTVIEW));
    if (ptv == NULL)
        return NULL;
    ptv->m_hWnd = hwnd;
    return ptv;
}

void TextView__delete(TEXTVIEW *ptv) { free(ptv); }

//
//	Painting procedure for TextView objects
//
LRESULT WINAPI TextView__OnPaint(TEXTVIEW *ptv)
{
    HDC hdc;
    PAINTSTRUCT ps;
    RECT rect;
    char *text = _T("Hello World!");

    HANDLE hOldFont;
    HFONT hFont;

    hdc = BeginPaint(ptv->m_hWnd, &ps);

    hFont = (HFONT)GetStockObject(ANSI_FIXED_FONT);
    hOldFont = SelectObject(hdc, hFont);

    GetClientRect(ptv->m_hWnd, &rect);

    ExtTextOut(hdc, 10, 10, ETO_OPAQUE, &rect, text, lstrlen(text), 0);

    SelectObject(hdc, hOldFont);

    EndPaint(ptv->m_hWnd, &ps);

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

        if ((ptv = TextView__new(hwnd)) == NULL)
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
