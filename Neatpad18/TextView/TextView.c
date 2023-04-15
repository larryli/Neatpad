//
//  MODULE:     TextView.cpp
//
//  PURPOSE:    Implementation of the TextView control
//
//  NOTES:      www.catch22.net
//
#define _WIN32_WINNT 0x501
#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <tchar.h>
#include <windows.h>

// for the EM_xxx message constants
#include <richedit.h>

#include "TextView.h"
#include "TextViewInternal.h"
#include "racursor.h"

#if !defined(UNICODE)
#error "Please build as Unicode only!"
#endif

// #if !defined(GetWindowLongPtr)
// #error "Latest Platform SDK is required to build Neatpad - try PSDK-Feb-2003
// #endif

#pragma comment(lib, "comctl32.lib")

//
//  Constructor for TextView class
//
TEXTVIEW *TextView__new(HWND hwnd)
{
    TEXTVIEW *ptv = malloc(sizeof(TEXTVIEW));
    if (ptv == NULL)
        return NULL;
    ptv->m_hWnd = hwnd;

    ptv->m_hTheme = OpenThemeData(hwnd, L"edit");

    // Font-related data
    ptv->m_nNumFonts = 1;
    ptv->m_nHeightAbove = 0;
    ptv->m_nHeightBelow = 0;

    // File-related data
    ptv->m_nLineCount = 0;
    ptv->m_nLongestLine = 0;

    // Scrollbar related data
    ptv->m_nVScrollPos = 0;
    ptv->m_nHScrollPos = 0;
    ptv->m_nVScrollMax = 0;
    ptv->m_nHScrollMax = 0;

    // Display-related data
    ptv->m_nTabWidthChars = 4;
    ptv->m_uStyleFlags = 0;
    ptv->m_nCaretWidth = 0;
    ptv->m_nLongLineLimit = 80;
    ptv->m_nLineInfoCount = 0;
    ptv->m_nCRLFMode = TXL_CRLF; // ALL;

    // allocate the USPDATA cache
    ptv->m_uspCache = malloc(sizeof(USPCACHE) * USP_CACHE_SIZE);
    if (ptv->m_uspCache == NULL) {
        TextView__delete(ptv);
        return NULL;
    }

    for (int i = 0; i < USP_CACHE_SIZE; i++) {
        ptv->m_uspCache[i].usage = 0;
        ptv->m_uspCache[i].lineno = 0;
        ptv->m_uspCache[i].uspData = UspAllocate();
    }

    SystemParametersInfo(SPI_GETCARETWIDTH, 0, &ptv->m_nCaretWidth, 0);

    if (ptv->m_nCaretWidth == 0)
        ptv->m_nCaretWidth = 2;

    // Default display colours
    ptv->m_rgbColourList[TXC_FOREGROUND] = SYSCOL(COLOR_WINDOWTEXT);
    ptv->m_rgbColourList[TXC_BACKGROUND] =
        SYSCOL(COLOR_WINDOW); // RGB(34,54,106)
    ptv->m_rgbColourList[TXC_HIGHLIGHTTEXT] = SYSCOL(COLOR_HIGHLIGHTTEXT);
    ptv->m_rgbColourList[TXC_HIGHLIGHT] = SYSCOL(COLOR_HIGHLIGHT);
    ptv->m_rgbColourList[TXC_HIGHLIGHTTEXT2] =
        SYSCOL(COLOR_WINDOWTEXT); // INACTIVECAPTIONTEXT);
    ptv->m_rgbColourList[TXC_HIGHLIGHT2] =
        SYSCOL(COLOR_3DFACE); // INACTIVECAPTION);
    ptv->m_rgbColourList[TXC_SELMARGIN1] = SYSCOL(COLOR_3DFACE);
    ptv->m_rgbColourList[TXC_SELMARGIN2] = SYSCOL(COLOR_3DHIGHLIGHT);
    ptv->m_rgbColourList[TXC_LINENUMBERTEXT] = SYSCOL(COLOR_3DSHADOW);
    ptv->m_rgbColourList[TXC_LINENUMBER] = SYSCOL(COLOR_3DFACE);
    ptv->m_rgbColourList[TXC_LONGLINETEXT] = SYSCOL(COLOR_3DSHADOW);
    ptv->m_rgbColourList[TXC_LONGLINE] = SYSCOL(COLOR_3DFACE);
    ptv->m_rgbColourList[TXC_CURRENTLINETEXT] = SYSCOL(COLOR_WINDOWTEXT);
    ptv->m_rgbColourList[TXC_CURRENTLINE] = RGB(230, 240, 255);

    // Runtime data
    ptv->m_nSelectionMode = SEL_NONE;
    ptv->m_nEditMode = MODE_INSERT;
    ptv->m_nScrollTimer = 0;
    ptv->m_fHideCaret = false;
    ptv->m_hUserMenu = 0;
    ptv->m_hImageList = 0;

    ptv->m_nSelectionStart = 0;
    ptv->m_nSelectionEnd = 0;
    ptv->m_nSelectionType = SEL_NONE;
    ptv->m_nCursorOffset = 0;
    ptv->m_nCurrentLine = 0;

    ptv->m_nLinenoWidth = 0;
    ptv->m_nCaretPosX = 0;
    ptv->m_nAnchorPosX = 0;

    // SetRect(&ptv->m_rcBorder, 2, 2, 2, 2);

    ptv->m_pTextDoc = TextDocument__new();
    if (ptv->m_pTextDoc == NULL) {
        free(ptv->m_uspCache);
        TextView__delete(ptv);
        return NULL;
    }

    ptv->m_hMarginCursor =
        CreateCursor(GetModuleHandle(0), 21, 5, 32, 32, XORMask, ANDMask);

    //
    //  The TextView state must be fully initialized before we
    //  start calling member-functions
    //

    memset(ptv->m_uspFontList, 0, sizeof(ptv->m_uspFontList));

    // Set the default font
    TextView__OnSetFont(ptv, (HFONT)GetStockObject(ANSI_FIXED_FONT));

    TextView__UpdateMetrics(ptv);
    TextView__UpdateMarginWidth(ptv);
    return ptv;
}

//
//  Destructor for TextView class
//
void TextView__delete(TEXTVIEW *ptv)
{
    if (ptv->m_pTextDoc) {
        TextDocument__delete(ptv->m_pTextDoc);
        free(ptv->m_pTextDoc);
    }

    DestroyCursor(ptv->m_hMarginCursor);

    for (int i = 0; i < USP_CACHE_SIZE; i++)
        UspFree(ptv->m_uspCache[i].uspData);

    CloseThemeData(ptv->m_hTheme);
    free(ptv->m_uspCache);
}

ULONG TextView__NotifyParent(TEXTVIEW *ptv, UINT nNotifyCode, NMHDR *optional)
{
    UINT nCtrlId = GetWindowLong(ptv->m_hWnd, GWL_ID);
    NMHDR nmhdr = {ptv->m_hWnd, nCtrlId, nNotifyCode};
    NMHDR *nmptr = &nmhdr;

    if (optional) {
        nmptr = optional;
        *nmptr = nmhdr;
    }

    return SendMessage(GetParent(ptv->m_hWnd), WM_NOTIFY, (WPARAM)nCtrlId,
                       (LPARAM)nmptr);
}

VOID TextView__UpdateMetrics(TEXTVIEW *ptv)
{
    RECT rect;
    GetClientRect(ptv->m_hWnd, &rect);

    TextView__OnSize(ptv, 0, rect.right, rect.bottom);
    TextView__RefreshWindow(ptv);

    TextView__RepositionCaret(ptv);
}

LONG TextView__OnSetFocus(TEXTVIEW *ptv, HWND hwndOld)
{
    CreateCaret(ptv->m_hWnd, (HBITMAP)NULL, ptv->m_nCaretWidth,
                ptv->m_nLineHeight);
    TextView__RepositionCaret(ptv);

    ShowCaret(ptv->m_hWnd);
    TextView__RefreshWindow(ptv);
    return 0;
}

LONG TextView__OnKillFocus(TEXTVIEW *ptv, HWND hwndNew)
{
    // if we are making a selection when we lost focus then
    // stop the selection logic
    if (ptv->m_nSelectionMode != SEL_NONE) {
        TextView__OnLButtonUp(ptv, 0, 0, 0);
    }

    HideCaret(ptv->m_hWnd);
    DestroyCaret();
    TextView__RefreshWindow(ptv);
    return 0;
}

ULONG TextView__SetStyle(TEXTVIEW *ptv, ULONG uMask, ULONG uStyles)
{
    ULONG oldstyle = ptv->m_uStyleFlags;

    ptv->m_uStyleFlags = (ptv->m_uStyleFlags & ~uMask) | uStyles;

    TextView__ResetLineCache(ptv);

    // update display here
    TextView__UpdateMetrics(ptv);
    TextView__RefreshWindow(ptv);

    return oldstyle;
}

ULONG TextView__SetVar(TEXTVIEW *ptv, ULONG nVar, ULONG nValue)
{
    return 0; // oldval;
}

ULONG TextView__GetVar(TEXTVIEW *ptv, ULONG nVar) { return 0; }

ULONG TextView__GetStyleMask(TEXTVIEW *ptv, ULONG uMask)
{
    return ptv->m_uStyleFlags & uMask;
}

bool TextView__CheckStyle(TEXTVIEW *ptv, ULONG uMask)
{
    return (ptv->m_uStyleFlags & uMask) ? true : false;
}

int TextView__SetCaretWidth(TEXTVIEW *ptv, int nWidth)
{
    int oldwidth = ptv->m_nCaretWidth;
    ptv->m_nCaretWidth = nWidth;

    return oldwidth;
}

BOOL TextView__SetImageList(TEXTVIEW *ptv, HIMAGELIST hImgList)
{
    ptv->m_hImageList = hImgList;
    return TRUE;
}

LONG TextView__SetLongLine(TEXTVIEW *ptv, int nLength)
{
    int oldlen = ptv->m_nLongLineLimit;
    ptv->m_nLongLineLimit = nLength;
    return oldlen;
}

int CompareLineInfo(LINEINFO *elem1, LINEINFO *elem2)
{
    if (elem1->nLineNo < elem2->nLineNo)
        return -1;
    if (elem1->nLineNo > elem2->nLineNo)
        return 1;
    else
        return 0;
}

int TextView__SetLineImage(TEXTVIEW *ptv, ULONG nLineNo, ULONG nImageIdx)
{
    LINEINFO *linfo = TextView__GetLineInfo(ptv, nLineNo);

    // if already a line with an image
    if (linfo) {
        linfo->nImageIdx = nImageIdx;
    } else {
        linfo = &(ptv->m_LineInfo[ptv->m_nLineInfoCount++]);
        linfo->nLineNo = nLineNo;
        linfo->nImageIdx = nImageIdx;

        // sort the array
        qsort(ptv->m_LineInfo, ptv->m_nLineInfoCount, sizeof(LINEINFO),
              (COMPAREPROC)CompareLineInfo);
    }
    return 0;
}

LINEINFO *TextView__GetLineInfo(TEXTVIEW *ptv, ULONG nLineNo)
{
    LINEINFO key = {nLineNo, 0};

    // perform the binary search
    return (LINEINFO *)bsearch(&key, ptv->m_LineInfo, ptv->m_nLineInfoCount,
                               sizeof(LINEINFO), (COMPAREPROC)CompareLineInfo);
}

ULONG TextView__SelectionSize(TEXTVIEW *ptv)
{
    ULONG s1 = min(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    ULONG s2 = max(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    return s2 - s1;
}

ULONG TextView__SelectAll(TEXTVIEW *ptv)
{
    ptv->m_nSelectionStart = 0;
    ptv->m_nSelectionEnd = TextDocument__size(ptv->m_pTextDoc);
    ptv->m_nCursorOffset = ptv->m_nSelectionEnd;

    TextView__Smeg(ptv, TRUE);
    TextView__RefreshWindow(ptv);
    return 0;
}

//
//  Public memberfunction
//
LONG WINAPI TextView__WndProc(TEXTVIEW *ptv, UINT msg, WPARAM wParam,
                              LPARAM lParam)
{
    switch (msg) {
    // Draw contents of TextView whenever window needs updating
    case WM_ERASEBKGND:
        return 1;

    // Need to custom-draw the border for XP/Vista themes
    case WM_NCPAINT:
        return TextView__OnNcPaint(ptv, (HRGN)wParam);

    case WM_PAINT:
        return TextView__OnPaint(ptv);

    // Set a new font
    case WM_SETFONT:
        return TextView__OnSetFont(ptv, (HFONT)wParam);

    case WM_SIZE:
        return TextView__OnSize(ptv, wParam, LOWORD(lParam), HIWORD(lParam));

    case WM_VSCROLL:
        return TextView__OnVScroll(ptv, LOWORD(wParam), HIWORD(wParam));

    case WM_HSCROLL:
        return TextView__OnHScroll(ptv, LOWORD(wParam), HIWORD(wParam));

    case WM_MOUSEACTIVATE:
        return TextView__OnMouseActivate(ptv, (HWND)wParam, LOWORD(lParam),
                                         HIWORD(lParam));

    case WM_CONTEXTMENU:
        return TextView__OnContextMenu(ptv, (HWND)wParam, (short)LOWORD(lParam),
                                       (short)HIWORD(lParam));

    case WM_MOUSEWHEEL:
        return TextView__OnMouseWheel(ptv, (short)HIWORD(wParam));

    case WM_SETFOCUS:
        return TextView__OnSetFocus(ptv, (HWND)wParam);

    case WM_KILLFOCUS:
        return TextView__OnKillFocus(ptv, (HWND)wParam);

    // make sure we get arrow-keys, enter, tab, etc when hosted inside a dialog
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_LBUTTONDOWN:
        return TextView__OnLButtonDown(ptv, wParam, (short)LOWORD(lParam),
                                       (short)HIWORD(lParam));

    case WM_LBUTTONUP:
        return TextView__OnLButtonUp(ptv, wParam, (short)LOWORD(lParam),
                                     (short)HIWORD(lParam));

    case WM_LBUTTONDBLCLK:
        return TextView__OnLButtonDblClick(ptv, wParam, (short)LOWORD(lParam),
                                           (short)HIWORD(lParam));

    case WM_MOUSEMOVE:
        return TextView__OnMouseMove(ptv, wParam, (short)LOWORD(lParam),
                                     (short)HIWORD(lParam));

    case WM_KEYDOWN:
        return TextView__OnKeyDown(ptv, wParam, lParam);

    case WM_UNDO:
    case TXM_UNDO:
    case EM_UNDO:
        return TextView__Undo(ptv);

    case TXM_REDO:
    case EM_REDO:
        return TextView__Redo(ptv);

    case TXM_CANUNDO:
    case EM_CANUNDO:
        return TextView__CanUndo(ptv);

    case TXM_CANREDO:
    case EM_CANREDO:
        return TextView__CanRedo(ptv);

    case WM_CHAR:
        return TextView__OnChar(ptv, wParam, lParam);

    case WM_SETCURSOR:

        if (LOWORD(lParam) == HTCLIENT)
            return TRUE;

        break;

    case WM_COPY:
        return TextView__OnCopy(ptv);

    case WM_CUT:
        return TextView__OnCut(ptv);

    case WM_PASTE:
        return TextView__OnPaste(ptv);

    case WM_CLEAR:
        return TextView__OnClear(ptv);

    case WM_GETTEXT:
        return 0;

    case WM_TIMER:
        return TextView__OnTimer(ptv, wParam);

    //
    case TXM_OPENFILE:
        return TextView__OpenFile(ptv, (TCHAR *)lParam);

    case TXM_CLEAR:
        return TextView__ClearFile(ptv);

    case TXM_SETLINESPACING:
        return TextView__SetLineSpacing(ptv, wParam, lParam);

    case TXM_ADDFONT:
        return TextView__AddFont(ptv, (HFONT)wParam);

    case TXM_SETCOLOR:
        return TextView__SetColour(ptv, wParam, lParam);

    case TXM_SETSTYLE:
        return TextView__SetStyle(ptv, wParam, lParam);

    case TXM_SETCARETWIDTH:
        return TextView__SetCaretWidth(ptv, wParam);

    case TXM_SETIMAGELIST:
        return TextView__SetImageList(ptv, (HIMAGELIST)wParam);

    case TXM_SETLONGLINE:
        return TextView__SetLongLine(ptv, lParam);

    case TXM_SETLINEIMAGE:
        return TextView__SetLineImage(ptv, wParam, lParam);

    case TXM_GETFORMAT:
        return TextDocument__getformat(ptv->m_pTextDoc);

    case TXM_GETSELSIZE:
        return TextView__SelectionSize(ptv);

    case TXM_SETSELALL:
        return TextView__SelectAll(ptv);

    case TXM_GETCURPOS:
        return ptv->m_nCursorOffset;

    case TXM_GETCURLINE:
        return ptv->m_nCurrentLine;

    case TXM_GETCURCOL: {
        ULONG nOffset;
        TextView__GetUspData(ptv, 0, ptv->m_nCurrentLine, &nOffset);
        return ptv->m_nCursorOffset - nOffset;
    }

    case TXM_GETEDITMODE:
        return ptv->m_nEditMode;

    case TXM_SETEDITMODE:
        lParam = ptv->m_nEditMode;
        ptv->m_nEditMode = wParam;
        return lParam;

    case TXM_SETCONTEXTMENU:
        ptv->m_hUserMenu = (HMENU)wParam;
        return 0;

    default:
        break;
    }

    return DefWindowProc(ptv->m_hWnd, msg, wParam, lParam);
}

//
//  Win32 TextView window procedure stub
//
LRESULT WINAPI TextViewWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                               LPARAM lParam)
{
    TEXTVIEW *ptv = (TEXTVIEW *)GetWindowLongPtr(hwnd, 0);

    switch (msg) {
    // First message received by any window - make a new TextView object
    // and store pointer to it in our extra-window-bytes
    case WM_NCCREATE:

        if ((ptv = TextView__new(hwnd)) == 0)
            return FALSE;

        SetWindowLongPtr(hwnd, 0, (LONG)ptv);
        return TRUE;

    // Last message received by any window - delete the TextView object
    case WM_NCDESTROY:
        TextView__delete(ptv);
        SetWindowLongPtr(hwnd, 0, 0);
        return 0;

    // Pass everything to the TextView window procedure
    default:
        if (ptv)
            return TextView__WndProc(ptv, msg, wParam, lParam);
        else
            return 0;
    }
}

//
//  Register the TextView window class
//
BOOL InitTextView(void)
{
    WNDCLASSEX wcx;

    // Window class for the main application parent window
    wcx.cbSize = sizeof(wcx);
    wcx.style = CS_DBLCLKS;
    wcx.lpfnWndProc = TextViewWndProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = sizeof(TEXTVIEW *);
    wcx.hInstance = GetModuleHandle(0);
    wcx.hIcon = 0;
    wcx.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wcx.hbrBackground = (HBRUSH)0; // NO FLICKERING FOR US!!
    wcx.lpszMenuName = 0;
    wcx.lpszClassName = TEXTVIEW_CLASS;
    wcx.hIconSm = 0;

    return RegisterClassEx(&wcx) ? TRUE : FALSE;
}

//
//  Create a TextView control!
//
HWND CreateTextView(HWND hwndParent)
{
    return CreateWindowEx(WS_EX_CLIENTEDGE,
                          //      L"EDIT", L"",
                          TEXTVIEW_CLASS, _T(""),
                          WS_VSCROLL | WS_HSCROLL | WS_CHILD | WS_VISIBLE, 0, 0,
                          0, 0, hwndParent, 0, GetModuleHandle(0), 0);
}
