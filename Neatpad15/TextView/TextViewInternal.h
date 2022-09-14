#ifndef TEXTVIEW_INTERNAL_INCLUDED
#define TEXTVIEW_INTERNAL_INCLUDED

#define TEXTBUFSIZE 128
#define LINENO_FMT _T(" %d ")
#define LINENO_PAD 8

#include "TextDocument.h"
#include <commctrl.h>

#include "..\UspLib\usplib.h"

typedef struct {
    USPDATA *uspData;
    ULONG lineno;
    ULONG offset;
    ULONG usage;

} USP_CACHE;

#define USP_CACHE_SIZE 200

//
//	ATTR - text character attribute
//
/*
typedef struct
{
    COLORREF fg; // foreground colour
    COLORREF bg; // background colour
    ULONG style; // possible font-styling information

    int len; // not used should be "1"

} ATTR;*/

/*//
//	FONT - font attributes
//
typedef struct
{
    // Windows font information
    HFONT hFont;
    TEXTMETRIC tm;

    // dimensions needed for control-character 'bitmaps'
    int nInternalLeading;
    int nDescent;

} FONT;*/

//
//	LINEINFO - information about a specific line
//
typedef struct {
    ULONG nLineNo;
    int nImageIdx;

    // more here in the future?

} LINEINFO;

typedef int(__cdecl *COMPAREPROC)(const void *, const void *);

// maximum number of lines that we can hold info for at one time
#define MAX_LINE_INFO 128

// maximum fonts that a TextView can hold
#define MAX_FONTS 32

typedef enum { SELMODE_NONE, SELMODE_NORMAL, SELMODE_MARGIN } SELMODE;

//
//	TextView - internal window implementation
//
typedef struct {
    HWND m_hWnd;
    ULONG m_uStyleFlags;

    // Font-related data
    USPFONT m_uspFontList[MAX_FONTS];
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
    DWORD m_nCaretWidth;
    ULONG m_nCurrentLine;
    int m_nLongLineLimit;
    int m_nCRLFMode;

    LINEINFO m_LineInfo[MAX_LINE_INFO];
    int m_nLineInfoCount;

    int m_nLinenoWidth;
    HCURSOR m_hMarginCursor;
    RECT m_rcBorder;

    COLORREF m_rgbColourList[TXC_MAX_COLOURS];

    // Runtime data
    SELMODE m_nSelectionMode;
    ULONG m_nSelMarginOffset1;
    ULONG m_nSelMarginOffset2;
    UINT m_nScrollTimer;
    int m_nScrollCounter;
    BOOL m_fHideCaret;
    BOOL m_fTransparent;
    HIMAGELIST m_hImageList;

    // File-related data
    ULONG m_nLineCount;

    // Cache for USPDATA objects
    USP_CACHE *m_uspCache;

    TEXTDOCUMENT *m_pTextDoc;
} TEXTVIEW;

TEXTVIEW *TextView__new(HWND hwnd);
void TextView__delete(TEXTVIEW *ptv);

LONG WINAPI TextView__WndProc(TEXTVIEW *ptv, UINT msg, WPARAM wParam, LPARAM lParam);

// private
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

LONG TextView__OnMouseActivate(TEXTVIEW *ptv, HWND hwndTop, UINT nHitTest, UINT nMessage);
LONG TextView__OnLButtonDown(TEXTVIEW *ptv, UINT nFlags, int x, int y);
LONG TextView__OnLButtonUp(TEXTVIEW *ptv, UINT nFlags, int x, int y);
LONG TextView__OnMouseMove(TEXTVIEW *ptv, UINT nFlags, int x, int y);

LONG TextView__OnSetFocus(TEXTVIEW *ptv, HWND hwndOld);
LONG TextView__OnKillFocus(TEXTVIEW *ptv, HWND hwndNew);

LONG TextView__OnKeyDown(TEXTVIEW *ptv, UINT nVirtKey, UINT nFlags);

//
//
//
LONG TextView__OpenFile(TEXTVIEW *ptv, TCHAR *szFileName);
LONG TextView__ClearFile(TEXTVIEW *ptv);

LONG TextView__AddFont(TEXTVIEW *ptv, HFONT);
LONG TextView__SetFont(TEXTVIEW *ptv, HFONT, int idx);
LONG TextView__SetLineSpacing(TEXTVIEW *ptv, int nAbove, int nBelow);
LONG TextView__SetLongLine(TEXTVIEW *ptv, int nLength);
COLORREF TextView__SetColour(TEXTVIEW *ptv, UINT idx, COLORREF rgbColour);

//
//	Private Helper functions
//
void TextView__PaintLine(TEXTVIEW *ptv, HDC hdc, ULONG line, int x, int y, HRGN hrgnUpdate);
void TextView__PaintText(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo, int x, int y, RECT *bounds);
int TextView__PaintMargin(TEXTVIEW *ptv, HDC hdc, ULONG line, int x, int y);

int TextView__ApplyTextAttributes(TEXTVIEW *ptv, ULONG nLineNo, ULONG offset, ULONG *nColumn,
                        TCHAR *szText, int nTextLen, ATTR *attr);
void TextView__RefreshWindow(TEXTVIEW *ptv);
LONG TextView__InvalidateRange(TEXTVIEW *ptv, ULONG nStart, ULONG nFinish);
LONG TextView__InvalidateLine(TEXTVIEW *ptv, ULONG nLineNo, BOOL forceAnalysis);
VOID TextView__UpdateLine(TEXTVIEW *ptv, ULONG nLineNo);
int TextView__SyntaxColour(TEXTVIEW *ptv, TCHAR *szText, ULONG nTextLen, ATTR *attr);

int TextView__NeatTextYOffset(TEXTVIEW *ptv, USPFONT *font);
int TextView__TextWidth(TEXTVIEW *ptv, HDC hdc, TCHAR *buf, int len);
// int TextView__TabWidth(TEXTVIEW *ptv);
int TextView__LeftMarginWidth(TEXTVIEW *ptv);
void TextView__UpdateMarginWidth(TEXTVIEW *ptv);
int TextView__SetCaretWidth(TEXTVIEW *ptv, int nWidth);
BOOL TextView__SetImageList(TEXTVIEW *ptv, HIMAGELIST hImgList);

BOOL TextView__MouseCoordToFilePos(TEXTVIEW *ptv, int x, int y, ULONG *pnLineNo, ULONG *pnFileOffset,
                         int *px); //, ULONG *pnLineLen=0);
ULONG TextView__RepositionCaret(TEXTVIEW *ptv);
VOID TextView__MoveCaret(TEXTVIEW *ptv, int x, int y);

ULONG TextView__SetStyle(TEXTVIEW *ptv, ULONG uMask, ULONG uStyles);

ULONG TextView__SetVar(TEXTVIEW *ptv, ULONG nVar, ULONG nValue);
ULONG TextView__GetVar(TEXTVIEW *ptv, ULONG nVar);

ULONG TextView__GetStyleMask(TEXTVIEW *ptv, ULONG uMask);
BOOL TextView__CheckStyle(TEXTVIEW *ptv, ULONG uMask);

COLORREF TextView__GetColour(TEXTVIEW *ptv, UINT idx);
COLORREF TextView__LineColour(TEXTVIEW *ptv, ULONG nLineNo);
COLORREF TextView__LongColour(TEXTVIEW *ptv, ULONG nLineNo);

int TextView__SetLineImage(TEXTVIEW *ptv, ULONG nLineNo, ULONG nImageIdx);
LINEINFO *TextView__GetLineInfo(TEXTVIEW *ptv, ULONG nLineNo);
int TextView__StripCRLF(TEXTVIEW *ptv, TCHAR *szText, ATTR *attrList, int nLength, BOOL fAllow);

VOID TextView__SetupScrollbars(TEXTVIEW *ptv);
VOID TextView__UpdateMetrics(TEXTVIEW *ptv);
VOID TextView__RecalcLineHeight(TEXTVIEW *ptv);
BOOL TextView__PinToBottomCorner(TEXTVIEW *ptv);

void TextView__Scroll(TEXTVIEW *ptv, int dx, int dy);
HRGN TextView__ScrollRgn(TEXTVIEW *ptv, int dx, int dy, BOOL fReturnUpdateRgn);

void TextView__ResetLineCache(TEXTVIEW *ptv);

USPDATA *TextView__GetUspData(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo);
USPDATA *TextView__GetUspData_offset(TEXTVIEW *ptv, HDC hdc, ULONG nLineNo, ULONG *off_chars);

#endif
