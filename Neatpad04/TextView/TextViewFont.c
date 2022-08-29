//
//	MODULE:		TextViewFont.cpp
//
//	PURPOSE:	Font support for the TextView control
//
//	NOTES:		www.catch22.net
//
#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

static const char *CtrlStr(DWORD ch)
{
    static const char *reps[] = {
        "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
        "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
        "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
        "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US"};

    return ch < ' ' ? reps[ch] : "???";
}

void PaintRect(HDC hdc, RECT *rect, COLORREF fill)
{
    fill = SetBkColor(hdc, fill);

    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, rect, 0, 0, 0);

    SetBkColor(hdc, fill);
}

//
//	Manually calculate the internal-leading and descent
//  values for a font by parsing a small bitmap of a single letter "E"
//	and locating the top and bottom of this letter.
//
void TextView__InitCtrlCharFontAttr(TEXTVIEW *ptv, HDC hdc, FONT *font)
{
    // create a temporary off-screen bitmap
    HDC hdcTemp = CreateCompatibleDC(hdc);
    HBITMAP hbmTemp =
        CreateBitmap(font->tm.tmAveCharWidth, font->tm.tmHeight, 1, 1, 0);
    HANDLE hbmOld = SelectObject(hdcTemp, hbmTemp);
    HANDLE hfnOld = SelectObject(hdcTemp, font->hFont);

    // black-on-white text
    SetTextColor(hdcTemp, RGB(0, 0, 0));
    SetBkColor(hdcTemp, RGB(255, 255, 255));
    SetBkMode(hdcTemp, OPAQUE);

    TextOut(hdcTemp, 0, 0, "E", 1);

    font->nInternalLeading = font->tm.tmInternalLeading;
    font->nDescent = font->tm.tmDescent;

    // scan downwards looking for the top of the letter 'E'
    for (int y = 0; y < font->tm.tmHeight; y++) {
        COLORREF col;

        if ((col = GetPixel(hdcTemp, font->tm.tmAveCharWidth / 2, y)) ==
            RGB(0, 0, 0)) {
            font->nInternalLeading = y;
            break;
        }
    }

    // scan upwards looking for the bottom of the letter 'E'
    for (int y = font->tm.tmHeight - 1; y >= 0; y--) {
        COLORREF col;

        if ((col = GetPixel(hdcTemp, font->tm.tmAveCharWidth / 2, y)) ==
            RGB(0, 0, 0)) {
            font->nDescent = font->tm.tmHeight - y - 1;
            break;
        }
    }

    // give larger fonts a thicker border
    if (font->nInternalLeading > 1 && font->nDescent > 1 &&
        font->tm.tmHeight > 18) {
        font->nInternalLeading--;
        font->nDescent--;
    }

    // cleanup
    SelectObject(hdcTemp, hbmOld);
    SelectObject(hdcTemp, hfnOld);
    DeleteDC(hdcTemp);
    DeleteObject(hbmTemp);
}

//
//	Display an ASCII control character in inverted colours
//  to what is currently set in the DC
//
int TextView__PaintCtrlChar(TEXTVIEW *ptv, HDC hdc, int xpos, int ypos,
                            ULONG chValue, FONT *font)
{
    SIZE sz;
    RECT rect;
    const char *str = CtrlStr(chValue % 32);

    int yoff = ptv->m_nMaxAscent + ptv->m_nHeightAbove - font->tm.tmAscent;

    COLORREF fg = GetTextColor(hdc);
    COLORREF bg = GetBkColor(hdc);

    // find out how big the text will be
    GetTextExtentPoint32(hdc, str, strlen(str), &sz);
    SetRect(&rect, xpos, ypos, xpos + sz.cx + 4, ypos + ptv->m_nLineHeight);

    // paint the background white
    PaintRect(hdc, &rect, bg);

    // adjust rectangle for first black block
    rect.right -= 1;
    rect.top += font->nInternalLeading + yoff;
    rect.bottom =
        rect.top + font->tm.tmHeight - font->nDescent - font->nInternalLeading;

    // paint the first black block
    PaintRect(hdc, &rect, fg);

    // prepare device context
    fg = SetTextColor(hdc, bg);
    bg = SetBkColor(hdc, fg);

    // paint the text and the second "black" block at the same time
    InflateRect(&rect, -1, 1);
    ExtTextOut(hdc, xpos + 1, ypos + yoff, ETO_OPAQUE | ETO_CLIPPED, &rect, str,
               strlen(str), 0);

    // restore device context
    SetTextColor(hdc, fg);
    SetBkColor(hdc, bg);

    return sz.cx + 4;
}

//
//	Update the lineheight based on current font settings
//
VOID TextView__RecalcLineHeight(TEXTVIEW *ptv)
{
    ptv->m_nLineHeight = 0;
    ptv->m_nMaxAscent = 0;

    // find the tallest font in the TextView
    for (int i = 0; i < ptv->m_nNumFonts; i++) {
        // always include a font's external-leading
        int fontheight = ptv->m_FontAttr[i].tm.tmHeight +
                         ptv->m_FontAttr[i].tm.tmExternalLeading;

        ptv->m_nLineHeight = max(ptv->m_nLineHeight, fontheight);
        ptv->m_nMaxAscent =
            max(ptv->m_nMaxAscent, ptv->m_FontAttr[i].tm.tmAscent);
    }

    // add on the above+below spacings
    ptv->m_nLineHeight += ptv->m_nHeightAbove + ptv->m_nHeightBelow;
}

//
//	Set a font for the TextView
//
LONG TextView__SetFont(TEXTVIEW *ptv, HFONT hFont, int idx)
{
    FONT *font = &ptv->m_FontAttr[idx];

    // need a DC to query font data
    HDC hdc = GetDC(0);
    HANDLE hold = SelectObject(hdc, hFont);

    // get font settings
    font->hFont = hFont;
    GetTextMetrics(hdc, &font->tm);

    // pre-calc the control-characters for this font
    TextView__InitCtrlCharFontAttr(ptv, hdc, font);

    // cleanup
    SelectObject(hdc, hold);
    ReleaseDC(0, hdc);

    TextView__RecalcLineHeight(ptv);

    return 0;
}

//
//	Add a secondary font to the TextView
//
LONG TextView__AddFont(TEXTVIEW *ptv, HFONT hFont)
{
    int idx = ptv->m_nNumFonts++;

    TextView__SetFont(ptv, hFont, idx);
    TextView__UpdateMetrics(ptv);

    return 0;
}

//
//	WM_SETFONT handler: set a new default font
//
LONG TextView__OnSetFont(TEXTVIEW *ptv, HFONT hFont)
{
    // default font is always #0
    TextView__SetFont(ptv, hFont, 0);

    ptv->m_nFontWidth = ptv->m_FontAttr[0].tm.tmAveCharWidth;
    TextView__UpdateMetrics(ptv);

    return 0;
}

//
//	Set spacing (in pixels) above and below each line -
//  this is in addition to the external-leading of a font
//
LONG TextView__SetLineSpacing(TEXTVIEW *ptv, int nAbove, int nBelow)
{
    ptv->m_nHeightAbove = nAbove;
    ptv->m_nHeightBelow = nBelow;
    TextView__RecalcLineHeight(ptv);
    return TRUE;
}
