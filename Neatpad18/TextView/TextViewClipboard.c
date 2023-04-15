//
//  MODULE:     TextViewClipboard.cpp
//
//  PURPOSE:    Basic clipboard support for TextView
//              Just uses GetClipboardData/SetClipboardData until I migrate
//              to the OLE code from my drag+drop tutorials
//
//  NOTES:      www.catch22.net
//

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

#ifdef UNICODE
#define CF_TCHARTEXT CF_UNICODETEXT
#else
#define CF_TCHARTEXT CF_TEXT
#endif

//
//  Paste any CF_TEXT/CF_UNICODE text from the clipboard
//
BOOL TextView__OnPaste(TEXTVIEW *ptv)
{
    BOOL success = FALSE;

    if (ptv->m_nEditMode == MODE_READONLY)
        return FALSE;

    if (OpenClipboard(ptv->m_hWnd)) {
        HANDLE hMem = GetClipboardData(CF_TCHARTEXT);
        TCHAR *szText = (TCHAR *)GlobalLock(hMem);

        if (szText) {
            ULONG textlen = lstrlen(szText);
            TextView__EnterText(ptv, szText, textlen);

            if (textlen > 1)
                sequence__breakopt(ptv->m_pTextDoc->m_seq);

            GlobalUnlock(hMem);

            success = TRUE;
        }

        CloseClipboard();
    }

    return success;
}

//
//  Retrieve the specified range of text and copy it to supplied buffer
//  szDest must be big enough to hold nLength characters
//  nLength includes the terminating NULL
//
ULONG TextView__GetText(TEXTVIEW *ptv, TCHAR *szDest, ULONG nStartOffset,
                        ULONG nLength)
{
    ULONG copied = 0;

    if (nLength > 1) {
        TEXTITERATOR *itor =
            TextDocument__iterate(ptv->m_pTextDoc, nStartOffset);
        copied = TextIterator__gettext(itor, szDest, nLength - 1);

        // null-terminate
        szDest[copied] = 0;
        TextIterator__delete(itor);
    }

    return copied;
}

//
//  Copy the currently selected text to the clipboard as CF_TEXT/CF_UNICODE
//
BOOL TextView__OnCopy(TEXTVIEW *ptv)
{
    ULONG selstart = min(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    ULONG sellen = TextView__SelectionSize(ptv);
    BOOL success = FALSE;

    if (sellen == 0)
        return FALSE;

    if (OpenClipboard(ptv->m_hWnd)) {
        HANDLE hMem;
        TCHAR *ptr;

        if ((hMem = GlobalAlloc(GPTR, (sellen + 1) * sizeof(TCHAR))) != 0) {
            if ((ptr = (TCHAR *)GlobalLock(hMem)) != 0) {
                EmptyClipboard();

                TextView__GetText(ptv, ptr, selstart, sellen + 1);

                SetClipboardData(CF_TCHARTEXT, hMem);
                success = TRUE;

                GlobalUnlock(hMem);
            }
        }

        CloseClipboard();
    }

    return success;
}

//
//  Remove current selection and copy to the clipboard
//
BOOL TextView__OnCut(TEXTVIEW *ptv)
{
    BOOL success = FALSE;

    if (ptv->m_nEditMode == MODE_READONLY)
        return FALSE;

    if (TextView__SelectionSize(ptv) > 0) {
        // copy selected text to clipboard then erase current selection
        success = TextView__OnCopy(ptv);
        success = success && TextView__ForwardDelete(ptv);
    }

    return success;
}

//
//  Remove the current selection
//
BOOL TextView__OnClear(TEXTVIEW *ptv)
{
    BOOL success = FALSE;

    if (ptv->m_nEditMode == MODE_READONLY)
        return FALSE;

    if (TextView__SelectionSize(ptv) > 0) {
        TextView__ForwardDelete(ptv);
        success = TRUE;
    }

    return success;
}
