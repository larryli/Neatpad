#ifndef TEXTVIEW_INTERNAL_INCLUDED
#define TEXTVIEW_INTERNAL_INCLUDED

#define LONGEST_LINE 0x100

#include "TextDocument.h"

//
//	ATTR - text character attribute
//
typedef struct {
    COLORREF fg; // foreground colour
    COLORREF bg; // background colour
    ULONG style; // possible font-styling information
} ATTR;

//
//	FONT - font attributes
//
typedef struct {
    // Windows font information
    HFONT hFont;
    TEXTMETRIC tm;

    // dimensions needed for control-character 'bitmaps'
    int nInternalLeading;
    int nDescent;
} FONT;

// maximum fonts that a TextView can hold
#define MAX_FONTS 32

//
//	TextView - internal window implementation
//
typedef struct {
    HWND m_hWnd;

    // Font-related data
    FONT m_FontAttr[MAX_FONTS];
    int m_nNumFonts;
    int m_nFontWidth;
    int m_nMaxAscent;
    int m_nLineHeight;
    int m_nHeightAbove;
    int m_nHeightBelow;

    // Scrollbar-related data
    ULONG m_nVScrollPos;
    ULONG m_nVScrollMax;
    int m_nHScrollPos;
    int m_nHScrollMax;

    int m_nLongestLine;
    int m_nWindowLines;
    int m_nWindowColumns;

    // Display-related data
    int m_nTabWidthChars;
    ULONG m_nSelectionStart;
    ULONG m_nSelectionEnd;

    // File-related data
    ULONG m_nLineCount;

    TEXTDOCUMENT *m_pTextDoc;
} TEXTVIEW;

TEXTVIEW *TextView__new(HWND hwnd);
void TextView__delete(TEXTVIEW *ptv);
LONG TextView__OnPaint(TEXTVIEW *ptv);
LONG TextView__OnSetFont(TEXTVIEW *ptv, HFONT hFont);
LONG TextView__OnSize(TEXTVIEW *ptv, UINT nFlags, int width, int height);
LONG TextView__OnVScroll(TEXTVIEW *ptv, UINT nSBCode, UINT nPos);
LONG TextView__OnHScroll(TEXTVIEW *ptv, UINT nSBCode, UINT nPos);
LONG TextView__OnMouseWheel(TEXTVIEW *ptv, int nDelta);
LONG TextView__OpenFile(TEXTVIEW *ptv, TCHAR *szFileName);
LONG TextView__ClearFile(TEXTVIEW *ptv);
LONG TextView__AddFont(TEXTVIEW *ptv, HFONT);
LONG TextView__SetFont(TEXTVIEW *ptv, HFONT, int idx);
LONG TextView__SetLineSpacing(TEXTVIEW *ptv, int nAbove, int nBelow);
// private
void TextView__PaintLine(TEXTVIEW *ptv, HDC hdc, ULONG line);
void TextView__PaintText(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo, RECT *rect);
int TextView__ApplyTextAttributes(TEXTVIEW *ptv, ULONG nLineNo, ULONG offset,
                                  TCHAR *szText, int nTextLen, ATTR *attr);
int TextView__NeatTextOut(TEXTVIEW *ptv, HDC hdc, int xpos, int ypos,
                          TCHAR *szText, int nLen, int nTabOrigin, ATTR *attr);
int TextView__PaintCtrlChar(TEXTVIEW *ptv, HDC hdc, int xpos, int ypos,
                            ULONG chValue, FONT *font);
void TextView__InitCtrlCharFontAttr(TEXTVIEW *ptv, HDC hdc, FONT *font);
void TextView__RefreshWindow(TEXTVIEW *ptv);
COLORREF TextView__GetColour(TEXTVIEW *ptv, UINT idx);
VOID TextView__RecalcLineHeight(TEXTVIEW *ptv);
VOID TextView__SetupScrollbars(TEXTVIEW *ptv);
VOID TextView__UpdateMetrics(TEXTVIEW *ptv);
BOOL TextView__PinToBottomCorner(TEXTVIEW *ptv);
void TextView__Scroll(TEXTVIEW *ptv, int dx, int dy);

#endif
