//
//	MODULE:		TextViewClipboard.cpp
//
//	PURPOSE:	Clipboard support for TextView
//
//	NOTES:		www.catch22.net
//
#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

LONG TextView__OnPaste(TEXTVIEW *ptv) { return 0; }

LONG TextView__OnCopy(TEXTVIEW *ptv)
{
    ULONG selstart = min(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    ULONG sellen = TextView__SelectionSize(ptv);

    if (sellen == 0)
        return 0;

    if (OpenClipboard(ptv->m_hWnd)) {
        HANDLE hMem = GlobalAlloc(GPTR, sellen * sizeof(TCHAR));

        EmptyClipboard();

        TEXTITERATOR *itor = TextDocument__iterate(ptv->m_pTextDoc, selstart);
        if (itor == NULL)
            return 0;
        TextIterator__gettext(itor, (TCHAR *)hMem, sellen);
        TextIterator__delete(itor);

        SetClipboardData(CF_UNICODETEXT, hMem);
        CloseClipboard();
    }

    return 0;
}

LONG TextView__OnCut(TEXTVIEW *ptv) { return 0; }
