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

//
//	Set scrollbar positions and range
//
VOID TextView__SetupScrollbars(TEXTVIEW *ptv)
{
    SCROLLINFO si = {sizeof(si)};

    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;

    //
    //	Vertical scrollbar
    //
    si.nPos = ptv->m_nVScrollPos;   // scrollbar thumb position
    si.nPage = ptv->m_nWindowLines; // number of lines in a page
    si.nMin = 0;
    si.nMax = ptv->m_nLineCount - 1; // total number of lines in file

    SetScrollInfo(ptv->m_hWnd, SB_VERT, &si, TRUE);

    //
    //	Horizontal scrollbar
    //
    si.nPos = ptv->m_nHScrollPos;     // scrollbar thumb position
    si.nPage = ptv->m_nWindowColumns; // number of lines in a page
    si.nMin = 0;
    si.nMax = ptv->m_nLongestLine - 1; // total number of lines in file

    SetScrollInfo(ptv->m_hWnd, SB_HORZ, &si, TRUE);

    // adjust our interpretation of the max scrollbar range to make
    // range-checking easier. The scrollbars don't use these values, they
    // are for our own use.
    ptv->m_nVScrollMax = ptv->m_nLineCount - ptv->m_nWindowLines;
    ptv->m_nHScrollMax = ptv->m_nLongestLine - ptv->m_nWindowColumns;
}

//
//	Ensure that we never scroll off the end of the file
//
BOOL TextView__PinToBottomCorner(TEXTVIEW *ptv)
{
    BOOL repos = FALSE;

    if (ptv->m_nHScrollPos + ptv->m_nWindowColumns > ptv->m_nLongestLine) {
        ptv->m_nHScrollPos = ptv->m_nLongestLine - ptv->m_nWindowColumns;
        repos = TRUE;
    }

    if (ptv->m_nVScrollPos + ptv->m_nWindowLines > ptv->m_nLineCount) {
        ptv->m_nVScrollPos = ptv->m_nLineCount - ptv->m_nWindowLines;
        repos = TRUE;
    }

    return repos;
}

//
//	The window has changed size - update the scrollbars
//
LONG TextView__OnSize(TEXTVIEW *ptv, UINT nFlags, int width, int height)
{
    int margin = TextView__LeftMarginWidth(ptv);

    ptv->m_nWindowLines =
        min((unsigned)height / ptv->m_nLineHeight, ptv->m_nLineCount);
    ptv->m_nWindowColumns =
        min((width - margin) / ptv->m_nFontWidth, ptv->m_nLongestLine);

    if (TextView__PinToBottomCorner(ptv)) {
        TextView__RefreshWindow(ptv);
        TextView__RepositionCaret(ptv);
    }

    TextView__SetupScrollbars(ptv);

    return 0;
}

//
//	ScrollRgn
//
//	Scrolls the viewport in specified direction. If fReturnUpdateRgn is TRUE,
//	then a HRGN is returned which holds the client-region that must be redrawn
//	manually. This region must be deleted by the caller using DeleteObject.
//
//  Otherwise ScrollRgn returns NULL and updates the entire window
//
HRGN TextView__ScrollRgn(TEXTVIEW *ptv, int dx, int dy, BOOL fReturnUpdateRgn)
{
    RECT clip;

    GetClientRect(ptv->m_hWnd, &clip);

    //
    // make sure that dx,dy don't scroll us past the edge of the document!
    //

    // scroll up
    if (dy < 0) {
        dy = -(int)min((ULONG)-dy, ptv->m_nVScrollPos);
        clip.top = -dy * ptv->m_nLineHeight;
    }
    // scroll down
    else if (dy > 0) {
        dy = min((ULONG)dy, ptv->m_nVScrollMax - ptv->m_nVScrollPos);
        clip.bottom = (ptv->m_nWindowLines - dy) * ptv->m_nLineHeight;
    }

    // scroll left
    if (dx < 0) {
        dx = -(int)min(-dx, ptv->m_nHScrollPos);
        clip.left = -dx * ptv->m_nFontWidth * 4;
    }
    // scroll right
    else if (dx > 0) {
        dx = min((unsigned)dx,
                 (unsigned)ptv->m_nHScrollMax - ptv->m_nHScrollPos);
        clip.right = (ptv->m_nWindowColumns - dx - 4) * ptv->m_nFontWidth;
    }

    // adjust the scrollbar thumb position
    ptv->m_nHScrollPos += dx;
    ptv->m_nVScrollPos += dy;

    // ignore clipping rectangle if its a whole-window scroll
    if (fReturnUpdateRgn == FALSE)
        GetClientRect(ptv->m_hWnd, &clip);

    // take margin into account
    clip.left += TextView__LeftMarginWidth(ptv);

    // perform the scroll
    if (dx != 0 || dy != 0) {
        // do the scroll!
        ScrollWindowEx(ptv->m_hWnd,
                       -dx * ptv->m_nFontWidth, // scale up to pixel coords
                       -dy * ptv->m_nLineHeight,
                       NULL,  // scroll entire window
                       &clip, // clip the non-scrolling part
                       0, 0, SW_INVALIDATE);

        TextView__SetupScrollbars(ptv);

        if (fReturnUpdateRgn) {
            RECT client;

            GetClientRect(ptv->m_hWnd, &client);

            // clip.left -= LeftMarginWidth();

            HRGN hrgnClient = CreateRectRgnIndirect(&client);
            HRGN hrgnUpdate = CreateRectRgnIndirect(&clip);

            // create a region that represents the area outside the
            // clipping rectangle (i.e. the part that is never scrolled)
            CombineRgn(hrgnUpdate, hrgnClient, hrgnUpdate, RGN_XOR);

            DeleteObject(hrgnClient);

            return hrgnUpdate;
        }
    }

    if (dy != 0) {
        GetClientRect(ptv->m_hWnd, &clip);
        clip.right = TextView__LeftMarginWidth(ptv);
        // ScrollWindow(ptv->m_hWnd, 0, -dy * ptv->m_nLineHeight, 0, &clip);
        InvalidateRect(ptv->m_hWnd, &clip, 0);
    }

    return NULL;
}

//
//	Scroll viewport in specified direction
//
VOID TextView__Scroll(TEXTVIEW *ptv, int dx, int dy)
{
    // do a "normal" scroll - don't worry about invalid regions,
    // just scroll the whole window
    TextView__ScrollRgn(ptv, dx, dy, FALSE);
}

LONG GetTrackPos32(HWND hwnd, int nBar)
{
    SCROLLINFO si = {sizeof(si), SIF_TRACKPOS};
    GetScrollInfo(hwnd, nBar, &si);
    return si.nTrackPos;
}

//
//	Vertical scrollbar support
//
LONG TextView__OnVScroll(TEXTVIEW *ptv, UINT nSBCode, UINT nPos)
{
    ULONG oldpos = ptv->m_nVScrollPos;

    switch (nSBCode) {
    case SB_TOP:
        ptv->m_nVScrollPos = 0;
        TextView__RefreshWindow(ptv);
        break;

    case SB_BOTTOM:
        ptv->m_nVScrollPos = ptv->m_nVScrollMax;
        TextView__RefreshWindow(ptv);
        break;

    case SB_LINEUP:
        TextView__Scroll(ptv, 0, -1);
        break;

    case SB_LINEDOWN:
        TextView__Scroll(ptv, 0, 1);
        break;

    case SB_PAGEDOWN:
        TextView__Scroll(ptv, 0, ptv->m_nWindowLines);
        break;

    case SB_PAGEUP:
        TextView__Scroll(ptv, 0, -ptv->m_nWindowLines);
        break;

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:

        ptv->m_nVScrollPos = GetTrackPos32(ptv->m_hWnd, SB_VERT);
        TextView__RefreshWindow(ptv);

        break;
    }

    if (oldpos != ptv->m_nVScrollPos) {
        TextView__SetupScrollbars(ptv);
        TextView__RepositionCaret(ptv);
    }

    return 0;
}

//
//	Horizontal scrollbar support
//
LONG TextView__OnHScroll(TEXTVIEW *ptv, UINT nSBCode, UINT nPos)
{
    int oldpos = ptv->m_nHScrollPos;

    switch (nSBCode) {
    case SB_LEFT:
        ptv->m_nHScrollPos = 0;
        TextView__RefreshWindow(ptv);
        break;

    case SB_RIGHT:
        ptv->m_nHScrollPos = ptv->m_nHScrollMax;
        TextView__RefreshWindow(ptv);
        break;

    case SB_LINELEFT:
        TextView__Scroll(ptv, -1, 0);
        break;

    case SB_LINERIGHT:
        TextView__Scroll(ptv, 1, 0);
        break;

    case SB_PAGELEFT:
        TextView__Scroll(ptv, -ptv->m_nWindowColumns, 0);
        break;

    case SB_PAGERIGHT:
        TextView__Scroll(ptv, ptv->m_nWindowColumns, 0);
        break;

    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:

        ptv->m_nHScrollPos = GetTrackPos32(ptv->m_hWnd, SB_HORZ);
        TextView__RefreshWindow(ptv);
        break;
    }

    if (oldpos != ptv->m_nHScrollPos) {
        TextView__SetupScrollbars(ptv);
        TextView__RepositionCaret(ptv);
    }

    return 0;
}

LONG TextView__OnMouseWheel(TEXTVIEW *ptv, int nDelta)
{
#ifndef SPI_GETWHEELSCROLLLINES
#define SPI_GETWHEELSCROLLLINES 104
#endif

    int nScrollLines;

    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &nScrollLines, 0);

    if (nScrollLines <= 1)
        nScrollLines = 3;

    TextView__Scroll(ptv, 0, (-nDelta / 120) * nScrollLines);
    TextView__RepositionCaret(ptv);

    return 0;
}
