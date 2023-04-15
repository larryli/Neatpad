#ifndef TEXTDOC_INCLUDED
#define TEXTDOC_INCLUDED

#include "codepages.h"
#include "sequence.h"

typedef struct {
    sequence *m_seq;

    ULONG m_nDocLength_chars;
    ULONG m_nDocLength_bytes;

    ULONG *m_pLineBuf_byte;
    ULONG *m_pLineBuf_char;
    ULONG m_nNumLines;

    int m_nFileFormat;
    int m_nHeaderSize;
} TEXTDOCUMENT;

typedef struct {
    TEXTDOCUMENT *text_doc;
    ULONG off_bytes;
    ULONG len_bytes;
} TEXTITERATOR;

// public:
TEXTDOCUMENT *TextDocument__new(void);
void TextDocument__delete(TEXTDOCUMENT *ptd);

bool TextDocument__init(TEXTDOCUMENT *ptd, HANDLE hFile);
bool TextDocument__init_filename(TEXTDOCUMENT *ptd, TCHAR *filename);

bool TextDocument__clear(TEXTDOCUMENT *ptd);
bool TextDocument__EmptyDoc(TEXTDOCUMENT *ptd);

bool TextDocument__Undo(TEXTDOCUMENT *ptd, ULONG *offset_start,
                        ULONG *offset_end);
bool TextDocument__Redo(TEXTDOCUMENT *ptd, ULONG *offset_start,
                        ULONG *offset_end);

// UTF-16 text-editing interface
ULONG TextDocument__insert_text(TEXTDOCUMENT *ptd, ULONG offset_chars,
                                TCHAR *text, ULONG length);
ULONG TextDocument__replace_text(TEXTDOCUMENT *ptd, ULONG offset_chars,
                                 TCHAR *text, ULONG length, ULONG erase_len);
ULONG TextDocument__erase_text(TEXTDOCUMENT *ptd, ULONG offset_chars,
                               ULONG length);

ULONG TextDocument__lineno_from_offset(TEXTDOCUMENT *ptd, ULONG offset);
ULONG TextDocument__offset_from_lineno(TEXTDOCUMENT *ptd, ULONG lineno);

bool TextDocument__lineinfo_from_offset(TEXTDOCUMENT *ptd, ULONG offset_chars,
                                        ULONG *lineno, ULONG *lineoff_chars,
                                        ULONG *linelen_chars,
                                        ULONG *lineoff_bytes,
                                        ULONG *linelen_bytes);
bool TextDocument__lineinfo_from_lineno(TEXTDOCUMENT *ptd, ULONG lineno,
                                        ULONG *lineoff_chars,
                                        ULONG *linelen_chars,
                                        ULONG *lineoff_bytes,
                                        ULONG *linelen_bytes);

TEXTITERATOR *TextDocument__iterate(TEXTDOCUMENT *ptd, ULONG offset);
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
ULONG TextDocument__getline(TEXTDOCUMENT *ptd, ULONG nLineNo, TCHAR *buf,
                            ULONG buflen, ULONG *off_chars);

int TextDocument__getformat(TEXTDOCUMENT *ptd);
ULONG TextDocument__linecount(TEXTDOCUMENT *ptd);
ULONG TextDocument__longestline(TEXTDOCUMENT *ptd, int tabwidth);
ULONG TextDocument__size(TEXTDOCUMENT *ptd);

// private:
bool TextDocument__init_linebuffer(TEXTDOCUMENT *ptd);

ULONG TextDocument__charoffset_to_byteoffset(TEXTDOCUMENT *ptd,
                                             ULONG offset_chars);
ULONG TextDocument__byteoffset_to_charoffset(TEXTDOCUMENT *ptd,
                                             ULONG offset_bytes);

ULONG TextDocument__count_chars(TEXTDOCUMENT *ptd, ULONG offset_bytes,
                                ULONG length_chars);

size_t TextDocument__utf16_to_rawdata(TEXTDOCUMENT *ptd, TCHAR *utf16str,
                                      size_t utf16len, BYTE *rawdata,
                                      size_t *rawlen);
size_t TextDocument__rawdata_to_utf16(TEXTDOCUMENT *ptd, BYTE *rawdata,
                                      size_t rawlen, TCHAR *utf16str,
                                      size_t *utf16len);

int TextDocument__detect_file_format(TEXTDOCUMENT *ptd, int *headersize);
ULONG TextDocument__gettext(TEXTDOCUMENT *ptd, ULONG offset, ULONG lenbytes,
                            TCHAR *buf, ULONG *len);
int TextDocument__getchar(TEXTDOCUMENT *ptd, ULONG offset, ULONG lenbytes,
                          ULONG *pch32);

// UTF-16 text-editing interface
ULONG TextDocument__insert_raw(TEXTDOCUMENT *ptd, ULONG offset_bytes,
                               TCHAR *text, ULONG length);
ULONG TextDocument__replace_raw(TEXTDOCUMENT *ptd, ULONG offset_bytes,
                                TCHAR *text, ULONG length, ULONG erase_len);
ULONG TextDocument__erase_raw(TEXTDOCUMENT *ptd, ULONG offset_bytes,
                              ULONG length);

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

/*int insert_text(TCHAR *buf, int buflen)
   {
   if(text_doc)
   {
   // get text from the TextDocument at the specified byte-offset
   int len = text_doc->insert(off_bytes, buf, buflen);

   // adjust the iterator's internal position
   off_bytes += len;
   return buflen;
   }
   else
   {
   return 0;
   }
   }

   int replace_text(TCHAR *buf, int buflen)
   {
   if(text_doc)
   {
   // get text from the TextDocument at the specified byte-offset
   int len = text_doc->replace(off_bytes, buf, buflen);

   // adjust the iterator's internal position
   off_bytes += len;
   return buflen;
   }
   else
   {
   return 0;
   }
   }

   int erase_text(int length)
   {
   if(text_doc)
   {
   // get text from the TextDocument at the specified byte-offset
   int len = text_doc->erase(off_bytes, length);

   // adjust the iterator's internal position
   off_bytes += len;
   return len;
   }
   else
   {
   return 0;
   }
   } */

inline bool TextIterator__bool(TEXTITERATOR *pti)
{
    return pti->text_doc ? true : false;
}

typedef struct {
    TEXTDOCUMENT *m_pTextDoc;
} LINEITERATOR;

inline LINEITERATOR *LINEITERATOR__new(void)
{
    LINEITERATOR *pli = malloc(sizeof(LINEITERATOR));
    if (pli == NULL)
        return NULL;
    return pli;
}

inline void LINEITERATOR__delete(LINEITERATOR *pli) { free(pli); }

struct _BOM_LOOKUP {
    DWORD bom;
    ULONG len;
    int type;
};

#endif
