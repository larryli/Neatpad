#ifndef TEXTVIEW_INTERNAL_INCLUDED
#define TEXTVIEW_INTERNAL_INCLUDED

#define LONGEST_LINE 0x100

#include "TextDocument.h"

typedef struct {
    HWND m_hWnd;

    // Font-related data
    HFONT m_hFont;
    int m_nFontWidth;
    int m_nFontHeight;

    // Scrollbar related data
    ULONG m_nVScrollPos;
    ULONG m_nVScrollMax;
    int m_nHScrollPos;
    int m_nHScrollMax;

    int m_nLongestLine;
    int m_nWindowLines;
    int m_nWindowColumns;

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

// private
void TextView__PaintLine(TEXTVIEW *ptv, HDC hdc, ULONG line);
void TextView__TabbedExtTextOut(TEXTVIEW *ptv, HDC hdc, RECT *rect, TCHAR *buf,
                                int len);
void TextView__RefreshWindow(TEXTVIEW *ptv);
COLORREF TextView__GetColour(TEXTVIEW *ptv, UINT idx);
VOID TextView__SetupScrollbars(TEXTVIEW *ptv);
VOID TextView__UpdateMetrics(TEXTVIEW *ptv);
BOOL TextView__PinToBottomCorner(TEXTVIEW *ptv);
void TextView__Scroll(TEXTVIEW *ptv, int dx, int dy);

#endif
