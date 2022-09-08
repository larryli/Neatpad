//
//	codeview.c
//
//	UTF-16 codepoint view window.
//
//	This window is really simple - it just displays a word-wrapped
//	hex-dump of 16bit WCHAR values, and highlights any range that
//	is selected within the string
//
#define UNICODE
#define _UNICODE

#include <tchar.h>
#include <windows.h>

static HFONT hfont;

#define CODEVIEWCLASS _T("CodeView")

extern int nSelStart;
extern int nSelEnd;
extern int nCurPos;
extern WCHAR g_szCurrentString[];

#define XBORDER 4
#define YBORDER 4

//
//	Fill the specified HDC with 16bit hex values
//
static void DrawCodePoints(HDC hdc, WCHAR *wstr, int wlen, RECT *rect, int s1,
                           int s2, int cp)
{
    int i;
    SIZE sz;
    WCHAR buf[1000];
    int x = XBORDER;
    int y = YBORDER;

    if (s1 > s2) {
        int t = s1;
        s1 = s2;
        s2 = t;
    }

    SetTextAlign(hdc, TA_LEFT | TA_NOUPDATECP);

    // draw each 16bit WCHAR separately as a 4-digit hex value
    for (i = 0; i < wlen; i++) {
        int buflen = wsprintf(buf, _T("%04x "), wstr[i]);

        // colourize depending on selection state
        if (i >= s1 && i < s2) {
            SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
            SetBkColor(hdc, GetSysColor(COLOR_HIGHLIGHT));
        } else {
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        }

        // work out the size of the hex-string
        GetTextExtentPoint32(hdc, buf, buflen, &sz);

        // advance to a new line if necessary
        if (x + sz.cx > rect->right) {
            x = XBORDER;
            y += sz.cy + YBORDER + 2;
        }

        // draw!
        TextOut(hdc, x, y, buf, lstrlen(buf));

        // draw an underline if caret-pos mataches
        if (i == cp) {
            MoveToEx(hdc, x, y + sz.cy + 1, 0);
            LineTo(hdc, x + sz.cx - 8, y + sz.cy + 1);
            MoveToEx(hdc, x, y + sz.cy + 2, 0);
            LineTo(hdc, x + sz.cx - 8, y + sz.cy + 2);
        }

        x += sz.cx;
    }
}

static void OnPaintWnd(HWND hwnd, HFONT hFont)
{
    PAINTSTRUCT ps;
    RECT rect;
    HDC hdcMem;
    HBITMAP hbmMem;

    BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &rect);

    // Create a mem-DC for double-buffering
    hdcMem = CreateCompatibleDC(ps.hdc);
    hbmMem = CreateCompatibleBitmap(ps.hdc, rect.right, rect.bottom);

    SelectObject(hdcMem, hbmMem);
    SelectObject(hdcMem, hFont);

    // draw into the mem-dc
    FillRect(hdcMem, &rect, GetSysColorBrush(COLOR_WINDOW));
    DrawCodePoints(hdcMem, g_szCurrentString, lstrlen(g_szCurrentString), &rect,
                   nSelStart, nSelEnd, nCurPos);

    // paint onto visible window
    BitBlt(ps.hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);

    // cleanup
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    EndPaint(hwnd, &ps);
}

//
//	CodeView window procedure
//
static LRESULT WINAPI CodeViewWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        hfont = CreateFont(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0,
                           _T("Courier New"));
        return 0;

    case WM_DESTROY:
        DeleteObject(hfont);
        return 0;

    case WM_PAINT:
        OnPaintWnd(hwnd, hfont);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//
//	Register the CodeView window-class
//
static ATOM InitCodeViewClass(void)
{
    WNDCLASSEX wc;

    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW;
    wc.lpfnWndProc = CodeViewWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(0);
    wc.hIcon = LoadIcon(0, MAKEINTRESOURCE(IDI_APPLICATION));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszMenuName = 0;
    wc.lpszClassName = CODEVIEWCLASS;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    return RegisterClassEx(&wc);
}

//
//	Create a CodeView window
//
HWND CreateCodePointView(HWND hwndParent)
{
    InitCodeViewClass();

    return CreateWindowEx(WS_EX_CLIENTEDGE, CODEVIEWCLASS, 0,
                          WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwndParent, 0, 0,
                          0);
}
