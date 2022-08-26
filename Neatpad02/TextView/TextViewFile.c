//
//	MODULE:		TextViewFile.cpp
//
//	PURPOSE:	TextView file input routines
//
//	NOTES:		www.catch22.net
//

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

    TextDocument__init_filename(ptv->m_pTextDoc, szFileName);
    ptv->m_nLineCount = TextDocument__linecount(ptv->m_pTextDoc);

    InvalidateRect(ptv->m_hWnd, NULL, FALSE);
    return TRUE;
}

//
//
//
LONG TextView__ClearFile(TEXTVIEW *ptv)
{
    if (ptv->m_pTextDoc)
        TextDocument__clear(ptv->m_pTextDoc);

    ptv->m_nLineCount = TextDocument__linecount(ptv->m_pTextDoc);

    InvalidateRect(ptv->m_hWnd, NULL, FALSE);
    return TRUE;
}
