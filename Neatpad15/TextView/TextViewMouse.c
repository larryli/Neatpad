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
    int snappedX;

    // remove any existing selection
    TextView__InvalidateRange(ptv, ptv->m_nSelectionStart,
                              ptv->m_nSelectionEnd);

    // regular mouse input - mouse is within
    if (mx >= TextView__LeftMarginWidth(ptv)) {
        // map the mouse-coordinates to a real file-offset-coordinate
        TextView__MouseCoordToFilePos(ptv, mx, my, &nLineNo, &nFileOff,
                                      &snappedX);

        TextView__MoveCaret(
            ptv, snappedX, (nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight);

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
                ptv->m_nScrollTimer = SetTimer(ptv->m_hWnd, 1, 30, 0);
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
            // TextView__OnLButtonDown(ptv, 0, mx, my);
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
    int *psnappedX       // [out] adjusted x coord of caret
)
{
    ULONG nLineNo;
    ULONG off_chars = 0;
    RECT rect;
    int cp;

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

    mx += ptv->m_nHScrollPos * ptv->m_nFontWidth;

    // get the USPDATA object for the selected line!!
    USPDATA *uspData = TextView__GetUspData(ptv, 0, nLineNo);

    // convert mouse-x coordinate to a character-offset relative to start of
    // line
    UspSnapXToOffset(uspData, mx, &mx, &cp, 0);

    // return coords!
    TEXTITERATOR *itor =
        TextDocument__iterate_line_start(ptv->m_pTextDoc, nLineNo, &off_chars);
    if (itor == NULL)
        return 0;
    TextIterator__delete(itor);
    *pnLineNo = nLineNo;
    *pnFileOffset = cp + off_chars;
    *psnappedX = mx - ptv->m_nHScrollPos * ptv->m_nFontWidth;
    *psnappedX += TextView__LeftMarginWidth(ptv);

    return 0;
}

LONG TextView__InvalidateLine(TEXTVIEW *ptv, ULONG nLineNo, BOOL forceAnalysis)
{
    if (nLineNo >= ptv->m_nVScrollPos &&
        nLineNo <= ptv->m_nVScrollPos + ptv->m_nWindowLines) {
        RECT rect;

        GetClientRect(ptv->m_hWnd, &rect);

        rect.top = (nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight;
        rect.bottom = rect.top + ptv->m_nLineHeight;

        InvalidateRect(ptv->m_hWnd, &rect, FALSE);
    }

    if (forceAnalysis) {
        for (int i = 0; i < USP_CACHE_SIZE; i++) {
            if (nLineNo == ptv->m_uspCache[i].lineno) {
                ptv->m_uspCache[i].usage = 0;
                break;
            }
        }
    }

    return 0;
}
//
//	Redraw any line which spans the specified range of text
//
LONG TextView__InvalidateRange(TEXTVIEW *ptv, ULONG nStart, ULONG nFinish)
{
    ULONG start = min(nStart, nFinish);
    ULONG finish = max(nStart, nFinish);

    int ypos;
    RECT rect;
    RECT client;
    TEXTITERATOR *itor;

    // information about current line:
    ULONG lineno;
    ULONG off_chars;
    ULONG len_chars;

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

    if (itor == NULL)
        return 0;
    if (!TextIterator__bool(itor) || start >= finish) {
        TextIterator__delete(itor);
        return 0;
    }

    ypos = (lineno - ptv->m_nVScrollPos) * ptv->m_nLineHeight;
    GetClientRect(ptv->m_hWnd, &client);

    // invalidate *whole* lines. don't care about flickering anymore because
    // all output is double-buffered now, and this method is much simpler
    while (TextIterator__bool(itor) && off_chars < finish) {
        SetRect(&rect, 0, ypos, client.right, ypos + ptv->m_nLineHeight);
        rect.left -= ptv->m_nHScrollPos * ptv->m_nFontWidth;
        rect.left += TextView__LeftMarginWidth(ptv);

        InvalidateRect(ptv->m_hWnd, &rect, FALSE);

        // jump down to next line
        if (itor)
            TextIterator__delete(itor);
        itor = TextDocument__iterate_line_start_len(ptv->m_pTextDoc, ++lineno,
                                                    &off_chars, &len_chars);
        if (itor == NULL)
            return 0;
        ypos += ptv->m_nLineHeight;
    }

    TextIterator__delete(itor);
    return 0;
}

//
//	Wrapper around SetCaretPos, hides the caret when it goes
//  off-screen (this protects against x/y wrap around due to integer overflow)
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

    ULONG lineno;
    ULONG off_chars;

    USPDATA *uspData;

    // get line information from cursor-offset
    TEXTITERATOR *itor = TextDocument__iterate_line_offset_start(
        ptv->m_pTextDoc, ptv->m_nCursorOffset, &lineno, &off_chars);

    if (itor == NULL)
        return 0;
    if (!TextIterator__bool(itor)) {
        TextIterator__delete(itor);
        return 0;
    }
    TextIterator__delete(itor);

    if ((uspData = TextView__GetUspData(ptv, NULL, lineno)) != 0) {
        off_chars = ptv->m_nCursorOffset - off_chars;
        UspOffsetToX(uspData, off_chars, FALSE, &xpos);
    }

    // y-coordinate from line-number
    ypos = (lineno - ptv->m_nVScrollPos) * ptv->m_nLineHeight;

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
            TextView__InvalidateLine(ptv, ptv->m_nCurrentLine, TRUE);

        ptv->m_nCurrentLine = nLineNo;

        if (TextView__CheckStyle(ptv, TXS_HIGHLIGHTCURLINE))
            TextView__InvalidateLine(ptv, ptv->m_nCurrentLine, TRUE);
    }
}

//
//	return direction to scroll (+ve, -ve or 0) based on
//  distance of mouse from window edge
//
//	note: counter now redundant, we scroll multiple lines at
//  a time (with a slower timer than before) to achieve
//	variable-speed scrolling
//
int ScrollDir(int counter, int distance)
{
    if (distance > 48)
        return 5;
    if (distance > 16)
        return 2;
    if (distance > 3)
        return 1;
    if (distance > 0)
        return counter % 5 == 0 ? 1 : 0;

    if (distance < -48)
        return -5;
    if (distance < -16)
        return -2;
    if (distance < -3)
        return -1;
    if (distance < 0)
        return counter % 5 == 0 ? -1 : 0;

    return 0;
}
