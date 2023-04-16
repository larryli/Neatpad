//
//	MODULE:		TextViewPaint.cpp
//
//	PURPOSE:	Painting and display for the TextView control
//
//	NOTES:		www.catch22.net
//

#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

//
//	Painting procedure for TextView objects
//
LONG TextView__OnPaint(TEXTVIEW *ptv)
{
    PAINTSTRUCT ps;

    ULONG i, first, last;

    BeginPaint(ptv->m_hWnd, &ps);

    // select which font we will be using
    SelectObject(ps.hdc, ptv->m_hFont);

    // figure out which lines to redraw
    first = ps.rcPaint.top / ptv->m_nFontHeight;
    last = ps.rcPaint.bottom / ptv->m_nFontHeight;

    // make sure we never wrap around the 4gb boundary
    if (last < first)
        last = -1;

    // draw the display line-by-line
    for (i = first; i <= last; i++) {
        TextView__PaintLine(ptv, ps.hdc, i);
    }

    EndPaint(ptv->m_hWnd, &ps);

    return 0;
}

//
//	Emulates ExtTextOut, but draws text using tabs using TabbedTextOut
//
void TextView__TabbedExtTextOut(TEXTVIEW *ptv, HDC hdc, RECT *rect, TCHAR *buf,
                                int len)
{
    int tab = 4 * ptv->m_nFontWidth;
    int width;
    RECT fill = *rect;

    // Draw line and expand tabs
    width = TabbedTextOut(hdc, rect->left, rect->top, buf, len, 1, &tab,
                          rect->left);

    // Erase the rest of the line with the background colour
    fill.left += LOWORD(width);
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &fill, 0, 0, 0);
}

//
//	Draw the specified line
//
void TextView__PaintLine(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo)
{
    RECT rect;

    TCHAR buf[LONGEST_LINE];
    int len;

    // Get the area we want to update
    GetClientRect(ptv->m_hWnd, &rect);

    // calculate rectangle for entire length of line in window
    rect.top = nLineNo * ptv->m_nFontHeight;
    rect.bottom = rect.top + ptv->m_nFontHeight;

    // check we have data to draw on this line
    if (nLineNo >= ptv->m_nLineCount) {
        SetBkColor(hdc, TextView__GetTextViewColor(ptv, HVC_BACKGROUND));
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, 0, 0, 0);
        return;
    }

    // get the data for this single line of text
    len = TextDocument__getline(ptv->m_pTextDoc, nLineNo, buf, LONGEST_LINE);

    // set the colours
    SetTextColor(hdc, TextView__GetTextViewColor(ptv, HVC_FOREGROUND));
    SetBkColor(hdc, TextView__GetTextViewColor(ptv, HVC_BACKGROUND));

    // draw text and fill line background at the same time
    TextView__TabbedExtTextOut(ptv, hdc, &rect, buf, len);
}

//
//	Return an RGB value corresponding to the specified HVC_xxx index
//
COLORREF TextView__GetTextViewColor(TEXTVIEW *ptv, UINT idx)
{
    switch (idx) {
    case HVC_BACKGROUND:
        return GetSysColor(COLOR_WINDOW);
    case HVC_FOREGROUND:
        return GetSysColor(COLOR_WINDOWTEXT);
    default:
        return 0;
    }
}
