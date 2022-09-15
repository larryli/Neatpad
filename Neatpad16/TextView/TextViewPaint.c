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

void PaintRect(HDC hdc, int x, int y, int width, int height, COLORREF fill);
void PaintRectPoint(HDC hdc, RECT *rect, COLORREF fill);
void DrawCheckedRect(HDC hdc, RECT *rect, COLORREF fg, COLORREF bg);

// extern "C" COLORREF MixRGB(COLORREF, COLORREF);

//
//	Perform a full redraw of the entire window
//
VOID TextView__RefreshWindow(TEXTVIEW *ptv)
{
    InvalidateRect(ptv->m_hWnd, NULL, FALSE);
}

USPCACHE *TextView__GetUspCache(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo)
{
    return TextView__GetUspCache_offset(ptv, hdc, nLineNo, 0);
}

USPCACHE *TextView__GetUspCache_offset(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo,
                                       ULONG *nOffset /*=0*/)
{
    TCHAR buff[TEXTBUFSIZE];
    ATTR attr[TEXTBUFSIZE];
    ULONG colno = 0;
    ULONG off_chars = 0;
    int len;
    HDC hdcTemp;

    USPDATA *uspData;
    ULONG lru_usage = -1;
    int lru_index = 0;

    //
    //	Search the cache to see if we've already analyzed the requested line
    //
    for (int i = 0; i < USP_CACHE_SIZE; i++) {
        // remember the least-recently used
        if (ptv->m_uspCache[i].usage < lru_usage) {
            lru_index = i;
            lru_usage = ptv->m_uspCache[i].usage;
        }

        // match the line#
        if (ptv->m_uspCache[i].usage > 0 &&
            ptv->m_uspCache[i].lineno == nLineNo) {
            if (nOffset)
                *nOffset = ptv->m_uspCache[i].offset;

            ptv->m_uspCache[i].usage++;
            return &ptv->m_uspCache[i];
        }
    }

    //
    // not found? overwrite the "least-recently-used" entry
    //
    ptv->m_uspCache[lru_index].lineno = nLineNo;
    ptv->m_uspCache[lru_index].usage = 1;
    uspData = ptv->m_uspCache[lru_index].uspData;

    if (hdc == 0)
        hdcTemp = GetDC(ptv->m_hWnd);
    else
        hdcTemp = hdc;

    //
    // get the text for the entire line and apply style attributes
    //
    len = TextDocument__getline(ptv->m_pTextDoc, nLineNo, buff, TEXTBUFSIZE,
                                &off_chars);

    // cache the line's offset and length information
    ptv->m_uspCache[lru_index].offset = off_chars;
    ptv->m_uspCache[lru_index].length = len;
    ptv->m_uspCache[lru_index].length_CRLF =
        len - TextView__CRLF_size(ptv, buff, len);

    len = TextView__ApplyTextAttributes(ptv, nLineNo, off_chars, &colno, buff,
                                        len, attr);

    //
    // setup the tabs + itemization states
    //
    int tablist[] = {ptv->m_nTabWidthChars};
    SCRIPT_TABDEF tabdef = {1, 0, tablist, 0};
    SCRIPT_CONTROL scriptControl = {0};
    SCRIPT_STATE scriptState = {0};

    // SCRIPT_DIGITSUBSTITUTE scriptDigitSub;
    // ScriptRecordDigitSubstitution(LOCALE_USER_DEFAULT, &scriptDigitSub);
    // ScriptApplyDigitSubstitution(&scriptDigitSub, &scriptControl,
    // &scriptState);

    //
    // go!
    //
    UspAnalyze(uspData, hdcTemp, buff, len, attr, 0, ptv->m_uspFontList,
               &scriptControl, &scriptState, &tabdef);

    //
    //	Apply the selection
    //
    TextView__ApplySelection(ptv, uspData, nLineNo, off_chars, len);

    if (hdc == 0)
        ReleaseDC(ptv->m_hWnd, hdcTemp);

    if (nOffset)
        *nOffset = off_chars;

    return &ptv->m_uspCache[lru_index];
}

//
//	Return a fully-analyzed USPDATA object for the specified line
//
USPDATA *TextView__GetUspData(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo)
{
    return TextView__GetUspData_offset(ptv, hdc, nLineNo, NULL);
}

USPDATA *TextView__GetUspData_offset(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo,
                                     ULONG *nOffset /*=0*/)
{
    USPCACHE *uspCache =
        TextView__GetUspCache_offset(ptv, hdc, nLineNo, nOffset);

    if (uspCache)
        return uspCache->uspData;
    else
        return 0;
}

//
//	Invalidate every entry in the cache so we can start afresh
//
void TextView__ResetLineCache(TEXTVIEW *ptv)
{
    for (int i = 0; i < USP_CACHE_SIZE; i++) {
        ptv->m_uspCache[i].usage = 0;
    }
}

//
//	Painting procedure for TextView objects
//
LONG TextView__OnPaint(TEXTVIEW *ptv)
{
    PAINTSTRUCT ps;
    ULONG i;
    ULONG first;
    ULONG last;

    HRGN hrgnUpdate;
    HDC hdcMem;
    HBITMAP hbmMem;
    RECT rect;

    //
    // get update region *before* BeginPaint validates the window
    //
    hrgnUpdate = CreateRectRgn(0, 0, 1, 1);
    GetUpdateRgn(ptv->m_hWnd, hrgnUpdate, FALSE);

    //
    // create a memoryDC the same size a single line, for double-buffering
    //
    BeginPaint(ptv->m_hWnd, &ps);
    GetClientRect(ptv->m_hWnd, &rect);

    hdcMem = CreateCompatibleDC(ps.hdc);
    hbmMem = CreateCompatibleBitmap(ps.hdc, rect.right - rect.left,
                                    ptv->m_nLineHeight);

    SelectObject(hdcMem, hbmMem);

    //
    // figure out which lines to redraw
    //
    first = ptv->m_nVScrollPos + ps.rcPaint.top / ptv->m_nLineHeight;
    last = ptv->m_nVScrollPos + ps.rcPaint.bottom / ptv->m_nLineHeight;

    // make sure we never wrap around the 4gb boundary
    if (last < first)
        last = -1;

    //
    // draw the display line-by-line
    //
    for (i = first; i <= last; i++) {
        int sx = 0;
        int sy = (i - ptv->m_nVScrollPos) * ptv->m_nLineHeight;
        int width = rect.right - rect.left;

        // prep the background
        PaintRect(hdcMem, 0, 0, width, ptv->m_nLineHeight,
                  TextView__LineColour(ptv, i));
        // PaintRect(hdcMem,
        // ptv->m_cpBlockStart.xpos+TextView__LeftMarginWidth(ptv), 0,
        // ptv->m_cpBlockEnd.xpos-ptv->m_cpBlockStart.xpos,
        // ptv->m_nLineHeight,TextView__GetColour(ptv, TXC_HIGHLIGHT));

        // draw each line into the offscreen buffer
        TextView__PaintLine(ptv, hdcMem, i,
                            -ptv->m_nHScrollPos * ptv->m_nFontWidth, 0,
                            hrgnUpdate);

        // transfer to screen
        BitBlt(ps.hdc, sx, sy, width, ptv->m_nLineHeight, hdcMem, 0, 0,
               SRCCOPY);
    }

    //
    //	Cleanup
    //
    EndPaint(ptv->m_hWnd, &ps);

    DeleteDC(hdcMem);
    DeleteObject(hbmMem);
    DeleteObject(hrgnUpdate);

    return 0;
}

//
//	Draw the specified line (including margins etc) to the specified location
//
void TextView__PaintLine(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo, int xpos,
                         int ypos, HRGN hrgnUpdate)
{
    RECT bounds;
    HRGN hrgnBounds;

    GetClientRect(ptv->m_hWnd, &bounds);
    SelectClipRgn(hdc, NULL);

    // no point in drawing outside the window-update-region
    if (hrgnUpdate != NULL) {
        // work out where the line would have been on-screen
        bounds.left = (long)(-ptv->m_nHScrollPos * ptv->m_nFontWidth +
                             TextView__LeftMarginWidth(ptv));
        bounds.top =
            (long)((nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight);
        bounds.right = (long)(bounds.right);
        bounds.bottom = (long)(bounds.top + ptv->m_nLineHeight);

        //	clip the window update-region with the line's bounding rectangle
        hrgnBounds = CreateRectRgnIndirect(&bounds);
        CombineRgn(hrgnBounds, hrgnUpdate, hrgnBounds, RGN_AND);

        // work out the bounding-rectangle of this intersection
        GetRgnBox(hrgnBounds, &bounds);
        bounds.top = 0;
        bounds.bottom = ptv->m_nLineHeight;
    }

    TextView__PaintText(ptv, hdc, nLineNo,
                        xpos + TextView__LeftMarginWidth(ptv), ypos, &bounds);

    DeleteObject(hrgnBounds);
    SelectClipRgn(hdc, NULL);

    //
    //	draw the margin straight over the top
    //
    if (TextView__LeftMarginWidth(ptv) > 0) {
        TextView__PaintMargin(ptv, hdc, nLineNo, 0, 0);
    }
}

//
//	Return width of margin
//
int TextView__LeftMarginWidth(TEXTVIEW *ptv)
{
    int width = 0;
    int cx = 0;
    int cy = 0;

    // get dimensions of imagelist icons
    if (ptv->m_hImageList)
        ImageList_GetIconSize(ptv->m_hImageList, &cx, &cy);

    if (TextView__CheckStyle(ptv, TXS_LINENUMBERS)) {
        width += ptv->m_nLinenoWidth;

        if (TextView__CheckStyle(ptv, TXS_SELMARGIN) && cx > 0)
            width += cx + 4;

        if (1)
            width += 1;
        if (0)
            width += 5;

        return width;
    }
    // selection margin by itself
    else if (TextView__CheckStyle(ptv, TXS_SELMARGIN)) {
        width += cx + 4;

        if (0)
            width += 1;
        if (0)
            width += 5;

        return width;
    }

    return 0;
}

//
//	This must be called whenever the number of lines changes
//  (probably easier to call it when the file-size changes)
//
void TextView__UpdateMarginWidth(TEXTVIEW *ptv)
{
    HDC hdc = GetDC(ptv->m_hWnd);
    HANDLE hOldFont = SelectObject(hdc, ptv->m_uspFontList[0].hFont);

    TCHAR buf[32];
    int len = wsprintf(buf, LINENO_FMT, ptv->m_nLineCount);

    ptv->m_nLinenoWidth = TextView__TextWidth(ptv, hdc, buf, len);

    SelectObject(hdc, hOldFont);
    ReleaseDC(ptv->m_hWnd, hdc);
}

//
//	Draw the specified line's margin into the area described by *margin*
//
int TextView__PaintMargin(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo, int xpos,
                          int ypos)
{
    RECT rect = {xpos, ypos, xpos + TextView__LeftMarginWidth(ptv),
                 ypos + ptv->m_nLineHeight};

    int imgWidth;
    int imgHeight;
    int imgX;
    int imgY;
    int selwidth = TextView__CheckStyle(ptv, TXS_SELMARGIN) ? 20 : 0;

    TCHAR ach[32];

    // int nummaxwidth = 60;

    if (ptv->m_hImageList && selwidth > 0) {
        // selection margin must include imagelists
        ImageList_GetIconSize(ptv->m_hImageList, &imgWidth, &imgHeight);

        imgX = xpos + (selwidth - imgWidth) / 2;
        imgY = ypos + (ptv->m_nLineHeight - imgHeight) / 2;
    }

    if (TextView__CheckStyle(ptv, TXS_LINENUMBERS)) {
        HANDLE hOldFont = SelectObject(hdc, ptv->m_uspFontList[0].hFont);

        int len = wsprintf(ach, LINENO_FMT, nLineNo + 1);
        int width = TextView__TextWidth(ptv, hdc, ach, len);

        // only draw line number if in-range
        if (nLineNo >= ptv->m_nLineCount)
            len = 0;

        rect.right = rect.left + ptv->m_nLinenoWidth;

        if (TextView__CheckStyle(ptv, TXS_SELMARGIN) && ptv->m_hImageList) {
            imgX = rect.right;
            rect.right += imgWidth + 4;
        }

        SetTextColor(hdc, TextView__GetColour(ptv, TXC_LINENUMBERTEXT));
        SetBkColor(hdc, TextView__GetColour(ptv, TXC_LINENUMBER));

        ExtTextOut(hdc, rect.left + ptv->m_nLinenoWidth - width,
                   rect.top +
                       TextView__NeatTextYOffset(ptv, &ptv->m_uspFontList[0]),
                   ETO_OPAQUE | ETO_CLIPPED, &rect, ach, len, 0);

        // vertical line
        rect.left = rect.right;
        rect.right += 1;
        // PaintRect(hdc, &rect, MixRGB(GetSysColor(COLOR_3DFACE), 0xffffff));
        PaintRectPoint(hdc, &rect, TextView__GetColour(ptv, TXC_BACKGROUND));

        // bleed area - use this to draw "folding" arrows
        /*rect.left   = rect.right;
        rect.right += 5;
        PaintRect(hdc, &rect, TextView__GetColour(ptv, TXC_BACKGROUND));*/

        SelectObject(hdc, hOldFont);
    } else {
        DrawCheckedRect(hdc, &rect, TextView__GetColour(ptv, TXC_SELMARGIN1),
                        TextView__GetColour(ptv, TXC_SELMARGIN2));
    }

    //
    //	Retrieve information about this specific line
    //
    LINEINFO *linfo = TextView__GetLineInfo(ptv, nLineNo);

    if (ptv->m_hImageList && linfo && nLineNo < ptv->m_nLineCount) {
        ImageList_DrawEx(ptv->m_hImageList, linfo->nImageIdx, hdc, imgX, imgY,
                         imgWidth, imgHeight, CLR_NONE, CLR_NONE,
                         ILD_TRANSPARENT);
    }

    return rect.right - rect.left;
}

//
//	Draw a line of text into the specified device-context
//
void TextView__PaintText(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo, int xpos,
                         int ypos, RECT *bounds)
{
    USPDATA *uspData;
    ULONG lineOffset;

    // grab the USPDATA for this line
    uspData = TextView__GetUspData_offset(ptv, hdc, nLineNo, &lineOffset);

    // set highlight-colours depending on window-focus
    if (GetFocus() == ptv->m_hWnd)
        UspSetSelColor(uspData, TextView__GetColour(ptv, TXC_HIGHLIGHTTEXT),
                       TextView__GetColour(ptv, TXC_HIGHLIGHT));
    else
        UspSetSelColor(uspData, TextView__GetColour(ptv, TXC_HIGHLIGHTTEXT2),
                       TextView__GetColour(ptv, TXC_HIGHLIGHT2));

    // update selection-attribute information for the line
    UspApplySelection(uspData, ptv->m_nSelectionStart - lineOffset,
                      ptv->m_nSelectionEnd - lineOffset);

    TextView__ApplySelection(ptv, uspData, nLineNo, lineOffset,
                             uspData->stringLen);

    // draw the text!
    UspTextOut(uspData, hdc, xpos, ypos, ptv->m_nLineHeight,
               ptv->m_nHeightAbove, bounds);
}

int TextView__ApplySelection(TEXTVIEW *ptv, USPDATA *uspData, ULONG nLine,
                             ULONG nOffset, ULONG nTextLen)
{
    int selstart = 0;
    int selend = 0;

    if (ptv->m_nSelectionType != SEL_BLOCK)
        return 0;

    if (nLine >= ptv->m_cpBlockStart.line && nLine <= ptv->m_cpBlockEnd.line) {
        int trailing;

        UspXToOffset(uspData, ptv->m_cpBlockStart.xpos, &selstart, &trailing,
                     0);
        selstart += trailing;

        UspXToOffset(uspData, ptv->m_cpBlockEnd.xpos, &selend, &trailing, 0);
        selend += trailing;

        if (selstart > selend)
            selstart ^= selend ^= selstart ^= selend;
    }

    UspApplySelection(uspData, selend, selstart);

    return 0;
}

//
//	Apply visual-styles to the text by returning colour and font
//	information into the supplied TEXT_ATTR structure
//
//	nLineNo	- line number
//	nOffset	- actual offset of line within file
//
//	Returns new length of buffer if text has been modified
//
int TextView__ApplyTextAttributes(TEXTVIEW *ptv, ULONG nLineNo, ULONG nOffset,
                                  ULONG *nColumn, TCHAR *szText, int nTextLen,
                                  ATTR *attr)
{
    int i;

    ULONG selstart = min(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    ULONG selend = max(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);

    //
    //	STEP 1. Apply the "base coat"
    //
    for (i = 0; i < nTextLen; i++) {
        attr[i].len = 1;
        attr[i].font = 0;
        attr[i].eol = 0;
        attr[i].reserved = 0;

        // change the background if the line is too long
        if (*nColumn >= (ULONG)ptv->m_nLongLineLimit &&
            TextView__CheckStyle(ptv, TXS_LONGLINES)) {
            attr[i].fg = TextView__GetColour(ptv, TXC_FOREGROUND);
            attr[i].bg = TextView__LongColour(ptv, nLineNo);
        } else {
            attr[i].fg = TextView__GetColour(ptv, TXC_FOREGROUND);
            attr[i].bg = TextView__LineColour(
                ptv, nLineNo); // TextView__GetColour(ptv, TXC_BACKGROUND);
        }

        // keep track of how many columns we have processed
        if (szText[i] == '\t')
            *nColumn +=
                ptv->m_nTabWidthChars - (*nColumn % ptv->m_nTabWidthChars);
        else
            *nColumn += 1;
    }

    //
    //	TODO: 1. Apply syntax colouring first of all
    //

    //
    //	TODO: 2. Apply bookmarks, line highlighting etc (override syntax
    // colouring)
    //

    //
    //	STEP 3:  Now apply text-selection (overrides everything else)
    //
    if (ptv->m_nSelectionType == SEL_NORMAL) {
        for (i = 0; i < nTextLen; i++) {
            // highlight uses a separate attribute-flag
            if (nOffset + i >= selstart && nOffset + i < selend)
                attr[i].sel = 1;
            else
                attr[i].sel = 0;
        }
    } else if (ptv->m_nSelectionType == SEL_BLOCK) {
    }

    // TextView__SyntaxColour(ptv, szText, nTextLen, attr);

    //
    //	Turn any CR/LF at the end of a line into a single 'space' character
    //
    nTextLen = TextView__StripCRLF(ptv, szText, attr, nTextLen, FALSE);

    //
    //	Finally identify control-characters (after CR/LF has been changed to
    //'space')
    //
    for (i = 0; i < nTextLen; i++) {
        ULONG ch = szText[i];
        attr[i].ctrl = ch < 0x20 ? 1 : 0;
        if (ch == '\r' || ch == '\n')
            attr[i].eol = TRUE;
    }

    return nTextLen;
}

void PaintRect(HDC hdc, int x, int y, int width, int height, COLORREF fill)
{
    RECT rect = {x, y, x + width, y + height};

    fill = SetBkColor(hdc, fill);
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, 0, 0, 0);
    SetBkColor(hdc, fill);
}

void PaintRectPoint(HDC hdc, RECT *rect, COLORREF fill)
{
    fill = SetBkColor(hdc, fill);
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, rect, 0, 0, 0);
    SetBkColor(hdc, fill);
}

int TextView__CRLF_size(TEXTVIEW *ptv, TCHAR *szText, int nLength)
{
    if (nLength >= 2) {
        if (szText[nLength - 2] == '\r' && szText[nLength - 1] == '\n')
            return 2;
    }

    if (nLength >= 1) {
        if (szText[nLength - 1] == '\r' || szText[nLength - 1] == '\n' ||
            szText[nLength - 1] == '\x0b' || szText[nLength - 1] == '\x0c' ||
            szText[nLength - 1] == '\x85' || szText[nLength - 1] == 0x2028 ||
            szText[nLength - 1] == 0x2029)
            return 1;
    }

    return 0;
}

//
//	Strip CR/LF combinations from the end of a line and
//  replace with a single space character (for drawing purposes)
//
int TextView__StripCRLF(TEXTVIEW *ptv, TCHAR *szText, ATTR *attr, int nLength,
                        BOOL fAllow)
{
    if (nLength >= 2) {
        if (szText[nLength - 2] == '\r' && szText[nLength - 1] == '\n') {
            attr[nLength - 2].eol = TRUE;

            if (ptv->m_nCRLFMode & TXL_CRLF) {
                // convert CRLF to a single space
                szText[nLength - 2] = ' ';
                return nLength - 1 - (int)fAllow;
            } else {
                return nLength;
            }
        }
    }

    if (nLength >= 1) {
        if (szText[nLength - 1] == '\x0b' || szText[nLength - 1] == '\x0c' ||
            szText[nLength - 1] == 0x0085 || szText[nLength - 1] == 0x2029 ||
            szText[nLength - 1] == 0x2028) {
            attr[nLength - 1].eol = TRUE;
            // szText[nLength-1] = ' ';
            return nLength - 0; //(int)fAllow;
        }

        if (szText[nLength - 1] == '\r') {
            attr[nLength - 1].eol = TRUE;

            if (ptv->m_nCRLFMode & TXL_CR) {
                szText[nLength - 1] = ' ';
                return nLength - (int)fAllow;
            }
        }

        if (szText[nLength - 1] == '\n') {
            attr[nLength - 1].eol = TRUE;

            if (ptv->m_nCRLFMode & TXL_LF) {
                szText[nLength - 1] = ' ';
                return nLength - (int)fAllow;
            }
        }
    }

    return nLength;
}

//
//
//
COLORREF TextView__LineColour(TEXTVIEW *ptv, ULONG nLineNo)
{
    if (ptv->m_nCurrentLine == nLineNo &&
        TextView__CheckStyle(ptv, TXS_HIGHLIGHTCURLINE))
        return TextView__GetColour(ptv, TXC_CURRENTLINE);
    else
        return TextView__GetColour(ptv, TXC_BACKGROUND);
}

COLORREF TextView__LongColour(TEXTVIEW *ptv, ULONG nLineNo)
{
    if (ptv->m_nCurrentLine == nLineNo &&
        TextView__CheckStyle(ptv, TXS_HIGHLIGHTCURLINE))
        return TextView__GetColour(ptv, TXC_CURRENTLINE);
    else
        return TextView__GetColour(ptv, TXC_LONGLINE);
}

COLORREF MixRGB(COLORREF rgbCol1, COLORREF rgbCol2)
{
    return RGB((GetRValue(rgbCol1) + GetRValue(rgbCol2)) / 2,
               (GetGValue(rgbCol1) + GetGValue(rgbCol2)) / 2,
               (GetBValue(rgbCol1) + GetBValue(rgbCol2)) / 2);
}

COLORREF RealizeColour(COLORREF col)
{
    COLORREF result = col;

    if (col & 0x80000000)
        result = GetSysColor(col & 0xff);

    if (col & 0x40000000)
        result = MixRGB(GetSysColor((col & 0xff00) >> 8), result);

    if (col & 0x20000000)
        result = MixRGB(GetSysColor((col & 0xff00) >> 8), result);

    return result;
}

//
//	Return an RGB value corresponding to the specified HVC_xxx index
//
//	If the RGB value has the top bit set (0x80000000) then it is
//  not a real RGB value - instead the low 29bits specify one
//  of the GetSysColor COLOR_xxx indices. This allows us to use
//	system colours without worrying about colour-scheme changes etc.
//
COLORREF TextView__GetColour(TEXTVIEW *ptv, UINT idx)
{
    if (idx >= TXC_MAX_COLOURS)
        return 0;

    return REALIZE_SYSCOL(ptv->m_rgbColourList[idx]);
}

COLORREF TextView__SetColour(TEXTVIEW *ptv, UINT idx, COLORREF rgbColour)
{
    COLORREF rgbOld;

    if (idx >= TXC_MAX_COLOURS)
        return 0;

    rgbOld = ptv->m_rgbColourList[idx];
    ptv->m_rgbColourList[idx] = rgbColour;

    TextView__ResetLineCache(ptv);

    return rgbOld;
}

//
//	Paint a checkered rectangle, with each alternate
//	pixel being assigned a different colour
//
void DrawCheckedRect(HDC hdc, RECT *rect, COLORREF fg, COLORREF bg)
{
    static WORD wCheckPat[8] = {0xaaaa, 0x5555, 0xaaaa, 0x5555,
                                0xaaaa, 0x5555, 0xaaaa, 0x5555};

    HBITMAP hbmp;
    HBRUSH hbr, hbrold;
    COLORREF fgold, bgold;

    hbmp = CreateBitmap(8, 8, 1, 1, wCheckPat);
    hbr = CreatePatternBrush(hbmp);

    SetBrushOrgEx(hdc, rect->left, 0, 0);
    hbrold = (HBRUSH)SelectObject(hdc, hbr);

    fgold = SetTextColor(hdc, fg);
    bgold = SetBkColor(hdc, bg);

    PatBlt(hdc, rect->left, rect->top, rect->right - rect->left,
           rect->bottom - rect->top, PATCOPY);

    SetBkColor(hdc, bgold);
    SetTextColor(hdc, fgold);

    SelectObject(hdc, hbrold);
    DeleteObject(hbr);
    DeleteObject(hbmp);
}
