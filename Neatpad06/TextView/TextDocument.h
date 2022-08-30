#ifndef TEXTDOC_INCLUDED
#define TEXTDOC_INCLUDED

typedef struct {
    char *buffer;
    ULONG length;

    ULONG *linebuffer;
    ULONG numlines;
} TEXTDOCUMENT;

TEXTDOCUMENT *TextDocument__new(void);
void TextDocument__delete(TEXTDOCUMENT *ptd);

BOOL TextDocument__init(TEXTDOCUMENT *ptd, HANDLE hFile);
BOOL TextDocument__init_filename(TEXTDOCUMENT *ptd, char *filename);

BOOL TextDocument__clear(TEXTDOCUMENT *ptd);

BOOL TextDocument__offset_to_line(TEXTDOCUMENT *ptd, ULONG fileoffset,
                                  ULONG *lineno, ULONG *offset);
BOOL TextDocument__getlineinfo(TEXTDOCUMENT *ptd, ULONG lineno, ULONG *fileoff,
                               ULONG *length);

ULONG TextDocument__getline(TEXTDOCUMENT *ptd, ULONG lineno, char *buf,
                            size_t len);
ULONG TextDocument__getline_off(TEXTDOCUMENT *ptd, ULONG lineno, char *buf,
                                size_t len, ULONG *fileoff);
ULONG TextDocument__getline_offset(TEXTDOCUMENT *ptd, ULONG lineno,
                                   ULONG offset, char *buf, size_t len);
ULONG TextDocument__getline_offset_off(TEXTDOCUMENT *ptd, ULONG lineno,
                                       ULONG offset, char *buf, size_t len,
                                       ULONG *fileoff);
ULONG TextDocument__getdata(TEXTDOCUMENT *ptd, ULONG offset, char *buf,
                            size_t len);

ULONG TextDocument__linecount(TEXTDOCUMENT *ptd);
ULONG TextDocument__longestline(TEXTDOCUMENT *ptd, int tabwidth);
ULONG TextDocument__size(TEXTDOCUMENT *ptd);

// private
BOOL TextDocument__init_linebuffer(TEXTDOCUMENT *ptd);

#endif
