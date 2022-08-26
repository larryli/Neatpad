//
//	MODULE:		TextDocument.cpp
//
//	PURPOSE:	Basic implementation of a text data-sequence class
//
//	NOTES:		www.catch22.net
//
#include <windows.h>

#include "TextDocument.h"

//
//	TextDocument constructor
//
TEXTDOCUMENT *TextDocument__new(void)
{
    TEXTDOCUMENT *ptd = malloc(sizeof(TEXTDOCUMENT));
    if (ptd == NULL)
        return NULL;
    ptd->buffer = 0;
    ptd->length = 0;
    ptd->linebuffer = 0;
    ptd->numlines = 0;
    return ptd;
}

//
//	TextDocument destructor
//
void TextDocument__delete(TEXTDOCUMENT *ptd)
{
    TextDocument__clear(ptd);
    free(ptd);
}

//
//	Initialize the TextDocument with the specified file
//
BOOL TextDocument__init_filename(TEXTDOCUMENT *ptd, char *filename)
{
    HANDLE hFile;

    hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0,
                       OPEN_EXISTING, 0, 0);

    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    return TextDocument__init(ptd, hFile);
}

//
//	Initialize using a file-handle
//
BOOL TextDocument__init(TEXTDOCUMENT *ptd, HANDLE hFile)
{
    ULONG numread;

    if ((ptd->length = GetFileSize(hFile, 0)) == 0)
        return FALSE;

    // allocate new file-buffer
    if ((ptd->buffer = malloc(sizeof(char) * ptd->length)) == NULL)
        return FALSE;

    // read entire file into memory
    ReadFile(hFile, ptd->buffer, ptd->length, &numread, 0);

    // work out where each line of text starts
    if (!TextDocument__init_linebuffer(ptd))
        TextDocument__clear(ptd);

    CloseHandle(hFile);
    return TRUE;
}

//
//	Initialize the line-buffer, so we know where every
//  line starts within the file
//
BOOL TextDocument__init_linebuffer(TEXTDOCUMENT *ptd)
{
    ULONG i = 0;
    ULONG linestart = 0;

    // allocate the line-buffer
    if ((ptd->linebuffer = malloc(sizeof(ULONG) * ptd->length)) == NULL)
        return FALSE;

    ptd->numlines = 0;

    // loop through every byte in the file
    for (i = 0; i < ptd->length;) {
        if (ptd->buffer[i++] == '\r') {
            // carriage-return / line-feed combination
            if (ptd->buffer[i] == '\n')
                i++;

            // record where the line starts
            ptd->linebuffer[ptd->numlines++] = linestart;
            linestart = i;
        }
    }

    if (ptd->length > 0)
        ptd->linebuffer[ptd->numlines++] = linestart;

    ptd->linebuffer[ptd->numlines] = ptd->length;

    return TRUE;
}

//
//	Empty the data-TextDocument
//
BOOL TextDocument__clear(TEXTDOCUMENT *ptd)
{
    if (ptd->buffer) {
        free(ptd->buffer);
        ptd->buffer = NULL;
        ptd->length = 0;
    }

    if (ptd->linebuffer) {
        free(ptd->linebuffer);
        ptd->linebuffer = NULL;
        ptd->numlines = 0;
    }

    return TRUE;
}

//
//	Retrieve the specified line of text and store it in "buf"
//
ULONG TextDocument__getline(TEXTDOCUMENT *ptd, ULONG lineno, char *buf,
                            size_t len)
{
    char *lineptr;
    ULONG linelen;

    if (lineno >= ptd->numlines || ptd->buffer == 0 || ptd->length == 0)
        return 0;

    // find the start of the specified line
    lineptr = ptd->buffer + ptd->linebuffer[lineno];

    // work out how long it is, by looking at the next line's starting point
    linelen = ptd->linebuffer[lineno + 1] - ptd->linebuffer[lineno];

    // make sure we don't overflow caller's buffer
    linelen = min(len, linelen);

    memcpy(buf, lineptr, linelen);

    return linelen;
}

//
//	Return the number of lines
//
ULONG TextDocument__linecount(TEXTDOCUMENT *ptd) { return ptd->numlines; }

//
//	Return the length of longest line
//
ULONG TextDocument__longestline(TEXTDOCUMENT *ptd)
{
    ULONG i, len;
    ULONG longest = 0;

    // use the line-buffer to work out which is the longest line
    for (i = 0; i < ptd->numlines; i++) {
        len = ptd->linebuffer[i + 1] - ptd->linebuffer[i];

        // don't include carriage-returns
        if (ptd->buffer[ptd->linebuffer[i + 1] - 1] == '\n')
            len--;
        if (ptd->buffer[ptd->linebuffer[i + 1] - 2] == '\r')
            len--;

        longest = max(longest, len);
    }

    return longest;
}
