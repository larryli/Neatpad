#ifndef TEXTVIEW_INTERNAL_INCLUDED
#define TEXTVIEW_INTERNAL_INCLUDED

typedef struct TextView {
    HWND m_hWnd;
    int x;
} TEXTVIEW;

TEXTVIEW *TextView__new(HWND hwnd);
void TextView__delete(TEXTVIEW *ptv);
LRESULT WINAPI TextView__OnPaint(TEXTVIEW *ptv);

#endif
