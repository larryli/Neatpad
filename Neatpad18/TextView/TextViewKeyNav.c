//
//  MODULE:     TextViewKeyNav.cpp
//
//  PURPOSE:    Keyboard navigation for TextView
//
//  NOTES:      www.catch22.net
//

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <tchar.h>
#include <windows.h>

#include "TextView.h"
#include "TextViewInternal.h"

/*struct SCRIPT_LOGATTR
   {
   BYTE fSoftBreak  :1;
   BYTE fWhiteSpace :1;
   BYTE fCharStop   :1;
   BYTE fWordStop   :1;
   BYTE fInvalid        :1;
   BYTE fReserved   :3;
   }; */

bool IsKeyPressed(UINT nVirtKey)
{
    return GetKeyState(nVirtKey) < 0 ? true : false;
}

//
//  Get the UspCache and logical attributes for specified line
//
bool TextView__GetLogAttr(TEXTVIEW *ptv, ULONG nLineNo, USPCACHE **puspCache,
                          CSCRIPT_LOGATTR **plogAttr, ULONG *pnOffset)
{
    if ((*puspCache = TextView__GetUspCache(ptv, 0, nLineNo, pnOffset)) == 0)
        return false;

    if (plogAttr && (*plogAttr = UspGetLogAttr((*puspCache)->uspData)) == 0)
        return false;

    return true;
}

//
//  Move caret up specified number of lines
//
VOID TextView__MoveLineUp(TEXTVIEW *ptv, int numLines)
{
    USPDATA *uspData;
    ULONG lineOffset;

    int charPos;
    BOOL trailing;

    ptv->m_nCurrentLine -= min(ptv->m_nCurrentLine, (unsigned)numLines);

    // get Uniscribe data for prev line
    uspData = TextView__GetUspData(ptv, 0, ptv->m_nCurrentLine, &lineOffset);

    // move up to character nearest the caret-anchor positions
    UspXToOffset(uspData, ptv->m_nAnchorPosX, &charPos, &trailing, 0);

    ptv->m_nCursorOffset = lineOffset + charPos + trailing;
}

//
//  Move caret down specified number of lines
//
VOID TextView__MoveLineDown(TEXTVIEW *ptv, int numLines)
{
    USPDATA *uspData;
    ULONG lineOffset;

    int charPos;
    BOOL trailing;

    ptv->m_nCurrentLine +=
        min(ptv->m_nLineCount - ptv->m_nCurrentLine - 1, (unsigned)numLines);

    // get Uniscribe data for prev line
    uspData = TextView__GetUspData(ptv, 0, ptv->m_nCurrentLine, &lineOffset);

    // move down to character nearest the caret-anchor position
    UspXToOffset(uspData, ptv->m_nAnchorPosX, &charPos, &trailing, 0);

    ptv->m_nCursorOffset = lineOffset + charPos + trailing;
}

//
//  Move to start of previous word (to the left)
//
VOID TextView__MoveWordPrev(TEXTVIEW *ptv)
{
    USPCACHE *uspCache;
    CSCRIPT_LOGATTR *logAttr;
    ULONG lineOffset;
    int charPos;

    // get Uniscribe data for current line
    if (!TextView__GetLogAttr(ptv, ptv->m_nCurrentLine, &uspCache, &logAttr,
                              &lineOffset))
        return;

    // move 1 character to left
    charPos = ptv->m_nCursorOffset - lineOffset - 1;

    // skip to end of *previous* line if necessary
    if (charPos < 0) {
        charPos = 0;

        if (ptv->m_nCurrentLine > 0) {
            TextView__MoveLineEnd(ptv, ptv->m_nCurrentLine - 1);
            return;
        }
    }
    // skip preceding whitespace
    while (charPos > 0 && logAttr[charPos].fWhiteSpace)
        charPos--;

    // skip whole characters until we hit a word-break/more whitespace
    for (; charPos > 0; charPos--) {
        if (logAttr[charPos].fWordStop || logAttr[charPos - 1].fWhiteSpace)
            break;
    }

    ptv->m_nCursorOffset = lineOffset + charPos;
}

//
//  Move to start of next word
//
VOID TextView__MoveWordNext(TEXTVIEW *ptv)
{
    USPCACHE *uspCache;
    CSCRIPT_LOGATTR *logAttr;
    ULONG lineOffset;
    int charPos;

    // get Uniscribe data for current line
    if (!TextView__GetLogAttr(ptv, ptv->m_nCurrentLine, &uspCache, &logAttr,
                              &lineOffset))
        return;

    charPos = ptv->m_nCursorOffset - lineOffset;

    // if already at end-of-line, skip to next line
    if (charPos == uspCache->length_CRLF) {
        if (ptv->m_nCurrentLine + 1 < ptv->m_nLineCount)
            TextView__MoveLineStart(ptv, ptv->m_nCurrentLine + 1);

        return;
    }
    // if already on a word-break, go to next char
    if (logAttr[charPos].fWordStop)
        charPos++;

    // skip whole characters until we hit a word-break/more whitespace
    for (; charPos < uspCache->length_CRLF; charPos++) {
        if (logAttr[charPos].fWordStop || logAttr[charPos].fWhiteSpace)
            break;
    }

    // skip trailing whitespace
    while (charPos < uspCache->length_CRLF && logAttr[charPos].fWhiteSpace)
        charPos++;

    ptv->m_nCursorOffset = lineOffset + charPos;
}

//
//  Move to start of current word
//
VOID TextView__MoveWordStart(TEXTVIEW *ptv)
{
    USPCACHE *uspCache;
    CSCRIPT_LOGATTR *logAttr;
    ULONG lineOffset;
    int charPos;

    // get Uniscribe data for current line
    if (!TextView__GetLogAttr(ptv, ptv->m_nCurrentLine, &uspCache, &logAttr,
                              &lineOffset))
        return;

    charPos = ptv->m_nCursorOffset - lineOffset;

    while (charPos > 0 && !logAttr[charPos - 1].fWhiteSpace)
        charPos--;

    ptv->m_nCursorOffset = lineOffset + charPos;
}

//
//  Move to end of current word
//
VOID TextView__MoveWordEnd(TEXTVIEW *ptv)
{
    USPCACHE *uspCache;
    CSCRIPT_LOGATTR *logAttr;
    ULONG lineOffset;
    int charPos;

    // get Uniscribe data for current line
    if (!TextView__GetLogAttr(ptv, ptv->m_nCurrentLine, &uspCache, &logAttr,
                              &lineOffset))
        return;

    charPos = ptv->m_nCursorOffset - lineOffset;

    while (charPos < uspCache->length_CRLF && !logAttr[charPos].fWhiteSpace)
        charPos++;

    ptv->m_nCursorOffset = lineOffset + charPos;
}

//
//  Move to previous character
//
VOID TextView__MoveCharPrev(TEXTVIEW *ptv)
{
    USPCACHE *uspCache;
    CSCRIPT_LOGATTR *logAttr;
    ULONG lineOffset;
    int charPos;

    // get Uniscribe data for current line
    if (!TextView__GetLogAttr(ptv, ptv->m_nCurrentLine, &uspCache, &logAttr,
                              &lineOffset))
        return;

    charPos = ptv->m_nCursorOffset - lineOffset;

    // find the previous valid character-position
    for (--charPos; charPos >= 0; charPos--) {
        if (logAttr[charPos].fCharStop)
            break;
    }

    // move up to end-of-last line if necessary
    if (charPos < 0) {
        charPos = 0;

        if (ptv->m_nCurrentLine > 0) {
            TextView__MoveLineEnd(ptv, ptv->m_nCurrentLine - 1);
            return;
        }
    }
    // update cursor position
    ptv->m_nCursorOffset = lineOffset + charPos;
}

//
//  Move to next character
//
VOID TextView__MoveCharNext(TEXTVIEW *ptv)
{
    USPCACHE *uspCache;
    CSCRIPT_LOGATTR *logAttr;
    ULONG lineOffset;
    int charPos;

    // get Uniscribe data for specified line
    if (!TextView__GetLogAttr(ptv, ptv->m_nCurrentLine, &uspCache, &logAttr,
                              &lineOffset))
        return;

    charPos = ptv->m_nCursorOffset - lineOffset;

    // find the next valid character-position
    for (++charPos; charPos <= uspCache->length_CRLF; charPos++) {
        if (logAttr[charPos].fCharStop)
            break;
    }

    // skip to beginning of next line if we hit the CR/LF
    if (charPos > uspCache->length_CRLF) {
        if (ptv->m_nCurrentLine + 1 < ptv->m_nLineCount)
            TextView__MoveLineStart(ptv, ptv->m_nCurrentLine + 1);
    }
    // otherwise advance the character-position
    else {
        ptv->m_nCursorOffset = lineOffset + charPos;
    }
}

//
//  Move to start of specified line
//
VOID TextView__MoveLineStart(TEXTVIEW *ptv, ULONG lineNo)
{
    ULONG lineOffset;
    USPCACHE *uspCache;
    CSCRIPT_LOGATTR *logAttr;
    int charPos;

    // get Uniscribe data for current line
    if (!TextView__GetLogAttr(ptv, lineNo, &uspCache, &logAttr, &lineOffset))
        return;

    charPos = ptv->m_nCursorOffset - lineOffset;

    // if already at start of line, skip *forwards* past any whitespace
    if (ptv->m_nCursorOffset == lineOffset) {
        // skip whitespace
        while (logAttr[ptv->m_nCursorOffset - lineOffset].fWhiteSpace)
            ptv->m_nCursorOffset++;
    }
    // if not at start, goto start
    else {
        ptv->m_nCursorOffset = lineOffset;
    }
}

//
//  Move to end of specified line
//
VOID TextView__MoveLineEnd(TEXTVIEW *ptv, ULONG lineNo)
{
    USPCACHE *uspCache;

    if ((uspCache = TextView__GetUspCache(ptv, 0, lineNo, NULL)) == 0)
        return;

    ptv->m_nCursorOffset = uspCache->offset + uspCache->length_CRLF;
}

//
//  Move to start of file
//
VOID TextView__MoveFileStart(TEXTVIEW *ptv) { ptv->m_nCursorOffset = 0; }

//
//  Move to end of file
//
VOID TextView__MoveFileEnd(TEXTVIEW *ptv)
{
    ptv->m_nCursorOffset = TextDocument__size(ptv->m_pTextDoc);
}

//
//  Process keyboard-navigation keys
//
LONG TextView__OnKeyDown(TEXTVIEW *ptv, UINT nKeyCode, UINT nFlags)
{
    bool fCtrlDown = IsKeyPressed(VK_CONTROL);
    bool fShiftDown = IsKeyPressed(VK_SHIFT);
    BOOL fAdvancing = FALSE;

    //
    //  Process the key-press. Cursor movement is different depending
    //  on if <ctrl> is held down or not, so act accordingly
    //
    switch (nKeyCode) {
    case VK_SHIFT:
    case VK_CONTROL:
        return 0;

    // CTRL+Z undo
    case 'z':
    case 'Z':

        if (fCtrlDown && TextView__Undo(ptv))
            TextView__NotifyParent(ptv, TVN_CHANGED, NULL);

        return 0;

    // CTRL+Y redo
    case 'y':
    case 'Y':

        if (fCtrlDown && TextView__Redo(ptv))
            TextView__NotifyParent(ptv, TVN_CHANGED, NULL);

        return 0;

    // Change insert mode / clipboard copy&paste
    case VK_INSERT:

        if (fCtrlDown) {
            TextView__OnCopy(ptv);
            TextView__NotifyParent(ptv, TVN_CHANGED, NULL);
        } else if (fShiftDown) {
            TextView__OnPaste(ptv);
            TextView__NotifyParent(ptv, TVN_CHANGED, NULL);
        } else {
            if (ptv->m_nEditMode == MODE_INSERT)
                ptv->m_nEditMode = MODE_OVERWRITE;

            else if (ptv->m_nEditMode == MODE_OVERWRITE)
                ptv->m_nEditMode = MODE_INSERT;

            TextView__NotifyParent(ptv, TVN_EDITMODE_CHANGE, NULL);
        }

        return 0;

    case VK_DELETE:

        if (ptv->m_nEditMode != MODE_READONLY) {
            if (fShiftDown)
                TextView__OnCut(ptv);
            else
                TextView__ForwardDelete(ptv);

            TextView__NotifyParent(ptv, TVN_CHANGED, NULL);
        }
        return 0;

    case VK_BACK:

        if (ptv->m_nEditMode != MODE_READONLY) {
            TextView__BackDelete(ptv);
            fAdvancing = FALSE;

            TextView__NotifyParent(ptv, TVN_CHANGED, NULL);
        }
        return 0;

    case VK_LEFT:

        if (fCtrlDown)
            TextView__MoveWordPrev(ptv);
        else
            TextView__MoveCharPrev(ptv);

        fAdvancing = FALSE;
        break;

    case VK_RIGHT:

        if (fCtrlDown)
            TextView__MoveWordNext(ptv);
        else
            TextView__MoveCharNext(ptv);

        fAdvancing = TRUE;
        break;

    case VK_UP:
        if (fCtrlDown)
            TextView__Scroll(ptv, 0, -1);
        else
            TextView__MoveLineUp(ptv, 1);
        break;

    case VK_DOWN:
        if (fCtrlDown)
            TextView__Scroll(ptv, 0, 1);
        else
            TextView__MoveLineDown(ptv, 1);
        break;

    case VK_PRIOR:
        if (!fCtrlDown)
            TextView__MoveLineUp(ptv, ptv->m_nWindowLines);
        break;

    case VK_NEXT:
        if (!fCtrlDown)
            TextView__MoveLineDown(ptv, ptv->m_nWindowLines);
        break;

    case VK_HOME:
        if (fCtrlDown)
            TextView__MoveFileStart(ptv);
        else
            TextView__MoveLineStart(ptv, ptv->m_nCurrentLine);
        break;

    case VK_END:
        if (fCtrlDown)
            TextView__MoveFileEnd(ptv);
        else
            TextView__MoveLineEnd(ptv, ptv->m_nCurrentLine);
        break;

    default:
        return 0;
    }

    // Extend selection if <shift> is down
    if (fShiftDown) {
        TextView__InvalidateRange(ptv, ptv->m_nSelectionEnd,
                                  ptv->m_nCursorOffset);
        ptv->m_nSelectionEnd = ptv->m_nCursorOffset;
    }
    // Otherwise clear the selection
    else {
        if (ptv->m_nSelectionStart != ptv->m_nSelectionEnd)
            TextView__InvalidateRange(ptv, ptv->m_nSelectionStart,
                                      ptv->m_nSelectionEnd);

        ptv->m_nSelectionEnd = ptv->m_nCursorOffset;
        ptv->m_nSelectionStart = ptv->m_nCursorOffset;
    }

    // update caret-location (xpos, line#) from the offset
    TextView__UpdateCaretOffset(ptv, ptv->m_nCursorOffset, fAdvancing,
                                &ptv->m_nCaretPosX, &ptv->m_nCurrentLine);

    // maintain the caret 'anchor' position *except* for up/down actions
    if (nKeyCode != VK_UP && nKeyCode != VK_DOWN) {
        ptv->m_nAnchorPosX = ptv->m_nCaretPosX;

        // scroll as necessary to keep caret within viewport
        TextView__ScrollToPosition(ptv, ptv->m_nCaretPosX, ptv->m_nCurrentLine);
    } else {
        // scroll as necessary to keep caret within viewport
        if (!fCtrlDown)
            TextView__ScrollToPosition(ptv, ptv->m_nCaretPosX,
                                       ptv->m_nCurrentLine);
    }

    TextView__NotifyParent(ptv, TVN_CURSOR_CHANGE, NULL);

    return 0;
}
