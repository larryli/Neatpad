//
//  SpanView.cpp
//
//  Visual representation of a piece-chain
//
//  Written by J Brown 2006 Freeware
//
//
#define _WIN32_WINNT 0x501
#define UNICODE
#define _UNICODE

#include <tchar.h>
#include <windows.h>

#include "SpanView.h"

// using namespace Gdiplus;

#define ORIG_COLOR RGB(220, 230, 250)
#define MOD_COLOR RGB(255, 220, 250)
#define OTHER_COLOR RGB(230, 230, 230)

SpanView *SpanView__new(HWND hwnd, sequence *s)
{
    SpanView *psv = malloc(sizeof(SpanView));
    if (psv == NULL)
        return NULL;
    psv->m_hWnd = hwnd;
    psv->m_seq = s;
    psv->m_hFont =
        CreateFont(28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, _T("Tahoma"));
    psv->m_hFont2 =
        CreateFont(14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, _T("Tahoma"));
    return psv;
}

void SpanView__delete(SpanView *psv)
{
    DeleteObject(psv->m_hFont);
    DeleteObject(psv->m_hFont2);
    free(psv);
}

void PaintRect(HDC hdc, RECT *rect, COLORREF fill)
{
    fill = SetBkColor(hdc, fill);
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, rect, 0, 0, 0);
    SetBkColor(hdc, fill);
}

void DrawEndThing(HDC hdc, RECT *rect, COLORREF fg, COLORREF bg, int id)
{
    POINT pt[5];
    HPEN hpen, holdpen;
    HBRUSH hbr, holdbr;
    RECT rc = *rect;

    if (id == -1) {
        pt[0].x = rc.left;
        pt[0].y = rc.top;
        pt[1].x = rc.left + (rc.right - rc.left) / 2;
        pt[1].y = rc.top;
        pt[2].x = pt[1].x + 10;
        pt[2].y = rc.top + (rc.bottom - rc.top) / 2;
        pt[3].x = rc.left + (rc.right - rc.left) / 2;
        pt[3].y = rc.bottom;
        pt[4].x = rc.left;
        pt[4].y = rc.bottom;
    } else if (id == -2) {
        pt[0].x = rc.right;
        pt[0].y = rc.top;
        pt[1].x = rc.right;
        pt[1].y = rc.bottom;
        pt[2].x = rc.right - (rc.right - rc.left) / 2;
        pt[2].y = rc.bottom;
        pt[3].x = pt[2].x - 10;
        pt[3].y = rc.top + (rc.bottom - rc.top) / 2;
        pt[4].x = rc.right - (rc.right - rc.left) / 2;
        pt[4].y = rc.top;
    }

    hpen = CreatePen(fg == -1 ? PS_NULL : PS_SOLID, 0, fg);
    holdpen = (HPEN)SelectObject(hdc, hpen);
    hbr = CreateSolidBrush(bg);
    holdbr = (HBRUSH)SelectObject(hdc, hbr);

    Polygon(hdc, pt, 5);

    SelectObject(hdc, holdpen);
    SelectObject(hdc, holdbr);
    DeleteObject(hpen);
    DeleteObject(hbr);
}

void DrawShadowRect(HDC hdc, int x, int y, RECT *rect, WCHAR *buf, int len,
                    COLORREF col)
{
    RECT rc = *rect;

    OffsetRect(&rc, 3, 3);
    PaintRect(hdc, &rc, GetSysColor(COLOR_3DFACE));
    OffsetRect(&rc, -1, -1);
    PaintRect(hdc, &rc, GetSysColor(COLOR_3DSHADOW));
    OffsetRect(&rc, -2, -2);
    PaintRect(hdc, &rc, col);
    FrameRect(hdc, &rc, GetSysColorBrush(COLOR_3DDKSHADOW));

    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, x, y, buf, len);
}

void SpanView__DrawSpan2(SpanView *psv, HDC hdc, int *x, int *y, WCHAR *buf,
                         int len, int type, COLORREF col, SIZE *szOut)
{
    SIZE sz;
    RECT rc;

    GetTextExtentPoint32(hdc, buf, len, &sz);

    if (*x + sz.cx + 30 > psv->m_nWrapWidth) {
        *x = 10;
        *y += sz.cy + 40;
    }

    SetRect(&rc, *x + 3, *y + 3, *x + sz.cx + 30, *y + sz.cy + 8);

    if (type < 0) {
        // rc.bottom--;
        OffsetRect(&rc, 3, 3);
        DrawEndThing(hdc, &rc, -1, GetSysColor(COLOR_3DFACE), type);
        OffsetRect(&rc, -1, -1);
        DrawEndThing(hdc, &rc, -1, GetSysColor(COLOR_3DSHADOW), type);
        OffsetRect(&rc, -2, -2);
        rc.bottom--;
        rc.right--;
        DrawEndThing(hdc, &rc, 0, col, type);
    } else {
        OffsetRect(&rc, 3, 3);
        PaintRect(hdc, &rc, GetSysColor(COLOR_3DFACE));
        OffsetRect(&rc, -1, -1);
        PaintRect(hdc, &rc, GetSysColor(COLOR_3DSHADOW));
        OffsetRect(&rc, -2, -2);
        PaintRect(hdc, &rc, col);
        FrameRect(hdc, &rc, GetSysColorBrush(COLOR_3DDKSHADOW));

        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, *x + sz.cy / 2, *y + 4, buf, len);
    }

    szOut->cx = rc.right - rc.left;
    szOut->cy = rc.bottom - rc.top;
}

void SpanView__DrawSpan(SpanView *psv, HDC hdc, int *x, int *y,
                        sequence__span *sptr, SIZE *szOut)
{
    SIZE sz1;
    SIZE sz2;
    COLORREF col;

    WCHAR buf[100] = L"";
    sequence__buffer_control *psbc = VECTOR_GET_AS(
        sequence__buffer_control *, &(psv->m_seq->buffer_list), sptr->buffer);
    WCHAR *src = psbc->buffer + sptr->offset;

    if (src && sptr->id > 2)
        swprintf(buf, 100, _T("%.*ls"), sptr->length, src);

    if (sptr->buffer == 1)
        col = RGB(220, 230, 250);
    else
        col = RGB(255, 220, 250);
    if (sptr->id < 0)
        col = RGB(230, 230, 230);

    if (sptr->id < 0) {
        SelectObject(hdc, psv->m_hFont);
        SpanView__DrawSpan2(psv, hdc, x, y, L"X", 1, sptr->id, OTHER_COLOR,
                            &sz1);
    } else {
        SelectObject(hdc, psv->m_hFont);
        SpanView__DrawSpan2(psv, hdc, x, y, src, sptr->length, sptr->id, col,
                            &sz1);
    }

    SelectObject(hdc, psv->m_hFont2);

    wsprintf(buf, L"#%d (%d,%d)", sptr->id, sptr->offset, sptr->length);
    GetTextExtentPoint32(hdc, buf, lstrlen(buf), &sz2);

    if (sptr->id >= 0)
        TextOut(hdc, *x + (sz1.cx - sz2.cx) / 2, *y + 8 + sz1.cy, buf,
                lstrlen(buf));

    *szOut = sz1;
    szOut->cy += sz2.cy;
}

LONG SpanView__OnPaint(SpanView *psv)
{
    PAINTSTRUCT ps;
    BeginPaint(psv->m_hWnd, &ps);

    RECT rect;
    WCHAR buf[20];
    // int tablist = 4;

    GetClientRect(psv->m_hWnd, &rect);
    psv->m_nWrapWidth = rect.right - rect.left;

    HDC hdcMem = CreateCompatibleDC(ps.hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(ps.hdc, rect.right, rect.bottom);
    SelectObject(hdcMem, hbmMem);

    sequence__span *sptr;
    SIZE sz, sz2;

    int x = 10, y = 10, i;

    FillRect(hdcMem, &rect, GetSysColorBrush(COLOR_WINDOW));

    SelectObject(hdcMem, psv->m_hFont);
    sequence__buffer_control *bufctrl = 0;

    if (psv->m_seq->buffer_list.size > 1)
        bufctrl = VECTOR_GET_AS(sequence__buffer_control *,
                                &(psv->m_seq->buffer_list), 1);

    if (bufctrl && bufctrl->buffer) {
        SelectObject(hdcMem, psv->m_hFont2);
        GetTextExtentPoint32(hdcMem, L"Original file:", 14, &sz2);
        TextOut(hdcMem, x, y, L"Original file:", 14);
        y += sz2.cy + 2;

        SelectObject(hdcMem, psv->m_hFont);
        SpanView__DrawSpan2(psv, hdcMem, &x, &y, bufctrl->buffer,
                            bufctrl->length, 3, ORIG_COLOR, &sz);
        y += sz.cy + 30;
        x = 10;
    }

    SelectObject(hdcMem, psv->m_hFont2);

    GetTextExtentPoint32(hdcMem, L"Piece-chain:", 11, &sz2);
    TextOut(hdcMem, x, y, L"Piece-chain:", 11);
    y += sz2.cy + 2;

    SelectObject(hdcMem, psv->m_hFont);

    for (sptr = psv->m_seq->head; sptr != 0; sptr = sptr->next) {
        if (sptr) {
            SpanView__DrawSpan(psv, hdcMem, &x, &y, sptr, &sz);
        }

        x += sz.cx + 10;
    }

    y += 80;
    x = 10;

    SelectObject(hdcMem, psv->m_hFont2);

    GetTextExtentPoint32(hdcMem, L"Modify-buffer:", 14, &sz2);
    TextOut(hdcMem, x, y, L"Modify-buffer:", 14);
    y += sz2.cy;

    SelectObject(hdcMem, psv->m_hFont);

    if (psv->m_seq->buffer_list.size > 2)
        bufctrl = VECTOR_GET_AS(sequence__buffer_control *,
                                &(psv->m_seq->buffer_list), 2);
    else
        bufctrl = 0;

    if (bufctrl && bufctrl->buffer) {
        SpanView__DrawSpan2(psv, hdcMem, &x, &y, bufctrl->buffer,
                            bufctrl->length, 3, MOD_COLOR, &sz);
        y += sz.cy + 50;
        x = 10;
    }

    SelectObject(hdcMem, psv->m_hFont2);
    GetTextExtentPoint32(hdcMem, L"Undo-stack:", 11, &sz2);
    TextOut(hdcMem, x, y, L"Undo-stack:", 11);
    y += sz2.cy;

    for (i = psv->m_seq->undostack.size - 1; i >= 0; i--) {
        sequence__span_range *range =
            VECTOR_GET_AS(sequence__span_range *, &(psv->m_seq->undostack), i);

        x = 10;
        SelectObject(hdcMem, psv->m_hFont);
        wsprintf(buf, L"%d:", psv->m_seq->undostack.size - 1 - i);
        TextOut(hdcMem, x, y + 4, buf, lstrlen(buf));
        x += 30;
        if (range->boundary == false) {
            for (sptr = range->first; sptr != range->last->next;
                 sptr = sptr->next) {
                SpanView__DrawSpan(psv, hdcMem, &x, &y, sptr, &sz);
                x += sz.cx + 10;
            }
        } else {
            SpanView__DrawSpan(psv, hdcMem, &x, &y, range->first, &sz);
            x += sz.cx + 10;
            SelectObject(hdcMem, psv->m_hFont);
            TextOut(hdcMem, x, y + 4, L"X", 1);
            x += 20;
            SpanView__DrawSpan(psv, hdcMem, &x, &y, range->last, &sz);
            x += sz.cx + 10;
        }

        SelectObject(hdcMem, psv->m_hFont2);
        x += 20;
        switch (range->act) {
        case action_insert:
            TextOut(hdcMem, x, y + 4, L"(insert)", 8);
            break;

        case action_erase:
            TextOut(hdcMem, x, y + 4, L"(erase)", 7);
            break;

        case action_replace:
            TextOut(hdcMem, x, y + 4, L"(replace)", 9);
            break;

        default:
            break;
        }

        y += sz.cy + 20;
    }

    BitBlt(ps.hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    EndPaint(psv->m_hWnd, &ps);
    return 0;
}

//
//  SpanView private window procedure
//
LRESULT SpanView__SpanViewWndProc(SpanView *psv, UINT msg, WPARAM wParam,
                                  LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT:
        return SpanView__OnPaint(psv);

    default:
        break;
    }

    return DefWindowProc(psv->m_hWnd, msg, wParam, lParam);
}

//
//
//
static LRESULT WINAPI SpanViewWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam)
{
    SpanView *svptr = (SpanView *)GetWindowLongPtr(hwnd, 0);
    PVOID p;

    switch (msg) {
    case WM_NCCREATE:
        p = ((CREATESTRUCT *)lParam)->lpCreateParams;

        if ((svptr = SpanView__new(hwnd, (sequence *)p)) == 0)
            return FALSE;

        SetWindowLongPtr(hwnd, 0, (LONG_PTR)svptr);
        return TRUE;

    case WM_NCDESTROY:
        SpanView__delete(svptr);
        return 0;

    default:
        return SpanView__SpanViewWndProc(svptr, msg, wParam, lParam);
    }
}

ATOM InitSpanView(void)
{
    WNDCLASSEX wc;

    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = SpanViewWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(SpanView *);
    wc.hInstance = GetModuleHandle(0);
    wc.hIcon = 0;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = 0;
    wc.lpszClassName = _T("SpanView");
    wc.hIconSm = 0;

    return RegisterClassEx(&wc);
}

HWND CreateSpanView(HWND hwndParent, sequence *seq)
{
    InitSpanView();

    return CreateWindowEx(WS_EX_CLIENTEDGE, _T("SpanView"), 0,
                          WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hwndParent, 0, 0,
                          (LPVOID)seq);
}
