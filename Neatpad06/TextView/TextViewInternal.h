#ifndef TEXTVIEW_INTERNAL_INCLUDED
#define TEXTVIEW_INTERNAL_INCLUDED

#define TEXTBUFSIZE 32

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
    ULONG m_nCursorOffset;

    COLORREF m_rgbColourList[TXC_MAX_COLOURS];

    // Runtime data
    BOOL m_fMouseDown;
    UINT m_nScrollTimer;
    int m_nScrollCounter;

    // File-related data
    ULONG m_nLineCount;

    TEXTDOCUMENT *m_pTextDoc;
} TEXTVIEW;

TEXTVIEW *TextView__new(HWND hwnd);
void TextView__delete(TEXTVIEW *ptv);

//
//	Message handlers
//
LONG TextView__OnPaint(TEXTVIEW *ptv);
LONG TextView__OnSetFont(TEXTVIEW *ptv, HFONT hFont);
LONG TextView__OnSize(TEXTVIEW *ptv, UINT nFlags, int width, int height);
LONG TextView__OnVScroll(TEXTVIEW *ptv, UINT nSBCode, UINT nPos);
LONG TextView__OnHScroll(TEXTVIEW *ptv, UINT nSBCode, UINT nPos);
LONG TextView__OnMouseWheel(TEXTVIEW *ptv, int nDelta);
LONG TextView__OnTimer(TEXTVIEW *ptv, UINT nTimer);

LONG TextView__OnMouseActivate(TEXTVIEW *ptv, HWND hwndTop, UINT nHitTest,
                               UINT nMessage);
LONG TextView__OnLButtonDown(TEXTVIEW *ptv, UINT nFlags, int x, int y);
LONG TextView__OnLButtonUp(TEXTVIEW *ptv, UINT nFlags, int x, int y);
LONG TextView__OnMouseMove(TEXTVIEW *ptv, UINT nFlags, int x, int y);

LONG TextView__OnSetFocus(TEXTVIEW *ptv, HWND hwndOld);
LONG TextView__OnKillFocus(TEXTVIEW *ptv, HWND hwndNew);

//
//	Public interface
//
LONG TextView__OpenFile(TEXTVIEW *ptv, TCHAR *szFileName);
LONG TextView__ClearFile(TEXTVIEW *ptv);

LONG TextView__AddFont(TEXTVIEW *ptv, HFONT);
LONG TextView__SetFont(TEXTVIEW *ptv, HFONT, int idx);
LONG TextView__SetLineSpacing(TEXTVIEW *ptv, int nAbove, int nBelow);
COLORREF TextView__SetColour(TEXTVIEW *ptv, UINT idx, COLORREF rgbColour);

// private
void TextView__PaintLine(TEXTVIEW *ptv, HDC hdc, ULONG line);
void TextView__PaintText(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo, RECT *rect);

int TextView__ApplyTextAttributes(TEXTVIEW *ptv, ULONG nLineNo, ULONG offset,
                                  TCHAR *szText, int nTextLen, ATTR *attr);
int TextView__NeatTextOut(TEXTVIEW *ptv, HDC hdc, int xpos, int ypos,
                          TCHAR *szText, int nLen, int nTabOrigin, ATTR *attr);

int TextView__PaintCtrlChar(TEXTVIEW *ptv, HDC hdc, int xpos, int ypos,
                            ULONG chValue, FONT *fa);
void TextView__InitCtrlCharFontAttr(TEXTVIEW *ptv, HDC hdc, FONT *fa);

void TextView__RefreshWindow(TEXTVIEW *ptv);
LONG TextView__InvalidateRange(TEXTVIEW *ptv, ULONG nStart, ULONG nFinish);

int TextView__CtrlCharWidth(TEXTVIEW *ptv, HDC hdc, ULONG chValue, FONT *fa);
int TextView__NeatTextWidth(TEXTVIEW *ptv, HDC hdc, TCHAR *buf, int len,
                            int nTabOrigin);
int TextView__TabWidth(TEXTVIEW *ptv);

BOOL TextView__MouseCoordToFilePos(TEXTVIEW *ptv, int x, int y, ULONG *pnLineNo,
                                   ULONG *pnCharOffset, ULONG *pnFileOffset,
                                   int *px);
ULONG TextView__RepositionCaret(TEXTVIEW *ptv);

COLORREF TextView__GetColour(TEXTVIEW *ptv, UINT idx);

VOID TextView__SetupScrollbars(TEXTVIEW *ptv);
VOID TextView__UpdateMetrics(TEXTVIEW *ptv);
VOID TextView__RecalcLineHeight(TEXTVIEW *ptv);
BOOL TextView__PinToBottomCorner(TEXTVIEW *ptv);

void TextView__Scroll(TEXTVIEW *ptv, int dx, int dy);
HRGN TextView__ScrollRgn(TEXTVIEW *ptv, int dx, int dy, BOOL fReturnUpdateRgn);

#endif
