//
//	MODULE:		TextViewMouse.cpp
//
//	PURPOSE:	Mouse and caret support for the TextView control
//
//	NOTES:		www.catch22.net
//
#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

int ScrollDir(int counter, int dir);

//
//	Wptv->m_MOUSEACTIVATE
//
//	Grab the keyboard input focus
//
LONG TextView__OnMouseActivate(TEXTVIEW *ptv, HWND hwndTop, UINT nHitTest,
                               UINT nMessage)
{
    SetFocus(ptv->m_hWnd);
    return MA_ACTIVATE;
}

//
//	Wptv->m_LBUTTONDOWN
//
//  Position caret to nearest text character under mouse
//
LONG TextView__OnLButtonDown(TEXTVIEW *ptv, UINT nFlags, int mx, int my)
{
    ULONG nLineNo;
    ULONG nCharOff;
    ULONG nFileOff;
    int px;

    // map the mouse-coordinates to a real file-offset-coordinate
    TextView__MouseCoordToFilePos(ptv, mx, my, &nLineNo, &nCharOff, &nFileOff,
                                  &px);

    SetCaretPos(px, (nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight);

    // remove any existing selection
    TextView__InvalidateRange(ptv, ptv->m_nSelectionStart,
                              ptv->m_nSelectionEnd);

    // reset cursor and selection offsets to the same location
    ptv->m_nSelectionStart = nFileOff;
    ptv->m_nSelectionEnd = nFileOff;
    ptv->m_nCursorOffset = nFileOff;

    // set capture for mouse-move selections
    ptv->m_fMouseDown = TRUE;
    SetCapture(ptv->m_hWnd);

    return 0;
}

//
//	Wptv->m_LBUTTONUP
//
//	Release capture and cancel any mouse-scrolling
//
LONG TextView__OnLButtonUp(TEXTVIEW *ptv, UINT nFlags, int mx, int my)
{
    if (ptv->m_fMouseDown) {
        // cancel the scroll-timer if it is still running
        if (ptv->m_nScrollTimer != 0) {
            KillTimer(ptv->m_hWnd, ptv->m_nScrollTimer);
            ptv->m_nScrollTimer = 0;
        }

        ptv->m_fMouseDown = FALSE;
        ReleaseCapture();
    }

    return 0;
}

//
//	Wptv->m_MOUSEMOVE
//
//	Set the selection end-point if we are dragging the mouse
//
LONG TextView__OnMouseMove(TEXTVIEW *ptv, UINT nFlags, int mx, int my)
{
    if (ptv->m_fMouseDown) {
        ULONG nLineNo, nCharOff, nFileOff;

        RECT rect;
        POINT pt = {mx, my};
        int cx; // caret coordinates

        // get the non-scrolling area (an even no. of lines)
        GetClientRect(ptv->m_hWnd, &rect);
        rect.bottom -= rect.bottom % ptv->m_nLineHeight;

        // If mouse is within this area, we don't need to scroll
        if (PtInRect(&rect, pt)) {
            if (ptv->m_nScrollTimer != 0) {
                KillTimer(ptv->m_hWnd, ptv->m_nScrollTimer);
                ptv->m_nScrollTimer = 0;
            }
        }
        // If mouse is outside window, start a timer in
        // order to generate regular scrolling intervals
        else {
            if (ptv->m_nScrollTimer == 0) {
                ptv->m_nScrollCounter = 0;
                ptv->m_nScrollTimer = SetTimer(ptv->m_hWnd, 1, 10, 0);
            }
        }

        // get new cursor offset+coordinates
        TextView__MouseCoordToFilePos(ptv, mx, my, &nLineNo, &nCharOff,
                                      &nFileOff, &cx);

        // update the region of text that has changed selection state
        if (ptv->m_nSelectionEnd != nFileOff) {
            // redraw from old selection-pos to new position
            TextView__InvalidateRange(ptv, ptv->m_nSelectionEnd, nFileOff);

            // adjust the cursor + selection to the new offset
            ptv->m_nSelectionEnd = nFileOff;
            ptv->m_nCursorOffset = nFileOff;
        }

        // always set the caret position because we might be scrolling
        SetCaretPos(cx, (nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight);
    }

    return 0;
}

//
//	Wptv->m_TIMER handler
//
//	Used to create regular scrolling
//
LONG TextView__OnTimer(TEXTVIEW *ptv, UINT nTimerId)
{
    int dx = 0, dy = 0; // scrolling vectors
    RECT rect;
    POINT pt;

    // find client area, but make it an even no. of lines
    GetClientRect(ptv->m_hWnd, &rect);
    rect.bottom -= rect.bottom % ptv->m_nLineHeight;

    // get the mouse's client-coordinates
    GetCursorPos(&pt);
    ScreenToClient(ptv->m_hWnd, &pt);

    //
    // scrolling up / down??
    //
    if (pt.y < 0)
        dy = ScrollDir(ptv->m_nScrollCounter, pt.y);

    else if (pt.y >= rect.bottom)
        dy = ScrollDir(ptv->m_nScrollCounter, pt.y - rect.bottom);

    //
    // scrolling left / right?
    //
    if (pt.x < 0)
        dx = ScrollDir(ptv->m_nScrollCounter, pt.x);

    else if (pt.x > rect.right)
        dx = ScrollDir(ptv->m_nScrollCounter, pt.x - rect.right);

    //
    // Scroll the window but don't update any invalid
    // areas - we will do this manually after we have
    // repositioned the caret
    //
    HRGN hrgnUpdate = TextView__ScrollRgn(ptv, dx, dy, TRUE);

    //
    // do the redraw now that the selection offsets are all
    // pointing to the right places and the scroll positions are valid.
    //
    if (hrgnUpdate != NULL) {
        // We perform a "fake" Wptv->m_MOUSEMOVE for two reasons:
        //
        // 1. To get the cursor/caret/selection offsets set to the correct place
        //    *before* we redraw (so everything is synchronized correctly)
        //
        // 2. To invalidate any areas due to mouse-movement which won't
        //    get done until the next Wptv->m_MOUSEMOVE - and then it would
        //    be too late because we need to redraw *now*
        //
        TextView__OnMouseMove(ptv, 0, pt.x, pt.y);

        // invalidate the area returned by ScrollRegion
        InvalidateRgn(ptv->m_hWnd, hrgnUpdate, FALSE);
        DeleteObject(hrgnUpdate);

        // the next time we process Wptv->m_PAINT everything
        // should get drawn correctly!!
        UpdateWindow(ptv->m_hWnd);
    }

    // keep track of how many Wptv->m_TIMERs we process because
    // we might want to skip the next one
    ptv->m_nScrollCounter++;

    return 0;
}

//
//	Convert mouse(client) coordinates to a file-relative offset
//
//	Currently only uses the main font so will not support other
//	fonts introduced by syntax highlighting
//
BOOL TextView__MouseCoordToFilePos(
    TEXTVIEW *ptv,
    int mx,               // [in]  mouse x-coord
    int my,               // [in]  mouse x-coord
    ULONG *pnLineNo,      // [out] line number
    ULONG *pnCharOffset,  // [out] char-offset from start of line
    ULONG *pfnFileOffset, // [out] zero-based file-offset
    int *px               // [out] adjusted x coord of caret
)
{
    ULONG nLineNo;

    ULONG charoff = 0;
    ULONG fileoff = 0;

    TCHAR buf[TEXTBUFSIZE];
    int len;
    int curx = 0;
    RECT rect;

    // get scrollable area
    GetClientRect(ptv->m_hWnd, &rect);
    rect.bottom -= rect.bottom % ptv->m_nLineHeight;

    // clip mouse to edge of window
    if (mx < 0)
        mx = 0;
    if (my < 0)
        my = 0;
    if (my >= rect.bottom)
        my = rect.bottom - 1;
    if (mx >= rect.right)
        mx = rect.right - 1;

    // It's easy to find the line-number: just divide 'y' by the line-height
    nLineNo = (my / ptv->m_nLineHeight) + ptv->m_nVScrollPos;

    // make sure we don't go outside of the document
    if (nLineNo >= ptv->m_nLineCount) {
        nLineNo = ptv->m_nLineCount ? ptv->m_nLineCount - 1 : 0;
        fileoff = TextDocument__size(ptv->m_pTextDoc);
    }

    HDC hdc = GetDC(ptv->m_hWnd);
    HANDLE hOldFont = SelectObject(hdc, ptv->m_FontAttr[0].hFont);

    *pnCharOffset = 0;

    mx += ptv->m_nHScrollPos * ptv->m_nFontWidth;

    // character offset within the line is more complicated. We have to
    // parse the text.
    for (;;) {
        // grab some text
        if ((len = TextDocument__getline_offset_off(ptv->m_pTextDoc, nLineNo,
                                                    charoff, buf, TEXTBUFSIZE,
                                                    &fileoff)) == 0)
            break;

        int tlen = len;

        if (len > 1 && buf[len - 2] == '\r') {
            buf[len - 2] = 0;
            len -= 2;
        }

        // find it's width
        int width = TextView__NeatTextWidth(ptv, hdc, buf, len,
                                            -(curx % TextView__TabWidth(ptv)));

        // does cursor fall within this segment?
        if (mx >= curx && mx < curx + width) {
            //
            //	We have a range of text, with the mouse
            //  somewhere in the middle. Perform a "binary chop" to
            //  locate the exact character that the mouse is positioned over
            //
            int low = 0;
            int high = len;
            int lowx = 0;
            int highx = width;

            while (low < high - 1) {
                int newlen = (high - low) / 2;

                width = TextView__NeatTextWidth(ptv, hdc, buf + low, newlen,
                                                -lowx - curx);

                if (mx - curx < width + lowx) {
                    high = low + newlen;
                    highx = lowx + width;
                } else {
                    low = low + newlen;
                    lowx = lowx + width;
                }
            }

            // base coordinates on centre of characters, not the edges
            if (mx - curx > highx - ptv->m_nFontWidth / 2) {
                curx += highx;
                charoff += high;
            } else {
                curx += lowx;
                charoff += low;
            }

            *pnCharOffset = charoff;
            break;
        }

        curx += width;
        charoff += tlen;
        *pnCharOffset += len;
    }

    SelectObject(hdc, hOldFont);
    ReleaseDC(ptv->m_hWnd, hdc);

    *pnLineNo = nLineNo;
    //*pnCharOffset=charoff;
    *pfnFileOffset = fileoff + *pnCharOffset;
    *px = curx - ptv->m_nHScrollPos * ptv->m_nFontWidth;

    return 0;
}

//
//	Redraw the specified range of text/data in the control
//
LONG TextView__InvalidateRange(TEXTVIEW *ptv, ULONG nStart, ULONG nFinish)
{
    ULONG start = min(nStart, nFinish);
    ULONG finish = max(nStart, nFinish);

    ULONG lineno, charoff, linelen, lineoff;
    int xpos1 = 0, xpos2 = 0;
    int ypos;

    HDC hdc = GetDC(ptv->m_hWnd);
    SelectObject(hdc, ptv->m_FontAttr[0].hFont);

    RECT rect, client;
    GetClientRect(ptv->m_hWnd, &client);

    // nothing to do?
    if (start == finish)
        return 0;

    //
    //	Find the line number and character offset of specified file-offset
    //
    if (!TextDocument__offset_to_line(ptv->m_pTextDoc, start, &lineno,
                                      &charoff))
        return 0;

    // clip to top of window
    if (lineno < ptv->m_nVScrollPos) {
        lineno = ptv->m_nVScrollPos;
        charoff = 0;
        xpos1 = 0;
    }

    if (!TextDocument__getlineinfo(ptv->m_pTextDoc, lineno, &lineoff, &linelen))
        return 0;

    ypos = (lineno - ptv->m_nVScrollPos) * ptv->m_nLineHeight;

    // selection starts midline...
    if (charoff != 0) {
        ULONG off = 0;
        TCHAR buf[TEXTBUFSIZE];
        int len = charoff;
        int width = 0;

        // loop until we get on-screen
        while (off < charoff) {
            int tlen = min(len, TEXTBUFSIZE);
            tlen = TextDocument__getline_offset_off(ptv->m_pTextDoc, lineno,
                                                    off, buf, tlen, NULL);

            len -= tlen;
            off += tlen;

            width = TextView__NeatTextWidth(ptv, hdc, buf, tlen,
                                            -(xpos1 % TextView__TabWidth(ptv)));
            xpos1 += width;

            if (tlen == 0)
                break;
        }

        // xpos now equals the start of range
    }

    // Invalidate any lines that aren't part of the last line
    while (finish >= lineoff + linelen) {
        SetRect(&rect, xpos1, ypos, client.right, ypos + ptv->m_nLineHeight);
        rect.left -= ptv->m_nHScrollPos * ptv->m_nFontWidth;

        // InvertRect(hdc, &rect);
        InvalidateRect(ptv->m_hWnd, &rect, FALSE);

        xpos1 = 0;
        charoff = 0;
        ypos += ptv->m_nLineHeight;

        // get next line
        if (!TextDocument__getlineinfo(ptv->m_pTextDoc, ++lineno, &lineoff,
                                       &linelen))
            break;
    }

    // erase up to the end of selection
    ULONG offset = lineoff + charoff;
    // ULONG len = finish - charoff;

    xpos2 = xpos1;

    TCHAR buf[TEXTBUFSIZE];
    int width;

    while (offset < finish) {
        int tlen = min((finish - offset), TEXTBUFSIZE);
        tlen = TextDocument__getdata(ptv->m_pTextDoc, offset, buf, tlen);

        offset += tlen;

        width = TextView__NeatTextWidth(ptv, hdc, buf, tlen,
                                        -(xpos2 % TextView__TabWidth(ptv)));
        xpos2 += width;
    }

    SetRect(&rect, xpos1, ypos, xpos2, ypos + ptv->m_nLineHeight);
    OffsetRect(&rect, -ptv->m_nHScrollPos * ptv->m_nFontWidth, 0);

    // InvertRect(hdc, &rect);
    InvalidateRect(ptv->m_hWnd, &rect, FALSE);

    ReleaseDC(ptv->m_hWnd, hdc);

    return 0;
}

//
//	Set the caret position based on ptv->m_nCursorOffset,
//	typically used whilst scrolling
//	(i.e. not due to mouse clicks/keyboard input)
//
ULONG TextView__RepositionCaret(TEXTVIEW *ptv)
{
    ULONG lineno, charoff;
    ULONG offset = 0;
    int xpos = 0;
    int ypos = 0;
    TCHAR buf[TEXTBUFSIZE];

    ULONG nOffset = ptv->m_nCursorOffset;

    // make sure we are using the right font
    HDC hdc = GetDC(ptv->m_hWnd);
    SelectObject(hdc, ptv->m_FontAttr[0].hFont);

    // get line number from cursor-offset
    if (!TextDocument__offset_to_line(ptv->m_pTextDoc, nOffset, &lineno,
                                      &charoff))
        return 0;

    // y-coordinate from line-number
    ypos = (lineno - ptv->m_nVScrollPos) * ptv->m_nLineHeight;

    // now find the x-coordinate on the specified line
    while (offset < charoff) {
        int tlen = min((charoff - offset), TEXTBUFSIZE);
        tlen = TextDocument__getdata(ptv->m_pTextDoc,
                                     nOffset - charoff + offset, buf, tlen);

        offset += tlen;
        xpos += TextView__NeatTextWidth(ptv, hdc, buf, tlen, -xpos);
    }

    ReleaseDC(ptv->m_hWnd, hdc);

    // take horizontal scrollbar into account
    xpos -= ptv->m_nHScrollPos * ptv->m_nFontWidth;

    SetCaretPos(xpos, ypos);
    return 0;
}

//
//	return direction to scroll (+1,-1 or 0) based on
//  distance of mouse from window edge
//
//	"counter" is used to achieve variable-speed scrolling
//
int ScrollDir(int counter, int distance)
{
    int amt;

    // amount to scroll based on distance of mouse from window
    if (abs(distance) < 16)
        amt = 8;
    else if (abs(distance) < 48)
        amt = 3;
    else
        amt = 1;

    if (counter % amt == 0)
        return distance < 0 ? -1 : 1;
    else
        return 0;
}
