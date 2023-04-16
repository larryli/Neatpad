
#include "UspLib\UspLib.h"
#include "sequence.h"

typedef struct {
    HWND m_hWnd;
    int m_nWrapWidth;
    sequence *m_seq;

    HFONT m_hFont;
    HFONT m_hFont2;
} SpanView;

// public:
SpanView *SpanView__new(HWND hwnd, sequence *s);
void SpanView__delete(SpanView *);

void SpanView__DrawSpan(SpanView *, HDC hdc, int *x, int *y,
                        sequence__span *sptr, SIZE *szOut);
void SpanView__DrawSpan2(SpanView *, HDC hdc, int *x, int *y, WCHAR *buf,
                         int len, int type, COLORREF col, SIZE *szOut);
LONG SpanView__OnPaint(SpanView *);

LRESULT SpanView__SpanViewWndProc(SpanView *, UINT msg, WPARAM wParam,
                                  LPARAM lParam);
static LRESULT WINAPI SpanViewWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam);

typedef struct {
    HWND m_hWnd;

    // selection and cursor positions
    int m_nSelStart;
    int m_nSelEnd;
    int m_nCurPos;
    BOOL m_fMouseDown;
    BOOL m_fInsertMode;
    BOOL m_fTrailing;

    int m_nLineHeight;
    USPFONT m_uspFontList[1];

    USPDATA *m_uspData;
    ATTR m_attrRunList[100];

    sequence *m_seq;
} UniView;

// public:
UniView *UniView__new(HWND hwnd, sequence *s);
void SpanView__delete(SpanView *psv);
LRESULT UniView__WndProc(UniView *, UINT msg, WPARAM wParam, LPARAM lParam);

// private:
USPDATA *UniView__GetUspData(UniView *, HDC hdc);
int UniView__ApplyFormatting(UniView *, WCHAR *wstr, int wlen, ATTR *attrList);
void UniView__Uniscribe_MouseXToOffset(UniView *, HWND hwnd, int mouseX,
                                       int *charpos, int *snappedToX);

void UniView__PaintWnd(UniView *);
void UniView__MouseMove(UniView *, int x, int y);
void UniView__LButtonDown(UniView *, int x, int y);
void UniView__LButtonDblClick(UniView *, int x, int y);
void UniView__LButtonUp(UniView *, int x, int y);
void UniView__KeyDown(UniView *, UINT nKey, UINT nFlags);
void UniView__CharInput(UniView *, UINT uChar, DWORD flags);
void UniView__SetFont(UniView *, HFONT hFont);
void UniView__ReposCaret(UniView *, ULONG offset, BOOL fTrailing);
