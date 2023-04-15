//
//  MODULE:     TextViewFile.cpp
//
//  PURPOSE:    TextView file input routines
//
//  NOTES:      www.catch22.net
//

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

//
//
//
LONG TextView__OpenFile(TEXTVIEW *ptv, TCHAR *szFileName)
{
    TextView__ClearFile(ptv);

    if (TextDocument__init_filename(ptv->m_pTextDoc, szFileName)) {
        ptv->m_nLineCount = TextDocument__linecount(ptv->m_pTextDoc);
        ptv->m_nLongestLine =
            TextDocument__longestline(ptv->m_pTextDoc, ptv->m_nTabWidthChars);

        ptv->m_nVScrollPos = 0;
        ptv->m_nHScrollPos = 0;

        ptv->m_nSelectionStart = 0;
        ptv->m_nSelectionEnd = 0;
        ptv->m_nCursorOffset = 0;

        TextView__UpdateMarginWidth(ptv);
        TextView__UpdateMetrics(ptv);
        TextView__ResetLineCache(ptv);
        return TRUE;
    }

    return FALSE;
}

//
//
//
LONG TextView__ClearFile(TEXTVIEW *ptv)
{
    if (ptv->m_pTextDoc) {
        TextDocument__clear(ptv->m_pTextDoc);
        TextDocument__EmptyDoc(ptv->m_pTextDoc);
    }

    TextView__ResetLineCache(ptv);

    ptv->m_nLineCount = TextDocument__linecount(ptv->m_pTextDoc);
    ptv->m_nLongestLine =
        TextDocument__longestline(ptv->m_pTextDoc, ptv->m_nTabWidthChars);

    ptv->m_nVScrollPos = 0;
    ptv->m_nHScrollPos = 0;

    ptv->m_nSelectionStart = 0;
    ptv->m_nSelectionEnd = 0;
    ptv->m_nCursorOffset = 0;

    ptv->m_nCurrentLine = 0;
    ptv->m_nCaretPosX = 0;

    TextView__UpdateMetrics(ptv);

    return TRUE;
}
