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

int StripCRLF(TCHAR *szText, int nLength);
void PaintRect(HDC hdc, int x, int y, int width, int height, COLORREF fill);
void DrawCheckedRect(HDC hdc, RECT *rect, COLORREF fg, COLORREF bg);

//
//	Perform a full redraw of the entire window
//
VOID TextView__RefreshWindow(TEXTVIEW *ptv)
{
    InvalidateRect(ptv->m_hWnd, NULL, FALSE);
}

//
//	Painting procedure for TextView objects
//
LONG TextView__OnPaint(TEXTVIEW *ptv)
{
    PAINTSTRUCT ps;

    ULONG i, first, last;

    BeginPaint(ptv->m_hWnd, &ps);

    // figure out which lines to redraw
    first = ptv->m_nVScrollPos + ps.rcPaint.top / ptv->m_nLineHeight;
    last = ptv->m_nVScrollPos + ps.rcPaint.bottom / ptv->m_nLineHeight;

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
//	Draw the specified line (including margins etc)
//
void TextView__PaintLine(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo)
{
    RECT rect;

    // Get the area we want to update
    GetClientRect(ptv->m_hWnd, &rect);

    // calculate rectangle for entire length of line in window
    rect.left = (long)(-ptv->m_nHScrollPos * ptv->m_nFontWidth);
    rect.top = (long)((nLineNo - ptv->m_nVScrollPos) * ptv->m_nLineHeight);
    rect.right = (long)(rect.right);
    rect.bottom = (long)(rect.top + ptv->m_nLineHeight);

    //
    //	do things like margins, line numbers etc. here
    //

    //
    //	check we have data to draw on this line
    //
    if (nLineNo >= ptv->m_nLineCount) {
        SetBkColor(hdc, TextView__GetColour(ptv, TXC_BACKGROUND));
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, 0, 0, 0);
        return;
    }

    //
    //	paint the line's text
    //
    TextView__PaintText(ptv, hdc, nLineNo, &rect);
}

//
//	Draw a line of text into the TextView window
//
void TextView__PaintText(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo, RECT *rect)
{
    TCHAR buff[TEXTBUFSIZE];
    ATTR attr[TEXTBUFSIZE];

    ULONG charoff = 0;
    int len;

    int xpos = rect->left;
    int ypos = rect->top;
    int xtab = rect->left;

    //
    //	TODO: Clip text to left side of window
    //

    //
    //	Keep drawing until we reach the edge of the window
    //
    while (xpos < rect->right) {
        ULONG fileoff;
        int lasti = 0;
        int i;

        //
        //	Get a block of text for drawing
        //
        if ((len = TextDocument__getline_offset_off(ptv->m_pTextDoc, nLineNo,
                                                    charoff, buff, TEXTBUFSIZE,
                                                    &fileoff)) == 0)
            break;

        // ready for the next block of characters (do this before stripping
        // CR/LF)
        fileoff += charoff;
        charoff += len;

        //
        //	Apply text attributes -
        //	i.e. syntax highlighting, mouse selection colours etc.
        //
        // len = ApplyTextAttributes(nLineNo, fileoff+charoff, buff, len, attr);
        len = TextView__ApplyTextAttributes(ptv, nLineNo, fileoff, buff, len,
                                            attr);

        //
        //	Display the text by breaking it into spans of colour/style
        //
        for (i = 0; i <= len; i++) {
            // if the colour or font changes, then need to output
            if (i == len || attr[i].fg != attr[lasti].fg ||
                attr[i].bg != attr[lasti].bg ||
                attr[i].style != attr[lasti].style) {
                xpos +=
                    TextView__NeatTextOut(ptv, hdc, xpos, ypos, buff + lasti,
                                          i - lasti, xtab, &attr[lasti]);

                lasti = i;
            }
        }
    }

    //
    // Erase to the end of the line
    //
    rect->left = xpos;
    SetBkColor(hdc, TextView__GetColour(ptv, TXC_BACKGROUND));
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, rect, 0, 0, 0);
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
                                  TCHAR *szText, int nTextLen, ATTR *attr)
{
    int font = nLineNo % ptv->m_nNumFonts;
    int i;

    ULONG selstart = min(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    ULONG selend = max(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);

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
    for (i = 0; i < nTextLen; i++) {
        // apply highlight colours
        if (nOffset + i >= selstart && nOffset + i < selend) {
            if (GetFocus() == ptv->m_hWnd) {
                attr[i].fg = TextView__GetColour(ptv, TXC_HIGHLIGHTTEXT);
                attr[i].bg = TextView__GetColour(ptv, TXC_HIGHLIGHT);
            } else {
                attr[i].fg = TextView__GetColour(ptv, TXC_HIGHLIGHTTEXT2);
                attr[i].bg = TextView__GetColour(ptv, TXC_HIGHLIGHT2);
            }
        }
        // normal text colours
        else {
            attr[i].fg = TextView__GetColour(ptv, TXC_FOREGROUND);
            attr[i].bg = TextView__GetColour(ptv, TXC_BACKGROUND);
        }

        if (szText[i] == ' ')
            font = (font + 1) % ptv->m_nNumFonts;

        attr[i].style = font;
    }

    //
    //	Turn any CR/LF at the end of a line into a single 'space' character
    //
    return StripCRLF(szText, nTextLen);
}

//
//	Draw tabbed text using specified colours and font, return width of output
// text
//
//	We could just use TabbedTextOut - however, we need to parse the text because
//  it might contain ascii-control characters which must be output separately.
//  because of this we'll just handle the tabs at the same time.
//
int TextView__NeatTextOut(TEXTVIEW *ptv, HDC hdc, int xpos, int ypos,
                          TCHAR *szText, int nLen, int nTabOrigin, ATTR *attr)
{
    int i;
    int xold = xpos;
    int lasti = 0;
    SIZE sz;

    const int TABWIDTHPIXELS = ptv->m_nTabWidthChars * ptv->m_nFontWidth;

    FONT *font = &ptv->m_FontAttr[attr->style];

    // configure the device context
    SetTextColor(hdc, attr->fg);
    SetBkColor(hdc, attr->bg);
    SelectObject(hdc, font->hFont);

    // loop over each character
    for (i = 0; i <= nLen; i++) {
        int yoff = ptv->m_nMaxAscent + ptv->m_nHeightAbove - font->tm.tmAscent;

        // output any "deferred" text before handling tab/control chars
        if (i == nLen || szText[i] == '\t' || szText[i] < 32) {
            RECT rect;

            // get size of text
            GetTextExtentPoint32(hdc, szText + lasti, i - lasti, &sz);
            SetRect(&rect, xpos, ypos, xpos + sz.cx, ypos + ptv->m_nLineHeight);

            // draw the text and erase it's background at the same time
            ExtTextOut(hdc, xpos, ypos + yoff, ETO_CLIPPED | ETO_OPAQUE, &rect,
                       szText + lasti, i - lasti, 0);

            xpos += sz.cx;
        }

        // special case for TAB and Control characters
        if (i < nLen) {
            // TAB characters
            if (szText[i] == '\t') {
                // calculate distance in pixels to the next tab-stop
                int width =
                    TABWIDTHPIXELS - ((xpos - nTabOrigin) % TABWIDTHPIXELS);

                // draw a blank space
                PaintRect(hdc, xpos, ypos, width, ptv->m_nLineHeight, attr->bg);

                xpos += width;
                lasti = i + 1;
            }
            // ASCII-CONTROL characters
            else if (szText[i] < 32) {
                xpos += TextView__PaintCtrlChar(ptv, hdc, xpos, ypos, szText[i],
                                                font);
                lasti = i + 1;
            }
        }
    }

    // return the width of text drawn
    return xpos - xold;
}

void PaintRect(HDC hdc, int x, int y, int width, int height, COLORREF fill)
{
    RECT rect = {x, y, x + width, y + height};

    fill = SetBkColor(hdc, fill);
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, 0, 0, 0);
    SetBkColor(hdc, fill);
}

//
//	Strip CR/LF combinations from the end of a line and
//  replace with a single space character (for drawing purposes)
//
int StripCRLF(TCHAR *szText, int nLength)
{
    if (nLength >= 2) {
        if (szText[nLength - 2] == '\r' && szText[nLength - 1] == '\n') {
            szText[nLength - 2] = ' ';
            return --nLength;
        }
    }

    if (nLength >= 1) {
        if (szText[nLength - 1] == '\r' || szText[nLength - 1] == '\n') {
            szText[nLength - 1] = ' ';
            nLength--;
        }
    }

    return nLength;
}

//
//	Return an RGB value corresponding to the specified HVC_xxx index
//
//	If the RGB value has the top bit set (0x80000000) then it is
//  not a real RGB value - instead the low 31bits specify one
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
