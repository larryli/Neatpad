#ifndef TEXTVIEW_INTERNAL_INCLUDED
#define TEXTVIEW_INTERNAL_INCLUDED

typedef struct TextView {
    HWND m_hWnd;
    int x;
} TEXTVIEW;

LRESULT WINAPI TextView_OnPaint(TEXTVIEW *);

#endif
