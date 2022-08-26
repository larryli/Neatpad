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

    // File-related data
    ULONG m_nLineCount;

    TEXTDOCUMENT *m_pTextDoc;
} TEXTVIEW;

TEXTVIEW *TextView__new(HWND hwnd);
void TextView__delete(TEXTVIEW *ptv);
LONG TextView__OnPaint(TEXTVIEW *ptv);
LONG TextView__OnSetFont(TEXTVIEW *ptv, HFONT hFont);
LONG TextView__OpenFile(TEXTVIEW *ptv, TCHAR *szFileName);
LONG TextView__ClearFile(TEXTVIEW *ptv);
// private
void TextView__PaintLine(TEXTVIEW *ptv, HDC hdc, ULONG line);
void TextView__TabbedExtTextOut(TEXTVIEW *ptv, HDC hdc, RECT *rect, TCHAR *buf,
                                 int len);
COLORREF TextView__GetTextViewColor(TEXTVIEW *ptv, UINT idx);

#endif
