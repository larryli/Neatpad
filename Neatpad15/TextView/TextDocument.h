#ifndef TEXTDOC_INCLUDED
#define TEXTDOC_INCLUDED

#include "codepages.h"

typedef struct {
    char *buffer;
    ULONG length_chars;
    ULONG length_bytes;

    ULONG *linebuf_byte;
    ULONG *linebuf_char;
    ULONG numlines;

    int fileformat;
    int headersize;
} TEXTDOCUMENT;

typedef struct {
    TEXTDOCUMENT *text_doc;

    ULONG off_bytes;
    ULONG len_bytes;
} TEXTITERATOR;

TEXTDOCUMENT *TextDocument__new(void);
void TextDocument__delete(TEXTDOCUMENT *ptd);

BOOL TextDocument__init(TEXTDOCUMENT *ptd, HANDLE hFile);
BOOL TextDocument__init_filename(TEXTDOCUMENT *ptd, TCHAR *filename);

BOOL TextDocument__clear(TEXTDOCUMENT *ptd);

ULONG TextDocument__lineno_from_offset(TEXTDOCUMENT *ptd, ULONG offset);

BOOL TextDocument__lineinfo_from_offset(TEXTDOCUMENT *ptd, ULONG offset_chars,
                                        ULONG *lineno, ULONG *lineoff_chars,
                                        ULONG *linelen_chars,
                                        ULONG *lineoff_bytes,
                                        ULONG *linelen_bytes);
BOOL TextDocument__lineinfo_from_lineno(TEXTDOCUMENT *ptd, ULONG lineno,
                                        ULONG *lineoff_chars,
                                        ULONG *linelen_chars,
                                        ULONG *lineoff_bytes,
                                        ULONG *linelen_bytes);

TEXTITERATOR *TextDocument__iterate_line(TEXTDOCUMENT *ptd, ULONG lineno);
TEXTITERATOR *TextDocument__iterate_line_start(TEXTDOCUMENT *ptd, ULONG lineno,
                                               ULONG *linestart);
TEXTITERATOR *TextDocument__iterate_line_start_len(TEXTDOCUMENT *ptd,
                                                   ULONG lineno,
                                                   ULONG *linestart,
                                                   ULONG *linelen);
TEXTITERATOR *TextDocument__iterate_line_offset(TEXTDOCUMENT *ptd,
                                                ULONG offset_chars,
                                                ULONG *lineno);
TEXTITERATOR *TextDocument__iterate_line_offset_start(TEXTDOCUMENT *ptd,
                                                      ULONG offset_chars,
                                                      ULONG *lineno,
                                                      ULONG *linestart);

ULONG TextDocument__getdata(TEXTDOCUMENT *ptd, ULONG offset, BYTE *buf,
                            size_t len);
int TextDocument__getline(TEXTDOCUMENT *ptd, ULONG nLineNo, TCHAR *buf,
                          int buflen, ULONG *off_chars);

int TextDocument__getformat(TEXTDOCUMENT *ptd);
ULONG TextDocument__linecount(TEXTDOCUMENT *ptd);
ULONG TextDocument__longestline(TEXTDOCUMENT *ptd, int tabwidth);
ULONG TextDocument__size(TEXTDOCUMENT *ptd);

// private
BOOL TextDocument__init_linebuffer(TEXTDOCUMENT *ptd);

int TextDocument__detect_file_format(TEXTDOCUMENT *ptd, int *headersize);
int TextDocument__gettext(TEXTDOCUMENT *ptd, ULONG offset, ULONG lenbytes,
                          TCHAR *buf, int *len);
int TextDocument__getchar(TEXTDOCUMENT *ptd, ULONG offset, ULONG lenbytes,
                          ULONG *pch32);

// default constructor sets all members to zero
inline TEXTITERATOR *TextIterator__new(void)
{
    TEXTITERATOR *pti = malloc(sizeof(TEXTITERATOR));
    if (pti == NULL)
        return NULL;
    pti->text_doc = NULL;
    pti->off_bytes = 0;
    pti->len_bytes = 0;
    return pti;
}

inline TEXTITERATOR *TextIterator__new_assign(ULONG off, ULONG len,
                                              TEXTDOCUMENT *ptd)
{
    TEXTITERATOR *pti = malloc(sizeof(TEXTITERATOR));
    if (pti == NULL)
        return NULL;
    pti->text_doc = ptd;
    pti->off_bytes = off;
    pti->len_bytes = len;
    return pti;
}

inline void TextIterator__delete(TEXTITERATOR *pti) { free(pti); }

// default copy-constructor
inline TEXTITERATOR *TextIterator__copy(TEXTITERATOR *pti2)
{
    TEXTITERATOR *pti = malloc(sizeof(TEXTITERATOR));
    if (pti == NULL)
        return NULL;
    pti->text_doc = pti2->text_doc;
    pti->off_bytes = pti2->off_bytes;
    pti->len_bytes = pti2->len_bytes;
    return pti;
}

// assignment operator
inline TEXTITERATOR *TextIterator__assign(TEXTITERATOR *pti, TEXTITERATOR *pti2)
{
    pti->text_doc = pti2->text_doc;
    pti->off_bytes = pti2->off_bytes;
    pti->len_bytes = pti2->len_bytes;
    return pti;
}

inline int TextIterator__gettext(TEXTITERATOR *pti, TCHAR *buf, int buflen)
{
    if (pti->text_doc) {
        // get text from the TextDocument at the specified byte-offset
        int len = TextDocument__gettext(pti->text_doc, pti->off_bytes,
                                        pti->len_bytes, buf, &buflen);

        // adjust the iterator's internal position
        pti->off_bytes += len;
        pti->len_bytes -= len;

        return buflen;
    } else {
        return 0;
    }
}

inline BOOL TextIterator__bool(TEXTITERATOR *pti)
{
    return pti->text_doc ? TRUE : FALSE;
}

#endif
