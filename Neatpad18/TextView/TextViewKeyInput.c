//
//  MODULE:     TextViewKeyInput.cpp
//
//  PURPOSE:    Keyboard input for TextView
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
//  TextView__EnterText
//
//  Import the specified text into the TextView at the current
//  cursor position, replacing any text-selection in the process
//
ULONG TextView__EnterText(TEXTVIEW *ptv, TCHAR *szText, ULONG nLength)
{
    ULONG selstart = min(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    ULONG selend = max(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);

    BOOL fReplaceSelection = (selstart == selend) ? FALSE : TRUE;
    ULONG erase_len = nLength;

    switch (ptv->m_nEditMode) {
    case MODE_READONLY:
        return 0;

    case MODE_INSERT:

        // if there is a selection then remove it
        if (fReplaceSelection) {
            // group this erase with the insert/replace operation
            sequence__group(ptv->m_pTextDoc->m_seq);
            TextDocument__erase_text(ptv->m_pTextDoc, selstart,
                                     selend - selstart);
            ptv->m_nCursorOffset = selstart;
        }

        if (!TextDocument__insert_text(ptv->m_pTextDoc, ptv->m_nCursorOffset,
                                       szText, nLength))
            return 0;

        if (fReplaceSelection)
            sequence__ungroup(ptv->m_pTextDoc->m_seq);

        break;

    case MODE_OVERWRITE:

        if (fReplaceSelection) {
            erase_len = selend - selstart;
            ptv->m_nCursorOffset = selstart;
        } else {
            ULONG lineoff;
            USPCACHE *uspCache =
                TextView__GetUspCache(ptv, 0, ptv->m_nCurrentLine, &lineoff);

            // single-character overwrite - must behave like 'forward delete'
            // and remove a whole character-cluster (i.e. maybe more than 1
            // char)
            if (nLength == 1) {
                ULONG oldpos = ptv->m_nCursorOffset;
                TextView__MoveCharNext(ptv);
                erase_len = ptv->m_nCursorOffset - oldpos;
                ptv->m_nCursorOffset = oldpos;
            }
            // if we are at the end of a line (just before the CRLF) then we
            // must not erase any text - instead we act like a regular insertion
            if (ptv->m_nCursorOffset == lineoff + uspCache->length_CRLF)
                erase_len = 0;
        }

        if (!TextDocument__replace_text(ptv->m_pTextDoc, ptv->m_nCursorOffset,
                                        szText, nLength, erase_len))
            return 0;

        break;

    default:
        return 0;
    }

    // update cursor+selection positions
    ptv->m_nCursorOffset += nLength;
    ptv->m_nSelectionStart = ptv->m_nCursorOffset;
    ptv->m_nSelectionEnd = ptv->m_nCursorOffset;

    // we altered the document, recalculate line+scrollbar information
    TextView__ResetLineCache(ptv);
    TextView__RefreshWindow(ptv);

    TextView__Smeg(ptv, TRUE);
    TextView__NotifyParent(ptv, TVN_CURSOR_CHANGE, NULL);

    return nLength;
}

BOOL TextView__ForwardDelete(TEXTVIEW *ptv)
{
    ULONG selstart = min(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    ULONG selend = max(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);

    if (selstart != selend) {
        TextDocument__erase_text(ptv->m_pTextDoc, selstart, selend - selstart);
        ptv->m_nCursorOffset = selstart;

        sequence__breakopt(ptv->m_pTextDoc->m_seq);
    } else {
        BYTE tmp[2];
        // USPCACHE      * uspCache;
        // CSCRIPT_LOGATTR * logAttr;
        // ULONG           lineOffset;
        // ULONG           index;

        sequence__render(ptv->m_pTextDoc->m_seq, ptv->m_nCursorOffset, tmp, 2);

        /*GetLogAttr(ptv->m_nCurrentLine, &uspCache, &logAttr, &lineOffset);

           index = ptv->m_nCursorOffset - lineOffset;

           do
           {
           sequence__erase(ptv->m_pTextDoc->seq, ptv->m_nCursorOffset, 1);
           index++;
           }
           while(!logAttr[index].fCharStop); */

        ULONG oldpos = ptv->m_nCursorOffset;
        TextView__MoveCharNext(ptv);

        TextDocument__erase_text(ptv->m_pTextDoc, oldpos,
                                 ptv->m_nCursorOffset - oldpos);
        ptv->m_nCursorOffset = oldpos;

        // if(tmp[0] == '\r')
        //   TextDocument__erase_text(ptv->m_pTextDoc, ptv->m_nCursorOffset, 2);
        // else
        //   TextDocument__erase_text(ptv->m_pTextDoc, ptv->m_nCursorOffset, 1);
    }

    ptv->m_nSelectionStart = ptv->m_nCursorOffset;
    ptv->m_nSelectionEnd = ptv->m_nCursorOffset;

    TextView__ResetLineCache(ptv);
    TextView__RefreshWindow(ptv);
    TextView__Smeg(ptv, FALSE);

    return TRUE;
}

BOOL TextView__BackDelete(TEXTVIEW *ptv)
{
    ULONG selstart = min(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);
    ULONG selend = max(ptv->m_nSelectionStart, ptv->m_nSelectionEnd);

    // if there's a selection then delete it
    if (selstart != selend) {
        TextDocument__erase_text(ptv->m_pTextDoc, selstart, selend - selstart);
        ptv->m_nCursorOffset = selstart;
        sequence__breakopt(ptv->m_pTextDoc->m_seq);
    }
    // otherwise do a back-delete
    else if (ptv->m_nCursorOffset > 0) {
        // ptv->m_nCursorOffset--;
        ULONG oldpos = ptv->m_nCursorOffset;
        TextView__MoveCharPrev(ptv);
        // TextDocument__erase_text(ptv->m_pTextDoc, ptv->m_nCursorOffset, 1);
        TextDocument__erase_text(ptv->m_pTextDoc, ptv->m_nCursorOffset,
                                 oldpos - ptv->m_nCursorOffset);
    }

    ptv->m_nSelectionStart = ptv->m_nCursorOffset;
    ptv->m_nSelectionEnd = ptv->m_nCursorOffset;

    TextView__ResetLineCache(ptv);
    TextView__RefreshWindow(ptv);
    TextView__Smeg(ptv, FALSE);

    return TRUE;
}

void TextView__Smeg(TEXTVIEW *ptv, BOOL fAdvancing)
{
    TextDocument__init_linebuffer(ptv->m_pTextDoc);

    ptv->m_nLineCount = TextDocument__linecount(ptv->m_pTextDoc);

    TextView__UpdateMetrics(ptv);
    TextView__UpdateMarginWidth(ptv);
    TextView__SetupScrollbars(ptv);

    TextView__UpdateCaretOffset(ptv, ptv->m_nCursorOffset, fAdvancing,
                                &ptv->m_nCaretPosX, &ptv->m_nCurrentLine);

    ptv->m_nAnchorPosX = ptv->m_nCaretPosX;
    TextView__ScrollToPosition(ptv, ptv->m_nCaretPosX, ptv->m_nCurrentLine);
    TextView__RepositionCaret(ptv);
}

BOOL TextView__Undo(TEXTVIEW *ptv)
{
    if (ptv->m_nEditMode == MODE_READONLY)
        return FALSE;

    if (!TextDocument__Undo(ptv->m_pTextDoc, &ptv->m_nSelectionStart,
                            &ptv->m_nSelectionEnd))
        return FALSE;

    ptv->m_nCursorOffset = ptv->m_nSelectionEnd;

    TextView__ResetLineCache(ptv);
    TextView__RefreshWindow(ptv);

    TextView__Smeg(ptv, ptv->m_nSelectionStart != ptv->m_nSelectionEnd);

    return TRUE;
}

BOOL TextView__Redo(TEXTVIEW *ptv)
{
    if (ptv->m_nEditMode == MODE_READONLY)
        return FALSE;

    if (!TextDocument__Redo(ptv->m_pTextDoc, &ptv->m_nSelectionStart,
                            &ptv->m_nSelectionEnd))
        return FALSE;

    ptv->m_nCursorOffset = ptv->m_nSelectionEnd;

    TextView__ResetLineCache(ptv);
    TextView__RefreshWindow(ptv);
    TextView__Smeg(ptv, ptv->m_nSelectionStart != ptv->m_nSelectionEnd);

    return TRUE;
}

BOOL TextView__CanUndo(TEXTVIEW *ptv)
{
    return sequence__canundo(ptv->m_pTextDoc->m_seq) ? TRUE : FALSE;
}

BOOL TextView__CanRedo(TEXTVIEW *ptv)
{
    return sequence__canredo(ptv->m_pTextDoc->m_seq) ? TRUE : FALSE;
}

LONG TextView__OnChar(TEXTVIEW *ptv, UINT nChar, UINT nFlags)
{
    WCHAR ch = (WCHAR)nChar;

    if (nChar < 32 && nChar != '\t' && nChar != '\r' && nChar != '\n')
        return 0;

    // change CR into a CR/LF sequence
    if (nChar == '\r')
        PostMessage(ptv->m_hWnd, WM_CHAR, '\n', 1);

    if (TextView__EnterText(ptv, &ch, 1)) {
        if (nChar == '\n')
            sequence__breakopt(ptv->m_pTextDoc->m_seq);

        TextView__NotifyParent(ptv, TVN_CHANGED, NULL);
    }

    return 0;
}
