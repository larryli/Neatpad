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
//	WM_MOUSEACTIVATE
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
//	WM_LBUTTONDOWN
//
//  Position caret to nearest text character under mouse
//
LONG TextView__OnLButtonDown(TEXTVIEW *ptv, UINT nFlags, int mx, int my)
{
    ULONG nLineNo;
    //	ULONG nCharOff;
    ULONG nFileOff;
    int px;

    // remove any existing selection
    TextView__InvalidateRange(ptv, ptv->m_nSelectionStart,
                              ptv->m_nSelectionEnd);

    // regular mouse input - mouse is within
    if (mx >= TextView__LeftMarginWidth(ptv)) {
        // map the mouse-coordinates to a real file-offset-coordinate
        TextView__MouseCoordToFilePos(ptv, mx, my, &nLineNo, &nFileOff, &px);

        TextView__MoveCaret(
            ptv, px, (nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight);

        // reset cursor and selection offsets to the same location
        ptv->m_nSelectionStart = nFileOff;
        ptv->m_nSelectionEnd = nFileOff;
        ptv->m_nCursorOffset = nFileOff;

        // set capture for mouse-move selections
        ptv->m_nSelectionMode = SELMODE_NORMAL;
    }
    // mouse clicked within margin
    else {
        nLineNo = (my / ptv->m_nLineHeight) + ptv->m_nVScrollPos;

        //
        // if we click in the margin then jump back to start of line
        //
        if (ptv->m_nHScrollPos != 0) {
            ptv->m_nHScrollPos = 0;
            TextView__SetupScrollbars(ptv);
            TextView__RefreshWindow(ptv);
        }

        TextDocument__lineinfo_from_lineno(ptv->m_pTextDoc, nLineNo,
                                           &ptv->m_nSelectionStart,
                                           &ptv->m_nSelectionEnd, 0, 0);
        ptv->m_nSelectionEnd += ptv->m_nSelectionStart;
        ptv->m_nCursorOffset = ptv->m_nSelectionStart;

        ptv->m_nSelMarginOffset1 = ptv->m_nSelectionStart;
        ptv->m_nSelMarginOffset2 = ptv->m_nSelectionEnd;

        TextView__InvalidateRange(ptv, ptv->m_nSelectionStart,
                                  ptv->m_nSelectionEnd);

        // TextView__MoveCaret(ptv, TextView__LeftMarginWidth(ptv), (nLineNo -
        // ptv->m_nVScrollPos) * ptv->m_nLineHeight);
        TextView__RepositionCaret(ptv);

        // set capture for mouse-move selections
        ptv->m_nSelectionMode = SELMODE_MARGIN;
    }

    TextView__UpdateLine(ptv, nLineNo);

    SetCapture(ptv->m_hWnd);

    return 0;
}

//
//	WM_LBUTTONUP
//
//	Release capture and cancel any mouse-scrolling
//
LONG TextView__OnLButtonUp(TEXTVIEW *ptv, UINT nFlags, int mx, int my)
{
    // shift cursor to end of selection
    if (ptv->m_nSelectionMode == SELMODE_MARGIN) {
        ptv->m_nCursorOffset = ptv->m_nSelectionEnd;
        TextView__RepositionCaret(ptv);
    }

    if (ptv->m_nSelectionMode) {
        // cancel the scroll-timer if it is still running
        if (ptv->m_nScrollTimer != 0) {
            KillTimer(ptv->m_hWnd, ptv->m_nScrollTimer);
            ptv->m_nScrollTimer = 0;
        }

        ptv->m_nSelectionMode = SELMODE_NONE;
        ReleaseCapture();
    }

    return 0;
}

//
//	WM_MOUSEMOVE
//
//	Set the selection end-point if we are dragging the mouse
//
LONG TextView__OnMouseMove(TEXTVIEW *ptv, UINT nFlags, int mx, int my)
{
    if (ptv->m_nSelectionMode) {
        ULONG nLineNo, nFileOff;

        RECT rect;
        POINT pt = {mx, my};
        int cx; // caret coordinates

        //
        //	First thing we must do is switch from margin-mode to normal-mode
        //	if the mouse strays into the main document area
        //
        if (ptv->m_nSelectionMode == SELMODE_MARGIN &&
            mx > TextView__LeftMarginWidth(ptv)) {
            ptv->m_nSelectionMode = SELMODE_NORMAL;
            SetCursor(LoadCursor(0, IDC_IBEAM));
        }

        //
        //	Mouse-scrolling: detect if the mouse
        //	is inside/outside of the TextView scrolling area
        //  and stop/start a scrolling timer appropriately
        //
        GetClientRect(ptv->m_hWnd, &rect);

        // build the scrolling area
        rect.bottom -= rect.bottom % ptv->m_nLineHeight;
        rect.left += TextView__LeftMarginWidth(ptv);

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
        TextView__MouseCoordToFilePos(ptv, mx, my, &nLineNo, &nFileOff, &cx);

        // redraw the old and new lines if they are different
        TextView__UpdateLine(ptv, nLineNo);

        // update the region of text that has changed selection state
        // if(ptv->m_nSelectionEnd != nFileOff)
        {
            ULONG linelen;
            TextDocument__lineinfo_from_lineno(ptv->m_pTextDoc, nLineNo, 0,
                                               &linelen, 0, 0);

            ptv->m_nCursorOffset = nFileOff;

            if (ptv->m_nSelectionMode == SELMODE_MARGIN) {
                if (nFileOff >= ptv->m_nSelectionStart) {
                    nFileOff += linelen;
                    ptv->m_nSelectionStart = ptv->m_nSelMarginOffset1;
                } else {
                    ptv->m_nSelectionStart = ptv->m_nSelMarginOffset2;
                }
            }

            // redraw from old selection-pos to new position
            TextView__InvalidateRange(ptv, ptv->m_nSelectionEnd, nFileOff);

            // adjust the cursor + selection to the new offset
            ptv->m_nSelectionEnd = nFileOff;
        }

        // always set the caret position because we might be scrolling
        TextView__MoveCaret(
            ptv, cx, (nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight);

    }
    // mouse isn't being used for a selection, so set the cursor instead
    else {
        if (mx < TextView__LeftMarginWidth(ptv)) {
            SetCursor(ptv->m_hMarginCursor);
        } else {
            // OnLButtonDown(0, mx, my);
            SetCursor(LoadCursor(0, IDC_IBEAM));
        }
    }

    return 0;
}

//
//	WM_TIMER handler
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
    rect.left += TextView__LeftMarginWidth(ptv);

    // get the mouse's client-coordinates
    GetCursorPos(&pt);
    ScreenToClient(ptv->m_hWnd, &pt);

    //
    // scrolling up / down??
    //
    if (pt.y < rect.top)
        dy = ScrollDir(ptv->m_nScrollCounter, pt.y - rect.top);

    else if (pt.y >= rect.bottom)
        dy = ScrollDir(ptv->m_nScrollCounter, pt.y - rect.bottom);

    //
    // scrolling left / right?
    //
    if (pt.x < rect.left)
        dx = ScrollDir(ptv->m_nScrollCounter, pt.x - rect.left);

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
        // We perform a "fake" WM_MOUSEMOVE for two reasons:
        //
        // 1. To get the cursor/caret/selection offsets set to the correct place
        //    *before* we redraw (so everything is synchronized correctly)
        //
        // 2. To invalidate any areas due to mouse-movement which won't
        //    get done until the next WM_MOUSEMOVE - and then it would
        //    be too late because we need to redraw *now*
        //
        TextView__OnMouseMove(ptv, 0, pt.x, pt.y);

        // invalidate the area returned by ScrollRegion
        InvalidateRgn(ptv->m_hWnd, hrgnUpdate, FALSE);
        DeleteObject(hrgnUpdate);

        // the next time we process WM_PAINT everything
        // should get drawn correctly!!
        UpdateWindow(ptv->m_hWnd);
    }

    // keep track of how many WM_TIMERs we process because
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
    int mx,              // [in]  mouse x-coord
    int my,              // [in]  mouse x-coord
    ULONG *pnLineNo,     // [out] line number
    ULONG *pnFileOffset, // [out] zero-based file-offset (in chars)
    int *px              // [out] adjusted x coord of caret
)
{
    TCHAR buf[TEXTBUFSIZE];
    int len;
    int curx = 0;
    RECT rect;

    ULONG nLineNo;
    ULONG off_chars;

    // get scrollable area
    GetClientRect(ptv->m_hWnd, &rect);
    rect.bottom -= rect.bottom % ptv->m_nLineHeight;

    // take left margin into account
    mx -= TextView__LeftMarginWidth(ptv);

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
        off_chars = TextDocument__size(ptv->m_pTextDoc);
    }

    HDC hdc = GetDC(ptv->m_hWnd);
    HANDLE hOldFont = SelectObject(hdc, ptv->m_FontAttr[0].hFont);

    mx += ptv->m_nHScrollPos * ptv->m_nFontWidth;

    TEXTITERATOR *itor =
        TextDocument__iterate_line_start(ptv->m_pTextDoc, nLineNo, &off_chars);

    // character offset within the line is more complicated.
    // We have to parse the text to work out the exact character which
    // falls under the mouse x-coordinate
    while ((len = TextIterator__gettext(itor, buf, TEXTBUFSIZE)) > 0) {
        len = TextView__StripCRLF(ptv, buf, len, TRUE);

        // find it's width
        int width = TextView__NeatTextWidth(ptv, hdc, buf, len,
                                            -(curx % TextView__TabWidth(ptv)));

        // does cursor fall within this segment?
        if (mx >= curx && mx < curx + width) {
            //
            //	We have a block of text, with the mouse
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
                off_chars += high;
            } else {
                curx += lowx;
                off_chars += low;
            }

            break;
        } else {
            curx += width;
            off_chars += len;
        }
    }
    TextIterator__delete(itor);

    SelectObject(hdc, hOldFont);
    ReleaseDC(ptv->m_hWnd, hdc);

    *pnLineNo = nLineNo;
    *pnFileOffset = off_chars;
    *px = curx - ptv->m_nHScrollPos * ptv->m_nFontWidth;
    *px += TextView__LeftMarginWidth(ptv);

    return 0;
}

LONG TextView__InvalidateLine(TEXTVIEW *ptv, ULONG nLineNo)
{
    if (nLineNo >= ptv->m_nVScrollPos &&
        nLineNo <= ptv->m_nVScrollPos + ptv->m_nWindowLines) {
        RECT rect;

        GetClientRect(ptv->m_hWnd, &rect);

        rect.top = (nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight;
        rect.bottom = rect.top + ptv->m_nLineHeight;

        InvalidateRect(ptv->m_hWnd, &rect, FALSE);
    }

    return 0;
}
//
//	Redraw the specified range of text/data in the control
//
LONG TextView__InvalidateRange(TEXTVIEW *ptv, ULONG nStart, ULONG nFinish)
{
    ULONG start = min(nStart, nFinish);
    ULONG finish = max(nStart, nFinish);

    int xpos1 = 0, xpos2 = 0;
    int ypos;
    RECT rect;
    RECT client;

    TEXTITERATOR *itor;

    // information about current line:
    ULONG lineno;
    ULONG off_chars;
    ULONG len_chars;

    GetClientRect(ptv->m_hWnd, &client);

    // nothing to do?
    if (start == finish)
        return 0;

    //
    //	Find the start-of-line information from specified file-offset
    //
    lineno = TextDocument__lineno_from_offset(ptv->m_pTextDoc, start);

    // clip to top of window
    if (lineno < ptv->m_nVScrollPos) {
        lineno = ptv->m_nVScrollPos;
        itor = TextDocument__iterate_line_start_len(ptv->m_pTextDoc, lineno,
                                                    &off_chars, &len_chars);
        start = off_chars;
    } else {
        itor = TextDocument__iterate_line_start_len(ptv->m_pTextDoc, lineno,
                                                    &off_chars, &len_chars);
    }

    if (!TextIterator__bool(itor) || start >= finish)
        return 0;

    HDC hdc = GetDC(ptv->m_hWnd);
    SelectObject(hdc, ptv->m_FontAttr[0].hFont);

    ypos = (lineno - ptv->m_nVScrollPos) * ptv->m_nLineHeight;

    //
    // selection starts midline:
    // find the x-coordinate of this character-offset within the line
    //
    if (off_chars < start) {
        int len = start - off_chars;
        int width = 0;
        int tlen;

        TCHAR buf[TEXTBUFSIZE];

        // loop until we get on-screen
        while ((tlen = TextIterator__gettext(itor, buf,
                                             min(len, TEXTBUFSIZE))) > 0 &&
               off_chars < start) {
            len -= tlen;
            off_chars += tlen;
            len_chars -= tlen;

            width = TextView__NeatTextWidth(ptv, hdc, buf, tlen,
                                            -(xpos1 % TextView__TabWidth(ptv)));
            xpos1 += width;

            if (tlen == 0)
                break;
        }

        // xpos now equals the start of range
    }

    //
    // Invalidate all *whole* lines that aren't part of the last line
    //
    while (TextIterator__bool(itor) && finish >= off_chars + len_chars) {
        SetRect(&rect, xpos1, ypos, client.right, ypos + ptv->m_nLineHeight);
        rect.left -= ptv->m_nHScrollPos * ptv->m_nFontWidth;
        rect.left += TextView__LeftMarginWidth(ptv);

        // InvertRect(hdc, &rect);
        InvalidateRect(ptv->m_hWnd, &rect, FALSE);

        // jump down to next line
        xpos1 = 0;
        ypos += ptv->m_nLineHeight;

        // get next line's file-offset & length
        if (itor)
            TextIterator__delete(itor);
        itor = TextDocument__iterate_line_start_len(ptv->m_pTextDoc, ++lineno,
                                                    &off_chars, &len_chars);
    }

    xpos2 = xpos1;

    //
    // erase up to the end of selection on *last* line of selection
    //
    if (off_chars < finish) {
        TCHAR buf[TEXTBUFSIZE];
        int width;
        int len;

        while ((len = TextIterator__gettext(
                    itor, buf, min((finish - off_chars), TEXTBUFSIZE))) &&
               off_chars < finish) {
            width = TextView__NeatTextWidth(ptv, hdc, buf, len,
                                            -(xpos2 % TextView__TabWidth(ptv)));
            xpos2 += width;
            off_chars += len;
        }
    }
    TextIterator__delete(itor);

    SetRect(&rect, xpos1, ypos, xpos2, ypos + ptv->m_nLineHeight);
    OffsetRect(&rect, -ptv->m_nHScrollPos * ptv->m_nFontWidth, 0);
    OffsetRect(&rect, TextView__LeftMarginWidth(ptv), 0);

    // InvertRect(hdc, &rect);
    InvalidateRect(ptv->m_hWnd, &rect, FALSE);

    ReleaseDC(ptv->m_hWnd, hdc);

    return 0;
}

//
//	Wrapper around SetCaretPos, hides the caret when it goes
//  off-screen (in case x/y wrap around)
//
VOID TextView__MoveCaret(TEXTVIEW *ptv, int x, int y)
{
    if (x < TextView__LeftMarginWidth(ptv) && ptv->m_fHideCaret == FALSE) {
        ptv->m_fHideCaret = TRUE;
        HideCaret(ptv->m_hWnd);
    } else if (x >= TextView__LeftMarginWidth(ptv) &&
               ptv->m_fHideCaret == TRUE) {
        ptv->m_fHideCaret = FALSE;
        ShowCaret(ptv->m_hWnd);
    }

    if (ptv->m_fHideCaret == FALSE)
        SetCaretPos(x, y);
}

//
//	Set the caret position based on ptv->m_nCursorOffset,
//	typically used whilst scrolling
//	(i.e. not due to mouse clicks/keyboard input)
//
ULONG TextView__RepositionCaret(TEXTVIEW *ptv)
{
    int xpos = 0;
    int ypos = 0;
    TCHAR buf[TEXTBUFSIZE];

    ULONG lineno;
    ULONG off_chars;
    int len;

    // get start-of-line information from cursor-offset
    TEXTITERATOR *itor = TextDocument__iterate_line_offset_start(
        ptv->m_pTextDoc, ptv->m_nCursorOffset, &lineno, &off_chars);

    if (!TextIterator__bool(itor)) {
        TextIterator__delete(itor);
        return 0;
    }

    // make sure we are using the right font
    HDC hdc = GetDC(ptv->m_hWnd);
    SelectObject(hdc, ptv->m_FontAttr[0].hFont);

    // y-coordinate from line-number
    ypos = (lineno - ptv->m_nVScrollPos) * ptv->m_nLineHeight;

    // now find the x-coordinate on the specified line
    while ((len = TextIterator__gettext(itor, buf, TEXTBUFSIZE)) > 0 &&
           off_chars < ptv->m_nCursorOffset) {
        // only parse the bit we want
        len = min((int)(ptv->m_nCursorOffset - off_chars), len);
        xpos += TextView__NeatTextWidth(ptv, hdc, buf, len, -xpos);
        off_chars += len;
    }
    TextIterator__delete(itor);

    ReleaseDC(ptv->m_hWnd, hdc);

    // take horizontal scrollbar into account
    xpos -= ptv->m_nHScrollPos * ptv->m_nFontWidth;

    // take left margin into account
    xpos += TextView__LeftMarginWidth(ptv);

    TextView__MoveCaret(ptv, xpos, ypos);

    return 0;
}

void TextView__UpdateLine(TEXTVIEW *ptv, ULONG nLineNo)
{
    // redraw the old and new lines if they are different
    if (ptv->m_nCurrentLine != nLineNo) {
        if (TextView__CheckStyle(ptv, TXS_HIGHLIGHTCURLINE))
            TextView__InvalidateLine(ptv, ptv->m_nCurrentLine);

        ptv->m_nCurrentLine = nLineNo;

        if (TextView__CheckStyle(ptv, TXS_HIGHLIGHTCURLINE))
            TextView__InvalidateLine(ptv, ptv->m_nCurrentLine);
    }
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
