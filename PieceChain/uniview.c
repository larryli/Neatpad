//
//  UniView.cpp
//
//  single-line text editor using UspLib
//
//  Written by J Brown 2006 Freeware
//
//
#define _WIN32_WINNT 0x501
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include "resource.h"
#include "sequence.h"
#include "spanview.h"
#include "usplib\usplib.h"

#define UNIVIEWCLASS _T("UNIVIEW")
#define XBORDER 10
#define YBORDER 10

UniView * UniView__new(HWND hwnd, sequence * s)
{
    UniView * puv = malloc(sizeof(UniView));
    if (puv == NULL)
        return NULL;
    puv->m_hWnd = hwnd;
    puv->m_seq = s;
    puv->m_nSelStart = 0;
    puv->m_nSelEnd = 0;
    puv->m_nCurPos = 0;
    puv->m_fMouseDown = FALSE;
    puv->m_fInsertMode = TRUE;

    ZeroMemory(puv->m_uspFontList, sizeof(puv->m_uspFontList));
    UniView__SetFont(puv, (HFONT)GetStockObject(DEFAULT_GUI_FONT));

    SetCaretPos(XBORDER, YBORDER / 2);
    return puv;
}

void UniView__delete(UniView * puv)
{
    free(puv);
}

void UniView__ReposCaret(UniView * puv, ULONG offset, BOOL fTrailing)
{
    HDC hdc = GetDC(puv->m_hWnd);
    int x;

    if (fTrailing)
        UspOffsetToX(UniView__GetUspData(puv, hdc), puv->m_nCurPos - 1, TRUE, &x);
    else
        UspOffsetToX(UniView__GetUspData(puv, hdc), puv->m_nCurPos, FALSE, &x);

    ReleaseDC(puv->m_hWnd, hdc);
    SetCaretPos(x + XBORDER, YBORDER / 2);

    {
        WCHAR buf[100];
        wsprintf(buf, L"%d\n", puv->m_nCurPos);
        OutputDebugString(buf);
    }
}

void UniView__SetFont(UniView * puv, HFONT hFont)
{
    HDC hdc = GetDC(puv->m_hWnd);
    RECT rect;

    puv->m_uspFontList[0].hFont = hFont;
    UspInitFont(&puv->m_uspFontList[0], hdc, hFont);
    puv->m_nLineHeight = puv->m_uspFontList[0].tm.tmHeight + puv->m_uspFontList[0].tm.tmExternalLeading;

    ReleaseDC(puv->m_hWnd, hdc);

    GetWindowRect(puv->m_hWnd, &rect);
    SetWindowPos(puv->m_hWnd, 0, 0, 0, rect.right - rect.left,
        puv->m_nLineHeight + YBORDER * 3, SWP_NOMOVE | SWP_NOZORDER);
}

int UniView__ApplyFormatting(UniView * puv, WCHAR * wstr, int wlen, ATTR * attrList)
{
    int i;
    int s1 = min(puv->m_nSelStart, puv->m_nSelEnd);
    int s2 = max(puv->m_nSelStart, puv->m_nSelEnd);

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

USPDATA * UniView__GetUspData(UniView * puv, HDC hdc)
{
    SCRIPT_CONTROL sc = {
    0};
    SCRIPT_STATE ss = {
    0};

    WCHAR wstr[100] = L"";
    int wlen = min(100, sequence__size(puv->m_seq));

    sequence__render(puv->m_seq, 0, wstr, wlen);

    UspAnalyze(puv->m_uspData, hdc, wstr, wlen, 0, 0, puv->m_uspFontList, &sc, &ss, 0);
    UspApplySelection(puv->m_uspData, puv->m_nSelStart, puv->m_nSelEnd);

    return puv->m_uspData;
}

//
//  Map mouse-x coordinate to a character-offset and return x-coord
//  of selected character
//
void UniView__Uniscribe_MouseXToOffset(UniView * puv, HWND hwnd, int mouseX, int *charpos, int *snappedToX)
{
    HDC hdc = GetDC(hwnd);
    UspSnapXToOffset(UniView__GetUspData(puv, hdc), mouseX, snappedToX, charpos, 0);
    ReleaseDC(hwnd, hdc);
}

//
//  Display the string
//
void UniView__PaintWnd(UniView * puv)
{
    PAINTSTRUCT ps;
    RECT rect;
    HDC hdcMem;
    HBITMAP hbmMem;
    USPDATA * uspData;

    BeginPaint(puv->m_hWnd, &ps);
    GetClientRect(puv->m_hWnd, &rect);

    //
    //  Create memory-dc for double-buffering
    //  (StringStringOut flickers badly!!)
    //
    hdcMem = CreateCompatibleDC(ps.hdc);
    hbmMem = CreateCompatibleBitmap(ps.hdc, rect.right, rect.bottom);
    SelectObject(hdcMem, hbmMem);

    //  paint the text
    FillRect(hdcMem, &rect, GetSysColorBrush(COLOR_WINDOW));

    uspData = UniView__GetUspData(puv, hdcMem);

    UspTextOut(uspData, hdcMem, XBORDER, YBORDER, puv->m_nLineHeight, 0, &rect);

    // copy to window-dc
    BitBlt(ps.hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);

    // cleanup
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    EndPaint(puv->m_hWnd, &ps);
}

//
//  Left mouse-down handler
//
void UniView__LButtonDown(UniView * puv, int x, int y)
{
    UniView__Uniscribe_MouseXToOffset(puv, puv->m_hWnd, x - XBORDER, &puv->m_nCurPos, &x);

    puv->m_nSelStart = puv->m_nCurPos;
    puv->m_nSelEnd = puv->m_nCurPos;

    InvalidateRect(puv->m_hWnd, 0, 0);

    SetCaretPos(x + XBORDER, YBORDER / 2);

    puv->m_fMouseDown = TRUE;
    SetCapture(puv->m_hWnd);

    PostMessage(GetParent(puv->m_hWnd), WM_USER, 0, 0);
}

void UniView__LButtonDblClick(UniView * puv, int x, int y)
{
    puv->m_nSelStart = 0;
    puv->m_nSelEnd = sequence__size(puv->m_seq);
    puv->m_nCurPos = puv->m_nSelEnd;

    InvalidateRect(puv->m_hWnd, 0, 0);
    UniView__ReposCaret(puv, puv->m_nCurPos, FALSE);
}

//
//  mouse-move handler
//
void UniView__MouseMove(UniView * puv, int x, int y)
{
    if (puv->m_fMouseDown) {
        UniView__Uniscribe_MouseXToOffset(puv, puv->m_hWnd, x - XBORDER, &puv->m_nCurPos, &x);

        puv->m_nSelEnd = puv->m_nCurPos;

        InvalidateRect(puv->m_hWnd, 0, 0);
        SetCaretPos(x + XBORDER, YBORDER / 2);

        PostMessage(GetParent(puv->m_hWnd), WM_USER, 0, 0);
    }
}

//
//  mouse-up handler
//
void UniView__LButtonUp(UniView * puv, int x, int y)
{
    ReleaseCapture();
    puv->m_fMouseDown = FALSE;
}

bool IsKeyPressed(UINT uVK)
{
    return(GetKeyState(uVK) & 0x80000000) ? true : false;
}

void UniView__KeyDown(UniView * puv, UINT nKey, UINT nFlags)
{
    int s1 = min(puv->m_nSelStart, puv->m_nSelEnd);
    int s2 = max(puv->m_nSelStart, puv->m_nSelEnd);
    // BOOL fTrailing = FALSE;
    puv->m_fTrailing = FALSE;

    switch (nKey) {
    case VK_CONTROL:
    case VK_SHIFT:
        return;

    case VK_INSERT:
        puv->m_fInsertMode = !puv->m_fInsertMode;
        return;

    case 'z':
    case 'Z':

        if (IsKeyPressed(VK_CONTROL)) {
            if (sequence__undo(puv->m_seq)) {
                puv->m_nCurPos = sequence__event_index(puv->m_seq);
                puv->m_nSelStart = puv->m_nCurPos;
                puv->m_nCurPos += sequence__event_length(puv->m_seq);
                puv->m_nSelEnd = puv->m_nCurPos;

                InvalidateRect(puv->m_hWnd, 0, 0);
                PostMessage(GetParent(puv->m_hWnd), WM_USER, 0, 0);
                goto repos;
            }

            return;
        }

        break;

    case 'y':
    case 'Y':
        if (IsKeyPressed(VK_CONTROL)) {
            if (sequence__redo(puv->m_seq)) {
                // InvalidateRect(hwnd, 0, 0);
                // PostMessage(GetParent(hwnd), WM_USER, 0, 0);
                puv->m_nCurPos = sequence__event_index(puv->m_seq);
                puv->m_nSelStart = puv->m_nCurPos;
                puv->m_nCurPos += sequence__event_length(puv->m_seq);
                puv->m_nSelEnd = puv->m_nCurPos;
                InvalidateRect(puv->m_hWnd, 0, 0);
                PostMessage(GetParent(puv->m_hWnd), WM_USER, 0, 0);
                goto repos;
            }

            return;
        }

        break;

    case VK_BACK:

        if (s1 != s2) {
            sequence__erase_len(puv->m_seq, s1, s2 - s1);
            puv->m_nCurPos = s1;
        } else if (puv->m_nCurPos > 0) {
            puv->m_nCurPos--;
            sequence__erase_len(puv->m_seq, puv->m_nCurPos, 1);
        }

        PostMessage(GetParent(puv->m_hWnd), WM_USER, 0, 0);
        InvalidateRect(puv->m_hWnd, 0, 0);

        break;

    case VK_DELETE:

        if (s1 != s2) {
            sequence__erase_len(puv->m_seq, s1, s2 - s1);
            puv->m_nCurPos = s1;
        } else {
            sequence__erase_len(puv->m_seq, puv->m_nCurPos, 1);
        }

        PostMessage(GetParent(puv->m_hWnd), WM_USER, 0, 0);
        InvalidateRect(puv->m_hWnd, 0, 0);

        break;

    case VK_LEFT:

        puv->m_fTrailing = FALSE;
        if (puv->m_nCurPos > 0)
            puv->m_nCurPos--;

        break;

    case VK_RIGHT:
        puv->m_fTrailing = TRUE;
        puv->m_nCurPos++;
        break;

    case VK_HOME:
        puv->m_nCurPos = 0;
        break;

    case VK_END:
        puv->m_fTrailing = TRUE;
        puv->m_nCurPos = sequence__size(puv->m_seq);
        break;

    default:
        return;
    }

    if (IsKeyPressed(VK_SHIFT)) {
        puv->m_nSelEnd = puv->m_nCurPos;
        InvalidateRect(puv->m_hWnd, 0, 0);
    } else {
        // if(nSelEnd != nCurPos)
        InvalidateRect(puv->m_hWnd, 0, 0);

        puv->m_nSelStart = puv->m_nCurPos;
        puv->m_nSelEnd = puv->m_nCurPos;
    }

repos:
    UniView__ReposCaret(puv, puv->m_nCurPos, puv->m_fTrailing);
}

void UniView__CharInput(UniView * puv, UINT uChar, DWORD flags)
{
    int s1 = min(puv->m_nSelStart, puv->m_nSelEnd);
    int s2 = max(puv->m_nSelStart, puv->m_nSelEnd);
    WCHAR ch = uChar;

    if (uChar < 32)
        return;

    BOOL fReplaceSelection = (puv->m_nSelStart == puv->m_nSelEnd) ? FALSE : TRUE;

    if (fReplaceSelection) {
        // if(!puv->m_fInsertMode) s2--;
        sequence__group(puv->m_seq);
        sequence__erase_len(puv->m_seq, s1, s2 - s1);
        puv->m_nCurPos = s1;
    }

    if (puv->m_fInsertMode)     // && !fReplaceSelection)
    {
        sequence__insert(puv->m_seq, puv->m_nCurPos, ch);
    } else {
        sequence__replace(puv->m_seq, puv->m_nCurPos, ch);
    }

    if (fReplaceSelection)
        sequence__ungroup(puv->m_seq);

    puv->m_nCurPos++;
    puv->m_nSelStart = puv->m_nCurPos;
    puv->m_nSelEnd = puv->m_nCurPos;

    /*HDC       hdc = GetDC(puv->m_hWnd);
       int x;
       UspOffsetToX(GetUspData(hdc), puv->m_nCurPos, FALSE, &x);
       ReleaseDC(puv->m_hWnd, hdc);
       SetCaretPos(x + XBORDER, YBORDER/2); */
    UniView__ReposCaret(puv, puv->m_nCurPos, TRUE);

    InvalidateRect(puv->m_hWnd, 0, 0);
    PostMessage(GetParent(puv->m_hWnd), WM_USER, 0, 0);
}

//
//  Main window procedure
//
LRESULT UniView__WndProc(UniView * puv, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        puv->m_uspData = UspAllocate();
        return 0;

    case WM_DESTROY:
        UspFree(puv->m_uspData);
        return 0;

    case WM_SETFONT:
        UniView__SetFont(puv, (HFONT)wParam);
        return 0;

    case WM_PAINT:
        UniView__PaintWnd(puv);
        return 0;

    case WM_RBUTTONDOWN:
        InvalidateRect(puv->m_hWnd, 0, 0);
        return 0;

    case WM_LBUTTONDBLCLK:
        UniView__LButtonDblClick(puv, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONDOWN:
        UniView__LButtonDown(puv, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        UniView__MouseMove(puv, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
        UniView__LButtonUp(puv, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_KEYDOWN:
        UniView__KeyDown(puv, wParam, lParam);
        return 0;

    case WM_CHAR:
        UniView__CharInput(puv, wParam, lParam);
        return 0;

    case WM_KILLFOCUS:
        HideCaret(puv->m_hWnd);
        DestroyCaret();
        return 0;

    case WM_SETFOCUS:
        CreateCaret(puv->m_hWnd, NULL, 2, puv->m_nLineHeight + YBORDER);
        ShowCaret(puv->m_hWnd);
        return 0;
    }

    return DefWindowProc(puv->m_hWnd, msg, wParam, lParam);
}

LRESULT WINAPI UniViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UniView * uvptr = (UniView *)GetWindowLongPtr(hwnd, 0);

    switch (msg) {
    case WM_NCCREATE:
        if ((uvptr = UniView__new(hwnd, (sequence *) ((CREATESTRUCT *)lParam)->lpCreateParams)) == 0)
            return FALSE;

        SetWindowLongPtr(hwnd, 0, (LONG_PTR)uvptr);
        return TRUE;

    case WM_NCDESTROY:
        UniView__delete(uvptr);
        return 0;

    default:
        if (uvptr)
            return UniView__WndProc(uvptr, msg, wParam, lParam);
        else
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

//
//  Initialize the window-class
//
ATOM InitUniView(void)
{
    WNDCLASSEX wc;

    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = UniViewWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(UniView *);
    wc.hInstance = GetModuleHandle(0);
    wc.hIcon = LoadIcon(0, MAKEINTRESOURCE(IDI_APPLICATION));
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wc.hbrBackground = 0;
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
    wc.lpszClassName = UNIVIEWCLASS;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    return RegisterClassEx(&wc);
}

//
//  Create the unicode viewer
//
HWND CreateUniView(HWND hwndParent, sequence * seq)
{
    InitUniView();

    return CreateWindowEx(WS_EX_CLIENTEDGE, UNIVIEWCLASS, 0, WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwndParent, 0, 0, seq);
}
