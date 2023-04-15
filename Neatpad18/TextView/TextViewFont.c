//
//  MODULE:     TextViewFont.cpp
//
//  PURPOSE:    Font support for the TextView control
//
//  NOTES:      www.catch22.net
//

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

//
//  TextView__
//
int TextView__NeatTextYOffset(TEXTVIEW *ptv, USPFONT *font)
{
    return ptv->m_nMaxAscent + ptv->m_nHeightAbove - font->tm.tmAscent;
}

int TextView__TextWidth(TEXTVIEW *ptv, HDC hdc, TCHAR *buf, int len)
{
    SIZE sz;
    if (len == -1)
        len = lstrlen(buf);
    GetTextExtentPoint32(hdc, buf, len, &sz);
    return sz.cx;
}

//
//  Update the lineheight based on current font settings
//
VOID TextView__RecalcLineHeight(TEXTVIEW *ptv)
{
    ptv->m_nLineHeight = 0;
    ptv->m_nMaxAscent = 0;

    // find the tallest font in the TextView
    for (int i = 0; i < ptv->m_nNumFonts; i++) {
        // always include a font's external-leading
        int fontheight = ptv->m_uspFontList[i].tm.tmHeight +
                         ptv->m_uspFontList[i].tm.tmExternalLeading;

        ptv->m_nLineHeight = max(ptv->m_nLineHeight, fontheight);
        ptv->m_nMaxAscent =
            max(ptv->m_nMaxAscent, ptv->m_uspFontList[i].tm.tmAscent);
    }

    // add on the above+below spacings
    ptv->m_nLineHeight += ptv->m_nHeightAbove + ptv->m_nHeightBelow;

    // force caret resize if we've got the focus
    if (GetFocus() == ptv->m_hWnd) {
        TextView__OnKillFocus(ptv, 0);
        TextView__OnSetFocus(ptv, 0);
    }
}

//
//  Set a font for the TextView
//
LONG TextView__SetFont(TEXTVIEW *ptv, HFONT hFont, int idx)
{
    USPFONT *uspFont = &(ptv->m_uspFontList[idx]);

    // need a DC to query font data
    HDC hdc = GetDC(ptv->m_hWnd);

    // Initialize the font for USPLIB
    UspFreeFont(uspFont);
    UspInitFont(uspFont, hdc, hFont);

    ReleaseDC(ptv->m_hWnd, hdc);

    // calculate new line metrics
    ptv->m_nFontWidth = ptv->m_uspFontList[0].tm.tmAveCharWidth;

    TextView__RecalcLineHeight(ptv);
    TextView__UpdateMarginWidth(ptv);

    TextView__ResetLineCache(ptv);

    return 0;
}

//
//  Add a secondary font to the TextView
//
LONG TextView__AddFont(TEXTVIEW *ptv, HFONT hFont)
{
    int idx = ptv->m_nNumFonts++;

    TextView__SetFont(ptv, hFont, idx);
    TextView__UpdateMetrics(ptv);

    return 0;
}

//
//  WM_SETFONT handler: set a new default font
//
LONG TextView__OnSetFont(TEXTVIEW *ptv, HFONT hFont)
{
    // default font is always #0
    TextView__SetFont(ptv, hFont, 0);
    TextView__UpdateMetrics(ptv);

    return 0;
}

//
//  Set spacing (in pixels) above and below each line -
//  this is in addition to the external-leading of a font
//
LONG TextView__SetLineSpacing(TEXTVIEW *ptv, int nAbove, int nBelow)
{
    ptv->m_nHeightAbove = nAbove;
    ptv->m_nHeightBelow = nBelow;
    TextView__RecalcLineHeight(ptv);
    return TRUE;
}
